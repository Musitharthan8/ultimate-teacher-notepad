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
constexpr const char* RESTORE_KEY = "utn-restore";
constexpr const char* CONFIRMING_KEY = "utn-confirming";
constexpr const char* INDEX_KEY = "utn-index";
constexpr const char* KIND_KEY = "utn-kind";  ///< "insert" or "remove" on the entry buttons
/// Messages wrap at about this many characters, so a long one never widens the popover
constexpr int TEXT_WIDTH_CHARS = 32;
/// Width of the entry list in pixels: a steady width, never wider than a narrow window
constexpr int LIST_MIN_WIDTH = 260;
constexpr int LIST_MAX_WIDTH = 380;

Control* controlOf(GtkWidget* panel) { return static_cast<Control*>(g_object_get_data(G_OBJECT(panel), CONTROL_KEY)); }

GtkWidget* partOf(GtkWidget* panel, const char* key) {
    return GTK_WIDGET(g_object_get_data(G_OBJECT(panel), key));
}

/// The status line is a status bar for screen readers: every message is set anew, so it is read even when the same
/// message comes twice in a row
void setStatus(GtkWidget* panel, const std::string& text) {
    GtkLabel* status = GTK_LABEL(partOf(panel, STATUS_KEY));
    gtk_label_set_text(status, "");
    gtk_label_set_text(status, text.c_str());
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

/// The Insert or Remove button of row @p index after a rebuild, or nullptr
GtkWidget* rowButton(GtkWidget* panel, std::size_t index, const char* kind) {
    GtkWidget* found = nullptr;
    GList* rows = gtk_container_get_children(GTK_CONTAINER(partOf(panel, ENTRIES_KEY)));
    for (GList* row = rows; row && !found; row = row->next) {
        if (!GTK_IS_CONTAINER(row->data)) {
            continue;
        }
        GList* buttons = gtk_container_get_children(GTK_CONTAINER(row->data));
        for (GList* b = buttons; b && !found; b = b->next) {
            auto* button = GTK_WIDGET(b->data);
            if (g_strcmp0(static_cast<const char*>(g_object_get_data(G_OBJECT(button), KIND_KEY)), kind) == 0 &&
                indexOf(button) == index) {
                found = button;
            }
        }
        g_list_free(buttons);
    }
    g_list_free(rows);
    return found;
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
        // Say why, and keep the keyboard on the same entry although the rows are rebuilt
        refreshAnnotationBankPanel(panel);
        if (GtkWidget* same = rowButton(panel, index, "insert")) {
            gtk_widget_grab_focus(same);
        }
        const std::string reason = control->annotationUnavailableReason();
        setStatus(panel, reason.empty() ? std::string(_("Nothing was put on the page.")) : reason);
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
    Control* control = controlOf(panel);
    const std::size_t index = indexOf(GTK_WIDGET(button));
    if (index >= control->getAnnotationBank().all().size()) {
        return;
    }
    const std::string label = control->getAnnotationBank().all()[index].label;
    control->updateAnnotationBank([index](AnnotationBank& bank) { bank.remove(index); });
    refreshAnnotationBankPanel(panel);
    // The removed row took the keyboard with it: the next row's Remove, else the one before, else the comment box
    GtkWidget* next = rowButton(panel, index, "remove");
    if (!next && index > 0) {
        next = rowButton(panel, index - 1, "remove");
    }
    gtk_widget_grab_focus(next ? next : partOf(panel, ENTRY_KEY));
    setStatus(panel, FS(_F("Removed: {1}") % label));
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
        case AnnotationBank::AddResult::Shortened:
            gtk_entry_set_text(GTK_ENTRY(entry), "");
            setStatus(panel, _("Added to the bank, shortened to fit."));
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

void setRestoreConfirming(GtkWidget* panel, bool confirming) {
    GtkWidget* restore = partOf(panel, RESTORE_KEY);
    g_object_set_data(G_OBJECT(restore), CONFIRMING_KEY, GINT_TO_POINTER(confirming ? 1 : 0));
    const char* label = confirming ? _("Confirm restore") : _("Restore defaults");
    gtk_button_set_label(GTK_BUTTON(restore), label);
    // Not colour alone (the words change too): the danger style says the next click throws the comments away
    if (confirming) {
        gtk_widget_add_css_class(restore, "destructive-action");
    } else {
        gtk_widget_remove_css_class(restore, "destructive-action");
    }
    utn::setAccessibleName(restore, label);
}

/// Two steps, without a dialog: the first click asks, the second replaces the bank
void restoreClicked(GtkWidget* panel) {
    GtkWidget* restore = partOf(panel, RESTORE_KEY);
    if (!GPOINTER_TO_INT(g_object_get_data(G_OBJECT(restore), CONFIRMING_KEY))) {
        setRestoreConfirming(panel, true);
        setStatus(panel, _("Click Confirm restore to go back to the built-in entries. Your own comments will go."));
        return;
    }
    controlOf(panel)->updateAnnotationBank([](AnnotationBank& bank) { bank.restoreDefaults(); });
    refreshAnnotationBankPanel(panel);  // also puts the button back to Restore defaults
    gtk_widget_grab_focus(restore);
    setStatus(panel, _("Defaults restored."));
}

/// Asking is forgotten when the keyboard leaves the button, so a later click never replaces the bank by surprise
gboolean restoreFocusOut(GtkWidget* restore, GdkEvent*, gpointer panel) {
    if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(restore), CONFIRMING_KEY))) {
        setRestoreConfirming(GTK_WIDGET(panel), false);
        showDefaultStatus(GTK_WIDGET(panel));
    }
    return GDK_EVENT_PROPAGATE;
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
    gtk_label_set_max_width_chars(GTK_LABEL(status), TEXT_WIDTH_CHARS);
    gtk_label_set_xalign(GTK_LABEL(status), 0);
    // Read out when it changes, like a status bar
    atk_object_set_role(gtk_widget_get_accessible(status), ATK_ROLE_STATUSBAR);
    gtk_box_append(GTK_BOX(panel), status);
    g_object_set_data(G_OBJECT(panel), STATUS_KEY, status);

    GtkWidget* scroller = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scroller), 96);
    // A steady width: long entries are cut with "..." (the tooltip has the whole text) instead of stretching the popover
    gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(scroller), LIST_MIN_WIDTH);
    gtk_scrolled_window_set_max_content_width(GTK_SCROLLED_WINDOW(scroller), LIST_MAX_WIDTH);
    gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(scroller), true);
    gtk_widget_set_vexpand(scroller, true);
    GtkWidget* entries = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_name(entries, "utnAnnotationBankEntries");
    utn::setAccessibleName(entries, _("Annotation Bank entries"));
    gtk_container_add(GTK_CONTAINER(scroller), entries);
    // Tab into a row below the fold scrolls it into view
    gtk_container_set_focus_vadjustment(GTK_CONTAINER(entries),
                                        gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroller)));
    gtk_box_append(GTK_BOX(panel), scroller);
    g_object_set_data(G_OBJECT(panel), ENTRIES_KEY, entries);

    GtkWidget* addRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* entry = gtk_entry_new();
    gtk_widget_set_name(entry, "utnAnnotationBankEntry");
    gtk_widget_set_hexpand(entry, true);
    gtk_entry_set_max_length(GTK_ENTRY(entry), static_cast<gint>(AnnotationBank::MAX_TEXT_BYTES));
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), _("Your own comment"));
    // Starts with the placeholder, the only words shown in the empty box
    utn::setAccessibleName(entry, _("Your own comment for the bank"));
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
    gtk_widget_set_tooltip_text(restore,
                                _("Bring back tick, cross, star, \"See me\" and \"Well done\"; your own comments go"));
    gtk_widget_set_halign(restore, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(panel), restore);
    g_object_set_data(G_OBJECT(panel), RESTORE_KEY, restore);
    g_signal_connect_swapped(restore, "clicked", G_CALLBACK(restoreClicked), panel);
    g_signal_connect(restore, "focus-out-event", G_CALLBACK(restoreFocusOut), panel);

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
    setRestoreConfirming(panel, false);

    // Unavailable entries stay focusable, so the keyboard and screen readers still reach them and hear why; clicking
    // one says why on the status line and Control refuses the insert
    const std::string reason = control->annotationUnavailableReason();
    const std::string hint = reason.empty() ? std::string(_("Puts it in the middle of the page; Undo takes it away")) :
                                              reason;
    const auto& bank = control->getAnnotationBank().all();
    for (std::size_t i = 0; i < bank.size(); ++i) {
        const Annotation& a = bank[i];
        GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);  // apart: Remove cannot be undone
        GtkWidget* insert = gtk_button_new_with_label(a.label.c_str());
        gtk_widget_set_hexpand(insert, true);
        gtk_widget_set_focus_on_click(insert, false);  // keyboard users can still Tab to it
        if (GtkWidget* label = gtk_bin_get_child(GTK_BIN(insert)); GTK_IS_LABEL(label)) {
            gtk_label_set_xalign(GTK_LABEL(label), 0);
            gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
            if (!reason.empty()) {
                gtk_widget_add_css_class(label, "dim-label");
            }
        }
        if (!reason.empty()) {
            // Still focusable, so the reason can be heard, but it must not look ready: the stylesheet draws a dashed
            // border, which does not depend on colour
            gtk_widget_add_css_class(insert, "utn-bank-unavailable");
        }
        utn::setAccessibleName(insert, a.label.c_str());  // exactly what it reads
        atk_object_set_description(gtk_widget_get_accessible(insert), hint.c_str());
        // The whole label too: a long one is cut short on the button
        gtk_widget_set_tooltip_text(insert, (a.label + ": " + hint).c_str());
        g_object_set_data(G_OBJECT(insert), INDEX_KEY, GSIZE_TO_POINTER(i));
        g_object_set_data(G_OBJECT(insert), KIND_KEY, const_cast<char*>("insert"));
        g_signal_connect(insert, "clicked", G_CALLBACK(insertClicked), nullptr);
        gtk_box_append(GTK_BOX(row), insert);

        GtkWidget* remove = gtk_button_new_with_label(_("Remove"));
        gtk_widget_set_focus_on_click(remove, false);
        const std::string removeName = std::string(_("Remove")) + " " + a.label;
        utn::setAccessibleName(remove, removeName.c_str());
        gtk_widget_set_tooltip_text(remove, _("Take this entry out of the bank (nothing on the page changes)"));
        g_object_set_data(G_OBJECT(remove), INDEX_KEY, GSIZE_TO_POINTER(i));
        g_object_set_data(G_OBJECT(remove), KIND_KEY, const_cast<char*>("remove"));
        g_signal_connect(remove, "clicked", G_CALLBACK(removeClicked), nullptr);
        gtk_box_append(GTK_BOX(row), remove);
        gtk_box_append(GTK_BOX(entries), row);
    }
    if (bank.empty()) {
        GtkWidget* empty = gtk_label_new(_("The bank is empty. Add a comment below or restore the defaults."));
        gtk_label_set_line_wrap(GTK_LABEL(empty), true);
        gtk_label_set_max_width_chars(GTK_LABEL(empty), TEXT_WIDTH_CHARS);
        gtk_label_set_xalign(GTK_LABEL(empty), 0);
        gtk_box_append(GTK_BOX(entries), empty);
    }
    gtk_widget_show_all(entries);
    showDefaultStatus(panel);
}

}  // namespace utn
