#include "TextBoxUndoAction.h"

#include <algorithm>  // for max, min
#include <memory>     // for allocator, __shared_ptr_access, __share...
#include <utility>

#include "control/Control.h"
#include "model/Document.h"
#include "model/Element.h"    // for Element
#include "model/Layer.h"      // for Layer
#include "model/PageRef.h"    // for PageRef
#include "model/XojPage.h"    // for XojPage
#include "undo/UndoAction.h"  // for UndoAction
#include "util/Rectangle.h"   // for Rectangle
#include "util/i18n.h"        // for _

using xoj::util::Rectangle;

TextBoxUndoAction::TextBoxUndoAction(const PageRef& page, Layer* layer, Element* element, ElementPtr oldelement,
                                   Document* document):
        UndoAction("TextBoxUndoAction"), layer(layer), element(element), oldelement(std::move(oldelement)),
        document(document) {
    this->page = page;
}

TextBoxUndoAction::~TextBoxUndoAction() = default;

auto TextBoxUndoAction::getText() -> std::string { return _("Edit text"); }

auto TextBoxUndoAction::undo(Control* control) -> bool {
    Document* doc = control ? control->getDocument() : document;
    doc->lock();
    auto rect = element->getBoundingBox();
    rect.unite(oldelement->getBoundingBox());

    // swap them to be memory safe
    auto [elementPtr, elementIndex] =
            this->layer->removeElement(std::exchange(this->element, this->oldelement.get()));
    this->layer->insertElement(std::exchange(this->oldelement, std::move(elementPtr)), elementIndex);
    doc->unlock();
    this->page->fireRectChanged(rect);

    this->undone = true;
    return true;
}

auto TextBoxUndoAction::redo(Control* control) -> bool {
    undo(control);
    this->undone = false;
    return true;
}
