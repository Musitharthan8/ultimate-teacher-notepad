#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "model/Document.h"
#include "model/Layer.h"
#include "model/Text.h"
#include "model/XojPage.h"
#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "util/PathUtil.h"
#include "undo/TextBoxUndoAction.h"

TEST(TextReliability, UndoRedoPreservesTextStackingAndFormatting) {
    Document document(nullptr);
    auto page = std::make_shared<XojPage>(200, 200);
    auto* layer = page->getSelectedLayer();
    auto original = std::make_unique<Text>();
    const std::string originalText = "Original: caf\xC3\xA9 / \xE4\xB8\xAD\xE6\x96\x87";
    original->setText(originalText);
    original->setWrap(85.0);
    original->setAlignment(TextAlignment::RIGHT);
    original->setUnderlined(true);
    original->setLineSpacing(1.5);
    auto* originalPtr = original.get();
    auto edited = original->cloneText();
    edited->setText("Edited");
    edited->setStrikethrough(true);
    auto* editedPtr = edited.get();
    layer->addElement(std::move(edited));
    auto foreground = std::make_unique<Text>();
    foreground->setText("Foreground annotation");
    auto* foregroundPtr = foreground.get();
    layer->addElement(std::move(foreground));

    TextBoxUndoAction action(page, layer, editedPtr, std::move(original), &document);
    ASSERT_TRUE(action.undo(nullptr));
    ASSERT_EQ(layer->getElements().size(), 2U);
    EXPECT_EQ(layer->getElements()[0].get(), originalPtr);
    EXPECT_EQ(layer->getElements()[1].get(), foregroundPtr);
    EXPECT_EQ(originalPtr->getText(), originalText);
    EXPECT_DOUBLE_EQ(originalPtr->getWrap(), 85.0);
    EXPECT_EQ(originalPtr->getAlign(), TextAlignment::RIGHT);
    EXPECT_TRUE(originalPtr->isUnderlined());
    EXPECT_FALSE(originalPtr->isStrikethrough());
    EXPECT_DOUBLE_EQ(originalPtr->getLineSpacing(), 1.5);

    ASSERT_TRUE(action.redo(nullptr));
    EXPECT_EQ(layer->getElements()[0].get(), editedPtr);
    EXPECT_EQ(layer->getElements()[1].get(), foregroundPtr);
    EXPECT_EQ(editedPtr->getText(), "Edited");
    EXPECT_DOUBLE_EQ(editedPtr->getWrap(), 85.0);
    EXPECT_EQ(editedPtr->getAlign(), TextAlignment::RIGHT);
    EXPECT_TRUE(editedPtr->isUnderlined());
    EXPECT_TRUE(editedPtr->isStrikethrough());
    EXPECT_DOUBLE_EQ(editedPtr->getLineSpacing(), 1.5);
}

TEST(TextReliability, ScaledAnswerBoxAndWrappedTextKeepLocalGeometryAfterReload) {
    for (bool boxed: {false, true}) {
        Document document(nullptr);
        auto page = std::make_shared<XojPage>(600, 800);
        document.addPage(page);
        auto text = std::make_unique<Text>();
        text->setText("A long numbered answer that must wrap onto more than one line");
        text->setBoxEnabled(boxed);
        text->setWrap(90);
        text->setBoxPadding(8);
        text->setBoxBorderWidth(2);
        text->setBoxCornerRadius(6);
        text->setTransformation(xoj::util::Matrix::TRANSLATION(50, 80).scale(1.5, 1.5));
        const auto matrix = text->getTransformation();
        const auto bounds = text->getBoundingBox();
        const auto content = text->getText();
        page->getSelectedLayer()->addElement(std::move(text));
        const auto file = Util::getTmpDirSubfolder() / (boxed ? "scaled-answer.xopp" : "scaled-wrapped.xopp");
        SaveHandler saver;
        saver.prepareSave(&document, file);
        saver.saveTo(file);
        ASSERT_TRUE(saver.getErrorMessage().empty()) << saver.getErrorMessage();
        auto loaded = LoadHandler{}.loadDocument(file);
        ASSERT_NE(loaded, nullptr);
        auto* reopened = dynamic_cast<Text*>(loaded->getPage(0)->getSelectedLayer()->getElements().front().get());
        ASSERT_NE(reopened, nullptr);
        EXPECT_EQ(reopened->getText(), content);
        EXPECT_EQ(reopened->getTransformation(), matrix);
        EXPECT_DOUBLE_EQ(reopened->getWrap(), 90);
        EXPECT_EQ(reopened->isBoxEnabled(), boxed);
        if (boxed) {
            EXPECT_DOUBLE_EQ(reopened->getBoxPadding(), 8);
            EXPECT_DOUBLE_EQ(reopened->getBoxBorderWidth(), 2);
            EXPECT_DOUBLE_EQ(reopened->getBoxCornerRadius(), 6);
        }
        EXPECT_NEAR(reopened->getBoundingBox().x, bounds.x, 0.001);
        EXPECT_NEAR(reopened->getBoundingBox().y, bounds.y, 0.001);
        EXPECT_NEAR(reopened->getBoundingBox().width, bounds.width, 0.001);
        EXPECT_NEAR(reopened->getBoundingBox().height, bounds.height, 0.001);
    }
}
