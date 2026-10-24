/*
 * Ultimate Teacher Notepad
 *
 * The compact/wide decision and the tool caption rule, tested at fixed logical widths. These tests do not depend on
 * the monitor: a CI runner with a 1024 px desktop clamps window requests (it allocated 1008 px for a 1280 px request),
 * which made the former widget-only test fail for a reason unrelated to the product.
 *
 * Based on Xournal++ GPLv2+
 */

#include <initializer_list>

#include <gtest/gtest.h>

#include "gui/ResponsiveLayout.h"

using namespace utn::layout;

TEST(ResponsiveLayout, WideWindowsKeepTheWideLayout) {
    for (int width: {1080, 1121, 1280, 1920, 2560, 3840}) {
        EXPECT_FALSE(nextCompactLayout(false, width)) << width;
    }
}

TEST(ResponsiveLayout, NarrowWindowsEnterTheCompactLayout) {
    // 1008 and 1024 are the widths a 1024 px desktop gives; 1079 is the last compact width
    for (int width: {200, 800, 1008, 1024, 1079}) {
        EXPECT_TRUE(nextCompactLayout(false, width)) << width;
    }
}

TEST(ResponsiveLayout, EnteringIsExactlyBelow1080) {
    EXPECT_TRUE(nextCompactLayout(false, COMPACT_ENTER_BELOW - 1));
    EXPECT_FALSE(nextCompactLayout(false, COMPACT_ENTER_BELOW));
}

TEST(ResponsiveLayout, LeavingIsExactlyAbove1120) {
    EXPECT_TRUE(nextCompactLayout(true, COMPACT_LEAVE_ABOVE));
    EXPECT_FALSE(nextCompactLayout(true, COMPACT_LEAVE_ABOVE + 1));
}

TEST(ResponsiveLayout, HysteresisBandKeepsWhateverStateTheWindowHad) {
    for (int width = COMPACT_ENTER_BELOW; width <= COMPACT_LEAVE_ABOVE; ++width) {
        EXPECT_FALSE(nextCompactLayout(false, width)) << width;
        EXPECT_TRUE(nextCompactLayout(true, width)) << width;
    }
}

TEST(ResponsiveLayout, PlaceholderSizesBeforeTheWindowIsShownAreIgnored) {
    for (int width: {0, 1, 199}) {
        EXPECT_FALSE(nextCompactLayout(false, width)) << width;
        EXPECT_TRUE(nextCompactLayout(true, width)) << width;  // and do not undo a compact state either
    }
}

TEST(ResponsiveLayout, DraggingTheEdgeDoesNotFlicker) {
    bool compact = false;
    int changes = 0;
    // Wiggle around the thresholds: one change on the way down, one on the way up, none in between
    for (int width: {1300, 1100, 1085, 1079, 1090, 1075, 1110, 1119, 1121, 1115, 1130}) {
        const bool next = nextCompactLayout(compact, width);
        changes += next != compact;
        compact = next;
    }
    EXPECT_EQ(changes, 2);
    EXPECT_FALSE(compact);
}

TEST(ResponsiveLayout, CompactToWideToCompactRoundTrip) {
    bool compact = false;
    compact = nextCompactLayout(compact, 1280);
    EXPECT_FALSE(compact);
    compact = nextCompactLayout(compact, 1008);
    EXPECT_TRUE(compact);
    compact = nextCompactLayout(compact, 1280);
    EXPECT_FALSE(compact);
    compact = nextCompactLayout(compact, 1024);
    EXPECT_TRUE(compact);
}

TEST(ResponsiveLayout, AutomaticCaptionsFollowTheLayout) {
    EXPECT_TRUE(railLabelsVisible(LABELS_AUTOMATIC, false));
    EXPECT_FALSE(railLabelsVisible(LABELS_AUTOMATIC, true));
}

TEST(ResponsiveLayout, ShowCaptionsIgnoresTheWidth) {
    EXPECT_TRUE(railLabelsVisible(LABELS_SHOW, false));
    EXPECT_TRUE(railLabelsVisible(LABELS_SHOW, true));
}

TEST(ResponsiveLayout, HideCaptionsIgnoresTheWidth) {
    EXPECT_FALSE(railLabelsVisible(LABELS_HIDE, false));
    EXPECT_FALSE(railLabelsVisible(LABELS_HIDE, true));
}

TEST(ResponsiveLayout, UnknownModesNeverShowCaptions) {
    // A corrupt setting must not make the rail inconsistent: only the documented modes show captions
    for (int mode: {-1, 3, 99}) {
        EXPECT_FALSE(railLabelsVisible(mode, false)) << mode;
        EXPECT_FALSE(railLabelsVisible(mode, true)) << mode;
    }
}

TEST(ResponsiveLayout, CaptionsAtTheWidthsOfTheCiRunnerAndOfATeacherLaptop) {
    // The same decision chain the window runs: width -> compact -> captions
    struct Case {
        int width;
        bool automaticCaptions;
    };
    for (const Case c: {Case{1008, false}, Case{1024, false}, Case{1280, true}, Case{1920, true}}) {
        const bool compact = nextCompactLayout(false, c.width);
        EXPECT_EQ(railLabelsVisible(LABELS_AUTOMATIC, compact), c.automaticCaptions) << c.width;
        EXPECT_TRUE(railLabelsVisible(LABELS_SHOW, compact)) << c.width;
        EXPECT_FALSE(railLabelsVisible(LABELS_HIDE, compact)) << c.width;
    }
}
