/*
 * Ultimate Teacher Notepad
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolWorkspaceSwitch.h"

#include <string>

#include "control/Control.h"
#include "control/settings/Settings.h"
#include "gui/MainWindow.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"

#include "UtnWidgets.h"

namespace {
struct Workspace {
    const char* layoutId;  // toolbar.ini section that the workspace opens
    const char* label;
    const char* tooltip;
};

// Teach and Mark are the two working layouts. Present is the projector layout with the least chrome.
const Workspace WORKSPACES[] = {
        {"UTN Teacher", "Teach", "Teach: the full teaching tool rail"},
        {"UTN Marking", "Mark", "Mark: comments, feedback and marked copies"},
        {"UTN Present", "Present", "Present: a quiet layout for the projector"},
};

void onWorkspaceClicked(GtkButton* button, gpointer data) {
    auto* control = static_cast<Control*>(data);
    if (utn::isSyncing(GTK_WIDGET(button))) {
        return;
    }
    const auto* layoutId = static_cast<const char*>(g_object_get_data(G_OBJECT(button), "utn-layout"));
    // Keep the tapped workspace active; the toolbar rebuild refreshes the others.
    utn::syncToggle(GTK_TOGGLE_BUTTON(button), true);
    if (auto* window = control->getWindow()) {
        window->toolbarSelected(layoutId);
    }
}
}  // namespace

ToolWorkspaceSwitch::ToolWorkspaceSwitch(std::string id, Control* control):
        AbstractToolItem(std::move(id), Category::MISC), control(control) {}

xoj::util::WidgetSPtr ToolWorkspaceSwitch::createItem(bool) {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(box, "utnWorkspaceSwitch");
    utn::setAccessibleName(box, _("Workspace"));

    // The workspace the window is showing now comes from settings, which are updated before toolbars rebuild.
    const std::string& current = control->getSettings()->getSelectedToolbar();
    for (const auto& ws: WORKSPACES) {
        GtkWidget* toggle = gtk_toggle_button_new();
        gtk_button_set_child(GTK_BUTTON(toggle), gtk_label_new(_(ws.label)));
        gtk_widget_add_css_class(toggle, "utnWorkspaceButton");
        gtk_widget_set_focus_on_click(toggle, false);  // keyboard users can still Tab to it
        gtk_widget_set_tooltip_text(toggle, _(ws.tooltip));
        utn::setAccessibleName(toggle, _(ws.label));
        g_object_set_data(G_OBJECT(toggle), "utn-layout", const_cast<char*>(ws.layoutId));
        utn::syncToggle(GTK_TOGGLE_BUTTON(toggle), current == ws.layoutId);
        g_signal_connect(toggle, "clicked", G_CALLBACK(onWorkspaceClicked), control);
        gtk_box_append(GTK_BOX(box), toggle);
    }
    return xoj::util::WidgetSPtr(box, xoj::util::adopt);
}

std::string ToolWorkspaceSwitch::getToolDisplayName() const { return _("Workspace"); }

GtkWidget* ToolWorkspaceSwitch::getNewToolIcon() const {
    return gtk_image_new_from_icon_name("view-grid", GTK_ICON_SIZE_SMALL_TOOLBAR);
}
