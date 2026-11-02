/*
 * Ultimate Teacher Notepad
 *
 * The Annotation Bank page of the Feedback popover: one button per entry that puts it onto the current page at once
 * (Control::insertAnnotation), a Remove button beside each, a box to add a comment of one's own and Restore defaults.
 * The entries are rebuilt each time the popover opens, so they show the bank as it is and whether inserting can run
 * right now: not while busy, not while the Blank cover is up, not in Present while its tools are put away.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <gtk/gtk.h>

class Control;

namespace utn {

/// Widget name of the panel, for tests and CSS
inline constexpr const char* ANNOTATION_BANK_NAME = "utnAnnotationBank";

/// Returns: (transfer floating) the panel; it refreshes itself whenever @p popover is shown
GtkWidget* createAnnotationBankPanel(Control* control, GtkPopover* popover);

/// Rebuilds the entry buttons of a panel made by createAnnotationBankPanel and their availability
void refreshAnnotationBankPanel(GtkWidget* panel);

}  // namespace utn
