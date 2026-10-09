/*
 * Ultimate Teacher Notepad
 *
 * Workspace switcher: Teach, Mark and Present, one tap apart in the app bar.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>

#include "AbstractToolItem.h"

class Control;

class ToolWorkspaceSwitch: public AbstractToolItem {
public:
    ToolWorkspaceSwitch(std::string id, Control* control);

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

private:
    Control* control;
};
