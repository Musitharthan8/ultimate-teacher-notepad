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

#include "UtnWidgets.h"

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

auto ToolTextMode::createItem(bool) -> xoj::util::WidgetSPtr {
    GtkWidget* toggle = gtk_toggle_button_new();
    gtk_button_set_child(GTK_BUTTON(toggle), getNewToolIcon());
    gtk_widget_set_can_focus(toggle, false);
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
    return xoj::util::WidgetSPtr(toggle, xoj::util::adopt);
}

auto ToolTextMode::getToolDisplayName() const -> std::string {
    return mode == TextMode::AnswerBox ? _("Answer Box") : _("Text");
}

auto ToolTextMode::getNewToolIcon() const -> GtkWidget* {
    return gtk_image_new_from_icon_name(iconName.c_str(), GTK_ICON_SIZE_LARGE_TOOLBAR);
}
