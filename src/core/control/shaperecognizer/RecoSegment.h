/*
 * Xournal++
 *
 * Part of the Xournal shape recognizer
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */
#pragma once

#include <optional>

#include "model/Point.h"

class Stroke;
class Inertia;

struct RecoSegment final {
    /**
     * Where this edge meets the edge `r2`. Nothing when the two are (nearly) parallel, which has no intersection to
     * divide out, or when the result is not a finite point.
     */
    std::optional<Point> calcEdgeIsect(const RecoSegment* r2) const;

    /**
     * Find the geometry of a recognized segment.
     * @return false when there is no sensible fit: the points carry no mass (they all coincide), or the fitted centre,
     *         angle, radius or ends are not finite. The segment must then not be used.
     */
    bool calcSegmentGeometry(const Point* pt, int start, int end, Inertia* s);

    Stroke* stroke{nullptr};
    int startpt{0};
    int endpt{0};

    double xcenter{0};
    double ycenter{0};
    double angle{0};
    double radius{0};

    double x1{0};
    double y1{0};
    double x2{0};
    double y2{0};

    bool reversed{};
};
