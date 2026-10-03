/*
 * Ultimate Teacher Notepad
 *
 * Teacher stamp toolbar item
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>

#include <gtk/gtk.h>

#include "AbstractToolItem.h"

class Control;

class ToolTeacherStamp: public AbstractToolItem {
public:
    ToolTeacherStamp(std::string id, Control* control);
    ~ToolTeacherStamp() override = default;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

private:
    Control* control;
};
