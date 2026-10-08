/*
 * Ultimate Teacher Notepad
 *
 * Fast classroom clear actions
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>

#include <gtk/gtk.h>

#include "AbstractToolItem.h"

class Control;

class ToolClear: public AbstractToolItem {
public:
    ToolClear(std::string id, Control* control);
    ~ToolClear() override = default;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

private:
    Control* control;
};
