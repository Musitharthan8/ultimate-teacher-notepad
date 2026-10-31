#include "SelectionToolbarPlacement.h"

#include <algorithm>  // for max, min

namespace utn {

std::optional<ToolbarPosition> placeSelectionToolbar(const ToolbarBox& selection, double width, double height,
                                                     const ToolbarBox& visible, double gap) {
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
    if (above >= 0) {
        at.y = above;
    } else if (below + height <= visible.height) {
        at.y = below;
    } else {
        at.y = 0;  // no room on either side: stay inside the window, at the top
    }
    return at;
}

}  // namespace utn
