/*
 * Ultimate Teacher Notepad
 *
 * Sentence and paragraph boundaries for Ctrl+double-click and Ctrl+triple-click on PDF text.
 */

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "pdf/base/PdfTextUnits.h"

using namespace xoj::pdf::textunits;

TEST(PdfTextUnits, aSentenceStopsAtFullStopsButNotAtAbbreviationsOrDecimals) {
    const std::u32string text = U"Dr. Smith went home. He left! Ok? 3.5 is a number. Next";

    auto [first, last] = sentenceRange(text, text.find(U"Smith"));
    EXPECT_EQ(first, 0U) << "Dr. is an abbreviation, so the sentence starts at Dr";
    EXPECT_EQ(text[last], U'.');
    EXPECT_EQ(last, text.find(U"home.") + 4);

    auto [heFirst, heLast] = sentenceRange(text, text.find(U"left"));
    EXPECT_EQ(heFirst, text.find(U"He"));
    EXPECT_EQ(text[heLast], U'!');

    auto [numFirst, numLast] = sentenceRange(text, text.find(U"is a"));
    EXPECT_EQ(numFirst, text.find(U"3.5"));
    EXPECT_EQ(numLast, text.find(U"number.") + 6) << "3.5 does not split the sentence";
}

TEST(PdfTextUnits, emptyTextHasNoSentence) {
    auto [first, last] = sentenceRange(U"", 0);
    EXPECT_EQ(first, 0U);
    EXPECT_EQ(last, 0U);
}

TEST(PdfTextUnits, aParagraphBreaksAtALargeVerticalGap) {
    const std::u32string text = U"abcdef";
    // Two characters per line, 10 high: lines 1 and 2 are 2 apart, line 3 is far below
    const std::vector<Box> boxes = {{0, 0, 5, 10},   {5, 0, 10, 10},  {0, 12, 5, 22},
                                    {5, 12, 10, 22}, {0, 60, 5, 70}, {5, 60, 10, 70}};
    auto [first, last] = paragraphRange(text, boxes, 1);
    EXPECT_EQ(first, 0U);
    EXPECT_EQ(last, 3U) << "the third line is a new paragraph";

    auto [first2, last2] = paragraphRange(text, boxes, 5);
    EXPECT_EQ(first2, 4U);
    EXPECT_EQ(last2, 5U);
}

TEST(PdfTextUnits, aPointFindsTheCharacterUnderIt) {
    const std::vector<Box> boxes = {{0, 0, 5, 10}, {5, 0, 10, 10}};
    EXPECT_EQ(charIndexAt(boxes, 7, 5), 1U);
    EXPECT_EQ(charIndexAt(boxes, 1, 5), 0U);
}
