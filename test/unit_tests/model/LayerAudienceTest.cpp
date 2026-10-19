/*
 * Ultimate Teacher Notepad
 *
 * Privacy rules for Student View and Hide & Reveal, and their persistence in XOPP files.
 * Based on Xournal++ GPLv2+.
 */
#include <memory>
#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "model/Document.h"
#include "model/Layer.h"
#include "model/LayerAudience.h"
#include "model/Stroke.h"
#include "model/XojPage.h"
#include "util/PathUtil.h"
#include "view/DocumentView.h"

#include "filesystem.h"

namespace {
/// Layers are normally added through LayerController; tests add them directly
struct TestPage: XojPage {
    TestPage(): XojPage(200, 200, /*suppressLayerCreation=*/true) {}
    using XojPage::addLayer;
};

std::unique_ptr<Layer> makeLayer(LayerAudience audience, bool visible, const std::string& name = {}) {
    auto layer = std::make_unique<Layer>();
    layer->setAudience(audience);
    layer->setVisible(visible);
    if (!name.empty()) {
        layer->setName(name);
    }
    return layer;
}
}  // namespace

TEST(LayerAudience, studentsSeeOnlyOrdinaryAndRevealedLayers) {
    EXPECT_TRUE(utn::visibleToStudents(*makeLayer(LayerAudience::Everyone, true)));
    EXPECT_FALSE(utn::visibleToStudents(*makeLayer(LayerAudience::Everyone, false)));

    // Revealing answers (making the layer visible) must show them to students: this was broken in 0.2
    EXPECT_TRUE(utn::visibleToStudents(*makeLayer(LayerAudience::Answers, true)));
    EXPECT_FALSE(utn::visibleToStudents(*makeLayer(LayerAudience::Answers, false)));

    // Teacher-only notes never reach students, whatever their visibility
    EXPECT_FALSE(utn::visibleToStudents(*makeLayer(LayerAudience::TeacherOnly, true)));
    EXPECT_FALSE(utn::visibleToStudents(*makeLayer(LayerAudience::TeacherOnly, false)));
}

TEST(LayerAudience, privacyDoesNotDependOnTheName) {
    // Renaming an answers layer must not expose it
    auto renamed = makeLayer(LayerAudience::Answers, false, "Homework");
    EXPECT_FALSE(utn::visibleToStudents(*renamed));
    EXPECT_TRUE(utn::isHiddenAnswers(*renamed));

    // A layer named like an old answers layer is ordinary once its audience is Everyone
    auto ordinary = makeLayer(LayerAudience::Everyone, true, "UTN Reveal");
    EXPECT_TRUE(utn::visibleToStudents(*ordinary));
}

TEST(LayerAudience, hiddenAnswersAreOnlyUnrevealedAnswers) {
    EXPECT_TRUE(utn::isHiddenAnswers(*makeLayer(LayerAudience::Answers, false)));
    EXPECT_FALSE(utn::isHiddenAnswers(*makeLayer(LayerAudience::Answers, true)));
    EXPECT_FALSE(utn::isHiddenAnswers(*makeLayer(LayerAudience::Everyone, false)));
    EXPECT_FALSE(utn::isHiddenAnswers(*makeLayer(LayerAudience::TeacherOnly, false)));
}

TEST(LayerAudience, attributeValuesRoundTrip) {
    for (auto audience: {LayerAudience::Everyone, LayerAudience::TeacherOnly, LayerAudience::Answers}) {
        EXPECT_EQ(utn::audienceFromString(utn::audienceToString(audience)), audience);
    }
    EXPECT_FALSE(utn::audienceFromString("future-value").has_value());
}

TEST(LayerAudience, legacyNamesMigrate) {
    EXPECT_EQ(utn::audienceFromLegacyName("UTN Reveal"), LayerAudience::Answers);
    EXPECT_EQ(utn::audienceFromLegacyName("UTN Reveal 2"), LayerAudience::Answers);
    EXPECT_EQ(utn::audienceFromLegacyName("UTN Teacher notes"), LayerAudience::TeacherOnly);
    EXPECT_EQ(utn::audienceFromLegacyName("Layer 1"), LayerAudience::Everyone);
    EXPECT_EQ(utn::audienceFromLegacyName(""), LayerAudience::Everyone);
}

TEST(LayerAudience, loadedAnswersStartHidden) {
    // A 0.2 file: no attribute, old name, saved while revealed
    Layer legacy;
    legacy.setName("UTN Reveal");
    utn::prepareLoadedLayer(legacy, std::nullopt);
    EXPECT_EQ(legacy.getAudience(), LayerAudience::Answers);
    EXPECT_FALSE(legacy.isVisible());

    // The saved attribute wins over the name
    Layer saved;
    saved.setName("UTN Reveal");
    utn::prepareLoadedLayer(saved, LayerAudience::Everyone);
    EXPECT_EQ(saved.getAudience(), LayerAudience::Everyone);
    EXPECT_TRUE(saved.isVisible());

    Layer teacher;
    utn::prepareLoadedLayer(teacher, LayerAudience::TeacherOnly);
    EXPECT_EQ(teacher.getAudience(), LayerAudience::TeacherOnly);
    EXPECT_TRUE(teacher.isVisible());
}

TEST(LayerAudience, cloneKeepsAudience) {
    auto layer = makeLayer(LayerAudience::Answers, true, "Answers");
    std::unique_ptr<Layer> copy(layer->clone());
    EXPECT_EQ(copy->getAudience(), LayerAudience::Answers);
}

TEST(LayerAudience, audienceSurvivesSaveAndReload) {
    Document doc(nullptr);
    auto page = std::make_shared<TestPage>();
    page->addLayer(makeLayer(LayerAudience::Everyone, true, "Worksheet").release());
    page->addLayer(makeLayer(LayerAudience::Answers, true, "Answers").release());  // revealed when saved
    page->addLayer(makeLayer(LayerAudience::TeacherOnly, true, "Notes").release());
    doc.addPage(page);

    const fs::path file = Util::getTmpDirSubfolder() / "utn-audience.xopp";
    {
        SaveHandler saver;
        saver.prepareSave(&doc, file);
        saver.saveTo(file);
    }
    auto reloaded = LoadHandler{}.loadDocument(file);
    fs::remove(file);
    ASSERT_TRUE(reloaded);
    ASSERT_EQ(reloaded->getPageCount(), 1U);
    const auto& layers = reloaded->getPage(0)->getLayers();
    ASSERT_EQ(layers.size(), 3U);

    EXPECT_EQ(layers.at(0)->getAudience(), LayerAudience::Everyone);
    EXPECT_TRUE(layers.at(0)->isVisible());
    EXPECT_EQ(layers.at(1)->getAudience(), LayerAudience::Answers);
    EXPECT_FALSE(layers.at(1)->isVisible()) << "Answers must be hidden when a lesson is opened";
    EXPECT_EQ(layers.at(2)->getAudience(), LayerAudience::TeacherOnly);
}

TEST(LayerAudience, writableLayerAfterHideSkipsHiddenAnswers) {
    // Answers on top and in the middle: the pen must land on the topmost ordinary layer, layer 1 here
    auto ordinary = makeLayer(LayerAudience::Everyone, true);
    auto answersA = makeLayer(LayerAudience::Answers, false);
    auto answersB = makeLayer(LayerAudience::Answers, false);
    std::vector<Layer*> layers{ordinary.get(), answersA.get(), answersB.get()};
    auto id = utn::writableLayerIdAfterHide(layers);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(*id, 1u);
}

TEST(LayerAudience, writableLayerAfterHidePrefersTopmostOrdinaryLayer) {
    // Answers at the bottom, ordinary in the middle, answers on top: the pen goes to layer 2
    auto answersBottom = makeLayer(LayerAudience::Answers, false);
    auto ordinary = makeLayer(LayerAudience::Everyone, true);
    auto answersTop = makeLayer(LayerAudience::Answers, false);
    std::vector<Layer*> layers{answersBottom.get(), ordinary.get(), answersTop.get()};
    auto id = utn::writableLayerIdAfterHide(layers);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(*id, 2u);
}

TEST(LayerAudience, writableLayerAfterHideIgnoresTeacherOnlyAsAnswers) {
    // Teacher-only notes are not answers: the pen may stay on them
    auto answers = makeLayer(LayerAudience::Answers, false);
    auto teacher = makeLayer(LayerAudience::TeacherOnly, true);
    std::vector<Layer*> layers{answers.get(), teacher.get()};
    auto id = utn::writableLayerIdAfterHide(layers);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(*id, 2u);
}

TEST(LayerAudience, writableLayerAfterHideNoneWhenOnlyAnswersExist) {
    // Nothing safe to write on: the caller must not pretend a layer is writable
    auto answers = makeLayer(LayerAudience::Answers, false);
    std::vector<Layer*> layers{answers.get()};
    EXPECT_FALSE(utn::writableLayerIdAfterHide(layers).has_value());
    EXPECT_FALSE(utn::writableLayerIdAfterHide({}).has_value());
}

TEST(LayerAudience, inkStaysOnAVisibleSelectedLayer) {
    // A visible layer, revealed answers included, keeps the ink: nothing to correct
    auto ordinary = makeLayer(LayerAudience::Everyone, true);
    auto revealed = makeLayer(LayerAudience::Answers, true);
    std::vector<Layer*> layers{ordinary.get(), revealed.get()};
    EXPECT_FALSE(utn::visibleLayerIdForInk(layers, 1).has_value());
    EXPECT_FALSE(utn::visibleLayerIdForInk(layers, 2).has_value());
    EXPECT_FALSE(utn::visibleLayerIdForInk(layers, 0).has_value()) << "ID 0 draws on the bottom layer";
}

TEST(LayerAudience, inkLeavesHiddenAnswersAfterReopening) {
    // The reported case: a reopened lesson selected its top layer, the hidden answers, so new ink was invisible
    auto ordinary = makeLayer(LayerAudience::Everyone, true);
    auto answers = makeLayer(LayerAudience::Answers, false);
    std::vector<Layer*> layers{ordinary.get(), answers.get()};
    auto id = utn::visibleLayerIdForInk(layers, 2);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(*id, 1u);
}

TEST(LayerAudience, inkSkipsHiddenAndRevealedAnswersForTopmostVisibleOrdinary) {
    // Hidden ordinary layer on top of a visible one; revealed answers are never chosen for ordinary ink
    auto visible = makeLayer(LayerAudience::Everyone, true);
    auto revealed = makeLayer(LayerAudience::Answers, true);
    auto teacher = makeLayer(LayerAudience::TeacherOnly, true);
    auto hidden = makeLayer(LayerAudience::Everyone, false);
    std::vector<Layer*> layers{visible.get(), revealed.get(), teacher.get(), hidden.get()};
    auto id = utn::visibleLayerIdForInk(layers, 4);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(*id, 3u);
}

TEST(LayerAudience, inkHasNoVisibleTargetWhenEverythingIsHidden) {
    // The caller then shows the layer or starts a new one; it must not be told a hidden layer is fine
    auto hidden = makeLayer(LayerAudience::Everyone, false);
    auto answers = makeLayer(LayerAudience::Answers, false);
    std::vector<Layer*> layers{hidden.get(), answers.get()};
    EXPECT_FALSE(utn::visibleLayerIdForInk(layers, 2).has_value());
    EXPECT_FALSE(utn::visibleLayerIdForInk({}, 0).has_value());
}

namespace {
void addPrivacyMarker(Layer& layer, double y) {
    auto stroke = std::make_unique<Stroke>();
    stroke->setWidth(8);
    stroke->setColor(Color{0, 0, 0});
    stroke->addPoint(Point(20, y));
    stroke->addPoint(Point(180, y));
    layer.addElement(std::move(stroke));
}

// Exercise the actual DocumentView path used by Student View, including the
// teacher ghost setting. Merely checking visibleToStudents cannot catch a
// renderer accidentally drawing a rejected layer through its ghost fallback.
uint32_t studentPixel(const PageRef& page, int y, double ghostOpacity = 0.4) {
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 200, 200);
    auto* cr = cairo_create(surface);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_paint(cr);
    DocumentView view;
    view.setHiddenAnswersOpacity(ghostOpacity);
    view.setLayerFilter(utn::visibleToStudents);
    view.drawPage(page, cr, true,
                  {xoj::view::HIDE_PDF_BACKGROUND, xoj::view::HIDE_IMAGE_BACKGROUND,
                   xoj::view::HIDE_RULING_BACKGROUND, xoj::view::DONT_FORCE_BACKGROUND_COLOR,
                   xoj::view::FORCE_VISIBLE});
    cairo_surface_flush(surface);
    const auto* row = reinterpret_cast<const uint32_t*>(
            cairo_image_surface_get_data(surface) + y * cairo_image_surface_get_stride(surface));
    const auto pixel = row[100];
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return pixel;
}
}  // namespace

TEST(LayerAudience, studentRendererNeverGhostsHiddenAnswersOrTeacherNotes) {
    auto page = std::make_shared<TestPage>();
    auto ordinary = makeLayer(LayerAudience::Everyone, true);
    auto answers = makeLayer(LayerAudience::Answers, false, "Renamed answer key");
    auto teacher = makeLayer(LayerAudience::TeacherOnly, true);
    addPrivacyMarker(*ordinary, 30);
    addPrivacyMarker(*answers, 80);
    addPrivacyMarker(*teacher, 130);
    auto* answerLayer = answers.get();
    page->addLayer(ordinary.release());
    page->addLayer(answers.release());
    page->addLayer(teacher.release());

    for (int cycle = 0; cycle < 20; ++cycle) {
        SCOPED_TRACE(cycle);
        answerLayer->setVisible(false);
        EXPECT_EQ(studentPixel(page, 30), 0xff000000U);
        EXPECT_EQ(studentPixel(page, 80), 0xffffffffU);
        EXPECT_EQ(studentPixel(page, 130), 0xffffffffU);
        answerLayer->setVisible(true);
        EXPECT_EQ(studentPixel(page, 80), 0xff000000U);
        EXPECT_EQ(studentPixel(page, 130), 0xffffffffU);
    }
}

TEST(LayerAudience, studentRendererKeepsSavedRevealedAnswersHiddenAfterReload) {
    Document doc(nullptr);
    auto page = std::make_shared<TestPage>();
    auto answers = makeLayer(LayerAudience::Answers, true, "Not a legacy name");
    auto teacher = makeLayer(LayerAudience::TeacherOnly, true);
    addPrivacyMarker(*answers, 80);
    addPrivacyMarker(*teacher, 130);
    page->addLayer(answers.release());
    page->addLayer(teacher.release());
    doc.addPage(page);
    const auto file = Util::getTmpDirSubfolder() / "utn-audience-render.xopp";
    SaveHandler saver;
    saver.prepareSave(&doc, file);
    saver.saveTo(file);
    ASSERT_TRUE(saver.getErrorMessage().empty()) << saver.getErrorMessage();
    auto reopened = LoadHandler{}.loadDocument(file);
    fs::remove(file);
    ASSERT_TRUE(reopened);
    EXPECT_EQ(studentPixel(reopened->getPage(0), 80), 0xffffffffU);
    EXPECT_EQ(studentPixel(reopened->getPage(0), 130), 0xffffffffU);
}
