/*
 * Ultimate Teacher Notepad
 *
 * "Export as Xournal (Compatibility)" writes a .xoj that teachers send to students. It must follow the same student
 * rule as the PDF, PNG and SVG exports: no teacher-only layers, no hidden answers, no thumbnail that was rendered from
 * them. The native .xopp save stays the complete private lesson.
 *
 * Based on Xournal++ GPLv2+
 */

#include <memory>
#include <string>

#include <cairo.h>
#include <gtest/gtest.h>
#include <zlib.h>

#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "control/xojfile/XojExportHandler.h"
#include "model/Document.h"
#include "model/Layer.h"
#include "model/LayerAudience.h"
#include "model/PageType.h"
#include "model/Stroke.h"
#include "model/Text.h"
#include "model/XojPage.h"
#include "util/PathUtil.h"
#include "util/raii/CairoWrappers.h"

namespace {
struct TestPage: ::XojPage {
    TestPage(): ::XojPage(200, 200, true) { setBackgroundType(PageType(PageTypeFormat::Plain)); }
    using ::XojPage::addLayer;
};

void addMarker(Layer& layer, int y, const char* text) {
    auto stroke = std::make_unique<Stroke>();
    stroke->setWidth(8);
    stroke->setColor(Color{0, 0, 0});
    stroke->addPoint(Point(20, y));
    stroke->addPoint(Point(180, y));
    layer.addElement(std::move(stroke));
    auto label = std::make_unique<Text>();
    label->setText(text);
    label->setFont(XojFont("Sans", 9));
    label->move(20, y + 6);
    layer.addElement(std::move(label));
}

struct LayerSpec {
    LayerAudience audience;
    bool visible;
    const char* text;
};

auto makeDocument(std::initializer_list<LayerSpec> layers) {
    auto doc = std::make_unique<Document>(nullptr);
    auto page = std::make_shared<TestPage>();
    int y = 20;
    for (const auto& spec: layers) {
        auto* layer = new Layer();
        layer->setAudience(spec.audience);
        layer->setVisible(spec.visible);
        addMarker(*layer, y, spec.text);
        y += 40;
        page->addLayer(layer);
    }
    page->setSelectedLayerId(1);
    doc->addPage(page);
    // The thumbnail the real save writes: rendered from every visible layer, teacher content included
    doc->setPreview(xoj::util::CairoSurfaceSPtr(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 8, 8),
                                                 xoj::util::adopt));
    return doc;
}

auto readGz(const fs::path& file) -> std::string {
    gzFile in = gzopen(file.string().c_str(), "rb");
    EXPECT_NE(in, nullptr);
    std::string content;
    if (in) {
        char buffer[4096];
        int n = 0;
        while ((n = gzread(in, buffer, sizeof(buffer))) > 0) {
            content.append(buffer, static_cast<size_t>(n));
        }
        gzclose(in);
    }
    return content;
}

constexpr LayerSpec PUBLIC_LAYER{LayerAudience::Everyone, true, "PUBLIC_LESSON"};
constexpr LayerSpec TEACHER_LAYER{LayerAudience::TeacherOnly, true, "PRIVATE_TEACHER_KEY"};
constexpr LayerSpec HIDDEN_ANSWERS{LayerAudience::Answers, false, "HIDDEN_ANSWER_KEY"};
constexpr LayerSpec REVEALED_ANSWERS{LayerAudience::Answers, true, "REVEALED_ANSWER"};
constexpr LayerSpec HIDDEN_DRAFT{LayerAudience::Everyone, false, "HIDDEN_PUBLIC_DRAFT"};
}  // namespace

TEST(StudentXojExport, compatibilityExportOmitsTeacherLayersHiddenContentAndThumbnail) {
    auto doc = makeDocument({PUBLIC_LAYER, TEACHER_LAYER, HIDDEN_ANSWERS, HIDDEN_DRAFT, REVEALED_ANSWERS});
    const auto file = Util::getTmpDirSubfolder() / "student-compat.xoj";
    XojExportHandler exporter;
    exporter.prepareSave(doc.get(), file);
    exporter.saveTo(file);
    ASSERT_TRUE(exporter.getErrorMessage().empty()) << exporter.getErrorMessage();

    const std::string xml = readGz(file);
    ASSERT_FALSE(xml.empty());
    EXPECT_NE(xml.find("PUBLIC_LESSON"), std::string::npos);
    EXPECT_NE(xml.find("REVEALED_ANSWER"), std::string::npos) << "a revealed answers layer is part of the student copy";
    EXPECT_EQ(xml.find("PRIVATE_TEACHER_KEY"), std::string::npos) << "teacher-only layer";
    EXPECT_EQ(xml.find("HIDDEN_ANSWER_KEY"), std::string::npos) << "unrevealed answers";
    EXPECT_EQ(xml.find("HIDDEN_PUBLIC_DRAFT"), std::string::npos) << "hidden layer";
    EXPECT_EQ(xml.find("<preview"), std::string::npos) << "the thumbnail was rendered from every visible layer";
}

TEST(StudentXojExport, pageWithOnlyPrivateLayersStillWritesALayerSoOldReadersCanOpenIt) {
    auto doc = makeDocument({TEACHER_LAYER, HIDDEN_ANSWERS});
    const auto file = Util::getTmpDirSubfolder() / "student-compat-empty-page.xoj";
    XojExportHandler exporter;
    exporter.prepareSave(doc.get(), file);
    exporter.saveTo(file);
    ASSERT_TRUE(exporter.getErrorMessage().empty()) << exporter.getErrorMessage();

    const std::string xml = readGz(file);
    EXPECT_NE(xml.find("<layer"), std::string::npos);
    EXPECT_EQ(xml.find("PRIVATE_TEACHER_KEY"), std::string::npos);
    EXPECT_EQ(xml.find("HIDDEN_ANSWER_KEY"), std::string::npos);
}

TEST(StudentXojExport, nativeSaveStillWritesTheCompletePrivateLesson) {
    auto doc = makeDocument({PUBLIC_LAYER, TEACHER_LAYER, HIDDEN_ANSWERS, HIDDEN_DRAFT, REVEALED_ANSWERS});
    const auto file = Util::getTmpDirSubfolder() / "native-private-lesson.xopp";
    SaveHandler saver;
    saver.prepareSave(doc.get(), file);
    saver.saveTo(file);
    ASSERT_TRUE(saver.getErrorMessage().empty()) << saver.getErrorMessage();

    // .xopp is a zip with a compressed content.xml: reload it instead of grepping
    auto reopened = LoadHandler{}.loadDocument(file);
    ASSERT_NE(reopened, nullptr);
    ASSERT_EQ(reopened->getPage(0)->getLayerCount(), 5U);
    EXPECT_EQ(reopened->getPage(0)->getLayers()[1]->getAudience(), LayerAudience::TeacherOnly);
    EXPECT_EQ(reopened->getPage(0)->getLayers()[2]->getAudience(), LayerAudience::Answers);
    EXPECT_FALSE(reopened->getPage(0)->getLayers()[2]->isVisible());
    EXPECT_EQ(reopened->getPage(0)->getLayers()[1]->getElements().size(), 2U);
}

TEST(StudentXojExport, explicitTeacherOptInKeepsPrivateLayersAndThumbnail) {
    auto doc = makeDocument({PUBLIC_LAYER, TEACHER_LAYER, HIDDEN_ANSWERS});
    const auto file = Util::getTmpDirSubfolder() / "teacher-compat.xoj";
    XojExportHandler exporter;
    exporter.setExportAudience(ExportAudience::Teacher);
    exporter.prepareSave(doc.get(), file);
    exporter.saveTo(file);
    ASSERT_TRUE(exporter.getErrorMessage().empty()) << exporter.getErrorMessage();

    const std::string xml = readGz(file);
    EXPECT_NE(xml.find("PUBLIC_LESSON"), std::string::npos);
    EXPECT_NE(xml.find("PRIVATE_TEACHER_KEY"), std::string::npos);
    EXPECT_NE(xml.find("HIDDEN_ANSWER_KEY"), std::string::npos);
    EXPECT_NE(xml.find("<preview"), std::string::npos);
}
