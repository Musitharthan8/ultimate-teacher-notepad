/*
 * Ultimate Teacher Notepad
 *
 * Teacher rail button for one Text tool mode (plain Text or Answer Box). Its selected state follows the active tool
 * and mode, so Text and Answer Box are never shown as selected together. Styling lives in the properties bar.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>

#include <gtk/gtk.h>

#include "control/ToolHandler.h"  // for TextMode
#include "gui/IconNameHelper.h"

#include "AbstractToolItem.h"

class Control;

class ToolTextMode: public AbstractToolItem {
public:
    ToolTextMode(std::string id, Control* control, IconNameHelper iconNameHelper, TextMode mode);
    ~ToolTextMode() override = default;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

private:
    bool isSelected() const;
    void syncButton();

    Control* control;
    TextMode mode;
    std::string iconName;
    GtkToggleButton* button = nullptr;
};
