/*
 * Ultimate Teacher Notepad
 *
 * Classroom presentation tools
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>

#include <gtk/gtk.h>

#include "AbstractToolItem.h"

class Control;

class ToolPresentationTools: public AbstractToolItem {
public:
    ToolPresentationTools(std::string id, Control* control);
    ~ToolPresentationTools() override = default;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

private:
    Control* control;
};
