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

#include "control/ToolEnums.h"

#include "AbstractToolItem.h"

class Control;

class ToolUtnContextBar: public AbstractToolItem {
public:
    ToolUtnContextBar(std::string id, Control* control);
    ~ToolUtnContextBar() override = default;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    std::string getToolDisplayName() const override;
    bool expandsInToolbar() const override { return true; }
    GtkWidget* getNewToolIcon() const override;

    /**
     * Horizontal properties viewport: asks for the full natural width of @p content, keeps a small minimum so long
     * rows never widen the window, and scrolls with an overlay scrollbar only when the row is genuinely too narrow.
     * Returns: (transfer floating)
     */
    static GtkWidget* createPropertiesViewport(GtkWidget* content);

private:
    void rebuild(ToolType tool);
    void appendLabel(const char* text);
    void appendSeparator();
    void appendColorButton();
    void appendSizeButtons(ToolType tool);
    void appendEraserControls();
    void appendMarkupControls();
    void appendTextControls();
    void appendShapeControls(DrawingType type);
    void appendSelectionControls();
    void appendGenericMessage(const char* text);

private:
    Control* control;
    GtkBox* box = nullptr;
};
