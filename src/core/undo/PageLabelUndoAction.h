/*
 * Ultimate Teacher Notepad
 *
 * Undo action for classroom page labels
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>

#include "UndoAction.h"

class PageLabelUndoAction: public UndoAction {
public:
    PageLabelUndoAction(PageRef page, std::string oldLabel, std::string newLabel);
    ~PageLabelUndoAction() override = default;

    std::string getText() override;
    bool undo(Control* control) override;
    bool redo(Control* control) override;

private:
    std::string oldLabel;
    std::string newLabel;
};
