#include "CircleRecognizer.h"

#include <algorithm>  // for max
#include <cmath>     // for hypot, cos, fabs, sin, M_PI
#include <iterator>  // for begin, end, next
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
        if (squareError < 0.5 * circleError) {
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
