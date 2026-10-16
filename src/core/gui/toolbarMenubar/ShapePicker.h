/*
 * Ultimate Teacher Notepad
 *
 * The Pen's Freehand / shape radio group in the properties panel.
 *
 * Building the group must never report a choice: GTK "clicks" the previously active radio while a later radio is
 * switched on, and a handler that answered that click used to switch the Pen back to Freehand and rebuild the panel
 * from inside its own construction (duplicated controls, Rectangle shown but Freehand drawn).
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <array>

#include <gtk/gtk.h>

#include "control/ToolEnums.h"
#include "util/i18n.h"  // for N_

namespace utn {

struct ShapeChoice {
    DrawingType type;
    const char* label;  ///< untranslated
    const char* hint;   ///< untranslated
};

inline constexpr std::array<ShapeChoice, 6> SHAPE_CHOICES{{
        {DRAWING_TYPE_DEFAULT, N_("Freehand"), N_("Write freehand with the pen")},
        {DRAWING_TYPE_RECTANGLE, N_("Rectangle"), N_("Draw a rectangle")},
        {DRAWING_TYPE_ELLIPSE, N_("Ellipse"), N_("Draw an ellipse or circle")},
        {DRAWING_TYPE_LINE, N_("Line"), N_("Draw a straight line")},
        {DRAWING_TYPE_ARROW, N_("Arrow"), N_("Draw an arrow")},
        {DRAWING_TYPE_SHAPE_RECOGNIZER, N_("Smart Shape"), N_("Draw a rough circle or box and let UTN tidy it")},
}};

/// Called once for each shape the user switches on; never while the picker is being built.
using ShapeChosen = void (*)(DrawingType type, gpointer data);

/**
 * Two-column radio grid with @p current switched on (Freehand when @p current is not one of the choices).
 * Returns: (transfer floating)
 */
GtkWidget* createShapePicker(DrawingType current, ShapeChosen chosen, gpointer data);

/// The drawing type a picker button stands for
DrawingType shapePickerButtonType(GtkWidget* button);

}  // namespace utn
