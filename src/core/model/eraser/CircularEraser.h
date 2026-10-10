/*
 * Ultimate Teacher Notepad
 *
 * Exact geometry for a circular eraser. Pure functions with no GTK or document state, so they can be unit tested.
 *
 * Model: a stroke is a centreline polyline. Each centreline point carries a half width h, so its ink lies
 * within h of the centreline. The eraser is a disc of radius r.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace utn::eraser {

struct Vec {
    double x = 0.0;
    double y = 0.0;
};

/// A centreline point with the half width of the ink around it
struct Knot {
    Vec p;
    double halfWidth = 0.0;
};

/// Distance from point q to the segment [a, b]
inline double distancePointSegment(Vec q, Vec a, Vec b) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double len2 = dx * dx + dy * dy;
    double t = 0.0;
    if (len2 > 0.0) {
        t = std::clamp(((q.x - a.x) * dx + (q.y - a.y) * dy) / len2, 0.0, 1.0);
    }
    const double px = a.x + t * dx - q.x;
    const double py = a.y + t * dy - q.y;
    return std::hypot(px, py);
}

/**
 * Whole-stroke hit test. A stroke is hit when some centreline segment comes within r + h of the eraser centre,
 * where h is the larger half width of the segment's two knots. A single knot is tested as a point.
 * Touching exactly (distance == r + h) counts as a hit.
 */
inline bool circleHitsStroke(Vec centre, double radius, const std::vector<Knot>& knots) {
    if (knots.empty()) {
        return false;
    }
    if (knots.size() == 1) {
        return distancePointSegment(centre, knots[0].p, knots[0].p) <= radius + knots[0].halfWidth;
    }
    for (std::size_t k = 0; k + 1 < knots.size(); ++k) {
        const double h = std::max(knots[k].halfWidth, knots[k + 1].halfWidth);
        if (distancePointSegment(centre, knots[k].p, knots[k + 1].p) <= radius + h) {
            return true;
        }
    }
    return false;
}

/**
 * Parameters t in [0, 1] for which S(t) = a + t (b - a) lies inside the disc of radius R about c.
 * Returns the closed interval [t0, t1], or nothing when the segment misses the disc.
 */
inline std::optional<std::pair<double, double>> segmentDiscInterval(Vec a, Vec b, Vec c, double R) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double fx = a.x - c.x;
    const double fy = a.y - c.y;
    const double A = dx * dx + dy * dy;
    const double B = 2.0 * (fx * dx + fy * dy);
    const double C = fx * fx + fy * fy - R * R;

    if (A <= 0.0) {
        return C <= 0.0 ? std::optional(std::pair(0.0, 1.0)) : std::nullopt;
    }
    const double disc = B * B - 4.0 * A * C;
    if (disc < 0.0) {
        return std::nullopt;
    }
    const double s = std::sqrt(disc);
    double t0 = (-B - s) / (2.0 * A);
    double t1 = (-B + s) / (2.0 * A);
    t0 = std::max(t0, 0.0);
    t1 = std::min(t1, 1.0);
    if (t0 > t1) {
        return std::nullopt;
    }
    return std::pair(t0, t1);
}

}  // namespace utn::eraser
