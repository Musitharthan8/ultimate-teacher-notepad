/*
 * Ultimate Teacher Notepad
 *
 * Smart Shape regression: small hand-drawn circles were rejected because pen jitter is a large part of a small
 * radius. Small circles now get a jitter allowance; large circles and non-circles must not change.
 *
 * Based on Xournal++ GPLv2+
 */

#include <algorithm>
#include <cmath>
#include <memory>

#include <gtest/gtest.h>

#include "control/shaperecognizer/CircleRecognizer.h"
#include "control/shaperecognizer/ShapeRecognizer.h"
#include "model/Point.h"
#include "model/Stroke.h"

namespace {
// Deterministic jitter, so the outcome does not depend on a random seed. Points are spaced about 3 units apart,
// like pen samples.
std::unique_ptr<Stroke> jitteredCircle(double radius, double amplitude) {
    auto stroke = std::make_unique<Stroke>();
    const int n = std::max(12, static_cast<int>(2 * M_PI * radius / 3.0));
    for (int i = 0; i <= n; ++i) {
        const double t = 2 * M_PI * i / n;
        const double j = amplitude * std::sin(7.3 * i + 0.4);
        const double k = amplitude * std::cos(3.1 * i + 1.2);
        stroke->addPoint(Point(100 + (radius + j) * std::cos(t) + 0.5 * k, 100 + (radius + j) * std::sin(t) - 0.3 * k));
    }
    return stroke;
}
}  // namespace

// Radius 9 with amplitude 1.5 scores 0.108 without the allowance (threshold 0.10) and 0.060 with it.
TEST(CircleRecognizer, smallHandDrawnCircleIsRecognised) {
    auto stroke = jitteredCircle(9.0, 1.5);
    EXPECT_NE(CircleRecognizer::recognize(stroke.get()), nullptr);
}

TEST(CircleRecognizer, largeCircleIsStillRecognised) {
    auto stroke = jitteredCircle(100.0, 3.0);
    EXPECT_NE(CircleRecognizer::recognize(stroke.get()), nullptr);
}

TEST(CircleRecognizer, straightLineIsNotACircle) {
    auto stroke = std::make_unique<Stroke>();
    for (int i = 0; i <= 20; ++i) {
        stroke->addPoint(Point(20.0 + 3.0 * i, 50.0));
    }
    EXPECT_EQ(CircleRecognizer::recognize(stroke.get()), nullptr);
}


// The public Smart Shape pipeline tries rectangles and other polygons before it
// reaches CircleRecognizer. A direct circle unit test alone cannot prove the user
// actually receives a circle rather than a small polygon or no recognised shape.
TEST(CircleRecognizer, smallCircleSurvivesFullSmartShapePipeline) {
    auto stroke = jitteredCircle(9.0, 1.5);
    ShapeRecognizer recognizer;
    auto result = recognizer.recognizePatterns(stroke.get(), 0.0);
    ASSERT_NE(result, nullptr);
    // CircleRecognizer's fitted output uses >=24 segments; rectangle/triangle have 4/3.
    EXPECT_GE(result->getPointCount(), 25U);
}

// Small boxes must continue to prefer rectangle recognition over a circle,
// even with the allowance for small-circle jitter.
TEST(CircleRecognizer, smallBoxRemainsRectangleInSmartShapePipeline) {
    auto stroke = std::make_unique<Stroke>();
    constexpr double left = 91.0, top = 91.0, side = 18.0;
    constexpr int steps = 8;
    for (int i = 0; i <= steps; ++i) {
        stroke->addPoint(Point(left + side * i / steps, top));
    }
    for (int i = 1; i <= steps; ++i) {
        stroke->addPoint(Point(left + side, top + side * i / steps));
    }
    for (int i = 1; i <= steps; ++i) {
        stroke->addPoint(Point(left + side - side * i / steps, top + side));
    }
    for (int i = 1; i <= steps; ++i) {
        stroke->addPoint(Point(left, top + side - side * i / steps));
    }
    ShapeRecognizer recognizer;
    auto result = recognizer.recognizePatterns(stroke.get(), 0.0);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getPointCount(), 5U);
}
