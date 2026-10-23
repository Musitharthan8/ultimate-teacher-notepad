/*
 * Xournal++
 *
 * Resource limits and error handling for the attachments of zip based .xopp files (untrusted input).
 * All fixtures are a few kilobytes: nothing here allocates or writes anything large.
 *
 * @license GNU GPLv2 or later
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <config-test.h>
#include <glib.h>
#include <gtest/gtest.h>
#include <zip.h>

#include "control/xojfile/AttachmentLimits.h"
#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "model/Document.h"
#include "model/Image.h"
#include "model/Layer.h"
#include "model/PageRef.h"
#include "model/Stroke.h"
#include "model/TexImage.h"
#include "model/XojPage.h"
#include "util/BoundedZipRead.h"
#include "util/BoundedPixbuf.h"
#include "util/PathUtil.h"
#include "util/StringUtils.h"

#include "filesystem.h"

namespace zipx = xoj::util::zip;
using xoj::load::AttachmentLimits;

namespace {

// ---------------------------------------------------------------------------------------------------------------
// readBounded(): the loop shared by every attachment read
// ---------------------------------------------------------------------------------------------------------------

/// Mock of zip_fread(): delivers `payload`, then `tail` bytes more than announced if asked to, then behaves as `after`
struct MockSource {
    size_t available;            ///< bytes delivered before the source dries up
    zip_int64_t afterwards = 0;  ///< what read() returns once `available` is consumed (0 = EOF, -1 = error)
    size_t calls = 0;
    size_t delivered = 0;

    zip_int64_t operator()(void* buf, zip_uint64_t n) {
        calls++;
        if (delivered >= available) {
            return afterwards;
        }
        const auto k = std::min<size_t>(n, available - delivered);
        std::memset(buf, 'x', k);
        delivered += k;
        return static_cast<zip_int64_t>(k);
    }
};

struct CollectSink {
    size_t total = 0;
    bool accept = true;
    bool operator()(const char*, size_t n) {
        total += n;
        return accept;
    }
};

}  // namespace

TEST(XoppAttachmentRead, ReadsExactlyTheDeclaredBytes) {
    MockSource src{200'000};  // > 3 chunks
    CollectSink sink;
    uint64_t read = 0;
    EXPECT_EQ(zipx::readBounded(src, 200'000, 1'000'000, sink, read), zipx::ReadStatus::Ok);
    EXPECT_EQ(sink.total, 200'000U);
    EXPECT_EQ(read, 200'000U);
}

TEST(XoppAttachmentRead, EmptyAttachmentIsValid) {
    MockSource src{0};
    CollectSink sink;
    uint64_t read = 1;
    EXPECT_EQ(zipx::readBounded(src, 0, 10, sink, read), zipx::ReadStatus::Ok);
    EXPECT_EQ(read, 0U);
}

TEST(XoppAttachmentRead, ZeroReadBeforeDeclaredEndIsTruncatedAndTerminates) {
    // zip_fread() returning 0 early used to leave the loop spinning forever
    MockSource src{1000};
    CollectSink sink;
    uint64_t read = 0;
    EXPECT_EQ(zipx::readBounded(src, 5000, 10'000, sink, read), zipx::ReadStatus::Truncated);
    EXPECT_EQ(read, 1000U);
    EXPECT_LE(src.calls, 3U);
}

TEST(XoppAttachmentRead, DataBeyondDeclaredSizeIsRejected) {
    MockSource src{1500};  // announces 1000, delivers 1500
    CollectSink sink;
    uint64_t read = 0;
    EXPECT_EQ(zipx::readBounded(src, 1000, 10'000, sink, read), zipx::ReadStatus::LargerThanDeclared);
    EXPECT_EQ(read, 1000U);
}

TEST(XoppAttachmentRead, ReadErrorsAreReportedAtAnyPoint) {
    CollectSink sink;
    uint64_t read = 0;
    MockSource early{0, -1};
    EXPECT_EQ(zipx::readBounded(early, 10, 100, sink, read), zipx::ReadStatus::ReadError);

    // libzip reports a CRC mismatch on the read that hits the end of the entry
    MockSource atEnd{10, -1};
    EXPECT_EQ(zipx::readBounded(atEnd, 10, 100, sink, read), zipx::ReadStatus::ReadError);
}

TEST(XoppAttachmentRead, SourceReturningMoreThanAskedIsRejected) {
    auto liar = [](void*, zip_uint64_t n) -> zip_int64_t { return static_cast<zip_int64_t>(n + 1); };
    CollectSink sink;
    uint64_t read = 0;
    EXPECT_EQ(zipx::readBounded(liar, 100, 1000, sink, read), zipx::ReadStatus::ReadError);
}

TEST(XoppAttachmentRead, LimitBoundary) {
    CollectSink sink;
    uint64_t read = 0;
    MockSource atLimit{100};
    EXPECT_EQ(zipx::readBounded(atLimit, 100, 100, sink, read), zipx::ReadStatus::Ok);

    MockSource aboveLimit{101};
    EXPECT_EQ(zipx::readBounded(aboveLimit, 101, 100, sink, read), zipx::ReadStatus::TooLarge);
    EXPECT_EQ(aboveLimit.calls, 0U) << "an oversized entry must be refused before reading anything";
}

TEST(XoppAttachmentRead, DeclaredSizeConversionBoundaries) {
    constexpr zip_uint64_t INT64_MAX_U = static_cast<zip_uint64_t>(std::numeric_limits<zip_int64_t>::max());

    EXPECT_EQ(zipx::checkedSize(0), std::optional<size_t>{0});
    EXPECT_EQ(zipx::checkedSize(1), std::optional<size_t>{1});
    EXPECT_FALSE(zipx::checkedSize(INT64_MAX_U + 1).has_value()) << "does not fit zip_int64_t";
    EXPECT_FALSE(zipx::checkedSize(std::numeric_limits<zip_uint64_t>::max()).has_value());
    if constexpr (sizeof(size_t) >= sizeof(zip_int64_t)) {
        EXPECT_EQ(zipx::checkedSize(INT64_MAX_U), std::optional<size_t>{static_cast<size_t>(INT64_MAX_U)});
    } else {
        EXPECT_FALSE(zipx::checkedSize(INT64_MAX_U).has_value()) << "does not fit a 32 bit size_t";
        EXPECT_FALSE(zipx::checkedSize(std::uint64_t{1} << 32).has_value());
    }

    // Even with no caller limit at all a size libzip cannot represent is refused before any read
    MockSource src{10};
    CollectSink sink;
    uint64_t read = 0;
    EXPECT_EQ(zipx::readBounded(src, INT64_MAX_U + 1, std::numeric_limits<uint64_t>::max(), sink, read),
              zipx::ReadStatus::SizeNotRepresentable);
    EXPECT_EQ(zipx::readBounded(src, std::numeric_limits<zip_uint64_t>::max(), std::numeric_limits<uint64_t>::max(),
                                sink, read),
              zipx::ReadStatus::SizeNotRepresentable);
    EXPECT_EQ(src.calls, 0U);
}

TEST(XoppAttachmentRead, SinkRefusalStopsReading) {
    MockSource src{200'000};
    CollectSink sink;
    sink.accept = false;
    uint64_t read = 0;
    EXPECT_EQ(zipx::readBounded(src, 200'000, 1'000'000, sink, read), zipx::ReadStatus::SinkError);
    EXPECT_EQ(src.calls, 1U);
}

TEST(XoppAttachmentRead, StringSinkDoesNotTrustTheDeclaredSize) {
    // The entry announces 1 TiB (accepted here: the limit is lifted) but only delivers 10 bytes.
    // Nothing close to the announced size may have been allocated.
    MockSource src{10};
    std::string out;
    constexpr uint64_t declared = std::uint64_t{1} << 40;
    uint64_t read = 0;
    const auto status = zipx::readBounded(src, declared, std::numeric_limits<uint64_t>::max(),
                                          zipx::StringSink{out, static_cast<size_t>(declared)}, read);
    if constexpr (sizeof(size_t) >= 8) {
        EXPECT_EQ(status, zipx::ReadStatus::Truncated);
    }
    EXPECT_EQ(out.size(), 10U);
    EXPECT_LE(out.capacity(), 4U * 1024 * 1024);
}

TEST(XoppAttachmentRead, RasterDimensionsAreBoundedWithoutIntegerOverflow) {
    for (const auto [width, height]: {std::pair{16000, 12000},
                                      std::pair{std::numeric_limits<int>::max(),
                                                std::numeric_limits<int>::max()},
                                      std::pair{std::numeric_limits<int>::max(), 1}}) {
        const auto [boundedWidth, boundedHeight] = xoj::util::boundedPixbufDimensions(width, height);
        EXPECT_GT(boundedWidth, 0);
        EXPECT_GT(boundedHeight, 0);
        EXPECT_LE(static_cast<uint64_t>(boundedWidth) * static_cast<uint64_t>(boundedHeight),
                  xoj::util::MAX_DECODED_PIXBUF_PIXELS);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// LoadHandler with synthetic archives
// ---------------------------------------------------------------------------------------------------------------

namespace {

// 1x1 transparent PNG
const std::vector<unsigned char> TINY_PNG = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4, 0x89, 0x00,
        0x00, 0x00, 0x0A, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00, 0x01,
        0x0D, 0x0A, 0x2D, 0xB4, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

std::string pngHeader(uint32_t width, uint32_t height) {
    std::string data("\x89PNG\r\n\x1a\n", 8);
    data.append("\0\0\0\rIHDR", 8);
    auto appendBigEndian = [&](uint32_t value) {
        data.push_back(static_cast<char>(value >> 24));
        data.push_back(static_cast<char>(value >> 16));
        data.push_back(static_cast<char>(value >> 8));
        data.push_back(static_cast<char>(value));
    };
    appendBigEndian(width);
    appendBigEndian(height);
    data.append("\x08\x06\0\0\0", 5);
    return data;
}

TEST(XoppAttachmentRead, TexPngDimensionsAreRejectedBeforeCairoAllocation) {
    TexImage tooLarge;
    EXPECT_FALSE(tooLarge.loadData(pngHeader(8192, 8192)));

    TexImage valid;
    std::string onePixel(TINY_PNG.begin(), TINY_PNG.end());
    EXPECT_TRUE(valid.loadData(std::move(onePixel)));
}

/// A valid PNG padded with zeros to `size` bytes. Decoders ignore what follows IEND.
std::string paddedPng(size_t size) {
    std::string s(TINY_PNG.begin(), TINY_PNG.end());
    if (s.size() < size) {
        s.resize(size, '\0');
    }
    return s;
}

/// Compressible but not constant, so that deflate has something to do
std::string patternData(size_t size) {
    std::string s(size, '\0');
    for (size_t i = 0; i < size; i++) {
        s[i] = static_cast<char>('a' + (i * 7 + i / 13) % 23);
    }
    return s;
}

struct ZipEntry {
    std::string name;
    std::string data;
    bool deflate = true;
};

std::string readWholeFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
}

void writeWholeFile(const fs::path& p, const std::string& data) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
}

void buildZip(const fs::path& out, const std::vector<ZipEntry>& entries) {
    fs::remove(out);
    int err = 0;
    zip_t* z = zip_open(char_cast(out.u8string().c_str()), ZIP_CREATE | ZIP_TRUNCATE, &err);
    ASSERT_NE(z, nullptr);
    for (const auto& e: entries) {
        zip_source_t* src = zip_source_buffer(z, e.data.data(), e.data.size(), 0);  // e outlives zip_close()
        ASSERT_NE(src, nullptr);
        const auto idx = zip_file_add(z, e.name.c_str(), src, ZIP_FL_ENC_UTF_8);
        ASSERT_GE(idx, 0);
        ASSERT_EQ(zip_set_file_compression(z, static_cast<zip_uint64_t>(idx), e.deflate ? ZIP_CM_DEFLATE : ZIP_CM_STORE,
                                           0),
                  0);
    }
    ASSERT_EQ(zip_close(z), 0) << "could not write the test archive";
}

std::string wrapXml(const std::string& body) {
    return "<?xml version=\"1.0\" standalone=\"no\"?>\n<xournal creator=\"test\">\n<title>t</title>\n" + body +
           "</xournal>\n";
}

std::string pageWithAttachedBg(const std::string& attachment) {
    return "<page width=\"200\" height=\"200\"><background type=\"pixmap\" domain=\"attach\" filename=\"" + attachment +
           "\"/><layer/></page>\n";
}

std::string pageWithImage(const std::string& attachment) {
    return "<page width=\"200\" height=\"200\"><background type=\"solid\" color=\"#ffffffff\" style=\"plain\"/>"
           "<layer><image left=\"10\" top=\"10\" right=\"50\" bottom=\"50\"><attachment path=\"" +
           attachment + "\"/></image></layer></page>\n";
}

std::string encodeBase64(std::string_view data) {
    gchar* encoded = g_base64_encode(reinterpret_cast<const guchar*>(data.data()), data.size());
    std::string result(encoded);
    g_free(encoded);
    return result;
}

std::string pageWithInlineImage(const std::string& encoded) {
    return "<page width=\"200\" height=\"200\"><background type=\"solid\" color=\"#ffffffff\" style=\"plain\"/>"
           "<layer><image left=\"10\" top=\"10\" right=\"50\" bottom=\"50\">" + encoded +
           "</image></layer></page>\n";
}

/// Archive layout matching what LoadHandler requires, plus the given entries
fs::path makeXopp(const std::string& name, const std::string& contentXml, std::vector<ZipEntry> attachments,
                  bool* ok = nullptr) {
    const fs::path p = Util::getTmpDirSubfolder("attachment-limits") / (name + ".xopp");
    std::vector<ZipEntry> entries;
    entries.push_back({"mimetype", " application/xournal++\n", false});
    entries.push_back({"META-INF/version", "current=4\nmin=4\n", false});
    entries.push_back({"content.xml", contentXml, true});
    for (auto& a: attachments) {
        entries.push_back(std::move(a));
    }
    buildZip(p, entries);
    if (ok) {
        *ok = true;
    }
    return p;
}

/// Change the uncompressed size recorded for `entryName` in both the central directory and the local header.
/// Used to build archives whose announced size does not match what they contain.
bool patchDeclaredSize(const fs::path& zipPath, const std::string& entryName, uint32_t newSize) {
    std::string bytes = readWholeFile(zipPath);
    auto put32 = [&](size_t off, uint32_t v) {
        for (int i = 0; i < 4; i++) {
            bytes[off + static_cast<size_t>(i)] = static_cast<char>((v >> (8 * i)) & 0xFF);
        }
    };
    auto get16 = [&](size_t off) {
        return static_cast<size_t>(static_cast<unsigned char>(bytes[off])) |
               (static_cast<size_t>(static_cast<unsigned char>(bytes[off + 1])) << 8);
    };
    bool central = false;
    for (size_t i = 0; i + 46 < bytes.size(); i++) {
        if (bytes.compare(i, 4, "PK\x01\x02", 4) == 0 && get16(i + 28) == entryName.size() &&
            bytes.compare(i + 46, entryName.size(), entryName) == 0) {
            put32(i + 24, newSize);
            central = true;
        } else if (bytes.compare(i, 4, "PK\x03\x04", 4) == 0 && get16(i + 26) == entryName.size() &&
                   bytes.compare(i + 30, entryName.size(), entryName) == 0) {
            put32(i + 22, newSize);
        }
    }
    writeWholeFile(zipPath, bytes);
    return central;
}

bool corruptStoredEntry(const fs::path& zipPath, const std::string& entryName) {
    std::string bytes = readWholeFile(zipPath);
    auto get16 = [&](size_t off) {
        return static_cast<size_t>(static_cast<unsigned char>(bytes[off])) |
               (static_cast<size_t>(static_cast<unsigned char>(bytes[off + 1])) << 8);
    };
    for (size_t i = 0; i + 30 < bytes.size(); i++) {
        if (bytes.compare(i, 4, "PK\x03\x04", 4) != 0 || get16(i + 26) != entryName.size() ||
            bytes.compare(i + 30, entryName.size(), entryName) != 0) {
            continue;
        }
        const size_t data = i + 30 + entryName.size() + get16(i + 28);
        if (data >= bytes.size()) {
            return false;
        }
        bytes[data + std::min<size_t>(10, bytes.size() - data - 1)] ^= 0x55;
        writeWholeFile(zipPath, bytes);
        return true;
    }
    return false;
}

struct LoadResult {
    std::unique_ptr<Document> doc;
    std::vector<std::string> errors;
    bool threw = false;
    std::string what;
};

LoadResult load(const fs::path& p, const AttachmentLimits& limits = AttachmentLimits{}) {
    LoadResult r;
    try {
        LoadHandler h(&r.errors);
        h.setAttachmentLimits(limits);
        r.doc = h.loadDocument(p);
    } catch (const std::exception& e) {
        r.threw = true;
        r.what = e.what();
    }
    return r;
}

size_t countErrorsContaining(const LoadResult& r, const std::string& needle) {
    return static_cast<size_t>(std::count_if(r.errors.begin(), r.errors.end(), [&](const std::string& e) {
        return e.find(needle) != std::string::npos;
    }));
}

/// Temporary audio files with exactly this size (sizes used in these tests are unique, so this is robust against
/// other tests running at the same time)
size_t countAudioTempFilesOfSize(uintmax_t size) {
    size_t n = 0;
    std::error_code ec;
    for (const auto& entry: fs::directory_iterator(g_get_tmp_dir(), ec)) {
        const auto name = entry.path().filename().string();
        if (name.rfind("xournal_audio_", 0) == 0 && name.ends_with(".tmp") && entry.is_regular_file(ec) &&
            entry.file_size(ec) == size) {
            n++;
        }
    }
    return n;
}

void removeAudioTempFilesOfSize(uintmax_t size) {
    std::error_code ec;
    for (const auto& entry: fs::directory_iterator(g_get_tmp_dir(), ec)) {
        const auto name = entry.path().filename().string();
        if (name.rfind("xournal_audio_", 0) == 0 && name.ends_with(".tmp") && entry.is_regular_file(ec) &&
            entry.file_size(ec) == size) {
            fs::remove(entry.path(), ec);
        }
    }
}

AttachmentLimits smallLimits() {
    AttachmentLimits l;
    l.maxImageBytes = 4096;
    l.maxPdfBytes = 8192;
    l.maxAudioBytes = 16384;
    l.maxMemoryBudgetBytes = 64 * 1024;
    l.maxDiskBudgetBytes = 64 * 1024;
    l.maxAttachmentCount = 100;
    l.maxXmlBytes = 1 << 20;
    return l;
}

const Image* firstImage(const Document& doc) {
    ConstPageRef page = doc.getPage(0);
    if (!page || page->getLayerCount() == 0) {
        return nullptr;
    }
    const auto elements = page->getLayersView()[0]->getElementsView();
    return elements.size() == 0 ? nullptr : dynamic_cast<const Image*>(elements[0]);
}

}  // namespace

TEST(XoppAttachmentLoad, DefaultLimitsAreSane) {
    const AttachmentLimits l;
    // Per attachment limits must fit in the budgets, or a single legitimate file could never load
    EXPECT_LE(l.maxImageBytes, l.maxMemoryBudgetBytes);
    EXPECT_LE(l.maxPdfBytes, l.maxMemoryBudgetBytes);
    EXPECT_LE(l.maxAudioBytes, l.maxDiskBudgetBytes);
    EXPECT_GE(l.maxPdfBytes, 200 * xoj::load::MiB) << "scanned textbooks must keep loading";
    EXPECT_GE(l.maxImageBytes, 32 * xoj::load::MiB) << "high resolution photos must keep loading";
    EXPECT_GE(l.maxAttachmentCount, 1000U);
}

TEST(XoppAttachmentLoad, OrdinaryPackagedDocumentsLoadWithoutMessages) {
    for (const char8_t* file:
         {u8"packaged_xopp/imgAttachment/new.xopp", u8"packaged_xopp/imgAttachment/old.xopp",
          u8"packaged_xopp/imgBackground/new.xopp", u8"packaged_xopp/pdfBackground/new.xopp",
          u8"packaged_xopp/audioAttachment/new.xopp", u8"packaged_xopp/audioAttachment/old.xopp"}) {
        // PDF backgrounds of the old format live next to the document, not inside it: skip the expectation on those
        const auto path = fs::path(GET_TESTFILE(file));
        auto r = load(path);
        ASSERT_FALSE(r.threw) << path << ": " << r.what;
        ASSERT_TRUE(r.doc) << path;
        // Only the bg.pdf of the new format is contained in the archive. Messages about a missing sibling file are
        // not attachment errors.
        EXPECT_EQ(countErrorsContaining(r, "skipped"), 0U) << path;
        EXPECT_EQ(countErrorsContaining(r, "damaged"), 0U) << path;
        EXPECT_EQ(countErrorsContaining(r, "allowed"), 0U) << path;
    }
}

TEST(XoppAttachmentLoad, EmbeddedImageSurvivesSaveAndReopen) {
    auto first = load(GET_TESTFILE(u8"packaged_xopp/imgAttachment/new.xopp"));
    ASSERT_TRUE(first.doc);
    ASSERT_TRUE(first.errors.empty());
    const Image* img = firstImage(*first.doc);
    ASSERT_NE(img, nullptr);
    const std::string original(reinterpret_cast<const char*>(img->getRawData()), img->getRawDataLength());
    ASSERT_FALSE(original.empty());

    const fs::path saved = Util::getTmpDirSubfolder("attachment-limits") / "roundtrip.xopp";
    fs::remove(saved);
    {
        SaveHandler saver;
        saver.prepareSave(first.doc.get(), saved);
        saver.saveTo(saved);
    }

    auto second = load(saved);
    ASSERT_TRUE(second.doc);
    EXPECT_TRUE(second.errors.empty());
    const Image* img2 = firstImage(*second.doc);
    ASSERT_NE(img2, nullptr);
    EXPECT_GT(img2->getRawDataLength(), 0U);
    EXPECT_EQ(second.doc->getPageCount(), first.doc->getPageCount());
    fs::remove(saved);
}

TEST(XoppAttachmentLoad, AttachedImageAndBackgroundWithinLimitsLoad) {
    const auto p =
            makeXopp("valid", wrapXml(pageWithImage("attachments/a.png") + pageWithAttachedBg("attachments/b.png")),
                     {{"attachments/a.png", paddedPng(1000)}, {"attachments/b.png", paddedPng(2000)}});
    auto r = load(p, smallLimits());
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    EXPECT_TRUE(r.errors.empty()) << (r.errors.empty() ? "" : r.errors[0]);
    const Image* img = firstImage(*r.doc);
    ASSERT_NE(img, nullptr);
    EXPECT_EQ(img->getRawDataLength(), 1000U);
    EXPECT_FALSE(r.doc->getPage(1)->getBackgroundImage().isEmpty());
}

TEST(XoppAttachmentLoad, AttachedPdfWithinLimitsLoads) {
    const auto pdf = readWholeFile(fs::path(GET_TESTFILE(u8"packaged_xopp/pdfBackground/old.xopp.bg.pdf")));
    ASSERT_FALSE(pdf.empty());
    AttachmentLimits l = smallLimits();
    l.maxPdfBytes = pdf.size();  // exactly at the limit
    const auto p =
            makeXopp("pdf",
                     wrapXml("<page width=\"612\" height=\"792\"><background type=\"pdf\" "
                             "domain=\"attach\" filename=\"attachments/bg.pdf\" pageno=\"1\"/><layer/></page>\n"),
                     {{"attachments/bg.pdf", pdf}});
    auto r = load(p, l);
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    EXPECT_EQ(countErrorsContaining(r, "skipped"), 0U);
    EXPECT_TRUE(r.doc->isAttachPdf());

    // One byte under the limit of the same file is refused, and the document still opens
    l.maxPdfBytes = pdf.size() - 1;
    auto refused = load(p, l);
    ASSERT_FALSE(refused.threw) << refused.what;
    ASSERT_TRUE(refused.doc);
    EXPECT_EQ(countErrorsContaining(refused, "larger than"), 1U);
}

TEST(XoppAttachmentLoad, EmptyAttachmentsDoNotCrashOrHang) {
    const auto p =
            makeXopp("empty", wrapXml(pageWithImage("attachments/e.png") + pageWithAttachedBg("attachments/f.png")),
                     {{"attachments/e.png", ""}, {"attachments/f.png", ""}});
    auto r = load(p, smallLimits());
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    EXPECT_EQ(firstImage(*r.doc), nullptr) << "an image without data is dropped";
    EXPECT_GE(countErrorsContaining(r, "damaged"), 1U) << "and the teacher is told";
}

TEST(XoppAttachmentLoad, OversizedAttachmentIsRefusedAndDocumentStillOpens) {
    const auto p = makeXopp("oversized",
                            wrapXml(pageWithImage("attachments/big.png") + pageWithAttachedBg("attachments/ok.png")),
                            {{"attachments/big.png", paddedPng(4097)}, {"attachments/ok.png", paddedPng(4096)}});
    auto r = load(p, smallLimits());
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    EXPECT_EQ(countErrorsContaining(r, "attachments/big.png"), 1U);
    EXPECT_EQ(countErrorsContaining(r, "attachments/ok.png"), 0U);
    EXPECT_EQ(firstImage(*r.doc), nullptr);
    EXPECT_FALSE(r.doc->getPage(1)->getBackgroundImage().isEmpty());
    EXPECT_TRUE(r.doc->getFilepath().empty()) << "after an error the original file must not be overwritten silently";
}

TEST(XoppAttachmentLoad, EntryDeclaringHugeSizeIsRefusedWithoutReading) {
    // The announced size (4 GiB - 1) is far beyond every default limit. The real data is 100 bytes.
    const auto p = makeXopp("lying-huge", wrapXml(pageWithImage("attachments/a.png")),
                            {{"attachments/a.png", patternData(100)}});
    ASSERT_TRUE(patchDeclaredSize(p, "attachments/a.png", 0xFFFFFFFFu));
    auto r = load(p);  // production limits
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    EXPECT_EQ(countErrorsContaining(r, "larger than"), 1U);
    EXPECT_EQ(firstImage(*r.doc), nullptr);
}

TEST(XoppAttachmentLoad, DeclaredSizeLargerThanContentIsTruncated) {
    // Within the limit, but the archive does not deliver what it announces
    const auto p = makeXopp("lying-big", wrapXml(pageWithImage("attachments/a.png")),
                            {{"attachments/a.png", patternData(1000)}});
    ASSERT_TRUE(patchDeclaredSize(p, "attachments/a.png", 3000));
    auto r = load(p, smallLimits());
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    EXPECT_EQ(countErrorsContaining(r, "damaged"), 1U) << "the attachment is reported, not silently half-loaded";
    EXPECT_EQ(firstImage(*r.doc), nullptr);
}

TEST(XoppAttachmentLoad, DeclaredSizeSmallerThanContentIsRejected) {
    const auto p = makeXopp("lying-small", wrapXml(pageWithImage("attachments/a.png")),
                            {{"attachments/a.png", patternData(3000)}});
    ASSERT_TRUE(patchDeclaredSize(p, "attachments/a.png", 1000));
    auto r = load(p, smallLimits());
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    EXPECT_EQ(countErrorsContaining(r, "damaged"), 1U);
    EXPECT_EQ(firstImage(*r.doc), nullptr);
}

TEST(XoppAttachmentLoad, CumulativeBudgetStopsManyIndividuallyAcceptableAttachments) {
    AttachmentLimits l = smallLimits();
    l.maxImageBytes = 4096;
    l.maxMemoryBudgetBytes = 10'000;  // two 4000 byte images fit, a third does not
    std::string xml;
    std::vector<ZipEntry> entries;
    for (int i = 0; i < 5; i++) {
        const std::string name = "attachments/p" + std::to_string(i) + ".png";
        xml += pageWithAttachedBg(name);
        entries.push_back({name, paddedPng(4000)});
    }
    const auto p = makeXopp("budget", wrapXml(xml), std::move(entries));
    auto r = load(p, l);
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    ASSERT_EQ(r.doc->getPageCount(), 5U);
    EXPECT_FALSE(r.doc->getPage(0)->getBackgroundImage().isEmpty());
    EXPECT_FALSE(r.doc->getPage(1)->getBackgroundImage().isEmpty());
    EXPECT_TRUE(r.doc->getPage(2)->getBackgroundImage().isEmpty());
    EXPECT_TRUE(r.doc->getPage(4)->getBackgroundImage().isEmpty());
    EXPECT_EQ(countErrorsContaining(r, "larger than the allowed total"), 1U) << "reported once, not per attachment";
}

TEST(XoppAttachmentLoad, RepeatedReferencesToOneAttachmentShareTheReadBudget) {
    AttachmentLimits l = smallLimits();
    l.maxMemoryBudgetBytes = 4000;
    std::string xml;
    for (int i = 0; i < 20; i++) {
        xml += pageWithImage("attachments/same.png");
    }
    const auto p = makeXopp("repeat", wrapXml(xml), {{"attachments/same.png", paddedPng(4000)}});
    auto r = load(p, l);
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    size_t loaded = 0;
    for (size_t i = 0; i < r.doc->getPageCount(); i++) {
        const auto elements = r.doc->getPage(i)->getLayersView()[0]->getElementsView();
        loaded += elements.size() != 0 && dynamic_cast<const Image*>(elements[0]) ? 1 : 0;
    }
    EXPECT_EQ(loaded, 20U);
    EXPECT_TRUE(r.errors.empty());
}

TEST(XoppAttachmentLoad, RepeatedFailedImageReferenceDoesNotBlockDistinctValidImage) {
    AttachmentLimits l = smallLimits();
    l.maxAttachmentCount = 2;
    std::string xml;
    for (int i = 0; i < 20; i++) {
        xml += pageWithImage("attachments/broken.png");
    }
    xml += pageWithImage("attachments/good.png");
    const auto p = makeXopp("repeat-broken-image", wrapXml(xml),
                            {{"attachments/broken.png", paddedPng(1000), false},
                             {"attachments/good.png", paddedPng(1000)}});
    ASSERT_TRUE(corruptStoredEntry(p, "attachments/broken.png"));

    auto r = load(p, l);
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    const auto elements = r.doc->getPage(20)->getLayersView()[0]->getElementsView();
    ASSERT_EQ(elements.size(), 1U);
    ASSERT_NE(dynamic_cast<const Image*>(elements[0]), nullptr);
    EXPECT_EQ(countErrorsContaining(r, "damaged"), 1U);
    EXPECT_EQ(countErrorsContaining(r, "more than 2 attachments"), 0U);
}

TEST(XoppAttachmentLoad, DistinctFailedReadsConsumeTheDocumentBudget) {
    AttachmentLimits l = smallLimits();
    l.maxImageBytes = 4096;
    l.maxMemoryBudgetBytes = 5000;
    const auto p = makeXopp("distinct-broken-images",
                            wrapXml(pageWithImage("attachments/broken-a.png") +
                                    pageWithImage("attachments/broken-b.png") +
                                    pageWithImage("attachments/good.png")),
                            {{"attachments/broken-a.png", paddedPng(3000), false},
                             {"attachments/broken-b.png", paddedPng(3000), false},
                             {"attachments/good.png", paddedPng(1000)}});
    ASSERT_TRUE(corruptStoredEntry(p, "attachments/broken-a.png"));
    ASSERT_TRUE(corruptStoredEntry(p, "attachments/broken-b.png"));

    auto r = load(p, l);
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    EXPECT_EQ(firstImage(*r.doc), nullptr);
    const auto lastElements = r.doc->getPage(2)->getLayersView()[0]->getElementsView();
    ASSERT_EQ(lastElements.size(), 1U);
    EXPECT_NE(dynamic_cast<const Image*>(lastElements[0]), nullptr);
    EXPECT_EQ(countErrorsContaining(r, "damaged"), 1U) << "the second corrupt entry is refused before another read";
    EXPECT_EQ(countErrorsContaining(r, "larger than the allowed total"), 1U);
}

TEST(XoppAttachmentLoad, RepeatedFailedAudioReferenceIsCachedByZipEntry) {
    AttachmentLimits l = smallLimits();
    l.maxAttachmentCount = 2;
    std::string xml;
    for (int i = 0; i < 20; i++) {
        xml += "<audio fn=\"attachments/broken.ogg\"/>\n";
    }
    xml += "<audio fn=\"attachments/good.ogg\"/>\n";
    xml += "<page width=\"200\" height=\"200\"><background type=\"solid\" color=\"#ffffffff\" style=\"plain\"/>"
           "<layer><stroke tool=\"pen\" ts=\"0\" fn=\"attachments/good.ogg\" color=\"#000000ff\" width=\"1\">"
           "1 1 5 5</stroke></layer></page>\n";
    const auto p = makeXopp("repeat-broken-audio", wrapXml(xml),
                            {{"attachments/broken.ogg", patternData(1000), false},
                             {"attachments/good.ogg", patternData(1100)}});
    ASSERT_TRUE(corruptStoredEntry(p, "attachments/broken.ogg"));

    auto r = load(p, l);
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    const auto* stroke = dynamic_cast<const Stroke*>(r.doc->getPage(0)->getLayersView()[0]->getElementsView()[0]);
    ASSERT_NE(stroke, nullptr);
    const fs::path extracted = stroke->getAudioFilename();
    ASSERT_FALSE(extracted.empty());
    EXPECT_EQ(fs::file_size(extracted), 1100U);
    EXPECT_EQ(countErrorsContaining(r, "damaged"), 1U);
    EXPECT_EQ(countErrorsContaining(r, "more than 2 attachments"), 0U);
    std::error_code ec;
    fs::remove(extracted, ec);
}

TEST(XoppAttachmentLoad, CorruptAttachmentWithNoErrorCollectorCannotSaveOverSource) {
    const auto p = makeXopp("no-errors-collector", wrapXml(pageWithImage("attachments/broken.png")),
                            {{"attachments/broken.png", paddedPng(1000), false}});
    ASSERT_TRUE(corruptStoredEntry(p, "attachments/broken.png"));
    LoadHandler loader;
    auto doc = loader.loadDocument(p);
    ASSERT_TRUE(doc);
    EXPECT_TRUE(doc->getFilepath().empty()) << "partial loads must use Save As even without a diagnostic collector";
}

TEST(XoppAttachmentLoad, OversizedInlineImageIsRefusedBeforeDecode) {
    AttachmentLimits l = smallLimits();
    l.maxImageBytes = 512;
    l.maxMemoryBudgetBytes = 4096;
    const auto p = makeXopp("inline-image-large", wrapXml(pageWithInlineImage(encodeBase64(paddedPng(1000)))), {});
    auto r = load(p, l);
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    EXPECT_EQ(firstImage(*r.doc), nullptr);
    EXPECT_EQ(countErrorsContaining(r, "larger than"), 1U);
}

TEST(XoppAttachmentLoad, RepeatedInlineImagesConsumeCumulativeBudget) {
    AttachmentLimits l = smallLimits();
    l.maxImageBytes = 4096;
    l.maxMemoryBudgetBytes = 2000;
    const auto encoded = encodeBase64(paddedPng(1000));
    std::string xml;
    for (int i = 0; i < 5; i++) {
        xml += pageWithInlineImage(encoded);
    }
    auto r = load(makeXopp("inline-image-budget", wrapXml(xml), {}), l);
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    size_t loaded = 0;
    for (size_t i = 0; i < r.doc->getPageCount(); i++) {
        const auto elements = r.doc->getPage(i)->getLayersView()[0]->getElementsView();
        loaded += elements.size() != 0 && dynamic_cast<const Image*>(elements[0]) ? 1 : 0;
    }
    EXPECT_EQ(loaded, 2U);
    EXPECT_EQ(countErrorsContaining(r, "larger than the allowed total"), 1U);
}

TEST(XoppAttachmentLoad, MalformedInlineBase64IsRejectedAndForcesSaveAs) {
    auto r = load(makeXopp("inline-image-invalid", wrapXml(pageWithInlineImage("!!!!")), {}));
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    EXPECT_EQ(firstImage(*r.doc), nullptr);
    EXPECT_EQ(countErrorsContaining(r, "invalid base64"), 1U);
    EXPECT_TRUE(r.doc->getFilepath().empty());
}

TEST(XoppAttachmentLoad, AttachmentCountIsLimited) {
    AttachmentLimits l = smallLimits();
    l.maxAttachmentCount = 3;
    std::string xml;
    std::vector<ZipEntry> entries;
    for (int i = 0; i < 6; i++) {
        const std::string name = "attachments/p" + std::to_string(i) + ".png";
        xml += pageWithAttachedBg(name);
        entries.push_back({name, paddedPng(100)});
    }
    const auto p = makeXopp("count", wrapXml(xml), std::move(entries));
    auto r = load(p, l);
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    EXPECT_FALSE(r.doc->getPage(2)->getBackgroundImage().isEmpty());
    EXPECT_TRUE(r.doc->getPage(3)->getBackgroundImage().isEmpty());
    EXPECT_EQ(countErrorsContaining(r, "more than 3 attachments"), 1U);
}

TEST(XoppAttachmentLoad, AttachmentAtTheCountLimitStillLoads) {
    AttachmentLimits l = smallLimits();
    l.maxAttachmentCount = 2;
    const auto p = makeXopp("count-exact",
                            wrapXml(pageWithAttachedBg("attachments/a.png") + pageWithAttachedBg("attachments/b.png")),
                            {{"attachments/a.png", paddedPng(100)}, {"attachments/b.png", paddedPng(100)}});
    auto r = load(p, l);
    ASSERT_TRUE(r.doc);
    EXPECT_TRUE(r.errors.empty());
}

TEST(XoppAttachmentLoad, OversizedXmlIsRefused) {
    AttachmentLimits l = smallLimits();
    l.maxXmlBytes = 512;
    std::string xml;
    for (int i = 0; i < 20; i++) {
        xml += "<page width=\"200\" height=\"200\"><background type=\"solid\" color=\"#ffffffff\" "
               "style=\"plain\"/><layer/></page>\n";
    }
    const auto p = makeXopp("xml", wrapXml(xml), {});
    auto r = load(p, l);
    EXPECT_TRUE(r.threw);
    EXPECT_NE(r.what.find("larger than"), std::string::npos) << r.what;
}

// --- audio: temporary files -------------------------------------------------------------------------------------

namespace {
std::string audioXml(const std::string& attachment, bool withPage = true) {
    std::string xml = "<audio fn=\"" + attachment + "\"/>\n";
    if (withPage) {
        xml += "<page width=\"200\" height=\"200\"><background type=\"solid\" color=\"#ffffffff\" style=\"plain\"/>"
               "<layer><stroke tool=\"pen\" ts=\"0\" fn=\"" +
               attachment + "\" color=\"#000000ff\" width=\"1\">1 1 5 5</stroke></layer></page>\n";
    }
    return xml;
}
}  // namespace

TEST(XoppAttachmentLoad, AudioAttachmentIsExtractedAndOwnedByTheDocument) {
    constexpr size_t SIZE = 6101;  // unique among the sizes used here
    const auto p =
            makeXopp("audio-ok", wrapXml(audioXml("attachments/a.ogg")), {{"attachments/a.ogg", patternData(SIZE)}});
    auto r = load(p, smallLimits());
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    EXPECT_TRUE(r.errors.empty());
    const auto* stroke = dynamic_cast<const Stroke*>(r.doc->getPage(0)->getLayersView()[0]->getElementsView()[0]);
    ASSERT_NE(stroke, nullptr);
    const fs::path tmp = stroke->getAudioFilename();
    ASSERT_FALSE(tmp.empty());
    EXPECT_EQ(fs::file_size(tmp), SIZE);
    EXPECT_EQ(countAudioTempFilesOfSize(SIZE), 1U) << "the document keeps the file after the loader is gone";
    removeAudioTempFilesOfSize(SIZE);
}

TEST(XoppAttachmentLoad, EmptyAudioAttachmentIsHarmless) {
    const auto p = makeXopp("audio-empty", wrapXml(audioXml("attachments/a.ogg")), {{"attachments/a.ogg", ""}});
    auto r = load(p, smallLimits());
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    EXPECT_TRUE(r.errors.empty());
    // empty files cannot be told from other tests' by size: remove ours through the stroke
    const auto* stroke = dynamic_cast<const Stroke*>(r.doc->getPage(0)->getLayersView()[0]->getElementsView()[0]);
    ASSERT_NE(stroke, nullptr);
    std::error_code ec;
    fs::remove(stroke->getAudioFilename(), ec);
}

TEST(XoppAttachmentLoad, OversizedAudioIsRefusedWithoutCreatingAFile) {
    constexpr size_t SIZE = 16385;  // one above the test limit
    const auto p =
            makeXopp("audio-big", wrapXml(audioXml("attachments/a.ogg")), {{"attachments/a.ogg", patternData(SIZE)}});
    auto r = load(p, smallLimits());
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    EXPECT_GE(countErrorsContaining(r, "larger than"), 1U);
    EXPECT_EQ(countAudioTempFilesOfSize(SIZE), 0U);
}

TEST(XoppAttachmentLoad, TruncatedAudioLeavesNoTemporaryFile) {
    constexpr size_t SIZE = 5003;
    const auto p =
            makeXopp("audio-trunc", wrapXml(audioXml("attachments/a.ogg")), {{"attachments/a.ogg", patternData(SIZE)}});
    ASSERT_TRUE(patchDeclaredSize(p, "attachments/a.ogg", 9000));
    auto r = load(p, smallLimits());
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    EXPECT_GE(countErrorsContaining(r, "damaged"), 1U);
    EXPECT_EQ(countAudioTempFilesOfSize(SIZE), 0U) << "a partially written recording must be deleted";
    EXPECT_EQ(countAudioTempFilesOfSize(9000), 0U);
}

TEST(XoppAttachmentLoad, CorruptAudioFailingItsChecksumLeavesNoTemporaryFile) {
    constexpr size_t SIZE = 4801;
    std::string data = patternData(SIZE);
    const auto p = makeXopp("audio-crc", wrapXml(audioXml("attachments/a.ogg")),
                            {{"attachments/a.ogg", data, /*deflate=*/false}});
    // Flip one byte of the stored (uncompressed) data inside the archive
    std::string bytes = readWholeFile(p);
    const auto pos = bytes.find(data.substr(100, 64));
    ASSERT_NE(pos, std::string::npos);
    bytes[pos + 10] = static_cast<char>(bytes[pos + 10] ^ 0x55);
    writeWholeFile(p, bytes);

    auto r = load(p, smallLimits());
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    EXPECT_GE(countErrorsContaining(r, "damaged"), 1U);
    EXPECT_EQ(countAudioTempFilesOfSize(SIZE), 0U);
}

TEST(XoppAttachmentLoad, ExtractedAudioIsRemovedWhenTheDocumentFailsToLoad) {
    constexpr size_t SIZE = 4409;
    // The audio is valid and gets extracted, but the document has no page and cannot be loaded
    const auto p = makeXopp("audio-fatal", wrapXml(audioXml("attachments/a.ogg", /*withPage=*/false)),
                            {{"attachments/a.ogg", patternData(SIZE)}});
    auto r = load(p, smallLimits());
    EXPECT_TRUE(r.threw);
    EXPECT_EQ(countAudioTempFilesOfSize(SIZE), 0U) << "nobody owns the extracted file: the loader must delete it";
}

TEST(XoppAttachmentLoad, AudioDiskBudgetAppliesAcrossAttachments) {
    AttachmentLimits l = smallLimits();
    l.maxAudioBytes = 16384;
    l.maxDiskBudgetBytes = 2 * 4201 + 10;  // two fit, the third does not
    std::string xml;
    std::vector<ZipEntry> entries;
    for (int i = 0; i < 3; i++) {
        const std::string name = "attachments/a" + std::to_string(i) + ".ogg";
        xml += "<audio fn=\"" + name + "\"/>\n";
        entries.push_back({name, patternData(4201)});
    }
    xml += "<page width=\"200\" height=\"200\"><background type=\"solid\" color=\"#ffffffff\" "
           "style=\"plain\"/><layer/></page>\n";
    const auto p = makeXopp("audio-budget", wrapXml(xml), std::move(entries));
    auto r = load(p, l);
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    EXPECT_EQ(countAudioTempFilesOfSize(4201), 2U);
    EXPECT_EQ(countErrorsContaining(r, "larger than the allowed total"), 1U);
    removeAudioTempFilesOfSize(4201);
}

TEST(XoppAttachmentLoad, RepeatedAudioReferenceExtractsOnce) {
    constexpr size_t SIZE = 3607;
    std::string xml;
    for (int i = 0; i < 5; i++) {
        xml += "<audio fn=\"attachments/a.ogg\"/>\n";
    }
    xml += "<page width=\"200\" height=\"200\"><background type=\"solid\" color=\"#ffffffff\" "
           "style=\"plain\"/><layer/></page>\n";
    const auto p = makeXopp("audio-repeat", wrapXml(xml), {{"attachments/a.ogg", patternData(SIZE)}});
    auto r = load(p, smallLimits());
    ASSERT_TRUE(r.doc);
    EXPECT_EQ(countAudioTempFilesOfSize(SIZE), 1U) << "duplicates used to overwrite the map and leak the first files";
    removeAudioTempFilesOfSize(SIZE);
}

TEST(XoppAttachmentLoad, AttachmentThatIsNotAPictureDoesNotAbortTheProgram) {
    // Image::setImage() asserts on such data; a hostile document must not be able to end the program that way
    const auto p = makeXopp("not-a-picture", wrapXml(pageWithImage("attachments/a.png")),
                            {{"attachments/a.png", patternData(500)}});
    auto r = load(p, smallLimits());
    ASSERT_FALSE(r.threw) << r.what;
    ASSERT_TRUE(r.doc);
    EXPECT_EQ(countErrorsContaining(r, "damaged"), 1U);
    EXPECT_EQ(firstImage(*r.doc), nullptr);
}
