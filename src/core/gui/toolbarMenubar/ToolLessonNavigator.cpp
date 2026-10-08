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

std::string getPageLabel(Control* control, size_t pageIndex) {
    auto* doc = control->getDocument();
    if (!doc || pageIndex >= doc->getPageCount()) {
        return {};
    }

    std::shared_lock lock(*doc);
    PageRef page = doc->getPage(pageIndex);
    return page ? page->getUtnPageLabel() : std::string{};
}
}  // namespace

class ToolLessonNavigator::Instance {
public:
    Instance(ToolLessonNavigator* handler, Control* control, bool horizontal):
            handler(handler), control(control), horizontal(horizontal) {}

    xoj::util::WidgetSPtr makeWidget() {
        GtkBox* box = GTK_BOX(gtk_box_new(horizontal ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL, 2));

        GtkWidget* previous = gtk_button_new_with_label("‹");
        gtk_widget_set_tooltip_text(previous, _("Previous page"));
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

        GtkPopover* popover = GTK_POPOVER(gtk_popover_new());
        gtk_widget_add_css_class(GTK_WIDGET(popover), "toolbar");
        gtk_menu_button_set_popover(menuButton, GTK_WIDGET(popover));
        gtk_menu_button_set_direction(menuButton, horizontal ? GTK_ARROW_UP : GTK_ARROW_RIGHT);

        GtkBox* panel = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 6));
        gtk_widget_set_margin_start(GTK_WIDGET(panel), 10);
        gtk_widget_set_margin_end(GTK_WIDGET(panel), 10);
        gtk_widget_set_margin_top(GTK_WIDGET(panel), 10);
        gtk_widget_set_margin_bottom(GTK_WIDGET(panel), 10);
        gtk_popover_set_child(popover, GTK_WIDGET(panel));

        GtkWidget* heading = gtk_label_new(_("Lesson Navigator"));
        gtk_widget_set_halign(heading, GTK_ALIGN_START);
        gtk_widget_add_css_class(heading, "utn-popover-title");
        gtk_box_append(panel, heading);

        GtkBox* labelRow = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6));
        currentEntry = GTK_ENTRY(gtk_entry_new());
        gtk_entry_set_placeholder_text(currentEntry, _("Label this page, e.g. CW, Q4, Answers"));
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

        g_signal_connect(
                previousLabel,
                "clicked",
                G_CALLBACK(+[](GtkButton*, gpointer data) {
                    auto* self = static_cast<Instance*>(data);
                    size_t current = self->control->getCurrentPageNo();
                    auto* doc = self->control->getDocument();
                    std::shared_lock lock(*doc);

                    for (size_t i = current; i > 0; --i) {
                        PageRef page = doc->getPage(i - 1);
                        if (page && !page->getUtnPageLabel().empty()) {
                            lock.unlock();
                            jumpToPage(self->control, i - 1);
                            return;
                        }
                    }
                }),
                this);

        g_signal_connect(
                nextLabel,
                "clicked",
                G_CALLBACK(+[](GtkButton*, gpointer data) {
                    auto* self = static_cast<Instance*>(data);
                    size_t current = self->control->getCurrentPageNo();
                    auto* doc = self->control->getDocument();
                    std::shared_lock lock(*doc);

                    for (size_t i = current + 1; i < doc->getPageCount(); ++i) {
                        PageRef page = doc->getPage(i);
                        if (page && !page->getUtnPageLabel().empty()) {
                            lock.unlock();
                            jumpToPage(self->control, i);
                            return;
                        }
                    }
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
        gtk_widget_set_size_request(scroller, 330, 240);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

        labelList = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 2));
        gtk_container_add(GTK_CONTAINER(scroller), GTK_WIDGET(labelList));
        gtk_box_append(panel, scroller);

        g_signal_connect(
                popover,
                "show",
                G_CALLBACK(+[](GtkWidget*, gpointer data) {
                    static_cast<Instance*>(data)->refresh();
                }),
                this);

        gtk_box_append(box, GTK_WIDGET(menuButton));

        GtkWidget* next = gtk_button_new_with_label("›");
        gtk_widget_set_tooltip_text(next, _("Next page"));
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
        gtk_widget_show_all(GTK_WIDGET(panel));
        return xoj::util::WidgetSPtr(GTK_WIDGET(box), xoj::util::adopt);
    }

    void setPageInfo(size_t page, size_t count) {
        currentPage = page;
        pageCount = count;
        refreshMainLabel();
    }

    void disconnect() { handler = nullptr; }
    ToolLessonNavigator* getHandler() const { return handler; }

private:
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
    }

    void rebuildLabelList() {
        if (!labelList) {
            return;
        }

        GList* children = gtk_container_get_children(GTK_CONTAINER(labelList));
        for (GList* child = children; child != nullptr; child = child->next) {
            gtk_container_remove(GTK_CONTAINER(labelList), GTK_WIDGET(child->data));
        }
        g_list_free(children);

        auto* doc = control->getDocument();
        std::shared_lock lock(*doc);
        bool foundAny = false;

        for (size_t i = 0; i < doc->getPageCount(); ++i) {
            PageRef page = doc->getPage(i);
            if (!page || page->getUtnPageLabel().empty()) {
                continue;
            }

            foundAny = true;
            std::string buttonText = std::to_string(i + 1) + "   " + page->getUtnPageLabel();
            GtkWidget* button = gtk_button_new_with_label(buttonText.c_str());
            gtk_widget_set_halign(button, GTK_ALIGN_FILL);
            g_object_set_data(G_OBJECT(button), "utn-page-index", GUINT_TO_POINTER(static_cast<guint>(i)));

            g_signal_connect(
                    button,
                    "clicked",
                    G_CALLBACK(+[](GtkButton* button, gpointer data) {
                        auto* self = static_cast<Instance*>(data);
                        size_t page =
                                static_cast<size_t>(GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "utn-page-index")));
                        jumpToPage(self->control, page);
                    }),
                    this);

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

    void refresh() {
        currentPage = control->getCurrentPageNo();
        pageCount = control->getDocument()->getPageCount();
        refreshMainLabel();
        rebuildLabelList();
    }

private:
    ToolLessonNavigator* handler;
    Control* control;
    bool horizontal;
    size_t currentPage = 0;
    size_t pageCount = 0;

    GtkMenuButton* menuButton = nullptr;
    GtkEntry* currentEntry = nullptr;
    GtkBox* labelList = nullptr;
};

ToolLessonNavigator::ToolLessonNavigator(std::string id, Control* control):
        AbstractToolItem(std::move(id), Category::NAVIGATION), control(control) {}

ToolLessonNavigator::~ToolLessonNavigator() {
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
