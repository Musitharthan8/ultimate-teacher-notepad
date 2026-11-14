#include "PdfOcr.h"

#include <algorithm>
#include <cstdlib>
#include <system_error>
#include <tuple>

#include "config-features.h"
#include "util/PathUtil.h"

#ifdef ENABLE_OCR
#include <atomic>
#include <memory>

#include <cairo.h>
#include <glib.h>
#include <leptonica/allheaders.h>
#include <poppler.h>
#include <tesseract/baseapi.h>
#include <tesseract/renderer.h>
#endif

namespace utn::ocr {

#ifndef UTN_TESSDATA_HINT
#define UTN_TESSDATA_HINT ""
#endif

bool compiledIn() {
#ifdef ENABLE_OCR
    return true;
#else
    return false;
#endif
}

static bool hasAnyLanguageData(const fs::path& dir) {
    std::error_code ec;
    if (dir.empty() || !fs::is_directory(dir, ec)) {
        return false;
    }
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->path().extension() == ".traineddata" && it->path().stem() != "osd") {
            return true;
        }
    }
    return false;
}

fs::path findTessdataDir() {
    if (!compiledIn()) {
        return {};
    }
    std::vector<fs::path> candidates;
    if (const char* env = std::getenv("TESSDATA_PREFIX"); env && *env) {
        fs::path p = fs::u8path(env);
        candidates.push_back(p);
        candidates.push_back(p / "tessdata");
    }
    if (*UTN_TESSDATA_HINT) {
        candidates.push_back(fs::u8path(UTN_TESSDATA_HINT));
    }
    for (const auto& dir: candidates) {
        if (hasAnyLanguageData(dir)) {
            return dir;
        }
    }
    return {};
}

std::vector<std::string> availableLanguages(const fs::path& tessdataDir) {
    std::vector<std::string> languages;
    const fs::path dir = tessdataDir.empty() ? findTessdataDir() : tessdataDir;
    std::error_code ec;
    if (dir.empty() || !fs::is_directory(dir, ec)) {
        return languages;
    }
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->path().extension() == ".traineddata") {
            const std::string name = it->path().stem().string();
            if (name != "osd") {
                languages.push_back(name);
            }
        }
    }
    std::sort(languages.begin(), languages.end());
    return languages;
}

#ifndef ENABLE_OCR

int countPagesWithText(const fs::path&) { return -1; }

Result makeSearchable(const fs::path&, const fs::path&, const Options&, const Progress&) {
    Result r;
    r.error = "This build of UTN was made without OCR support.";
    return r;
}

#else

namespace {
struct DocDeleter {
    void operator()(PopplerDocument* d) const { g_object_unref(d); }
};
struct PageDeleter {
    void operator()(PopplerPage* p) const { g_object_unref(p); }
};
struct SurfaceDeleter {
    void operator()(cairo_surface_t* s) const { cairo_surface_destroy(s); }
};
struct PixDeleter {
    void operator()(Pix* p) const { pixDestroy(&p); }
};
using DocPtr = std::unique_ptr<PopplerDocument, DocDeleter>;
using PagePtr = std::unique_ptr<PopplerPage, PageDeleter>;
using SurfacePtr = std::unique_ptr<cairo_surface_t, SurfaceDeleter>;
using PixPtr = std::unique_ptr<Pix, PixDeleter>;

DocPtr openPdf(const fs::path& pdf) {
    auto uri = Util::toUri(pdf);
    if (!uri) {
        return nullptr;
    }
    GError* error = nullptr;
    DocPtr doc(poppler_document_new_from_file(uri->c_str(), nullptr, &error));
    if (error) {
        g_error_free(error);
    }
    return doc;
}

/// Renders one page to a Leptonica picture at `dpi`
PixPtr renderPage(PopplerPage* page, int dpi) {
    double w = 0;
    double h = 0;
    poppler_page_get_size(page, &w, &h);
    const double scale = dpi / 72.0;
    const int pw = std::max(1, static_cast<int>(w * scale + 0.5));
    const int ph = std::max(1, static_cast<int>(h * scale + 0.5));
    SurfacePtr surface(cairo_image_surface_create(CAIRO_FORMAT_RGB24, pw, ph));
    if (cairo_surface_status(surface.get()) != CAIRO_STATUS_SUCCESS) {
        return nullptr;
    }
    cairo_t* cr = cairo_create(surface.get());
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    cairo_scale(cr, static_cast<double>(pw) / w, static_cast<double>(ph) / h);
    poppler_page_render(page, cr);
    cairo_destroy(cr);
    cairo_surface_flush(surface.get());

    PixPtr pix(pixCreate(pw, ph, 32));
    if (!pix) {
        return nullptr;
    }
    const unsigned char* src = cairo_image_surface_get_data(surface.get());
    const int stride = cairo_image_surface_get_stride(surface.get());
    l_uint32* dst = pixGetData(pix.get());
    const int wpl = pixGetWpl(pix.get());
    for (int y = 0; y < ph; ++y) {
        const auto* row = reinterpret_cast<const uint32_t*>(src + static_cast<size_t>(y) * stride);
        l_uint32* out = dst + static_cast<size_t>(y) * wpl;
        for (int x = 0; x < pw; ++x) {
            const uint32_t argb = row[x];  // cairo RGB24: 0x00RRGGBB in native order
            const l_uint32 r = (argb >> 16) & 0xff;
            const l_uint32 g = (argb >> 8) & 0xff;
            const l_uint32 b = argb & 0xff;
            out[x] = (r << 24) | (g << 16) | (b << 8);  // Leptonica: R in the top byte
        }
    }
    pixSetResolution(pix.get(), dpi, dpi);
    return pix;
}

fs::path scratchFolder() {
    // Tesseract takes narrow file names, so the work files live in a folder of ours
    fs::path dir = Util::getTmpDirSubfolder("ocr");
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

fs::path scratchBase() {
    static std::atomic<unsigned> counter{0};
    return scratchFolder() / ("run-" + std::to_string(g_get_real_time()) + "-" + std::to_string(counter++));
}

/// Leptonica writes JPEG data for the PDF through a temporary file in %TMP% (GetTempFileName), which fails when that
/// folder is missing or unusable. For the duration of one run, point TMP and TEMP at our own scratch folder.
class TempDirGuard {
public:
    explicit TempDirGuard(const fs::path& dir) {
#ifdef _WIN32
        for (const char* name: {"TMP", "TEMP"}) {
            const char* old = std::getenv(name);
            saved.emplace_back(name, old ? std::string(old) : std::string(), old != nullptr);
            _putenv_s(name, dir.string().c_str());
        }
#else
        (void)dir;
#endif
    }
    ~TempDirGuard() {
#ifdef _WIN32
        for (const auto& [name, value, was]: saved) {
            _putenv_s(name.c_str(), was ? value.c_str() : "");
        }
#endif
    }
    TempDirGuard(const TempDirGuard&) = delete;
    TempDirGuard& operator=(const TempDirGuard&) = delete;

private:
#ifdef _WIN32
    std::vector<std::tuple<std::string, std::string, bool>> saved;
#endif
};
}  // namespace

int countPagesWithText(const fs::path& pdf) {
    DocPtr doc = openPdf(pdf);
    if (!doc) {
        return -1;
    }
    int withText = 0;
    const int n = poppler_document_get_n_pages(doc.get());
    for (int i = 0; i < n; ++i) {
        PagePtr page(poppler_document_get_page(doc.get(), i));
        if (!page) {
            continue;
        }
        char* text = poppler_page_get_text(page.get());
        if (text) {
            for (const char* c = text; *c; ++c) {
                if (!g_ascii_isspace(*c)) {
                    ++withText;
                    break;
                }
            }
            g_free(text);
        }
    }
    return withText;
}

Result makeSearchable(const fs::path& input, const fs::path& output, const Options& options,
                      const Progress& progress) {
    Result result;
    const fs::path tessdata = options.tessdataDir.empty() ? findTessdataDir() : options.tessdataDir;
    if (tessdata.empty()) {
        result.error = "No OCR language data was found. Install the Tesseract language files (tessdata).";
        return result;
    }
    DocPtr doc = openPdf(input);
    if (!doc) {
        result.error = "The PDF could not be opened.";
        return result;
    }
    const int total = poppler_document_get_n_pages(doc.get());
    if (total <= 0) {
        result.error = "The PDF has no pages.";
        return result;
    }

    tesseract::TessBaseAPI api;
    const std::string tessdataStr = tessdata.string();
    if (api.Init(tessdataStr.c_str(), options.language.c_str()) != 0) {
        result.error = "The OCR language \"" + options.language + "\" could not be loaded.";
        return result;
    }

    const TempDirGuard tempDir(scratchFolder());
    const fs::path base = scratchBase();
    const std::string baseStr = base.string();
    fs::path produced = base;
    produced += ".pdf";
    std::error_code ec;
    {
        tesseract::TessPDFRenderer renderer(baseStr.c_str(), tessdataStr.c_str(), false);
        if (!renderer.BeginDocument("")) {
            result.error = "The OCR output could not be started.";
            return result;
        }
        for (int i = 0; i < total; ++i) {
            PagePtr page(poppler_document_get_page(doc.get(), i));
            PixPtr pix = page ? renderPage(page.get(), options.dpi) : nullptr;
            if (!pix) {
                result.error = "Page " + std::to_string(i + 1) + " could not be rendered.";
                break;
            }
            if (!api.ProcessPage(pix.get(), i, "", nullptr, 0, &renderer)) {
                result.error = "Page " + std::to_string(i + 1) + " could not be recognised.";
                break;
            }
            if (progress && !progress(i + 1, total)) {
                result.cancelled = true;
                result.error = "cancelled";
                break;
            }
        }
        if (result.error.empty()) {
            renderer.EndDocument();
        }
    }
    api.End();

    if (!result.error.empty()) {
        fs::remove(produced, ec);
        return result;
    }

    // Move the finished file into place through a name beside the destination, so `output` is replaced in one step
    fs::path staged = output;
    staged += ".utn-part";
    fs::copy_file(produced, staged, fs::copy_options::overwrite_existing, ec);
    fs::remove(produced, ec);
    if (ec) {
        result.error = "The searchable PDF could not be written: " + ec.message();
        fs::remove(staged, ec);
        return result;
    }
    fs::rename(staged, output, ec);
    if (ec) {
        result.error = "The searchable PDF could not be saved to the chosen file: " + ec.message();
        fs::remove(staged, ec);
        return result;
    }
    result.ok = true;
    result.pages = total;
    return result;
}

#endif  // ENABLE_OCR

}  // namespace utn::ocr
