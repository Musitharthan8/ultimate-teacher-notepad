/*
 * Ultimate Teacher Notepad
 *
 * Placement of the floating selection toolbar: above the selection, below it near the top edge, clamped inside the
 * visible canvas, following the scroll position, and absent when the selection is off screen.
 *
 * Based on Xournal++ GPLv2+
 */

#include <gtest/gtest.h>

#include "model/SelectionToolbarPlacement.h"

using namespace utn;

namespace {
constexpr double GAP = 8;
constexpr double WIDTH = 120;
constexpr double HEIGHT = 30;
const ToolbarBox VIEW{0, 0, 800, 600};
}  // namespace

TEST(SelectionToolbarPlacement, sitsCentredAboveTheSelectionWhenThereIsRoom) {
    const auto at = placeSelectionToolbar({100, 200, 50, 40}, WIDTH, HEIGHT, VIEW, GAP);
    ASSERT_TRUE(at);
    EXPECT_DOUBLE_EQ(at->x, 125 - WIDTH / 2);
    EXPECT_DOUBLE_EQ(at->y, 200 - GAP - HEIGHT);
}

TEST(SelectionToolbarPlacement, flipsBelowTheSelectionNearTheTopEdge) {
    const auto at = placeSelectionToolbar({100, 20, 50, 40}, WIDTH, HEIGHT, VIEW, GAP);
    ASSERT_TRUE(at);
    EXPECT_DOUBLE_EQ(at->y, 20 + 40 + GAP);
    // Exactly enough room above still counts as room
    const auto exact = placeSelectionToolbar({100, GAP + HEIGHT, 50, 40}, WIDTH, HEIGHT, VIEW, GAP);
    ASSERT_TRUE(exact);
    EXPECT_DOUBLE_EQ(exact->y, 0);
}

TEST(SelectionToolbarPlacement, staysInsideTheVisibleCanvasAtTheLeftAndRightEdges) {
    const auto left = placeSelectionToolbar({0, 200, 20, 40}, WIDTH, HEIGHT, VIEW, GAP);
    ASSERT_TRUE(left);
    EXPECT_DOUBLE_EQ(left->x, 0);
    const auto right = placeSelectionToolbar({790, 200, 10, 40}, WIDTH, HEIGHT, VIEW, GAP);
    ASSERT_TRUE(right);
    EXPECT_DOUBLE_EQ(right->x, 800 - WIDTH);
}

TEST(SelectionToolbarPlacement, aSelectionTallerThanTheWindowGetsTheToolbarAtTheTop) {
    const auto at = placeSelectionToolbar({100, 10, 50, 580}, WIDTH, HEIGHT, VIEW, GAP);
    ASSERT_TRUE(at);
    EXPECT_DOUBLE_EQ(at->y, 0);
    EXPECT_GE(at->y + HEIGHT, 0);
    EXPECT_LE(at->y + HEIGHT, 600);
}

TEST(SelectionToolbarPlacement, followsTheScrollPosition) {
    const ToolbarBox scrolled{300, 1000, 800, 600};
    const auto at = placeSelectionToolbar({400, 1200, 50, 40}, WIDTH, HEIGHT, scrolled, GAP);
    ASSERT_TRUE(at);
    EXPECT_DOUBLE_EQ(at->x, 425 - WIDTH / 2 - 300);
    EXPECT_DOUBLE_EQ(at->y, 1200 - GAP - HEIGHT - 1000);
    // Scrolled so the selection touches the top of the window: the toolbar goes below
    const auto below = placeSelectionToolbar({400, 1010, 50, 40}, WIDTH, HEIGHT, scrolled, GAP);
    ASSERT_TRUE(below);
    EXPECT_DOUBLE_EQ(below->y, 1010 + 40 + GAP - 1000);
}

TEST(SelectionToolbarPlacement, noToolbarWhileTheSelectionIsScrolledOutOfView) {
    EXPECT_FALSE(placeSelectionToolbar({100, 700, 50, 40}, WIDTH, HEIGHT, VIEW, GAP));
    EXPECT_FALSE(placeSelectionToolbar({100, -100, 50, 40}, WIDTH, HEIGHT, VIEW, GAP));
    EXPECT_FALSE(placeSelectionToolbar({900, 200, 50, 40}, WIDTH, HEIGHT, VIEW, GAP));
    const ToolbarBox scrolled{0, 1000, 800, 600};
    EXPECT_FALSE(placeSelectionToolbar({100, 200, 50, 40}, WIDTH, HEIGHT, scrolled, GAP));
}

TEST(SelectionToolbarPlacement, aToolbarWiderThanTheWindowStartsAtItsLeftEdge) {
    const auto at = placeSelectionToolbar({100, 200, 50, 40}, 900, HEIGHT, VIEW, GAP);
    ASSERT_TRUE(at);
    EXPECT_DOUBLE_EQ(at->x, 0);
}
