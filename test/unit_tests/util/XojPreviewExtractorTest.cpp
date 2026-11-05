/*
 * Xournal++
 *
 * This file is part of the Xournal UnitTests
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#include <cstdlib>
#include <ctime>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <zip.h>

#include "util/XojPreviewExtractor.h"

#include "config-test.h"
#include "util/StringUtils.h"
#include "filesystem.h"


using namespace std;

namespace {
fs::path writeThumbnailArchive(std::string data) {
    const fs::path path = fs::temp_directory_path() / ("utn-thumbnail-" + std::to_string(g_random_int()) + ".xopp");
    int error = 0;
    zip_t* archive = zip_open(char_cast(path.u8string().c_str()), ZIP_CREATE | ZIP_TRUNCATE, &error);
    if (!archive) {
        return {};
    }
    zip_source_t* source = zip_source_buffer(archive, data.data(), data.size(), 0);
    if (!source || zip_file_add(archive, "thumbnails/thumbnail.png", source, ZIP_FL_ENC_UTF_8) < 0) {
        if (source) zip_source_free(source);
        zip_discard(archive);
        return {};
    }
    if (zip_set_file_compression(archive, 0, ZIP_CM_STORE, 0) != 0) {
        zip_discard(archive);
        return {};
    }
    if (zip_close(archive) != 0) {
        zip_discard(archive);
        return {};
    }
    return path;
}

bool patchThumbnailUncompressedSize(const fs::path& path, uint32_t size) {
    std::ifstream in(path, std::ios::binary);
    std::string bytes(std::istreambuf_iterator<char>(in), {});
    const std::string signature("PK\x01\x02", 4);
    const auto at = bytes.find(signature);
    if (at == std::string::npos || at + 28 > bytes.size()) return false;
    for (unsigned int i = 0; i < 4; ++i) bytes[at + 24 + i] = static_cast<char>(size >> (8 * i));
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

struct TemporaryArchive {
    fs::path path;
    ~TemporaryArchive() { if (!path.empty()) fs::remove(path); }
};
}  // namespace


TEST(UtilXojPreviewExtractor, testNonExistingFile) {
    XojPreviewExtractor extractor;
    PreviewExtractResult result = extractor.readFile(GET_TESTFILE(u8"THIS FILE DOES NOT EXIST.xoj"));

    EXPECT_EQ(PREVIEW_RESULT_COULD_NOT_OPEN_FILE, result);
}

TEST(UtilXojPreviewExtractor, testExtensionCheck) {
    XojPreviewExtractor extractor;
    PreviewExtractResult result = extractor.readFile(GET_TESTFILE(u8"test.xoi"));

    EXPECT_EQ(PREVIEW_RESULT_BAD_FILE_EXTENSION, result);
}

TEST(UtilXojPreviewExtractor, testLoadGzipped) {
    XojPreviewExtractor extractor;
    PreviewExtractResult result = extractor.readFile(GET_TESTFILE(u8"preview-test.xoj"));

    EXPECT_EQ(PREVIEW_RESULT_IMAGE_READ, result);

    gsize dataLen = 0;
    unsigned char* imageData = extractor.getData(dataLen);
    EXPECT_EQ(string("CppUnitTestString"), string((char*)imageData, (size_t)dataLen));
}

TEST(UtilXojPreviewExtractor, testLoadGzipped2) {
    XojPreviewExtractor extractor;
    PreviewExtractResult result = extractor.readFile(GET_TESTFILE(u8"preview-test2.xoj"));

    EXPECT_EQ(PREVIEW_RESULT_IMAGE_READ, result);

    gsize dataLen = 0;
    extractor.getData(dataLen);
    EXPECT_EQ((std::string::size_type)2856, dataLen);
}

TEST(UtilXojPreviewExtractor, testLoad1Unzipped) {
    XojPreviewExtractor extractor;
    PreviewExtractResult result = extractor.readFile(GET_TESTFILE(u8"preview-test.unzipped.xoj"));

    EXPECT_EQ(PREVIEW_RESULT_IMAGE_READ, result);

    gsize dataLen = 0;
    unsigned char* imageData = extractor.getData(dataLen);
    EXPECT_EQ(string("CppUnitTestString"), string((char*)imageData, (size_t)dataLen));
}

TEST(UtilXojPreviewExtractor, testLoad1Zipped) {
    XojPreviewExtractor extractor;
    PreviewExtractResult result = extractor.readFile(GET_TESTFILE(u8"packaged_xopp/testPreview.xopp"));

    EXPECT_EQ(PREVIEW_RESULT_IMAGE_READ, result);

    gsize dataLen = 0;
    unsigned char* imageData = extractor.getData(dataLen);
    EXPECT_EQ(string("CppUnitTestString \n"), string((char*)imageData, (size_t)dataLen));
}

TEST(UtilXojPreviewExtractor, testLoad2Zipped) {
    XojPreviewExtractor extractor;
    PreviewExtractResult result = extractor.readFile(GET_TESTFILE(u8"packaged_xopp/testPreview2.xopp"));

    EXPECT_EQ(PREVIEW_RESULT_IMAGE_READ, result);

    gsize dataLen = 0;
    extractor.getData(dataLen);
    EXPECT_EQ((std::string::size_type)804, dataLen);
}

TEST(UtilXojPreviewExtractor, testNoPreview) {
    XojPreviewExtractor extractor;
    PreviewExtractResult result = extractor.readFile(GET_TESTFILE(u8"preview-test-no-preview.unzipped.xoj"));

    EXPECT_EQ(PREVIEW_RESULT_NO_PREVIEW, result);
}

TEST(UtilXojPreviewExtractor, testInvalidFile) {
    XojPreviewExtractor extractor;
    PreviewExtractResult result = extractor.readFile(GET_TESTFILE(u8"preview-test-invalid.xoj"));

    EXPECT_EQ(PREVIEW_RESULT_ERROR_READING_PREVIEW, result);
}

TEST(UtilXojPreviewExtractor, rejectsOversizedThumbnailZipEntry) {
    TemporaryArchive archive{writeThumbnailArchive(std::string(1024 * 1024 + 1, 'x'))};
    ASSERT_FALSE(archive.path.empty());
    XojPreviewExtractor extractor;
    EXPECT_EQ(PREVIEW_RESULT_ERROR_READING_PREVIEW, extractor.readFile(archive.path));
}

TEST(UtilXojPreviewExtractor, rejectsThumbnailDeclaredLongerThanStoredData) {
    TemporaryArchive archive{writeThumbnailArchive("short-thumbnail")};
    ASSERT_FALSE(archive.path.empty());
    ASSERT_TRUE(patchThumbnailUncompressedSize(archive.path, 16));
    XojPreviewExtractor extractor;
    EXPECT_EQ(PREVIEW_RESULT_ERROR_READING_PREVIEW, extractor.readFile(archive.path));
}

TEST(UtilXojPreviewExtractor, rejectsThumbnailDeclaredShorterThanStoredData) {
    TemporaryArchive archive{writeThumbnailArchive("short-thumbnail")};
    ASSERT_FALSE(archive.path.empty());
    ASSERT_TRUE(patchThumbnailUncompressedSize(archive.path, 14));
    XojPreviewExtractor extractor;
    EXPECT_EQ(PREVIEW_RESULT_ERROR_READING_PREVIEW, extractor.readFile(archive.path));
}
