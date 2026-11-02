#include "AnnotationBankPanel.h"

#include <cstdint>  // for uint32_t
#include <string>   // for string

#include "control/Control.h"          // for Control
#include "control/ToolHandler.h"      // for ToolHandler
#include "model/AnnotationBank.h"     // for AnnotationBank
#include "util/gtk4_helper.h"         // for gtk_box_append
#include "util/i18n.h"                // for _

#include "UtnWidgets.h"  // for setAccessibleName

namespace utn {

namespace {
constexpr const char* CONTROL_KEY = "utn-control";
constexpr const char* POPOVER_KEY = "utn-popover";
constexpr const char* ENTRIES_KEY = "utn-entries";
constexpr const char* STATUS_KEY = "utn-status";
constexpr const char* ENTRY_KEY = "utn-entry";
constexpr const char* INDEX_KEY = "utn-index";

Control* controlOf(GtkWidget* panel) { return static_cast<Control*>(g_object_get_data(G_OBJECT(panel), CONTROL_KEY)); }

GtkWidget* partOf(GtkWidget* panel, const char* key) {
    return GTK_WIDGET(g_object_get_data(G_OBJECT(panel), key));
}

void setStatus(GtkWidget* panel, const std::string& text) {
    gtk_label_set_text(GTK_LABEL(partOf(panel, STATUS_KEY)), text.c_str());
}

/// The status line when nothing else has to be said
void showDefaultStatus(GtkWidget* panel) {
    const std::string reason = controlOf(panel)->annotationUnavailableReason();
    setStatus(panel, reason.empty() ? std::string(_("Click an entry to put it in the middle of the page.")) : reason);
}

/// The panel a button inside it belongs to
GtkWidget* panelOf(GtkWidget* widget) {
    for (GtkWidget* w = widget; w; w = gtk_widget_get_parent(w)) {
        if (g_strcmp0(gtk_widget_get_name(w), ANNOTATION_BANK_NAME) == 0) {
            return w;
        }
    }
    return nullptr;
}

std::size_t indexOf(GtkWidget* button) {
    return GPOINTER_TO_SIZE(g_object_get_data(G_OBJECT(button), INDEX_KEY));
}

void insertClicked(GtkButton* button, gpointer) {
    GtkWidget* panel = panelOf(GTK_WIDGET(button));
    if (!panel) {
        return;
    }
    Control* control = controlOf(panel);
    const auto& entries = control->getAnnotationBank().all();
    const std::size_t index = indexOf(GTK_WIDGET(button));
    if (index >= entries.size()) {
        return;
    }
    const Annotation chosen = entries[index];  // a copy: inserting may rebuild what the bank hands out
    if (!control->insertAnnotation(chosen)) {
        refreshAnnotationBankPanel(panel);  // the reason it could not, on the buttons and the status line
        return;
    }
    // The teacher sees it land; the popover would cover the middle of the page
    gtk_popover_popdown(GTK_POPOVER(partOf(panel, POPOVER_KEY)));
}

void removeClicked(GtkButton* button, gpointer) {
    GtkWidget* panel = panelOf(GTK_WIDGET(button));
    if (!panel) {
        return;
    }
    const std::size_t index = indexOf(GTK_WIDGET(button));
    controlOf(panel)->updateAnnotationBank([index](AnnotationBank& bank) { bank.remove(index); });
    refreshAnnotationBankPanel(panel);
    // The removed row took the keyboard with it: keep it in the panel
    gtk_widget_grab_focus(partOf(panel, ENTRY_KEY));
}

void addClicked(GtkWidget* panel) {
    GtkWidget* entry = partOf(panel, ENTRY_KEY);
    const std::string text = gtk_entry_get_text(GTK_ENTRY(entry));
    Control* control = controlOf(panel);
    // In the colour Feedback comments are written in
    const auto colour = uint32_t(control->getToolHandler()->getTeacherStampColor()) & 0xFFFFFFU;
    AnnotationBank::AddResult result = AnnotationBank::AddResult::Empty;
    control->updateAnnotationBank([&](AnnotationBank& bank) { result = bank.addComment(text, colour); });
    refreshAnnotationBankPanel(panel);
    switch (result) {
        case AnnotationBank::AddResult::Added:
            gtk_entry_set_text(GTK_ENTRY(entry), "");
            setStatus(panel, _("Added to the bank."));
            break;
        case AnnotationBank::AddResult::Empty:
            setStatus(panel, _("Type a comment first."));
            break;
        case AnnotationBank::AddResult::Full:
            setStatus(panel, FS(_F("The bank is full ({1} entries). Remove one first.") % AnnotationBank::MAX_ENTRIES));
            break;
        case AnnotationBank::AddResult::Duplicate:
            setStatus(panel, _("That comment is already in the bank."));
            break;
    }
}
}  // namespace

GtkWidget* createAnnotationBankPanel(Control* control, GtkPopover* popover) {
    GtkWidget* panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_name(panel, ANNOTATION_BANK_NAME);
    gtk_widget_set_margin_start(panel, 8);
    gtk_widget_set_margin_end(panel, 8);
    gtk_widget_set_margin_top(panel, 8);
    gtk_widget_set_margin_bottom(panel, 8);
    g_object_set_data(G_OBJECT(panel), CONTROL_KEY, control);
    g_object_set_data(G_OBJECT(panel), POPOVER_KEY, popover);

    GtkWidget* status = gtk_label_new(nullptr);
    gtk_widget_set_name(status, "utnAnnotationBankStatus");
    gtk_label_set_line_wrap(GTK_LABEL(status), true);
    gtk_label_set_xalign(GTK_LABEL(status), 0);
    gtk_box_append(GTK_BOX(panel), status);
    g_object_set_data(G_OBJECT(panel), STATUS_KEY, status);

    GtkWidget* scroller = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroller), 96);
    gtk_widget_set_vexpand(scroller, true);
    GtkWidget* entries = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_name(entries, "utnAnnotationBankEntries");
    utn::setAccessibleName(entries, _("Annotation Bank entries"));
    gtk_container_add(GTK_CONTAINER(scroller), entries);
    gtk_box_append(GTK_BOX(panel), scroller);
    g_object_set_data(G_OBJECT(panel), ENTRIES_KEY, entries);

    GtkWidget* addRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* entry = gtk_entry_new();
    gtk_widget_set_name(entry, "utnAnnotationBankEntry");
    gtk_widget_set_hexpand(entry, true);
    gtk_entry_set_max_length(GTK_ENTRY(entry), static_cast<gint>(AnnotationBank::MAX_TEXT_BYTES));
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), _("Your own comment"));
    utn::setAccessibleName(entry, _("New comment for the bank"));
    gtk_box_append(GTK_BOX(addRow), entry);
    g_object_set_data(G_OBJECT(panel), ENTRY_KEY, entry);
    GtkWidget* add = gtk_button_new_with_label(_("Add to bank"));
    gtk_widget_set_tooltip_text(add, _("Keep this comment in the bank for next time"));
    gtk_box_append(GTK_BOX(addRow), add);
    gtk_box_append(GTK_BOX(panel), addRow);
    g_signal_connect_swapped(add, "clicked", G_CALLBACK(addClicked), panel);
    // Enter in the box adds, like the button
    g_signal_connect_swapped(entry, "activate", G_CALLBACK(addClicked), panel);

    GtkWidget* restore = gtk_button_new_with_label(_("Restore defaults"));
    gtk_widget_set_tooltip_text(restore, _("Bring back tick, cross, star, \"See me\" and \"Well done\"; your own comments go"));
    gtk_widget_set_halign(restore, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(panel), restore);
    g_signal_connect_swapped(restore, "clicked", G_CALLBACK(+[](GtkWidget* panel) {
                                 controlOf(panel)->updateAnnotationBank(
                                         [](AnnotationBank& bank) { bank.restoreDefaults(); });
                                 refreshAnnotationBankPanel(panel);
                             }),
                             panel);

    // Rebuilt whenever the popover opens: the bank may have changed, and inserting may have become (un)available
    g_signal_connect_object(popover, "show", G_CALLBACK(+[](GtkWidget*, gpointer panel) {
                                refreshAnnotationBankPanel(GTK_WIDGET(panel));
                            }),
                            panel, GConnectFlags(0));
    refreshAnnotationBankPanel(panel);
    return panel;
}

void refreshAnnotationBankPanel(GtkWidget* panel) {
    Control* control = controlOf(panel);
    GtkWidget* entries = partOf(panel, ENTRIES_KEY);
    GList* rows = gtk_container_get_children(GTK_CONTAINER(entries));
    for (GList* row = rows; row; row = row->next) {
        gtk_widget_destroy(GTK_WIDGET(row->data));
    }
    g_list_free(rows);

    const std::string reason = control->annotationUnavailableReason();
    const auto& bank = control->getAnnotationBank().all();
    for (std::size_t i = 0; i < bank.size(); ++i) {
        const Annotation& a = bank[i];
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        GtkWidget* insert = gtk_button_new_with_label(a.label.c_str());
        gtk_widget_set_hexpand(insert, true);
        gtk_widget_set_focus_on_click(insert, false);  // keyboard users can still Tab to it
        if (GtkWidget* label = gtk_bin_get_child(GTK_BIN(insert)); GTK_IS_LABEL(label)) {
            gtk_label_set_xalign(GTK_LABEL(label), 0);
            gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
        }
        utn::setAccessibleName(insert, a.label.c_str());  // exactly what it reads
        gtk_widget_set_sensitive(insert, reason.empty());
        gtk_widget_set_tooltip_text(insert,
                                    reason.empty() ? _("Put it in the middle of the page (Undo takes it away)") :
                                                     reason.c_str());
        g_object_set_data(G_OBJECT(insert), INDEX_KEY, GSIZE_TO_POINTER(i));
        g_signal_connect(insert, "clicked", G_CALLBACK(insertClicked), nullptr);
        gtk_box_append(GTK_BOX(row), insert);

        GtkWidget* remove = gtk_button_new_with_label(_("Remove"));
        gtk_widget_set_focus_on_click(remove, false);
        const std::string removeName = std::string(_("Remove")) + " " + a.label;
        utn::setAccessibleName(remove, removeName.c_str());
        gtk_widget_set_tooltip_text(remove, _("Take this entry out of the bank (nothing on the page changes)"));
        g_object_set_data(G_OBJECT(remove), INDEX_KEY, GSIZE_TO_POINTER(i));
        g_signal_connect(remove, "clicked", G_CALLBACK(removeClicked), nullptr);
        gtk_box_append(GTK_BOX(row), remove);
        gtk_box_append(GTK_BOX(entries), row);
    }
    if (bank.empty()) {
        GtkWidget* empty = gtk_label_new(_("The bank is empty. Add a comment below or restore the defaults."));
        gtk_label_set_line_wrap(GTK_LABEL(empty), true);
        gtk_label_set_xalign(GTK_LABEL(empty), 0);
        gtk_box_append(GTK_BOX(entries), empty);
    }
    gtk_widget_show_all(entries);
    showDefaultStatus(panel);
}

}  // namespace utn
