/*
 * Ultimate Teacher Notepad
 *
 * Teacher tool profile selector
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>

#include <gtk/gtk.h>

#include "AbstractToolItem.h"

class Control;

class ToolProfileSelector: public AbstractToolItem {
public:
    ToolProfileSelector(std::string id, Control* control);
    ~ToolProfileSelector() override = default;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

private:
    Control* control;
};
