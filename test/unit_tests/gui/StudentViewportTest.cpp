/*
 * Ultimate Teacher Notepad
 *
 * Student View zoom: placement of the page in the projector window.
 *
 * Based on Xournal++ GPLv2+
 */

#include <gtest/gtest.h>

#include "gui/toolbarMenubar/StudentViewport.h"

using utn::StudentViewport;

namespace {
constexpr double PW = 600, PH = 800;  // portrait page
constexpr double WW = 1280, WH = 720;  // landscape projector
}  // namespace

TEST(StudentViewport, DefaultFitsWholePageCentred) {
    StudentViewport v;
    auto l = v.resolve(WW, WH, PW, PH);
    EXPECT_DOUBLE_EQ(l.scale, WH / PH);
    EXPECT_NEAR(l.offsetX, 0.5 * (WW - PW * l.scale), 1e-9);
    EXPECT_NEAR(l.offsetY, 0.0, 1e-9);
}

TEST(StudentViewport, FitWidthFillsWidthAndStartsAtTop) {
    StudentViewport v;
    v.setFitWidth();
    auto l = v.resolve(WW, WH, PW, PH);
    EXPECT_DOUBLE_EQ(l.scale, WW / PW);
    EXPECT_NEAR(l.offsetX, 0.0, 1e-9);
    EXPECT_NEAR(l.offsetY, 0.0, 1e-9);
    // scrolling down moves the page up, but never past its bottom
    v.panBy(0, 1e6, WW, WH, PW, PH);
    l = v.resolve(WW, WH, PW, PH);
    EXPECT_NEAR(l.offsetY, WH - PH * l.scale, 1e-6);
}

TEST(StudentViewport, ZoomInKeepsPointUnderCursor) {
    StudentViewport v;
    auto before = v.resolve(WW, WH, PW, PH);
    const double ax = WW / 2, ay = 200;
    const double px = (ax - before.offsetX) / before.scale, py = (ay - before.offsetY) / before.scale;
    v.zoomBy(2.0, ax, ay, WW, WH, PW, PH);
    auto after = v.resolve(WW, WH, PW, PH);
    EXPECT_NEAR(after.scale, before.scale * 2.0, 1e-9);
    EXPECT_NEAR(px * after.scale + after.offsetX, ax, 1e-6);
    EXPECT_NEAR(py * after.scale + after.offsetY, ay, 1e-6);
}

TEST(StudentViewport, ZoomOutStopsAtFitPageAndZoomInIsCapped) {
    StudentViewport v;
    v.zoomBy(0.1, 10, 10, WW, WH, PW, PH);
    EXPECT_DOUBLE_EQ(v.resolve(WW, WH, PW, PH).scale, WH / PH);
    v.zoomBy(1e6, 10, 10, WW, WH, PW, PH);
    EXPECT_NEAR(v.resolve(WW, WH, PW, PH).scale, WH / PH * StudentViewport::MAX_MAGNIFICATION, 1e-9);
}

TEST(StudentViewport, ZoomOutFromFitWidthLeavesFitMode) {
    StudentViewport v;
    v.setFitWidth();
    v.zoomBy(0.5, WW / 2, WH / 2, WW, WH, PW, PH);
    EXPECT_FALSE(v.isFit());
    EXPECT_LT(v.resolve(WW, WH, PW, PH).scale, WW / PW);
    v.setFitPage();
    EXPECT_TRUE(v.isFit());
}

TEST(StudentViewport, PanIsClampedToThePage) {
    StudentViewport v;
    v.zoomBy(4.0, 0, 0, WW, WH, PW, PH);
    v.panBy(-1e6, -1e6, WW, WH, PW, PH);
    auto l = v.resolve(WW, WH, PW, PH);
    EXPECT_NEAR(l.offsetX, 0.0, 1e-9);
    EXPECT_NEAR(l.offsetY, 0.0, 1e-9);
    v.panBy(1e6, 1e6, WW, WH, PW, PH);
    l = v.resolve(WW, WH, PW, PH);
    EXPECT_NEAR(l.offsetX, WW - PW * l.scale, 1e-6);
    EXPECT_NEAR(l.offsetY, WH - PH * l.scale, 1e-6);
}

TEST(StudentViewport, DegenerateSizesDoNotCrash) {
    StudentViewport v;
    v.zoomBy(2.0, 0, 0, 0, 0, 0, 0);
    v.panBy(5, 5, 0, 0, 0, 0);
    EXPECT_DOUBLE_EQ(v.resolve(0, 0, 0, 0).scale, 1.0);
}

// Follow Teacher: the students' window shows the part of the page the teacher's window shows
TEST(StudentViewportFollow, WholePageVisibleIsPlainFitPage) {
    StudentViewport v;
    StudentViewport::Region r;
    // teacher window shows the whole page at zoom 1 (page at 0,0 in a 1000x900 window)
    ASSERT_TRUE(StudentViewport::visibleRegionOfPage(0, 0, 1000, 900, 20, 10, 1.0, PW, PH, r));
    v.showRegion(r, WW, WH, PW, PH);
    EXPECT_TRUE(v.isFit());
    auto l = v.resolve(WW, WH, PW, PH);
    EXPECT_DOUBLE_EQ(l.scale, WH / PH);
}

TEST(StudentViewportFollow, ScrollingTheTeachersWindowMovesTheStudentsRegion) {
    // Zoomed page 2x: 1200x1600 px. Teacher window 800x500 px shows 400x250 page units.
    StudentViewport v;
    StudentViewport::Region top, lower;
    ASSERT_TRUE(StudentViewport::visibleRegionOfPage(0, 0, 800, 500, 0, 0, 2.0, PW, PH, top));
    ASSERT_TRUE(StudentViewport::visibleRegionOfPage(0, 600, 800, 500, 0, 0, 2.0, PW, PH, lower));
    EXPECT_NEAR(top.y, 0.0, 1e-9);
    EXPECT_NEAR(lower.y, 300.0, 1e-9);  // paragraph three, not one

    v.showRegion(top, WW, WH, PW, PH);
    auto a = v.resolve(WW, WH, PW, PH);
    v.showRegion(lower, WW, WH, PW, PH);
    auto b = v.resolve(WW, WH, PW, PH);
    EXPECT_FALSE(v.isFit());
    // The page point at the top of the teacher's region sits at the top of the students' window in both cases (+- centring)
    const double topOfTopRegion = -a.offsetY / a.scale;
    const double topOfLowerRegion = -b.offsetY / b.scale;
    EXPECT_NEAR(topOfLowerRegion - topOfTopRegion, 300.0, 1.0);
    // and the students see the teacher's height of page (250 units), not the whole page
    EXPECT_NEAR(WH / b.scale, 250.0, 1.0);
}

TEST(StudentViewportFollow, PageNotOnScreenOrDegenerateInputIsFitPageNeverACrash) {
    StudentViewport::Region r;
    EXPECT_FALSE(StudentViewport::visibleRegionOfPage(0, 2000, 800, 500, 0, 0, 1.0, PW, PH, r));  // scrolled past
    EXPECT_FALSE(StudentViewport::visibleRegionOfPage(0, 0, 0, 500, 0, 0, 1.0, PW, PH, r));
    EXPECT_FALSE(StudentViewport::visibleRegionOfPage(0, 0, 800, 500, 0, 0, 0.0, PW, PH, r));
    StudentViewport v;
    v.showRegion({0, 0, 0, 0}, WW, WH, PW, PH);
    EXPECT_TRUE(v.isFit());
    v.showRegion({0, 0, 100, 100}, 0, 0, PW, PH);
    EXPECT_TRUE(v.isFit());
}

TEST(StudentViewportFollow, RegionNeverMagnifiesBeyondTheCap) {
    StudentViewport v;
    v.showRegion({100, 100, 1, 1}, WW, WH, PW, PH);
    auto l = v.resolve(WW, WH, PW, PH);
    EXPECT_LE(l.scale, (WH / PH) * StudentViewport::MAX_MAGNIFICATION + 1e-9);
}

TEST(StudentViewportFollow, TeacherViewScrolledPastPageEdgesIsCutToThePage) {
    StudentViewport::Region r;
    // teacher window larger than the page and offset: the region is exactly the page
    ASSERT_TRUE(StudentViewport::visibleRegionOfPage(-50, -50, 2000, 2000, 100, 100, 1.0, PW, PH, r));
    EXPECT_DOUBLE_EQ(r.x, 0.0);
    EXPECT_DOUBLE_EQ(r.y, 0.0);
    EXPECT_DOUBLE_EQ(r.width, PW);
    EXPECT_DOUBLE_EQ(r.height, PH);
}
