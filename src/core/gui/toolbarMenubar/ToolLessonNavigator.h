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

private:
    class Instance;
    Control* control;
    std::vector<Instance*> instances;
};
