/*
 * Ultimate Teacher Notepad
 *
 * The backup step of a normal Save, as one transaction: the document that is replaced and any older backup next to it
 * both stay recoverable until the new file is in place.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>

#include "filesystem.h"  // for path

class SaveHandler;
class ProgressListener;

namespace SaveBackup {
struct Result {
    /// Empty when the document was saved. Already worded for the user.
    std::string error;
    /// False when the save failed before anything was written (no backup could be made)
    bool saveAttempted = true;
};

/**
 * Saves the prepared `handler` to `target` with `SaveHandler::saveAtomically`.
 *
 * With `createBackup`, the previous `target` is first copied to a private candidate file; an existing `target~` is
 * never touched at that point. Afterwards:
 *  - saved: the candidate and the now outdated `target~` are removed (the usual state after a successful save).
 *  - failed: if there is no `target~` the candidate becomes it. If there is one, it stays as it is and the candidate is
 *    only dropped when `target` still holds exactly the same bytes; otherwise the candidate is kept under its own name
 *    (named in the error text), because it is then the only copy of the previous document.
 */
Result saveWithBackup(SaveHandler& handler, const fs::path& target, bool createBackup,
                      ProgressListener* listener = nullptr);
}  // namespace SaveBackup
