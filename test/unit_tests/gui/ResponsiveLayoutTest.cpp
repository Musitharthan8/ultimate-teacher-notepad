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

TEST(ResponsiveLayout, ThumbnailsStepAsideBelow1400AndReturnAbove1440) {
    EXPECT_TRUE(nextThumbnailsAway(false, THUMBNAILS_HIDE_BELOW - 1));
    EXPECT_FALSE(nextThumbnailsAway(false, THUMBNAILS_HIDE_BELOW));
    EXPECT_TRUE(nextThumbnailsAway(true, THUMBNAILS_RETURN_ABOVE));
    EXPECT_FALSE(nextThumbnailsAway(true, THUMBNAILS_RETURN_ABOVE + 1));
    for (int width: {900, 1024, 1280, 1399}) {
        EXPECT_TRUE(nextThumbnailsAway(false, width)) << width;
    }
    for (int width: {1400, 1440, 1920, 3440}) {
        EXPECT_FALSE(nextThumbnailsAway(false, width)) << width;
    }
}

TEST(ResponsiveLayout, InspectorFoldsBelow960AndReturnsAbove1000) {
    EXPECT_TRUE(nextInspectorFolded(false, INSPECTOR_FOLD_BELOW - 1));
    EXPECT_FALSE(nextInspectorFolded(false, INSPECTOR_FOLD_BELOW));
    EXPECT_TRUE(nextInspectorFolded(true, INSPECTOR_RETURN_ABOVE));
    EXPECT_FALSE(nextInspectorFolded(true, INSPECTOR_RETURN_ABOVE + 1));
    // The three widths the sprint is tested at: only 900 folds the inspector, only 1280 and below hide thumbnails
    EXPECT_TRUE(nextInspectorFolded(false, 900));
    EXPECT_FALSE(nextInspectorFolded(false, 1024));
    EXPECT_FALSE(nextInspectorFolded(false, 1280));
}

TEST(ResponsiveLayout, PanelPoliciesIgnorePlaceholderSizesAndDoNotFlicker) {
    for (int width: {0, 1, 199}) {
        EXPECT_FALSE(nextThumbnailsAway(false, width)) << width;
        EXPECT_FALSE(nextInspectorFolded(false, width)) << width;
        EXPECT_TRUE(nextThumbnailsAway(true, width)) << width;
        EXPECT_TRUE(nextInspectorFolded(true, width)) << width;
    }
    bool away = false;
    int changes = 0;
    for (int width: {1500, 1420, 1399, 1410, 1380, 1430, 1440, 1441, 1425, 1450}) {
        const bool next = nextThumbnailsAway(away, width);
        changes += next != away;
        away = next;
    }
    EXPECT_EQ(changes, 2) << "one change on the way down, one on the way up";
}

// The app bar is measured, so these use the widths measured on a 96 DPI and a 120 DPI display (728 and 803 px for the
// same bar) as plain numbers: the rule must hold for both without knowing which display it is.
TEST(ResponsiveLayout, AppBarThatFitsMovesNothing) {
    EXPECT_EQ(appBarItemsToOverflow(728, 800, {36, 36, 36}, 36), 0u);
    EXPECT_EQ(appBarItemsToOverflow(800, 800, {36, 36, 36}, 36), 0u) << "exactly the available width fits";
}

TEST(ResponsiveLayout, AppBarMovesJustEnoughItemsAndPaysForTheMoreButton) {
    const std::vector<int> movable{36, 36, 36, 36, 36};
    // 803 -> 800 needs 3 px, but the first item only helps once it has paid for the 36 px "More" button: 2 items
    EXPECT_EQ(appBarItemsToOverflow(803, 800, movable, 36), 2u);
    // One item more than a bare 36 px saving: 803 needs 3 px + 36 px button = 39 px
    EXPECT_EQ(appBarItemsToOverflow(840, 800, movable, 36), 3u);
    EXPECT_EQ(appBarItemsToOverflow(836, 800, movable, 36), 2u) << "72 px saved - 36 px button = 36 px, which is exactly enough";
}

TEST(ResponsiveLayout, AppBarMovesEverythingMovableWhenThatIsStillNotEnough) {
    const std::vector<int> movable{36, 36};
    EXPECT_EQ(appBarItemsToOverflow(2000, 800, movable, 36), 2u);
}

TEST(ResponsiveLayout, AppBarWithNothingMovableStaysPut) {
    EXPECT_EQ(appBarItemsToOverflow(2000, 800, {}, 36), 0u);
}

TEST(ResponsiveLayout, AppBarMovesTheSamePrefixWhateverTheWidthOfTheItems) {
    // Items are taken in the order given, not by size: the least important goes first even when it is the narrow one
    const std::vector<int> movable{20, 60, 36};
    EXPECT_EQ(appBarItemsToOverflow(850, 800, movable, 10), 2u);  // 20 + 60 - 10 = 70 >= 50; 20 - 10 = 10 < 50
}

TEST(ResponsiveLayout, BeginnerNamesEveryToolAndExpertShowsIconsOnly) {
    EXPECT_EQ(toolLabelModeFor(InterfaceMode::Beginner), LABELS_SHOW);
    EXPECT_EQ(toolLabelModeFor(InterfaceMode::Expert), LABELS_HIDE);
    EXPECT_EQ(toolLabelModeFor(InterfaceMode::Custom), -1) << "Custom sets nothing: the teacher's own labels stay";
    EXPECT_TRUE(contextHintsShown(InterfaceMode::Beginner));
    EXPECT_TRUE(contextHintsShown(InterfaceMode::Custom)) << "an existing profile keeps its hints";
    EXPECT_FALSE(contextHintsShown(InterfaceMode::Expert));
}

TEST(ResponsiveLayout, AChoiceThatAgreesWithTheModeKeepsItAndADifferentOneIsTheTeachersOwn) {
    EXPECT_EQ(modeAfterLabelChoice(InterfaceMode::Expert, LABELS_HIDE), InterfaceMode::Expert);
    EXPECT_EQ(modeAfterLabelChoice(InterfaceMode::Beginner, LABELS_SHOW), InterfaceMode::Beginner);
    EXPECT_EQ(modeAfterLabelChoice(InterfaceMode::Expert, LABELS_SHOW), InterfaceMode::Custom)
            << "a by-hand choice is never silently overridden by the mode";
    EXPECT_EQ(modeAfterLabelChoice(InterfaceMode::Beginner, LABELS_AUTOMATIC), InterfaceMode::Custom);
    for (int labels: {LABELS_AUTOMATIC, LABELS_SHOW, LABELS_HIDE}) {
        EXPECT_EQ(modeAfterLabelChoice(InterfaceMode::Custom, labels), InterfaceMode::Custom);
    }
}

TEST(ResponsiveLayout, TheSavedModeRoundTripsAndAnythingElseIsCustom) {
    for (InterfaceMode mode: {InterfaceMode::Custom, InterfaceMode::Beginner, InterfaceMode::Expert}) {
        EXPECT_EQ(parseInterfaceMode(interfaceModeName(mode)), mode);
    }
    for (const char* unknown: {"", "Expert", "advanced", "beginner ", "0"}) {
        EXPECT_EQ(parseInterfaceMode(unknown), InterfaceMode::Custom) << "'" << unknown << "'";
    }
}
