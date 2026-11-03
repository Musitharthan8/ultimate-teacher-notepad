/*
 * UTN Smart Shape regression cases: scale, rotation, pen jitter and open loops.
 * No GTK, settings profile or real user data is touched.
 */

#include <cmath>
#include <vector>

#include <gtest/gtest.h>

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

void ellipse(Stroke& stroke, double cx, double cy, double rx, double ry, double rotation,
             double arc = 2.0 * PI, double jitter = 0.0) {
    for (int i = 0; i <= 90; ++i) {
        const double t = arc * i / 90.0;
        const auto p = rotate(rx * std::cos(t), ry * std::sin(t), rotation);
        const double wobble = jitter * std::sin(8.7 * t);
        stroke.addPoint({cx + p.x + wobble, cy + p.y - wobble * 0.5});
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
    auto result = recognizer.recognizePatterns(&input, 60.0);
    ASSERT_NE(result, nullptr);
    EXPECT_GE(result->getPointCount(), 24U);
    expectClosed(*result);
}

TEST(ShapeRecognizerRobustness, SmallRotatedRectangleWhoseStrokeStartsMidEdge) {
    std::vector<Point> corners;
    for (const Point& p: {Point{-8.0, -6.0}, Point{8.0, -6.0},
                          Point{8.0, 6.0}, Point{-8.0, 6.0}}) {
        auto rotated = rotate(p.x, p.y, 0.53);
        corners.emplace_back(rotated.x + 40.0, rotated.y + 35.0);
    }
    Stroke input;
    polygon(input, corners, 18, true, 0.15);
    ShapeRecognizer recognizer;
    auto result = recognizer.recognizePatterns(&input, 60.0);
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
    auto result = recognizer.recognizePatterns(&input, 60.0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getPointCount(), 4U);
    expectClosed(*result);
}

TEST(ShapeRecognizerRobustness, RotatedGappedEllipseNotMistakenForACircle) {
    Stroke input;
    ellipse(input, 95.0, 95.0, 26.0, 13.0, 0.41, 2.0 * PI - 0.30, 0.28);
    ShapeRecognizer recognizer;
    auto result = recognizer.recognizePatterns(&input, 60.0);
    ASSERT_NE(result, nullptr);
    EXPECT_GE(result->getPointCount(), 24U);
    expectClosed(*result);
    const auto bounds = result->getSnappedBounds();
    EXPECT_GT(bounds.width, 30.0);
    EXPECT_GT(bounds.height, 20.0);
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
    EXPECT_EQ(recognizer.recognizePatterns(&input, 60.0), nullptr);
}
