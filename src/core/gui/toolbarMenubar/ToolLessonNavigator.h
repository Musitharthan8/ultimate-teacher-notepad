/*
 * Ultimate Teacher Notepad
 *
 * Compact lesson/page navigator for classroom delivery
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <gtk/gtk.h>

#include "model/PageNavigation.h"

#include "AbstractToolItem.h"

class Control;

class ToolLessonNavigator: public AbstractToolItem {
public:
    ToolLessonNavigator(std::string id, Control* control);
    ~ToolLessonNavigator() override;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;
    void setPageInfo(size_t currentPage, size_t pageCount);

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

    /// What the navigator buttons do, as functions so the Command Palette runs the very same operations
    /// @return false if the document has no such page
    static bool goToPage(Control* control, size_t pageIndex);
    /// Bookmark the current page (a page label, saved and undoable) or take the bookmark off. A page with a label of
    /// its own is left as it is.
    static utn::BookmarkChange toggleBookmark(Control* control);
    static bool currentPageIsMarked(Control* control);
    /// Show the sidebar on its page thumbnails
    static void showThumbnails(Control* control);
    /// Bring the page label shown on every navigator button up to date; for a label changed from outside the navigator
    static void refreshLabels(Control* control);

private:
    class Instance;
    Control* control;
    std::vector<Instance*> instances;
};
