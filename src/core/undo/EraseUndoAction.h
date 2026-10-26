/*
 * Xournal++
 *
 * Undo action for eraser
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <set>     // for multiset
#include <string>  // for string

#include "model/PageRef.h"  // for PageRef
#include "model/Stroke.h"   // for Stroke

#include "PageLayerPosEntry.h"  // for PageLayerPosEntry
#include "UndoAction.h"         // for UndoAction

class Control;
class Document;
class Layer;

class EraseUndoAction: public UndoAction {
public:
    EraseUndoAction(const PageRef& page, Document* document = nullptr);

public:
    bool undo(Control* control) override;
    bool redo(Control* control) override;

    void addOriginal(Layer* layer, Stroke* element, Element::Index pos);
    // Snapshot a stroke before non-destructive clipping and replace it in place.
    Stroke* beginMaskedEdit(Layer* layer, Stroke* element, Element::Index originalPos);
    void addEdited(Layer* layer, Stroke* element, Element::Index pos);
    [[maybe_unused]] void removeEdited(Stroke* element);

    /// @return whether elements were replaced (the legacy ErasableStroke path). Masked edits change nothing visible
    /// here, so the caller need not repaint the page.
    bool finalize();

    std::string getText() override;

private:
    // Non-owning document reference lets eraser actions run in headless tests without constructing a GUI Control.
    Document* document = nullptr;
    std::multiset<PageLayerPosEntry<Stroke>> edited{};
    std::multiset<PageLayerPosEntry<Stroke>> original{};
};
