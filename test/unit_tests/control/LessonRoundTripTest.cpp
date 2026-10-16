/*
 * Ultimate Teacher Notepad
 *
 * Release gate: a lesson with ink, highlight, text, an Answer Box, a Feedback comment, a picture and a hidden answers
 * layer survives save and reopen, and the answers stay hidden after every reopen. The same fixture is exported by
 * the packaged UTN.exe in the release checks.
 *
 * Based on Xournal++ GPLv2+
 */

#include <memory>
#include <string>
#include <vector>

#include <config-test.h>
#include <gtest/gtest.h>

#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "model/Document.h"
#include "model/Element.h"
#include "model/Layer.h"
#include "model/Stroke.h"
#include "model/Text.h"
#include "model/XojPage.h"
#include "util/PathUtil.h"

#include "filesystem.h"

namespace {

fs::path fixture() {
    // PROJECT_SOURCE_DIR is a u8 literal (char8_t in C++20)
    return fs::path(std::u8string(PROJECT_SOURCE_DIR) + u8"/test/files/utn/lesson-roundtrip.xopp");
}

std::vector<const Element*> elementsOf(const Layer& layer) {
    std::vector<const Element*> elements;
    for (const auto& e: const_cast<Layer&>(layer).getElements()) {
        elements.push_back(e.get());
    }
    return elements;
}

const Text* textWith(const std::vector<const Element*>& elements, const std::string& content) {
    for (const auto* e: elements) {
        if (e->getType() == ELEMENT_TEXT && static_cast<const Text*>(e)->getText() == content) {
            return static_cast<const Text*>(e);
        }
    }
    return nullptr;
}

void expectLesson(Document& doc) {
    ASSERT_EQ(doc.getPageCount(), 2U);
    auto page = doc.getPage(0);
    ASSERT_EQ(page->getLayerCount(), 2U);
    const Layer& lesson = *page->getLayers()[0];
    const Layer& answers = *page->getLayers()[1];

    const auto items = elementsOf(lesson);
    ASSERT_EQ(items.size(), 6U);
    EXPECT_EQ(items[0]->getType(), ELEMENT_STROKE);
    EXPECT_TRUE(static_cast<const Stroke*>(items[1])->getToolType() == StrokeTool::HIGHLIGHTER);
    EXPECT_NE(textWith(items, "PLAIN TEXT LINE"), nullptr);
    EXPECT_EQ(items[5]->getType(), ELEMENT_IMAGE);

    const Text* box = textWith(items, "ANSWER BOX CONTENT");
    ASSERT_NE(box, nullptr);
    EXPECT_TRUE(box->isBoxEnabled());
    EXPECT_EQ(box->getBoxBackgroundColor(), (Color{0xFFU, 0xF8U, 0xD6U, 0xE6U}));
    EXPECT_DOUBLE_EQ(box->getBoxBorderWidth(), 1.5);
    EXPECT_DOUBLE_EQ(box->getBoxCornerRadius(), 5.0);

    const Text* feedback = textWith(items, "Good");
    ASSERT_NE(feedback, nullptr);
    EXPECT_EQ(feedback->getColor(), (Color{0xD3U, 0x2FU, 0x2FU}));
    EXPECT_FALSE(feedback->isStrikethrough());
    EXPECT_FALSE(feedback->isUnderlined());

    EXPECT_EQ(answers.getAudience(), LayerAudience::Answers);
    EXPECT_FALSE(answers.isVisible()) << "opening a lesson must never show its answers";
    // Visual pass 16 Oct: the hidden answers layer was left selected, so new ink was invisible after reopening
    EXPECT_EQ(page->getSelectedLayer(), &lesson) << "new ink must go to a visible layer after opening";
    EXPECT_NE(textWith(elementsOf(answers), "HIDDEN ANSWER 42"), nullptr);
}

}  // namespace

TEST(LessonRoundTrip, lessonSurvivesSaveAndReopenWithAnswersHidden) {
    auto original = LoadHandler{}.loadDocument(fixture());
    ASSERT_TRUE(original);
    expectLesson(*original);

    const auto saved = Util::getTmpDirSubfolder() / "utn-lesson-roundtrip.xopp";
    SaveHandler saver;
    saver.prepareSave(original.get(), saved);
    saver.saveTo(saved);
    ASSERT_TRUE(saver.getErrorMessage().empty()) << saver.getErrorMessage();

    auto reopened = LoadHandler{}.loadDocument(saved);
    ASSERT_TRUE(reopened);
    expectLesson(*reopened);

    // A second save of the reopened lesson must not lose or reveal anything either
    const auto savedAgain = Util::getTmpDirSubfolder() / "utn-lesson-roundtrip-2.xopp";
    SaveHandler again;
    again.prepareSave(reopened.get(), savedAgain);
    again.saveTo(savedAgain);
    auto reopenedAgain = LoadHandler{}.loadDocument(savedAgain);
    ASSERT_TRUE(reopenedAgain);
    expectLesson(*reopenedAgain);
}
