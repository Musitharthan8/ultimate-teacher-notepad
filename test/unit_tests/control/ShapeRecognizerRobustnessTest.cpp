/*
 * UTN Smart Shape regression cases: scale, rotation, pen jitter and open loops.
 * No GTK, settings profile or real user data is touched.
 */

#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "control/shaperecognizer/CircleRecognizer.h"
#include "control/shaperecognizer/ShapeRecognizer.h"
#include "model/Point.h"
#include "model/Stroke.h"

namespace {
constexpr double PI = 3.14159265358979323846;

Point rotate(double x, double y, double radians) {
    return {x * std::cos(radians) - y * std::sin(radians),
            x * std::sin(radians) + y * std::cos(radians)};
}

void polygon(Stroke& stroke, const std::vector<Point>& corners, int samplesPerEdge, bool leaveGap,
             double jitter = 0.0) {
    for (size_t edge = 0; edge < corners.size(); ++edge) {
        const Point& a = corners[edge];
        const Point& b = corners[(edge + 1) % corners.size()];
        const bool last = edge + 1 == corners.size();
        const double portion = (leaveGap && last) ? 0.55 : 1.0;
        for (int i = 0; i < samplesPerEdge; ++i) {
            const double t = portion * i / samplesPerEdge;
            const double wobble = jitter * std::sin(i * 2.37 + edge * 1.17);
            stroke.addPoint({a.x + (b.x - a.x) * t + wobble,
                             a.y + (b.y - a.y) * t - wobble * 0.7});
        }
        if (last) {
            const double t = portion;
            stroke.addPoint({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t});
        }
    }
}

void rectangleStartingMidEdge(Stroke& stroke, const std::vector<Point>& corners) {
    // Start halfway along edge 0 and finish a quarter of the way along it.
    // The endpoints leave a genuine gap on the middle of an edge, not at a corner.
    const auto add = [&](const Point& a, const Point& b, double t) {
        const double wobble = 0.13 * std::sin(stroke.getPointCount() * 2.37);
        stroke.addPoint({a.x + (b.x - a.x) * t + wobble, a.y + (b.y - a.y) * t - 0.7 * wobble});
    };
    for (int i = 0; i <= 12; ++i) {
        add(corners[0], corners[1], 0.5 + 0.5 * i / 12.0);
    }
    for (size_t edge = 1; edge < 4; ++edge) {
        for (int i = 1; i <= 18; ++i) {
            add(corners[edge], corners[(edge + 1) % 4], i / 18.0);
        }
    }
    for (int i = 1; i <= 6; ++i) {
        add(corners[0], corners[1], 0.25 * i / 6.0);
    }
}

void ellipse(Stroke& stroke, double cx, double cy, double rx, double ry, double rotation,
             double arc = 2.0 * PI, double jitter = 0.0) {
    for (int i = 0; i <= 90; ++i) {
        const double t = arc * i / 90.0;
        const auto p = rotate(rx * std::cos(t), ry * std::sin(t), rotation);
        const double wobble = jitter * std::sin(8.7 * t);
        stroke.addPoint({cx + p.x + wobble, cy + p.y - wobble * 0.5});
    }
}

void expectFiniteVertices(const Stroke& stroke) {
    for (const Point& p: stroke.getPointVector()) {
        EXPECT_TRUE(std::isfinite(p.x));
        EXPECT_TRUE(std::isfinite(p.y));
    }
}

void expectClosed(const Stroke& stroke) {
    ASSERT_GT(stroke.getPointCount(), 3U);
    const auto& pts = stroke.getPointVector();
    EXPECT_NEAR(pts.front().x, pts.back().x, 0.001);
    EXPECT_NEAR(pts.front().y, pts.back().y, 0.001);
}
}  // namespace

TEST(ShapeRecognizerRobustness, SmallNoisyGappedCircleUsesScaleNotSavedLargeThreshold) {
    Stroke input;
    ellipse(input, 60.0, 55.0, 6.5, 6.5, 0.0, 2.0 * PI - 0.36, 0.20);
    ShapeRecognizer recognizer;
    auto result = recognizer.recognizePatterns(&input);
    ASSERT_NE(result, nullptr);
    EXPECT_GE(result->getPointCount(), 24U);
    expectClosed(*result);
    const auto bounds = result->getSnappedBounds();
    EXPECT_NEAR(bounds.width, 13.0, 1.5);
    EXPECT_NEAR(bounds.height, 13.0, 1.5);
}

TEST(ShapeRecognizerRobustness, SmallRotatedRectangleWhoseStrokeStartsMidEdge) {
    std::vector<Point> corners;
    for (const Point& p: {Point{-8.0, -6.0}, Point{8.0, -6.0},
                          Point{8.0, 6.0}, Point{-8.0, 6.0}}) {
        auto rotated = rotate(p.x, p.y, 0.53);
        corners.emplace_back(rotated.x + 40.0, rotated.y + 35.0);
    }
    Stroke input;
    rectangleStartingMidEdge(input, corners);
    ShapeRecognizer recognizer;
    auto result = recognizer.recognizePatterns(&input);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getPointCount(), 5U);
    expectClosed(*result);
}

TEST(ShapeRecognizerRobustness, NoisyGappedRotatedTriangle) {
    std::vector<Point> corners;
    for (const Point& p: {Point{-11.0, -7.0}, Point{12.0, -6.0}, Point{1.0, 10.0}}) {
        auto rotated = rotate(p.x, p.y, -0.39);
        corners.emplace_back(rotated.x + 80.0, rotated.y + 65.0);
    }
    Stroke input;
    polygon(input, corners, 20, true, 0.16);
    ShapeRecognizer recognizer;
    auto result = recognizer.recognizePatterns(&input);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getPointCount(), 4U);
    expectClosed(*result);
}

TEST(ShapeRecognizerRobustness, RotatedGappedEllipseNotMistakenForACircle) {
    Stroke input;
    ellipse(input, 95.0, 95.0, 26.0, 13.0, 0.41, 2.0 * PI - 0.30, 0.28);
    ShapeRecognizer recognizer;
    auto result = recognizer.recognizePatterns(&input);
    ASSERT_NE(result, nullptr);
    EXPECT_GE(result->getPointCount(), 24U);
    expectClosed(*result);
    const auto bounds = result->getSnappedBounds();
    const auto inputBounds = input.getSnappedBounds();
    EXPECT_NEAR(bounds.width, inputBounds.width, 2.5);
    EXPECT_NEAR(bounds.height, inputBounds.height, 2.5);
    EXPECT_GT(bounds.width, 30.0);
    EXPECT_GT(bounds.height, 20.0);
}

TEST(ShapeRecognizerRobustness, GappedTriangleRecognisesAcrossDrawingScales) {
    // The old saved minimum could make the same outline work at 4x and fail at 0.5x.
    // Keep the gap and jitter proportional to the page-space geometry.
    for (double scale: {0.5, 1.0, 4.0}) {
        SCOPED_TRACE(scale);
        Stroke input;
        const std::vector<Point> corners{
                Point{scale * -11.0, scale * -7.0},
                Point{scale * 12.0, scale * -6.0},
                Point{scale * 1.0, scale * 10.0}};
        polygon(input, corners, 20, true, 0.12 * scale);
        ShapeRecognizer recognizer;
        auto result = recognizer.recognizePatterns(&input);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->getPointCount(), 4U) << "Triangle must not become a circle or line";
        expectClosed(*result);
        const auto inputBounds = input.getSnappedBounds();
        const auto resultBounds = result->getSnappedBounds();
        EXPECT_NEAR(resultBounds.width, inputBounds.width, 1.0 * scale);
        EXPECT_NEAR(resultBounds.height, inputBounds.height, 1.0 * scale);
    }
}

TEST(ShapeRecognizerRobustness, OpenCShapedStrokeRemainsInk) {
    // A letter-like open C must not be rounded into a closed O simply because an ellipse fits.
    Stroke input;
    ellipse(input, 45.0, 45.0, 20.0, 20.0, 0.0, 1.65 * PI, 0.12);
    ShapeRecognizer recognizer;
    EXPECT_EQ(recognizer.recognizePatterns(&input), nullptr);
}

TEST(ShapeRecognizerRobustness, HandwritingLikeLoopWithStemAndCrossbarStaysInk) {
    Stroke input;
    // A rounded 'a', then a protruding downstroke and an internal crossbar.
    ellipse(input, 30.0, 26.0, 7.0, 8.0, 0.0, 2.0 * PI - 0.25, 0.15);
    for (int i = 0; i <= 15; ++i) {
        input.addPoint({36.0, 24.0 + i * 1.35});
    }
    for (int i = 0; i <= 12; ++i) {
        input.addPoint({36.0 - i * 0.9, 32.0});
    }
    ShapeRecognizer recognizer;
    EXPECT_EQ(recognizer.recognizePatterns(&input), nullptr);
}

TEST(ShapeRecognizerRobustness, LegacyCircleRejectsNonFinitePoints) {
    // The direct legacy entry point must reject invalid coordinates before
    // bounding-box and inertia calculations, including a NaN at an endpoint.
    for (double invalid: {std::numeric_limits<double>::quiet_NaN(),
                          std::numeric_limits<double>::infinity()}) {
        SCOPED_TRACE(invalid);
        for (int badIndex: {0, 17, 90}) {
            Stroke reference;
            ellipse(reference, 50.0, 50.0, 12.0, 12.0, 0.0);
            Stroke input;
            const auto& points = reference.getPointVector();
            for (size_t i = 0; i < points.size(); ++i) {
                const auto& p = points[i];
                input.addPoint({static_cast<int>(i) == badIndex ? invalid : p.x, p.y});
            }
            EXPECT_EQ(CircleRecognizer::recognize(&input), nullptr);
        }
    }
}

TEST(ShapeRecognizerRobustness, LegacyCircleRejectsOpenCWithoutClosingIt) {
    Stroke input;
    ellipse(input, 45.0, 45.0, 20.0, 20.0, 0.0, 1.65 * PI, 0.12);
    EXPECT_EQ(CircleRecognizer::recognize(&input), nullptr);
}

TEST(ShapeRecognizerRobustness, SmoothLoopRejectsNonFiniteSamples) {
    // Exercise the least-squares entry point directly, not just the legacy
    // inertia path. Invalid points must never reach cached bounds or the fit.
    for (double invalid: {std::numeric_limits<double>::quiet_NaN(),
                          std::numeric_limits<double>::infinity(),
                          -std::numeric_limits<double>::infinity()}) {
        for (int badIndex: {0, 45, 90}) {
            SCOPED_TRACE(badIndex);
            Stroke reference;
            ellipse(reference, 50.0, 50.0, 12.0, 8.0, 0.42);
            Stroke input;
            const auto& points = reference.getPointVector();
            for (size_t i = 0; i < points.size(); ++i) {
                const Point& p = points[i];
                input.addPoint({static_cast<int>(i) == badIndex ? invalid : p.x, p.y});
            }
            EXPECT_EQ(CircleRecognizer::recognizeSmoothLoop(&input), nullptr);
        }
    }
}

TEST(ShapeRecognizerRobustness, SmoothLoopRejectsOverflowingFinitePathLength) {
    // Every input coordinate is finite, but their differences and path
    // length can overflow. Do not feed infinite values into least squares.
    Stroke input;
    for (int i = 0; i < 30; ++i) {
        input.addPoint({i % 2 ? 1e308 : -1e308,
                        i % 3 ? 1e308 : -1e308});
    }
    EXPECT_EQ(CircleRecognizer::recognizeSmoothLoop(&input), nullptr);
}

TEST(ShapeRecognizerRobustness, LargeCircleOutlineHasBoundedPointCount) {
    // The legacy path formerly emitted two points per page-unit radius.
    // Keep large but ordinary classroom diagrams bounded in memory.
    Stroke input;
    ellipse(input, 1500.0, 1500.0, 1000.0, 1000.0, 0.0);
    auto result = CircleRecognizer::recognize(&input);
    ASSERT_NE(result, nullptr);
    EXPECT_GE(result->getPointCount(), 25U);
    EXPECT_LE(result->getPointCount(), 513U);
    expectClosed(*result);
    expectFiniteVertices(*result);
}

TEST(ShapeRecognizerRobustness, LargeSmoothEllipseOutlineHasBoundedPointCount) {
    Stroke input;
    ellipse(input, 1500.0, 1500.0, 1000.0, 600.0, 0.31);
    auto result = CircleRecognizer::recognizeSmoothLoop(&input);
    ASSERT_NE(result, nullptr);
    EXPECT_GE(result->getPointCount(), 37U);
    EXPECT_LE(result->getPointCount(), 129U);
    expectClosed(*result);
    expectFiniteVertices(*result);
}


TEST(ShapeRecognizerRobustness, SmoothLoopRejectsOverflowingFiniteFitMatrix) {
    // Coordinates and path length remain finite, but fourth-power ellipse
    // moments overflow. No NaN outline should escape the recogniser.
    Stroke input;
    ellipse(input, 0.0, 0.0, 1e80, 6e79, 0.31);
    EXPECT_EQ(CircleRecognizer::recognizeSmoothLoop(&input), nullptr);
}

TEST(ShapeRecognizerRobustness, SmoothLoopRejectsOverflowingCircleNormalEquations) {
    // This radius overflows x^2 * x in the circle normal equations,
    // although individual samples, endpoint gap and path length are finite.
    Stroke input;
    ellipse(input, 0.0, 0.0, 1e105, 1e105, 0.0);
    EXPECT_EQ(CircleRecognizer::recognizeSmoothLoop(&input), nullptr);
}

TEST(ShapeRecognizerRobustness, LegacyCircleRejectsOverflowingFiniteInertia) {
    // All samples and the path length are finite. Weighted inertia products
    // may nevertheless overflow; never turn their invalid score into a circle.
    for (double radius: {1e80, 1e105}) {
        SCOPED_TRACE(radius);
        Stroke input;
        ellipse(input, 0.0, 0.0, radius, radius, 0.0);
        EXPECT_EQ(CircleRecognizer::recognize(&input), nullptr);
    }
}

TEST(ShapeRecognizerRobustness, LegacyCircleRejectsNonFiniteSegmentLengths) {
    // The public entry point must reject a stroke whose individual finite
    // coordinates produce infinite segment distances before inertia scoring.
    Stroke input;
    input.addPoint({-1e308, 0.0});
    input.addPoint({1e308, 0.0});
    input.addPoint({-1e308, 0.0});
    EXPECT_EQ(CircleRecognizer::recognize(&input), nullptr);
}

TEST(ShapeRecognizerRobustness, SmoothLoopRejectsOverflowingWindingProducts) {
    // The sampled coordinates and perimeter are finite, but the cross/dot
    // products in the winding calculation overflow at this scale.
    // A non-finite winding must never be treated as a valid single loop.
    Stroke input;
    ellipse(input, 0.0, 0.0, 1e155, 6e154, 0.31);
    EXPECT_EQ(CircleRecognizer::recognizeSmoothLoop(&input), nullptr);
}

// Review (Codex): the polygon fallback recursed per corner candidate, so a dense contour with hundreds of corners could
// recurse deeply and scan quadratically on the UI thread. The simplifier is iterative and gives up after four sides.
TEST(ShapeRecognizerRobustness, aDenseZigZagLoopIsRejectedQuicklyAndLeftAsInk) {
    Stroke input;
    constexpr int points = 4000;
    for (int i = 0; i <= points; ++i) {
        const double angle = 2.0 * M_PI * i / points;
        const double radius = (i % 2 == 0) ? 100.0 : 62.0;  // every sample is a corner
        input.addPoint(Point(300.0 + radius * std::cos(angle), 300.0 + radius * std::sin(angle)));
    }
    ShapeRecognizer recognizer;
    const auto start = std::chrono::steady_clock::now();
    const auto result = recognizer.recognizePatterns(&input);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
    EXPECT_EQ(result, nullptr) << "a saw-tooth loop is not a shape";
    EXPECT_LT(elapsed.count(), 1000) << "bounded work, took " << elapsed.count() << " ms";
}

// Squares, products and fourth powers in the fits assume page-sized numbers. A stroke far outside any page, or with
// coordinates that would overflow them, is left alone before anything computes with it.
TEST(ShapeRecognizerRobustness, strokesFarOutsideAnyPageAreLeftAloneAndLargeOffsetsStillWork) {
    const auto rectangleAt = [](double offsetX, double offsetY) {
        auto stroke = std::make_unique<Stroke>();
        const std::array<std::pair<double, double>, 5> corners{{{0, 0}, {120, 0}, {120, 80}, {0, 80}, {0, 0}}};
        for (size_t c = 0; c + 1 < corners.size(); ++c) {
            for (int step = 0; step < 10; ++step) {
                const double t = step / 10.0;
                stroke->addPoint(Point(offsetX + corners[c].first + (corners[c + 1].first - corners[c].first) * t,
                                       offsetY + corners[c].second + (corners[c + 1].second - corners[c].second) * t));
            }
        }
        stroke->addPoint(Point(offsetX, offsetY));
        return stroke;
    };
    for (const double offset: {5.0e7, 1.0e200}) {
        auto far = rectangleAt(offset, offset);
        ShapeRecognizer recognizer;
        EXPECT_EQ(recognizer.recognizePatterns(far.get()), nullptr) << "offset " << offset;
    }
    auto large = rectangleAt(1.0e5, 1.0e5);  // a very long scroll, still a page-sized number
    ShapeRecognizer recognizer;
    const auto result = recognizer.recognizePatterns(large.get());
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getPointCount(), 5U);
}
