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

class Control;

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

/// Change the text of an entry made by appendMenuButton and keep it left-aligned. GTK3 rebuilds the label child when
/// the text changes, which would otherwise centre it.
void setMenuButtonLabel(GtkWidget* button, const char* label);

/// Pin the full settings inspector beside the worksheet from a tool popover; never changes the active tool.
GtkWidget* appendPinSettingsButton(GtkBox* panel, GtkPopover* popover, Control* control);

/// Name read by screen readers, for buttons whose visible content is an icon, a colour or a single letter
void setAccessibleName(GtkWidget* widget, const char* name);

/**
 * Append the keyboard shortcut of the button's action to its tooltip, e.g. "Pen (Ctrl+Alt+P)".
 * The shortcut is looked up when the tooltip opens, so it follows the user's key bindings and any later
 * set_tooltip_text(). The button needs an action name; without a shortcut the tooltip is unchanged.
 */
void showAcceleratorInTooltip(GtkWidget* button);

/**
 * Change a toggle's state without it counting as a user click.
 * GTK3 emits "clicked" for programmatic gtk_toggle_button_set_active(); handlers must check isSyncing() so that
 * keeping a button in step with the active tool never re-selects that tool.
 */
void syncToggle(GtkToggleButton* toggle, bool active);
bool isSyncing(GtkWidget* widget);

/// An active tool opens its existing options on primary press or Enter/Space, without reactivating its action.
void bindActiveToolOptions(GtkToggleButton* tool, GtkMenuButton* options);

/// Small line drawings for the properties panel. They use the button's own text colour, so they follow the light,
/// dark and High Contrast themes and a selected button's colour.
enum class Glyph { Freehand, Rectangle, Ellipse, Line, Arrow, SmartShape, Weight };

/**
 * A decorative drawing of @p glyph. For Glyph::Weight, @p weight is the stroke thickness in pixels, so a size button
 * shows how heavy that size writes. Not read by screen readers: the button that holds it carries the name.
 * Returns: (transfer floating)
 */
GtkWidget* createGlyph(Glyph glyph, double weight = 0.0);

/// A round colour swatch button for @p colour, named for screen readers
GtkWidget* createColourSwatch(Color colour, const char* name);
void setSwatchSelected(GtkWidget* swatch, bool selected);

}  // namespace utn
