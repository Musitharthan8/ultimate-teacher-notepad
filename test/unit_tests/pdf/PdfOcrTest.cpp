#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

#include <cairo-pdf.h>
#include <cairo.h>
#include <gtest/gtest.h>
#include <poppler.h>

#include "config-features.h"
#include "pdf/ocr/PdfOcr.h"
#include "util/PathUtil.h"

namespace {

fs::path scratchFolder(const char* name) {
    fs::path dir = Util::getTmpDirSubfolder("ocr-test") / name;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    return dir;
}

/// A PDF whose pages are only pictures of text, like a scan: the text is drawn into a bitmap, and only the bitmap goes
/// into the PDF, so the file has no text layer at all.
fs::path makeScan(const fs::path& file, const std::vector<std::string>& pageTexts) {
    const double pageW = 612;
    const double pageH = 792;
    const int dpi = 150;
    const double scale = dpi / 72.0;
    const std::string utf8 = file.string();
    cairo_surface_t* pdf = cairo_pdf_surface_create(utf8.c_str(), pageW, pageH);
    cairo_t* out = cairo_create(pdf);
    for (const auto& text: pageTexts) {
        cairo_surface_t* bitmap = cairo_image_surface_create(CAIRO_FORMAT_RGB24, static_cast<int>(pageW * scale),
                                                             static_cast<int>(pageH * scale));
        cairo_t* cr = cairo_create(bitmap);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_paint(cr);
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 30 * scale);
        cairo_move_to(cr, 60 * scale, 200 * scale);
        cairo_show_text(cr, text.c_str());
        cairo_destroy(cr);
        cairo_surface_flush(bitmap);

        cairo_save(out);
        cairo_scale(out, 1 / scale, 1 / scale);
        cairo_set_source_surface(out, bitmap, 0, 0);
        cairo_paint(out);
        cairo_restore(out);
        cairo_surface_destroy(bitmap);
        cairo_show_page(out);
    }
    cairo_destroy(out);
    cairo_surface_finish(pdf);
    cairo_surface_destroy(pdf);
    return file;
}

std::string pageText(const fs::path& pdf, int index) {
    auto uri = Util::toUri(pdf);
    GError* error = nullptr;
    PopplerDocument* doc = poppler_document_new_from_file(uri->c_str(), nullptr, &error);
    if (!doc) {
        if (error) {
            g_error_free(error);
        }
        return {};
    }
    std::string text;
    if (PopplerPage* page = poppler_document_get_page(doc, index)) {
        if (char* t = poppler_page_get_text(page)) {
            text = t;
            g_free(t);
        }
        g_object_unref(page);
    }
    g_object_unref(doc);
    return text;
}

std::string lower(std::string s) {
    for (auto& c: s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

int countPartFiles(const fs::path& dir) {
    int count = 0;
    for (const auto& entry: fs::directory_iterator(dir)) {
        count += entry.path().extension() == ".utn-part" ? 1 : 0;
    }
    return count;
}

}  // namespace

TEST(PdfOcr, withoutSupportEveryCallSaysSoAndNothingIsWritten) {
    if (utn::ocr::compiledIn()) {
        GTEST_SUCCEED() << "this build has OCR; the real behaviour is tested below";
        return;
    }
    const fs::path dir = scratchFolder("unsupported");
    auto result = utn::ocr::makeSearchable(dir / "in.pdf", dir / "out.pdf");
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.error.empty());
    EXPECT_FALSE(fs::exists(dir / "out.pdf"));
    EXPECT_TRUE(utn::ocr::availableLanguages().empty());
}

#ifdef ENABLE_OCR

TEST(PdfOcr, languageDataIsFoundAndEnglishIsOffered) {
    const fs::path dir = utn::ocr::findTessdataDir();
    ASSERT_FALSE(dir.empty()) << "install the Tesseract language data (tessdata-eng) or set TESSDATA_PREFIX";
    const auto languages = utn::ocr::availableLanguages();
    EXPECT_NE(std::find(languages.begin(), languages.end(), "eng"), languages.end());
    EXPECT_EQ(std::find(languages.begin(), languages.end(), "osd"), languages.end()) << "osd is not a language";
}

TEST(PdfOcr, aScanHasNoTextAndAnUnreadableFileIsReported) {
    const fs::path dir = scratchFolder("count");
    makeScan(dir / "scan.pdf", {"Hello classroom", "Second page"});
    EXPECT_EQ(utn::ocr::countPagesWithText(dir / "scan.pdf"), 0);
    EXPECT_EQ(utn::ocr::countPagesWithText(dir / "missing.pdf"), -1);
}

TEST(PdfOcr, makeSearchableAddsAnInvisibleTextLayerAndKeepsEveryPage) {
    const fs::path dir = scratchFolder("run");
    const fs::path scan = makeScan(dir / "scan.pdf", {"Welcome to the classroom", "Homework is due Friday"});
    ASSERT_EQ(pageText(scan, 0).find_first_not_of(" \n\t"), std::string::npos) << "the fixture really is a scan";

    int lastDone = 0;
    int lastTotal = 0;
    const fs::path out = dir / "scan (searchable).pdf";
    const auto result = utn::ocr::makeSearchable(scan, out, {}, [&](int done, int total) {
        lastDone = done;
        lastTotal = total;
        return true;
    });
    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.pages, 2);
    EXPECT_EQ(lastDone, 2);
    EXPECT_EQ(lastTotal, 2);

    ASSERT_TRUE(fs::exists(out));
    EXPECT_EQ(utn::ocr::countPagesWithText(out), 2) << "both pages now carry text";
    EXPECT_NE(lower(pageText(out, 0)).find("classroom"), std::string::npos) << pageText(out, 0);
    EXPECT_NE(lower(pageText(out, 1)).find("friday"), std::string::npos) << pageText(out, 1);

    // The source is untouched, and nothing temporary is left beside the output
    EXPECT_EQ(utn::ocr::countPagesWithText(scan), 0);
    EXPECT_EQ(countPartFiles(dir), 0);
}

TEST(PdfOcr, theOutputKeepsThePageSize) {
    const fs::path dir = scratchFolder("size");
    const fs::path scan = makeScan(dir / "scan.pdf", {"Size check"});
    ASSERT_TRUE(utn::ocr::makeSearchable(scan, dir / "out.pdf").ok);

    const auto sizeOf = [](const fs::path& pdf) {
        auto uri = Util::toUri(pdf);
        PopplerDocument* doc = poppler_document_new_from_file(uri->c_str(), nullptr, nullptr);
        PopplerPage* page = poppler_document_get_page(doc, 0);
        double w = 0;
        double h = 0;
        poppler_page_get_size(page, &w, &h);
        g_object_unref(page);
        g_object_unref(doc);
        return std::make_pair(w, h);
    };
    EXPECT_NEAR(sizeOf(dir / "out.pdf").first, sizeOf(scan).first, 0.5);
    EXPECT_NEAR(sizeOf(dir / "out.pdf").second, sizeOf(scan).second, 0.5);
}

TEST(PdfOcr, cancellingLeavesNoOutputAndKeepsAnExistingFile) {
    const fs::path dir = scratchFolder("cancel");
    const fs::path scan = makeScan(dir / "scan.pdf", {"One", "Two", "Three"});
    const fs::path out = dir / "out.pdf";
    {
        std::ofstream(out) << "previous";
    }
    const auto result = utn::ocr::makeSearchable(scan, out, {}, [](int done, int) { return done < 1; });
    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(result.cancelled);
    EXPECT_EQ(fs::file_size(out), 8u) << "the earlier file was not replaced by half a result";
    EXPECT_EQ(countPartFiles(dir), 0);
}

TEST(PdfOcr, failuresAreReportedAndNothingIsWritten) {
    const fs::path dir = scratchFolder("failures");
    const fs::path scan = makeScan(dir / "scan.pdf", {"Text"});

    auto missing = utn::ocr::makeSearchable(dir / "nope.pdf", dir / "out.pdf");
    EXPECT_FALSE(missing.ok);
    EXPECT_FALSE(missing.error.empty());

    utn::ocr::Options badLanguage;
    badLanguage.language = "zzz";
    auto unknown = utn::ocr::makeSearchable(scan, dir / "out.pdf", badLanguage);
    EXPECT_FALSE(unknown.ok);
    EXPECT_NE(unknown.error.find("zzz"), std::string::npos) << "the message names the language that failed";

    utn::ocr::Options noData;
    noData.tessdataDir = dir / "no-such-folder";
    EXPECT_FALSE(utn::ocr::makeSearchable(scan, dir / "out.pdf", noData).ok);
    EXPECT_FALSE(fs::exists(dir / "out.pdf"));
}

#endif  // ENABLE_OCR
