#include "RecoSegment.h"

#include <algorithm>  // for max
#include <cmath>

#include "Inertia.h"

auto RecoSegment::calcEdgeIsect(const RecoSegment* r2) const -> std::optional<Point> {
    // Edges closer than about 0.06 degrees to parallel never form a corner of a triangle or box; dividing by their sine
    // would give a point at (or beyond) infinity
    constexpr double MIN_SINE_OF_ANGLE_BETWEEN_EDGES = 1e-3;
    const double denominator = sin(r2->angle - this->angle);
    if (!std::isfinite(denominator) || std::abs(denominator) < MIN_SINE_OF_ANGLE_BETWEEN_EDGES) {
        return std::nullopt;
    }
    double t = (r2->xcenter - this->xcenter) * sin(r2->angle) - (r2->ycenter - this->ycenter) * cos(r2->angle);
    t /= denominator;
    double x = this->xcenter + t * cos(this->angle);
    double y = this->ycenter + t * sin(this->angle);
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return std::nullopt;
    }

    return Point(x, y);
}

/**
 * Find the geometry of a recognized segment
 */
bool RecoSegment::calcSegmentGeometry(const Point* pt, int start, int end, Inertia* s) {
    // The centre divides by the mass: a segment whose points all coincide has none, and has no direction either
    if (!(s->getMass() > 0.0) || !std::isfinite(s->getMass())) {
        return false;
    }
    this->xcenter = s->centerX();
    this->ycenter = s->centerY();
    double a = s->xx();
    double b = s->xy();
    double c = s->yy();

    // max angle for inertia quadratic form solves: tan(2t) = 2b/(a-c)
    this->angle = atan2(2 * b, a - c) / 2;
    // Rounding can leave the sum of the two variances a hair below zero: never take the root of that
    this->radius = sqrt(std::max(0.0, 3 * (a + c)));

    double lmin = 0;
    double lmax = 0;

    for (int i = start; i <= end; i++) {
        double l = (pt[i].x - this->xcenter) * cos(this->angle) + (pt[i].y - this->ycenter) * sin(this->angle);
        if (l < lmin) {
            lmin = l;
        }

        if (l > lmax) {
            lmax = l;
        }
    }

    this->x1 = this->xcenter + lmin * cos(this->angle);
    this->y1 = this->ycenter + lmin * sin(this->angle);
    this->x2 = this->xcenter + lmax * cos(this->angle);
    this->y2 = this->ycenter + lmax * sin(this->angle);

    for (const double value: {xcenter, ycenter, angle, radius, x1, y1, x2, y2}) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return true;
}
