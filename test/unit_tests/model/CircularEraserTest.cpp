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

// ---- Partial erasing: the ink of a segment touches the disc when |S(t) - c| <= R + h(t) ----

namespace {
// Brute force reference for segmentInkDiscIntervals: sample t and test the inequality directly
bool touchesAt(Vec a, Vec b, Vec c, double R, double h0, double h1, double t) {
    const double x = a.x + t * (b.x - a.x) - c.x;
    const double y = a.y + t * (b.y - a.y) - c.y;
    const double h = h0 + t * (h1 - h0);
    return std::hypot(x, y) <= R + h;
}

bool inIntervals(const std::vector<std::pair<double, double>>& iv, double t) {
    for (auto [t0, t1]: iv) {
        if (t >= t0 && t <= t1) {
            return true;
        }
    }
    return false;
}
}  // namespace

TEST(CircularInk, constantWidthSegmentMatchesClosedForm) {
    // Segment (0,0)-(100,0), eraser centre (50,10), R = 5, half width 6: touch when (100t-50)^2 + 100 <= 121
    auto iv = segmentInkDiscIntervals({0, 0}, {100, 0}, {50, 10}, 5.0, 6.0, 6.0);
    ASSERT_EQ(iv.size(), 1U);
    const double half = std::sqrt(21.0) / 100.0;
    EXPECT_NEAR(iv[0].first, 0.5 - half, 1e-12);
    EXPECT_NEAR(iv[0].second, 0.5 + half, 1e-12);
}

TEST(CircularInk, missesWhenInkDoesNotReachDisc) {
    // Distance 10 from the centreline, R + h = 5 + 1 = 6
    EXPECT_TRUE(segmentInkDiscIntervals({0, 0}, {100, 0}, {50, 10}, 5.0, 1.0, 1.0).empty());
}

TEST(CircularInk, zeroLengthSegmentIsAPoint) {
    auto hit = segmentInkDiscIntervals({5, 5}, {5, 5}, {5, 9}, 3.0, 1.0, 1.0);
    ASSERT_EQ(hit.size(), 1U);
    EXPECT_EQ(hit[0], std::pair(0.0, 1.0));
    EXPECT_TRUE(segmentInkDiscIntervals({5, 5}, {5, 5}, {5, 20}, 3.0, 1.0, 1.0).empty());
}

// Property: over many random segments, discs and width profiles (including steep tapers where the ink cross-section
// grows faster than the segment is long), the closed form agrees with a dense brute force sample of the inequality.
TEST(CircularInk, closedFormAgreesWithBruteForce) {
    unsigned state = 12345U;
    auto rnd = [&state](double lo, double hi) {
        state = state * 1664525U + 1013904223U;
        return lo + (hi - lo) * (static_cast<double>(state >> 8) / static_cast<double>(1U << 24));
    };
    int checked = 0;
    for (int trial = 0; trial < 300; ++trial) {
        const Vec a{rnd(-50, 50), rnd(-50, 50)};
        const Vec b{rnd(-50, 50), rnd(-50, 50)};
        const Vec c{rnd(-60, 60), rnd(-60, 60)};
        const double R = rnd(0.5, 25);
        const double h0 = rnd(0, 8);
        // Half the trials use a steep taper, which makes the touching set the complement of an interval
        const double h1 = (trial % 2 == 0) ? rnd(0, 8) : rnd(0, 60);
        const auto iv = segmentInkDiscIntervals(a, b, c, R, h0, h1);
        for (int k = 0; k <= 4000; ++k) {
            const double t = k / 4000.0;
            const bool brute = touchesAt(a, b, c, R, h0, h1, t);
            const double margin = std::abs(std::hypot(a.x + t * (b.x - a.x) - c.x, a.y + t * (b.y - a.y) - c.y) -
                                           (R + h0 + t * (h1 - h0)));
            if (margin < 1e-6) {
                continue;  // on a boundary, either answer is acceptable
            }
            EXPECT_EQ(inIntervals(iv, t), brute) << "trial " << trial << " t " << t;
            ++checked;
        }
    }
    EXPECT_GT(checked, 100000);
}

// Object eraser: a whole element is removed when the disc touches its bounding box (tangency included).
TEST(CircularObjectHit, InsideTouchingAndFar) {
    // Box [0, 10] x [0, 10]
    EXPECT_TRUE(circleTouchesRect({5, 5}, 1, 0, 0, 10, 10));      // centre inside
    EXPECT_TRUE(circleTouchesRect({-3, 5}, 3, 0, 0, 10, 10));     // tangent to the left edge
    EXPECT_TRUE(circleTouchesRect({-3, 5}, 3.5, 0, 0, 10, 10));   // overlapping the left edge
    EXPECT_FALSE(circleTouchesRect({-3.01, 5}, 3, 0, 0, 10, 10)); // just outside the left edge
}

TEST(CircularObjectHit, CornersAreCircular) {
    // Corner (10, 10): a square of half size 3 about (12.5, 12.5) would hit; a disc of radius 3 does not,
    // because the nearest point is at distance sqrt(2.5^2 + 2.5^2) ~ 3.54 > 3.
    EXPECT_FALSE(circleTouchesRect({12.5, 12.5}, 3, 0, 0, 10, 10));
    EXPECT_TRUE(circleTouchesRect({12.5, 12.5}, 3.6, 0, 0, 10, 10));
    // Exact diagonal tangency: distance to corner is exactly R.
    EXPECT_TRUE(circleTouchesRect({13, 14}, 5, 0, 0, 10, 10));
}

TEST(CircularObjectHit, DegenerateRectangle) {
    // A zero-width box behaves as a segment
    EXPECT_TRUE(circleTouchesRect({4, 2}, 2, 4, 0, 0, 10));
    EXPECT_FALSE(circleTouchesRect({1, 2}, 2, 4, 0, 0, 10));
}
