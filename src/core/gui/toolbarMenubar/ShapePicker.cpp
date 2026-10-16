/*
 * Ultimate Teacher Notepad
 *
 * The Pen's Freehand / shape radio group
 *
 * Based on Xournal++ GPLv2+
 */

#include "ShapePicker.h"

#include <vector>

#include "util/gtk4_helper.h"

#include "UtnWidgets.h"

namespace utn {

namespace {
constexpr const char* TYPE_KEY = "utn-drawing-type";
constexpr const char* CHOSEN_KEY = "utn-shape-chosen";
constexpr const char* DATA_KEY = "utn-shape-data";
}  // namespace

DrawingType shapePickerButtonType(GtkWidget* button) {
    return static_cast<DrawingType>(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), TYPE_KEY)));
}

GtkWidget* createShapePicker(DrawingType current, ShapeChosen chosen, gpointer data) {
    GtkGrid* grid = GTK_GRID(gtk_grid_new());
    gtk_widget_add_css_class(GTK_WIDGET(grid), "utn-shape-picker");
    gtk_grid_set_row_spacing(grid, 4);
    gtk_grid_set_column_spacing(grid, 4);
    gtk_grid_set_column_homogeneous(grid, true);

    std::vector<GtkWidget*> buttons;
    GtkWidget* first = nullptr;
    GtkWidget* active = nullptr;
    for (const auto& choice: SHAPE_CHOICES) {
        GtkWidget* button = first ? gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(first), _(choice.label))
                                  : gtk_radio_button_new_with_label(nullptr, _(choice.label));
        first = first ? first : button;
        gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), false);
        gtk_widget_add_css_class(button, "utn-shape-mode");
        gtk_widget_set_focus_on_click(button, false);  // keyboard users can still Tab to it
        gtk_widget_set_tooltip_text(button, _(choice.hint));
        utn::setAccessibleName(button, _(choice.label));
        g_object_set_data(G_OBJECT(button), TYPE_KEY, GINT_TO_POINTER(static_cast<int>(choice.type)));
        if (choice.type == current) {
            active = button;
        }
        const int index = static_cast<int>(buttons.size());
        gtk_grid_attach(grid, button, index % 2, index / 2, 1, 1);
        buttons.push_back(button);
    }

    // Set the state before any handler exists: switching a radio on "clicks" the previous one
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(active ? active : first), true);

    for (GtkWidget* button: buttons) {
        g_object_set_data(G_OBJECT(button), CHOSEN_KEY, reinterpret_cast<gpointer>(chosen));
        g_object_set_data(G_OBJECT(button), DATA_KEY, data);
        g_signal_connect(button, "toggled", G_CALLBACK(+[](GtkToggleButton* button, gpointer) {
                             if (!gtk_toggle_button_get_active(button)) {
                                 return;  // the radio being switched off
                             }
                             auto report = reinterpret_cast<ShapeChosen>(
                                     g_object_get_data(G_OBJECT(button), CHOSEN_KEY));
                             report(shapePickerButtonType(GTK_WIDGET(button)),
                                    g_object_get_data(G_OBJECT(button), DATA_KEY));
                         }),
                         nullptr);
    }
    return GTK_WIDGET(grid);
}

}  // namespace utn
