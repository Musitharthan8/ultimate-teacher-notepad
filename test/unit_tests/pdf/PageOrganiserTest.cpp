#include <gtest/gtest.h>

#include "pdf/tools/PageOrganiser.h"

using utn::pdftools::PageOrganiser;

namespace {
std::vector<size_t> order(const PageOrganiser& o) {
    std::vector<size_t> out;
    for (const auto& p: o.pieces()) {
        out.push_back(p.pageIndex);
    }
    return out;
}
}  // namespace

TEST(PageOrganiser, startsWithEveryPageInOrderAndUnturned) {
    PageOrganiser o("a.pdf", 4);
    EXPECT_EQ(o.size(), 4u);
    EXPECT_EQ(order(o), (std::vector<size_t>{0, 1, 2, 3}));
    EXPECT_FALSE(o.changed());
    EXPECT_FALSE(o.canUndo());
    for (const auto& p: o.pieces()) {
        EXPECT_EQ(p.rotateDegrees, 0);
        EXPECT_EQ(p.file, fs::path("a.pdf"));
    }
}

TEST(PageOrganiser, pagesMoveUpAndDownAndStopAtTheEnds) {
    PageOrganiser o("a.pdf", 3);
    EXPECT_FALSE(o.moveUp(0)) << "already first";
    EXPECT_FALSE(o.moveDown(2)) << "already last";
    EXPECT_FALSE(o.moveUp(9));
    EXPECT_TRUE(o.moveDown(0));
    EXPECT_EQ(order(o), (std::vector<size_t>{1, 0, 2}));
    EXPECT_TRUE(o.moveUp(2));
    EXPECT_EQ(order(o), (std::vector<size_t>{1, 2, 0}));
    EXPECT_TRUE(o.changed());
}

TEST(PageOrganiser, moveToPutsThePageAtTheTarget) {
    PageOrganiser o("a.pdf", 5);
    EXPECT_TRUE(o.moveTo(0, 3));
    EXPECT_EQ(order(o), (std::vector<size_t>{1, 2, 3, 0, 4}));
    EXPECT_TRUE(o.moveTo(4, 0));
    EXPECT_EQ(order(o), (std::vector<size_t>{4, 1, 2, 3, 0}));
    EXPECT_FALSE(o.moveTo(2, 2)) << "nothing to do";
    EXPECT_FALSE(o.moveTo(0, 5));
    EXPECT_FALSE(o.moveTo(7, 0));
}

TEST(PageOrganiser, rotatingAddsUpWrapsAndWorksBothWays) {
    PageOrganiser o("a.pdf", 2);
    EXPECT_TRUE(o.rotate(0, 1));
    EXPECT_EQ(o.at(0).rotateDegrees, 90);
    EXPECT_TRUE(o.rotate(0, 1));
    EXPECT_TRUE(o.rotate(0, 1));
    EXPECT_EQ(o.at(0).rotateDegrees, 270);
    EXPECT_TRUE(o.rotate(0, 1));
    EXPECT_EQ(o.at(0).rotateDegrees, 0) << "four quarter turns is a full turn";
    EXPECT_TRUE(o.rotate(1, -1));
    EXPECT_EQ(o.at(1).rotateDegrees, 270) << "anticlockwise from upright";
    EXPECT_TRUE(o.rotate(1, 6));
    EXPECT_EQ(o.at(1).rotateDegrees, 90);
    EXPECT_FALSE(o.rotate(1, 4)) << "a whole number of turns changes nothing";
    EXPECT_FALSE(o.rotate(5, 1));
}

TEST(PageOrganiser, removingTakesAPageOutButNeverTheLastOne) {
    PageOrganiser o("a.pdf", 3);
    EXPECT_TRUE(o.remove(1));
    EXPECT_EQ(order(o), (std::vector<size_t>{0, 2}));
    EXPECT_TRUE(o.remove(0));
    EXPECT_EQ(order(o), (std::vector<size_t>{2}));
    EXPECT_FALSE(o.remove(0)) << "a PDF needs a page";
    EXPECT_FALSE(o.remove(4));
    EXPECT_EQ(o.size(), 1u);
}

TEST(PageOrganiser, undoStepsBackThroughEveryChange) {
    PageOrganiser o("a.pdf", 3);
    o.moveDown(0);
    o.rotate(0, 1);
    o.remove(2);
    EXPECT_EQ(order(o), (std::vector<size_t>{1, 0}));
    EXPECT_TRUE(o.undo());
    EXPECT_EQ(order(o), (std::vector<size_t>{1, 0, 2}));
    EXPECT_TRUE(o.undo());
    EXPECT_EQ(o.at(0).rotateDegrees, 0);
    EXPECT_TRUE(o.undo());
    EXPECT_EQ(order(o), (std::vector<size_t>{0, 1, 2}));
    EXPECT_FALSE(o.undo());
    EXPECT_FALSE(o.changed());
}

TEST(PageOrganiser, resetGoesBackToTheStartAndCanBeUndone) {
    PageOrganiser o("a.pdf", 3);
    o.remove(0);
    o.rotate(0, 2);
    o.reset();
    EXPECT_FALSE(o.changed());
    EXPECT_EQ(order(o), (std::vector<size_t>{0, 1, 2}));
    EXPECT_TRUE(o.undo());
    EXPECT_EQ(order(o), (std::vector<size_t>{1, 2})) << "undoing the reset brings the edits back";
    PageOrganiser untouched("a.pdf", 2);
    untouched.reset();
    EXPECT_FALSE(untouched.canUndo()) << "resetting nothing adds no undo step";
}

TEST(PageOrganiser, changedIsFalseAgainWhenEditsCancelOut) {
    PageOrganiser o("a.pdf", 3);
    o.moveDown(0);
    EXPECT_TRUE(o.changed());
    o.moveUp(1);
    EXPECT_FALSE(o.changed());
    o.rotate(0, 1);
    o.rotate(0, -1);
    EXPECT_FALSE(o.changed());
}

TEST(PageOrganiser, failedOperationsAddNoUndoStep) {
    PageOrganiser o("a.pdf", 2);
    o.moveUp(0);
    o.rotate(0, 4);
    o.remove(9);
    EXPECT_FALSE(o.canUndo());
}
