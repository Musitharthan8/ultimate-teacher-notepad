#include "LoadHandler.h"

#include <algorithm>      // for copy
#include <array>          // for array
#include <cmath>          // for isnan
#include <cstddef>        // for byte
#include <cstdlib>        // for atoi, size_t
#include <cstring>        // for strcmp, strlen
#include <iterator>       // for back_inserter
#include <limits>         // for numeric_limits
#include <memory>         // for make_unique, make_shared...
#include <optional>       // for optional
#include <ranges>         // for take
#include <regex>          // for regex_search, match_results
#include <stdexcept>      // for runtime_error
#include <string>         // for string
#include <string_view>    // for string_view
#include <system_error>   // for error_code
#include <unordered_map>  // for unordered_map
#include <utility>        // for move, forward, exchange
#include <vector>         // for vector

#include <gdk-pixbuf/gdk-pixbuf.h>  // for GdkPixbufLoader
#include <gio/gio.h>     // for g_file_get_path, g_fil...
#include <glib.h>        // for g_message...
#include <glibconfig.h>  // for gssize...
#include <zip.h>         // for zip_file_t, zip_fopen,...
#include <zipconf.h>     // for zip_int64_t, zip_uint64_t

#include "control/xojfile/XmlParser.h"  // for XmlParser
#include "model/BackgroundImage.h"      // for BackgroundImage
#include "model/Document.h"             // for Document
#include "model/LayerAudience.h"        // for prepareLoadedLayer
#include "model/Font.h"                 // for XojFont
#include "model/Image.h"                // for Image
#include "model/Layer.h"                // for Layer
#include "model/Link.h"                 // for Link
#include "model/PageType.h"             // for PageType, PageTypeFormat
#include "model/Point.h"                // for Point
#include "model/Stroke.h"               // for Stroke, StrokeCapStyle
#include "model/eraser/MaskedStrokeCodec.h"
#include "model/TexImage.h"             // for TexImage
#include "model/Text.h"                 // for Text
#include "model/XojPage.h"              // for XojPage
#include "util/Assert.h"                // for xoj_assert
#include "util/BoundedPixbuf.h"         // for limitGdkPixbufLoaderPixels
#include "util/Color.h"                 // for Color
#include "util/GzInputStream.h"         // for GzInputStream
#include "util/LoopUtil.h"              // for for_first_then_each
#include "util/Matrix.h"                // for Matrix
#include "util/PathUtil.h"              // for PathStorageMode
#include "util/StringUtils.h"           // for char_cast
#include "util/ZipInputStream.h"        // for ZipInputStream
#include "util/i18n.h"                  // for _F, FS, _
#include "util/raii/CLibrariesSPtr.h"   // for adopt
#include "util/raii/GLibGuards.h"       // for GErrorGuard
#include "util/raii/GObjectSPtr.h"      // for GObjectSPtr

#include "filesystem.h"  // for path, is_regular_file


namespace {
constexpr size_t MAX_VERSION_LENGTH = 50;
constexpr size_t MAX_MIMETYPE_LENGTH = 25;

struct zip_file_deleter {
    void operator()(zip_file_t* ptr) noexcept { zip_fclose(ptr); }
};

/// "64 MiB", or "4 KiB" for limits below 1 MiB (only seen in tests)
std::string describeSize(uint64_t bytes) {
    if (bytes >= xoj::load::MiB || bytes % 1024 != 0) {
        return std::to_string(bytes / xoj::load::MiB) + " MiB";
    }
    return std::to_string(bytes / 1024) + " KiB";
}

/**
 * Image::setImage() asserts when the data is not a picture GdkPixbuf recognises (and leaves a null format in release
 * builds). Corrupt or hostile data must be refused here instead of ending the program.
 * This mirrors the detection of Image::setImage(): it looks at the first 4096 bytes only.
 */
bool isRecognisedPicture(const std::string& data) {
    if (data.empty()) {
        return false;
    }
    const xoj::util::GObjectSPtr<GdkPixbufLoader> loader(gdk_pixbuf_loader_new(), xoj::util::adopt);
    xoj::util::limitGdkPixbufLoaderPixels(loader.get());
    const size_t head = std::min<size_t>(data.size(), 4096);
    if (gdk_pixbuf_loader_write(loader.get(), reinterpret_cast<const guchar*>(data.data()), head, nullptr)) {
        if (gdk_pixbuf_loader_get_format(loader.get())) {
            gdk_pixbuf_loader_close(loader.get(), nullptr);
            return true;
        }
    }
    gdk_pixbuf_loader_close(loader.get(), nullptr);
    return gdk_pixbuf_loader_get_format(loader.get()) != nullptr;
}

auto maxBase64EncodedSize(uint64_t decodedLimit) -> std::optional<size_t> {
    if (decodedLimit > std::numeric_limits<size_t>::max() || decodedLimit > UINT64_MAX - 2) {
        return std::nullopt;
    }
    const uint64_t groups = (decodedLimit + 2) / 3;
    if (groups > (std::numeric_limits<size_t>::max() - 16) / 4) {
        return std::nullopt;
    }
    const size_t chars = static_cast<size_t>(groups * 4);
    const size_t lines = chars / 76 + (chars % 76 != 0);
    if (lines > (std::numeric_limits<size_t>::max() - chars - 16) / 2) {
        return std::nullopt;
    }
    return chars + lines * 2 + 16;
}

auto decodeBase64Bounded(std::string_view encoded, uint64_t decodedLimit) -> std::optional<std::string> {
    size_t symbols = 0;
    size_t padding = 0;
    bool sawPadding = false;
    for (const unsigned char c: encoded) {
        if (g_ascii_isspace(c)) {
            continue;
        }
        if (c == '=') {
            sawPadding = true;
            if (++padding > 2) {
                return std::nullopt;
            }
            continue;
        }
        const bool base64Char = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                                c == '+' || c == '/';
        if (!base64Char || sawPadding) {
            return std::nullopt;
        }
        symbols++;
    }
    const size_t remainder = symbols % 4;
    if (remainder == 1 || (padding != 0 && (padding > 2 || (symbols + padding) % 4 != 0 ||
                                             (padding == 1 && remainder != 3) ||
                                             (padding == 2 && remainder != 2)))) {
        return std::nullopt;
    }
    const size_t groups = symbols / 4;
    const size_t tail = remainder == 0 ? 0 : remainder - 1;
    if (groups > (std::numeric_limits<size_t>::max() - tail) / 3) {
        return std::nullopt;
    }
    const size_t decodedMax = groups * 3 + tail;
    if (decodedMax > decodedLimit) {
        return std::nullopt;
    }

    std::string decoded(decodedMax, '\0');
    gint state = 0;
    guint save = 0;
    const size_t decodedSize = g_base64_decode_step(encoded.data(), encoded.size(),
                                                     reinterpret_cast<guchar*>(decoded.data()), &state, &save);
    if (decodedSize > decodedMax) {
        return std::nullopt;
    }
    decoded.resize(decodedSize);
    return decoded;
}
}  // namespace

using zip_file_wrapper = std::unique_ptr<zip_file_t, zip_file_deleter>;

namespace {
struct TemporaryAudioGuard {
    GFile* file;
    bool keep = false;
    ~TemporaryAudioGuard() {
        if (this->file && !this->keep) {
            g_file_delete(this->file, nullptr, nullptr);
        }
    }
};
}  // namespace

LoadHandler::LoadHandler(std::vector<std::string>* errorMessages):
        parsingComplete(false),
        errorMessages(errorMessages),
        attachedPdfMissing(false),
        pdfFilenameParsed(false),
        fileVersion(0),
        minimalFileVersion(0),
        isGzFile(false) {}

LoadHandler::~LoadHandler() { removeExtractedAudio(); }

auto LoadHandler::isAttachedPdfMissing() const -> bool { return this->attachedPdfMissing; }

auto LoadHandler::getMissingPdfFilename() const -> const fs::path& { return this->missingPdf; }

auto LoadHandler::getFileVersion() const -> int { return this->fileVersion; }

void LoadHandler::addDocument(std::u8string creator, int fileVersion) {
    this->creator = std::move(creator);
    if (this->isGzFile) {
        this->fileVersion = fileVersion;
    }

    this->doc = std::make_unique<Document>(&this->dHandler);
}

void LoadHandler::finalizeDocument() {
    xoj_assert(this->doc);

    this->doc->addPages(this->pages.begin(), this->pages.end());
    this->pages.clear();
    this->parsingComplete = true;
}

void LoadHandler::addPage(double width, double height, std::string utnPageLabel) {
    xoj_assert(!this->page);

    this->page = std::make_shared<XojPage>(width, height, /*suppressLayerCreation*/ true);
    this->page->setUtnPageLabel(std::move(utnPageLabel));
    this->pages.emplace_back(this->page);
}

void LoadHandler::finalizePage() {
    xoj_assert(this->page);

    // Handle unnecessary layer insertion in case of existing layers in file
    if (this->page->getLayerCount() == 0) {
        this->page->addLayer(new Layer{});
    }
    // A page selects its top layer. Answers layers sit on top but open hidden, so writing there would be invisible:
    // select the topmost layer students may see instead (as hiding answers does)
    auto& layers = this->page->getLayers();
    if (utn::isHiddenAnswers(*layers.back())) {
        if (auto writable = utn::writableLayerIdAfterHide(layers)) {
            this->page->setSelectedLayerId(*writable);
        }
    }
    // this->pages already holds a reference to the page
    this->page.reset();
}

void LoadHandler::setAttachmentLimits(const xoj::load::AttachmentLimits& newLimits) { this->limits = newLimits; }

auto LoadHandler::decodeInlineImage(std::string_view encoded, uint64_t perImageLimit) -> std::shared_ptr<std::string> {
    if (encoded.empty()) {
        return {};
    }
    const auto countAdmission = admitAttachment(0, perImageLimit, this->limits.maxMemoryBudgetBytes,
                                                this->memoryBytesUsed, true);
    if (countAdmission == Admission::CountExceeded) {
        logAttachmentRefused("inline image", countAdmission);
        return {};
    }
    const auto encodedLimit = maxBase64EncodedSize(perImageLimit);
    if (!encodedLimit || encoded.size() > *encodedLimit) {
        logError(FS(_F("An inline image is larger than the {1} allowed and was skipped.") %
                    describeSize(perImageLimit)));
        return {};
    }
    // Bound the temporary decoded string before allocating it. Whitespace is legal in XML base64 and does not
    // contribute to the upper bound.
    size_t symbols = 0;
    size_t padding = 0;
    for (const unsigned char c: encoded) {
        if (!g_ascii_isspace(c)) {
            if (symbols == std::numeric_limits<size_t>::max()) {
                logAttachmentRefused("inline image", Admission::BudgetExceeded);
                return {};
            }
            symbols++;
            if (c == '=') {
                padding++;
            }
        }
    }
    if (symbols > std::numeric_limits<size_t>::max() - 3) {
        logAttachmentRefused("inline image", Admission::BudgetExceeded);
        return {};
    }
    const size_t decodedUpperBound = ((symbols + 3) / 4) * 3 - std::min<size_t>(padding, 2);
    if (decodedUpperBound > this->limits.maxMemoryBudgetBytes - this->memoryBytesUsed) {
        logAttachmentRefused("inline image", Admission::BudgetExceeded);
        return {};
    }
    auto decoded = decodeBase64Bounded(encoded, perImageLimit);
    if (!decoded) {
        logError(_("An inline image is damaged or has invalid base64 data and was skipped."));
        return {};
    }
    this->memoryBytesUsed += decoded->size();
    return std::make_shared<std::string>(std::move(*decoded));
}

auto LoadHandler::admitAttachment(uint64_t declaredBytes, uint64_t perAttachmentLimit, uint64_t budgetBytes,
                                  uint64_t usedBytes, bool countAttachment) -> Admission {
    if (countAttachment && this->attachmentCount >= this->limits.maxAttachmentCount) {
        return Admission::CountExceeded;
    }
    if (countAttachment) {
        this->attachmentCount++;
    }
    if (declaredBytes > perAttachmentLimit) {
        return Admission::TooLarge;
    }
    // Written as a subtraction: usedBytes never exceeds budgetBytes, so this cannot wrap around
    if (declaredBytes > budgetBytes - usedBytes) {
        return Admission::BudgetExceeded;
    }
    return Admission::Ok;
}

void LoadHandler::logAttachmentRefused(const fs::path& filename, Admission reason) {
    // The limits apply to the whole document: a hostile file would otherwise get one message per attachment
    if (this->limitErrorLogged) {
        return;
    }
    this->limitErrorLogged = true;
    if (reason == Admission::CountExceeded) {
        logError(FS(_F("This document has more than {1} attachments. The remaining ones, starting with \"{2}\", were "
                       "skipped.") %
                    this->limits.maxAttachmentCount % filename.u8string()));
    } else {
        logError(FS(_F("The attachments of this document are larger than the allowed total. The remaining ones, "
                       "starting with \"{1}\", were skipped.") %
                    filename.u8string()));
    }
}

void LoadHandler::logAttachmentFailure(const fs::path& filename, xoj::util::zip::ReadStatus status,
                                       uint64_t limitBytes) {
    using xoj::util::zip::ReadStatus;
    switch (status) {
        case ReadStatus::TooLarge:
        case ReadStatus::SizeNotRepresentable:
            logError(FS(_F("Attachment \"{1}\" is larger than the {2} allowed and was skipped.") % filename.u8string() %
                        describeSize(limitBytes)));
            break;
        case ReadStatus::SinkError:
            logError(FS(_F("Attachment \"{1}\" could not be stored. Is the disk full?") % filename.u8string()));
            break;
        case ReadStatus::OutOfMemory:
            logError(FS(_F("Not enough memory to load attachment \"{1}\".") % filename.u8string()));
            break;
        case ReadStatus::Ok:
            break;
        default:  // SizeUnknown, Truncated, LargerThanDeclared, ReadError
            logError(FS(_F("Attachment \"{1}\" is damaged (truncated, inconsistent or unreadable) and was skipped.") %
                        filename.u8string()));
            break;
    }
}

void LoadHandler::addAudioAttachment(const fs::path& filename) {
    namespace zipx = xoj::util::zip;
    const std::string entryName = char_cast(filename.u8string().c_str());
    const zip_int64_t located = zip_name_locate(this->zipFp.get(), entryName.c_str(), ZIP_FL_ENC_GUESS);
    if (located < 0) {
        if (this->missingZipEntries.emplace(entryName).second) {
            logError(FS(_F("Could not open attachment: {1}. Error message: {2}") % filename.u8string() %
                        zip_error_strerror(zip_get_error(this->zipFp.get()))));
        }
        return;
    }
    const auto index = static_cast<zip_uint64_t>(located);
    auto& cached = this->audioAttachments[index];
    if (cached.attempted) {
        if (!cached.tempPath.empty()) {
            this->audioFiles[filename] = cached.tempPath;
        }
        return;
    }
    cached.attempted = true;
    zip_stat_t attachmentFileStat;
    const int statStatus = zip_stat_index(this->zipFp.get(), index, ZIP_FL_UNCHANGED, &attachmentFileStat);
    if (statStatus != 0) {
        logError(FS(_F("Could not open attachment: {1}. Error message: {2}") % filename.u8string() %
                    zip_error_strerror(zip_get_error(this->zipFp.get()))));
        return;
    }

    const auto declared = zipx::declaredSize(attachmentFileStat);
    if (!declared) {
        cached.status = zipx::ReadStatus::SizeUnknown;
        logAttachmentFailure(filename, zipx::ReadStatus::SizeUnknown, this->limits.maxAudioBytes);
        return;
    }

    const bool countAttachment = !this->countedZipEntries.contains(index);
    const auto admission = admitAttachment(*declared, this->limits.maxAudioBytes, this->limits.maxDiskBudgetBytes,
                                           this->diskBytesUsed, countAttachment);
    if (countAttachment && admission != Admission::CountExceeded) {
        this->countedZipEntries.emplace(index);
    }
    if (admission == Admission::TooLarge) {
        cached.status = zipx::ReadStatus::TooLarge;
        logAttachmentFailure(filename, zipx::ReadStatus::TooLarge, this->limits.maxAudioBytes);
        return;
    }
    if (admission != Admission::Ok) {
        logAttachmentRefused(filename, admission);
        return;
    }
    // Reserve the announced processing budget before reading. Failed CRC/EOF reads consume it too.
    this->diskBytesUsed += *declared;

    const auto attachmentFile = zip_file_wrapper{zip_fopen_index(this->zipFp.get(), index, ZIP_FL_UNCHANGED)};

    if (!attachmentFile) {
        logError(FS(_F("Could not open attachment: {1}. Error message: {2}") % filename.u8string() %
                    zip_error_strerror(zip_get_error(this->zipFp.get()))));
        return;
    }

    // Extract audio to temporary file
    GFileIOStream* fileStream = nullptr;
    const xoj::util::GObjectSPtr<GFile> tmpFile(g_file_new_tmp("xournal_audio_XXXXXX.tmp", &fileStream, nullptr),
                                                xoj::util::adopt);
    if (!tmpFile) {
        cached.status = zipx::ReadStatus::SinkError;
        logError(_("Unable to create temporary file for audio attachment"));
        return;
    }
    TemporaryAudioGuard temporaryFileGuard{tmpFile.get()};
    const xoj::util::GObjectSPtr<GFileIOStream> ioStream(fileStream, xoj::util::adopt);
    GOutputStream* outputStream = g_io_stream_get_output_stream(G_IO_STREAM(ioStream.get()));

    uint64_t written = 0;
    auto status = zipx::readZipFileBounded(
            attachmentFile.get(), *declared, this->limits.maxAudioBytes,
            [outputStream](const char* data, size_t size) {
                return g_output_stream_write_all(outputStream, data, size, nullptr, nullptr, nullptr) != FALSE;
            },
            written);
    // Close explicitly: a failing close (e.g. the final flush on a full disk) is a failed extraction
    if (!g_io_stream_close(G_IO_STREAM(ioStream.get()), nullptr, nullptr) && status == zipx::ReadStatus::Ok) {
        status = zipx::ReadStatus::SinkError;
    }

    if (status != zipx::ReadStatus::Ok) {
        // Do not leave a partial recording behind in the temp folder
        cached.status = status;
        logAttachmentFailure(filename, status, this->limits.maxAudioBytes);
        return;
    }

    // Map the filename to the extracted temporary file path
    const char* tmpPath = g_file_peek_path(tmpFile.get());
    cached.status = zipx::ReadStatus::Ok;
    cached.tempPath = tmpPath;
    this->audioFiles[filename] = cached.tempPath;
    temporaryFileGuard.keep = true;
}

void LoadHandler::removeExtractedAudio() noexcept {
    if (!this->audioFilesOwned) {
        return;
    }
    for (const auto& [attachment, tmpPath]: this->audioFiles) {
        std::error_code ec;
        fs::remove(tmpPath, ec);  // Best effort: nothing more can be done if it fails
    }
    this->audioFiles.clear();
}

void LoadHandler::setBgName(const std::string& name) {
    xoj_assert(!name.empty());
    this->page->setBackgroundName(name);
}

void LoadHandler::setBgSolid(const PageType& bg, Color color) {
    this->page->setBackgroundType(bg);
    this->page->setBackgroundColor(color);
}

void LoadHandler::setBgPixmap(bool attach, const fs::path& filename) {
    if (this->isGzFile || !attach) {
        const fs::path fileToLoad = getAbsoluteFilepath(filename, attach);
        BackgroundImage img;
        xoj::util::GErrorGuard error{};
        img.loadFile(fileToLoad, xoj::util::out_ptr(error));
        if (error || !img.getPixbuf()) {
            const std::string reason = error ? error->message : "image decoder returned no pixels";
            logError(FS(_F("Could not read image: {1}. Error message: {2}") % fileToLoad.u8string() % reason));
            return;
        }
        img.setAttach(attach);
        this->page->setBackgroundImage(img);
        this->page->setBackgroundType(PageType{PageTypeFormat::Image});
        return;
    } else {
        const std::string entryName = char_cast(filename.u8string().c_str());
        const zip_int64_t located = zip_name_locate(this->zipFp.get(), entryName.c_str(), ZIP_FL_ENC_GUESS);
        if (located < 0) {
            if (this->missingZipEntries.emplace(entryName).second) {
                logError(FS(_F("Could not open attachment: {1}. Error message: {2}") % filename.u8string() %
                            zip_error_strerror(zip_get_error(this->zipFp.get()))));
            }
            return;
        }
        auto& cached = this->backgroundAttachments[static_cast<zip_uint64_t>(located)];
        if (!cached.attempted) {
            cached.attempted = true;
            const auto data = readZipAttachment(filename, AttachmentKind::Image);
            if (data) {
                BackgroundImage image;
                const xoj::util::GObjectSPtr<GInputStream> inputStream{
                        g_memory_input_stream_new_from_data(data->data(), static_cast<gssize>(data->size()), nullptr),
                        xoj::util::adopt};
                xoj::util::GErrorGuard error{};
                image.loadFile(inputStream.get(), filename, xoj::util::out_ptr(error));
                g_input_stream_close(inputStream.get(), nullptr, nullptr);
                if (error || !image.getPixbuf()) {
                    const std::string reason = error ? error->message : "image decoder returned no pixels";
                    logError(FS(_F("Could not read image attachment: {1}. Error message: {2}") % filename.u8string() %
                                reason));
                } else {
                    image.setAttach(true);
                    cached.image = std::move(image);
                    cached.valid = true;
                }
            }
        }
        if (!cached.valid) {
            return;
        }
        this->page->setBackgroundImage(cached.image);
        this->page->setBackgroundType(PageType{PageTypeFormat::Image});
        return;
    }
}

void LoadHandler::setBgPixmapCloned(size_t pageNr) {
    if (pageNr >= this->doc->getPageCount()) {
        logError(FS(_F("Cloned pixmap page background on page {1} references inexistent page {2}") %
                    this->doc->getPageCount() % pageNr));
        return;
    }

    auto p = this->doc->getPage(pageNr);
    if (p) {
        this->page->setBackgroundImage(p->getBackgroundImage());
    }
    this->page->setBackgroundType(PageType{PageTypeFormat::Image});
}

void LoadHandler::setBgPdf(size_t pageno) { this->page->setBackgroundPdfPageNr(pageno); }

void LoadHandler::loadBgPdf(bool attach, const fs::path& filename) {
    if (this->isGzFile || !attach) {
        // Resolve correct file path
        const fs::path pdfFilepath = getAbsoluteFilepath(filename, attach);

        // pdfFilepath should point to the background PDF file
        if (fs::is_regular_file(pdfFilepath)) {
            this->doc->readPdf(pdfFilepath, false, attach);

            if (!this->doc->getLastErrorMsg().empty()) {
                logError(FS(_F("Error reading PDF: {1}") % this->doc->getLastErrorMsg()));
            }
        } else {
            // Even if loading the PDF failed, tell the document what should
            // have been loaded so the background is not silently removed on save
            this->doc->setPdfAttributes(pdfFilepath, attach);
            if (attach) {
                this->attachedPdfMissing = true;
            } else {
                this->missingPdf = pdfFilepath;
            }
        }
    } else {
        // filename should point to an attachment inside the zip archive
        auto pdfBytes = readZipAttachment(filename, AttachmentKind::Pdf);
        if (pdfBytes) {
        this->doc->readPdf(filename, false, attach, std::make_unique<std::string>(*pdfBytes));

            if (!this->doc->getLastErrorMsg().empty()) {
                logError(FS(_F("Error reading PDF: {1}") % this->doc->getLastErrorMsg()));
            }
        } else {
            this->doc->setPdfAttributes(filename, true);
            this->attachedPdfMissing = true;
        }
    }
    this->pdfFilenameParsed = true;
}

void LoadHandler::addLayer(const std::optional<std::string_view>& name) {
    xoj_assert(!this->layer);
    this->layer = std::make_unique<Layer>();
    this->layerAudience.reset();

    if (name) {
        this->layer->setName(std::string{*name});
    }
}

void LoadHandler::setLayerAudience(std::string_view audience) {
    this->layerAudience = utn::audienceFromString(audience);
}

void LoadHandler::finalizeLayer() {
    xoj_assert(this->layer);
    // Answers start hidden; UTN 0.2 files are recognised by their layer names
    utn::prepareLoadedLayer(*this->layer, this->layerAudience);
    this->layerAudience.reset();
    this->page->addLayer(this->layer.release());
}

void LoadHandler::addStroke(StrokeTool tool, Color color, double width, int fill, StrokeCapStyle capStyle,
                            const LineStyle& lineStyle, fs::path filename, size_t timestamp) {
    xoj_assert(!this->stroke);
    this->stroke = std::make_unique<Stroke>();

    this->stroke->setToolType(tool);
    this->stroke->setColor(color);
    this->stroke->setWidth(width);
    this->stroke->setFill(fill);
    this->stroke->setStrokeCapStyle(capStyle);
    this->stroke->setLineStyle(lineStyle);

    setAudioAttributes(*this->stroke, std::move(filename), timestamp);
}

void LoadHandler::setStrokePoints(std::vector<Point> pointVector, bool hasPressure) {
    // Check if stroke still exists or has already been assigned points.
    // In corrupt files, this function may be called more than once, and the
    // stroke may have been deleted in a previous call.
    if (!this->stroke || !this->stroke->getPointVector().empty()) {
        return;
    }

    if (hasPressure && !std::ranges::all_of(
                               pointVector | std::views::take(pointVector.size() - 1),
                               [](double pressure) { return pressure > 0; }, &Point::z)) {
        // Warning: this may delete this->stroke if no positive pressure values are provided
        // Do not dereference this->stroke after that without checking first
        fixNullPressureValues(std::move(pointVector));
    } else {
        this->stroke->setPointVector(std::move(pointVector));
    }
}

void LoadHandler::finalizeStroke() {
    // fixNullPressureValues() may have deleted the stroke. Check if it still
    // exists before accessing it.
    if (this->stroke) {
        if (this->stroke->getPointCount() < 2) {
            // Strokes with less than two points can't be drawn
            g_warning("LoadHandler: Ignoring stroke with less than two points");
            this->stroke.reset();
            return;
        }

        this->layer->addElement(std::move(this->stroke));
    }
}

void LoadHandler::addText(std::string font, double size, xoj::util::Matrix matrix, Color color,
                          std::optional<double> wrap, std::optional<TextAlignment> align, bool justify,
                          bool underlined, bool strikethrough, double lineSpacing, bool boxEnabled,
                          Color boxBackground, Color boxBorder, double boxBorderWidth, double boxPadding,
                          double boxRadius, fs::path filename, size_t timestamp) {
    xoj_assert(!this->text);
    this->text = std::make_unique<Text>();

    XojFont& f = this->text->getFont();
    f.setName(std::move(font));
    f.setSize(size);
    this->text->setTransformation(matrix);
    this->text->setColor(color);
    this->text->setWrap(wrap.value_or(Text::NO_WRAP));
    this->text->setAlignment(align.value_or(TextAlignment::LEFT));
    this->text->setJustify(justify);
    this->text->setUnderlined(underlined);
    this->text->setStrikethrough(strikethrough);
    this->text->setLineSpacing(lineSpacing);
    this->text->setBoxEnabled(boxEnabled);
    this->text->setBoxBackgroundColor(boxBackground);
    this->text->setBoxBorderColor(boxBorder);
    this->text->setBoxBorderWidth(boxBorderWidth);
    this->text->setBoxPadding(boxPadding);
    this->text->setBoxCornerRadius(boxRadius);

    setAudioAttributes(*this->text, std::move(filename), timestamp);
}

void LoadHandler::setTextContents(std::string contents) {
    xoj_assert(this->text);

    this->text->setText(std::move(contents));
}

void LoadHandler::finalizeText() {
    xoj_assert(this->text);

    this->layer->addElement(std::move(this->text));
}

void LoadHandler::addImageLegacy(double left, double top, double right, double bottom,
                                 std::optional<xoj::util::Size<double>> size) {
    xoj_assert(!this->image);
    this->imageBase64Data.clear();
    this->imageBase64Rejected = false;
    this->image = std::make_unique<Image>();
    this->currentElementFrameLegacy = {left, top, right - left, bottom - top};
    this->currentElementNaturalSizeLegacy = size;
}

void LoadHandler::addImage(xoj::util::Matrix matrix) {
    xoj_assert(!this->image);
    this->imageBase64Data.clear();
    this->imageBase64Rejected = false;
    this->image = std::make_unique<Image>();
    this->image->setTransformation(matrix);
}

void LoadHandler::setImageData(std::string data) {
    xoj_assert(this->image);
    if (this->imageBase64Rejected || data.empty()) {
        return;
    }
    const auto maxEncoded = maxBase64EncodedSize(this->limits.maxImageBytes);
    if (!maxEncoded || data.size() > *maxEncoded || this->imageBase64Data.size() > *maxEncoded - data.size()) {
        this->imageBase64Rejected = true;
        this->imageBase64Data.clear();
        logError(FS(_F("An inline image is larger than the {1} allowed and was skipped.") %
                    describeSize(this->limits.maxImageBytes)));
        return;
    }
    this->imageBase64Data.append(data);
}

void LoadHandler::setImageStrokeData(std::string data) { imageStrokeData = std::move(data); }

void LoadHandler::setImageAttachment(const fs::path& filename) {
    xoj_assert(this->image);

    if (this->image->hasData()) {
        g_warning("LoadHandler: Image attachment found, but the image already has data");
    }

    auto imageData = readZipAttachment(filename, AttachmentKind::Image);
    if (imageData) {
        if (!isRecognisedPicture(*imageData)) {
            logAttachmentFailure(filename, xoj::util::zip::ReadStatus::ReadError, this->limits.maxImageBytes);
            return;
        }
        this->image->setImage(std::move(imageData));
    }
}

void LoadHandler::finalizeImage() {
    xoj_assert(this->image);

    if (this->imageBase64Rejected) {
        this->imageBase64Data.clear();
        this->imageBase64Rejected = false;
    } else if (!this->imageBase64Data.empty()) {
        auto data = decodeInlineImage(this->imageBase64Data, this->limits.maxImageBytes);
        this->imageBase64Data.clear();
        if (data) {
            if (isRecognisedPicture(*data)) {
                this->image->setImage(std::move(data));
            } else {
                logError(_("A picture in this document is damaged or in an unsupported format and was skipped."));
            }
        }
    }

    if (!this->image->hasData()) {
        g_warning("LoadHandler: Ignoring image with no data");
        this->image.reset();
        imageStrokeData.clear();
        return;
    }

    if (currentElementFrameLegacy) {
        // To compute the transformation matrix, we need the natural size of the image
        // If it has been read from the xml file, use it, otherwise, we need to load the iamge via getNaturalSize()
        auto [width, height] = currentElementNaturalSizeLegacy ?
                                       *currentElementNaturalSizeLegacy :
                                       this->image->getNaturalSize();  // Effectively loads the image
        if (width != 0 && height != 0) {
            this->image->setTransformation(xoj::util::Matrix::SCALING(currentElementFrameLegacy->width / width,
                                                                      currentElementFrameLegacy->height / height));
        }
        this->image->move(currentElementFrameLegacy->x, currentElementFrameLegacy->y);

        currentElementFrameLegacy = std::nullopt;
        currentElementNaturalSizeLegacy = std::nullopt;
    }

    auto decoded = utn::eraser::decodeMaskedStroke(std::exchange(imageStrokeData, {}));
    if (decoded && utn::eraser::matchesFallbackMatrix(image->getTransformation(), decoded->fallbackMatrix)) {
        this->layer->addElement(std::move(decoded->stroke));
        image.reset();
    } else {
        this->layer->addElement(std::move(this->image));
    }
}

void LoadHandler::addTexImageLegacy(double left, double top, double right, double bottom, std::string text,
                                    std::optional<xoj::util::Size<double>> size) {
    xoj_assert(!this->teximage);
    this->texImageBase64Data.clear();
    this->texImageBase64Rejected = false;
    this->teximage = std::make_unique<TexImage>();

    this->currentElementFrameLegacy = {left, top, right - left, bottom - top};
    this->currentElementNaturalSizeLegacy = size;

    this->teximage->setText(std::move(text));
}

void LoadHandler::addTexImage(xoj::util::Matrix matrix, std::string text) {
    xoj_assert(!this->teximage);
    this->texImageBase64Data.clear();
    this->texImageBase64Rejected = false;
    this->teximage = std::make_unique<TexImage>();
    this->teximage->setTransformation(matrix);
    this->teximage->setText(std::move(text));
}

void LoadHandler::setTexImageData(std::string data) {
    xoj_assert(this->teximage);
    if (this->texImageBase64Rejected || data.empty()) {
        return;
    }
    const auto maxEncoded = maxBase64EncodedSize(this->limits.maxImageBytes);
    if (!maxEncoded || data.size() > *maxEncoded || this->texImageBase64Data.size() > *maxEncoded - data.size()) {
        this->texImageBase64Rejected = true;
        this->texImageBase64Data.clear();
        logError(FS(_F("An inline TeX image is larger than the {1} allowed and was skipped.") %
                    describeSize(this->limits.maxImageBytes)));
        return;
    }
    this->texImageBase64Data.append(data);
}

void LoadHandler::setTexImageAttachment(const fs::path& filename) {
    xoj_assert(this->teximage);

    auto imageData = readZipAttachment(filename, AttachmentKind::Image);
    if (imageData) {
        this->teximage->loadData(std::move(imageData));
    }
}

void LoadHandler::finalizeTexImage() {
    xoj_assert(this->teximage);

    if (this->texImageBase64Rejected) {
        this->texImageBase64Data.clear();
        this->texImageBase64Rejected = false;
    } else if (!this->texImageBase64Data.empty()) {
        auto data = decodeInlineImage(this->texImageBase64Data, this->limits.maxImageBytes);
        this->texImageBase64Data.clear();
        if (data && !this->teximage->loadData(std::move(data))) {
            logError(_("A TeX image in this document is damaged or in an unsupported format and was skipped."));
        }
    }

    if (currentElementFrameLegacy) {
        auto [width, height] =
                currentElementNaturalSizeLegacy ? *currentElementNaturalSizeLegacy : this->teximage->getNaturalSize();
        if (width != 0 && height != 0) [[likely]] {
            this->teximage->setTransformation(xoj::util::Matrix::SCALING(currentElementFrameLegacy->width / width,
                                                                         currentElementFrameLegacy->height / height));
        }
        this->teximage->move(currentElementFrameLegacy->x, currentElementFrameLegacy->y);

        currentElementFrameLegacy = std::nullopt;
        currentElementNaturalSizeLegacy = std::nullopt;
    }

    this->layer->addElement(std::move(this->teximage));
}

void LoadHandler::addLink(TextAlignment align, std::string font, double size, xoj::util::Matrix matrix, Color color,
                          std::string url) {
    this->link = std::make_unique<Link>();

    this->link->setAlignment(align);

    this->link->setUrl(std::string(url.c_str()));

    this->link->setTransformation(matrix);

    XojFont& f = this->link->getFont();
    f.setName(std::move(font));
    f.setSize(size);
    this->link->setColor(color);
}

void LoadHandler::setLinkContent(std::string contents) {
    xoj_assert(this->link);

    this->link->setText(std::move(contents));
}

void LoadHandler::finalizeLink() {
    xoj_assert(this->link);

    this->layer->addElement(std::move(this->link));
}


void LoadHandler::logError(const std::string& error) {
    this->incompleteLoad = true;
    g_warning("LoadHandler: %s", error.c_str());
    if (this->errorMessages) {
        this->errorMessages->emplace_back(error);
    }
}


auto LoadHandler::openFile(fs::path const& filepath) -> std::unique_ptr<xoj::util::InputStream> {
    this->xournalFilepath = filepath;
    int zipError = 0;
    this->zipFp = zip_wrapper{zip_open(char_cast(filepath.u8string().c_str()), ZIP_RDONLY, &zipError)};

    // Check if file is a gzip
    if (!this->zipFp && zipError == ZIP_ER_NOZIP) {
        this->isGzFile = true;
        return std::make_unique<xoj::util::GzInputStream>(filepath);
    }

    // Check if file is a zip
    if (this->zipFp && !this->isGzFile) {
        // Check the mimetype
        auto mimetypeFp = zip_file_wrapper{zip_fopen(this->zipFp.get(), "mimetype", 0)};
        if (!mimetypeFp) {
            throw std::runtime_error{
                    FS(_F("The file \"{1}\" is no valid .xopp file (Mimetype missing). Error message: {2}") %
                       filepath.u8string() % zip_error_strerror(zip_get_error(this->zipFp.get())))};
        }
        std::array<char, MAX_MIMETYPE_LENGTH + 1> mimetype = {};
        // read the mimetype and a few more bytes to make sure we do not only read a subset
        zip_fread(mimetypeFp.get(), mimetype.data(), MAX_MIMETYPE_LENGTH);
        if (!strcmp(mimetype.data(), "application/xournal++")) {
            throw std::runtime_error{FS(_F("The file \"{1}\" is no valid .xopp file (Mimetype wrong): \"{2}\"") %
                                        filepath.u8string() % mimetype.data())};
        }

        // Get the file version
        auto versionFp = zip_file_wrapper{zip_fopen(this->zipFp.get(), "META-INF/version", 0)};
        if (!versionFp) {
            throw std::runtime_error{
                    FS(_F("The file \"{1}\" is no valid .xopp file (Version missing). Error message: {2}") %
                       filepath.u8string() % zip_error_strerror(zip_get_error(zipFp.get())))};
        }
        std::array<char, MAX_VERSION_LENGTH + 1> versionString = {};
        const auto length = std::max(zip_fread(versionFp.get(), versionString.data(), MAX_VERSION_LENGTH),
                                     static_cast<zip_int64_t>(0));
        const std::regex versionRegex{"current=(\\d+?)(?:\n|\r\n)min=(\\d+?)"};
        std::match_results<char*> match;
        if (std::regex_search(versionString.data(), versionString.data() + length, match, versionRegex)) {
            this->fileVersion = std::stoi(match.str(1));
            this->minimalFileVersion = std::stoi(match.str(2));
        } else {
            throw std::runtime_error{
                    FS(_F("The file \"{1}\" is not a valid .xopp file (Version string corrupted): \"{2}\"") %
                       filepath.u8string() % std::string(versionString.begin(), versionString.end()))};
        }

        // open the main content file
        return std::make_unique<xoj::util::ZipInputStream>(this->zipFp.get(), "content.xml");
    }

    // Fail if neither method could open the file
    throw std::runtime_error{FS(_F("Could not open file: \"{1}\"") % filepath.u8string())};
}

void LoadHandler::closeFile() noexcept { this->zipFp.reset(); }

XOJ_GIO_GUARD_GENERATOR_TYPE(GMarkupParseContext, g_markup_parse_context_free);

void LoadHandler::parseXml(std::unique_ptr<xoj::util::InputStream> xmlContentStream) {
    xoj_assert(xmlContentStream);

    XmlParser parserInterface{*this};
    auto context = GMarkupParseContextGuard{g_markup_parse_context_new(
            &XmlParser::interface, static_cast<GMarkupParseFlags>(0), &parserInterface, nullptr)};

    const auto handleGError = [this](xoj::util::GErrorGuard error) -> void {
        if (error) {
            logError(FS(_F("XML Parser error: {1}") % error->message));
        }
    };

    std::array<char, 1024> buffer{};
    int len{};
    uint64_t xmlBytesRead = 0;
    xoj::util::GErrorGuard error;
    while (true) {
        len = xmlContentStream->read(buffer.data(), buffer.size());
        if (len < 0) {
            throw std::runtime_error{_("Failed to read from compressed file")};
        } else if (len == 0) {
            // Reached EOF
            break;
        }

        // The XML is the one part of a document that is not an attachment. A compressed bomb here would bypass
        // every attachment limit.
        xmlBytesRead += static_cast<uint64_t>(len);
        if (xmlBytesRead > this->limits.maxXmlBytes) {
            throw std::runtime_error{FS(_F("The document content is larger than the {1} allowed.") %
                                        describeSize(this->limits.maxXmlBytes))};
        }

        auto valid = g_markup_parse_context_parse(context.get(), buffer.data(), len, xoj::util::out_ptr(error));
        handleGError(std::move(error));
        if (!valid) {
            throw std::runtime_error{_("Invalid XML data read")};
        }
    }

    // Sanity checks for document validity
    if (!g_markup_parse_context_end_parse(context.get(), xoj::util::out_ptr(error)) || !this->parsingComplete) {
        handleGError(std::move(error));

        // The end may be cut off. Attempt to recover contents by closing the remaining open nodes.
        parserInterface.closeOpenNodes();
        // If a document was created in addDocument(), finalizeDocument() must have been called now.
        xoj_assert((this->doc != nullptr) == this->parsingComplete);

        if (this->parsingComplete) {
            logError(_("Document is not complete.\n"
                       "We have tried recovering the data that could be read, but the last element may have been "
                       "discarded, and the end of the file is likely missing."));
        }
        // Else, then this->doc == nullptr and we will throw just below
    }
    if (!this->doc) {
        throw std::runtime_error{_("Document is corrupted (no document tag found in file)")};
    }
    if (this->doc->getPageCount() == 0) {
        throw std::runtime_error{_("Document is corrupted (no pages found in file)")};
    }
}


auto LoadHandler::loadDocument(fs::path const& filepath) -> std::unique_ptr<Document> {
    auto xmlContentStream = openFile(filepath);
    parseXml(std::move(xmlContentStream));
    closeFile();

    xoj_assert(this->doc);
    this->doc->setCreateBackupOnSave(true);

    if (this->fileVersion == 1 || this->incompleteLoad || (this->errorMessages && !this->errorMessages->empty())) {
        // Either, this file was created by Xournal, not Xournal++, or loading
        // the file failed to some extent (i.e. file is corrupt or uses unknown
        // features). In all cases, do not set the doc's filename, in order to
        // prompt a "Save as" dialog when the user saves the document. In this
        // way, we do not silently make the file will become incompatible with
        // Xournal, nor erase data that we couldn't parse.

        this->doc->setFilepath("");
    } else {
        this->doc->setFilepath(filepath);
    }

    // The document refers to the extracted audio files from now on; they are not ours to delete anymore
    this->audioFilesOwned = false;
    return std::move(this->doc);
}


void LoadHandler::fixNullPressureValues(std::vector<Point> pts) {
    /*
     * Due to various bugs (see e.g. https://github.com/xournalpp/xournalpp/issues/3643), old files may contain strokes
     * with non-positive pressure values.
     *
     * Those strokes thus fail the somewhat reasonable assumption that pressure values should be positive.
     * The portions of the stroke with non-positive pressure values are in any case invisible.
     *
     * This function fixes corrupted strokes by
     *  1- removing every point with a non-positive pressure value.
     *  2- if required, splitting the affected stroke into several strokes.
     *  3- entirely deleting strokes without any valid pressure value
     */

    xoj_assert_message(pts.back().z == Point::NO_PRESSURE, "The last point of a stroke must have no pressure");
    auto nextPositive = std::ranges::find_if(pts, [](double v) { return v > 0; }, &Point::z);

    std::vector<std::vector<Point>> strokePortions;
    while (nextPositive != pts.end()) {
        auto nextNonPositive =
                std::find_if(nextPositive, pts.end(), [](const Point& p) { return p.z <= 0 || std::isnan(p.z); });

        // Since the last point has no pressure, nextNonPositive is always dereferencable
        xoj_assert(nextNonPositive != pts.end());
        std::vector<Point> ps{nextPositive, std::next(nextNonPositive)};
        ps.back().z = Point::NO_PRESSURE;
        strokePortions.emplace_back(std::move(ps));

        nextPositive = std::find_if(nextNonPositive, pts.end(), [](const Point& p) { return p.z > 0; });
    }

    if (strokePortions.empty()) {
        // There were no valid pressure values! Delete the stroke entirely
        g_warning("LoadHandler: Found a stroke with only non-positive pressure values! Removing this invisible stroke");
        this->stroke.reset();
        return;
    }

    g_warning("LoadHandler: Found a stroke with some non-positive pressure values. Removing the affected points");
    for_first_then_each(
            strokePortions, [&](std::vector<Point>& points) { this->stroke->setPointVector(std::move(points)); },
            [&](std::vector<Point>& points) {
                auto newStroke = std::make_unique<Stroke>();
                newStroke->applyStyleFrom(this->stroke.get());
                newStroke->setPointVector(std::move(points));
                this->layer->addElement(std::exchange(this->stroke, std::move(newStroke)));
            });
}

auto LoadHandler::readZipAttachment(fs::path const& filename, AttachmentKind kind) -> std::shared_ptr<std::string> {
    namespace zipx = xoj::util::zip;
    const uint64_t perAttachmentLimit =
            kind == AttachmentKind::Pdf ? this->limits.maxPdfBytes : this->limits.maxImageBytes;
    const std::string entryName = char_cast(filename.u8string().c_str());
    const zip_int64_t located = zip_name_locate(this->zipFp.get(), entryName.c_str(), ZIP_FL_ENC_GUESS);
    if (located < 0) {
        if (this->missingZipEntries.emplace(entryName).second) {
            logError(FS(_F("Could not open attachment: {1}. Error message: {2}") % filename.u8string() %
                        zip_error_strerror(zip_get_error(this->zipFp.get()))));
        }
        return {};
    }
    const auto index = static_cast<zip_uint64_t>(located);
    auto& cache = kind == AttachmentKind::Pdf ? this->pdfAttachments[index] : this->imageAttachments[index];
    if (cache.attempted) {
        return cache.data;
    }
    cache.attempted = true;

    zip_stat_t attachmentFileStat;
    const int statStatus = zip_stat_index(this->zipFp.get(), index, ZIP_FL_UNCHANGED, &attachmentFileStat);
    if (statStatus != 0) {
        cache.status = zipx::ReadStatus::ReadError;
        logError(FS(_F("Could not open attachment: {1}. Error message: {2}") % filename.u8string() %
                    zip_error_strerror(zip_get_error(this->zipFp.get()))));
        return {};
    }

    const auto declared = zipx::declaredSize(attachmentFileStat);
    if (!declared) {
        cache.status = zipx::ReadStatus::SizeUnknown;
        logAttachmentFailure(filename, zipx::ReadStatus::SizeUnknown, perAttachmentLimit);
        return {};
    }

    const bool countAttachment = !this->countedZipEntries.contains(index);
    const auto admission = admitAttachment(*declared, perAttachmentLimit, this->limits.maxMemoryBudgetBytes,
                                           this->memoryBytesUsed, countAttachment);
    if (countAttachment && admission != Admission::CountExceeded) {
        this->countedZipEntries.emplace(index);
    }
    if (admission == Admission::TooLarge) {
        cache.status = zipx::ReadStatus::TooLarge;
        logAttachmentFailure(filename, zipx::ReadStatus::TooLarge, perAttachmentLimit);
        return {};
    }
    if (admission != Admission::Ok) {
        logAttachmentRefused(filename, admission);
        return {};
    }
    // Charge failed reads as well as successful ones so a collection of bad entries cannot evade the work budget.
    this->memoryBytesUsed += *declared;

    const auto attachmentFile = zip_file_wrapper{zip_fopen_index(this->zipFp.get(), index, ZIP_FL_UNCHANGED)};
    if (!attachmentFile) {
        cache.status = zipx::ReadStatus::ReadError;
        logError(FS(_F("Could not open attachment: {1}. Error message: {2}") % filename.u8string() %
                    zip_error_strerror(zip_get_error(this->zipFp.get()))));
        return {};
    }

    // *declared passed the limit above, so it fits in size_t (checked again, defensively, by readZipFileBounded)
    auto data = std::make_shared<std::string>();
    uint64_t bytesRead = 0;
    const auto status = zipx::readZipFileBounded(attachmentFile.get(), *declared, perAttachmentLimit,
                                                 zipx::StringSink{*data, static_cast<size_t>(*declared)}, bytesRead);
    if (status != zipx::ReadStatus::Ok) {
        cache.status = status;
        logAttachmentFailure(filename, status, perAttachmentLimit);
        return {};
    }

    cache.status = zipx::ReadStatus::Ok;
    cache.data = std::move(data);
    return cache.data;
}

void LoadHandler::setAudioAttributes(AudioContent& audio, fs::path filename, size_t timestamp) {
    if (!filename.empty()) {
        if (this->isGzFile) {
            audio.setAudioFilename(std::move(filename));
        } else {
            auto tempFile = getTempFileForPath(filename);
            if (!tempFile.empty()) {
                audio.setAudioFilename(std::move(tempFile));
            }
        }

        if (this->fileVersion < 4) {
            timestamp *= 1000;
        }
        audio.setTimestamp(timestamp);
    }
}

auto LoadHandler::getTempFileForPath(fs::path const& filename) -> fs::path {
    auto it = this->audioFiles.find(filename);

    if (it == this->audioFiles.end()) {
        if (!this->isGzFile) {
            const std::string entryName = char_cast(filename.u8string().c_str());
            const zip_int64_t located = zip_name_locate(this->zipFp.get(), entryName.c_str(), ZIP_FL_ENC_GUESS);
            if (located >= 0) {
                const auto cached = this->audioAttachments.find(static_cast<zip_uint64_t>(located));
                if (cached != this->audioAttachments.end() && cached->second.attempted) {
                    return cached->second.tempPath;
                }
            } else if (this->missingZipEntries.contains(entryName)) {
                return {};
            }
        }
        logError(FS(_F("Requested temporary file was not found for attachment {1}") % filename.u8string()));
        return {};
    } else {
        return it->second;
    }
}

auto LoadHandler::getAbsoluteFilepath(const fs::path& filename, bool attach) const -> fs::path {
    fs::path absolutePath;

    if (attach) {
        // For a file xyz.xopp, the PDF background is saved in the same
        // directory as xyz.xopp.[filename]
        absolutePath = (fs::path{this->xournalFilepath} += ".") += filename;
    } else {
        // domain = "absolute" doesn't forcefully mean an absolute path
        if (filename.is_relative()) {
            // The path is relative to the .xopp file's location
            this->doc->setPathStorageMode(Util::PathStorageMode::AS_RELATIVE_PATH);
            absolutePath = fs::path{this->xournalFilepath}.remove_filename() / filename;
        } else {
            this->doc->setPathStorageMode(Util::PathStorageMode::AS_ABSOLUTE_PATH);
            absolutePath = filename;
        }
    }
    return absolutePath;
}
