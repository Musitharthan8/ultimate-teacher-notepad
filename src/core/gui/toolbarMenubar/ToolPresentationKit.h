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
class ToolStudentView;

class ToolPresentationKit: public AbstractToolItem {
public:
    static void runClassroomAction(Control* control, Action action);
    /// What the "Back to normal" button does: Spotlight and Curtain off, Freeze and Blank off. Saved work is untouched.
    static void backToNormal(Control* control, ToolStudentView* students);

    /// @param students Student View, so Freeze, Blank and the way back to normal sit with Spotlight and Curtain. Without
    ///        it the controls cover only the overlays.
    ToolPresentationKit(std::string id, Control* control, IconNameHelper iconNameHelper,
                        ToolStudentView* students = nullptr);
    ~ToolPresentationKit() override = default;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

private:
    Control* control;
    ToolStudentView* students;
    std::string iconName;
};
