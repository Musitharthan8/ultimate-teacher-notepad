/*
 * Ultimate Teacher Notepad
 *
 * Smart highlighter toolbar control
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolSmartHighlighter.h"

#include <utility>

#include "HighlightModes.h"  // for HIGHLIGHT_MODES
#include "control/Control.h"
#include "control/ToolHandler.h"
#include "control/actions/ActionDatabase.h"
#include "util/GVariantTemplate.h"
#include "util/raii/GVariantSPtr.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"

ToolSmartHighlighter::ToolSmartHighlighter(std::string id, Control* control, IconNameHelper iconNameHelper):
        AbstractToolItem(std::move(id), Category::TOOLS),
        control(control),
        iconName(iconNameHelper.iconName("tool-highlighter")) {}

auto ToolSmartHighlighter::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    auto* tools = control->getToolHandler();

    GtkWidget* toggle = gtk_toggle_button_new();
    gtk_button_set_child(GTK_BUTTON(toggle), getNewToolIcon());
    gtk_widget_set_tooltip_text(toggle, getToolDisplayName().c_str());
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle), tools->getToolType() == TOOL_HIGHLIGHTER);

    // Selection changes update the toggle without activating Highlight again.
    auto action = control->getActionDatabase()->getAction(Action::SELECT_TOOL);
    g_signal_connect_object(
            action.get(), "notify::state", G_CALLBACK(+[](GObject* action, GParamSpec*, gpointer toggle) {
                xoj::util::GVariantSPtr state(g_action_get_state(G_ACTION(action)), xoj::util::adopt);
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle),
                                             getGVariantValue<ToolType>(state.get()) == TOOL_HIGHLIGHTER);
            }),
            toggle, GConnectFlags(0));
    g_object_set_data(G_OBJECT(toggle), "utn-control", control);
    g_signal_connect(
            toggle, "clicked", G_CALLBACK(+[](GtkButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                auto* tools = ctrl->getToolHandler();
                // Re-selecting Highlight keeps the chosen Freehand/Smart mode; switching to it keeps it as well.
                ctrl->selectHighlighter(tools->isSmartHighlighterEnabled() ||
                                        tools->getToolType() != TOOL_HIGHLIGHTER);
                // A GtkToggleButton flips itself on every click; the selected state follows the active tool only.
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
                                             tools->getToolType() == TOOL_HIGHLIGHTER);
            }),
            control);

    GtkPopover* popover = GTK_POPOVER(gtk_popover_new());
    gtk_widget_add_css_class(GTK_WIDGET(popover), "toolbar");

    GtkBox* panel = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 2));
    gtk_widget_set_margin_start(GTK_WIDGET(panel), 6);
    gtk_widget_set_margin_end(GTK_WIDGET(panel), 6);
    gtk_widget_set_margin_top(GTK_WIDGET(panel), 6);
    gtk_widget_set_margin_bottom(GTK_WIDGET(panel), 6);
    gtk_popover_set_child(popover, GTK_WIDGET(panel));

    GtkWidget* modesTitle = gtk_label_new(_("Highlight mode"));
    gtk_widget_set_halign(modesTitle, GTK_ALIGN_START);
    gtk_widget_add_css_class(modesTitle, "utn-popover-title");
    gtk_box_append(panel, modesTitle);

    for (size_t i = 0; i < utn::HIGHLIGHT_MODES.size(); ++i) {
        const auto& entry = utn::HIGHLIGHT_MODES[i];
        GtkWidget* button = gtk_button_new_with_label(_(entry.label));
        gtk_widget_set_tooltip_text(button, _(entry.hint));
        gtk_widget_set_halign(gtk_bin_get_child(GTK_BIN(button)), GTK_ALIGN_START);
        g_object_set_data(G_OBJECT(button), "utn-control", control);
        g_object_set_data(G_OBJECT(button), "utn-toggle", toggle);
        g_object_set_data(G_OBJECT(button), "utn-mode-index", GINT_TO_POINTER(static_cast<int>(i)));

        g_signal_connect(
                button,
                "clicked",
                G_CALLBACK(+[](GtkButton* button, gpointer data) {
                    auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                    auto* toggle = GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(button), "utn-toggle"));
                    const auto& mode = utn::HIGHLIGHT_MODES[static_cast<size_t>(
                            GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "utn-mode-index")))];

                    ctrl->selectHighlighter(mode.snap.has_value(), mode.snap);
                    gtk_toggle_button_set_active(toggle, true);
                    gtk_popover_popdown(GTK_POPOVER(data));
                }),
                popover);
        gtk_box_append(panel, button);
    }

    GtkMenuButton* menuButton = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_widget_set_can_focus(GTK_WIDGET(menuButton), false);
    gtk_widget_set_tooltip_text(GTK_WIDGET(menuButton), _("Highlight mode"));
    gtk_menu_button_set_popover(menuButton, GTK_WIDGET(popover));
    gtk_menu_button_set_direction(menuButton, horizontal ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT);

    GtkBox* box = GTK_BOX(gtk_box_new(horizontal ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL, 0));
    gtk_box_append(box, toggle);
    gtk_box_append(box, GTK_WIDGET(menuButton));

    gtk_widget_show_all(GTK_WIDGET(panel));
    return xoj::util::WidgetSPtr(GTK_WIDGET(box), xoj::util::adopt);
}

auto ToolSmartHighlighter::getToolDisplayName() const -> std::string {
    return _("Highlight");
}

auto ToolSmartHighlighter::getNewToolIcon() const -> GtkWidget* {
    return gtk_image_new_from_icon_name(iconName.c_str(), GTK_ICON_SIZE_LARGE_TOOLBAR);
}
