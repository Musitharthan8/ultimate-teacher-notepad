/*
 * Ultimate Teacher Notepad
 * Classroom material and marking workflows, based on Xournal++ GPLv2+.
 */
#pragma once

#include <string>

#include "AbstractToolItem.h"
#include "gui/IconNameHelper.h"

class Control;

class ToolClassroomWorkflow: public AbstractToolItem {
public:
    ToolClassroomWorkflow(std::string id, Control* control, IconNameHelper icons);
    xoj::util::WidgetSPtr createItem(bool horizontal) override;
    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

private:
    Control* control;
    std::string iconName;
};
