/*
 * Ultimate Teacher Notepad
 *
 * Undo action for changing the background colour of existing text boxes
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>
#include <vector>

#include "model/PageRef.h"
#include "util/Color.h"

#include "UndoAction.h"

class Text;
class Control;

class TextBackgroundUndoAction: public UndoAction {
public:
    explicit TextBackgroundUndoAction(const PageRef& page);
    ~TextBackgroundUndoAction() override = default;

    bool undo(Control* control) override;
    bool redo(Control* control) override;
    std::string getText() override;

    void addText(Text* text, Color before, Color after);

private:
    struct Entry {
        Text* text;
        Color before;
        Color after;
    };
    std::vector<Entry> entries;
};
