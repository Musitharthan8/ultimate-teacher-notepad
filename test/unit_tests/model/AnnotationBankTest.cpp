/*
 * Ultimate Teacher Notepad
 *
 * Annotation Bank: the built-in marks, comments of the teacher's own, a saved form that survives a restart and never
 * trusts what it reads, and where an annotation lands on the page.
 *
 * Based on Xournal++ GPLv2+
 */

#include <string>

#include <gtest/gtest.h>

#include "model/AnnotationBank.h"

using namespace utn;

TEST(AnnotationBank, StartsWithTickCrossStarSeeMeAndWellDone) {
    AnnotationBank bank;
    ASSERT_EQ(bank.all().size(), 5U);
    EXPECT_EQ(bank.all()[0].text, "✓");
    EXPECT_EQ(bank.all()[1].text, "✗");
    EXPECT_EQ(bank.all()[2].text, "★");
    EXPECT_EQ(bank.all()[3].text, "See me");
    EXPECT_EQ(bank.all()[4].text, "Well done");
    for (std::size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(bank.all()[i].style, AnnotationStyle::Symbol) << i;
    }
    EXPECT_EQ(bank.all()[3].style, AnnotationStyle::Comment);
    EXPECT_EQ(bank.all()[4].style, AnnotationStyle::Comment);
    for (const Annotation& entry: bank.all()) {
        EXPECT_FALSE(entry.label.empty());
        // A symbol's button says in words what the mark is, and still shows the mark
        EXPECT_NE(entry.label.find(entry.text), std::string::npos) << entry.label;
        EXPECT_EQ(entry.rgb & 0xFF000000U, 0U);
    }
    EXPECT_EQ(bank.all()[0].label, "✓ Tick");
}

TEST(AnnotationBank, AddsTheTeachersOwnCommentTrimmedAndOnOneLine) {
    AnnotationBank bank;
    EXPECT_EQ(bank.addComment("  Show\nyour\tworking  ", 0x1E5BD8), AnnotationBank::AddResult::Added);
    ASSERT_EQ(bank.all().size(), 6U);
    const Annotation& added = bank.all().back();
    EXPECT_EQ(added.text, "Show your working");
    EXPECT_EQ(added.label, "Show your working");
    EXPECT_EQ(added.style, AnnotationStyle::Comment);
    EXPECT_EQ(added.rgb, 0x1E5BD8U);
}

TEST(AnnotationBank, RefusesEmptyRepeatedAndInvalidComments) {
    AnnotationBank bank;
    EXPECT_EQ(bank.addComment("", 0), AnnotationBank::AddResult::Empty);
    EXPECT_EQ(bank.addComment(" \t\n ", 0), AnnotationBank::AddResult::Empty);
    EXPECT_EQ(bank.addComment("\xFF\xFE", 0), AnnotationBank::AddResult::Empty) << "not UTF-8";
    EXPECT_EQ(bank.addComment("see ME", 0), AnnotationBank::AddResult::Duplicate) << "already there, any case";
    EXPECT_EQ(bank.all().size(), 5U);
}

TEST(AnnotationBank, CutsLongCommentsWithoutSplittingACharacter) {
    AnnotationBank bank;
    std::string longText;
    for (int i = 0; i < 100; ++i) {
        longText += "é";  // two bytes each
    }
    ASSERT_EQ(bank.addComment(longText, 0), AnnotationBank::AddResult::Shortened) << "says it was cut";
    const Annotation& added = bank.all().back();
    EXPECT_LE(added.text.size(), AnnotationBank::MAX_TEXT_BYTES);
    EXPECT_LE(added.label.size(), AnnotationBank::MAX_LABEL_BYTES);
    EXPECT_EQ(added.text.size() % 2, 0U) << "a whole number of two-byte characters";
    EXPECT_EQ(added.label.size() % 2, 0U);
    // Exactly at the limit is not cut
    EXPECT_EQ(bank.addComment(std::string(AnnotationBank::MAX_TEXT_BYTES, 'x'), 0), AnnotationBank::AddResult::Added);
}

TEST(AnnotationBank, SavedFieldsOfMultiByteCharactersAreKeptUpToTheByteLimitAndNotCut) {
    std::string label;
    for (std::size_t i = 0; i < AnnotationBank::MAX_LABEL_BYTES / 2; ++i) {
        label += "é";  // exactly MAX_LABEL_BYTES bytes
    }
    const AnnotationBank atLimit = AnnotationBank::parse("v1;comment|000000|" + label + "|x");
    ASSERT_EQ(atLimit.all().size(), 1U);
    EXPECT_EQ(atLimit.all()[0].label, label);
    // One more character: dropped whole, never cut inside a character
    const AnnotationBank over = AnnotationBank::parse("v1;comment|000000|" + label + "é|x;comment|000000|ok|ok");
    ASSERT_EQ(over.all().size(), 1U);
    EXPECT_EQ(over.all()[0].text, "ok");
}

TEST(AnnotationBank, TheSameTextAsSymbolAndAsCommentIsOneEntry) {
    const AnnotationBank read = AnnotationBank::parse("v1;symbol|2e7d32|✓ Tick|✓;comment|000000|Tick again|✓");
    ASSERT_EQ(read.all().size(), 1U) << "the first one stays";
    EXPECT_EQ(read.all()[0].style, AnnotationStyle::Symbol);
    AnnotationBank bank;
    EXPECT_EQ(bank.addComment("✓", 0), AnnotationBank::AddResult::Duplicate) << "the tick symbol is already there";
}

TEST(AnnotationBank, HoldsAtMostTheMaximumNumberOfEntries) {
    AnnotationBank bank;
    for (std::size_t i = bank.all().size(); i < AnnotationBank::MAX_ENTRIES; ++i) {
        ASSERT_EQ(bank.addComment("Comment " + std::to_string(i), 0), AnnotationBank::AddResult::Added);
    }
    EXPECT_EQ(bank.addComment("One too many", 0), AnnotationBank::AddResult::Full);
    EXPECT_EQ(bank.all().size(), AnnotationBank::MAX_ENTRIES);
}

TEST(AnnotationBank, RemovesAnEntryAndRestoresTheDefaults) {
    AnnotationBank bank;
    bank.addComment("Neat work", 0);
    EXPECT_TRUE(bank.remove(0));
    EXPECT_FALSE(bank.remove(99));
    ASSERT_EQ(bank.all().size(), 5U);
    EXPECT_EQ(bank.all()[0].text, "✗");
    bank.restoreDefaults();
    ASSERT_EQ(bank.all().size(), 5U);
    EXPECT_EQ(bank.all()[0].text, "✓");
    EXPECT_EQ(bank.all()[4].text, "Well done") << "the teacher's own comment went with the restore";
}

TEST(AnnotationBank, SavedFormRoundTripsEveryField) {
    AnnotationBank bank;
    bank.remove(1);
    bank.addComment("100% | right; well done", 0x6A1B9A);
    const AnnotationBank read = AnnotationBank::parse(bank.serialise());
    ASSERT_EQ(read.all().size(), bank.all().size());
    for (std::size_t i = 0; i < bank.all().size(); ++i) {
        EXPECT_EQ(read.all()[i].label, bank.all()[i].label) << i;
        EXPECT_EQ(read.all()[i].text, bank.all()[i].text) << i;
        EXPECT_EQ(read.all()[i].style, bank.all()[i].style) << i;
        EXPECT_EQ(read.all()[i].rgb, bank.all()[i].rgb) << i;
    }
}

TEST(AnnotationBank, AnEmptiedBankStaysEmpty) {
    AnnotationBank bank;
    while (bank.remove(0)) {
    }
    const AnnotationBank read = AnnotationBank::parse(bank.serialise());
    EXPECT_TRUE(read.all().empty());
}

TEST(AnnotationBank, UnreadableSavedFormsGiveTheDefaults) {
    for (const std::string& saved: {std::string(), std::string("garbage"), std::string("v9;comment|000000|A|A"),
                                   std::string("v1"), std::string("v1;"), std::string("v1;comment|000000|A"),
                                   std::string(20000, 'x')}) {
        const AnnotationBank read = AnnotationBank::parse(saved);
        ASSERT_EQ(read.all().size(), 5U) << saved.substr(0, 40);
        EXPECT_EQ(read.all()[0].text, "✓");
    }
}

TEST(AnnotationBank, MalformedEntriesAreDroppedAndTheRestKept) {
    const std::string saved = "v1"
                              ";comment|00ff00|Good|Good"            // kept
                              ";sticker|000000|Pic|Pic"              // unknown style
                              ";comment|12345|Bad|Bad"               // colour too short
                              ";comment|zzzzzz|Bad|Bad"              // colour not hex
                              ";comment|000000||Empty label"         // empty label
                              ";comment|000000|Empty text|"          // empty text
                              ";comment|000000|Line%0Abreak|Line"    // control character
                              ";comment|000000|Bad%ZZ|Bad"           // broken escape
                              ";comment|000000|\xFF|x"               // not UTF-8
                              ";symbol|d32f2f|✗ Cross|✗"             // kept
                              ";comment|00ff00|Good again|Good";     // the same text again
    const AnnotationBank read = AnnotationBank::parse(saved);
    ASSERT_EQ(read.all().size(), 2U);
    EXPECT_EQ(read.all()[0].text, "Good");
    EXPECT_EQ(read.all()[0].rgb, 0x00FF00U);
    EXPECT_EQ(read.all()[1].text, "✗");
    EXPECT_EQ(read.all()[1].style, AnnotationStyle::Symbol);
}

TEST(AnnotationBank, OverLongEntriesAreDroppedAndTheCountIsBounded) {
    std::string saved = "v1;comment|000000|" + std::string(AnnotationBank::MAX_LABEL_BYTES + 1, 'a') + "|a";
    saved += ";comment|000000|b|" + std::string(AnnotationBank::MAX_TEXT_BYTES + 1, 'b');
    for (std::size_t i = 0; i < AnnotationBank::MAX_ENTRIES + 10; ++i) {
        saved += ";comment|000000|n" + std::to_string(i) + "|n" + std::to_string(i);
    }
    const AnnotationBank read = AnnotationBank::parse(saved);
    ASSERT_EQ(read.all().size(), AnnotationBank::MAX_ENTRIES);
    EXPECT_EQ(read.all()[0].text, "n0") << "both over-long entries were dropped";
}

TEST(AnnotationPlacement, CentredInTheVisiblePartOfThePage) {
    const auto at = placeAnnotation(AnnotationArea{100, 200, 300, 400}, 600, 800, 40, 20);
    EXPECT_DOUBLE_EQ(at.x, 100 + 150 - 20);
    EXPECT_DOUBLE_EQ(at.y, 200 + 200 - 10);
}

TEST(AnnotationPlacement, NothingOnScreenMeansTheMiddleOfThePage) {
    const auto at = placeAnnotation(std::nullopt, 600, 800, 40, 20);
    EXPECT_DOUBLE_EQ(at.x, 280);
    EXPECT_DOUBLE_EQ(at.y, 390);
}

TEST(AnnotationPlacement, StaysOnThePageNearItsEdges) {
    // Only a thin strip at the bottom right is on screen
    const auto at = placeAnnotation(AnnotationArea{590, 795, 10, 5}, 600, 800, 40, 20);
    EXPECT_DOUBLE_EQ(at.x, 560);
    EXPECT_DOUBLE_EQ(at.y, 780);
    const auto top = placeAnnotation(AnnotationArea{0, 0, 10, 5}, 600, 800, 40, 20);
    EXPECT_DOUBLE_EQ(top.x, 0);
    EXPECT_DOUBLE_EQ(top.y, 0);
}

TEST(AnnotationPlacement, LargerThanThePageStartsAtItsEdge) {
    const auto at = placeAnnotation(AnnotationArea{100, 100, 200, 200}, 600, 800, 700, 900);
    EXPECT_DOUBLE_EQ(at.x, 0);
    EXPECT_DOUBLE_EQ(at.y, 0);
}
