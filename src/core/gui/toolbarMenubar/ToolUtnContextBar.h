/*
 * Ultimate Teacher Notepad
 *
 * Context-sensitive teacher toolbar
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>

#include <gtk/gtk.h>

#include "AbstractToolItem.h"

class Control;

class ToolUtnContextBar: public AbstractToolItem {
public:
    ToolUtnContextBar(std::string id, Control* control);
    ~ToolUtnContextBar() override = default;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

private:
    void rebuild(ToolType tool);
    void appendLabel(const char* text);
    void appendSeparator();
    void appendColorButton();
    void appendSizeButtons(ToolType tool);
    void appendEraserControls();
    void appendMarkupControls();
    void appendTextControls();
    void appendGenericMessage(const char* text);

private:
    Control* control;
    GtkBox* box = nullptr;
};
