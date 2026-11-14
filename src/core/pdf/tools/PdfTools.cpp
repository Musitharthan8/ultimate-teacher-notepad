#include "PdfTools.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <map>
#include <memory>
#include <system_error>

#include "config-features.h"

#ifdef ENABLE_QPDF
#include <qpdf/DLL.h>

#define POINTERHOLDER_TRANSITION 4  // Only used for QPDF 11

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <qpdf/QPDFWriter.hh>
#endif

namespace utn::pdftools {

namespace {
std::string utf8(const fs::path& p) {
    const auto s = p.u8string();
    return std::string(s.begin(), s.end());
}

std::string trim(const std::string& s) {
    size_t a = 0;
    size_t b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) {
        ++a;
    }
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) {
        --b;
    }
    return s.substr(a, b - a);
}

bool parseNumber(const std::string& s, size_t& out) {
    if (s.empty() || s.size() > 9) {  // nine digits is far more pages than any PDF has
        return false;
    }
    size_t value = 0;
    const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc() || ptr != s.data() + s.size()) {
        return false;
    }
    out = value;
    return true;
}

void setError(std::string* error, const std::string& message) {
    if (error) {
        *error = message;
    }
}
}  // namespace

std::vector<size_t> parsePageRanges(const std::string& text, size_t pageCount, std::string* error) {
    std::vector<size_t> pages;
    const std::string whole = trim(text);
    std::string lowered = whole;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lowered == "all") {
        for (size_t i = 0; i < pageCount; ++i) {
            pages.push_back(i);
        }
        if (pages.empty()) {
            setError(error, "The document has no pages.");
        }
        return pages;
    }

    size_t start = 0;
    while (start <= whole.size()) {
        size_t end = whole.find_first_of(",;", start);
        if (end == std::string::npos) {
            end = whole.size();
        }
        const std::string token = trim(whole.substr(start, end - start));
        start = end + 1;
        if (token.empty()) {
            continue;
        }
        size_t first = 0;
        size_t last = 0;
        const size_t dash = token.find('-');
        if (dash == std::string::npos) {
            if (!parseNumber(token, first)) {
                setError(error, "\"" + token + "\" is not a page number.");
                return {};
            }
            last = first;
        } else {
            const std::string a = trim(token.substr(0, dash));
            const std::string b = trim(token.substr(dash + 1));
            if (a.empty() && b.empty()) {
                setError(error, "\"" + token + "\" is not a page range.");
                return {};
            }
            first = 1;
            last = pageCount;
            if ((!a.empty() && !parseNumber(a, first)) || (!b.empty() && !parseNumber(b, last))) {
                setError(error, "\"" + token + "\" is not a page range.");
                return {};
            }
            if (first > last) {
                setError(error, "\"" + token + "\" runs backwards.");
                return {};
            }
        }
        if (first < 1 || last > pageCount) {
            setError(error, "\"" + token + "\" is outside the document's pages 1 to " + std::to_string(pageCount) + ".");
            return {};
        }
        for (size_t p = first; p <= last; ++p) {
            pages.push_back(p - 1);
        }
    }
    if (pages.empty()) {
        setError(error, "No pages were chosen.");
    }
    return pages;
}

#ifndef ENABLE_QPDF

bool available() { return false; }
long pageCount(const fs::path&, std::string* error) {
    setError(error, "This build of UTN was made without PDF tools.");
    return -1;
}
Result assemble(const std::vector<Piece>&, const fs::path&) {
    Result r;
    r.error = "This build of UTN was made without PDF tools.";
    return r;
}

#else

bool available() { return true; }

long pageCount(const fs::path& pdf, std::string* error) {
    try {
        QPDF q;
        q.processFile(utf8(pdf).c_str());
        return static_cast<long>(QPDFPageDocumentHelper(q).getAllPages().size());
    } catch (const std::exception& e) {
        setError(error, std::string("The PDF could not be read (it may be damaged or password protected): ") + e.what());
        return -1;
    }
}

namespace {
struct Source {
    std::unique_ptr<QPDF> pdf;
    std::vector<QPDFPageObjectHelper> pages;
};

bool sameFile(const fs::path& a, const fs::path& b) {
    std::error_code ec;
    const fs::path ca = fs::weakly_canonical(a, ec);
    if (ec) {
        return a == b;
    }
    const fs::path cb = fs::weakly_canonical(b, ec);
    if (ec) {
        return a == b;
    }
    return ca == cb;
}
}  // namespace

Result assemble(const std::vector<Piece>& pieces, const fs::path& output) {
    Result result;
    if (pieces.empty()) {
        result.error = "There are no pages to write.";
        return result;
    }
    std::error_code ec;
    if (output.empty() || !fs::is_directory(output.parent_path().empty() ? fs::path(".") : output.parent_path(), ec)) {
        result.error = "The folder to save in does not exist.";
        return result;
    }
    for (const auto& p: pieces) {
        if (p.rotateDegrees % 90 != 0 || p.rotateDegrees < 0 || p.rotateDegrees > 270) {
            result.error = "A page can only be turned by 0, 90, 180 or 270 degrees.";
            return result;
        }
        if (sameFile(p.file, output)) {
            result.error = "The new file must have a different name from the PDFs it is made from.";
            return result;
        }
    }

    fs::path staged = output;
    staged += ".utn-part";
    try {
        std::map<std::string, Source> sources;  // keyed by file, kept alive until the output is written
        QPDF out;
        out.emptyPDF();
        QPDFPageDocumentHelper outPages(out);
        double version = 1.3;

        for (const auto& piece: pieces) {
            const std::string key = utf8(piece.file);
            auto it = sources.find(key);
            if (it == sources.end()) {
                Source s;
                s.pdf = std::make_unique<QPDF>();
                s.pdf->processFile(key.c_str());
                QPDFPageDocumentHelper helper(*s.pdf);
                helper.pushInheritedAttributesToPage();  // size, rotation and resources set on the page tree
                s.pages = helper.getAllPages();
                version = std::max(version, std::atof(s.pdf->getPDFVersion().c_str()));
                it = sources.emplace(key, std::move(s)).first;
            }
            if (piece.pageIndex >= it->second.pages.size()) {
                result.error = "Page " + std::to_string(piece.pageIndex + 1) + " does not exist in \"" +
                               utf8(piece.file.filename()) + "\".";
                return result;
            }
            outPages.addPage(it->second.pages[piece.pageIndex], false);
            if (piece.rotateDegrees != 0) {
                outPages.getAllPages().back().rotatePage(piece.rotateDegrees, true);
            }
        }

        QPDFWriter writer(out, utf8(staged).c_str());
        writer.setStreamDataMode(qpdf_s_preserve);
        writer.setMinimumPDFVersion(std::to_string(version).substr(0, 3));
        writer.write();
    } catch (const std::exception& e) {
        fs::remove(staged, ec);
        result.error = std::string("The PDF could not be written: ") + e.what();
        return result;
    }

    fs::rename(staged, output, ec);
    if (ec) {
        result.error = "The PDF could not be saved to the chosen file: " + ec.message();
        fs::remove(staged, ec);
        return result;
    }
    result.ok = true;
    result.pages = pieces.size();
    return result;
}

#endif  // ENABLE_QPDF

Result mergeFiles(const std::vector<fs::path>& inputs, const fs::path& output) {
    Result result;
    if (inputs.size() < 2) {
        result.error = "Choose at least two PDFs to merge.";
        return result;
    }
    std::vector<Piece> pieces;
    for (const auto& file: inputs) {
        std::string error;
        const long n = pageCount(file, &error);
        if (n < 0) {
            result.error = "\"" + utf8(file.filename()) + "\": " + error;
            return result;
        }
        for (long i = 0; i < n; ++i) {
            pieces.push_back({file, static_cast<size_t>(i), 0});
        }
    }
    return assemble(pieces, output);
}

Result extractPages(const fs::path& input, const std::string& ranges, const fs::path& output) {
    Result result;
    std::string error;
    const long n = pageCount(input, &error);
    if (n < 0) {
        result.error = error;
        return result;
    }
    const auto pages = parsePageRanges(ranges, static_cast<size_t>(n), &error);
    if (pages.empty()) {
        result.error = error;
        return result;
    }
    std::vector<Piece> pieces;
    for (size_t p: pages) {
        pieces.push_back({input, p, 0});
    }
    return assemble(pieces, output);
}

Result splitByPageCount(const fs::path& input, size_t pagesPerFile, const fs::path& folder, const std::string& stem,
                        std::vector<fs::path>* written) {
    Result result;
    if (pagesPerFile == 0) {
        result.error = "Pages per file must be at least 1.";
        return result;
    }
    std::string error;
    const long n = pageCount(input, &error);
    if (n < 0) {
        result.error = error;
        return result;
    }
    const size_t total = static_cast<size_t>(n);
    const size_t parts = (total + pagesPerFile - 1) / pagesPerFile;
    if (parts < 2) {
        result.error = "The PDF has no more than " + std::to_string(pagesPerFile) + " pages, so there is nothing to split.";
        return result;
    }
    std::vector<fs::path> targets;
    std::error_code ec;
    for (size_t part = 0; part < parts; ++part) {
        fs::path name = fs::u8path(stem + " (part " + std::to_string(part + 1) + ").pdf");
        targets.push_back(folder / name);
        if (fs::exists(fs::symlink_status(targets.back(), ec))) {
            result.error = "\"" + utf8(name) + "\" already exists. Nothing was written.";
            return result;
        }
    }
    std::vector<fs::path> done;
    for (size_t part = 0; part < parts; ++part) {
        std::vector<Piece> pieces;
        for (size_t p = part * pagesPerFile; p < std::min(total, (part + 1) * pagesPerFile); ++p) {
            pieces.push_back({input, p, 0});
        }
        const Result r = assemble(pieces, targets[part]);
        if (!r.ok) {
            for (const auto& f: done) {
                fs::remove(f, ec);  // all or nothing
            }
            result.error = r.error;
            return result;
        }
        done.push_back(targets[part]);
    }
    if (written) {
        *written = done;
    }
    result.ok = true;
    result.pages = total;
    return result;
}

}  // namespace utn::pdftools
