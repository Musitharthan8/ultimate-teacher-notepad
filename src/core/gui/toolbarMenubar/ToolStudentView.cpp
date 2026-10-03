/*
 * Ultimate Teacher Notepad
 *
 * Student view toolbar item
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolStudentView.h"

#include <string>
#include <utility>

#include "control/Control.h"
#include "control/layer/LayerController.h"
#include "model/Layer.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"

namespace {
constexpr const char* TEACHER_LAYER_PREFIX = "UTN Teacher Notes";

void createTeacherNotesLayer(Control* control) {
    auto* layers = control->getLayerController();
    layers->addNewLayer(false);

    unsigned int suffix = 1;
    std::string name = TEACHER_LAYER_PREFIX;

    while (true) {
        bool exists = false;
        for (Layer::Index id = 1; id <= layers->getLayerCount(); ++id) {
            if (layers->getLayerNameById(id) == name) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            break;
        }

        ++suffix;
        name = std::string(TEACHER_LAYER_PREFIX) + " " + std::to_string(suffix);
    }

    layers->setCurrentLayerName(name);
    control->refreshStudentView();
}
}  // namespace

ToolStudentView::ToolStudentView(std::string id, Control* control):
        AbstractToolItem(std::move(id), Category::MISC), control(control) {}

auto ToolStudentView::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    GtkPopover* popover = GTK_POPOVER(gtk_popover_new());
    gtk_widget_add_css_class(GTK_WIDGET(popover), "toolbar");

    GtkBox* panel = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 2));
    gtk_widget_set_margin_start(GTK_WIDGET(panel), 6);
    gtk_widget_set_margin_end(GTK_WIDGET(panel), 6);
    gtk_widget_set_margin_top(GTK_WIDGET(panel), 6);
    gtk_widget_set_margin_bottom(GTK_WIDGET(panel), 6);
    gtk_popover_set_child(popover, GTK_WIDGET(panel));

    GtkWidget* open = gtk_button_new_with_label(_("Open Student View"));
    GtkWidget* fullscreen = gtk_button_new_with_label(_("Fullscreen Student View"));
    GtkWidget* windowed = gtk_button_new_with_label(_("Windowed Student View"));
    GtkWidget* hide = gtk_button_new_with_label(_("Hide Student View"));
    GtkWidget* notes = gtk_button_new_with_label(_("Create Teacher Notes Layer"));

    for (GtkWidget* button: {open, fullscreen, windowed, hide, notes}) {
        gtk_widget_set_can_focus(button, false);
        g_object_set_data(G_OBJECT(button), "utn-control", control);
    }

    g_signal_connect(
            open,
            "clicked",
            G_CALLBACK(+[](GtkButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                ctrl->showStudentView();
                gtk_popover_popdown(GTK_POPOVER(data));
            }),
            popover);

    g_signal_connect(
            fullscreen,
            "clicked",
            G_CALLBACK(+[](GtkButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                ctrl->setStudentViewFullscreen(true);
                gtk_popover_popdown(GTK_POPOVER(data));
            }),
            popover);

    g_signal_connect(
            windowed,
            "clicked",
            G_CALLBACK(+[](GtkButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                ctrl->setStudentViewFullscreen(false);
                gtk_popover_popdown(GTK_POPOVER(data));
            }),
            popover);

    g_signal_connect(
            hide,
            "clicked",
            G_CALLBACK(+[](GtkButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                ctrl->hideStudentView();
                gtk_popover_popdown(GTK_POPOVER(data));
            }),
            popover);

    g_signal_connect(
            notes,
            "clicked",
            G_CALLBACK(+[](GtkButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                createTeacherNotesLayer(ctrl);
                gtk_popover_popdown(GTK_POPOVER(data));
            }),
            popover);

    gtk_box_append(panel, open);
    gtk_box_append(panel, fullscreen);
    gtk_box_append(panel, windowed);
    gtk_box_append(panel, hide);
    gtk_box_append(panel, gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(panel, notes);

    GtkMenuButton* menuButton = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_widget_set_can_focus(GTK_WIDGET(menuButton), false);
    gtk_widget_set_tooltip_text(GTK_WIDGET(menuButton), getToolDisplayName().c_str());
    gtk_button_set_child(GTK_BUTTON(menuButton), getNewToolIcon());
    gtk_menu_button_set_popover(menuButton, GTK_WIDGET(popover));
    gtk_menu_button_set_direction(menuButton, horizontal ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT);

    gtk_widget_show_all(GTK_WIDGET(panel));
    return xoj::util::WidgetSPtr(GTK_WIDGET(menuButton), xoj::util::adopt);
}

auto ToolStudentView::getToolDisplayName() const -> std::string {
    return _("Teacher / Student View");
}

auto ToolStudentView::getNewToolIcon() const -> GtkWidget* {
    return gtk_label_new("S");
}
