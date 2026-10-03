/**
 * Xournal++
 *
 * @brief Small structure for a square with a padding
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "control/UtnEraserShape.h"
#include "model/Point.h"
#include "util/Rectangle.h"

struct PaddedBox {
    Point center;
    double halfSize;
    double halfSizeWithPadding;
    UtnEraserShape shape = UtnEraserShape::Square;

    double getInnerHalfWidth() const {
        return shape == UtnEraserShape::Flat ? halfSize * 1.75 : halfSize;
    }

    double getInnerHalfHeight() const {
        return shape == UtnEraserShape::Flat ? halfSize * 0.45 : halfSize;
    }

    double getOuterHalfWidth() const {
        const double padding = halfSizeWithPadding - halfSize;
        return getInnerHalfWidth() + padding;
    }

    double getOuterHalfHeight() const {
        const double padding = halfSizeWithPadding - halfSize;
        return getInnerHalfHeight() + padding;
    }

    xoj::util::Rectangle<double> getInnerRectangle() const {
        const double halfWidth = getInnerHalfWidth();
        const double halfHeight = getInnerHalfHeight();
        return {center.x - halfWidth, center.y - halfHeight, 2 * halfWidth, 2 * halfHeight};
    }

    xoj::util::Rectangle<double> getOuterRectangle() const {
        const double halfWidth = getOuterHalfWidth();
        const double halfHeight = getOuterHalfHeight();
        return {center.x - halfWidth, center.y - halfHeight, 2 * halfWidth, 2 * halfHeight};
    }

    void addToRange(Range& range) const {
        const auto rect = getOuterRectangle();
        range.addPoint(rect.x, rect.y);
        range.addPoint(rect.x + rect.width, rect.y + rect.height);
    }
};
