#include "CommandPalette.h"

#include <algorithm>
#include <string>
#include <utility>

#include "control/Control.h"
#include "control/ToolEnums.h"
#include "control/actions/ActionDatabase.h"
#include "control/settings/Settings.h"
#include "gui/MainWindow.h"
#include "gui/toolbarMenubar/model/ToolbarData.h"
#include "gui/toolbarMenubar/ToolLessonNavigator.h"
#include "gui/toolbarMenubar/ToolMenuHandler.h"
#include "gui/toolbarMenubar/ToolPresentationKit.h"
#include "gui/toolbarMenubar/ToolStudentView.h"
#include "gui/toolbarMenubar/UtnWidgets.h"
#include "model/Document.h"
#include "model/PageNavigation.h"
#include "model/XojPage.h"
#include "util/i18n.h"

namespace {
/// The shortcut a person would press for an action, as GTK shows it ("Ctrl+Z"); empty if it has none
std::string shortcutFor(Control* control, Action action) {
    auto* app = GTK_APPLICATION(gtk_window_get_application(control->getGtkWindow()));
    if (!app) {
        return {};
    }
    for (const char* prefix: {"win.", "app."}) {
        const std::string name = std::string(prefix) + Action_toString(action);
        gchar** accels = gtk_application_get_accels_for_action(app, name.c_str());
        std::string out;
        if (accels && accels[0]) {
            guint key = 0;
            GdkModifierType mods = static_cast<GdkModifierType>(0);
            gtk_accelerator_parse(accels[0], &key, &mods);
            if (key != 0) {
                gchar* label = gtk_accelerator_get_label(key, mods);
                out = label;
                g_free(label);
            }
        }
        g_strfreev(accels);
        if (!out.empty()) {
            return out;
        }
    }
    return {};
}

constexpr int PRIORITY_TOOL = 90;
constexpr int PRIORITY_PAGE = 80;
constexpr int PRIORITY_CLASSROOM = 75;
constexpr int PRIORITY_WORKSPACE = 70;
constexpr int PRIORITY_EDIT = 60;
constexpr int PRIORITY_VIEW = 40;
constexpr int PRIORITY_FILE = 30;
}  // namespace

CommandPalette::CommandPalette(Control* control, MainWindow* window): control(control), window(window) {}

CommandPalette::~CommandPalette() {
    closing = true;  // the main window is going away with us: the destroy handler must not touch it
    if (dialog) {
        gtk_widget_destroy(dialog);
    }
}

void CommandPalette::add(Entry entry) {
    // While a save or export runs, Control has disabled every action. A command that does not go through one (or that
    // fires a state change, which GAction does not hold back) must be as unavailable as the ones that do.
    entry.available = [this, available = std::move(entry.available)]() { return !control->isBlocked() && available(); };
    entries.push_back(std::move(entry));
}

void CommandPalette::addAction(const std::string& id, const std::string& title, const std::string& category,
                               const std::string& keywords, int priority, Action action, std::function<void()> run,
                               const std::string& unavailable) {
    Entry e;
    e.meta.id = id;
    e.meta.title = title;
    e.meta.category = category;
    e.meta.keywords = keywords;
    e.meta.shortcut = shortcutFor(control, action);
    e.meta.priority = priority;
    e.meta.unavailableReason = unavailable.empty() ? std::string(_("Not available right now")) : unavailable;
    e.available = [this, action]() { return control->getActionDatabase()->isActionEnabled(action); };
    e.run = std::move(run);
    add(std::move(e));
}

void CommandPalette::buildEntries() {
    entries.clear();
    auto* actions = control->getActionDatabase();
    auto fire = [this, actions](Action a) { return [actions, a]() { actions->fireActivateAction(a); }; };
    auto simple = [&](const char* id, const char* title, const char* category, const char* keywords, int priority,
                      Action a) { addAction(id, title, category, keywords, priority, a, fire(a)); };

    // Tools: each is the very action the tool rail button fires
    auto tool = [&](const char* id, const char* title, const char* keywords, ToolType type) {
        addAction(id, title, _("Tool"), keywords, PRIORITY_TOOL, Action::SELECT_TOOL,
                  [actions, type]() { actions->fireActivateAction(Action::SELECT_TOOL, type); });
    };
    tool("tool.pen", _("Pen"), "write draw ink handwriting", TOOL_PEN);
    tool("tool.highlighter", _("Highlighter"), "mark highlight", TOOL_HIGHLIGHTER);
    tool("tool.eraser", _("Eraser"), "rub out delete ink", TOOL_ERASER);
    tool("tool.text", _("Text"), "type keyboard words", TOOL_TEXT);
    tool("tool.select-rect", _("Select rectangle"), "selection move resize lasso", TOOL_SELECT_RECT);
    tool("tool.select-region", _("Select region"), "selection lasso freehand", TOOL_SELECT_REGION);
    tool("tool.hand", _("Hand"), "move scroll pan page", TOOL_HAND);
    tool("tool.laser", _("Laser pointer"), "point presentation pen", TOOL_LASER_POINTER_PEN);
    simple("edit.undo", _("Undo"), _("Edit"), "back step", PRIORITY_EDIT, Action::UNDO);
    simple("edit.redo", _("Redo"), _("Edit"), "forward again", PRIORITY_EDIT, Action::REDO);

    // Pages: the same actions the Navigation and Journal menus use
    const std::string page = _("Page");
    simple("page.goto", _("Go to page..."), page.c_str(), "jump number navigate", PRIORITY_PAGE, Action::GOTO_PAGE);
    simple("page.next", _("Next page"), page.c_str(), "forward down", PRIORITY_PAGE, Action::GOTO_NEXT);
    simple("page.previous", _("Previous page"), page.c_str(), "back up", PRIORITY_PAGE, Action::GOTO_PREVIOUS);
    simple("page.first", _("First page"), page.c_str(), "start beginning top", PRIORITY_PAGE, Action::GOTO_FIRST);
    simple("page.last", _("Last page"), page.c_str(), "end bottom", PRIORITY_PAGE, Action::GOTO_LAST);
    simple("page.next-annotated", _("Next page with ink"), page.c_str(), "annotated marked written",
           PRIORITY_PAGE - 5, Action::GOTO_NEXT_ANNOTATED_PAGE);
    simple("page.previous-annotated", _("Previous page with ink"), page.c_str(), "annotated marked written",
           PRIORITY_PAGE - 5, Action::GOTO_PREVIOUS_ANNOTATED_PAGE);
    simple("page.back", _("Back to the previous place"), page.c_str(), "history recent return location",
           PRIORITY_PAGE, Action::NAVIGATE_BACK);
    simple("page.forward", _("Forward to the next place"), page.c_str(), "history recent location",
           PRIORITY_PAGE - 5, Action::NAVIGATE_FORWARD);
    simple("page.new-after", _("Add page after this one"), page.c_str(), "insert new blank", PRIORITY_PAGE - 10,
           Action::NEW_PAGE_AFTER);
    simple("page.new-before", _("Add page before this one"), page.c_str(), "insert new blank",
           PRIORITY_PAGE - 10, Action::NEW_PAGE_BEFORE);
    simple("page.new-end", _("Add page at the end"), page.c_str(), "append new blank", PRIORITY_PAGE - 10,
           Action::NEW_PAGE_AT_END);
    simple("page.duplicate", _("Duplicate page"), page.c_str(), "copy clone", PRIORITY_PAGE - 10,
           Action::DUPLICATE_PAGE);
    simple("page.move-earlier", _("Move page earlier"), page.c_str(), "reorder up", PRIORITY_PAGE - 10,
           Action::MOVE_PAGE_TOWARDS_BEGINNING);
    simple("page.move-later", _("Move page later"), page.c_str(), "reorder down", PRIORITY_PAGE - 10,
           Action::MOVE_PAGE_TOWARDS_END);
    simple("page.delete", _("Delete page"), page.c_str(), "remove", PRIORITY_PAGE - 20, Action::DELETE_PAGE);

    {
        // The bookmark is a page label: it is not offered on a page whose label is the teacher's own
        PageRef current = control->getCurrentPage();
        const std::string label = current ? current->getUtnPageLabel() : std::string{};
        const auto toggle = utn::toggleBookmark(label);
        Entry e;
        e.meta.id = "page.bookmark";
        e.meta.title = toggle.change == utn::BookmarkChange::Remove ? _("Remove bookmark") : _("Bookmark this page");
        e.meta.category = page;
        e.meta.keywords = "mark star save place label";
        e.meta.priority = PRIORITY_PAGE;
        e.meta.unavailableReason = _("This page already has a label");
        e.available = [this]() {
            PageRef p = control->getCurrentPage();
            return p && utn::toggleBookmark(p->getUtnPageLabel()).change != utn::BookmarkChange::KeepLabel;
        };
        e.run = [this]() { ToolLessonNavigator::toggleBookmark(control); };
        add(std::move(e));

        Entry thumbs;
        thumbs.meta.id = "page.thumbnails";
        thumbs.meta.title = _("Show page thumbnails");
        thumbs.meta.category = page;
        thumbs.meta.keywords = "sidebar pages overview previews navigator";
        thumbs.meta.priority = PRIORITY_PAGE;
        thumbs.available = []() { return true; };
        thumbs.run = [this]() { ToolLessonNavigator::showThumbnails(control); };
        add(std::move(thumbs));
    }

    // Workspaces: what Ctrl+Shift+1, 2, 3, P and T do
    const std::string workspace = _("Workspace");
    const bool inPresent = window->getSelectedToolbar() && window->getSelectedToolbar()->getId() == "UTN Present";
    auto choose = [&](const char* id, const char* title, const char* keywords, const char* toolbarId,
                      const char* shortcut) {
        Entry e;
        e.meta.id = id;
        e.meta.title = title;
        e.meta.category = workspace;
        e.meta.keywords = keywords;
        e.meta.shortcut = shortcut;
        e.meta.priority = PRIORITY_WORKSPACE;
        e.available = []() { return true; };
        e.run = [this, toolbarId]() { window->toolbarSelected(toolbarId); };
        add(std::move(e));
    };
    choose("workspace.teach", _("Teach workspace"), "lesson teacher layout", "UTN Teacher", "Ctrl+Shift+1");
    choose("workspace.mark", _("Mark workspace"), "marking grade review layout", "UTN Marking", "Ctrl+Shift+2");
    choose("workspace.present", _("Present workspace"), "projector class presentation layout", "UTN Present",
           "Ctrl+Shift+3");
    {
        Entry leave;
        leave.meta.id = "workspace.leave-present";
        leave.meta.title = _("Leave Present");
        leave.meta.category = workspace;
        leave.meta.keywords = "exit stop presentation back";
        leave.meta.shortcut = "Ctrl+Shift+P";
        leave.meta.priority = PRIORITY_WORKSPACE;
        leave.meta.unavailableReason = _("Not in Present");
        const auto presentNow = [this]() {
            return window->getSelectedToolbar() && window->getSelectedToolbar()->getId() == "UTN Present";
        };
        leave.available = presentNow;
        leave.run = [this]() { window->leavePresent(); };
        add(std::move(leave));

        Entry tools;
        tools.meta.id = "workspace.present-tools";
        tools.meta.title = window->isPresentToolsRevealed() && inPresent ? _("Hide tools") : _("Show tools");
        tools.meta.category = workspace;
        tools.meta.keywords = "dock toolbar present reveal";
        tools.meta.shortcut = "Ctrl+Shift+T";
        tools.meta.priority = PRIORITY_WORKSPACE;
        tools.meta.unavailableReason = _("Only in Present");
        tools.available = presentNow;
        tools.run = [this]() { window->setPresentToolsRevealed(!window->isPresentToolsRevealed()); };
        add(std::move(tools));
    }

    // Classroom: the same calls the Classroom controls make, so Student View keeps every one of its rules
    const std::string classroom = _("Classroom");
    ToolStudentView* students = window->getToolMenuHandler() ? window->getToolMenuHandler()->getStudentView() : nullptr;
    auto overlay = [&](const char* id, const char* title, const char* keywords, Action a) {
        Entry e;
        e.meta.id = id;
        e.meta.title = title;
        e.meta.category = classroom;
        e.meta.keywords = keywords;
        e.meta.priority = PRIORITY_CLASSROOM;
        e.available = []() { return true; };
        e.run = [this, a]() { ToolPresentationKit::runClassroomAction(control, a); };
        add(std::move(e));
    };
    overlay("classroom.spotlight", _("Spotlight"), "focus circle highlight area presentation",
            Action::CLASSROOM_SPOTLIGHT);
    overlay("classroom.curtain", _("Curtain"), "cover reveal hide lower presentation", Action::CLASSROOM_CURTAIN);
    overlay("classroom.exit-overlay", _("Exit Spotlight or Curtain"), "stop leave", Action::CLASSROOM_EXIT_OVERLAY);
    auto student = [&](const char* id, const std::string& title, const char* keywords, bool needsWindow,
                       std::function<void()> run) {
        Entry e;
        e.meta.id = id;
        e.meta.title = title;
        e.meta.category = classroom;
        e.meta.keywords = keywords;
        e.meta.priority = PRIORITY_CLASSROOM;
        e.meta.unavailableReason = students ? _("Open Student View first") : _("Student View is not available");
        e.available = [students, needsWindow]() { return students && (!needsWindow || students->isWindowOpen()); };
        e.run = std::move(run);
        add(std::move(e));
    };
    student("classroom.freeze",
            students && students->isFrozen() ? _("Show the page to students again") : _("Freeze students' screen"),
            "pause hold still students", true, [students]() { students->setFrozen(!students->isFrozen()); });
    student("classroom.blank",
            students && students->isBlanked() ? _("Show the page to students again") : _("Blank students' screen"),
            "hide black cover students", false, [students]() { students->setBlanked(!students->isBlanked()); });
    student("classroom.student-view",
            students && students->isWindowOpen() ? _("Close Student View") : _("Open Student View on the projector"),
            "projector class screen second display", false, [students]() {
                if (students->isWindowOpen()) {
                    students->closeStudentView();
                } else {
                    students->showStudentView();
                }
            });
    {
        Entry normal;
        normal.meta.id = "classroom.back-to-normal";
        normal.meta.title = _("Back to normal");
        normal.meta.category = classroom;
        normal.meta.keywords = "reset stop clear unfreeze unblank spotlight curtain";
        normal.meta.priority = PRIORITY_CLASSROOM;
        normal.available = []() { return true; };
        normal.run = [this, students]() { ToolPresentationKit::backToNormal(control, students); };
        add(std::move(normal));
    }

    // View and file: the ordinary menu actions
    const std::string view = _("View");
    simple("view.zoom-in", _("Zoom in"), view.c_str(), "bigger larger magnify", PRIORITY_VIEW, Action::ZOOM_IN);
    simple("view.zoom-out", _("Zoom out"), view.c_str(), "smaller", PRIORITY_VIEW, Action::ZOOM_OUT);
    simple("view.zoom-fit", _("Fit page to window"), view.c_str(), "zoom width whole", PRIORITY_VIEW,
           Action::ZOOM_FIT);
    simple("view.zoom-100", _("Zoom to 100%"), view.c_str(), "actual size reset", PRIORITY_VIEW, Action::ZOOM_100);
    {
        Entry sidebar;
        sidebar.meta.id = "view.sidebar";
        sidebar.meta.title = control->getSettings()->isSidebarVisible() ? _("Hide sidebar") : _("Show sidebar");
        sidebar.meta.category = view;
        sidebar.meta.keywords = "pages thumbnails panel";
        sidebar.meta.priority = PRIORITY_VIEW;
        sidebar.available = []() { return true; };
        sidebar.run = [this]() {
            control->getActionDatabase()->fireChangeActionState(Action::SHOW_SIDEBAR,
                                                                !control->getSettings()->isSidebarVisible());
        };
        add(std::move(sidebar));
    }
    const std::string file = _("File");
    simple("file.save", _("Save"), file.c_str(), "store write", PRIORITY_FILE, Action::SAVE);
    simple("file.save-as", _("Save as..."), file.c_str(), "store copy write", PRIORITY_FILE, Action::SAVE_AS);
    simple("file.export-pdf", _("Export as PDF..."), file.c_str(), "share print send", PRIORITY_FILE,
           Action::EXPORT_AS_PDF);

    for (auto& e: entries) {
        e.meta.enabled = e.available();
        if (control->isBlocked()) {
            e.meta.unavailableReason = _("Busy saving or exporting");
        }
    }
}

std::vector<utn::PaletteCommand> CommandPalette::describeCommands() {
    if (dialog) {  // `shown` points into the entries; rebuilding them under an open list would leave it dangling
        close();
    }
    buildEntries();
    std::vector<utn::PaletteCommand> out;
    out.reserve(entries.size());
    for (const auto& e: entries) {
        out.push_back(e.meta);
    }
    return out;
}

void CommandPalette::open() {
    if (dialog) {
        gtk_window_present(GTK_WINDOW(dialog));
        return;
    }
    buildEntries();

    GtkWidget* main = GTK_WIDGET(window->getWindow());
    previousFocus = gtk_window_get_focus(GTK_WINDOW(main));
    if (previousFocus) {
        g_object_add_weak_pointer(G_OBJECT(previousFocus), reinterpret_cast<gpointer*>(&previousFocus));
    }

    dialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(dialog), _("Command Palette"));
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(main));
    gtk_window_set_modal(GTK_WINDOW(dialog), true);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), true);
    gtk_window_set_decorated(GTK_WINDOW(dialog), false);
    gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 560, 380);
    gtk_widget_set_name(dialog, "utnCommandPalette");
    utn::setAccessibleName(dialog, _("Command Palette"));

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(box), 12);
    gtk_container_add(GTK_CONTAINER(dialog), box);

    search = gtk_search_entry_new();
    gtk_widget_set_name(search, "utnPaletteSearch");
    gtk_entry_set_placeholder_text(GTK_ENTRY(search), _("Type a command, a tool or a page number"));
    utn::setAccessibleName(search, _("Search commands"));
    gtk_box_pack_start(GTK_BOX(box), search, FALSE, FALSE, 0);

    GtkWidget* scroller = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroller, true);
    list = gtk_list_box_new();
    gtk_widget_set_name(list, "utnPaletteList");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_SINGLE);
    gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(list), true);
    gtk_list_box_set_adjustment(GTK_LIST_BOX(list), gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroller)));
    utn::setAccessibleName(list, _("Commands"));
    GtkWidget* none = gtk_label_new(_("No matching commands"));
    gtk_widget_set_margin_top(none, 12);
    gtk_widget_show(none);
    gtk_list_box_set_placeholder(GTK_LIST_BOX(list), none);
    gtk_container_add(GTK_CONTAINER(scroller), list);
    gtk_box_pack_start(GTK_BOX(box), scroller, TRUE, TRUE, 0);

    status = gtk_label_new("");
    gtk_widget_set_name(status, "utnPaletteStatus");
    gtk_widget_set_halign(status, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(status), "utn-context-hint");
    gtk_box_pack_start(GTK_BOX(box), status, FALSE, FALSE, 0);

    g_signal_connect(search, "search-changed", G_CALLBACK(+[](GtkSearchEntry*, gpointer data) {
                         // Down, Up or Enter may already have listed what was typed: do not start the list over
                         static_cast<CommandPalette*>(data)->listWhatWasTyped();
                     }),
                     this);
    g_signal_connect(list, "row-activated", G_CALLBACK(+[](GtkListBox*, GtkListBoxRow* row, gpointer data) {
                         auto* self = static_cast<CommandPalette*>(data);
                         const auto index = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row), "utn-shown"));
                         if (index >= 1 && index <= self->shown.size()) {
                             self->activateEntry(self->shown[index - 1]);
                         }
                     }),
                     this);
    g_signal_connect(dialog, "key-press-event", G_CALLBACK(&CommandPalette::onKeyPress), this);
    g_signal_connect(dialog, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer data) {
                         auto* self = static_cast<CommandPalette*>(data);
                         self->dialog = nullptr;
                         self->search = nullptr;
                         self->list = nullptr;
                         self->status = nullptr;
                         GtkWidget* target = self->previousFocus;
                         if (target) {
                             g_object_remove_weak_pointer(G_OBJECT(target),
                                                          reinterpret_cast<gpointer*>(&self->previousFocus));
                             self->previousFocus = nullptr;
                         }
                         if (self->closing) {
                             return;  // the owner is being destroyed: there is nobody to give the keyboard back to
                         }
                         // The keyboard goes back to where it was; a widget that has gone is simply not used
                         if (gtk_widget_in_destruction(self->window->getWindow())) {
                             return;  // the whole window is going away (destroy_with_parent): nothing to give back
                         }
                         gtk_window_present(GTK_WINDOW(self->window->getWindow()));
                         if (target && gtk_widget_get_mapped(target)) {
                             gtk_widget_grab_focus(target);
                         }
                     }),
                     this);

    filter();
    gtk_widget_show_all(dialog);
    gtk_window_present(GTK_WINDOW(dialog));
    gtk_widget_grab_focus(search);
}

void CommandPalette::close() {
    if (dialog) {
        gtk_widget_destroy(dialog);
    }
}

void CommandPalette::filter() {
    if (!list) {
        return;
    }
    const std::string query = gtk_entry_get_text(GTK_ENTRY(search));
    lastQuery = query;

    pageEntry.reset();
    size_t pageCount = 0;
    if (auto* doc = control->getDocument()) {
        pageCount = doc->getPageCount();
    }
    if (auto target = utn::pageNumberFromQuery(query, pageCount)) {
        Entry e;
        e.meta.id = "page.go-to-number";
        e.meta.title = std::string(_("Go to page")) + " " + std::to_string(*target + 1);
        e.meta.category = _("Page");
        e.meta.priority = 100;
        e.meta.unavailableReason = _("Busy saving or exporting");
        e.available = [this]() { return !control->isBlocked(); };
        e.meta.enabled = e.available();
        const size_t index = *target;
        e.run = [this, index]() { ToolLessonNavigator::goToPage(control, index); };
        pageEntry = std::move(e);
    }

    std::vector<utn::PaletteCommand> metas;
    metas.reserve(entries.size());
    for (const auto& e: entries) {
        metas.push_back(e.meta);
    }

    shown.clear();
    if (pageEntry) {
        shown.push_back(&*pageEntry);
    }
    for (size_t i: utn::rankCommands(metas, query)) {
        shown.push_back(&entries[i]);
    }

    gtk_container_foreach(
            GTK_CONTAINER(list), +[](GtkWidget* row, gpointer list) { gtk_container_remove(GTK_CONTAINER(list), row); },
            list);

    GtkListBoxRow* first = nullptr;
    guint position = 1;
    for (const Entry* e: shown) {
        GtkWidget* row = gtk_list_box_row_new();
        GtkWidget* line = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_container_set_border_width(GTK_CONTAINER(line), 6);

        GtkWidget* title = gtk_label_new(e->meta.title.c_str());
        gtk_label_set_xalign(GTK_LABEL(title), 0);
        gtk_widget_set_hexpand(title, true);
        gtk_box_pack_start(GTK_BOX(line), title, TRUE, TRUE, 0);

        // What it is, or why it does not work now: said in words, never by dimming alone
        std::string detail = e->meta.enabled ? e->meta.category :
                                               std::string(_("Unavailable")) + ": " + e->meta.unavailableReason;
        if (e->meta.enabled && !e->meta.shortcut.empty()) {
            detail += "   " + e->meta.shortcut;
        }
        GtkWidget* info = gtk_label_new(detail.c_str());
        gtk_style_context_add_class(gtk_widget_get_style_context(info), "utn-context-hint");
        gtk_box_pack_end(GTK_BOX(line), info, FALSE, FALSE, 0);

        gtk_container_add(GTK_CONTAINER(row), line);
        gtk_widget_set_sensitive(row, e->meta.enabled);
        utn::setAccessibleName(row, utn::accessibleLabel(e->meta).c_str());
        g_object_set_data(G_OBJECT(row), "utn-shown", GUINT_TO_POINTER(position++));
        gtk_list_box_insert(GTK_LIST_BOX(list), row, -1);
        if (!first && e->meta.enabled) {
            first = GTK_LIST_BOX_ROW(row);
        }
    }
    gtk_widget_show_all(list);
    if (first) {
        gtk_list_box_select_row(GTK_LIST_BOX(list), first);
    }

    const std::string count = std::to_string(shown.size()) + " " + (shown.size() == 1 ? _("command") : _("commands"));
    gtk_label_set_text(GTK_LABEL(status), (count + "   -   " + _("Up and Down choose, Enter runs, Escape closes")).c_str());
}

void CommandPalette::listWhatWasTyped() {
    if (search && gtk_entry_get_text(GTK_ENTRY(search)) != lastQuery) {
        filter();
    }
}

void CommandPalette::selectRow(int delta) {
    if (!list) {
        return;
    }
    GList* children = gtk_container_get_children(GTK_CONTAINER(list));
    std::vector<GtkListBoxRow*> usable;
    for (GList* c = children; c; c = c->next) {
        if (gtk_widget_get_sensitive(GTK_WIDGET(c->data))) {
            usable.push_back(GTK_LIST_BOX_ROW(c->data));
        }
    }
    g_list_free(children);
    if (usable.empty()) {
        return;
    }
    auto* selected = gtk_list_box_get_selected_row(GTK_LIST_BOX(list));
    auto it = std::find(usable.begin(), usable.end(), selected);
    long index = it == usable.end() ? (delta > 0 ? -1 : static_cast<long>(usable.size())) : it - usable.begin();
    index = std::clamp(index + delta, 0L, static_cast<long>(usable.size()) - 1);
    GtkListBoxRow* target = usable[static_cast<size_t>(index)];
    gtk_list_box_select_row(GTK_LIST_BOX(list), target);

    // Keep the chosen row in view
    GtkAllocation alloc;
    gtk_widget_get_allocation(GTK_WIDGET(target), &alloc);
    GtkWidget* scroller = gtk_widget_get_parent(gtk_widget_get_parent(list));
    if (GTK_IS_SCROLLED_WINDOW(scroller)) {
        gtk_adjustment_clamp_page(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroller)), alloc.y,
                                  alloc.y + alloc.height);
    }
}

void CommandPalette::activateEntry(const Entry* entry) {
    if (!entry || !entry->available()) {
        return;
    }
    // Close first: the keyboard returns to the page, and a command that opens a dialog or popover finds it free
    auto run = entry->run;
    close();
    run();
}

void CommandPalette::activateSelected() {
    if (!list) {
        return;
    }
    // Enter can come before the entry has reported the last keystrokes: run what is typed, not the list of a moment ago
    listWhatWasTyped();
    if (auto* row = gtk_list_box_get_selected_row(GTK_LIST_BOX(list))) {
        const auto index = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row), "utn-shown"));
        if (index >= 1 && index <= shown.size()) {
            activateEntry(shown[index - 1]);
        }
    }
}

gboolean CommandPalette::onKeyPress(GtkWidget*, GdkEventKey* event, gpointer data) {
    auto* self = static_cast<CommandPalette*>(data);
    const guint mods = event->state & gtk_accelerator_get_default_mod_mask();
    switch (event->keyval) {
        case GDK_KEY_Escape:
            self->close();
            return TRUE;
        case GDK_KEY_Down:
        case GDK_KEY_KP_Down:
            self->listWhatWasTyped();
            self->selectRow(+1);
            return TRUE;
        case GDK_KEY_Up:
        case GDK_KEY_KP_Up:
            self->listWhatWasTyped();
            self->selectRow(-1);
            return TRUE;
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
            self->activateSelected();
            return TRUE;
        case GDK_KEY_k:
        case GDK_KEY_K:
            if (mods == GDK_CONTROL_MASK) {
                self->close();  // Ctrl+K again puts it away
                return TRUE;
            }
            return FALSE;
        default:
            return FALSE;
    }
}
