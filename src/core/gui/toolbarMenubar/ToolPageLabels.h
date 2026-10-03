/*
 * Ultimate Teacher Notepad
 *
 * Classroom page labels and jumps
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>

#include <gtk/gtk.h>

#include "AbstractToolItem.h"

class Control;

class ToolPageLabels: public AbstractToolItem {
public:
    ToolPageLabels(std::string id, Control* control);
    ~ToolPageLabels() override = default;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

private:
    Control* control;
};
