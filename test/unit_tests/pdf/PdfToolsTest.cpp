#include <fstream>
#include <string>
#include <vector>

#include <cairo-pdf.h>
#include <cairo.h>
#include <gtest/gtest.h>
#include <poppler.h>

#include "config-features.h"
#include "pdf/tools/PdfTools.h"
#include "util/PathUtil.h"

namespace tools = utn::pdftools;

namespace {

fs::path scratch(const char* name) {
    fs::path dir = Util::getTmpDirSubfolder("pdftools-test") / name;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    return dir;
}

/// A real text PDF: one page per label, each page carrying its label as text (and an odd page size per page so the
/// tests can tell pages apart even by shape)
fs::path makePdf(const fs::path& file, const std::vector<std::string>& labels) {
    const std::string utf8 = file.string();
    cairo_surface_t* surface = cairo_pdf_surface_create(utf8.c_str(), 300, 400);
    cairo_t* cr = cairo_create(surface);
    int n = 0;
    for (const auto& label: labels) {
        cairo_pdf_surface_set_size(surface, 300 + 10 * n, 400);
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 24);
        cairo_move_to(cr, 20, 100);
        cairo_show_text(cr, label.c_str());
        cairo_show_page(cr);
        ++n;
    }
    cairo_destroy(cr);
    cairo_surface_finish(surface);
    cairo_surface_destroy(surface);
    return file;
}

PopplerDocument* open(const fs::path& pdf) {
    auto uri = Util::toUri(pdf);
    return poppler_document_new_from_file(uri->c_str(), nullptr, nullptr);
}

std::vector<std::string> texts(const fs::path& pdf) {
    std::vector<std::string> out;
    PopplerDocument* doc = open(pdf);
    if (!doc) {
        return out;
    }
    for (int i = 0; i < poppler_document_get_n_pages(doc); ++i) {
        PopplerPage* page = poppler_document_get_page(doc, i);
        char* t = poppler_page_get_text(page);
        std::string s = t ? t : "";
        g_free(t);
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
            s.pop_back();
        }
        out.push_back(s);
        g_object_unref(page);
    }
    g_object_unref(doc);
    return out;
}

std::pair<double, double> pageSize(const fs::path& pdf, int index) {
    PopplerDocument* doc = open(pdf);
    PopplerPage* page = poppler_document_get_page(doc, index);
    double w = 0;
    double h = 0;
    poppler_page_get_size(page, &w, &h);
    g_object_unref(page);
    g_object_unref(doc);
    return {w, h};
}

std::string fileBytes(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
}

int partFiles(const fs::path& dir) {
    int n = 0;
    for (const auto& e: fs::directory_iterator(dir)) {
        n += e.path().extension() == ".utn-part" ? 1 : 0;
    }
    return n;
}

}  // namespace

TEST(PdfToolsRanges, understandsPagesRangesOpenEndsAndKeepsTheOrderWritten) {
    std::string error;
    using V = std::vector<size_t>;
    EXPECT_EQ(tools::parsePageRanges("1-3, 5", 6, &error), (V{0, 1, 2, 4})) << error;
    EXPECT_EQ(tools::parsePageRanges("3,1", 6), (V{2, 0})) << "the order written is kept";
    EXPECT_EQ(tools::parsePageRanges("2,2", 6), (V{1, 1})) << "a page can be repeated";
    EXPECT_EQ(tools::parsePageRanges("5-", 6), (V{4, 5}));
    EXPECT_EQ(tools::parsePageRanges("-2", 6), (V{0, 1}));
    EXPECT_EQ(tools::parsePageRanges(" ALL ", 3), (V{0, 1, 2}));
    EXPECT_EQ(tools::parsePageRanges("1;2 ,, 3", 3), (V{0, 1, 2})) << "semicolons and empty entries are fine";
}

TEST(PdfToolsRanges, refusesAnythingThatIsNotAPageOfTheDocumentAndSaysWhy) {
    for (const char* bad: {"", "0", "7", "1-7", "3-1", "a", "1-b", "-", "1.5", "99999999999", "--2"}) {
        std::string error;
        EXPECT_TRUE(tools::parsePageRanges(bad, 6, &error).empty()) << "'" << bad << "'";
        EXPECT_FALSE(error.empty()) << "'" << bad << "' says why";
    }
    std::string error;
    tools::parsePageRanges("9", 6, &error);
    EXPECT_NE(error.find("1 to 6"), std::string::npos) << "the message names the valid pages: " << error;
    EXPECT_TRUE(tools::parsePageRanges("all", 0).empty());
}

TEST(PdfToolsRanges, describesTheSelectionWhileTyping) {
    bool ok = false;
    EXPECT_EQ(tools::describeRanges("1-3, 5", 8, &ok), "4 pages: 1, 2, 3, 5");
    EXPECT_TRUE(ok);
    EXPECT_EQ(tools::describeRanges("2", 8, &ok), "1 page: 2");
    EXPECT_EQ(tools::describeRanges("all", 20, &ok), "20 pages: 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, ...");
    EXPECT_TRUE(ok);
    EXPECT_NE(tools::describeRanges("", 8, &ok).find("Type the pages"), std::string::npos) << "an empty box is a hint";
    EXPECT_FALSE(ok);
    EXPECT_NE(tools::describeRanges("9", 8, &ok).find("1 to 8"), std::string::npos);
    EXPECT_FALSE(ok);
}

TEST(PdfToolsRanges, splitNamesMatchWhatSplitWrites) {
    using V = std::vector<std::string>;
    EXPECT_EQ(tools::splitNames("book", 5, 2), (V{"book (part 1).pdf", "book (part 2).pdf", "book (part 3).pdf"}));
    EXPECT_TRUE(tools::splitNames("book", 5, 5).empty()) << "nothing to split";
    EXPECT_TRUE(tools::splitNames("book", 5, 0).empty());
    EXPECT_EQ(tools::splitNames("x", 4, 2).size(), 2u);
}

#ifdef ENABLE_QPDF

TEST(PdfTools, mergeJoinsEveryPageOfEveryFileInTheOrderGiven) {
    const fs::path dir = scratch("merge");
    const auto a = makePdf(dir / "a.pdf", {"A1", "A2", "A3"});
    const auto b = makePdf(dir / "b.pdf", {"B1", "B2"});
    const auto r = tools::mergeFiles({b, a}, dir / "both.pdf");
    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_EQ(r.pages, 5u);
    EXPECT_EQ(texts(dir / "both.pdf"), (std::vector<std::string>{"B1", "B2", "A1", "A2", "A3"}));
    EXPECT_EQ(tools::pageCount(dir / "both.pdf"), 5);
    EXPECT_EQ(partFiles(dir), 0);
}

TEST(PdfTools, mergeKeepsEachPagesOwnSize) {
    const fs::path dir = scratch("sizes");
    const auto a = makePdf(dir / "a.pdf", {"A1", "A2"});  // second page is 10 pt wider
    const auto b = makePdf(dir / "b.pdf", {"B1"});
    ASSERT_TRUE(tools::mergeFiles({a, b}, dir / "out.pdf").ok);
    EXPECT_NEAR(pageSize(dir / "out.pdf", 0).first, 300, 0.5);
    EXPECT_NEAR(pageSize(dir / "out.pdf", 1).first, 310, 0.5) << "the wider page stays wider";
    EXPECT_NEAR(pageSize(dir / "out.pdf", 2).first, 300, 0.5);
}

TEST(PdfTools, mergeNeedsTwoReadableFilesAndNamesTheBadOne) {
    const fs::path dir = scratch("merge-bad");
    const auto a = makePdf(dir / "a.pdf", {"A1"});
    EXPECT_FALSE(tools::mergeFiles({a}, dir / "out.pdf").ok) << "one file is not a merge";

    {
        std::ofstream(dir / "notes.pdf") << "this is not a PDF";
    }
    const auto r = tools::mergeFiles({a, dir / "notes.pdf"}, dir / "out.pdf");
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.error.find("notes.pdf"), std::string::npos) << "the message names the file: " << r.error;
    EXPECT_FALSE(fs::exists(dir / "out.pdf"));
    EXPECT_FALSE(tools::mergeFiles({a, dir / "missing.pdf"}, dir / "out.pdf").ok);
    EXPECT_EQ(partFiles(dir), 0);
}

TEST(PdfTools, extractTakesTheNamedPagesInTheNamedOrder) {
    const fs::path dir = scratch("extract");
    const auto src = makePdf(dir / "src.pdf", {"P1", "P2", "P3", "P4", "P5"});
    ASSERT_TRUE(tools::extractPages(src, "4,1-2", dir / "some.pdf").ok);
    EXPECT_EQ(texts(dir / "some.pdf"), (std::vector<std::string>{"P4", "P1", "P2"}));
    ASSERT_TRUE(tools::extractPages(src, "5-", dir / "last.pdf").ok);
    EXPECT_EQ(texts(dir / "last.pdf"), (std::vector<std::string>{"P5"}));

    const auto bad = tools::extractPages(src, "9", dir / "none.pdf");
    EXPECT_FALSE(bad.ok);
    EXPECT_NE(bad.error.find("1 to 5"), std::string::npos) << bad.error;
    EXPECT_FALSE(fs::exists(dir / "none.pdf"));
}

TEST(PdfTools, assembleReordersRotatesAndDropsPages) {
    const fs::path dir = scratch("assemble");
    const auto src = makePdf(dir / "src.pdf", {"P1", "P2", "P3"});
    // reverse order, turn page 2 by 90 degrees
    const auto r = tools::assemble({{src, 2, 0}, {src, 1, 90}, {src, 0, 0}}, dir / "out.pdf");
    ASSERT_TRUE(r.ok) << r.error;
    EXPECT_EQ(texts(dir / "out.pdf"), (std::vector<std::string>{"P3", "P2", "P1"}));
    EXPECT_NEAR(pageSize(dir / "out.pdf", 1).first, 400, 0.5) << "turned: width and height swap";
    EXPECT_NEAR(pageSize(dir / "out.pdf", 1).second, 310, 0.5);
    EXPECT_NEAR(pageSize(dir / "out.pdf", 0).first, 320, 0.5) << "the others are not turned";

    // Turning a page that is already turned adds up
    ASSERT_TRUE(tools::assemble({{dir / "out.pdf", 1, 90}}, dir / "again.pdf").ok);
    EXPECT_NEAR(pageSize(dir / "again.pdf", 0).first, 310, 0.5) << "180 in total is upright again";

    // Dropping pages is just not listing them
    ASSERT_TRUE(tools::assemble({{src, 1, 0}}, dir / "one.pdf").ok);
    EXPECT_EQ(texts(dir / "one.pdf"), (std::vector<std::string>{"P2"}));
}

TEST(PdfTools, theSourceIsNeverChangedAndTheOutputCanNeverBeASource) {
    const fs::path dir = scratch("safety");
    const auto a = makePdf(dir / "a.pdf", {"A1", "A2"});
    const auto b = makePdf(dir / "b.pdf", {"B1"});
    const std::string before = fileBytes(a);

    const auto same = tools::mergeFiles({a, b}, a);
    EXPECT_FALSE(same.ok);
    EXPECT_EQ(fileBytes(a), before) << "the source was not overwritten";
    EXPECT_FALSE(tools::extractPages(a, "1", dir / "." / "a.pdf").ok) << "a different spelling of the same file";

    ASSERT_TRUE(tools::extractPages(a, "1", dir / "one.pdf").ok);
    EXPECT_EQ(fileBytes(a), before);
    EXPECT_EQ(partFiles(dir), 0);
}

TEST(PdfTools, aFailedRunKeepsAnExistingOutputAndLeavesNoPartialFile) {
    const fs::path dir = scratch("failed");
    const auto a = makePdf(dir / "a.pdf", {"A1"});
    {
        std::ofstream(dir / "out.pdf") << "previous result";
    }
    const auto r = tools::assemble({{a, 0, 0}, {a, 5, 0}}, dir / "out.pdf");  // page 6 does not exist
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(fileBytes(dir / "out.pdf"), "previous result");
    EXPECT_EQ(partFiles(dir), 0);

    EXPECT_FALSE(tools::assemble({{a, 0, 45}}, dir / "x.pdf").ok) << "only quarter turns";
    EXPECT_FALSE(tools::assemble({}, dir / "x.pdf").ok);
    EXPECT_FALSE(tools::assemble({{a, 0, 0}}, dir / "no-such-folder" / "x.pdf").ok);
}

TEST(PdfTools, aGoodRunReplacesAnOlderOutputInOneStep) {
    const fs::path dir = scratch("replace");
    const auto a = makePdf(dir / "a.pdf", {"A1", "A2"});
    {
        std::ofstream(dir / "out.pdf") << "previous result";
    }
    ASSERT_TRUE(tools::extractPages(a, "all", dir / "out.pdf").ok);
    EXPECT_EQ(tools::pageCount(dir / "out.pdf"), 2);
    EXPECT_EQ(partFiles(dir), 0);
}

TEST(PdfTools, splitCutsIntoNumberedFilesAndNeverReplacesOne) {
    const fs::path dir = scratch("split");
    const auto src = makePdf(dir / "book.pdf", {"P1", "P2", "P3", "P4", "P5"});
    std::vector<fs::path> written;
    const auto r = tools::splitByPageCount(src, 2, dir, "book", &written);
    ASSERT_TRUE(r.ok) << r.error;
    ASSERT_EQ(written.size(), 3u);
    EXPECT_EQ(written[0].filename().string(), "book (part 1).pdf");
    EXPECT_EQ(texts(written[0]), (std::vector<std::string>{"P1", "P2"}));
    EXPECT_EQ(texts(written[1]), (std::vector<std::string>{"P3", "P4"}));
    EXPECT_EQ(texts(written[2]), (std::vector<std::string>{"P5"}));

    // The names are now taken: a second split writes nothing and changes nothing
    const std::string firstBytes = fileBytes(written[0]);
    const auto again = tools::splitByPageCount(src, 2, dir, "book");
    EXPECT_FALSE(again.ok);
    EXPECT_EQ(fileBytes(written[0]), firstBytes);

    EXPECT_FALSE(tools::splitByPageCount(src, 0, dir, "x").ok);
    EXPECT_FALSE(tools::splitByPageCount(src, 5, dir, "x").ok) << "nothing to split";
    EXPECT_FALSE(fs::exists(dir / "x (part 1).pdf"));
}

TEST(PdfTools, pageCountReportsUnreadableFiles) {
    const fs::path dir = scratch("count");
    std::string error;
    EXPECT_EQ(tools::pageCount(dir / "missing.pdf", &error), -1);
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(tools::pageCount(makePdf(dir / "ok.pdf", {"a", "b", "c"})), 3);
}

#else

TEST(PdfTools, withoutQpdfEveryToolSaysSo) {
    EXPECT_FALSE(tools::available());
    const fs::path dir = scratch("unavailable");
    EXPECT_FALSE(tools::mergeFiles({dir / "a.pdf", dir / "b.pdf"}, dir / "out.pdf").ok);
    EXPECT_FALSE(fs::exists(dir / "out.pdf"));
}

#endif  // ENABLE_QPDF
