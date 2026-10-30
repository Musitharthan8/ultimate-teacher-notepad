/*
 * Ultimate Teacher Notepad
 *
 * The Presets menu of the pen and highlighter settings: apply a favourite in one click, save the current setup as a
 * new favourite, rename or delete favourites, bring the built-ins back.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>
#include <vector>

#include <gtk/gtk.h>

class Control;

namespace utn {

struct PresetMenuEntry {
    std::string id;
    std::string name;
    bool active = false;  ///< the tool is in exactly this preset's state right now
};

/// What the menu lists, in order. `active` comes from the tool itself, so an entry is marked only when applied.
std::vector<PresetMenuEntry> presetMenuEntries(const Control* control);

/// The "Presets" menu button. Its list is built each time it opens, so it always shows the saved presets and which one
/// the tool is in. Returns: (transfer floating)
GtkWidget* createPresetsMenuButton(Control* control);

}  // namespace utn
