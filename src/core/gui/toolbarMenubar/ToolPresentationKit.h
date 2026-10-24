/*
 * Ultimate Teacher Notepad
 *
 * Classroom presentation toolkit
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>

#include <gtk/gtk.h>

#include "AbstractToolItem.h"
#include "enums/Action.enum.h"
#include "gui/IconNameHelper.h"

class Control;

class ToolPresentationKit: public AbstractToolItem {
public:
    static void runClassroomAction(Control* control, Action action);

    ToolPresentationKit(std::string id, Control* control, IconNameHelper iconNameHelper);
    ~ToolPresentationKit() override = default;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

private:
    Control* control;
    std::string iconName;
};
