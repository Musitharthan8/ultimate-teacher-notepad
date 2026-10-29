/*
 * Ultimate Teacher Notepad
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolTextMode.h"

#include <utility>

#include "control/Control.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"

#include <algorithm>

#include "UtnWidgets.h"
#include "ToolUtnContextBar.h"

ToolTextMode::ToolTextMode(std::string id, Control* control, IconNameHelper iconNameHelper, TextMode mode):
        AbstractToolItem(std::move(id), Category::TOOLS),
        control(control),
        mode(mode),
        iconName(iconNameHelper.iconName(mode == TextMode::AnswerBox ? "utn-answer-box" : "tool-text")) {
    control->getToolHandler()->addToolChangedListener([this](ToolType) { syncButton(); });
}

auto ToolTextMode::isSelected() const -> bool {
    auto* tools = control->getToolHandler();
    return tools->getToolType() == TOOL_TEXT && tools->getTextMode() == mode;
}

void ToolTextMode::syncButton() {
    if (button) {
        utn::syncToggle(button, isSelected());
    }
}

auto ToolTextMode::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    GtkWidget* toggle = gtk_toggle_button_new();
    gtk_button_set_child(GTK_BUTTON(toggle), getNewToolIcon());
    gtk_widget_set_focus_on_click(toggle, false);  // keyboard users can still Tab to it
    gtk_widget_set_tooltip_text(toggle, getToolDisplayName().c_str());
    utn::setAccessibleName(toggle, getToolDisplayName().c_str());

    button = GTK_TOGGLE_BUTTON(toggle);
    g_signal_connect(toggle, "destroy", G_CALLBACK(+[](GtkWidget* widget, gpointer data) {
                         auto* self = static_cast<ToolTextMode*>(data);
                         if (GTK_WIDGET(self->button) == widget) {
                             self->button = nullptr;
                         }
                     }),
                     this);
    g_signal_connect(toggle, "clicked", G_CALLBACK(+[](GtkButton* widget, gpointer data) {
                         if (utn::isSyncing(GTK_WIDGET(widget))) {
                             return;
                         }
                         auto* self = static_cast<ToolTextMode*>(data);
                         // Clicking the active mode again keeps it; the button never turns itself off
                         self->control->selectTextMode(self->mode);
                         self->syncButton();
                     }),
                     this);

    utn::syncToggle(button, isSelected());

    auto [popover, panel] = utn::createPopoverPanel(8);
    GtkWidget* settings = ToolUtnContextBar::createTextSettingsPanel(control);
    // The Answer Box panel is about 620 px tall: scroll it instead of letting a short window clip the popover
    GtkWidget* scroller = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(scroller), true);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scroller), true);
    gtk_container_add(GTK_CONTAINER(scroller), settings);
    gtk_box_append(panel, scroller);
    gtk_widget_add_css_class(GTK_WIDGET(panel), "utn-text-settings");
    // The panel is built once with the toolbar, so refresh it on every opening: the mode, font and colours may have
    // changed since. The popover owns the panel; the reference below only lives as long as the popover.
    g_object_set_data_full(G_OBJECT(popover), "utn-text-settings", g_object_ref(settings), g_object_unref);
    g_object_set_data(G_OBJECT(popover), "utn-text-scroller", scroller);
    g_signal_connect(popover, "show", G_CALLBACK(+[](GtkWidget* widget, gpointer data) {
                         ToolUtnContextBar::refreshTextSettingsPanel(
                                 static_cast<Control*>(data),
                                 GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "utn-text-settings")));
                         // Leave room for the toolbar and title around the popover; never shrink below a usable height
                         GtkWidget* anchor = gtk_popover_get_relative_to(GTK_POPOVER(widget));
                         GtkWidget* window = anchor ? gtk_widget_get_toplevel(anchor) : nullptr;
                         const int available = (window ? gtk_widget_get_allocated_height(window) : 800) - 150;
                         gtk_scrolled_window_set_max_content_height(
                                 GTK_SCROLLED_WINDOW(g_object_get_data(G_OBJECT(widget), "utn-text-scroller")),
                                 std::max(available, 220));
                     }),
                     control);
    GtkWidget* options = gtk_menu_button_new();
    gtk_widget_set_tooltip_text(options, _("Text settings"));
    utn::setAccessibleName(options, _("Text settings"));
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(options), GTK_WIDGET(popover));
    // Same chevron direction as every other rail item that opens options
    gtk_menu_button_set_direction(GTK_MENU_BUTTON(options), horizontal ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT);
    utn::bindActiveToolOptions(button, GTK_MENU_BUTTON(options));
    // Keep the active text tool at the full rail width; its settings remain a
    // separate, keyboard-operable target without adding another rail column.
    GtkOverlay* overlay = GTK_OVERLAY(gtk_overlay_new());
    gtk_container_add(GTK_CONTAINER(overlay), toggle);
    gtk_overlay_add_overlay(overlay, options);
    utn::placeOptionsCorner(GTK_WIDGET(options));
    return xoj::util::WidgetSPtr(GTK_WIDGET(overlay), xoj::util::adopt);
}

auto ToolTextMode::getToolDisplayName() const -> std::string {
    return mode == TextMode::AnswerBox ? _("Answer Box") : _("Text");
}

auto ToolTextMode::getNewToolIcon() const -> GtkWidget* {
    return gtk_image_new_from_icon_name(iconName.c_str(), GTK_ICON_SIZE_LARGE_TOOLBAR);
}
