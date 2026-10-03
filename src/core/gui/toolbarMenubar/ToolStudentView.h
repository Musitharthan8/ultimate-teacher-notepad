/*
 * Ultimate Teacher Notepad
 *
 * Student view toolbar item
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>

#include <gtk/gtk.h>

#include "AbstractToolItem.h"

class Control;

class ToolStudentView: public AbstractToolItem {
public:
    ToolStudentView(std::string id, Control* control);
    ~ToolStudentView() override = default;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

private:
    Control* control;
};
