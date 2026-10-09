/*
 * Ultimate Teacher Notepad
 *
 * Small shared building blocks for the teacher interface, so menus and the properties bar look and behave alike.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <gtk/gtk.h>

#include "util/Color.h"

namespace utn {

/// A popover with the standard UTN menu panel inside it
struct PopoverPanel {
    GtkPopover* popover;
    GtkBox* panel;
};
PopoverPanel createPopoverPanel(int spacing = 2);

/// A small bold section title inside a popover panel
void appendPopoverHeading(GtkBox* panel, const char* text);

/// A menu entry: left-aligned label, optional explanation as tooltip. Closes the popover when clicked.
GtkWidget* appendMenuButton(GtkBox* panel, GtkPopover* popover, const char* label, const char* hint = nullptr);

/// Name read by screen readers, for buttons whose visible content is an icon, a colour or a single letter
void setAccessibleName(GtkWidget* widget, const char* name);

/**
 * Change a toggle's state without it counting as a user click.
 * GTK3 emits "clicked" for programmatic gtk_toggle_button_set_active(); handlers must check isSyncing() so that
 * keeping a button in step with the active tool never re-selects that tool.
 */
void syncToggle(GtkToggleButton* toggle, bool active);
bool isSyncing(GtkWidget* widget);

/// A round colour swatch button for @p colour, named for screen readers
GtkWidget* createColourSwatch(Color colour, const char* name);
void setSwatchSelected(GtkWidget* swatch, bool selected);

}  // namespace utn
