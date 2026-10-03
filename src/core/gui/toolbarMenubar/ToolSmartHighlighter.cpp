/*
 * Ultimate Teacher Notepad
 *
 * Smart highlighter toolbar toggle
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolSmartHighlighter.h"

#include <utility>

#include "control/Control.h"
#include "control/ToolHandler.h"
#include "util/i18n.h"

ToolSmartHighlighter::ToolSmartHighlighter(std::string id, Control* control, IconNameHelper iconNameHelper):
        AbstractToolItem(std::move(id), Category::TOOLS),
        control(control),
        iconName(iconNameHelper.iconName("tool-highlighter")) {}

auto ToolSmartHighlighter::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    GtkToolItem* item = gtk_toggle_tool_button_new();
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(item), getNewToolIcon());
    gtk_widget_set_tooltip_text(GTK_WIDGET(item), getToolDisplayName().c_str());

    gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(item),
                                      control->getToolHandler()->isSmartHighlighterEnabled());

    // Enable smart mode and switch directly to the highlighter
    g_signal_connect(
            item,
            "toggled",
            G_CALLBACK(+[](GtkToggleToolButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                bool enabled = gtk_toggle_tool_button_get_active(button);

                auto* tools = ctrl->getToolHandler();
                tools->setSmartHighlighterEnabled(enabled);

                if (enabled) {
                    tools->selectTool(TOOL_HIGHLIGHTER);
                    tools->fireToolChanged();
                }
            }),
            control);

    return xoj::util::WidgetSPtr(GTK_WIDGET(item), xoj::util::adopt);
}

auto ToolSmartHighlighter::getToolDisplayName() const -> std::string {
    return _("Smart Highlighter");
}

auto ToolSmartHighlighter::getNewToolIcon() const -> GtkWidget* {
    return gtk_image_new_from_icon_name(iconName.c_str(), GTK_ICON_SIZE_LARGE_TOOLBAR);
}
