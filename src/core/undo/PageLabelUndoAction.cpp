/*
 * Ultimate Teacher Notepad
 *
 * Undo action for classroom page labels
 *
 * Based on Xournal++ GPLv2+
 */

#include "PageLabelUndoAction.h"

#include <utility>

#include "control/Control.h"
#include "model/Document.h"
#include "model/XojPage.h"
#include "util/i18n.h"

PageLabelUndoAction::PageLabelUndoAction(PageRef page, std::string oldLabel, std::string newLabel):
        UndoAction("PageLabelUndoAction"),
        oldLabel(std::move(oldLabel)),
        newLabel(std::move(newLabel)) {
    this->page = std::move(page);
}

auto PageLabelUndoAction::getText() -> std::string {
    return _("Change page label");
}

auto PageLabelUndoAction::undo(Control* control) -> bool {
    auto* doc = control->getDocument();
    doc->lock();
    this->page->setUtnPageLabel(this->oldLabel);
    doc->unlock();
    return true;
}

auto PageLabelUndoAction::redo(Control* control) -> bool {
    auto* doc = control->getDocument();
    doc->lock();
    this->page->setUtnPageLabel(this->newLabel);
    doc->unlock();
    return true;
}
