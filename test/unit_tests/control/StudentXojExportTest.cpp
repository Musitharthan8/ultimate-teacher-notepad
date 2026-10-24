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
#include <cairo-pdf.h>
#include <gtest/gtest.h>
#include <zlib.h>

#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "control/xojfile/XojExportHandler.h"
#include "model/Document.h"
#include "model/DocumentHandler.h"
#include "model/Image.h"
#include "model/Layer.h"
#include "model/LayerAudience.h"
#include "model/PageType.h"
#include "model/Stroke.h"
#include "model/Text.h"
#include "model/TexImage.h"
#include "model/eraser/StrokeErasure.h"
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

TEST(StudentXojExport, omitsEditableErasedInkAudioAndPrivateNamesButKeepsRenderedInk) {
    auto doc = makeDocument({PUBLIC_LAYER});
    auto page = doc->getPage(0);
    page->setUtnPageLabel("PRIVATE_PAGE_LABEL");
    auto* layer = page->getSelectedLayer();
    layer->setName("PRIVATE_LAYER_NAME");
    auto stroke = std::make_unique<Stroke>();
    stroke->setWidth(12);
    stroke->addPoint(Point(20, 100));
    stroke->addPoint(Point(180, 100));
    stroke->setAudioFilename(fs::path("PRIVATE_AUDIO_PATH.wav"));
    stroke->setErasures({{{100, 80}, {100, 120}, 15, {}}});
    layer->addElement(std::move(stroke));

    const auto teacher = Util::getTmpDirSubfolder() / "teacher-metadata.xoj";
    XojExportHandler full;
    full.setExportAudience(ExportAudience::Teacher);
    full.prepareSave(doc.get(), teacher);
    full.saveTo(teacher);
    ASSERT_TRUE(full.getErrorMessage().empty()) << full.getErrorMessage();
    const auto privateXml = readGz(teacher);
    ASSERT_NE(privateXml.find("utn-stroke-v1="), std::string::npos);
    ASSERT_NE(privateXml.find("PRIVATE_AUDIO_PATH"), std::string::npos);
    ASSERT_NE(privateXml.find("PRIVATE_PAGE_LABEL"), std::string::npos);
    ASSERT_NE(privateXml.find("PRIVATE_LAYER_NAME"), std::string::npos);

    const auto student = Util::getTmpDirSubfolder() / "student-metadata.xoj";
    XojExportHandler safe;
    safe.prepareSave(doc.get(), student);
    safe.saveTo(student);
    ASSERT_TRUE(safe.getErrorMessage().empty()) << safe.getErrorMessage();
    const auto xml = readGz(student);
    EXPECT_EQ(xml.find("utn-stroke-v1="), std::string::npos);
    EXPECT_EQ(xml.find("PRIVATE_AUDIO_PATH"), std::string::npos);
    EXPECT_EQ(xml.find("PRIVATE_PAGE_LABEL"), std::string::npos);
    EXPECT_EQ(xml.find("PRIVATE_LAYER_NAME"), std::string::npos);
    EXPECT_NE(xml.find("<image"), std::string::npos) << "the clipped raster remains visible";
    EXPECT_NE(xml.find("PUBLIC_LESSON"), std::string::npos);
    const auto imageBegin = xml.find("<image ");
    ASSERT_NE(imageBegin, std::string::npos);
    const auto dataBegin = xml.find('>', imageBegin) + 1;
    const auto dataEnd = xml.find("</image>", dataBegin);
    ASSERT_NE(dataEnd, std::string::npos);
    gsize pngLength = 0;
    auto* png = g_base64_decode(xml.substr(dataBegin, dataEnd - dataBegin).c_str(), &pngLength);
    auto* stream = g_memory_input_stream_new_from_data(png, pngLength, g_free);
    GError* error = nullptr;
    auto* pixels = gdk_pixbuf_new_from_stream(stream, nullptr, &error);
    g_object_unref(stream);
    ASSERT_NE(pixels, nullptr) << (error ? error->message : "invalid clipped PNG");
    ASSERT_TRUE(gdk_pixbuf_get_has_alpha(pixels));
    const auto* row = gdk_pixbuf_read_pixels(pixels) +
                      (gdk_pixbuf_get_height(pixels) / 2) * gdk_pixbuf_get_rowstride(pixels);
    EXPECT_EQ(row[(gdk_pixbuf_get_width(pixels) / 2) * 4 + 3], 0) << "erased centre stays transparent";
    EXPECT_GT(row[(gdk_pixbuf_get_width(pixels) / 4) * 4 + 3], 0) << "remaining ink survives";
    g_object_unref(pixels);
    g_clear_error(&error);
}

TEST(StudentXojExport, rejectsReferencedAndAttachedPdfBeforeWritingAnyStudentFileOrAssets) {
    for (bool attached: {false, true}) {
        SCOPED_TRACE(attached);
        const auto folder = Util::getTmpDirSubfolder(attached ? "attached-pdf-boundary" : "linked-pdf-boundary");
        const auto background = folder / "PRIVATE_PDF_PATH.pdf";
        auto* surface = cairo_pdf_surface_create(background.string().c_str(), 200, 200);
        cairo_pdf_surface_set_metadata(surface, CAIRO_PDF_METADATA_TITLE, "PRIVATE_PDF_METADATA");
        auto* cr = cairo_create(surface);
        cairo_move_to(cr, 20, 30);
        cairo_show_text(cr, "PUBLIC_PAGE");
        cairo_show_page(cr);
        cairo_move_to(cr, 20, 30);
        cairo_show_text(cr, "PRIVATE_UNUSED_PDF_PAGE");
        cairo_show_page(cr);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);

        DocumentHandler handler;
        Document doc(&handler);
        ASSERT_TRUE(doc.readPdf(background, true, attached)) << doc.getLastErrorMsg();
        doc.deletePage(1);
        ASSERT_EQ(doc.getPageCount(), 1U);
        doc.setFilepath(folder / "native.xopp");
        const auto teacher = folder / "teacher.xoj";
        XojExportHandler full;
        full.setExportAudience(ExportAudience::Teacher);
        full.prepareSave(&doc, teacher);
        full.saveTo(teacher);
        ASSERT_TRUE(full.getErrorMessage().empty()) << full.getErrorMessage();
        const auto xml = readGz(teacher);
        ASSERT_NE(xml.find(attached ? "bg.pdf" : "PRIVATE_PDF_PATH.pdf"), std::string::npos);
        const auto pdfAsset = attached ? folder / "native.xopp.bg.pdf" : background;
        auto uri = Util::toUri(pdfAsset);
        ASSERT_TRUE(uri);
        GError* error = nullptr;
        auto* pdf = poppler_document_new_from_file(uri->c_str(), nullptr, &error);
        ASSERT_NE(pdf, nullptr) << (error ? error->message : "missing PDF asset");
        EXPECT_EQ(poppler_document_get_n_pages(pdf), 2) << "the unused private source page survives";
        gchar* title = poppler_document_get_title(pdf);
        EXPECT_STREQ(title, "PRIVATE_PDF_METADATA");
        g_free(title);
        auto* privatePage = poppler_document_get_page(pdf, 1);
        gchar* privateText = poppler_page_get_text(privatePage);
        EXPECT_NE(std::string(privateText ? privateText : "").find("PRIVATE_UNUSED_PDF_PAGE"), std::string::npos);
        g_free(privateText);
        g_object_unref(privatePage);
        g_object_unref(pdf);
        g_clear_error(&error);
        if (attached) fs::remove(pdfAsset);

        const auto student = folder / "student.xoj";
        fs::remove(student);
        XojExportHandler safe;
        safe.prepareSave(&doc, student);
        safe.saveTo(student);
        EXPECT_FALSE(safe.getErrorMessage().empty()) << "cannot label the source PDF as student safe";
        EXPECT_FALSE(fs::exists(student));
        if (attached) {
            EXPECT_FALSE(fs::exists(pdfAsset));
        }
        EXPECT_FALSE(fs::exists(fs::path(student).concat(".bg.pdf")));
    }
}

TEST(StudentXojExport, rejectsLinkedAndAttachedImageBackgroundsBeforeWritingStudentAssets) {
    for (bool attached: {false, true}) {
        const auto folder = Util::getTmpDirSubfolder(attached ? "attached-image-boundary" : "linked-image-boundary");
        const auto source = folder / "PRIVATE_IMAGE_PATH.png";
        auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 20, 20);
        ASSERT_EQ(cairo_surface_write_to_png(surface, source.string().c_str()), CAIRO_STATUS_SUCCESS);
        cairo_surface_destroy(surface);
        BackgroundImage image;
        GError* error = nullptr;
        image.loadFile(source, &error);
        ASSERT_EQ(error, nullptr);
        image.setAttach(attached);
        auto doc = makeDocument({PUBLIC_LAYER});
        auto page = doc->getPage(0);
        page->setBackgroundType(PageType(PageTypeFormat::Image));
        page->setBackgroundImage(image);
        const auto student = folder / "student.xoj";
        const auto sidecar = fs::path(student).concat(".bg_1.png");
        fs::remove(student);
        fs::remove(sidecar);
        XojExportHandler safe;
        safe.prepareSave(doc.get(), student);
        safe.saveTo(student);
        EXPECT_FALSE(safe.getErrorMessage().empty());
        EXPECT_FALSE(fs::exists(student));
        EXPECT_FALSE(fs::exists(sidecar));
        EXPECT_TRUE(fs::exists(source));
    }
}

TEST(StudentXojExport, rejectsLatexSourceAndBinaryPayloadInsteadOfCopyingPrivateMetadata) {
    auto doc = makeDocument({PUBLIC_LAYER});
    auto tex = std::make_unique<TexImage>();
    tex->setText("visible formula % PRIVATE_LATEX_SOURCE_COMMENT");
    std::string png;
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 20, 20);
    const auto write = [](void* target, const unsigned char* bytes, unsigned length) {
        static_cast<std::string*>(target)->append(reinterpret_cast<const char*>(bytes), length);
        return CAIRO_STATUS_SUCCESS;
    };
    ASSERT_EQ(cairo_surface_write_to_png_stream(surface, write, &png), CAIRO_STATUS_SUCCESS);
    cairo_surface_destroy(surface);
    ASSERT_TRUE(tex->loadData(std::move(png)));
    doc->getPage(0)->getSelectedLayer()->addElement(std::move(tex));
    const auto teacher = Util::getTmpDirSubfolder() / "teacher-latex.xoj";
    XojExportHandler full;
    full.setExportAudience(ExportAudience::Teacher);
    full.prepareSave(doc.get(), teacher);
    full.saveTo(teacher);
    ASSERT_TRUE(full.getErrorMessage().empty());
    ASSERT_NE(readGz(teacher).find("PRIVATE_LATEX_SOURCE_COMMENT"), std::string::npos);

    const auto student = Util::getTmpDirSubfolder() / "student-latex.xoj";
    fs::remove(student);
    XojExportHandler safe;
    safe.prepareSave(doc.get(), student);
    safe.saveTo(student);
    EXPECT_FALSE(safe.getErrorMessage().empty());
    EXPECT_FALSE(fs::exists(student));
}

TEST(StudentXojExport, embeddedImageContainsRenderedPixelsWithoutSourcePngMetadata) {
    auto* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, true, 8, 20, 20);
    gdk_pixbuf_fill(pixbuf, 0x123456ff);
    gchar* source = nullptr;
    gsize length = 0;
    GError* error = nullptr;
    ASSERT_TRUE(gdk_pixbuf_save_to_buffer(pixbuf, &source, &length, "png", &error,
                                        "tEXt::Comment", "PRIVATE_PNG_COMMENT", nullptr));
    g_object_unref(pixbuf);
    ASSERT_EQ(error, nullptr);
    ASSERT_NE(std::string(source, length).find("PRIVATE_PNG_COMMENT"), std::string::npos);
    auto doc = makeDocument({PUBLIC_LAYER});
    auto image = std::make_unique<Image>();
    image->setImage(std::string(source, length));
    g_free(source);
    doc->getPage(0)->getSelectedLayer()->addElement(std::move(image));
    const auto output = Util::getTmpDirSubfolder() / "student-image-pixels.xoj";
    XojExportHandler exporter;
    exporter.prepareSave(doc.get(), output);
    exporter.saveTo(output);
    ASSERT_TRUE(exporter.getErrorMessage().empty()) << exporter.getErrorMessage();
    const auto xml = readGz(output);
    const auto begin = xml.find("<image ");
    ASSERT_NE(begin, std::string::npos);
    const auto data = xml.find('>', begin) + 1;
    const auto end = xml.find("</image>", data);
    ASSERT_NE(end, std::string::npos);
    gsize decodedLength = 0;
    auto* decoded = g_base64_decode(xml.substr(data, end - data).c_str(), &decodedLength);
    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(decoded), decodedLength).find("PRIVATE_PNG_COMMENT"),
              std::string::npos);
    auto* stream = g_memory_input_stream_new_from_data(decoded, decodedLength, g_free);
    auto* exportedPixels = gdk_pixbuf_new_from_stream(stream, nullptr, &error);
    g_object_unref(stream);
    ASSERT_NE(exportedPixels, nullptr) << (error ? error->message : "invalid exported PNG");
    EXPECT_EQ(gdk_pixbuf_get_width(exportedPixels), 20);
    EXPECT_EQ(gdk_pixbuf_get_height(exportedPixels), 20);
    const auto* pixel = gdk_pixbuf_read_pixels(exportedPixels);
    EXPECT_EQ(pixel[0], 0x12);
    EXPECT_EQ(pixel[1], 0x34);
    EXPECT_EQ(pixel[2], 0x56);
    g_object_unref(exportedPixels);
    g_clear_error(&error);
}
