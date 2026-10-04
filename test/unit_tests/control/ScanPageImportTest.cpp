/*
 * Ultimate Teacher Notepad
 * Scan import tests, based on Xournal++ GPLv2+.
 */
#include <config-test.h>
#include <gtest/gtest.h>

#include "control/ScanPageImport.h"
#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "model/BackgroundImage.h"
#include "model/Document.h"
#include "model/XojPage.h"
#include "util/PathUtil.h"

TEST(ScanPageImport, embedsWorksheetAndPreservesAspectRatio) {
    auto result = xoj::utn::createScanPage(fs::path(GET_TESTFILE(u8"load/pages.xopp.bg_1.png")), 595);
    ASSERT_TRUE(std::holds_alternative<PageRef>(result));
    const auto& page = std::get<PageRef>(result);
    ASSERT_TRUE(page->getBackgroundType().isImagePage());
    EXPECT_TRUE(page->getBackgroundImage().isAttached());
    EXPECT_EQ(page->getLayerCount(), 1U);
    EXPECT_DOUBLE_EQ(page->getWidth(), 595);
    auto* pixels = page->getBackgroundImage().getPixbuf();
    EXPECT_DOUBLE_EQ(page->getHeight(), 595.0 * gdk_pixbuf_get_height(pixels) / gdk_pixbuf_get_width(pixels));

    Document doc(nullptr);
    doc.insertPage(page, 0);
    auto path = Util::getTmpDirSubfolder() / "scan-import.xopp";
    SaveHandler save;
    save.prepareSave(&doc, path);
    ASSERT_NO_THROW(save.saveTo(path));
    auto reloaded = LoadHandler{}.loadDocument(path);
    ASSERT_TRUE(reloaded);
    EXPECT_EQ(reloaded->getPageCount(), 1U);
    const auto& restored = reloaded->getPage(0);
    EXPECT_TRUE(restored->getBackgroundType().isImagePage());
    EXPECT_TRUE(restored->getBackgroundImage().isAttached());
    EXPECT_EQ(restored->getUtnPageLabel(), page->getUtnPageLabel());
    EXPECT_DOUBLE_EQ(restored->getWidth(), page->getWidth());
    EXPECT_DOUBLE_EQ(restored->getHeight(), page->getHeight());
}

TEST(ScanPageImport, rejectsUnreadableFilesAndInvalidSizes) {
    EXPECT_TRUE(std::holds_alternative<std::string>(xoj::utn::createScanPage("no-such-worksheet.png", 595)));
    EXPECT_TRUE(std::holds_alternative<std::string>(xoj::utn::createScanPage("unused.png", 0)));
}

TEST(ScanPageImport, appliesCameraOrientationWithoutChangingSharedImages) {
    const fs::path path(GET_TESTFILE(u8"images/r90.jpg"));
    BackgroundImage original;
    GError* error = nullptr;
    original.loadFile(path, &error);
    ASSERT_EQ(error, nullptr);
    ASSERT_NE(original.getPixbuf(), nullptr);
    auto oriented = original;
    oriented.applyEmbeddedOrientation();
    EXPECT_EQ(gdk_pixbuf_get_width(original.getPixbuf()), 500);
    EXPECT_EQ(gdk_pixbuf_get_height(original.getPixbuf()), 130);
    EXPECT_EQ(gdk_pixbuf_get_width(oriented.getPixbuf()), 130);
    EXPECT_EQ(gdk_pixbuf_get_height(oriented.getPixbuf()), 500);

    auto result = xoj::utn::createScanPage(path, 260);
    ASSERT_TRUE(std::holds_alternative<PageRef>(result));
    EXPECT_DOUBLE_EQ(std::get<PageRef>(result)->getHeight(), 1000);
}
