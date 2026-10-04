/*
 * Ultimate Teacher Notepad
 *
 * Shell appearance selector
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>

#include <gtk/gtk.h>

#include "AbstractToolItem.h"
#include "gui/IconNameHelper.h"

class Control;

class ToolAppearance: public AbstractToolItem {
public:
    ToolAppearance(std::string id, Control* control, IconNameHelper iconNameHelper);
    ~ToolAppearance() override = default;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

private:
    Control* control;
    std::string iconName;
};
