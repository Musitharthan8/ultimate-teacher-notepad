/*
 * Ultimate Teacher Notepad
 *
 * Classroom presentation toolkit
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>

#include <gtk/gtk.h>

#include "AbstractToolItem.h"

class Control;

class ToolPresentationKit: public AbstractToolItem {
public:
    ToolPresentationKit(std::string id, Control* control);
    ~ToolPresentationKit() override = default;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

private:
    Control* control;
};
