#include "SelectionToolbarPlacement.h"

#include <algorithm>  // for max, min, any_of, find_if_not

namespace utn {

std::optional<ToolbarPosition> placeSelectionToolbar(const ToolbarBox& selection, double width, double height,
                                                     const ToolbarBox& visible, double gap,
                                                     const std::vector<ToolbarBox>& avoid) {
    // The selection relative to the visible canvas
    const double left = selection.x - visible.x;
    const double top = selection.y - visible.y;
    const double right = left + selection.width;
    const double bottom = top + selection.height;
    if (right < 0 || bottom < 0 || left > visible.width || top > visible.height) {
        return std::nullopt;  // nothing to point at: the toolbar would float over unrelated content
    }

    ToolbarPosition at;
    const double maxX = std::max(0.0, visible.width - width);
    at.x = std::clamp(left + selection.width / 2 - width / 2, 0.0, maxX);

    const double above = top - gap - height;
    const double below = bottom + gap;
    std::vector<double> sides;  // the sides that fit, preferred first
    if (above >= 0) {
        sides.push_back(above);
    }
    if (below + height <= visible.height) {
        sides.push_back(below);
    }
    if (sides.empty()) {
        at.y = 0;  // no room on either side: stay inside the window, at the top
        return at;
    }
    const auto covered = [&](double y) {
        return std::any_of(avoid.begin(), avoid.end(), [&](const ToolbarBox& o) {
            return at.x < o.x + o.width && o.x < at.x + width && y < o.y + o.height && o.y < y + height;
        });
    };
    // Another overlay widget on the preferred side: take the other side if it is free, else keep the preferred one
    const auto free = std::find_if_not(sides.begin(), sides.end(), covered);
    at.y = free != sides.end() ? *free : sides.front();
    return at;
}

}  // namespace utn
