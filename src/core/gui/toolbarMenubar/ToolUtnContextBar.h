/*
 * Ultimate Teacher Notepad
 *
 * Properties bar for the active teaching tool. Every tool shows its settings in the same order:
 * tool name, colour, size, then what is specific to that tool. Less common settings sit in labelled menus.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <memory>
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
    void populate(ToolType tool);

    // Shared building blocks, used in this order by every tool
    void appendBadge(const char* name);
    void appendColourControls(Palette palette);
    void appendSizeControls(ToolType tool);
    void appendSeparator();
    void appendHint(const char* text);
    GtkWidget* appendButton(const char* label, const char* hint);

    /// The vertical panel beside the tool rail (not the horizontal strip)
    bool isSidePanel() const;
    /// A row of controls, or a grid wrapping after @p columns when @p columns > 0
    static GtkWidget* newControlGroup(int columns, int spacing);
    static void addToGroup(GtkWidget* group, GtkWidget* child, int index);

    // One section per tool
    void appendPenControls();
    void appendShapePicker(DrawingType current);
    void appendShapeControls(DrawingType type);
    void appendHighlightControls();
    void appendEraserControls();
    void appendTextControls();
    void appendAnswerBoxStyleMenu();
    /// Answers section of the Answer Box panel: a live status line plus Hide and Reveal
    void appendAnswersVisibility();
    void appendSelectionControls(ToolType tool);

    Control* control;
    GtkBox* box = nullptr;
    bool rebuilding = false;      ///< populate() is running
    bool rebuildPending = false;  ///< the tool changed during populate()

    // ToolHandler cannot remove listeners. Its tool-changed listener holds a weak reference to this token, so it does
    // nothing once this bar has been destroyed with its toolbar window.
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};
