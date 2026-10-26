#include "EraseUndoAction.h"

#include <memory>  // for __shared_ptr_access, __shar...
#include <vector>  // for vector

#include "control/Control.h"
#include "model/Document.h"
#include "model/Layer.h"                  // for Layer
#include "model/Stroke.h"                 // for Stroke
#include "model/XojPage.h"                // for XojPage
#include "model/eraser/ErasableStroke.h"  // for ErasableStroke
#include "undo/UndoAction.h"              // for UndoAction
#include "util/i18n.h"                    // for _


EraseUndoAction::EraseUndoAction(const PageRef& page, Document* document):
        UndoAction("EraseUndoAction"), document(document) { this->page = page; }

void EraseUndoAction::addOriginal(Layer* layer, Stroke* element, Element::Index pos) {
    original.emplace(layer, element, pos);
}

Stroke* EraseUndoAction::beginMaskedEdit(Layer* layer, Stroke* element, Element::Index originalPos) {
    for (const auto& entry: edited) {
        if (entry.element == element) return element;
    }
    auto copy = element->cloneStroke();
    Stroke* result = copy.get();
    auto [originalElement, currentPos] = layer->removeElement(element);
    auto entry = original.emplace(layer, element, originalPos);
    entry->elementOwn = std::move(originalElement);
    edited.emplace(layer, result, currentPos);
    layer->insertElement(std::move(copy), currentPos);
    return result;
}

void EraseUndoAction::addEdited(Layer* layer, Stroke* element, Element::Index pos) {
    edited.emplace(layer, element, pos);
}

void EraseUndoAction::removeEdited(Stroke* element) {
    for (auto entryIter = edited.begin(); entryIter != edited.end(); ++entryIter) {
        if (entryIter->element == element) {
            edited.erase(entryIter);
            return;
        }
    }
}

bool EraseUndoAction::finalize() {
    bool restructured = false;
    for (auto const& entry: original) {
        if (!entry.element->getErasable()) {
            // Masked edits already own the original and retain one editable
            // vector stroke in the layer, rather than centreline fragments.
            continue;
        }
        if (entry.element->getPointCount() == 0) {
            // TODO (Marmare314): is this really expected behaviour?
            continue;
        } else {
            // Remove the original and add the copy
            restructured = true;
            auto [own, pos] = entry.layer->removeElement(entry.element);
            entry.elementOwn = std::move(own);

            ErasableStroke* e = entry.element->getErasable();
            std::vector<std::unique_ptr<Stroke>> strokeList = e->getStrokes();
            for (auto& stroke: strokeList) {
                // TODO (Marmare314): should use unique_ptr in layer
                auto copy = std::move(stroke);
                this->addEdited(entry.layer, copy.get(), pos);
                entry.layer->insertElement(std::move(copy), pos);
                pos++;
            }

            delete e;
            e = nullptr;
            entry.element->setErasable(nullptr);
        }
    }

    // Record final edited positions after removals in the same gesture, so redo
    // restores the exact z-order as well as undo.
    std::multiset<PageLayerPosEntry<Stroke>> finalEdited;
    for (const auto& entry: edited) {
        const auto pos = entry.layer->indexOf(entry.element);
        if (pos != Element::InvalidIndex) finalEdited.emplace(entry.layer, entry.element, pos);
    }
    edited = std::move(finalEdited);
    return restructured;
}

auto EraseUndoAction::getText() -> std::string { return _("Erase stroke"); }

auto EraseUndoAction::undo(Control* control) -> bool {
    // Preserve the existing GUI path. The stored document is a non-owning fallback for headless eraser tests.
    Document* doc = control ? control->getDocument() : document;
    doc->lock();
    for (auto const& entry: edited) {
        entry.elementOwn = entry.layer->removeElement(entry.element).e;
    }
    for (auto const& entry: original) {
        entry.layer->insertElement(std::move(entry.elementOwn), entry.pos);
    }
    doc->unlock();
    for (auto const& entry: edited) {
        this->page->fireElementChanged(entry.element);
    }
    for (auto const& entry: original) {
        this->page->fireElementChanged(entry.element);
    }

    this->undone = true;
    return true;
}

auto EraseUndoAction::redo(Control* control) -> bool {
    // Preserve the existing GUI path. The stored document is a non-owning fallback for headless eraser tests.
    Document* doc = control ? control->getDocument() : document;
    doc->lock();
    for (auto const& entry: original) {
        entry.elementOwn = entry.layer->removeElement(entry.element).e;
    }
    for (auto const& entry: edited) {
        entry.layer->insertElement(std::move(entry.elementOwn), entry.pos);
    }
    doc->unlock();
    for (auto const& entry: original) {
        this->page->fireElementChanged(entry.element);
    }
    for (auto const& entry: edited) {
        this->page->fireElementChanged(entry.element);
    }

    this->undone = false;
    return true;
}
