/*
 * Ultimate Teacher Notepad
 *
 * Fast classroom clear actions
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolClear.h"

#include <utility>

#include "control/Control.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"

ToolClear::ToolClear(std::string id, Control* control):
        AbstractToolItem(std::move(id), Category::MISC), control(control) {}

auto ToolClear::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    GtkPopover* popover = GTK_POPOVER(gtk_popover_new());
    gtk_widget_add_css_class(GTK_WIDGET(popover), "toolbar");

    GtkBox* panel = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 2));
    gtk_widget_set_margin_start(GTK_WIDGET(panel), 6);
    gtk_widget_set_margin_end(GTK_WIDGET(panel), 6);
    gtk_widget_set_margin_top(GTK_WIDGET(panel), 6);
    gtk_widget_set_margin_bottom(GTK_WIDGET(panel), 6);
    gtk_popover_set_child(popover, GTK_WIDGET(panel));

    GtkWidget* deleteSelection = gtk_button_new_with_label(_("Delete Selection"));
    g_signal_connect(
            deleteSelection,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                ctrl->deleteSelection();
            }),
            control);
    gtk_box_append(panel, deleteSelection);

    GtkWidget* clearLayer = gtk_button_new_with_label(_("Clear Current Layer"));
    g_signal_connect(
            clearLayer,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                ctrl->selectAllOnPage();
                ctrl->deleteSelection();
            }),
            control);
    gtk_box_append(panel, clearLayer);

    GtkWidget* clearPageMenu = gtk_button_new_with_label(_("Clear Page Annotations"));
    g_signal_connect(
            clearPageMenu,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                static_cast<Control*>(data)->clearCurrentPageAnnotations();
            }),
            control);
    gtk_box_append(panel, clearPageMenu);

    // Main button is intentionally the common classroom action: clear this page.
    GtkWidget* clearPage = gtk_button_new_with_label(_("Clear"));
    gtk_widget_set_can_focus(clearPage, false);
    gtk_widget_set_tooltip_text(clearPage, _("Clear all annotations on this page (Undo available)"));
    g_signal_connect(
            clearPage,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                static_cast<Control*>(data)->clearCurrentPageAnnotations();
            }),
            control);

    GtkMenuButton* menuButton = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_widget_set_can_focus(GTK_WIDGET(menuButton), false);
    gtk_widget_set_tooltip_text(GTK_WIDGET(menuButton), _("More clear options"));
    gtk_menu_button_set_popover(menuButton, GTK_WIDGET(popover));
    gtk_menu_button_set_direction(menuButton, horizontal ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT);

    GtkBox* box = GTK_BOX(gtk_box_new(horizontal ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL, 0));
    gtk_box_append(box, clearPage);
    gtk_box_append(box, GTK_WIDGET(menuButton));

    gtk_widget_show_all(GTK_WIDGET(panel));
    return xoj::util::WidgetSPtr(GTK_WIDGET(box), xoj::util::adopt);
}

auto ToolClear::getToolDisplayName() const -> std::string {
    return _("Clear");
}

auto ToolClear::getNewToolIcon() const -> GtkWidget* {
    return gtk_label_new("⌫");
}
