/*
 * Ultimate Teacher Notepad
 *
 * Continuous eraser size slider
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolEraserSizeSlider.h"

#include <utility>

#include "control/Control.h"
#include "control/ToolHandler.h"
#include "util/i18n.h"

ToolEraserSizeSlider::ToolEraserSizeSlider(std::string id, Control* control, IconNameHelper iconNameHelper):
        AbstractToolItem(std::move(id), Category::TOOLS),
        control(control),
        iconName(iconNameHelper.iconName("tool-eraser")) {}

auto ToolEraserSizeSlider::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    GtkOrientation orientation = horizontal ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;

    // Eraser thickness range
    GtkWidget* slider = gtk_scale_new_with_range(orientation, 0.5, 30.0, 0.5);

    // Start at current eraser thickness
    gtk_range_set_value(GTK_RANGE(slider), control->getToolHandler()->getEraserThickness());

    gtk_scale_set_draw_value(GTK_SCALE(slider), true);
    gtk_scale_set_digits(GTK_SCALE(slider), 1);

    // Slider size for horizontal or vertical toolbars
    if (horizontal) {
        gtk_widget_set_size_request(slider, 140, 20);
    } else {
        gtk_widget_set_size_request(slider, 20, 140);
    }

    // Update eraser thickness when slider changes
    g_signal_connect(
            slider,
            "value-changed",
            G_CALLBACK(+[](GtkRange* range, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                double thickness = gtk_range_get_value(range);

                ctrl->getToolHandler()->setEraserThickness(thickness);
            }),
            control);

    return xoj::util::WidgetSPtr(slider, xoj::util::adopt);
}

auto ToolEraserSizeSlider::getToolDisplayName() const -> std::string {
    return _("Eraser Size Slider");
}

auto ToolEraserSizeSlider::getNewToolIcon() const -> GtkWidget* {
    return gtk_image_new_from_icon_name(iconName.c_str(), GTK_ICON_SIZE_SMALL_TOOLBAR);
}
