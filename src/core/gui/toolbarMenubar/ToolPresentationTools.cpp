/*
 * Ultimate Teacher Notepad
 *
 * Classroom presentation tools
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolPresentationTools.h"

#include <utility>

#include "control/Control.h"
#include "control/ToolHandler.h"
#include "gui/MainWindow.h"
#include "gui/PageView.h"
#include "gui/XournalView.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"

namespace {
void repaintPresentationOverlay(Control* control) {
    if (auto* window = control->getWindow()) {
        if (auto* xournal = window->getXournal()) {
            if (auto* pageView = xournal->getViewFor(control->getCurrentPageNo())) {
                pageView->repaintPage();
            }
        }
    }
}

void selectTemporaryTool(Control* control, ToolType type) {
    auto* tools = control->getToolHandler();
    tools->clearPresentationOverlay();
    tools->selectTool(type);
    tools->fireToolChanged();
    repaintPresentationOverlay(control);
}
}  // namespace

ToolPresentationTools::ToolPresentationTools(std::string id, Control* control):
        AbstractToolItem(std::move(id), Category::TOOLS), control(control) {}

auto ToolPresentationTools::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    GtkPopover* popover = GTK_POPOVER(gtk_popover_new());
    gtk_widget_add_css_class(GTK_WIDGET(popover), "toolbar");

    GtkBox* panel = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 2));
    gtk_widget_set_margin_start(GTK_WIDGET(panel), 6);
    gtk_widget_set_margin_end(GTK_WIDGET(panel), 6);
    gtk_widget_set_margin_top(GTK_WIDGET(panel), 6);
    gtk_widget_set_margin_bottom(GTK_WIDGET(panel), 6);
    gtk_popover_set_child(popover, GTK_WIDGET(panel));

    GtkWidget* laser = gtk_button_new_with_label(_("Temporary Pen"));
    GtkWidget* temporaryHighlighter = gtk_button_new_with_label(_("Temporary Highlighter"));
    GtkWidget* spotlight = gtk_button_new_with_label(_("Spotlight"));
    GtkWidget* curtain = gtk_button_new_with_label(_("Curtain Reveal"));
    GtkWidget* normal = gtk_button_new_with_label(_("Exit Presentation Tool"));

    g_signal_connect(
            laser,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                selectTemporaryTool(static_cast<Control*>(data), TOOL_LASER_POINTER_PEN);
            }),
            control);

    g_signal_connect(
            temporaryHighlighter,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                selectTemporaryTool(static_cast<Control*>(data), TOOL_LASER_POINTER_HIGHLIGHTER);
            }),
            control);

    g_signal_connect(
            spotlight,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                auto* tools = ctrl->getToolHandler();
                tools->setSpotlightEnabled(true);
                tools->selectTool(TOOL_HAND);
                tools->fireToolChanged();
                repaintPresentationOverlay(ctrl);
            }),
            control);

    g_signal_connect(
            curtain,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                auto* tools = ctrl->getToolHandler();
                tools->setCurtainEnabled(true);
                tools->selectTool(TOOL_HAND);
                tools->fireToolChanged();
                repaintPresentationOverlay(ctrl);
            }),
            control);

    g_signal_connect(
            normal,
            "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer data) {
                auto* ctrl = static_cast<Control*>(data);
                auto* tools = ctrl->getToolHandler();
                tools->clearPresentationOverlay();
                tools->selectTool(TOOL_PEN);
                tools->fireToolChanged();
                repaintPresentationOverlay(ctrl);
            }),
            control);

    gtk_box_append(panel, laser);
    gtk_box_append(panel, temporaryHighlighter);
    gtk_box_append(panel, spotlight);
    gtk_box_append(panel, curtain);
    gtk_box_append(panel, normal);

    GtkMenuButton* menuButton = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_widget_set_can_focus(GTK_WIDGET(menuButton), false);
    gtk_widget_set_tooltip_text(GTK_WIDGET(menuButton), getToolDisplayName().c_str());
    gtk_button_set_child(GTK_BUTTON(menuButton), getNewToolIcon());
    gtk_menu_button_set_popover(menuButton, GTK_WIDGET(popover));
    gtk_menu_button_set_direction(menuButton, horizontal ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT);

    gtk_widget_show_all(GTK_WIDGET(panel));
    return xoj::util::WidgetSPtr(GTK_WIDGET(menuButton), xoj::util::adopt);
}

auto ToolPresentationTools::getToolDisplayName() const -> std::string {
    return _("Presentation Tools");
}

auto ToolPresentationTools::getNewToolIcon() const -> GtkWidget* {
    return gtk_label_new("◉");
}
