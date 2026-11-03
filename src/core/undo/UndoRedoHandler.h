/*
 * Xournal++
 *
 * Handles Undo and Redo
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstdint>  // for uint64_t
#include <deque>   // for deque
#include <string>  // for string
#include <vector>  // for vector

#include "model/PageRef.h"  // for PageRef

#include "UndoAction.h"  // for UndoActionPtr

class Control;

class UndoRedoListener {
public:
    virtual void undoRedoChanged() = 0;
    virtual void undoRedoPageChanged(PageRef page) = 0;
    /// UTN: the application became busy (a save or export started) or free again; the history itself is as it was
    virtual void undoRedoBusyChanged() {}

    virtual ~UndoRedoListener() = default;
};

class UndoRedoHandler {
public:
    explicit UndoRedoHandler(Control* control);
    virtual ~UndoRedoHandler();

    void undo();
    void redo();

    bool canUndo();
    bool canRedo();

    void addUndoAction(UndoActionPtr action);
    /// UTN: takes the newest undo step back out, to fold it into one step with what follows; nullptr when there is none
    UndoActionPtr takeLastUndoAction();

    std::string undoDescription();
    std::string redoDescription();

    /// UTN Undo History: how many steps Undo (Redo) can take
    size_t undoCount() const;
    size_t redoCount() const;
    /// UTN Undo History: the descriptions of at most @p max steps, what Undo (Redo) does first coming first
    std::vector<std::string> undoTexts(size_t max);
    std::vector<std::string> redoTexts(size_t max);
    /// UTN Undo History: false when the last undo() or redo() reported that its step went wrong
    bool lastStepSucceeded() const;
    /// UTN Undo History: counts every change of the history (a step added, undone, redone, taken back, all cleared).
    /// Equal step counts do not mean the same history: one step replaced by another leaves them equal, the revision not.
    uint64_t revision() const;

    void clearContents();

    void fireUpdateUndoRedoButtons(const std::vector<PageRef>& pages);
    void addUndoRedoListener(UndoRedoListener* listener);
    /// UTN: for listeners that go before the handler (a panel); safe while the listeners are being told
    void removeUndoRedoListener(UndoRedoListener* listener);
    /// UTN: tells the listeners that the application became busy or free again
    void fireBusyChanged();

    bool isChanged();
    bool isChangedAutosave();
    void documentAutosaved();
    void documentSaved();

private:
    void clearRedo();
    void printContents();

private:
    std::deque<UndoActionPtr> undoList;
    std::deque<UndoActionPtr> redoList;

    UndoAction* savedUndo = nullptr;
    UndoAction* autosavedUndo = nullptr;

    std::vector<UndoRedoListener*> listener;
    /// Depth of fireUpdateUndoRedoButtons: a listener removed meanwhile is blanked, and dropped afterwards
    int firing = 0;
    bool lastStepOk = true;
    uint64_t historyRevision = 0;

    Control* control = nullptr;
};
