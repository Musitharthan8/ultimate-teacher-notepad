/*
 * Ultimate Teacher Notepad
 *
 * Undo History panel rules: which steps are listed, in what order, where the current point is, what each row says and
 * when the list must stay hidden. Free of GTK.
 *
 * Based on Xournal++ GPLv2+
 */

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "model/UndoHistory.h"

using namespace utn;

namespace {
std::vector<std::string> texts(const UndoHistoryView& view) {
    std::vector<std::string> out;
    for (const auto& row: view.rows) {
        out.push_back(row.text);
    }
    return out;
}
}  // namespace

// Done A, B, C, then Undo twice: C and B are undone. Newest first: C, B (next Redo), here, A (next Undo).
TEST(UndoHistory, listsNewestFirstWithTheCurrentPointBetweenUndoneAndDoneSteps) {
    const auto view = buildUndoHistory({"A"}, 1, {"B", "C"}, 2);
    EXPECT_EQ(texts(view), (std::vector<std::string>{"C", "B", "A"}));
    EXPECT_EQ(view.positionIndex, 2U) << "the marker goes after the undone steps";
    EXPECT_EQ(view.rows[0].side, HistorySide::Redo);
    EXPECT_EQ(view.rows[0].steps, 2U);
    EXPECT_EQ(view.rows[1].side, HistorySide::Redo);
    EXPECT_EQ(view.rows[1].steps, 1U) << "what Redo redoes";
    EXPECT_EQ(view.rows[2].side, HistorySide::Undo);
    EXPECT_EQ(view.rows[2].steps, 1U) << "what Undo undoes";
    EXPECT_EQ(view.olderHidden, 0U);
    EXPECT_EQ(view.newerHidden, 0U);
}

TEST(UndoHistory, anEmptyHistoryHasNoRowsAndTheMarkerAtTheTop) {
    const auto view = buildUndoHistory({}, 0, {}, 0);
    EXPECT_TRUE(view.rows.empty());
    EXPECT_EQ(view.positionIndex, 0U);
}

TEST(UndoHistory, isBoundedAndKeepsTheStepsNearestTheCurrentPoint) {
    std::vector<std::string> undo, redo;
    for (int i = 0; i < 80; ++i) {
        undo.push_back("u" + std::to_string(i));  // u0 is what Undo undoes
    }
    for (int i = 0; i < 40; ++i) {
        redo.push_back("r" + std::to_string(i));  // r0 is what Redo redoes
    }
    const auto view = buildUndoHistory(undo, 80, redo, 40, 50);
    ASSERT_EQ(view.rows.size(), 50U);
    EXPECT_EQ(view.positionIndex, 25U) << "half each side when both are long";
    EXPECT_EQ(view.rows[24].text, "r0");
    EXPECT_EQ(view.rows[25].text, "u0");
    EXPECT_EQ(view.rows.front().text, "r24");
    EXPECT_EQ(view.rows.back().text, "u24");
    EXPECT_EQ(view.newerHidden, 15U);
    EXPECT_EQ(view.olderHidden, 55U);
}

TEST(UndoHistory, aShortSideLeavesItsRoomToTheOther) {
    std::vector<std::string> undo;
    for (int i = 0; i < 80; ++i) {
        undo.push_back("u" + std::to_string(i));
    }
    const auto view = buildUndoHistory(undo, 80, {"r0", "r1"}, 2, 50);
    ASSERT_EQ(view.rows.size(), 50U);
    EXPECT_EQ(view.positionIndex, 2U);
    EXPECT_EQ(view.rows.back().text, "u47");
    EXPECT_EQ(view.olderHidden, 32U);
    EXPECT_EQ(view.newerHidden, 0U);
}

// The handler hands over at most the limit; the totals still say how many there are
TEST(UndoHistory, totalsLargerThanTheTextsHandedOverCountAsHidden) {
    const auto view = buildUndoHistory({"u0", "u1"}, 200, {}, 0, 2);
    EXPECT_EQ(view.rows.size(), 2U);
    EXPECT_EQ(view.olderHidden, 198U);
}

TEST(UndoHistory, aStepWithoutWordsIsStillDescribed) {
    EXPECT_EQ(stepText("Write text"), "Write text");
    EXPECT_EQ(stepText("  Change color \n"), "Change color");
    EXPECT_EQ(stepText(""), "Change");
    EXPECT_EQ(stepText("!! NOTHING !!"), "Change") << "an empty group's placeholder is not for people";
    const auto view = buildUndoHistory({""}, 1, {}, 0);
    EXPECT_EQ(view.rows[0].text, "Change");
}

TEST(UndoHistory, rowsSayWhatUndoAndRedoAffectAndTheNameStartsWithTheVisibleLabel) {
    const HistoryRow nextUndo{HistorySide::Undo, 1, "Write text"};
    const HistoryRow olderUndo{HistorySide::Undo, 3, "Delete"};
    const HistoryRow nextRedo{HistorySide::Redo, 1, "Change color"};
    const HistoryRow laterRedo{HistorySide::Redo, 2, "Insert layer"};
    EXPECT_EQ(rowLabel(nextUndo), "Write text (next Undo)");
    EXPECT_EQ(rowLabel(olderUndo), "Delete");
    EXPECT_EQ(rowLabel(nextRedo), "Change color (next Redo, undone)");
    EXPECT_EQ(rowLabel(laterRedo), "Insert layer (undone)");
    for (const auto& row: {nextUndo, olderUndo, nextRedo, laterRedo}) {
        const std::string name = rowAccessibleName(row);
        EXPECT_EQ(name.rfind(rowLabel(row), 0), 0U) << name;
        EXPECT_GT(name.size(), rowLabel(row).size()) << "the name also says what activating does";
    }
    EXPECT_NE(rowAccessibleName(olderUndo).find("3 steps"), std::string::npos) << rowAccessibleName(olderUndo);
    EXPECT_NE(rowAccessibleName(nextUndo).find("1 step"), std::string::npos) << rowAccessibleName(nextUndo);
    EXPECT_NE(rowAccessibleName(laterRedo).find("Redo"), std::string::npos) << rowAccessibleName(laterRedo);
}

// Busy: the list is the teacher's to read, nothing may run. Blank cover and quiet Present: the list stays hidden.
TEST(UndoHistory, theGateHidesTheListUnderTheCoverAndInQuietPresentAndOnlyRefusesWhileBusy) {
    EXPECT_EQ(historyGate(false, false, false), HistoryGate::Open);
    EXPECT_EQ(historyGate(true, false, false), HistoryGate::Busy);
    EXPECT_EQ(historyGate(false, true, false), HistoryGate::Covered);
    EXPECT_EQ(historyGate(false, false, true), HistoryGate::QuietPresent);
    EXPECT_EQ(historyGate(true, true, true), HistoryGate::Covered) << "privacy first";
    EXPECT_EQ(historyGate(true, false, true), HistoryGate::QuietPresent) << "privacy first";
    EXPECT_FALSE(gateHidesList(HistoryGate::Open));
    EXPECT_FALSE(gateHidesList(HistoryGate::Busy));
    EXPECT_TRUE(gateHidesList(HistoryGate::Covered));
    EXPECT_TRUE(gateHidesList(HistoryGate::QuietPresent));
    EXPECT_TRUE(gateReason(HistoryGate::Open).empty());
    for (auto gate: {HistoryGate::Busy, HistoryGate::Covered, HistoryGate::QuietPresent}) {
        EXPECT_EQ(gateReason(gate).rfind("Unavailable", 0), 0U) << gateReason(gate);
    }
}
