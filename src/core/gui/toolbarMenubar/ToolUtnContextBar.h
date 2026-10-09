/*
 * Ultimate Teacher Notepad
 *
 * Properties bar for the active teaching tool. Every tool shows its settings in the same order:
 * tool name, colour, size, then what is specific to that tool. Less common settings sit in labelled menus.
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
    enum class Palette { Ink, Highlight };

    void rebuild(ToolType tool);

    // Shared building blocks, used in this order by every tool
    void appendBadge(const char* name);
    void appendColourControls(Palette palette);
    void appendSizeControls(ToolType tool);
    void appendSeparator();
    void appendHint(const char* text);
    GtkWidget* appendButton(const char* label, const char* hint);

    // One section per tool
    void appendPenControls();
    void appendShapeControls(DrawingType type);
    void appendHighlightControls();
    void appendEraserControls();
    void appendTextControls();
    void appendAnswerBoxStyleMenu();
    void appendSelectionControls(ToolType tool);

    Control* control;
    GtkBox* box = nullptr;
};
