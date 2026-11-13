#include "TextBackgroundUndoAction.h"

#include "control/Control.h"
#include "model/Document.h"
#include "model/Text.h"
#include "model/XojPage.h"
#include "util/Range.h"
#include "util/i18n.h"

TextBackgroundUndoAction::TextBackgroundUndoAction(const PageRef& page): UndoAction("TextBackgroundUndoAction") {
    this->page = page;
}

void TextBackgroundUndoAction::addText(Text* text, Color before, Color after) {
    this->entries.push_back({text, before, after});
}

bool TextBackgroundUndoAction::undo(Control* control) {
    if (this->entries.empty()) {
        return true;
    }
    Range range;
    Document* doc = control->getDocument();
    doc->lock();
    for (const Entry& entry: this->entries) {
        // The padding is part of the bounds, so repaint the area before and after the change
        range = range.unite(Range(entry.text->getBoundingBox()));
        entry.text->setBackgroundColor(entry.before);
        range = range.unite(Range(entry.text->getBoundingBox()));
    }
    doc->unlock();
    this->page->fireRangeChanged(range);
    return true;
}

bool TextBackgroundUndoAction::redo(Control* control) {
    if (this->entries.empty()) {
        return true;
    }
    Range range;
    Document* doc = control->getDocument();
    doc->lock();
    for (const Entry& entry: this->entries) {
        range = range.unite(Range(entry.text->getBoundingBox()));
        entry.text->setBackgroundColor(entry.after);
        range = range.unite(Range(entry.text->getBoundingBox()));
    }
    doc->unlock();
    this->page->fireRangeChanged(range);
    return true;
}

std::string TextBackgroundUndoAction::getText() { return _("Change text background"); }
