/*
 * Ultimate Teacher Notepad
 *
 * The "Customise tool dock" dialog: reorder, pin, unpin, hide and show the tools on the teacher rail without dragging.
 * Every row has named buttons a keyboard reaches with Tab, so drag and drop is never the only way.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>
#include <vector>

class Control;

namespace utn {

struct DockRow {
    std::string id;
    std::string name;
    bool pinned = false;
    bool hidden = false;
    bool essential = false;  ///< cannot be hidden
    bool canMoveUp = false;
    bool canMoveDown = false;
};

/// What the dialog lists: the tools on the dock in their order, then the hidden ones. Empty when the teacher rail is not
/// the layout in use.
std::vector<DockRow> dockRows(Control* control);

void showDockCustomizeDialog(Control* control);

}  // namespace utn
