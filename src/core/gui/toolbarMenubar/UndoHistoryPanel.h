/*
 * Ultimate Teacher Notepad
 *
 * The History page of the Classroom popover in the tool dock: the recent steps of the existing undo history
 * (UndoRedoHandler), newest first, with the current point marked between what Redo would bring back and what Undo would
 * take away, Undo and Redo buttons, and going back (or forward) to a listed step. Going to a step is nothing but the
 * ordinary Undo (Redo) action fired once per main loop turn, one step at a time, stopping at the first step that cannot
 * run. It keeps up with the history through an UndoRedoListener. The steps are never listed under the Blank cover or in
 * Present while its tools are put away.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>

#include <gtk/gtk.h>

#include "model/UndoHistory.h"

class Control;

namespace utn {

/// Widget name of the panel, for tests and CSS
inline constexpr const char* UNDO_HISTORY_NAME = "utnUndoHistory";
/// Widget name of the Classroom menu button in the tool dock, whose popover holds the panel
inline constexpr const char* CLASSROOM_MENU_NAME = "utnClassroomMenu";

/// Returns: (transfer floating) the panel; it refreshes itself whenever @p popover is shown and the history changes
GtkWidget* createUndoHistoryPanel(Control* control, GtkPopover* popover);

/// Rebuilds the list of a panel made by createUndoHistoryPanel at once
void refreshUndoHistoryPanel(GtkWidget* panel);

/// True while the panel is going back or forward to a step
bool isUndoHistoryJumping(GtkWidget* panel);

/// Whether the history may be shown and used now
HistoryGate undoHistoryGate(Control* control);

/// Why the Command Palette cannot open the panel now; empty when it can
std::string undoHistoryOpenReason(Control* control);

/// Opens the Classroom popover on its History page, with the keyboard on what Undo would undo. False if it cannot.
bool openUndoHistory(Control* control);

}  // namespace utn
