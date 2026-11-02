#include "UndoHistoryPanel.h"

#include <algorithm>  // for max, min
#include <cstddef>   // for size_t
#include <optional>  // for optional
#include <string>    // for string

#include "control/Control.h"                 // for Control
#include "control/actions/ActionDatabase.h"  // for ActionDatabase
#include "gui/MainWindow.h"                  // for MainWindow
#include "undo/UndoRedoHandler.h"            // for UndoRedoHandler, UndoRedoListener
#include "util/gtk4_helper.h"                // for gtk_box_append
#include "util/i18n.h"                       // for _, FS, _F

#include "ToolMenuHandler.h"  // for ToolMenuHandler
#include "ToolStudentView.h"  // for ToolStudentView
#include "UtnWidgets.h"       // for setAccessibleName

namespace utn {

namespace {
constexpr const char* STATE_KEY = "utn-history-state";
constexpr const char* SIDE_KEY = "utn-history-side";    ///< on a step row: 1 Undo, 2 Redo; absent on the "You are here" row
constexpr const char* STEPS_KEY = "utn-history-steps";  ///< on a step row: how many presses reach it
constexpr int LABEL_CHARS = 40;                         ///< long descriptions are cut here; the tooltip has them whole
constexpr int BUTTON_CHARS = 16;  ///< Undo and Redo sit side by side: each gets less than half the width

struct Jump {
    HistorySide side;
    std::size_t total;
    std::size_t done = 0;
    /// The history as the last step left it: anything else at the next step means it was changed meanwhile
    std::size_t undoCount = 0;
    std::size_t redoCount = 0;
};

/// Everything a panel needs, alive exactly as long as the panel
struct PanelState final: UndoRedoListener {
    Control* control = nullptr;
    GtkPopover* popover = nullptr;
    GtkWidget* panel = nullptr;
    GtkWidget* status = nullptr;
    GtkWidget* undoButton = nullptr;
    GtkWidget* redoButton = nullptr;
    GtkWidget* scroller = nullptr;
    GtkWidget* list = nullptr;
    GtkWidget* hidden = nullptr;  ///< "12 earlier steps are not listed"
    guint refreshSource = 0;
    guint jumpSource = 0;
    guint focusSource = 0;
    std::optional<Jump> jump;
    /// What the last thing the teacher did here came to; shown until the popover opens again or the history changes
    std::string message;
    std::size_t messageUndoCount = 0;  ///< The history as the message found it
    std::size_t messageRedoCount = 0;
    /// After a refresh the keyboard goes to this row (side, steps), if it is listed
    std::optional<std::pair<HistorySide, std::size_t>> focusAfterRefresh;

    ~PanelState() override {
        if (refreshSource) {
            g_source_remove(refreshSource);
        }
        if (jumpSource) {
            g_source_remove(jumpSource);
        }
        if (focusSource) {
            g_source_remove(focusSource);
        }
        if (auto* handler = control->getUndoRedoHandler()) {
            handler->removeUndoRedoListener(this);
        }
    }

    void undoRedoChanged() override { scheduleRefresh(); }
    void undoRedoPageChanged(PageRef) override {}

    /// Coalesced, and outside the handler's own call: the rows may be the widgets that started the change
    void scheduleRefresh() {
        if (refreshSource) {
            return;
        }
        refreshSource = g_idle_add_full(
                G_PRIORITY_HIGH_IDLE,
                +[](gpointer data) -> gboolean {
                    auto* self = static_cast<PanelState*>(data);
                    self->refreshSource = 0;
                    refreshUndoHistoryPanel(self->panel);
                    return G_SOURCE_REMOVE;
                },
                this, nullptr);
    }
};

/// Keep @p text on the status line for as long as the history stays as it is now
void say(PanelState* s, std::string text) {
    s->message = std::move(text);
    s->messageUndoCount = s->control->getUndoRedoHandler()->undoCount();
    s->messageRedoCount = s->control->getUndoRedoHandler()->redoCount();
}

PanelState* stateOf(GtkWidget* panel) {
    return panel ? static_cast<PanelState*>(g_object_get_data(G_OBJECT(panel), STATE_KEY)) : nullptr;
}

/// The status line is a status bar for screen readers: every message is set anew, so it is read even when the same
/// message comes twice in a row
void setStatus(PanelState* s, const std::string& text) {
    gtk_label_set_text(GTK_LABEL(s->status), "");
    gtk_label_set_text(GTK_LABEL(s->status), text.c_str());
}

std::string summary(Control* control) {
    UndoRedoHandler* handler = control->getUndoRedoHandler();
    const auto undo = handler->undoTexts(1);
    const auto redo = handler->redoTexts(1);
    std::string text = undo.empty() ? std::string(_("Nothing to undo yet.")) :
                                      FS(_F("Undo takes away: {1}.") % stepText(undo.front()));
    if (!redo.empty()) {
        text += " " + FS(_F("Redo brings back: {1}.") % stepText(redo.front()));
    }
    return text;
}

void setLabelText(GtkWidget* button, const std::string& text) {
    GtkWidget* label = gtk_bin_get_child(GTK_BIN(button));
    gtk_label_set_text(GTK_LABEL(label), text.c_str());
    utn::setAccessibleName(button, text.c_str());  // exactly what it reads
}

GtkWidget* newEllipsizedLabel(const std::string& text) {
    GtkWidget* label = gtk_label_new(text.c_str());
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(label), LABEL_CHARS);  // the natural width too: the popover stays narrow
    return label;
}

GtkWidget* newEllipsizedButton() {
    GtkWidget* button = gtk_button_new();
    GtkWidget* label = newEllipsizedLabel("");
    gtk_label_set_max_width_chars(GTK_LABEL(label), BUTTON_CHARS);
    gtk_container_add(GTK_CONTAINER(button), label);
    gtk_widget_set_focus_on_click(button, false);  // keyboard users can still Tab to it
    return button;
}

/// The list row that holds @p widget, or nullptr
GtkListBoxRow* rowHolding(PanelState* s, GtkWidget* widget) {
    for (GtkWidget* w = widget; w; w = gtk_widget_get_parent(w)) {
        if (GTK_IS_LIST_BOX_ROW(w) && gtk_widget_get_parent(w) == s->list) {
            return GTK_LIST_BOX_ROW(w);
        }
    }
    return nullptr;
}

GtkListBoxRow* rowFor(PanelState* s, HistorySide side, std::size_t steps) {
    const int wantedSide = side == HistorySide::Undo ? 1 : 2;
    for (int i = 0;; ++i) {
        GtkListBoxRow* row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(s->list), i);
        if (!row) {
            return nullptr;
        }
        if (GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), SIDE_KEY)) == wantedSide &&
            GPOINTER_TO_SIZE(g_object_get_data(G_OBJECT(row), STEPS_KEY)) == steps) {
            return row;
        }
    }
}

const char* gateOrNothingReason(PanelState* s, Action action, std::string& out) {
    out = gateReason(undoHistoryGate(s->control));
    if (out.empty() && !s->control->getActionDatabase()->isActionEnabled(action)) {
        out = action == Action::UNDO ? _("Nothing to undo.") : _("Nothing to redo.");
    }
    return out.c_str();
}

void stopJump(PanelState* s, const std::string& why) {
    if (s->jumpSource) {
        g_source_remove(s->jumpSource);
        s->jumpSource = 0;
    }
    if (!s->jump) {
        return;
    }
    const Jump j = *s->jump;
    s->jump.reset();
    say(s, FS(_F("Stopped after {1} of {2} steps: {3}") % j.done % j.total % why));
    refreshUndoHistoryPanel(s->panel);
}

/// One step of a jump: the ordinary Undo (Redo) action, exactly what Ctrl+Z (Ctrl+Y) runs, then a check that it moved
/// the history by one step and reported no failure. Anything else stops the jump where it is.
gboolean jumpTick(gpointer data) {
    auto* s = static_cast<PanelState*>(data);
    if (!s->jump) {
        s->jumpSource = 0;
        return G_SOURCE_REMOVE;
    }
    Jump& j = *s->jump;
    const Action action = j.side == HistorySide::Undo ? Action::UNDO : Action::REDO;
    std::string why;
    if (*gateOrNothingReason(s, action, why)) {
        s->jumpSource = 0;
        stopJump(s, why);
        return G_SOURCE_REMOVE;
    }
    UndoRedoHandler* handler = s->control->getUndoRedoHandler();
    const std::size_t undoBefore = handler->undoCount();
    const std::size_t redoBefore = handler->redoCount();
    if (undoBefore != j.undoCount || redoBefore != j.redoCount) {
        // A step was made, undone or cleared between two of ours: the step the teacher chose is not where it was
        s->jumpSource = 0;
        stopJump(s, _("the history changed"));
        return G_SOURCE_REMOVE;
    }
    s->control->getActionDatabase()->fireActivateAction(action);
    const bool moved = j.side == HistorySide::Undo ?
                               handler->undoCount() + 1 == undoBefore && handler->redoCount() == redoBefore + 1 :
                               handler->redoCount() + 1 == redoBefore && handler->undoCount() == undoBefore + 1;
    if (!moved || !handler->lastStepSucceeded()) {
        s->jumpSource = 0;
        stopJump(s, moved ? _("a step could not be completed") : _("the history changed"));
        return G_SOURCE_REMOVE;
    }
    ++j.done;
    j.undoCount = handler->undoCount();
    j.redoCount = handler->redoCount();
    if (j.done < j.total) {
        setStatus(s, FS(_F("{1} of {2} steps…") % j.done % j.total));
        return G_SOURCE_CONTINUE;
    }
    s->jumpSource = 0;
    const HistorySide side = j.side;
    const std::size_t total = j.total;
    s->jump.reset();
    say(s, side == HistorySide::Undo ? FS(_F("Undid {1} steps.") % total) : FS(_F("Redid {1} steps.") % total));
    if (total == 1) {
        say(s, side == HistorySide::Undo ? _("Undid 1 step.") : _("Redid 1 step."));
    }
    // The keyboard stays with the step that was chosen: it is now the next Redo (or the next Undo)
    s->focusAfterRefresh = std::make_pair(side == HistorySide::Undo ? HistorySide::Redo : HistorySide::Undo,
                                          std::size_t(1));
    refreshUndoHistoryPanel(s->panel);
    return G_SOURCE_REMOVE;
}

void startJump(PanelState* s, HistorySide side, std::size_t steps) {
    if (s->jump) {
        setStatus(s, _("Wait: the last choice is still being carried out."));
        return;
    }
    const Action action = side == HistorySide::Undo ? Action::UNDO : Action::REDO;
    std::string why;
    if (*gateOrNothingReason(s, action, why)) {
        setStatus(s, why);
        return;
    }
    UndoRedoHandler* handler = s->control->getUndoRedoHandler();
    if (steps == 0 || steps > (side == HistorySide::Undo ? handler->undoCount() : handler->redoCount())) {
        refreshUndoHistoryPanel(s->panel);  // the list was out of date
        return;
    }
    s->jump = Jump{side, steps, 0, handler->undoCount(), handler->redoCount()};
    s->message.clear();
    setStatus(s, side == HistorySide::Undo ? FS(_F("Undoing {1} steps…") % steps) : FS(_F("Redoing {1} steps…") % steps));
    // One step per main loop turn: the page is redrawn and the window kept up to date between steps, as between key
    // presses, and a save or the Blank cover that starts meanwhile stops the rest
    s->jumpSource = g_idle_add(jumpTick, s);
}

void rowActivated(GtkListBox*, GtkListBoxRow* row, gpointer data) {
    auto* s = static_cast<PanelState*>(data);
    const int side = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), SIDE_KEY));
    if (side == 0) {
        setStatus(s, _("This is where you are now. Steps above it are undone, steps below it are done."));
        return;
    }
    startJump(s, side == 1 ? HistorySide::Undo : HistorySide::Redo,
              GPOINTER_TO_SIZE(g_object_get_data(G_OBJECT(row), STEPS_KEY)));
}

/// Up, Down, Home, End, Page Up and Page Down move the keyboard between rows; Enter and Space choose the row. Done here
/// rather than left to GtkListBox: its keys start from a cursor row that a row given the keyboard by the panel itself
/// (after opening or a rebuild) does not get.
gboolean listKeyPressed(GtkWidget*, GdkEventKey* event, gpointer data) {
    auto* s = static_cast<PanelState*>(data);
    if (event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SHIFT_MASK)) {
        return false;  // Ctrl+Z and the like stay with the window; Shift+Tab with the focus chain
    }
    GtkWidget* top = gtk_widget_get_toplevel(s->list);
    GtkListBoxRow* current = GTK_IS_WINDOW(top) ? rowHolding(s, gtk_window_get_focus(GTK_WINDOW(top))) : nullptr;
    if (!current) {
        return false;
    }
    const int index = gtk_list_box_row_get_index(current);
    int last = index;
    while (gtk_list_box_get_row_at_index(GTK_LIST_BOX(s->list), last + 1)) {
        ++last;
    }
    constexpr int PAGE = 10;
    int target = index;
    switch (event->keyval) {
        case GDK_KEY_Up:
        case GDK_KEY_KP_Up:
            target = index - 1;
            break;
        case GDK_KEY_Down:
        case GDK_KEY_KP_Down:
            target = index + 1;
            break;
        case GDK_KEY_Home:
        case GDK_KEY_KP_Home:
            target = 0;
            break;
        case GDK_KEY_End:
        case GDK_KEY_KP_End:
            target = last;
            break;
        case GDK_KEY_Page_Up:
        case GDK_KEY_KP_Page_Up:
            target = index - PAGE;
            break;
        case GDK_KEY_Page_Down:
        case GDK_KEY_KP_Page_Down:
            target = index + PAGE;
            break;
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
        case GDK_KEY_ISO_Enter:
        case GDK_KEY_space:
        case GDK_KEY_KP_Space:
            g_signal_emit_by_name(s->list, "row-activated", current);
            return true;
        default:
            return false;
    }
    target = std::max(0, std::min(target, last));  // stays at the ends rather than leaving the list
    if (GtkListBoxRow* row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(s->list), target)) {
        gtk_widget_grab_focus(GTK_WIDGET(row));
    }
    return true;
}

void stepClicked(PanelState* s, HistorySide side) {
    if (s->jump) {
        setStatus(s, _("Wait: the last choice is still being carried out."));
        return;
    }
    const Action action = side == HistorySide::Undo ? Action::UNDO : Action::REDO;
    std::string why;
    if (*gateOrNothingReason(s, action, why)) {
        setStatus(s, why);
        return;
    }
    UndoRedoHandler* handler = s->control->getUndoRedoHandler();
    const auto texts = side == HistorySide::Undo ? handler->undoTexts(1) : handler->redoTexts(1);
    const std::string what = texts.empty() ? std::string() : stepText(texts.front());
    s->control->getActionDatabase()->fireActivateAction(action);
    say(s, side == HistorySide::Undo ? FS(_F("Undid: {1}.") % what) : FS(_F("Redid: {1}.") % what));
    refreshUndoHistoryPanel(s->panel);
}

void addStepRow(PanelState* s, const HistoryRow& step) {
    GtkWidget* row = gtk_list_box_row_new();
    const std::string label = rowLabel(step);
    GtkWidget* text = newEllipsizedLabel(label);
    gtk_widget_set_margin_start(text, 6);
    gtk_widget_set_margin_end(text, 6);
    gtk_widget_set_margin_top(text, 3);
    gtk_widget_set_margin_bottom(text, 3);
    if (step.side == HistorySide::Redo) {
        gtk_widget_add_css_class(text, "dim-label");  // undone: not on the page now
    }
    gtk_container_add(GTK_CONTAINER(row), text);
    gtk_widget_add_css_class(row, step.side == HistorySide::Undo ? "utn-history-done" : "utn-history-undone");
    const std::string name = rowAccessibleName(step);
    utn::setAccessibleName(row, name.c_str());
    gtk_widget_set_tooltip_text(row, name.c_str());  // the whole description, also when the row cuts it short
    g_object_set_data(G_OBJECT(row), SIDE_KEY, GINT_TO_POINTER(step.side == HistorySide::Undo ? 1 : 2));
    g_object_set_data(G_OBJECT(row), STEPS_KEY, GSIZE_TO_POINTER(step.steps));
    gtk_list_box_insert(GTK_LIST_BOX(s->list), row, -1);
}

void addHereRow(PanelState* s) {
    GtkWidget* row = gtk_list_box_row_new();
    GtkWidget* text = gtk_label_new(_("You are here"));
    gtk_label_set_xalign(GTK_LABEL(text), 0);
    gtk_widget_set_margin_start(text, 6);
    gtk_widget_set_margin_top(text, 2);
    gtk_widget_set_margin_bottom(text, 2);
    gtk_widget_add_css_class(text, "utn-history-here");
    gtk_container_add(GTK_CONTAINER(row), text);
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), false);
    const char* name = _("You are here: Undo starts below this line, Redo above it");
    utn::setAccessibleName(row, name);
    gtk_widget_set_tooltip_text(row, name);
    gtk_list_box_insert(GTK_LIST_BOX(s->list), row, -1);
}

void updateButton(PanelState* s, GtkWidget* button, Action action, const std::string& label, const char* does) {
    setLabelText(button, label);
    std::string why;
    gateOrNothingReason(s, action, why);
    // Still focusable when it cannot run: the keyboard and screen readers find out why
    const std::string description = why.empty() ? std::string(does) : why;
    atk_object_set_description(gtk_widget_get_accessible(button), description.c_str());
    gtk_widget_set_tooltip_text(button, (label + ": " + description).c_str());
    GtkWidget* text = gtk_bin_get_child(GTK_BIN(button));
    if (why.empty()) {
        gtk_widget_remove_css_class(text, "dim-label");
    } else {
        gtk_widget_add_css_class(text, "dim-label");
    }
}
}  // namespace

HistoryGate undoHistoryGate(Control* control) {
    bool cover = false;
    bool quietPresent = false;
    if (MainWindow* win = control->getWindow()) {
        if (auto* menus = win->getToolMenuHandler()) {
            if (auto* students = menus->getStudentView()) {
                cover = students->isTeacherCoverShown();
            }
        }
        quietPresent = win->presentKeepsToolsAway();
    }
    return historyGate(control->isBlocked(), cover, quietPresent);
}

GtkWidget* createUndoHistoryPanel(Control* control, GtkPopover* popover) {
    auto* s = new PanelState;
    s->control = control;
    s->popover = popover;
    GtkWidget* panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    s->panel = panel;
    gtk_widget_set_name(panel, UNDO_HISTORY_NAME);
    gtk_widget_set_margin_start(panel, 10);
    gtk_widget_set_margin_end(panel, 10);
    gtk_widget_set_margin_top(panel, 10);
    gtk_widget_set_margin_bottom(panel, 10);
    g_object_set_data(G_OBJECT(panel), STATE_KEY, s);
    g_signal_connect(panel, "destroy", G_CALLBACK(+[](GtkWidget* w, gpointer data) {
                         g_object_set_data(G_OBJECT(w), STATE_KEY, nullptr);
                         delete static_cast<PanelState*>(data);
                     }),
                     s);

    s->status = gtk_label_new(nullptr);
    gtk_widget_set_name(s->status, "utnUndoHistoryStatus");
    gtk_label_set_line_wrap(GTK_LABEL(s->status), true);
    gtk_label_set_max_width_chars(GTK_LABEL(s->status), LABEL_CHARS);
    gtk_label_set_xalign(GTK_LABEL(s->status), 0);
    // Read out when it changes, like a status bar
    atk_object_set_role(gtk_widget_get_accessible(s->status), ATK_ROLE_STATUSBAR);
    gtk_box_append(GTK_BOX(panel), s->status);

    GtkWidget* buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_set_homogeneous(GTK_BOX(buttons), true);
    s->undoButton = newEllipsizedButton();
    gtk_widget_set_name(s->undoButton, "utnUndoHistoryUndo");
    s->redoButton = newEllipsizedButton();
    gtk_widget_set_name(s->redoButton, "utnUndoHistoryRedo");
    gtk_box_append(GTK_BOX(buttons), s->undoButton);
    gtk_box_append(GTK_BOX(buttons), s->redoButton);
    gtk_box_append(GTK_BOX(panel), buttons);
    g_signal_connect(s->undoButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         stepClicked(static_cast<PanelState*>(data), HistorySide::Undo);
                     }),
                     s);
    g_signal_connect(s->redoButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                         stepClicked(static_cast<PanelState*>(data), HistorySide::Redo);
                     }),
                     s);

    GtkWidget* hint = gtk_label_new(_("Newest first. Choose a step to undo back to before it, or an undone step to redo "
                                      "up to it."));
    gtk_label_set_line_wrap(GTK_LABEL(hint), true);
    gtk_label_set_max_width_chars(GTK_LABEL(hint), LABEL_CHARS);
    gtk_label_set_xalign(GTK_LABEL(hint), 0);
    gtk_widget_add_css_class(hint, "utn-context-hint");
    gtk_box_append(GTK_BOX(panel), hint);

    s->scroller = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(s->scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(s->scroller), 220);
    gtk_widget_set_vexpand(s->scroller, true);
    s->list = gtk_list_box_new();
    gtk_widget_set_name(s->list, "utnUndoHistoryList");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(s->list), GTK_SELECTION_NONE);
    gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(s->list), true);
    utn::setAccessibleName(s->list, _("Undo history, newest first"));
    gtk_container_add(GTK_CONTAINER(s->scroller), s->list);
    gtk_box_append(GTK_BOX(panel), s->scroller);
    g_signal_connect(s->list, "row-activated", G_CALLBACK(rowActivated), s);
    g_signal_connect(s->list, "key-press-event", G_CALLBACK(listKeyPressed), s);

    s->hidden = gtk_label_new(nullptr);
    gtk_label_set_line_wrap(GTK_LABEL(s->hidden), true);
    gtk_label_set_max_width_chars(GTK_LABEL(s->hidden), LABEL_CHARS);
    gtk_label_set_xalign(GTK_LABEL(s->hidden), 0);
    gtk_widget_add_css_class(s->hidden, "dim-label");
    gtk_widget_set_no_show_all(s->hidden, true);
    gtk_box_append(GTK_BOX(panel), s->hidden);

    control->getUndoRedoHandler()->addUndoRedoListener(s);
    // Opening shows the history as it is and says afresh what can run
    g_signal_connect_object(popover, "show", G_CALLBACK(+[](GtkWidget*, gpointer panel) {
                                if (auto* state = stateOf(GTK_WIDGET(panel))) {
                                    state->message.clear();
                                }
                                refreshUndoHistoryPanel(GTK_WIDGET(panel));
                            }),
                            panel, GConnectFlags(0));
    // The Blank cover makes the window's content insensitive: look again, and put the list away if it must not show
    g_signal_connect(panel, "state-flags-changed", G_CALLBACK(+[](GtkWidget* w, GtkStateFlags before, gpointer) {
                         const bool changed = ((before ^ gtk_widget_get_state_flags(w)) & GTK_STATE_FLAG_INSENSITIVE) != 0;
                         if (auto* state = stateOf(w); state && changed) {
                             state->scheduleRefresh();
                         }
                     }),
                     nullptr);
    refreshUndoHistoryPanel(panel);
    return panel;
}

void refreshUndoHistoryPanel(GtkWidget* panel) {
    PanelState* s = stateOf(panel);
    if (!s) {
        return;
    }
    if (s->refreshSource) {
        g_source_remove(s->refreshSource);
        s->refreshSource = 0;
    }
    UndoRedoHandler* handler = s->control->getUndoRedoHandler();
    const HistoryGate gate = undoHistoryGate(s->control);

    // Where the keyboard is in the list, to put it back on the same place once the rows are new
    std::optional<int> focusedIndex;
    if (GtkWidget* top = gtk_widget_get_toplevel(panel); GTK_IS_WINDOW(top)) {
        if (GtkListBoxRow* row = rowHolding(s, gtk_window_get_focus(GTK_WINDOW(top)))) {
            focusedIndex = gtk_list_box_row_get_index(row);
        }
    }

    GList* rows = gtk_container_get_children(GTK_CONTAINER(s->list));
    for (GList* r = rows; r; r = r->next) {
        gtk_widget_destroy(GTK_WIDGET(r->data));
    }
    g_list_free(rows);

    updateButton(s, s->undoButton, Action::UNDO, handler->undoDescription(), _("Takes away the newest done step"));
    updateButton(s, s->redoButton, Action::REDO, handler->redoDescription(), _("Brings back the newest undone step"));

    if (gateHidesList(gate)) {
        // Nothing of the history is on show while the class may be watching; the reason stays reachable
        if (s->jump) {
            const Jump j = *s->jump;
            s->jump.reset();
            if (s->jumpSource) {
                g_source_remove(s->jumpSource);
                s->jumpSource = 0;
            }
            say(s, FS(_F("Stopped after {1} of {2} steps: {3}") % j.done % j.total % gateReason(gate)));
        }
        gtk_widget_hide(s->scroller);
        gtk_widget_hide(s->hidden);
        setStatus(s, gateReason(gate));
        if (gtk_widget_get_visible(GTK_WIDGET(s->popover))) {
            gtk_popover_popdown(s->popover);
        }
        return;
    }
    gtk_widget_show(s->scroller);

    const UndoHistoryView view = buildUndoHistory(handler->undoTexts(UNDO_HISTORY_LIMIT), handler->undoCount(),
                                                  handler->redoTexts(UNDO_HISTORY_LIMIT), handler->redoCount());
    for (std::size_t i = 0; i < view.rows.size(); ++i) {
        if (i == view.positionIndex) {
            addHereRow(s);
        }
        addStepRow(s, view.rows[i]);
    }
    if (view.positionIndex == view.rows.size() && !view.rows.empty()) {
        addHereRow(s);  // everything listed is undone
    }
    if (view.rows.empty()) {
        GtkWidget* row = gtk_list_box_row_new();
        GtkWidget* text = gtk_label_new(_("No steps yet. What you change on the pages is listed here."));
        gtk_label_set_line_wrap(GTK_LABEL(text), true);
        gtk_label_set_max_width_chars(GTK_LABEL(text), LABEL_CHARS);
        gtk_label_set_xalign(GTK_LABEL(text), 0);
        gtk_container_add(GTK_CONTAINER(row), text);
        gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), false);
        gtk_list_box_insert(GTK_LIST_BOX(s->list), row, -1);
    }
    gtk_widget_show_all(s->list);

    std::string more;
    if (view.newerHidden > 0) {
        more = FS(_F("{1} more undone steps above are not listed.") % view.newerHidden);
    }
    if (view.olderHidden > 0) {
        more += (more.empty() ? "" : " ") + FS(_F("{1} earlier steps are not listed.") % view.olderHidden);
    }
    gtk_label_set_text(GTK_LABEL(s->hidden), more.c_str());
    gtk_widget_set_visible(s->hidden, !more.empty());

    if (s->jump) {
        setStatus(s, FS(_F("{1} of {2} steps…") % s->jump->done % s->jump->total));
    } else {
        const std::string reason = gateReason(gate);  // busy: the list can be read, nothing runs
        if (handler->undoCount() != s->messageUndoCount || handler->redoCount() != s->messageRedoCount) {
            s->message.clear();  // about a history that has changed since
        }
        std::string text = s->message.empty() ? summary(s->control) : s->message + " " + summary(s->control);
        setStatus(s, reason.empty() ? text : reason);
    }

    GtkListBoxRow* focus = nullptr;
    if (s->focusAfterRefresh) {
        focus = rowFor(s, s->focusAfterRefresh->first, s->focusAfterRefresh->second);
        s->focusAfterRefresh.reset();
        if (!focus && focusedIndex) {
            focus = gtk_list_box_get_row_at_index(GTK_LIST_BOX(s->list), *focusedIndex);
        }
    } else if (focusedIndex) {
        // The same place in the list, or its last row when the list got shorter
        focus = gtk_list_box_get_row_at_index(GTK_LIST_BOX(s->list), *focusedIndex);
        for (int i = *focusedIndex - 1; !focus && i >= 0; --i) {
            focus = gtk_list_box_get_row_at_index(GTK_LIST_BOX(s->list), i);
        }
    }
    if (focus) {
        gtk_widget_grab_focus(GTK_WIDGET(focus));
    }
}

bool isUndoHistoryJumping(GtkWidget* panel) {
    PanelState* s = stateOf(panel);
    return s && s->jump.has_value();
}

namespace {
GtkWidget* findDrawableNamed(GtkWidget* widget, const char* name) {
    if (!widget || !gtk_widget_get_visible(widget)) {
        return nullptr;
    }
    if (g_strcmp0(gtk_widget_get_name(widget), name) == 0 && gtk_widget_is_drawable(widget)) {
        return widget;
    }
    if (!GTK_IS_CONTAINER(widget)) {
        return nullptr;
    }
    GtkWidget* found = nullptr;
    GList* children = gtk_container_get_children(GTK_CONTAINER(widget));
    for (GList* c = children; c && !found; c = c->next) {
        found = findDrawableNamed(GTK_WIDGET(c->data), name);
    }
    g_list_free(children);
    return found;
}

GtkWidget* classroomMenu(Control* control) {
    GtkWindow* window = control->getGtkWindow();
    return window ? findDrawableNamed(GTK_WIDGET(window), CLASSROOM_MENU_NAME) : nullptr;
}
}  // namespace

std::string undoHistoryOpenReason(Control* control) {
    const HistoryGate gate = undoHistoryGate(control);
    if (gateHidesList(gate)) {
        return gateReason(gate);
    }
    if (!classroomMenu(control)) {
        return _("Unavailable: show the tool dock with Classroom in it (Teach or Mark)");
    }
    return {};
}

bool openUndoHistory(Control* control) {
    if (!undoHistoryOpenReason(control).empty()) {
        return false;
    }
    GtkWidget* menu = classroomMenu(control);
    auto* popover = GTK_POPOVER(gtk_menu_button_get_popover(GTK_MENU_BUTTON(menu)));
    GtkWidget* tabs = popover ? gtk_bin_get_child(GTK_BIN(popover)) : nullptr;
    if (!GTK_IS_NOTEBOOK(tabs)) {
        return false;
    }
    GtkWidget* panel = nullptr;
    const int pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(tabs));
    for (int i = 0; i < pages && !panel; ++i) {
        GtkWidget* page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(tabs), i);
        if (g_strcmp0(gtk_widget_get_name(page), UNDO_HISTORY_NAME) == 0) {
            panel = page;
            gtk_notebook_set_current_page(GTK_NOTEBOOK(tabs), i);
        }
    }
    PanelState* s = stateOf(panel);
    if (!s) {
        return false;
    }
    gtk_popover_popup(popover);  // refreshes the panel
    // The keyboard on what Undo would undo, else on the Undo button. Once the popover has settled: while it opens it
    // takes the keyboard for itself.
    if (!s->focusSource) {
        s->focusSource = g_idle_add(
                +[](gpointer data) -> gboolean {
                    auto* state = static_cast<PanelState*>(data);
                    state->focusSource = 0;
                    if (gtk_widget_get_visible(GTK_WIDGET(state->popover)) && gtk_widget_is_drawable(state->panel)) {
                        GtkListBoxRow* row = rowFor(state, HistorySide::Undo, 1);
                        gtk_widget_grab_focus(row ? GTK_WIDGET(row) : state->undoButton);
                    }
                    return G_SOURCE_REMOVE;
                },
                s);
    }
    return true;
}

}  // namespace utn
