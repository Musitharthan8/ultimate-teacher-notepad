#include "CircleRecognizer.h"

#include <algorithm>  // for max
#include <cmath>     // for hypot, cos, fabs, sin, M_PI
#include <iterator>  // for begin, end, next
#include <limits>
#include <memory>
#include <vector>    // for vector

#include "model/Point.h"   // for Point
#include "model/Stroke.h"  // for Stroke

#include "Inertia.h"                // for Inertia
#include "ShapeRecognizerConfig.h"  // for RDEBUG, CIRCLE_MAX_SCORE, CIRCLE_...

namespace {
// Isotropic inertia and a small radial error also describe a square. Compare
// against a square outline before accepting the circle fallback: polygon
// segmentation may have declined a perfectly recognizable rough box.
bool fitsSquareBetter(const Stroke& stroke, const Inertia& inertia) {
    struct Sample {
        double x, y, weight;
    };
    std::vector<Sample> samples;
    const auto& points = stroke.getPointVector();
    double circleError = 0;
    for (size_t i = 1; i < points.size(); ++i) {
        const auto& p = points[i - 1];
        const double weight = hypot(points[i].x - p.x, points[i].y - p.y);
        const double x = p.x - inertia.centerX(), y = p.y - inertia.centerY();
        samples.push_back({x, y, weight});
        // Compare raw geometric residuals, without the small-circle jitter allowance.
        circleError += weight * std::abs(hypot(x, y) - inertia.rad());
    }

    // A square repeats every quarter turn. A 2.5 degree search also covers
    // slanted boxes without depending on where sampling started on the outline.
    for (int angle = 0; angle < 36; ++angle) {
        const double theta = angle * M_PI / 72;
        const double c = cos(theta), s = sin(theta);
        auto distance = [c, s](const Sample& p) {
            return std::max(std::abs(c * p.x + s * p.y), std::abs(-s * p.x + c * p.y));
        };
        double halfSide = 0;
        for (const auto& p: samples) {
            halfSide += p.weight * distance(p);
        }
        halfSide /= inertia.getMass();
        double squareError = 0;
        for (const auto& p: samples) {
            squareError += p.weight * std::abs(distance(p) - halfSide);
        }
        // Require strong evidence for straight sides, so jittery circular loops
        // keep the existing circle tolerance. Both errors are length weighted.
        if (squareError < 0.9 * circleError) {
            return true;
        }
    }
    return false;
}
}  // namespace

/**
 * Create circle stroke for inertia
 */
auto CircleRecognizer::makeCircleShape(Stroke* originalStroke, Inertia& inertia) -> std::unique_ptr<Stroke> {
    int npts = static_cast<int>(2 * inertia.rad());
    if (npts < 24) {
        npts = 24;  // min. number of points
    }

    auto s = std::make_unique<Stroke>();
    s->applyStyleFrom(originalStroke);

    for (int i = 0; i <= npts; i++) {
        double x = inertia.centerX() + inertia.rad() * cos((2 * M_PI * i) / npts);
        double y = inertia.centerY() + inertia.rad() * sin((2 * M_PI * i) / npts);
        s->addPoint(Point(x, y));
    }

    return s;
}

/**
 *  Test if we have a circle; inertia has been precomputed by caller
 */
auto CircleRecognizer::scoreCircle(Stroke* s, Inertia& inertia) -> double {
    double r0 = inertia.rad();
    double divisor = inertia.getMass() * r0;

    // Todo: test: std::abs(divisor) <= std::numeric_limits<double>::epsilon()
    if (divisor == 0) {
        return 0;
    }

    double sum = 0.0;
    double x0 = inertia.centerX();
    double y0 = inertia.centerY();

    // Pen jitter is a fixed number of page units, so for a small circle it is a large part of the radius and a
    // hand-drawn circle could fail the score. Forgive a small deviation for small circles only. Larger circles are
    // scored exactly as before, so the accuracy of large diagrams is unchanged.
    const double allowance = r0 < CIRCLE_SMALL_RADIUS ? CIRCLE_JITTER_ALLOWANCE : 0.0;

    auto const& pv = s->getPointVector();
    for (auto pt_1st = begin(pv), pt_2nd = std::next(pt_1st), p_end_i = end(pv); pt_1st != p_end_i && pt_2nd != p_end_i;
         ++pt_2nd, ++pt_1st) {
        double dm = hypot(pt_2nd->x - pt_1st->x, pt_2nd->y - pt_1st->y);
        double deltar = std::max(0.0, std::abs(hypot(pt_1st->x - x0, pt_1st->y - y0) - r0) - allowance);
        sum += dm * deltar;
    }

    return sum / (divisor);
}

auto CircleRecognizer::recognize(Stroke* stroke) -> std::unique_ptr<Stroke> {
    Inertia s;
    s.calc(stroke->getPointVector());
    RDEBUG("Mass=%.0f, Center=(%.1f,%.1f), I=(%.0f,%.0f, %.0f), Rad=%.2f, Det=%.4f", s.getMass(), s.centerX(),
           s.centerY(), s.xx(), s.yy(), s.xy(), s.rad(), s.det());

    if (s.det() > CIRCLE_MIN_DET) {
        double score = CircleRecognizer::scoreCircle(stroke, s);
        RDEBUG("Circle score: %.2f", score);
        if (score < CIRCLE_MAX_SCORE && !fitsSquareBetter(*stroke, s)) {
            return CircleRecognizer::makeCircleShape(stroke, s);
        }
    }

    return nullptr;
}


namespace {
// Ordinary page units, not a user-specific recognition sensitivity setting.
constexpr double MIN_LOOP_DIAGONAL = 8.0;
constexpr double MAX_GAP_FRACTION = 0.14;  // round loops need tighter closure than sharp-corner polygons
constexpr double CIRCLE_RELATIVE_RMS = 0.09;
constexpr double ELLIPSE_RELATIVE_RMS = 0.095;

// Partial pivoting avoids introducing a geometry-library dependency. The caller centres its inputs.
bool solveThreeByThree(double augmented[3][4], double result[3]) {
    for (int col = 0; col < 3; ++col) {
        int pivot = col;
        for (int row = col + 1; row < 3; ++row) {
            if (std::abs(augmented[row][col]) > std::abs(augmented[pivot][col])) {
                pivot = row;
            }
        }
        if (std::abs(augmented[pivot][col]) < 1e-10) {
            return false;
        }
        for (int j = col; j < 4; ++j) {
            std::swap(augmented[pivot][j], augmented[col][j]);
        }
        const double divisor = augmented[col][col];
        for (int j = col; j < 4; ++j) {
            augmented[col][j] /= divisor;
        }
        for (int row = 0; row < 3; ++row) {
            if (row == col) {
                continue;
            }
            const double multiple = augmented[row][col];
            for (int j = col; j < 4; ++j) {
                augmented[row][j] -= multiple * augmented[col][j];
            }
        }
    }
    for (int i = 0; i < 3; ++i) {
        result[i] = augmented[i][3];
    }
    return true;
}

auto smoothOutline(Stroke* original, double cx, double cy, double rx, double ry, double angle)
        -> std::unique_ptr<Stroke> {
    const int segments = std::clamp(static_cast<int>(2.0 * std::max(rx, ry)), 36, 128);
    const double cosine = std::cos(angle), sine = std::sin(angle);
    auto fitted = std::make_unique<Stroke>();
    fitted->applyStyleFrom(original);
    for (int i = 0; i <= segments; ++i) {
        const double phase = 2.0 * M_PI * i / segments;
        const double x = rx * std::cos(phase), y = ry * std::sin(phase);
        fitted->addPoint(Point(cx + cosine * x - sine * y, cy + sine * x + cosine * y));
    }
    return fitted;
}
}  // namespace

auto CircleRecognizer::recognizeSmoothLoop(Stroke* stroke) -> std::unique_ptr<Stroke> {
    const auto& points = stroke->getPointVector();
    if (points.size() < 12) {
        return nullptr;
    }
    const auto bounds = stroke->getSnappedBounds();
    const double diagonal = std::hypot(bounds.width, bounds.height);
    if (!std::isfinite(diagonal) || diagonal < MIN_LOOP_DIAGONAL ||
        std::min(bounds.width, bounds.height) < 4.0) {
        return nullptr;
    }

    double length = 0.0, meanX = 0.0, meanY = 0.0;
    for (size_t i = 0; i < points.size(); ++i) {
        const Point& p = points[i];
        if (!std::isfinite(p.x) || !std::isfinite(p.y)) {
            return nullptr;
        }
        meanX += p.x;
        meanY += p.y;
        if (i > 0) {
            length += std::hypot(p.x - points[i - 1].x, p.y - points[i - 1].y);
        }
    }
    const double gap = std::hypot(points.back().x - points.front().x, points.back().y - points.front().y);
    if (length <= 0 || gap > MAX_GAP_FRACTION * length || gap > 0.45 * diagonal) {
        return nullptr;
    }
    meanX /= points.size();
    meanY /= points.size();

    // Reject scribbles and repeated loops: a convex round contour encircles its own sampled centre once.
    double winding = 0.0;
    for (size_t i = 0; i < points.size(); ++i) {
        const Point& a = points[i];
        const Point& b = points[(i + 1) % points.size()];
        const double ax = a.x - meanX, ay = a.y - meanY;
        const double bx = b.x - meanX, by = b.y - meanY;
        winding += std::atan2(ax * by - ay * bx, ax * bx + ay * by);
    }
    if (std::abs(std::abs(winding) - 2.0 * M_PI) > 0.7) {
        return nullptr;
    }

    // Kasa least-squares circle: x^2 + y^2 = 2*cx*x + 2*cy*y + c.
    // Work relative to the sample mean so very large page coordinates do not destabilise the normal equations.
    double augmented[3][4]{};
    for (const Point& p : points) {
        const double x = p.x - meanX, y = p.y - meanY;
        const double terms[3]{2.0 * x, 2.0 * y, 1.0};
        const double squaredRadius = x * x + y * y;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                augmented[i][j] += terms[i] * terms[j];
            }
            augmented[i][3] += terms[i] * squaredRadius;
        }
    }
    double coefficients[3]{};
    if (solveThreeByThree(augmented, coefficients)) {
        const double cx = meanX + coefficients[0];
        const double cy = meanY + coefficients[1];
        const double radiusSquared = coefficients[2] + coefficients[0] * coefficients[0] +
                                     coefficients[1] * coefficients[1];
        if (radiusSquared > 9.0 && std::isfinite(radiusSquared)) {
            const double radius = std::sqrt(radiusSquared);
            double relativeSquareError = 0.0;
            for (const Point& p : points) {
                const double delta = (std::hypot(p.x - cx, p.y - cy) - radius) / radius;
                relativeSquareError += delta * delta;
            }
            const double rms = std::sqrt(relativeSquareError / points.size());
            if (rms <= CIRCLE_RELATIVE_RMS) {
                return smoothOutline(stroke, cx, cy, radius, radius, 0.0);
            }
        }
    }

    // Ellipse: PCA supplies a rotation; positive least-squares coefficients for
    // A*u^2 + B*v^2 = 1 supply the axes. Evaluate geometric, not algebraic, relative residual.
    double sumXX = 0.0, sumYY = 0.0, sumXY = 0.0;
    for (const Point& p : points) {
        const double x = p.x - meanX, y = p.y - meanY;
        sumXX += x * x;
        sumYY += y * y;
        sumXY += x * y;
    }
    const double angle = 0.5 * std::atan2(2.0 * sumXY, sumXX - sumYY);
    const double cosine = std::cos(angle), sine = std::sin(angle);
    double sumU4 = 0.0, sumV4 = 0.0, sumU2V2 = 0.0, sumU2 = 0.0, sumV2 = 0.0;
    for (const Point& p : points) {
        const double x = p.x - meanX, y = p.y - meanY;
        const double u = cosine * x + sine * y;
        const double v = -sine * x + cosine * y;
        const double u2 = u * u, v2 = v * v;
        sumU4 += u2 * u2;
        sumV4 += v2 * v2;
        sumU2V2 += u2 * v2;
        sumU2 += u2;
        sumV2 += v2;
    }
    const double determinant = sumU4 * sumV4 - sumU2V2 * sumU2V2;
    if (determinant <= 1e-12 * std::max(1.0, sumU4 * sumV4)) {
        return nullptr;
    }
    const double aInverseSquared = (sumU2 * sumV4 - sumV2 * sumU2V2) / determinant;
    const double bInverseSquared = (sumV2 * sumU4 - sumU2 * sumU2V2) / determinant;
    if (aInverseSquared <= 0 || bInverseSquared <= 0) {
        return nullptr;
    }
    const double rx = 1.0 / std::sqrt(aInverseSquared);
    const double ry = 1.0 / std::sqrt(bInverseSquared);
    if (!std::isfinite(rx) || !std::isfinite(ry) || std::min(rx, ry) < 3.0 ||
        std::min(rx, ry) / std::max(rx, ry) < 0.25) {
        return nullptr;
    }
    double relativeSquareError = 0.0;
    for (const Point& p : points) {
        const double x = p.x - meanX, y = p.y - meanY;
        const double u = cosine * x + sine * y;
        const double v = -sine * x + cosine * y;
        const double radialDistance = std::sqrt(aInverseSquared * u * u + bInverseSquared * v * v);
        const double delta = radialDistance - 1.0;
        relativeSquareError += delta * delta;
    }
    const double rms = std::sqrt(relativeSquareError / points.size());
    return rms <= ELLIPSE_RELATIVE_RMS ? smoothOutline(stroke, meanX, meanY, rx, ry, angle) : nullptr;
}
