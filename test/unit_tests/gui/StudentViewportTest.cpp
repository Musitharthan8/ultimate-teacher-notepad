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
