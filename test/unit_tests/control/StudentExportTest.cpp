#include <cstdint>
#include <memory>
#include <string>

#include <gtest/gtest.h>
#include <poppler.h>
#include <cairo-pdf.h>
#include <librsvg/rsvg.h>

#include "control/ExportHelper.h"
#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "pdf/base/XojPdfExportFactory.h"
#include "pdf/base/XojPdfExport.h"
#include "model/Document.h"
#include "model/DocumentHandler.h"
#include "model/Layer.h"
#include "model/PageType.h"
#include "model/Stroke.h"
#include "model/Text.h"
#include "model/XojPage.h"
#include "util/PathUtil.h"
#include "util/raii/GObjectSPtr.h"
#include "view/DocumentView.h"
#include "model/LayerAudience.h"

namespace {
struct ExportPage: XojPage {
    ExportPage(): XojPage(200, 200, true) { setBackgroundType(PageType(PageTypeFormat::Plain)); }
    using XojPage::addLayer;
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

auto makePage() {
    auto page = std::make_shared<ExportPage>();
    struct Entry { LayerAudience audience; bool visible; int y; const char* text; };
    for (const auto& e: {Entry{LayerAudience::Everyone, true, 20, "PUBLIC_LESSON"},
                         {LayerAudience::TeacherOnly, true, 60, "PRIVATE_TEACHER_KEY"},
                         {LayerAudience::Answers, false, 100, "HIDDEN_ANSWER_KEY"},
                         {LayerAudience::Everyone, false, 140, "HIDDEN_PUBLIC_DRAFT"},
                         {LayerAudience::Answers, true, 180, "REVEALED_ANSWER"}}) {
        auto* layer = new Layer();
        layer->setAudience(e.audience);
        layer->setVisible(e.visible);
        addMarker(*layer, e.y, e.text);
        page->addLayer(layer);
    }
    page->setSelectedLayerId(1);
    return page;
}

auto readPdf(const fs::path& file) {
    GError* error = nullptr;
    gchar* uri = g_filename_to_uri(file.string().c_str(), nullptr, &error);
    EXPECT_EQ(error, nullptr) << (error ? error->message : "");
    auto* pdf = uri ? poppler_document_new_from_file(uri, nullptr, &error) : nullptr;
    g_free(uri);
    EXPECT_EQ(error, nullptr) << (error ? error->message : "");
    g_clear_error(&error);
    return xoj::util::GObjectSPtr<PopplerDocument>(pdf, xoj::util::adopt);
}

uint32_t pixel(cairo_surface_t* surface, int y) {
    cairo_surface_flush(surface);
    const auto* row = cairo_image_surface_get_data(surface) + y * cairo_image_surface_get_stride(surface);
    return reinterpret_cast<const uint32_t*>(row)[100] & 0xFFFFFFU;
}
}  // namespace

TEST(StudentExport, pdfAndProgressiveAndExplicitLayerRangeExcludePrivateContent) {
    for (bool progressive: {false, true}) {
        for (bool explicitLayers: {false, true}) {
            SCOPED_TRACE(::testing::Message() << "progressive=" << progressive << " layerRange=" << explicitLayers);
            Document doc(nullptr);
            auto page = makePage();
            doc.addPage(page);
            const auto file = Util::getTmpDirSubfolder() /
                    ("student-privacy-" + std::to_string(progressive) + std::to_string(explicitLayers) + ".pdf");
            ExportHelper::exportPdf(&doc, file, nullptr, explicitLayers ? "1-5" : nullptr,
                                    EXPORT_BACKGROUND_ALL, progressive, ExportBackend::CAIRO);
            auto pdf = readPdf(file);
            ASSERT_NE(pdf.get(), nullptr);
            const int count = poppler_document_get_n_pages(pdf.get());
            ASSERT_GT(count, 0);
            std::string allText;
            for (int i = 0; i < count; ++i) {
                auto p = xoj::util::GObjectSPtr<PopplerPage>(poppler_document_get_page(pdf.get(), i), xoj::util::adopt);
                gchar* text = poppler_page_get_text(p.get());
                if (text) allText += text;
                g_free(text);
                auto* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 200, 200);
                auto* cr = cairo_create(surface);
                cairo_set_source_rgb(cr, 1, 1, 1);
                cairo_paint(cr);
                poppler_page_render(p.get(), cr);
                EXPECT_EQ(pixel(surface, 20), 0x000000U);
                EXPECT_EQ(pixel(surface, 60), 0xFFFFFFU) << "teacher-only ink";
                EXPECT_EQ(pixel(surface, 100), 0xFFFFFFU) << "hidden answers";
                EXPECT_EQ(pixel(surface, 140), 0xFFFFFFU) << "hidden draft";
                if (i == count - 1) {
                    EXPECT_EQ(pixel(surface, 180), 0x000000U);
                }
                cairo_destroy(cr);
                cairo_surface_destroy(surface);
            }
            EXPECT_NE(allText.find("PUBLIC_LESSON"), std::string::npos);
            EXPECT_NE(allText.find("REVEALED_ANSWER"), std::string::npos);
            EXPECT_EQ(allText.find("PRIVATE_TEACHER_KEY"), std::string::npos);
            EXPECT_EQ(allText.find("HIDDEN_ANSWER_KEY"), std::string::npos);
            EXPECT_EQ(allText.find("HIDDEN_PUBLIC_DRAFT"), std::string::npos);
            EXPECT_TRUE(page->getLayers()[1]->isVisible());
            EXPECT_FALSE(page->getLayers()[2]->isVisible());
            EXPECT_FALSE(page->getLayers()[3]->isVisible());
            EXPECT_TRUE(page->getLayers()[4]->isVisible());
        }
    }
}

TEST(StudentExport, explicitRangesRespectTheStudentRendererFilter) {
    auto page = makePage();
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 200, 200);
    auto* cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    DocumentView view;
    view.setLayerFilter(utn::visibleToStudents);
    view.drawLayersOfPage({{0, 4}}, page, cr, true);
    EXPECT_EQ(pixel(surface, 20), 0x000000U);
    EXPECT_EQ(pixel(surface, 60), 0xFFFFFFU);
    EXPECT_EQ(pixel(surface, 100), 0xFFFFFFU);
    EXPECT_EQ(pixel(surface, 140), 0xFFFFFFU);
    EXPECT_EQ(pixel(surface, 180), 0x000000U);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

TEST(StudentExport, pngExportAndExplicitLayerRangeExcludePrivateContent) {
    for (bool explicitLayers: {false, true}) {
        SCOPED_TRACE(explicitLayers);
        Document doc(nullptr);
        auto page = makePage();
        doc.addPage(page);
        const auto file = Util::getTmpDirSubfolder() / ("student-image-" + std::to_string(explicitLayers) + ".png");
        ExportHelper::exportImg(&doc, file, nullptr, explicitLayers ? "1-5" : nullptr,
                                0, 200, 0, EXPORT_BACKGROUND_ALL);
        auto* surface = cairo_image_surface_create_from_png(file.string().c_str());
        ASSERT_EQ(cairo_surface_status(surface), CAIRO_STATUS_SUCCESS);
        EXPECT_EQ(pixel(surface, 20), 0x000000U);
        EXPECT_EQ(pixel(surface, 60), 0xFFFFFFU);
        EXPECT_EQ(pixel(surface, 100), 0xFFFFFFU);
        EXPECT_EQ(pixel(surface, 140), 0xFFFFFFU);
        EXPECT_EQ(pixel(surface, 180), 0x000000U);
        cairo_surface_destroy(surface);
    }
}

TEST(StudentExport, svgOutputExcludesPrivateInk) {
    Document doc(nullptr);
    doc.addPage(makePage());
    const auto file = Util::getTmpDirSubfolder() / "student-privacy.svg";
    ExportHelper::exportImg(&doc, file, nullptr, "1-5", 0, 0, 0, EXPORT_BACKGROUND_ALL);
    GError* error = nullptr;
    auto svg = xoj::util::GObjectSPtr<RsvgHandle>(rsvg_handle_new_from_file(file.string().c_str(), &error), xoj::util::adopt);
    ASSERT_NE(svg.get(), nullptr) << (error ? error->message : "");
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 200, 200);
    auto* cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    const RsvgRectangle viewport{0, 0, 200, 200};
    EXPECT_TRUE(rsvg_handle_render_document(svg.get(), cr, &viewport, &error));
    EXPECT_EQ(pixel(surface, 20), 0x000000U);
    EXPECT_EQ(pixel(surface, 60), 0xFFFFFFU);
    EXPECT_EQ(pixel(surface, 100), 0xFFFFFFU);
    EXPECT_EQ(pixel(surface, 140), 0xFFFFFFU);
    EXPECT_EQ(pixel(surface, 180), 0x000000U);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    g_clear_error(&error);
}

TEST(StudentExport, explicitTeacherPdfIncludesPrivateNotesAndNativeLessonRetainsEverything) {
    Document doc(nullptr);
    auto page = makePage();
    doc.addPage(page);
    const auto file = Util::getTmpDirSubfolder() / "explicit-teacher.pdf";
    auto exporter = XojPdfExportFactory::createExport(&doc, nullptr, ExportBackend::CAIRO);
    exporter->setExportAudience(ExportAudience::Teacher);
    ASSERT_TRUE(exporter->createPdf(file, false));
    auto pdf = readPdf(file);
    ASSERT_NE(pdf.get(), nullptr);
    auto p = xoj::util::GObjectSPtr<PopplerPage>(poppler_document_get_page(pdf.get(), 0), xoj::util::adopt);
    gchar* text = poppler_page_get_text(p.get());
    const std::string content = text ? text : "";
    g_free(text);
    EXPECT_NE(content.find("PRIVATE_TEACHER_KEY"), std::string::npos);
    EXPECT_EQ(content.find("HIDDEN_ANSWER_KEY"), std::string::npos);
    const auto native = Util::getTmpDirSubfolder() / "private-lesson-after-export.xopp";
    SaveHandler saver;
    saver.prepareSave(&doc, native);
    saver.saveTo(native);
    ASSERT_TRUE(saver.getErrorMessage().empty()) << saver.getErrorMessage();
    auto reopened = LoadHandler{}.loadDocument(native);
    ASSERT_NE(reopened, nullptr);
    ASSERT_EQ(reopened->getPage(0)->getLayerCount(), 5U);
    EXPECT_EQ(reopened->getPage(0)->getLayers()[1]->getAudience(), LayerAudience::TeacherOnly);
    EXPECT_EQ(reopened->getPage(0)->getLayers()[1]->getElements().size(), 2U);
    EXPECT_EQ(reopened->getPage(0)->getLayers()[2]->getElements().size(), 2U);
    EXPECT_FALSE(reopened->getPage(0)->getLayers()[2]->isVisible());
}

TEST(StudentExport, importedPdfDefaultAndCairoBackendsExcludePrivateOverlayText) {
    const auto background = Util::getTmpDirSubfolder() / "privacy-background.pdf";
    auto* surface = cairo_pdf_surface_create(background.string().c_str(), 200, 200);
    auto* cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    cairo_show_page(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    for (auto backend: {ExportBackend::DEFAULT, ExportBackend::CAIRO}) {
        SCOPED_TRACE(static_cast<int>(backend));
        DocumentHandler handler;
        Document doc(&handler);
        ASSERT_TRUE(doc.readPdf(background, false, false));
        auto page = makePage();
        page->setBackgroundPdfPageNr(0);
        doc.addPage(page);
        const auto output = Util::getTmpDirSubfolder() /
                ("private-overlay-" + std::to_string(static_cast<int>(backend)) + ".pdf");
        ExportHelper::exportPdf(&doc, output, nullptr, nullptr, EXPORT_BACKGROUND_ALL, false, backend);
        auto pdf = readPdf(output);
        ASSERT_NE(pdf.get(), nullptr);
        auto p = xoj::util::GObjectSPtr<PopplerPage>(poppler_document_get_page(pdf.get(), 0), xoj::util::adopt);
        gchar* text = poppler_page_get_text(p.get());
        const std::string content = text ? text : "";
        g_free(text);
        EXPECT_NE(content.find("PUBLIC_LESSON"), std::string::npos);
        EXPECT_EQ(content.find("PRIVATE_TEACHER_KEY"), std::string::npos);
        EXPECT_EQ(content.find("HIDDEN_ANSWER_KEY"), std::string::npos);
    }
}
