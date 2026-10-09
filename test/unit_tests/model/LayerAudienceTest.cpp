/*
 * Ultimate Teacher Notepad
 *
 * Privacy rules for Student View and Hide & Reveal, and their persistence in XOPP files.
 * Based on Xournal++ GPLv2+.
 */
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "model/Document.h"
#include "model/Layer.h"
#include "model/LayerAudience.h"
#include "model/XojPage.h"
#include "util/PathUtil.h"

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
