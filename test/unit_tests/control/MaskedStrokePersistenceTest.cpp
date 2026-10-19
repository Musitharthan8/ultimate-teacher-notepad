#include <limits>
#include <memory>
#include <string>
#include <gtest/gtest.h>
#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "model/Document.h"
#include "model/Layer.h"
#include "model/Image.h"
#include "model/Stroke.h"
#include "model/XojPage.h"
#include "model/eraser/MaskedStrokeCodec.h"
#include "model/eraser/StrokeErasure.h"
#include "util/PathUtil.h"
#include "util/GzInputStream.h"
#include "util/OutputStream.h"
#include "util/serializing/BinObjectEncoding.h"
#include "util/serializing/ObjectInputStream.h"
#include "util/serializing/ObjectOutputStream.h"
#include "view/StrokeView.h"
#include "view/View.h"

namespace {
std::unique_ptr<Stroke> maskedStroke() {
    auto s = std::make_unique<Stroke>();
    s->setWidth(30);
    s->setColor(Color{30, 60, 90, 200});
    s->addPoint(Point(20.125, 50.25, 30.5));
    s->addPoint(Point(180.75, 50.25, 12.25));
    s->setErasures({{{100, 30}, {100, 70}, 12.75, {}}});
    return s;
}
struct PersistencePage: XojPage {
    PersistencePage(): XojPage(200, 200) {}
};
std::string readXml(const fs::path& file) {
    xoj::util::GzInputStream input(file);
    std::string result;
    char buffer[4096];
    for (int count; (count = input.read(buffer, sizeof(buffer))) > 0;) {
        result.append(buffer, count);
    }
    return result;
}
void writeXml(const fs::path& file, const std::string& xml) {
    GzOutputStream output(file);
    output.write(xml.data(), xml.size());
    output.close();
}
uint32_t strokePixel(const Stroke& stroke, int x, int y) {
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 220, 120);
    auto* cr = cairo_create(surface);
    xoj::view::StrokeView(&stroke).draw(xoj::view::Context::createDefault(cr));
    cairo_surface_flush(surface);
    const auto* row = reinterpret_cast<const uint32_t*>(cairo_image_surface_get_data(surface) +
                                                       y * cairo_image_surface_get_stride(surface));
    const auto pixel = row[x];
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return pixel;
}
}

TEST(MaskedStrokePersistence, textCodecPreservesPressureStyleAndAffineMasks) {
    auto s = maskedStroke();
    s->setStrokeCapStyle(StrokeCapStyle::SQUARE);
    LineStyle line;
    line.setDashes({0, 3.75}); // Cairo permits zero-length round-cap dots.
    s->setLineStyle(line);
    s->setFill(125);
    s->setToolType(StrokeTool::HIGHLIGHTER);
    auto masks = s->getErasures();
    masks[0].transform = {1.25, .2, -.3, .75, {2.125, -8.5}};
    s->setErasures(masks);
    const xoj::util::Matrix fallback{.25, 0, 0, .25, {5.5, -7.25}};
    const auto text = utn::eraser::encodeMaskedStroke(*s, fallback);
    ASSERT_FALSE(text.empty());
    auto decoded = utn::eraser::decodeMaskedStroke(text);
    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded->fallbackMatrix, fallback);
    EXPECT_EQ(decoded->stroke->getPointVector(), s->getPointVector());
    EXPECT_EQ(decoded->stroke->getColor(), s->getColor());
    EXPECT_DOUBLE_EQ(decoded->stroke->getWidth(), s->getWidth());
    EXPECT_EQ(decoded->stroke->getLineStyle(), line);
    EXPECT_EQ(decoded->stroke->getFill(), 125);
    EXPECT_EQ(decoded->stroke->getStrokeCapStyle(), StrokeCapStyle::SQUARE);
    EXPECT_EQ(decoded->stroke->getToolType(), StrokeTool::HIGHLIGHTER);
    ASSERT_EQ(decoded->stroke->getErasures().size(), 1U);
    EXPECT_EQ(decoded->stroke->getErasures()[0].transform, masks[0].transform);
    EXPECT_DOUBLE_EQ(decoded->stroke->getErasures()[0].radius, 12.75);
}

TEST(MaskedStrokePersistence, corruptUnknownAndNonfiniteMetadataIsRejected) {
    for (auto text: {"", "UTN-STROKE 99", "UTN-STROKE 1 nan", "UTN-STROKE 1 inf", "garbage"}) {
        EXPECT_FALSE(utn::eraser::decodeMaskedStroke(text));
    }
    auto s = maskedStroke();
    const auto encoded = utn::eraser::encodeMaskedStroke(*s, {});
    EXPECT_FALSE(utn::eraser::decodeMaskedStroke(encoded + " trailing"));
    EXPECT_FALSE(utn::eraser::decodeMaskedStroke(encoded.substr(0, encoded.size() / 2)));
    EXPECT_FALSE(utn::eraser::decodeMaskedStroke(std::string(utn::eraser::MASKED_STROKE_MAX_TEXT + 1, '0')));
    s->setWidth(std::numeric_limits<double>::infinity());
    EXPECT_TRUE(utn::eraser::encodeMaskedStroke(*s, {}).empty());
}

TEST(MaskedStrokePersistence, transformedFallbackCannotRestoreStaleVector) {
    const auto expected = xoj::util::Matrix::SCALING(.25, .25).translate(20, 30);
    EXPECT_TRUE(utn::eraser::matchesFallbackMatrix(expected, expected));
    EXPECT_FALSE(utn::eraser::matchesFallbackMatrix(expected.translate(1, 0), expected));
    EXPECT_FALSE(utn::eraser::matchesFallbackMatrix(expected.scale(2, 1), expected));
    EXPECT_FALSE(utn::eraser::matchesFallbackMatrix({0, 0, 0, 0, {}}, expected));
}

TEST(MaskedStrokePersistence, decoderBoundsCountsBeforeAllocation) {
    // Version, fallback matrix, style, timestamp/audio, then dash count.
    const std::string prefix = "UTN-STROKE 1 1 0 0 1 0 0 0 0 -1 4278190080 3 0 \"\" ";
    EXPECT_FALSE(utn::eraser::decodeMaskedStroke(prefix + "999999999999999999999"));
    EXPECT_FALSE(utn::eraser::decodeMaskedStroke(prefix + "0 100001"));
    EXPECT_FALSE(utn::eraser::decodeMaskedStroke(prefix + "0 2 0 0 -1 10 0 -1 4097"));
    EXPECT_FALSE(utn::eraser::decodeMaskedStroke(prefix + "0 2 0 0 -1 10 0 -1 1 0 0 0 0 -2"));
    auto s = maskedStroke();
    s->setErasures(std::vector<utn::eraser::StrokeErasure>(utn::eraser::MASKED_STROKE_MAX_ERASURES + 1));
    EXPECT_TRUE(utn::eraser::encodeMaskedStroke(*s, {}).empty());
}

TEST(MaskedStrokePersistence, rasterFallbackBoundsAllocationAndResolution) {
    auto s = maskedStroke();
    s->setPointVector({Point(-10000, -10000), Point(10000, 10000)});
    auto fallback = utn::eraser::rasterizeMaskedStroke(*s);
    ASSERT_TRUE(fallback);
    const auto width = cairo_image_surface_get_width(fallback->surface.get());
    const auto height = cairo_image_surface_get_height(fallback->surface.get());
    EXPECT_LE(width, 8192);
    EXPECT_LE(height, 8192);
    EXPECT_LE(static_cast<size_t>(width) * height, 16U * 1024 * 1024);
    EXPECT_GE(fallback->matrix.xx, .25);
    EXPECT_GE(fallback->matrix.yy, .25);
}

TEST(MaskedStrokePersistence, maskedXoppReopensAsEditablePressureStroke) {
    Document doc(nullptr);
    auto page = std::make_shared<PersistencePage>();
    auto original = maskedStroke();
    const auto points = original->getPointVector();
    const auto expectedInk = strokePixel(*original, 40, 50);
    EXPECT_NE(expectedInk, 0U);
    EXPECT_EQ(strokePixel(*original, 100, 50), 0U);
    page->getSelectedLayer()->addElement(std::move(original));
    doc.addPage(page);
    const auto file = Util::getTmpDirSubfolder() / "utn-masked-stroke.xopp";
    SaveHandler saver;
    saver.prepareSave(&doc, file);
    saver.saveTo(file);
    ASSERT_TRUE(saver.getErrorMessage().empty()) << saver.getErrorMessage();
    auto reopened = LoadHandler{}.loadDocument(file);
    fs::remove(file);
    ASSERT_TRUE(reopened);
    const auto& elements = reopened->getPage(0)->getSelectedLayer()->getElements();
    ASSERT_EQ(elements.size(), 1U);
    ASSERT_EQ(elements[0]->getType(), ELEMENT_STROKE);
    const auto* stroke = static_cast<const Stroke*>(elements[0].get());
    EXPECT_EQ(stroke->getPointVector(), points);
    ASSERT_EQ(stroke->getErasures().size(), 1U);
    EXPECT_DOUBLE_EQ(stroke->getErasures()[0].radius, 12.75);
    EXPECT_EQ(strokePixel(*stroke, 100, 50), 0U);
    EXPECT_EQ(strokePixel(*stroke, 40, 50), expectedInk);
}

TEST(MaskedStrokePersistence, missingOrCorruptMetadataUsesClippedPngFallback) {
    Document doc(nullptr);
    auto page = std::make_shared<PersistencePage>();
    page->getSelectedLayer()->addElement(maskedStroke());
    doc.addPage(page);
    const auto file = Util::getTmpDirSubfolder() / "utn-masked-fallback.xopp";
    SaveHandler saver;
    saver.prepareSave(&doc, file);
    saver.saveTo(file);
    auto xml = readXml(file);
    const std::string attr = "utn-stroke-v1=\"";
    const auto start = xml.find(attr);
    ASSERT_NE(start, std::string::npos);
    const auto end = xml.find('"', start + attr.size());
    ASSERT_NE(end, std::string::npos);
    auto corrupt = xml;
    corrupt.replace(start + attr.size(), end - start - attr.size(), "UTN-STROKE 99");
    auto removed = xml;
    removed.erase(start, end - start + 1);
    for (const auto& input: {corrupt, removed}) {
        writeXml(file, input);
        auto reopened = LoadHandler{}.loadDocument(file);
        ASSERT_TRUE(reopened);
        const auto& items = reopened->getPage(0)->getSelectedLayer()->getElements();
        ASSERT_EQ(items.size(), 1U);
        ASSERT_EQ(items[0]->getType(), ELEMENT_IMAGE);
        auto* image = static_cast<Image*>(items[0].get());
        auto* surface = image->getImage();
        ASSERT_EQ(cairo_surface_status(surface), CAIRO_STATUS_SUCCESS);
        const auto inv = image->getTransformation().inverse();
        const auto hole = inv * xoj::util::Point<double>{100, 50};
        const auto ink = inv * xoj::util::Point<double>{40, 50};
        cairo_surface_flush(surface);
        const auto pixel = [surface](const auto& p) {
            const auto* row = reinterpret_cast<const uint32_t*>(cairo_image_surface_get_data(surface) +
                    static_cast<int>(p.y) * cairo_image_surface_get_stride(surface));
            return row[static_cast<int>(p.x)];
        };
        EXPECT_EQ(pixel(hole), 0U);
        EXPECT_NE(pixel(ink), 0U);
    }
    fs::remove(file);
}

TEST(MaskedStrokePersistence, ordinaryStrokesKeepStandardXoppEncoding) {
    Document doc(nullptr);
    auto page = std::make_shared<PersistencePage>();
    auto s = maskedStroke();
    s->setErasures({});
    page->getSelectedLayer()->addElement(std::move(s));
    doc.addPage(page);
    const auto file = Util::getTmpDirSubfolder() / "utn-ordinary-stroke.xopp";
    SaveHandler saver;
    saver.prepareSave(&doc, file);
    saver.saveTo(file);
    const auto xml = readXml(file);
    fs::remove(file);
    EXPECT_NE(xml.find("<stroke "), std::string::npos);
    EXPECT_EQ(xml.find("<image "), std::string::npos);
    EXPECT_EQ(xml.find("utn-stroke-v1"), std::string::npos);
}

TEST(MaskedStrokePersistence, olderReaderMovedFallbackStaysAnImage) {
    Document doc(nullptr);
    auto page = std::make_shared<PersistencePage>();
    page->getSelectedLayer()->addElement(maskedStroke());
    doc.addPage(page);
    const auto file = Util::getTmpDirSubfolder() / "utn-masked-moved.xopp";
    SaveHandler saver;
    saver.prepareSave(&doc, file);
    saver.saveTo(file);
    auto xml = readXml(file);
    const auto imageStart = xml.find("<image ");
    ASSERT_NE(imageStart, std::string::npos);
    const auto left = xml.find("left=\"", imageStart);
    ASSERT_NE(left, std::string::npos);
    const auto end = xml.find('"', left + 6);
    ASSERT_NE(end, std::string::npos);
    xml.replace(left + 6, end - left - 6, "-100");
    writeXml(file, xml);
    auto reopened = LoadHandler{}.loadDocument(file);
    fs::remove(file);
    ASSERT_TRUE(reopened);
    const auto& items = reopened->getPage(0)->getSelectedLayer()->getElements();
    ASSERT_EQ(items.size(), 1U);
    EXPECT_EQ(items[0]->getType(), ELEMENT_IMAGE);
    EXPECT_DOUBLE_EQ(items[0]->getBoundingBox().x, -100);
}

TEST(MaskedStrokePersistence, metadataLimitStillSavesTheClippedLesson) {
    Document doc(nullptr);
    auto page = std::make_shared<PersistencePage>();
    auto s = maskedStroke();
    s->setErasures(std::vector<utn::eraser::StrokeErasure>(utn::eraser::MASKED_STROKE_MAX_ERASURES + 1,
                            s->getErasures().front()));
    page->getSelectedLayer()->addElement(std::move(s));
    doc.addPage(page);
    const auto file = Util::getTmpDirSubfolder() / "utn-masked-complex.xopp";
    SaveHandler saver;
    saver.prepareSave(&doc, file);
    saver.saveTo(file);
    ASSERT_TRUE(saver.getErrorMessage().empty());
    auto reopened = LoadHandler{}.loadDocument(file);
    fs::remove(file);
    ASSERT_TRUE(reopened);
    const auto& items = reopened->getPage(0)->getSelectedLayer()->getElements();
    ASSERT_EQ(items.size(), 1U);
    EXPECT_EQ(items[0]->getType(), ELEMENT_IMAGE);
}

TEST(MaskedStrokePersistence, clipboardKeepsMasksAndLegacyPayloadClearsThem) {
    auto source = maskedStroke();
    Stroke result;
    for (bool legacy: {false, true}) {
        if (legacy) source->setErasures({});
        ObjectOutputStream output(new BinObjectEncoding);
        source->serialize(output);
        auto* data = output.stealData();
        ObjectInputStream input;
        ASSERT_TRUE(input.read(data->str, data->len));
        g_string_free(data, true);
        result.readSerialized(input);
        EXPECT_EQ(result.getPointVector(), source->getPointVector());
        EXPECT_EQ(result.getErasures().size(), legacy ? 0U : 1U);
        EXPECT_EQ(strokePixel(result, 100, 50), strokePixel(*source, 100, 50));
    }
}
