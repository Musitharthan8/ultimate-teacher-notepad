/*
 * Ultimate Teacher Notepad
 *
 * Consolidated classroom drawing and STEM tools
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>

#include <gtk/gtk.h>

#include "AbstractToolItem.h"

class Control;

class ToolTeachingKit: public AbstractToolItem {
public:
    ToolTeachingKit(std::string id, Control* control);
    ~ToolTeachingKit() override = default;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

private:
    Control* control;
};
