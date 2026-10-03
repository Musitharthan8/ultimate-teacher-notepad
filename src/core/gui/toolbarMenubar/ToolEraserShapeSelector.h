/*
 * Ultimate Teacher Notepad
 *
 * Eraser shape selector
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>

#include <gtk/gtk.h>

#include "AbstractToolItem.h"

class Control;

class ToolEraserShapeSelector: public AbstractToolItem {
public:
    ToolEraserShapeSelector(std::string id, Control* control);
    ~ToolEraserShapeSelector() override = default;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

private:
    Control* control;
};
