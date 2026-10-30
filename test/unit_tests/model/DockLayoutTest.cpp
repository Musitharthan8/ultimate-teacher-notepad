/*
 * Ultimate Teacher Notepad
 *
 * The customisable tool dock: pinned favourites, hidden tools and order within a group, applied to the rail's default
 * list, with a saved form that survives a restart and never trusts what it reads.
 *
 * Based on Xournal++ GPLv2+
 */

#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "model/DockLayout.h"

using namespace utn;
using Ids = std::vector<std::string>;

namespace {
// The teacher rail as shipped: two groups of tools
const Ids DEFAULT_RAIL{"UTN_SELECT", "SELECT",   "PEN",        "TEACHING_KIT",       "SMART_HIGHLIGHTER",
                       "UTN_COLOURS", "ERASER",  "UTN_TEXT",   "ANSWER_BOX",         "TEACHER_STAMP",
                       "SEPARATOR",   "CLASSROOM_WORKFLOW", "PREPARE_REVEAL", "HAND"};

bool contains(const Ids& ids, const std::string& id) { return std::find(ids.begin(), ids.end(), id) != ids.end(); }
size_t indexOf(const Ids& ids, const std::string& id) {
    return static_cast<size_t>(std::find(ids.begin(), ids.end(), id) - ids.begin());
}
}  // namespace

TEST(DockLayout, TheDefaultDockIsExactlyTheShippedRail) {
    const DockLayout layout;
    EXPECT_TRUE(layout.isDefault());
    EXPECT_EQ(layout.arrange(DEFAULT_RAIL), DEFAULT_RAIL) << "nothing customised: nothing changes, separator included";
}

TEST(DockLayout, APinnedToolMovesToAFavouritesGroupAtTheTop) {
    DockLayout layout;
    ASSERT_TRUE(layout.pin("HAND"));
    EXPECT_FALSE(layout.isDefault());
    const Ids rail = layout.arrange(DEFAULT_RAIL);
    EXPECT_EQ(rail.front(), "HAND");
    EXPECT_EQ(rail[1], "SEPARATOR") << "favourites are a group of their own";
    EXPECT_EQ(std::count(rail.begin(), rail.end(), "HAND"), 1) << "a pinned tool is not also shown in its old place";
    EXPECT_EQ(rail.size(), DEFAULT_RAIL.size() + 1) << "one more separator, for the favourites group; no tool lost";
}

TEST(DockLayout, PinnedToolsKeepTheOrderTheyWerePinnedIn) {
    DockLayout layout;
    layout.pin("ERASER");
    layout.pin("PEN");
    const Ids rail = layout.arrange(DEFAULT_RAIL);
    EXPECT_EQ(rail[0], "ERASER");
    EXPECT_EQ(rail[1], "PEN");
}

TEST(DockLayout, UnpinningReturnsAToolToItsPlace) {
    DockLayout layout;
    layout.pin("HAND");
    EXPECT_TRUE(layout.unpin("HAND"));
    EXPECT_TRUE(layout.isDefault());
    EXPECT_EQ(layout.arrange(DEFAULT_RAIL), DEFAULT_RAIL);
    EXPECT_FALSE(layout.unpin("HAND")) << "unpinning what is not pinned changes nothing";
}

TEST(DockLayout, AHiddenToolLeavesTheDockAndAnEmptiedGroupLeavesItsSeparator) {
    DockLayout layout;
    for (const char* id: {"CLASSROOM_WORKFLOW", "PREPARE_REVEAL", "HAND"}) {
        ASSERT_TRUE(layout.hide(id)) << id;
    }
    const Ids rail = layout.arrange(DEFAULT_RAIL);
    EXPECT_FALSE(contains(rail, "HAND"));
    EXPECT_FALSE(contains(rail, "SEPARATOR")) << "no separator is left dangling beside nothing";
    EXPECT_TRUE(layout.show("HAND"));
    EXPECT_TRUE(contains(layout.arrange(DEFAULT_RAIL), "HAND"));
}

TEST(DockLayout, EssentialToolsCanNeverBeHidden) {
    DockLayout layout;
    for (const char* id: {"PEN", "ERASER", "SELECT", "UTN_SELECT"}) {
        EXPECT_TRUE(DockLayout::isEssential(id)) << id;
        EXPECT_FALSE(layout.hide(id)) << id << " must stay reachable on the dock";
    }
    EXPECT_TRUE(layout.isDefault());
    EXPECT_TRUE(contains(layout.arrange(DEFAULT_RAIL), "PEN"));
}

TEST(DockLayout, PinningShowsAndHidingUnpins) {
    DockLayout layout;
    layout.hide("HAND");
    EXPECT_TRUE(layout.pin("HAND"));
    EXPECT_FALSE(layout.isHidden("HAND"));
    EXPECT_TRUE(layout.isPinned("HAND"));
    EXPECT_TRUE(layout.hide("HAND"));
    EXPECT_FALSE(layout.isPinned("HAND"));
    EXPECT_TRUE(layout.isHidden("HAND"));
}

TEST(DockLayout, MovingUpAndDownStaysInsideTheToolsGroup) {
    DockLayout layout;
    ASSERT_TRUE(layout.moveDown("PEN", DEFAULT_RAIL));
    Ids rail = layout.arrange(DEFAULT_RAIL);
    EXPECT_GT(indexOf(rail, "PEN"), indexOf(rail, "TEACHING_KIT"));
    EXPECT_TRUE(layout.moveUp("PEN", DEFAULT_RAIL));
    EXPECT_EQ(layout.arrange(DEFAULT_RAIL), DEFAULT_RAIL) << "up then down is where it began";

    EXPECT_FALSE(layout.moveUp("UTN_SELECT", DEFAULT_RAIL)) << "the first of a group cannot go further up";
    EXPECT_FALSE(layout.moveDown("TEACHER_STAMP", DEFAULT_RAIL)) << "and the last cannot cross the separator";
    EXPECT_FALSE(layout.moveUp("CLASSROOM_WORKFLOW", DEFAULT_RAIL)) << "first of the second group";
    EXPECT_TRUE(layout.moveDown("CLASSROOM_WORKFLOW", DEFAULT_RAIL));
    rail = layout.arrange(DEFAULT_RAIL);
    EXPECT_LT(indexOf(rail, "SEPARATOR"), indexOf(rail, "PREPARE_REVEAL"));
    EXPECT_LT(indexOf(rail, "PREPARE_REVEAL"), indexOf(rail, "CLASSROOM_WORKFLOW"));
    EXPECT_EQ(rail.size(), DEFAULT_RAIL.size());
}

TEST(DockLayout, MovingSkipsHiddenToolsAndWorksWithinTheFavourites) {
    DockLayout layout;
    layout.hide("TEACHING_KIT");
    ASSERT_TRUE(layout.moveDown("PEN", DEFAULT_RAIL));
    Ids rail = layout.arrange(DEFAULT_RAIL);
    EXPECT_LT(indexOf(rail, "SMART_HIGHLIGHTER"), indexOf(rail, "PEN")) << "one step past what is shown";

    layout.pin("HAND");
    layout.pin("ERASER");
    EXPECT_TRUE(layout.moveUp("ERASER", DEFAULT_RAIL));
    rail = layout.arrange(DEFAULT_RAIL);
    EXPECT_EQ(rail[0], "ERASER");
    EXPECT_EQ(rail[1], "HAND");
    EXPECT_FALSE(layout.moveUp("ERASER", DEFAULT_RAIL)) << "already first in the favourites";
}

TEST(DockLayout, ResetBringsBackTheShippedRail) {
    DockLayout layout;
    layout.pin("HAND");
    layout.hide("UTN_TEXT");
    layout.moveDown("PEN", DEFAULT_RAIL);
    layout.reset();
    EXPECT_TRUE(layout.isDefault());
    EXPECT_EQ(layout.arrange(DEFAULT_RAIL), DEFAULT_RAIL);
}

TEST(DockLayout, ToolsThatNoLongerExistOrAreDuplicatedDoNoHarm) {
    DockLayout layout;
    layout.pin("GONE_TOOL");  // a tool a later version removed
    layout.hide("ANOTHER_GONE");
    layout.pin("HAND");
    layout.pin("HAND");  // twice
    const Ids rail = layout.arrange(DEFAULT_RAIL);
    EXPECT_FALSE(contains(rail, "GONE_TOOL"));
    EXPECT_EQ(std::count(rail.begin(), rail.end(), "HAND"), 1);
    for (const std::string& id: DEFAULT_RAIL) {
        if (id != "SEPARATOR") {
            EXPECT_TRUE(contains(rail, id)) << id << " must not disappear because of an unrelated bad entry";
        }
    }
}

TEST(DockLayout, AToolAddedByALaterVersionIsNotHiddenByAnOldLayout) {
    DockLayout layout;
    layout.moveDown("PEN", DEFAULT_RAIL);
    Ids newer = DEFAULT_RAIL;
    newer.insert(newer.begin() + 4, "BRAND_NEW_TOOL");
    const Ids rail = layout.arrange(newer);
    EXPECT_TRUE(contains(rail, "BRAND_NEW_TOOL")) << "a tool the teacher never saw is not hidden by an old layout";
}

TEST(DockLayout, ASavedLayoutComesBackExactly) {
    DockLayout layout;
    layout.pin("HAND");
    layout.pin("ERASER");
    layout.hide("UTN_TEXT");
    layout.moveDown("PEN", DEFAULT_RAIL);
    const DockLayout back = DockLayout::parse(layout.serialise());
    EXPECT_EQ(back.arrange(DEFAULT_RAIL), layout.arrange(DEFAULT_RAIL));
    EXPECT_TRUE(back.isPinned("HAND"));
    EXPECT_TRUE(back.isHidden("UTN_TEXT"));
    EXPECT_EQ(DockLayout().serialise(), DockLayout::parse(DockLayout().serialise()).serialise());
}

TEST(DockLayout, NothingSavedOrUnreadableMeansTheDefaultDock) {
    for (const std::string bad: {std::string(""), std::string("garbage"), std::string("v9;pinned=HAND"),
                                 std::string("pinned=HAND"), std::string(100000, 'x')}) {
        EXPECT_TRUE(DockLayout::parse(bad).isDefault()) << bad.substr(0, 30);
    }
}

TEST(DockLayout, BadEntriesInASavedLayoutAreDroppedAndTheGoodOnesKept) {
    const DockLayout layout =
            DockLayout::parse("v1;pinned=HAND,bad id!,HAND,,ERASER;hidden=PEN,UTN_TEXT;order=;extra=1");
    EXPECT_TRUE(layout.isPinned("HAND"));
    EXPECT_TRUE(layout.isPinned("ERASER"));
    EXPECT_FALSE(layout.isHidden("PEN")) << "an essential tool is never hidden, whatever the file says";
    EXPECT_TRUE(layout.isHidden("UTN_TEXT"));
    EXPECT_EQ(std::count(layout.pinned().begin(), layout.pinned().end(), "HAND"), 1);
    EXPECT_EQ(std::count(layout.pinned().begin(), layout.pinned().end(), "bad id!"), 0);
}

TEST(DockLayout, AFileCannotPinAndHideTheSameTool) {
    const DockLayout layout = DockLayout::parse("v1;pinned=HAND;hidden=HAND");
    EXPECT_NE(layout.isPinned("HAND"), layout.isHidden("HAND"));
}
