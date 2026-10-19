#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "model/Document.h"
#include "model/Layer.h"
#include "model/Text.h"
#include "model/XojPage.h"
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
