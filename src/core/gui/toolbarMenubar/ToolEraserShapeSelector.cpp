/*
 * Ultimate Teacher Notepad
 *
 * Eraser shape selector
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolEraserShapeSelector.h"

#include <array>
#include <utility>

#include "control/Control.h"
#include "control/ToolHandler.h"
#include "control/UtnEraserShape.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"

namespace {
struct ShapeEntry {
    const char* label;
    UtnEraserShape shape;
};

constexpr std::array<ShapeEntry, 3> SHAPES{{
        {"○  Round", UtnEraserShape::Round},
        {"□  Square", UtnEraserShape::Square},
        {"▭  Flat", UtnEraserShape::Flat},
}};
}  // namespace

ToolEraserShapeSelector::ToolEraserShapeSelector(std::string id, Control* control):
        AbstractToolItem(std::move(id), Category::TOOLS), control(control) {}

auto ToolEraserShapeSelector::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    GtkPopover* popover = GTK_POPOVER(gtk_popover_new());
    gtk_widget_add_css_class(GTK_WIDGET(popover), "toolbar");

    GtkBox* panel = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 2));
    gtk_widget_set_margin_start(GTK_WIDGET(panel), 6);
    gtk_widget_set_margin_end(GTK_WIDGET(panel), 6);
    gtk_widget_set_margin_top(GTK_WIDGET(panel), 6);
    gtk_widget_set_margin_bottom(GTK_WIDGET(panel), 6);
    gtk_popover_set_child(popover, GTK_WIDGET(panel));

    for (const auto& entry: SHAPES) {
        GtkWidget* button = gtk_button_new_with_label(entry.label);
        gtk_widget_set_can_focus(button, false);
        g_object_set_data(G_OBJECT(button), "utn-control", control);
        g_object_set_data(G_OBJECT(button), "utn-shape", GINT_TO_POINTER(static_cast<int>(entry.shape)));

        g_signal_connect(
                button,
                "clicked",
                G_CALLBACK(+[](GtkButton* button, gpointer data) {
                    auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                    auto shape = static_cast<UtnEraserShape>(
                            GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "utn-shape")));

                    auto* tools = ctrl->getToolHandler();
                    tools->setEraserShape(shape);
                    tools->selectTool(TOOL_ERASER);
                    tools->fireToolChanged();

                    gtk_popover_popdown(GTK_POPOVER(data));
                }),
                popover);

        gtk_box_append(panel, button);
    }

    GtkMenuButton* menuButton = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_widget_set_can_focus(GTK_WIDGET(menuButton), false);
    gtk_widget_set_tooltip_text(GTK_WIDGET(menuButton), getToolDisplayName().c_str());
    gtk_button_set_child(GTK_BUTTON(menuButton), getNewToolIcon());
    gtk_menu_button_set_popover(menuButton, GTK_WIDGET(popover));
    gtk_menu_button_set_direction(menuButton, horizontal ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT);

    gtk_widget_show_all(GTK_WIDGET(panel));
    return xoj::util::WidgetSPtr(GTK_WIDGET(menuButton), xoj::util::adopt);
}

auto ToolEraserShapeSelector::getToolDisplayName() const -> std::string {
    return _("Eraser Shape");
}

auto ToolEraserShapeSelector::getNewToolIcon() const -> GtkWidget* {
    return gtk_label_new("○");
}
