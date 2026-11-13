/*
 * Ultimate Teacher Notepad
 *
 * Text background colour: the model, drawing, clipboard and save/reopen of a coloured box behind plain text.
 * (Undo of a background change on selected text needs a Control and is covered by the GTK tests.)
 *
 * Based on Xournal++ GPLv2+
 */

#include <memory>
#include <string>

#include <cairo.h>
#include <gtest/gtest.h>

#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "model/Document.h"
#include "model/Layer.h"
#include "model/Text.h"
#include "model/XojPage.h"
#include "util/Color.h"
#include "util/PathUtil.h"
#include "util/serializing/BinObjectEncoding.h"
#include "util/serializing/ObjectInputStream.h"
#include "util/serializing/ObjectOutputStream.h"
#include "view/TextBoxRendering.h"

namespace {
const Color YELLOW{255U, 235U, 59U, 255U};
const Color TRANSPARENT_{0U, 0U, 0U, 0U};

std::unique_ptr<Text> sampleText() {
    auto text = std::make_unique<Text>();
    text->setText("Good attempt! Say how he felt.");
    text->setColor(Color{200U, 30U, 30U});
    text->setTransformation(xoj::util::Matrix::TRANSLATION(50, 80));
    return text;
}
}  // namespace

TEST(TextBackground, defaultsToTransparentAndIsNotAnAnswerBox) {
    auto text = sampleText();
    EXPECT_FALSE(text->hasBackground());
    EXPECT_EQ(text->getBackgroundColor().alpha, 0U);
    EXPECT_FALSE(text->isBoxEnabled()) << "a background is not an Answer Box";
}

TEST(TextBackground, textColourAndBackgroundAreIndependent) {
    auto text = sampleText();
    text->setBackgroundColor(YELLOW);
    EXPECT_EQ(text->getColor(), (Color{200U, 30U, 30U})) << "red text stays red";
    EXPECT_EQ(text->getBackgroundColor(), YELLOW);
    text->setColor(Color{255U, 255U, 255U});
    EXPECT_EQ(text->getBackgroundColor(), YELLOW) << "changing the text colour keeps the background";
    text->setBackgroundColor(TRANSPARENT_);
    EXPECT_EQ(text->getColor(), (Color{255U, 255U, 255U})) << "and the other way round";
    EXPECT_FALSE(text->hasBackground());
}

TEST(TextBackground, theBackgroundHasPaddingAndIsPartOfTheBoundsSoMovingAndRepaintingCoverIt) {
    auto plain = sampleText();
    const auto plainBounds = plain->getBoundingBox();
    auto coloured = sampleText();
    coloured->setBackgroundColor(YELLOW);
    const auto bounds = coloured->getBoundingBox();
    EXPECT_NEAR(bounds.x, plainBounds.x - Text::BACKGROUND_PADDING, 0.01);
    EXPECT_NEAR(bounds.y, plainBounds.y - Text::BACKGROUND_PADDING, 0.01);
    EXPECT_NEAR(bounds.width, plainBounds.width + 2 * Text::BACKGROUND_PADDING, 0.01);
    EXPECT_NEAR(bounds.height, plainBounds.height + 2 * Text::BACKGROUND_PADDING, 0.01);

    // Going back to transparent gives the old bounds back
    coloured->setBackgroundColor(TRANSPARENT_);
    EXPECT_NEAR(coloured->getBoundingBox().width, plainBounds.width, 0.01);

    // It follows the text when the text moves or is scaled, because it lives in the text's own coordinates
    coloured->setBackgroundColor(YELLOW);
    coloured->setTransformation(xoj::util::Matrix::TRANSLATION(120, 200).scale(2.0, 2.0));
    const auto moved = coloured->getBoundingBox();
    EXPECT_NEAR(moved.width, (plainBounds.width + 2 * Text::BACKGROUND_PADDING) * 2.0, 0.05);
}

TEST(TextBackground, wrappingTheTextResizesTheBackgroundWithIt) {
    auto text = sampleText();
    text->setBackgroundColor(YELLOW);
    const auto wide = text->getBackgroundBounds();
    text->setWrap(60);
    const auto narrow = text->getBackgroundBounds();
    EXPECT_LT(narrow.width, wide.width);
    EXPECT_GT(narrow.height, wide.height) << "wrapped onto more lines, so taller: the background grew to match";
}

TEST(TextBackground, cloneKeepsTheBackgroundSoEditingAndUndoDoNotLoseIt) {
    auto text = sampleText();
    text->setBackgroundColor(YELLOW);
    auto copy = text->cloneText();
    EXPECT_EQ(copy->getBackgroundColor(), YELLOW);
    EXPECT_EQ(copy->getText(), text->getText());
    EXPECT_EQ(copy->getColor(), text->getColor());
}

TEST(TextBackground, clipboardFormatCarriesTheBackgroundAndPlainTextReadsBackTransparent) {
    auto text = sampleText();
    text->setBackgroundColor(YELLOW);
    ObjectOutputStream out(new BinObjectEncoding);
    text->serialize(out);
    auto* gstr = out.stealData();
    std::string data(gstr->str, gstr->len);
    g_string_free(gstr, true);
    ObjectInputStream in;
    ASSERT_TRUE(in.read(&data[0], data.size() + 1));
    Text pasted;
    pasted.readSerialized(in);
    EXPECT_EQ(pasted.getBackgroundColor(), YELLOW);
    EXPECT_EQ(pasted.getText(), text->getText());

    auto plain = sampleText();
    ObjectOutputStream plainOut(new BinObjectEncoding);
    plain->serialize(plainOut);
    auto* plainG = plainOut.stealData();
    std::string plainData(plainG->str, plainG->len);
    g_string_free(plainG, true);
    ObjectInputStream plainIn;
    ASSERT_TRUE(plainIn.read(&plainData[0], plainData.size() + 1));
    Text plainPasted;
    plainPasted.setBackgroundColor(YELLOW);  // must be reset by the read
    plainPasted.readSerialized(plainIn);
    EXPECT_FALSE(plainPasted.hasBackground());
}

TEST(TextBackground, drawingPaintsOnlyTheTextBoxAndNothingWhenTransparent) {
    auto text = sampleText();
    text->setText(" ");  // no visible glyphs: only the background can colour pixels
    text->setBackgroundColor(Color{0U, 128U, 255U, 255U});
    const auto bounds = text->getBackgroundBounds();
    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 300, 200);
    cairo_t* cr = cairo_create(surface);
    const auto pixelAt = [&](double x, double y) {
        cairo_surface_flush(surface);
        const auto* row = cairo_image_surface_get_data(surface) +
                          static_cast<int>(y) * cairo_image_surface_get_stride(surface);
        return reinterpret_cast<const uint32_t*>(row)[static_cast<int>(x)] & 0xFFFFFFU;
    };
    const auto paint = [&] {
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_paint(cr);
        cairo_save(cr);
        text->getTransformation().transformCairo(cr);
        xoj::view::drawTextBackground(cr, text.get(), text->getBackgroundBounds());
        cairo_restore(cr);
    };

    paint();
    EXPECT_EQ(pixelAt(50 + bounds.x + bounds.width / 2, 80 + bounds.y + bounds.height / 2), 0x0080FFU) << "inside";
    EXPECT_EQ(pixelAt(50 + bounds.x + bounds.width + 6, 80 + bounds.y + bounds.height / 2), 0xFFFFFFU)
            << "outside the text box the page is untouched: only the box is coloured, not the page";

    text->setBackgroundColor(TRANSPARENT_);
    paint();
    EXPECT_EQ(pixelAt(50 + bounds.x + bounds.width / 2, 80 + bounds.y + bounds.height / 2), 0xFFFFFFU)
            << "transparent draws nothing";
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

TEST(TextBackground, survivesSaveAndReopenIncludingScaledAndWrappedText) {
    Document document(nullptr);
    auto page = std::make_shared<XojPage>(600, 800);
    document.addPage(page);

    auto plain = sampleText();
    plain->setText("No background");
    plain->setTransformation(xoj::util::Matrix::TRANSLATION(40, 40));
    page->getSelectedLayer()->addElement(std::move(plain));

    auto coloured = sampleText();
    coloured->setText("A long feedback comment that wraps onto several lines of the script");
    coloured->setWrap(120);
    coloured->setBackgroundColor(Color{255U, 235U, 59U, 200U});
    coloured->setTransformation(xoj::util::Matrix::TRANSLATION(60, 200).scale(1.5, 1.5));
    const auto matrix = coloured->getTransformation();
    const auto bounds = coloured->getBoundingBox();
    page->getSelectedLayer()->addElement(std::move(coloured));

    const auto file = Util::getTmpDirSubfolder() / "text-background.xopp";
    SaveHandler saver;
    saver.prepareSave(&document, file);
    saver.saveTo(file);
    ASSERT_TRUE(saver.getErrorMessage().empty()) << saver.getErrorMessage();

    auto loaded = LoadHandler{}.loadDocument(file);
    ASSERT_NE(loaded, nullptr);
    const auto& elements = loaded->getPage(0)->getSelectedLayer()->getElements();
    ASSERT_EQ(elements.size(), 2U);
    auto* reopenedPlain = dynamic_cast<Text*>(elements[0].get());
    auto* reopened = dynamic_cast<Text*>(elements[1].get());
    ASSERT_NE(reopenedPlain, nullptr);
    ASSERT_NE(reopened, nullptr);
    EXPECT_FALSE(reopenedPlain->hasBackground()) << "transparent stays transparent";
    EXPECT_EQ(reopened->getBackgroundColor(), (Color{255U, 235U, 59U, 200U}));
    EXPECT_EQ(reopened->getColor(), (Color{200U, 30U, 30U})) << "text colour untouched";
    EXPECT_EQ(reopened->getTransformation(), matrix) << "scale kept, so the padding keeps its size";
    EXPECT_DOUBLE_EQ(reopened->getWrap(), 120);
    EXPECT_FALSE(reopened->isBoxEnabled());
    EXPECT_NEAR(reopened->getBoundingBox().width, bounds.width, 0.001);
    EXPECT_NEAR(reopened->getBoundingBox().height, bounds.height, 0.001);
}
