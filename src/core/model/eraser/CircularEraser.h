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
 * Whether the disc of radius R about c touches the closed rectangle [x, x + w] x [y, y + h] (tangency counts).
 * Used by the object eraser, which removes whole elements by their bounding box.
 */
inline bool circleTouchesRect(Vec c, double R, double x, double y, double w, double h) {
    const double dx = std::max({x - c.x, 0.0, c.x - (x + w)});
    const double dy = std::max({y - c.y, 0.0, c.y - (y + h)});
    return dx * dx + dy * dy <= R * R;
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

/**
 * Partial erasing. The ink of a segment is modelled as a sweep of discs: at parameter t the cross-section is a disc
 * of radius h(t) = h0 + (h1 - h0) t centred on the centreline point S(t). That cross-section touches the eraser disc
 * of radius R about c exactly when |S(t) - c| <= R + h(t). This solves that inequality exactly for t in [0, 1].
 *
 * Returns disjoint closed intervals in increasing order (at most two, since the set may be the complement of an
 * interval when the width changes faster than the segment length).
 */
inline std::vector<std::pair<double, double>> segmentInkDiscIntervals(Vec a, Vec b, Vec c, double R, double h0,
                                                                      double h1) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double fx = a.x - c.x;
    const double fy = a.y - c.y;
    const double H0 = R + h0;
    const double D = h1 - h0;  // slope of the touching radius H(t) = H0 + D t

    // g(t) = |S(t) - c|^2 - H(t)^2 = A t^2 + B t + C. The ink touches the eraser where g(t) <= 0.
    const double A = dx * dx + dy * dy - D * D;
    const double B = 2.0 * (fx * dx + fy * dy - H0 * D);
    const double C = fx * fx + fy * fy - H0 * H0;

    std::vector<std::pair<double, double>> out;
    const double scale = dx * dx + dy * dy + D * D + 1.0;
    if (std::abs(A) <= 1e-12 * scale) {
        // Linear: B t + C <= 0
        if (std::abs(B) <= 1e-12 * scale) {
            if (C <= 0.0) {
                out.emplace_back(0.0, 1.0);
            }
        } else if (B > 0.0) {
            const double r = -C / B;  // t <= r
            if (r >= 0.0) {
                out.emplace_back(0.0, std::min(r, 1.0));
            }
        } else {
            const double r = -C / B;  // t >= r
            if (r <= 1.0) {
                out.emplace_back(std::max(r, 0.0), 1.0);
            }
        }
        return out;
    }

    const double disc = B * B - 4.0 * A * C;
    if (A > 0.0) {
        // Upward parabola: touching exactly between its roots
        if (disc < 0.0) {
            return out;
        }
        const double s = std::sqrt(disc);
        const double r0 = std::max((-B - s) / (2.0 * A), 0.0);
        const double r1 = std::min((-B + s) / (2.0 * A), 1.0);
        if (r0 <= r1) {
            out.emplace_back(r0, r1);
        }
        return out;
    }

    // Downward parabola: touching outside its roots
    if (disc < 0.0) {
        out.emplace_back(0.0, 1.0);
        return out;
    }
    const double s = std::sqrt(disc);
    const double roots0 = (-B - s) / (2.0 * A);
    const double roots1 = (-B + s) / (2.0 * A);
    const double lo = std::min(roots0, roots1);
    const double hi = std::max(roots0, roots1);
    if (lo >= 0.0) {
        out.emplace_back(0.0, std::min(lo, 1.0));
    }
    if (hi <= 1.0) {
        out.emplace_back(std::max(hi, 0.0), 1.0);
    }
    return out;
}

}  // namespace utn::eraser
