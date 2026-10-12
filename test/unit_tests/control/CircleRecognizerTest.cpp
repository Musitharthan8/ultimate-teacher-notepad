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
