/*
 * Ultimate Teacher Notepad
 *
 * Answer box toolbar toggle
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>

#include <gtk/gtk.h>

#include "AbstractToolItem.h"
#include "gui/IconNameHelper.h"

class Control;

class ToolAnswerBox: public AbstractToolItem {
public:
    ToolAnswerBox(std::string id, Control* control, IconNameHelper iconNameHelper);
    ~ToolAnswerBox() override = default;

    xoj::util::WidgetSPtr createItem(bool horizontal) override;

    std::string getToolDisplayName() const override;
    GtkWidget* getNewToolIcon() const override;

private:
    Control* control;
    std::string iconName;
};
