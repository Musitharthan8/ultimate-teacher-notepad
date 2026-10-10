/*
 * Ultimate Teacher Notepad
 *
 * Regression tests for the circular eraser geometry. The square eraser used a box test, so these cases
 * distinguish a disc from a square of the same size.
 *
 * Based on Xournal++ GPLv2+
 */

#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "model/eraser/CircularEraser.h"

using namespace utn::eraser;

namespace {
std::vector<Knot> line(double x0, double y0, double x1, double y1, double h = 1.0) {
    return {Knot{{x0, y0}, h}, Knot{{x1, y1}, h}};
}
}  // namespace

// A square eraser of half size 5 would hit the corner point (4.9, 4.9); a disc of radius 5 does not
TEST(CircularEraserTest, CornerOfSquareIsNotInsideDisc) {
    auto knots = line(4.9, 4.9, 4.9, 4.9, 0.0);
    EXPECT_FALSE(circleHitsStroke(Vec{0, 0}, 5.0, knots));
}

TEST(CircularEraserTest, PointInsideDiscHits) {
    EXPECT_TRUE(circleHitsStroke(Vec{0, 0}, 5.0, line(3.0, 0.0, 3.0, 0.5, 0.0)));
}

TEST(CircularEraserTest, HitIncludesStrokeHalfWidth) {
    // Centreline at distance 7 with half width 2 reaches r = 5 exactly: a touch is a hit
    EXPECT_TRUE(circleHitsStroke(Vec{0, 0}, 5.0, line(7.0, -10.0, 7.0, 10.0, 2.0)));
    EXPECT_FALSE(circleHitsStroke(Vec{0, 0}, 5.0, line(7.1, -10.0, 7.1, 10.0, 2.0)));
}

TEST(CircularEraserTest, EndpointNearMissAndHit) {
    // Endpoint at (6, 0); the segment runs away from the centre
    EXPECT_FALSE(circleHitsStroke(Vec{0, 0}, 5.0, line(6.5, 0.0, 20.0, 0.0, 0.0)));
    EXPECT_TRUE(circleHitsStroke(Vec{0, 0}, 5.0, line(4.0, 0.0, 20.0, 0.0, 0.0)));
}

TEST(CircularEraserTest, DiagonalStrokeUsesPerpendicularDistance) {
    // Line x + y = 7 is at distance 7/sqrt(2) ~ 4.95 from the origin: a hit for r = 5
    EXPECT_TRUE(circleHitsStroke(Vec{0, 0}, 5.0, line(7.0, 0.0, 0.0, 7.0, 0.0)));
    // Line x + y = 8 is at distance ~5.66: a miss
    EXPECT_FALSE(circleHitsStroke(Vec{0, 0}, 5.0, line(8.0, 0.0, 0.0, 8.0, 0.0)));
}

TEST(CircularEraserTest, CurvedStrokeHitsOnAnySegment) {
    // Arc of radius 10 centred at the origin; the eraser sits at the origin
    std::vector<Knot> arc;
    for (int i = 0; i <= 8; ++i) {
        double a = (M_PI / 2.0) * i / 8.0;
        arc.push_back(Knot{{10 * std::cos(a), 10 * std::sin(a)}, 0.0});
    }
    EXPECT_FALSE(circleHitsStroke(Vec{0, 0}, 5.0, arc));
    // Each chord spans pi/16, so its closest point is 10 cos(pi/32) ~ 9.95 from the origin: r = 10 touches it
    EXPECT_TRUE(circleHitsStroke(Vec{0, 0}, 10.0, arc));
}

TEST(CircularEraserTest, SegmentDiscIntervalFullyInside) {
    auto iv = segmentDiscInterval(Vec{-1, 0}, Vec{1, 0}, Vec{0, 0}, 5.0);
    ASSERT_TRUE(iv.has_value());
    EXPECT_DOUBLE_EQ(iv->first, 0.0);
    EXPECT_DOUBLE_EQ(iv->second, 1.0);
}

TEST(CircularEraserTest, SegmentDiscIntervalCrossing) {
    // Segment from x=-10 to x=10 crosses the disc of radius 5 at x=-5 and x=5: t in [0.25, 0.75]
    auto iv = segmentDiscInterval(Vec{-10, 0}, Vec{10, 0}, Vec{0, 0}, 5.0);
    ASSERT_TRUE(iv.has_value());
    EXPECT_NEAR(iv->first, 0.25, 1e-12);
    EXPECT_NEAR(iv->second, 0.75, 1e-12);
}

TEST(CircularEraserTest, SegmentDiscIntervalMiss) {
    EXPECT_FALSE(segmentDiscInterval(Vec{-10, 6}, Vec{10, 6}, Vec{0, 0}, 5.0).has_value());
}

TEST(CircularEraserTest, DegenerateSegmentIsAPoint) {
    EXPECT_TRUE(segmentDiscInterval(Vec{1, 1}, Vec{1, 1}, Vec{0, 0}, 5.0).has_value());
    EXPECT_FALSE(segmentDiscInterval(Vec{9, 9}, Vec{9, 9}, Vec{0, 0}, 5.0).has_value());
}
