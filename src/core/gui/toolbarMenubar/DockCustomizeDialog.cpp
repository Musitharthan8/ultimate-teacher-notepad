#include "DockCustomizeDialog.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "control/Control.h"
#include "control/UtnLayout.h"
#include "gui/MainWindow.h"
#include "model/DockLayout.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"

#include "ToolMenuHandler.h"
#include "UtnWidgets.h"
#include "model/ToolbarData.h"

namespace utn {

namespace {
enum class DockAction { MoveUp, MoveDown, Pin, Unpin, Hide, Show };

struct DockButtonData {
    Control* control;
    std::string id;
    DockAction action;
    std::function<void()>* rebuild;
};

std::vector<std::string> defaultRail(Control* control) {
    MainWindow* window = control->getWindow();
    if (!window || !window->getSelectedToolbar() || !utn::isTeacherLayout(window->getSelectedToolbar()->getId())) {
        return {};
    }
    return window->getToolMenuHandler()->railItemIds(window->getSelectedToolbar());
}

void perform(const DockButtonData& data) {
    const std::vector<std::string> defaults = defaultRail(data.control);
    data.control->updateDockLayout([&](DockLayout& layout) {
        switch (data.action) {
            case DockAction::MoveUp: layout.moveUp(data.id, defaults); break;
            case DockAction::MoveDown: layout.moveDown(data.id, defaults); break;
            case DockAction::Pin: layout.pin(data.id); break;
            case DockAction::Unpin: layout.unpin(data.id); break;
            case DockAction::Hide: layout.hide(data.id); break;
            case DockAction::Show: layout.show(data.id); break;
        }
    });
}

GtkWidget* actionButton(Control* control, const DockRow& row, DockAction action, const char* label, const std::string& name,
                        bool enabled, std::function<void()>* rebuild) {
    GtkWidget* button = gtk_button_new_with_label(label);
    gtk_widget_set_focus_on_click(button, false);
    utn::setAccessibleName(button, name.c_str());
    gtk_widget_set_tooltip_text(button, name.c_str());
    gtk_widget_set_sensitive(button, enabled);
    auto* data = new DockButtonData{control, row.id, action, rebuild};
    g_object_set_data_full(G_OBJECT(button), "utn-dock-action", data,
                           +[](gpointer p) { delete static_cast<DockButtonData*>(p); });
    g_signal_connect(button, "clicked", G_CALLBACK(+[](GtkButton* b, gpointer) {
                         // Copied first: the rebuild below destroys this very button
                         const DockButtonData data = *static_cast<DockButtonData*>(g_object_get_data(G_OBJECT(b), "utn-dock-action"));
                         perform(data);
                         (*data.rebuild)();
                     }),
                     nullptr);
    return button;
}
}  // namespace

std::vector<DockRow> dockRows(Control* control) {
    std::vector<DockRow> rows;
    const std::vector<std::string> defaults = defaultRail(control);
    if (defaults.empty()) {
        return rows;
    }
    MainWindow* window = control->getWindow();
    const DockLayout& layout = control->getDockLayout();
    const auto makeRow = [&](const std::string& id, bool hidden) {
        DockRow row;
        row.id = id;
        row.name = window->getToolMenuHandler()->toolDisplayName(id);
        row.pinned = layout.isPinned(id);
        row.hidden = hidden;
        row.essential = DockLayout::isEssential(id);
        DockLayout probe = layout;
        row.canMoveUp = !hidden && probe.moveUp(id, defaults);
        probe = layout;
        row.canMoveDown = !hidden && probe.moveDown(id, defaults);
        return row;
    };
    for (const std::string& id: layout.arrange(defaults)) {
        if (id != "SEPARATOR") {
            rows.push_back(makeRow(id, false));
        }
    }
    for (const std::string& id: layout.hidden()) {
        if (std::find(defaults.begin(), defaults.end(), id) != defaults.end()) {
            rows.push_back(makeRow(id, true));
        }
    }
    return rows;
}

void showDockCustomizeDialog(Control* control) {
    GtkWidget* dialog = gtk_dialog_new_with_buttons(_("Customise tool dock"), control->getGtkWindow(), GTK_DIALOG_MODAL,
                                                    _("Reset to default"), GTK_RESPONSE_REJECT, _("Close"),
                                                    GTK_RESPONSE_CLOSE, nullptr);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 420, 460);
    GtkWidget* list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(list, 12);
    gtk_widget_set_margin_end(list, 12);
    gtk_widget_set_margin_top(list, 12);
    gtk_widget_set_margin_bottom(list, 12);
    GtkWidget* scroller = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroller, true);
    gtk_container_add(GTK_CONTAINER(scroller), list);
    GtkWidget* intro = gtk_label_new(_("Pin the tools you use most to the top, hide the ones you do not need, and move "
                                       "tools up or down. Hidden tools stay in the menus and on their shortcuts."));
    gtk_label_set_line_wrap(GTK_LABEL(intro), true);
    gtk_widget_set_halign(intro, GTK_ALIGN_START);
    gtk_widget_set_margin_start(intro, 12);
    gtk_widget_set_margin_end(intro, 12);
    gtk_widget_set_margin_top(intro, 12);
    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_pack_start(GTK_BOX(content), intro, false, false, 0);
    gtk_box_pack_start(GTK_BOX(content), scroller, true, true, 0);

    auto* rebuild = new std::function<void()>();
    *rebuild = [control, list, rebuild]() {
        gtk_container_foreach(GTK_CONTAINER(list), +[](GtkWidget* child, gpointer) { gtk_widget_destroy(child); }, nullptr);
        for (const DockRow& row: dockRows(control)) {
            GtkWidget* line = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
            std::string text = row.name;
            if (row.pinned) {
                text += "  \xE2\x98\x85";  // a star, as well as the Unpin button, marks a favourite
            }
            if (row.hidden) {
                text += std::string("  (") + _("hidden") + ")";
            }
            GtkWidget* name = gtk_label_new(text.c_str());
            gtk_widget_set_halign(name, GTK_ALIGN_START);
            gtk_widget_set_hexpand(name, true);
            gtk_box_pack_start(GTK_BOX(line), name, true, true, 0);
            if (!row.hidden) {
                gtk_box_pack_start(GTK_BOX(line),
                                   actionButton(control, row, DockAction::MoveUp, "\xE2\x96\xB2",
                                                std::string(_("Move up")) + ": " + row.name, row.canMoveUp, rebuild),
                                   false, false, 0);
                gtk_box_pack_start(GTK_BOX(line),
                                   actionButton(control, row, DockAction::MoveDown, "\xE2\x96\xBC",
                                                std::string(_("Move down")) + ": " + row.name, row.canMoveDown, rebuild),
                                   false, false, 0);
                gtk_box_pack_start(GTK_BOX(line),
                                   row.pinned ? actionButton(control, row, DockAction::Unpin, _("Unpin"),
                                                             std::string(_("Unpin")) + ": " + row.name, true, rebuild) :
                                                actionButton(control, row, DockAction::Pin, _("Pin"),
                                                             std::string(_("Pin to the top")) + ": " + row.name, true, rebuild),
                                   false, false, 0);
                gtk_box_pack_start(GTK_BOX(line),
                                   actionButton(control, row, DockAction::Hide, _("Hide"),
                                                row.essential ? std::string(_("Always on the dock")) + ": " + row.name :
                                                                std::string(_("Hide")) + ": " + row.name,
                                                !row.essential, rebuild),
                                   false, false, 0);
            } else {
                gtk_box_pack_start(GTK_BOX(line),
                                   actionButton(control, row, DockAction::Show, _("Show"),
                                                std::string(_("Show on the dock")) + ": " + row.name, true, rebuild),
                                   false, false, 0);
            }
            gtk_box_pack_start(GTK_BOX(list), line, false, false, 0);
        }
        gtk_widget_show_all(list);
    };
    (*rebuild)();

    g_object_set_data_full(G_OBJECT(dialog), "utn-rebuild", rebuild,
                           +[](gpointer p) { delete static_cast<std::function<void()>*>(p); });
    g_object_set_data(G_OBJECT(dialog), "utn-control", control);
    g_signal_connect(dialog, "response", G_CALLBACK(+[](GtkDialog* dlg, gint response, gpointer) {
                         auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(dlg), "utn-control"));
                         if (response == GTK_RESPONSE_REJECT) {
                             ctrl->updateDockLayout([](DockLayout& layout) { layout.reset(); });
                             (*static_cast<std::function<void()>*>(g_object_get_data(G_OBJECT(dlg), "utn-rebuild")))();
                             return;  // stay open: the list now shows the shipped rail
                         }
                         gtk_widget_destroy(GTK_WIDGET(dlg));
                     }),
                     nullptr);
    gtk_widget_show_all(dialog);
}

}  // namespace utn
