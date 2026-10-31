/*
 * Ultimate Teacher Notepad
 *
 * Compact lesson/page navigator for classroom delivery
 *
 * Based on Xournal++ GPLv2+
 */

#include "ToolLessonNavigator.h"

#include <algorithm>
#include <array>
#include <memory>
#include <shared_mutex>
#include <string>
#include <utility>
#include <vector>

#include "control/Control.h"
#include "control/NavigationHistory.h"
#include "control/ScrollHandler.h"
#include "control/actions/ActionDatabase.h"
#include "enums/Action.enum.h"
#include "gui/sidebar/Sidebar.h"
#include "model/Document.h"
#include "model/PageNavigation.h"
#include "model/XojPage.h"
#include "undo/PageLabelUndoAction.h"
#include "undo/UndoRedoHandler.h"
#include "util/Util.h"
#include "util/gtk4_helper.h"
#include "util/i18n.h"

#include "UtnWidgets.h"

namespace {
constexpr std::array<const char*, 4> QUICK_LABELS{{"CW", "SW", "Answer Key", "Revision"}};
constexpr size_t MAX_RECENT_PAGES = 5;

void jumpToPage(Control* control, size_t page) {
    auto* doc = control->getDocument();
    if (!doc || page >= doc->getPageCount()) {
        return;
    }

    control->getScrollHandler()->jumpToPage(page);
    control->firePageSelected(page);
}

void setCurrentPageLabel(Control* control, const std::string& label) {
    PageRef page = control->getCurrentPage();
    // A save or export is running: the document is not to change under it
    if (control->isBlocked() || !page || page->getUtnPageLabel() == label) {
        return;
    }

    std::string oldLabel = page->getUtnPageLabel();

    auto* doc = control->getDocument();
    doc->lock();
    page->setUtnPageLabel(label);
    doc->unlock();

    control->getUndoRedoHandler()->addUndoAction(
            std::make_unique<PageLabelUndoAction>(page, std::move(oldLabel), label));
    ToolLessonNavigator::refreshLabels(control);
}

std::string getPageLabel(Control* control, size_t pageIndex) {
    auto* doc = control->getDocument();
    if (!doc || pageIndex >= doc->getPageCount()) {
        return {};
    }

    std::shared_lock lock(*doc);
    PageRef page = doc->getPage(pageIndex);
    return page ? page->getUtnPageLabel() : std::string{};
}

std::vector<bool> markedPages(Control* control) {
    std::vector<bool> marked;
    auto* doc = control->getDocument();
    std::shared_lock lock(*doc);
    marked.reserve(doc->getPageCount());
    for (size_t i = 0; i < doc->getPageCount(); ++i) {
        PageRef page = doc->getPage(i);
        marked.push_back(page && utn::isMarked(page->getUtnPageLabel()));
    }
    return marked;
}

void jumpToAdjacentMarked(Control* control, int direction) {
    if (auto target = utn::adjacentMarked(markedPages(control), control->getCurrentPageNo(), direction)) {
        jumpToPage(control, *target);
    }
}
}  // namespace

bool ToolLessonNavigator::goToPage(Control* control, size_t pageIndex) {
    auto* doc = control->getDocument();
    if (!doc || pageIndex >= doc->getPageCount()) {
        return false;
    }
    jumpToPage(control, pageIndex);
    return true;
}

utn::BookmarkChange ToolLessonNavigator::toggleBookmark(Control* control) {
    PageRef page = control->getCurrentPage();
    if (!page || control->isBlocked()) {
        return utn::BookmarkChange::KeepLabel;
    }
    const auto toggle = utn::toggleBookmark(page->getUtnPageLabel());
    if (toggle.change != utn::BookmarkChange::KeepLabel) {
        setCurrentPageLabel(control, toggle.label);
    }
    return toggle.change;
}

bool ToolLessonNavigator::currentPageIsMarked(Control* control) {
    PageRef page = control->getCurrentPage();
    return page && utn::isMarked(page->getUtnPageLabel());
}

void ToolLessonNavigator::showThumbnails(Control* control) {
    if (control->isBlocked()) {
        return;  // firing the sidebar state works even while every action is disabled
    }
    control->getActionDatabase()->fireChangeActionState(Action::SHOW_SIDEBAR, true);
    if (auto* sidebar = control->getSidebar()) {
        sidebar->setSelectedTab(SIDEBAR_PAGES_TAB);
    }
}

class ToolLessonNavigator::Instance {
public:
    Instance(ToolLessonNavigator* handler, Control* control, bool horizontal):
            handler(handler), control(control), horizontal(horizontal) {}

    xoj::util::WidgetSPtr makeWidget() {
        GtkBox* box = GTK_BOX(gtk_box_new(horizontal ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL, 2));

        GtkWidget* previous = gtk_button_new_with_label("‹");
        gtk_widget_set_tooltip_text(previous, _("Previous page"));
        utn::setAccessibleName(previous, _("Previous page"));
        g_signal_connect(
                previous,
                "clicked",
                G_CALLBACK(+[](GtkButton*, gpointer data) {
                    auto* self = static_cast<Instance*>(data);
                    size_t current = self->control->getCurrentPageNo();
                    if (current > 0) {
                        jumpToPage(self->control, current - 1);
                    }
                }),
                this);
        gtk_box_append(box, previous);

        menuButton = GTK_MENU_BUTTON(gtk_menu_button_new());
        gtk_widget_set_tooltip_text(GTK_WIDGET(menuButton), _("Lesson Navigator"));

        popover = GTK_POPOVER(gtk_popover_new());
        gtk_widget_add_css_class(GTK_WIDGET(popover), "toolbar");
        gtk_menu_button_set_popover(menuButton, GTK_WIDGET(popover));
        gtk_menu_button_set_direction(menuButton, horizontal ? GTK_ARROW_UP : GTK_ARROW_RIGHT);

        GtkBox* panel = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
        gtk_widget_set_margin_start(GTK_WIDGET(panel), 10);
        gtk_widget_set_margin_end(GTK_WIDGET(panel), 10);
        gtk_widget_set_margin_top(GTK_WIDGET(panel), 10);
        gtk_widget_set_margin_bottom(GTK_WIDGET(panel), 10);
        // The panel is tall: a short window scrolls it instead of cutting it off
        GtkWidget* outer = gtk_scrolled_window_new(nullptr, nullptr);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(outer), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(outer), true);
        gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(outer), 480);
        gtk_container_add(GTK_CONTAINER(outer), GTK_WIDGET(panel));
        gtk_popover_set_child(popover, outer);

        GtkWidget* heading = gtk_label_new(_("Lesson Navigator"));
        gtk_widget_set_halign(heading, GTK_ALIGN_START);
        gtk_widget_add_css_class(heading, "utn-popover-title");
        gtk_box_append(panel, heading);

        // Go to a page by its number
        GtkBox* goRow = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6));
        goEntry = GTK_ENTRY(gtk_entry_new());
        gtk_widget_set_name(GTK_WIDGET(goEntry), "utnGoToPage");
        gtk_entry_set_placeholder_text(goEntry, _("Go to page number"));
        gtk_entry_set_width_chars(goEntry, 10);
        utn::setAccessibleName(GTK_WIDGET(goEntry), _("Go to page number"));
        gtk_widget_set_hexpand(GTK_WIDGET(goEntry), true);
        g_signal_connect(goEntry, "activate", G_CALLBACK(+[](GtkEntry*, gpointer data) {
                             static_cast<Instance*>(data)->goToTypedPage();
                         }),
                         this);
        gtk_box_append(goRow, GTK_WIDGET(goEntry));
        GtkWidget* go = gtk_button_new_with_label(_("Go"));
        utn::setAccessibleName(go, _("Go to the typed page"));
        g_signal_connect(go, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                             static_cast<Instance*>(data)->goToTypedPage();
                         }),
                         this);
        gtk_box_append(goRow, go);
        gtk_box_append(panel, GTK_WIDGET(goRow));

        goError = GTK_LABEL(gtk_label_new(""));
        gtk_widget_set_name(GTK_WIDGET(goError), "utnGoToPageError");
        gtk_widget_set_halign(GTK_WIDGET(goError), GTK_ALIGN_START);
        gtk_widget_add_css_class(GTK_WIDGET(goError), "utn-context-hint");
        gtk_box_append(panel, GTK_WIDGET(goError));

        // Bookmark the page; open the thumbnails
        GtkBox* markRow = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6));
        bookmarkButton = GTK_BUTTON(gtk_button_new_with_label(_("Bookmark this page")));
        gtk_widget_set_name(GTK_WIDGET(bookmarkButton), "utnBookmarkButton");
        g_signal_connect(bookmarkButton, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                             auto* self = static_cast<Instance*>(data);
                             ToolLessonNavigator::toggleBookmark(self->control);
                             self->refresh();
                         }),
                         this);
        gtk_box_append(markRow, GTK_WIDGET(bookmarkButton));
        GtkWidget* thumbnails = gtk_button_new_with_label(_("Show page thumbnails"));
        g_signal_connect(thumbnails, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                             auto* self = static_cast<Instance*>(data);
                             ToolLessonNavigator::showThumbnails(self->control);
                             gtk_popover_popdown(self->popover);
                         }),
                         this);
        gtk_box_append(markRow, thumbnails);
        gtk_box_append(panel, GTK_WIDGET(markRow));

        GtkBox* labelRow = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6));
        currentEntry = GTK_ENTRY(gtk_entry_new());
        gtk_entry_set_placeholder_text(currentEntry, _("Label this page, e.g. CW, Q4, Answers"));
        utn::setAccessibleName(GTK_WIDGET(currentEntry), _("Label for this page"));
        gtk_widget_set_hexpand(GTK_WIDGET(currentEntry), true);
        gtk_box_append(labelRow, GTK_WIDGET(currentEntry));

        GtkWidget* save = gtk_button_new_with_label(_("Save"));
        g_signal_connect(
                save,
                "clicked",
                G_CALLBACK(+[](GtkButton*, gpointer data) {
                    auto* self = static_cast<Instance*>(data);
                    setCurrentPageLabel(self->control, gtk_editable_get_text(GTK_EDITABLE(self->currentEntry)));
                    self->refresh();
                }),
                this);
        // Enter in the label box saves the label, like the button
        g_signal_connect_swapped(currentEntry, "activate", G_CALLBACK(gtk_button_clicked), save);
        gtk_box_append(labelRow, save);
        gtk_box_append(panel, GTK_WIDGET(labelRow));

        GtkBox* quick = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
        for (const char* label: QUICK_LABELS) {
            GtkWidget* button = gtk_button_new_with_label(label);
            g_object_set_data_full(G_OBJECT(button), "utn-label", g_strdup(label), g_free);
            g_signal_connect(
                    button,
                    "clicked",
                    G_CALLBACK(+[](GtkButton* button, gpointer data) {
                        auto* self = static_cast<Instance*>(data);
                        auto* label = static_cast<const char*>(g_object_get_data(G_OBJECT(button), "utn-label"));
                        setCurrentPageLabel(self->control, label ? label : "");
                        self->refresh();
                    }),
                    this);
            gtk_box_append(quick, button);
        }
        gtk_box_append(panel, GTK_WIDGET(quick));

        GtkBox* labelledNav = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4));
        GtkWidget* previousLabel = gtk_button_new_with_label(_("Previous labelled"));
        GtkWidget* nextLabel = gtk_button_new_with_label(_("Next labelled"));

        g_signal_connect(previousLabel, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                             jumpToAdjacentMarked(static_cast<Instance*>(data)->control, -1);
                         }),
                         this);
        g_signal_connect(nextLabel, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
                             jumpToAdjacentMarked(static_cast<Instance*>(data)->control, +1);
                         }),
                         this);

        gtk_box_append(labelledNav, previousLabel);
        gtk_box_append(labelledNav, nextLabel);
        gtk_box_append(panel, GTK_WIDGET(labelledNav));

        GtkWidget* listHeading = gtk_label_new(_("Lesson map"));
        gtk_widget_set_halign(listHeading, GTK_ALIGN_START);
        gtk_widget_add_css_class(listHeading, "utn-popover-title");
        gtk_box_append(panel, listHeading);

        GtkWidget* scroller = gtk_scrolled_window_new(nullptr, nullptr);
        gtk_widget_set_size_request(scroller, 330, 150);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

        labelList = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 2));
        gtk_widget_set_name(GTK_WIDGET(labelList), "utnLessonMap");
        gtk_container_add(GTK_CONTAINER(scroller), GTK_WIDGET(labelList));
        gtk_box_append(panel, scroller);

        GtkWidget* recentHeading = gtk_label_new(_("Recent pages"));
        gtk_widget_set_halign(recentHeading, GTK_ALIGN_START);
        gtk_widget_add_css_class(recentHeading, "utn-popover-title");
        gtk_box_append(panel, recentHeading);

        recentList = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 2));
        gtk_widget_set_name(GTK_WIDGET(recentList), "utnRecentPages");
        gtk_box_append(panel, GTK_WIDGET(recentList));

        g_signal_connect(
                popover,
                "show",
                G_CALLBACK(+[](GtkWidget*, gpointer data) {
                    auto* self = static_cast<Instance*>(data);
                    self->clearGoToError();
                    self->refresh();
                }),
                this);

        gtk_box_append(box, GTK_WIDGET(menuButton));

        GtkWidget* next = gtk_button_new_with_label("›");
        gtk_widget_set_tooltip_text(next, _("Next page"));
        utn::setAccessibleName(next, _("Next page"));
        g_signal_connect(
                next,
                "clicked",
                G_CALLBACK(+[](GtkButton*, gpointer data) {
                    auto* self = static_cast<Instance*>(data);
                    size_t current = self->control->getCurrentPageNo();
                    auto* doc = self->control->getDocument();
                    if (current + 1 < doc->getPageCount()) {
                        jumpToPage(self->control, current + 1);
                    }
                }),
                this);
        gtk_box_append(box, next);

        refresh();
        gtk_widget_show_all(GTK_WIDGET(outer));
        return xoj::util::WidgetSPtr(GTK_WIDGET(box), xoj::util::adopt);
    }

    void setPageInfo(size_t page, size_t count) {
        currentPage = page;
        pageCount = count;
        refreshMainLabel();
    }

    /// The page label may have changed without the navigator being used (palette, undo, redo)
    void refreshLabel() { setPageInfo(control->getCurrentPageNo(), control->getDocument()->getPageCount()); }

    void disconnect() { handler = nullptr; }
    ToolLessonNavigator* getHandler() const { return handler; }

private:
    void clearGoToError() {
        gtk_label_set_text(goError, "");
        gtk_style_context_remove_class(gtk_widget_get_style_context(GTK_WIDGET(goEntry)), "error");
    }

    void goToTypedPage() {
        const size_t count = control->getDocument()->getPageCount();
        const auto target = utn::parsePageNumber(gtk_editable_get_text(GTK_EDITABLE(goEntry)), count);
        if (!target) {
            std::string message = count == 0 ? std::string(_("This document has no pages")) :
                                               std::string(_("Enter a page from 1 to ")) + std::to_string(count);
            gtk_label_set_text(goError, message.c_str());
            gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(goEntry)), "error");
            return;
        }
        clearGoToError();
        ToolLessonNavigator::goToPage(control, *target);
        gtk_editable_set_text(GTK_EDITABLE(goEntry), "");
        gtk_popover_popdown(popover);
        refresh();
    }

    void refreshMainLabel() {
        if (!menuButton) {
            return;
        }

        std::string label = getPageLabel(control, currentPage);
        std::string text = _("Page");
        text += " " + std::to_string(pageCount == 0 ? 0 : currentPage + 1) + "/" + std::to_string(pageCount);

        if (!label.empty()) {
            text += "  ·  " + label;
        }

        gtk_button_set_label(GTK_BUTTON(menuButton), text.c_str());

        if (currentEntry) {
            gtk_editable_set_text(GTK_EDITABLE(currentEntry), label.c_str());
        }

        if (bookmarkButton) {
            // The bookmark button owns only the label it makes itself
            const auto toggle = utn::toggleBookmark(label);
            const char* words = toggle.change == utn::BookmarkChange::Add ? _("Bookmark this page") :
                                toggle.change == utn::BookmarkChange::Remove ? _("Remove bookmark") :
                                                                               _("Page is labelled");
            gtk_button_set_label(bookmarkButton, words);
            gtk_widget_set_sensitive(GTK_WIDGET(bookmarkButton), toggle.change != utn::BookmarkChange::KeepLabel);
        }
    }

    static void clearBox(GtkBox* box) {
        GList* children = gtk_container_get_children(GTK_CONTAINER(box));
        for (GList* child = children; child != nullptr; child = child->next) {
            gtk_container_remove(GTK_CONTAINER(box), GTK_WIDGET(child->data));
        }
        g_list_free(children);
    }

    /// A button that goes to a page, said as "4   CW" (a labelled page) or "Page 4"
    GtkWidget* makePageButton(size_t index, const std::string& label) {
        std::string text = label.empty() ? std::string(_("Page")) + " " + std::to_string(index + 1) :
                                           std::to_string(index + 1) + "   " + label;
        GtkWidget* button = gtk_button_new_with_label(text.c_str());
        gtk_widget_set_halign(button, GTK_ALIGN_FILL);
        g_object_set_data(G_OBJECT(button), "utn-page-index", GUINT_TO_POINTER(static_cast<guint>(index)));
        g_signal_connect(
                button,
                "clicked",
                G_CALLBACK(+[](GtkButton* button, gpointer data) {
                    auto* self = static_cast<Instance*>(data);
                    size_t page = static_cast<size_t>(GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "utn-page-index")));
                    jumpToPage(self->control, page);
                }),
                this);
        return button;
    }

    void rebuildLabelList() {
        if (!labelList) {
            return;
        }

        clearBox(labelList);

        auto* doc = control->getDocument();
        std::shared_lock lock(*doc);
        bool foundAny = false;

        for (size_t i = 0; i < doc->getPageCount(); ++i) {
            PageRef page = doc->getPage(i);
            if (!page || page->getUtnPageLabel().empty()) {
                continue;
            }

            foundAny = true;
            GtkWidget* button = makePageButton(i, page->getUtnPageLabel());
            if (i == currentPage) {
                // Said to a screen reader and shown in bold; the name also lets the page be found in tests
                gtk_widget_set_name(button, "utnLessonMapCurrent");
                gtk_widget_add_css_class(button, "utn-current-page");
                const std::string spoken = std::to_string(i + 1) + " " + page->getUtnPageLabel() + ", " +
                                           _("current page");
                utn::setAccessibleName(button, spoken.c_str());
            }
            gtk_box_append(labelList, button);
        }

        lock.unlock();

        if (!foundAny) {
            GtkWidget* empty = gtk_label_new(_("No labelled pages yet"));
            gtk_widget_set_halign(empty, GTK_ALIGN_START);
            gtk_widget_add_css_class(empty, "utn-context-hint");
            gtk_box_append(labelList, empty);
        }

        gtk_widget_show_all(GTK_WIDGET(labelList));
    }

    void rebuildRecentList() {
        if (!recentList) {
            return;
        }

        clearBox(recentList);

        // The history keeps the pages themselves, so after pages are inserted, deleted or moved each entry still
        // names the right page, and one that was deleted is simply not found
        auto* doc = control->getDocument();
        std::vector<size_t> recent;
        std::vector<std::string> labels;
        {
            std::shared_lock lock(*doc);
            std::vector<size_t> visited;
            for (const PageRef& page: control->getNavigationHistory()->visitedPages()) {
                if (size_t index = doc->indexOf(page); index != npos) {
                    visited.push_back(index);
                }
            }
            recent = utn::recentPages(visited, currentPage, MAX_RECENT_PAGES);
            for (size_t index: recent) {
                PageRef page = doc->getPage(index);
                labels.push_back(page ? page->getUtnPageLabel() : std::string{});
            }
        }
        for (size_t i = 0; i < recent.size(); ++i) {
            gtk_box_append(recentList, makePageButton(recent[i], labels[i]));
        }
        if (recent.empty()) {
            GtkWidget* empty = gtk_label_new(_("No recent pages yet"));
            gtk_widget_set_halign(empty, GTK_ALIGN_START);
            gtk_widget_add_css_class(empty, "utn-context-hint");
            gtk_box_append(recentList, empty);
        }
        gtk_widget_show_all(GTK_WIDGET(recentList));
    }

    void refresh() {
        currentPage = control->getCurrentPageNo();
        pageCount = control->getDocument()->getPageCount();
        refreshMainLabel();
        rebuildLabelList();
        rebuildRecentList();
    }

private:
    ToolLessonNavigator* handler;
    Control* control;
    bool horizontal;
    size_t currentPage = 0;
    size_t pageCount = 0;

    GtkMenuButton* menuButton = nullptr;
    GtkPopover* popover = nullptr;
    GtkEntry* currentEntry = nullptr;
    GtkEntry* goEntry = nullptr;
    GtkLabel* goError = nullptr;
    GtkButton* bookmarkButton = nullptr;
    GtkBox* labelList = nullptr;
    GtkBox* recentList = nullptr;
};

namespace {
/// Every navigator that exists, so a label changed by a static call (the palette) or by undo reaches the buttons
std::vector<ToolLessonNavigator*>& allNavigators() {
    static std::vector<ToolLessonNavigator*> navigators;
    return navigators;
}
}  // namespace

ToolLessonNavigator::ToolLessonNavigator(std::string id, Control* control):
        AbstractToolItem(std::move(id), Category::NAVIGATION), control(control) {
    allNavigators().push_back(this);
}

void ToolLessonNavigator::refreshLabels(Control* control) {
    for (auto* navigator: allNavigators()) {
        if (navigator->control == control) {
            for (auto* instance: navigator->instances) {
                instance->refreshLabel();
            }
        }
    }
}

ToolLessonNavigator::~ToolLessonNavigator() {
    auto& navigators = allNavigators();
    navigators.erase(std::remove(navigators.begin(), navigators.end(), this), navigators.end());
    for (auto* instance: instances) {
        instance->disconnect();
    }
}

auto ToolLessonNavigator::createItem(bool horizontal) -> xoj::util::WidgetSPtr {
    auto data = std::make_unique<Instance>(this, control, horizontal);
    auto item = data->makeWidget();

    instances.emplace_back(data.get());

    g_object_weak_ref(
            G_OBJECT(item.get()),
            +[](gpointer pointer, GObject*) {
                auto* instance = static_cast<Instance*>(pointer);
                if (auto* handler = instance->getHandler(); handler) {
                    auto& items = handler->instances;
                    if (auto it = std::find(items.begin(), items.end(), instance); it != items.end()) {
                        items.erase(it);
                    }
                }
                delete instance;
            },
            data.release());

    return item;
}

void ToolLessonNavigator::setPageInfo(size_t currentPage, size_t pageCount) {
    for (auto* instance: instances) {
        instance->setPageInfo(currentPage, pageCount);
    }
}

auto ToolLessonNavigator::getToolDisplayName() const -> std::string {
    return _("Lesson Navigator");
}

auto ToolLessonNavigator::getNewToolIcon() const -> GtkWidget* {
    return gtk_label_new(_("Lesson"));
}
