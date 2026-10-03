/*
 * Ultimate Teacher Notepad
 *
 * Continuous eraser size slider
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>

#include <gtk/gtk.h>

#include "AbstractToolItem.h"
#include "gui/IconNameHelper.h"

class Control;

// Continuous eraser size slider for the toolbar
class ToolEraserSizeSlider: public AbstractToolItem {
public:
    ToolEraserSizeSlider(std::string id, Control* control, IconNameHelper iconNameHelper);
    ~ToolEraserSizeSlider() override = default;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

private:
    Control* control;
    std::string iconName;
};
