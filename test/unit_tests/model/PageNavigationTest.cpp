/*
 * Ultimate Teacher Notepad
 *
 * Rules of the Lesson Navigator that need no GTK: typed page numbers, the bookmark toggle over page labels, the
 * previous/next labelled page, and the list of recent pages.
 *
 * Based on Xournal++ GPLv2+
 */

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "model/PageNavigation.h"

using namespace utn;

TEST(PageNavigation, aTypedPageNumberIsOneBasedAndComesBackZeroBased) {
    EXPECT_EQ(parsePageNumber("1", 10), std::optional<size_t>(0));
    EXPECT_EQ(parsePageNumber("10", 10), std::optional<size_t>(9));
    EXPECT_EQ(parsePageNumber("  7 ", 10), std::optional<size_t>(6));
}

TEST(PageNavigation, pageNumbersOutsideTheDocumentOrNotNumbersAreRefused) {
    EXPECT_FALSE(parsePageNumber("0", 10));
    EXPECT_FALSE(parsePageNumber("11", 10));
    EXPECT_FALSE(parsePageNumber("-3", 10));
    EXPECT_FALSE(parsePageNumber("", 10));
    EXPECT_FALSE(parsePageNumber("   ", 10));
    EXPECT_FALSE(parsePageNumber("4x", 10));
    EXPECT_FALSE(parsePageNumber("3.5", 10));
    EXPECT_FALSE(parsePageNumber("99999999999999999999999", 10));
    EXPECT_FALSE(parsePageNumber("1", 0));
}

TEST(PageNavigation, bookmarkingAnUnlabelledPageLabelsIt) {
    const auto t = toggleBookmark("");
    EXPECT_EQ(t.change, BookmarkChange::Add);
    EXPECT_EQ(t.label, BOOKMARK_LABEL);
}

TEST(PageNavigation, bookmarkingAgainRemovesOnlyTheBookmarkLabel) {
    const auto t = toggleBookmark(BOOKMARK_LABEL);
    EXPECT_EQ(t.change, BookmarkChange::Remove);
    EXPECT_EQ(t.label, "");
}

TEST(PageNavigation, aPageWithItsOwnLabelIsAlreadyMarkedAndKeepsItsLabel) {
    const auto t = toggleBookmark("Answer Key");
    EXPECT_EQ(t.change, BookmarkChange::KeepLabel);
    EXPECT_EQ(t.label, "Answer Key");
}

TEST(PageNavigation, isMarkedMeansAnyLabel) {
    EXPECT_FALSE(isMarked(""));
    EXPECT_TRUE(isMarked("CW"));
    EXPECT_TRUE(isMarked(BOOKMARK_LABEL));
}

TEST(PageNavigation, nextAndPreviousLabelledSkipUnlabelledPagesAndStopAtTheEnds) {
    const std::vector<bool> marked{false, true, false, false, true, false};
    EXPECT_EQ(adjacentMarked(marked, 0, +1), std::optional<size_t>(1));
    EXPECT_EQ(adjacentMarked(marked, 1, +1), std::optional<size_t>(4));
    EXPECT_FALSE(adjacentMarked(marked, 4, +1));
    EXPECT_EQ(adjacentMarked(marked, 5, -1), std::optional<size_t>(4));
    EXPECT_EQ(adjacentMarked(marked, 4, -1), std::optional<size_t>(1));
    EXPECT_FALSE(adjacentMarked(marked, 1, -1));
    EXPECT_FALSE(adjacentMarked({}, 0, +1));
}

TEST(PageNavigation, recentPagesAreNewestFirstWithoutDuplicatesOrTheCurrentPage) {
    const std::vector<size_t> newestFirst{3, 7, 3, 2, 7, 9};
    EXPECT_EQ(recentPages(newestFirst, 2, 10), (std::vector<size_t>{3, 7, 9}));
}

TEST(PageNavigation, previousAndNextAreUnavailableAtTheEnds) {
    EXPECT_FALSE(canGoPrevious(0, 5));
    EXPECT_TRUE(canGoPrevious(1, 5));
    EXPECT_TRUE(canGoPrevious(4, 5));
    EXPECT_TRUE(canGoNext(0, 5));
    EXPECT_TRUE(canGoNext(3, 5));
    EXPECT_FALSE(canGoNext(4, 5));
}

TEST(PageNavigation, anEmptyOrStaleDocumentOffersNoMove) {
    EXPECT_FALSE(canGoPrevious(0, 0));
    EXPECT_FALSE(canGoNext(0, 0));
    // A page index past the end (the count is momentarily behind): never step to a page that is not there
    EXPECT_FALSE(canGoPrevious(9, 3));
    EXPECT_FALSE(canGoNext(9, 3));
}

TEST(PageNavigation, recentPagesAreCapped) {
    const std::vector<size_t> newestFirst{1, 2, 3, 4, 5};
    EXPECT_EQ(recentPages(newestFirst, 0, 3), (std::vector<size_t>{1, 2, 3}));
    EXPECT_TRUE(recentPages({}, 0, 3).empty());
}
