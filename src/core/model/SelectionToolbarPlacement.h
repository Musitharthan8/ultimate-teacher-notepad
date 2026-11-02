/*
 * Ultimate Teacher Notepad
 *
 * Where the floating selection toolbar goes, without GTK: near the selection but never on it when there is room,
 * above it by preference, below it near the top edge, always inside the visible canvas.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <optional>
#include <vector>

namespace utn {

/// A rectangle in canvas layout pixels (the coordinates the page views and the scroll position use)
struct ToolbarBox {
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;
};

struct ToolbarPosition {
    double x = 0;  ///< from the left edge of the visible canvas
    double y = 0;  ///< from the top edge of the visible canvas
};

/**
 * Top-left corner of a toolbar of @p width x @p height for @p selection, in coordinates of the visible canvas.
 * @param visible the visible part of the canvas: its x and y are the scroll offset, its size the viewport size
 * @param gap space kept between the selection and the toolbar
 *
 * Centred over the selection and @p gap above it; below it when there is no room above; at the top of the visible
 * canvas when neither fits (a selection taller than the window). Never outside the visible canvas, except that a
 * toolbar larger than it starts at its top-left corner. std::nullopt when no part of the selection is visible.
 * @param avoid other widgets over the canvas (ink dock, banner), in the same coordinates as the result: when one
 * covers the preferred side, the other side of the selection is taken if it fits and is free
 */
std::optional<ToolbarPosition> placeSelectionToolbar(const ToolbarBox& selection, double width, double height,
                                                     const ToolbarBox& visible, double gap,
                                                     const std::vector<ToolbarBox>& avoid = {});

}  // namespace utn
