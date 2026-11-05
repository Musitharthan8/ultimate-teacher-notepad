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
#include "model/Document.h"
#include "model/Layer.h"
#include "model/XojPage.h"
#include "undo/DeleteUndoAction.h"

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
    auto result = recognizer.recognizePatterns(stroke.get());
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
    auto result = recognizer.recognizePatterns(stroke.get());
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getPointCount(), 5U);
}

// Full pipeline coverage for the shapes Smart Shape must not mis-hand to the circle recognizer.
namespace {
// Ellipse with deterministic jitter. The turn fraction < 1 gives a slightly open loop. Uneven spacing warps the
// sample parameter so neighbouring samples are not equally far apart.
std::unique_ptr<Stroke> jitteredEllipse(double rx, double ry, double amplitude, int n, bool unevenSpacing,
                                        double turn = 1.0) {
    auto stroke = std::make_unique<Stroke>();
    for (int i = 0; i <= n; ++i) {
        const double u = static_cast<double>(i) / n;
        const double warped = unevenSpacing ? u + 0.3 * std::sin(2 * M_PI * u) / (2 * M_PI) : u;
        const double t = 2 * M_PI * turn * warped;
        const double j = amplitude * std::sin(7.3 * i + 0.4);
        const double k = amplitude * std::cos(3.1 * i + 1.2);
        stroke->addPoint(Point(100 + (rx + j) * std::cos(t) + 0.5 * k, 100 + (ry + j) * std::sin(t) - 0.3 * k));
    }
    return stroke;
}

// Square outline of the given side with the same deterministic jitter, 8 samples per side.
std::unique_ptr<Stroke> jitteredSquare(double side, double amplitude) {
    auto stroke = std::make_unique<Stroke>();
    const double x = 100 - side / 2;
    const double y = 100 - side / 2;
    constexpr int steps = 8;
    int idx = 0;
    auto add = [&](double px, double py) {
        const int i = idx++;
        const double j = amplitude * std::sin(7.3 * i + 0.4);
        const double k = amplitude * std::cos(3.1 * i + 1.2);
        stroke->addPoint(Point(px + j + 0.4 * k, py + 0.4 * j - k));
    };
    for (int i = 0; i <= steps; ++i) add(x + side * i / steps, y);
    for (int i = 1; i <= steps; ++i) add(x + side, y + side * i / steps);
    for (int i = 1; i <= steps; ++i) add(x + side - side * i / steps, y + side);
    for (int i = 1; i <= steps; ++i) add(x, y + side - side * i / steps);
    return stroke;
}

std::unique_ptr<Stroke> pipelineOutline(const Stroke& input) {
    auto copy = std::make_unique<Stroke>();
    for (const Point& p: input.getPointVector()) {
        copy->addPoint(p);
    }
    ShapeRecognizer recognizer;
    return recognizer.recognizePatterns(copy.get());
}

// A round shape is a fitted outline of at least 25 points that closes on itself and has only finite vertices: a count
// alone would also pass a polygon with many sides or an outline that never closes.
bool isRoundOutline(const Stroke* result) {
    if (!result || result->getPointCount() < 25U) {
        return false;
    }
    const auto& points = result->getPointVector();
    for (const Point& p: points) {
        if (!std::isfinite(p.x) || !std::isfinite(p.y)) {
            return false;
        }
    }
    const auto bounds = result->getSnappedBounds();
    const double diagonal = std::hypot(bounds.width, bounds.height);
    return std::hypot(points.front().x - points.back().x, points.front().y - points.back().y) < 0.02 * diagonal;
}

// Shorter bounding-box side over longer: 1 for a circle, the axis ratio for an ellipse.
double outlineAspect(const Stroke& outline) {
    const auto bounds = outline.getSnappedBounds();
    return std::min(bounds.width, bounds.height) / std::max(bounds.width, bounds.height);
}

// A circle, not just any round outline: an ellipse must not satisfy this.
bool pipelineGivesCircle(const Stroke& input) {
    auto result = pipelineOutline(input);
    return isRoundOutline(result.get()) && outlineAspect(*result) > 0.9;
}
}  // namespace

// Circles with uneven sample spacing (a common pen artefact) must still be circles.
TEST(CircleRecognizer, unevenSpacingCircleIsCircleInPipeline) {
    auto stroke = jitteredEllipse(12.0, 12.0, 1.0, 24, /*unevenSpacing=*/true);
    EXPECT_TRUE(pipelineGivesCircle(*stroke));
}

// A slightly open loop (about 0.9 of a turn) is what a teacher draws for a quick circle.
TEST(CircleRecognizer, slightlyOpenSmallLoopIsCircleInPipeline) {
    auto stroke = jitteredEllipse(12.0, 12.0, 0.5, 30, /*unevenSpacing=*/false, /*turn=*/0.9);
    EXPECT_TRUE(pipelineGivesCircle(*stroke));
}

// A small ellipse is not a circle. It must not be reported as one, whatever the pipeline does with it.
TEST(CircleRecognizer, smallEllipseIsNotReportedAsCircle) {
    auto stroke = jitteredEllipse(14.0, 8.0, 1.0, 30, /*unevenSpacing=*/false);
    EXPECT_FALSE(pipelineGivesCircle(*stroke));
}

// The smooth-loop fit exists to give ellipses a tidy ellipse, so the axis ratio must survive (8/14 = 0.57).
TEST(CircleRecognizer, smallEllipseIsTidiedIntoAnEllipse) {
    auto stroke = jitteredEllipse(14.0, 8.0, 1.0, 30, /*unevenSpacing=*/false);
    auto result = pipelineOutline(*stroke);
    ASSERT_TRUE(isRoundOutline(result.get()));
    EXPECT_NEAR(outlineAspect(*result), 8.0 / 14.0, 0.1);
}

namespace {
// Rounded box |x/a|^8 + |y/b|^8 = 1, started at `start` of the way round and left 3 percent open. Sampling is
// even in angle with small deterministic wobble, like a hurried hand.
std::unique_ptr<Stroke> roundedBox(double halfWidth, double halfHeight, double start, double tilt) {
    auto stroke = std::make_unique<Stroke>();
    constexpr int samples = 80;
    for (int i = 0; i <= samples; ++i) {
        const double t = 2 * M_PI * (start + 0.97 * i / samples);
        const double c = std::cos(t), s = std::sin(t);
        const double r = std::pow(std::pow(std::abs(c), 8) + std::pow(std::abs(s), 8), -1.0 / 8);
        const double wobble = 1.0 + 0.01 * std::sin(5.1 * i + 0.3);
        const double x = halfWidth * r * c * wobble, y = halfHeight * r * s * wobble;
        stroke->addPoint(Point(100 + x * std::cos(tilt) - y * std::sin(tilt), 100 + x * std::sin(tilt) + y * std::cos(tilt)));
    }
    return stroke;
}
}  // namespace

// A rounded box lies within the smooth-loop tolerance of its moment circle or ellipse, so only its straight sides
// tell it from an oval. Boxes must never be tidied into ovals, whatever the aspect, tilt or starting point.
TEST(CircleRecognizer, roundedBoxIsNeverFittedAsACircleOrEllipse) {
    for (double aspect: {1.0, 0.75, 0.5}) {
        for (double tilt: {0.0, 0.2, M_PI / 4}) {
            for (double start: {0.0, 0.13, 0.37, 0.61}) {
                auto box = roundedBox(40.0, 40.0 * aspect, start, tilt);
                EXPECT_EQ(CircleRecognizer::recognizeSmoothLoop(box.get()), nullptr)
                        << "aspect=" << aspect << ", tilt=" << tilt << ", start=" << start;
            }
        }
    }
}

// The same guard must not cost real ovals: a wobbly circle and an ellipse stay recognised at any starting point.
TEST(CircleRecognizer, wobblyOvalsAreStillFittedBesideTheBoxGuard) {
    for (double aspect: {1.0, 0.6}) {
        for (double start: {0.0, 0.29, 0.77}) {
            auto oval = std::make_unique<Stroke>();
            constexpr int samples = 80;
            for (int i = 0; i <= samples; ++i) {
                const double t = 2 * M_PI * (start + 0.97 * i / samples);
                const double wobble = 1.0 + 0.02 * std::sin(5.1 * i + 0.3);
                oval->addPoint(Point(100 + 40.0 * std::cos(t) * wobble, 100 + 40.0 * aspect * std::sin(t) * wobble));
            }
            auto fitted = CircleRecognizer::recognizeSmoothLoop(oval.get());
            ASSERT_NE(fitted, nullptr) << "aspect=" << aspect << ", start=" << start;
            EXPECT_NEAR(outlineAspect(*fitted), aspect, 0.08) << "start=" << start;
        }
    }
}

// Square radial scores overlap the circle threshold, regardless of size.
TEST(CircleRecognizer, largeImperfectSquareIsNotCircleInPipeline) {
    auto stroke = jitteredSquare(60.0, 2.0);
    EXPECT_FALSE(pipelineGivesCircle(*stroke));
}

// The circle allowance applies to small radii only. Check that a near-straight line is still a line.
TEST(CircleRecognizer, nearlyStraightLineStaysLineInPipeline) {
    auto stroke = std::make_unique<Stroke>();
    for (int i = 0; i <= 20; ++i) {
        stroke->addPoint(Point(20.0 + 3.0 * i, 50.0 + 0.4 * std::sin(i)));
    }
    ShapeRecognizer recognizer;
    auto result = recognizer.recognizePatterns(stroke.get());
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->getPointCount(), 2U);
}

TEST(CircleRecognizer, roughSmallSquareIsNotCircleInPipeline) {
    auto stroke = jitteredSquare(18.0, 0.5);
    EXPECT_FALSE(pipelineGivesCircle(*stroke));
}

TEST(CircleRecognizer, rotatedRoughSquaresAreRejectedDirectly) {
    for (double side: {18.0, 60.0, 120.0}) {
        for (double angle: {0.0, 0.23, M_PI / 4}) {
            auto source = jitteredSquare(side, side / 36.0);
            Stroke rotated;
            for (const auto& p: source->getPointVector()) {
                const double x = p.x - 100, y = p.y - 100;
                rotated.addPoint(Point(100 + x * std::cos(angle) - y * std::sin(angle),
                                       100 + x * std::sin(angle) + y * std::cos(angle)));
            }
            EXPECT_EQ(CircleRecognizer::recognize(&rotated), nullptr) << "side=" << side << ", angle=" << angle;
        }
    }
}

// Exercise the geometry used by selection operations and the real delete undo action.
// GUI handle hit testing and Move/ScaleUndoAction dispatch require a separate UI smoke test.
TEST(CircleRecognizer, recognizedShapeSurvivesMoveResizeAndDeleteUndo) {
    auto input = jitteredCircle(9.0, 1.5);
    ShapeRecognizer recognizer;
    auto shape = recognizer.recognizePatterns(input.get());
    ASSERT_NE(shape, nullptr);
    const auto original = shape->getPointVector();
    shape->move(27, -13);
    for (size_t i = 0; i < original.size(); ++i) {
        EXPECT_NEAR(shape->getPoint(i).x, original[i].x + 27, 1e-9);
        EXPECT_NEAR(shape->getPoint(i).y, original[i].y - 13, 1e-9);
    }
    shape->scale(100, 100, 2, 0.5, 0, true);
    for (size_t i = 0; i < original.size(); ++i) {
        EXPECT_NEAR(shape->getPoint(i).x, 100 + 2 * (original[i].x + 27 - 100), 1e-9);
        EXPECT_NEAR(shape->getPoint(i).y, 100 + 0.5 * (original[i].y - 13 - 100), 1e-9);
    }
    shape->scale(100, 100, 0.5, 2, 0, true);
    shape->move(-27, 13);
    for (size_t i = 0; i < original.size(); ++i) {
        EXPECT_NEAR(shape->getPoint(i).x, original[i].x, 1e-9);
        EXPECT_NEAR(shape->getPoint(i).y, original[i].y, 1e-9);
    }
    auto page = std::make_shared<XojPage>(200, 200);
    auto* layer = page->getSelectedLayer();
    auto* identity = shape.get();
    layer->addElement(std::move(shape));
    Document document(nullptr);
    DeleteUndoAction deletion(page, false, &document);
    auto removed = layer->removeElement(identity);
    deletion.addElement(layer, std::move(removed.e), removed.pos);
    EXPECT_TRUE(layer->getElements().empty());
    ASSERT_TRUE(deletion.undo(nullptr));
    ASSERT_EQ(layer->getElements().size(), 1U);
    EXPECT_EQ(layer->getElements()[0].get(), identity);
    ASSERT_TRUE(deletion.redo(nullptr));
    EXPECT_TRUE(layer->getElements().empty());
    ASSERT_TRUE(deletion.undo(nullptr));
    EXPECT_EQ(layer->getElements()[0].get(), identity);
}
