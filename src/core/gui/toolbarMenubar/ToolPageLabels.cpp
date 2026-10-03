/*
 * Ultimate Teacher Notepad
 *
 * Classroom page labels and jumps
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolPageLabels.h"

#include <array>
#include <memory>
#include <shared_mutex>
#include <string>
#include <utility>

#include "control/Control.h"
#include "control/ScrollHandler.h"
#include "model/Document.h"
#include "model/XojPage.h"
#include "undo/PageLabelUndoAction.h"
#include "undo/UndoRedoHandler.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"

namespace {
constexpr std::array<const char*, 4> QUICK_LABELS{{"CW", "SW", "Answer Key", "Revision"}};

void setCurrentPageLabel(Control* control, const std::string& label) {
    PageRef page = control->getCurrentPage();
    if (!page || page->getUtnPageLabel() == label) {
        return;
    }

    std::string oldLabel = page->getUtnPageLabel();

    auto* doc = control->getDocument();
    doc->lock();
    page->setUtnPageLabel(label);
    doc->unlock();

    control->getUndoRedoHandler()->addUndoAction(
            std::make_unique<PageLabelUndoAction>(page, std::move(oldLabel), label));
}

bool jumpToLabel(Control* control, const std::string& label) {
    if (label.empty()) {
        return false;
    }

    auto* doc = control->getDocument();
    std::shared_lock lock(*doc);

    for (size_t i = 0; i < doc->getPageCount(); ++i) {
        PageRef page = doc->getPage(i);
        if (page && page->getUtnPageLabel() == label) {
            lock.unlock();
            control->getScrollHandler()->jumpToPage(i);
            control->firePageSelected(i);
            return true;
        }
    }

    return false;
}
}  // namespace

ToolPageLabels::ToolPageLabels(std::string id, Control* control):
        AbstractToolItem(std::move(id), Category::NAVIGATION), control(control) {}

auto ToolPageLabels::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    GtkPopover* popover = GTK_POPOVER(gtk_popover_new());
    gtk_widget_add_css_class(GTK_WIDGET(popover), "toolbar");

    GtkBox* panel = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
    gtk_widget_set_margin_start(GTK_WIDGET(panel), 8);
    gtk_widget_set_margin_end(GTK_WIDGET(panel), 8);
    gtk_widget_set_margin_top(GTK_WIDGET(panel), 8);
    gtk_widget_set_margin_bottom(GTK_WIDGET(panel), 8);
    gtk_popover_set_child(popover, GTK_WIDGET(panel));

    GtkWidget* title = gtk_label_new(_("Current page label"));
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(panel, title);

    GtkWidget* currentEntry = gtk_entry_new();
    if (PageRef page = control->getCurrentPage(); page) {
        gtk_editable_set_text(GTK_EDITABLE(currentEntry), page->getUtnPageLabel().c_str());
    }
    gtk_entry_set_placeholder_text(GTK_ENTRY(currentEntry), _("e.g. CW, SW, Q4, Answer Key"));
    gtk_box_append(panel, currentEntry);

    GtkWidget* save = gtk_button_new_with_label(_("Save Label"));
    g_object_set_data(G_OBJECT(save), "utn-control", control);
    g_object_set_data(G_OBJECT(save), "utn-entry", currentEntry);
    g_signal_connect(
            save,
            "clicked",
            G_CALLBACK(+[](GtkButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                auto* entry = GTK_EDITABLE(g_object_get_data(G_OBJECT(button), "utn-entry"));
                setCurrentPageLabel(ctrl, gtk_editable_get_text(entry));
                gtk_popover_popdown(GTK_POPOVER(data));
            }),
            popover);
    gtk_box_append(panel, save);

    GtkWidget* quickTitle = gtk_label_new(_("Quick labels"));
    gtk_widget_set_halign(quickTitle, GTK_ALIGN_START);
    gtk_box_append(panel, quickTitle);

    GtkBox* quickBox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
    for (const char* label: QUICK_LABELS) {
        GtkWidget* button = gtk_button_new_with_label(label);
        g_object_set_data(G_OBJECT(button), "utn-control", control);
        g_object_set_data_full(G_OBJECT(button), "utn-label", g_strdup(label), g_free);

        g_signal_connect(
                button,
                "clicked",
                G_CALLBACK(+[](GtkButton* button, gpointer data) {
                    auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                    auto* label = static_cast<const char*>(g_object_get_data(G_OBJECT(button), "utn-label"));
                    setCurrentPageLabel(ctrl, label ? label : "");
                    gtk_popover_popdown(GTK_POPOVER(data));
                }),
                popover);

        gtk_box_append(quickBox, button);
    }
    gtk_box_append(panel, GTK_WIDGET(quickBox));

    GtkWidget* jumpTitle = gtk_label_new(_("Jump to label"));
    gtk_widget_set_halign(jumpTitle, GTK_ALIGN_START);
    gtk_box_append(panel, jumpTitle);

    GtkWidget* jumpEntry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(jumpEntry), _("Exact label"));
    gtk_box_append(panel, jumpEntry);

    GtkWidget* jump = gtk_button_new_with_label(_("Jump"));
    g_object_set_data(G_OBJECT(jump), "utn-control", control);
    g_object_set_data(G_OBJECT(jump), "utn-entry", jumpEntry);
    g_signal_connect(
            jump,
            "clicked",
            G_CALLBACK(+[](GtkButton* button, gpointer data) {
                auto* ctrl = static_cast<Control*>(g_object_get_data(G_OBJECT(button), "utn-control"));
                auto* entry = GTK_EDITABLE(g_object_get_data(G_OBJECT(button), "utn-entry"));
                if (jumpToLabel(ctrl, gtk_editable_get_text(entry))) {
                    gtk_popover_popdown(GTK_POPOVER(data));
                }
            }),
            popover);
    gtk_box_append(panel, jump);

    GtkMenuButton* menuButton = GTK_MENU_BUTTON(gtk_menu_button_new());
    gtk_widget_set_can_focus(GTK_WIDGET(menuButton), false);
    gtk_widget_set_tooltip_text(GTK_WIDGET(menuButton), getToolDisplayName().c_str());
    gtk_button_set_child(GTK_BUTTON(menuButton), getNewToolIcon());
    gtk_menu_button_set_popover(menuButton, GTK_WIDGET(popover));
    gtk_menu_button_set_direction(menuButton, horizontal ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT);

    gtk_widget_show_all(GTK_WIDGET(panel));
    return xoj::util::WidgetSPtr(GTK_WIDGET(menuButton), xoj::util::adopt);
}

auto ToolPageLabels::getToolDisplayName() const -> std::string {
    return _("Page Labels");
}

auto ToolPageLabels::getNewToolIcon() const -> GtkWidget* {
    return gtk_label_new("Pg");
}
