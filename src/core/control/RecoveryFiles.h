/*
 * Ultimate Teacher Notepad
 *
 * Finds the files a crashed or killed session left behind, so the next start can offer to restore them.
 *
 * Recovery file kinds:
 *  - Emergency:  `emergencysave.xopp` in the configuration folder, written when UTN is asked to stop (SIGINT/SIGTERM).
 *  - Autosave:   `.<name>.autosave.xopp` next to a saved document, or `<pid>.autosave.xopp` in the cache folder
 *                `autosaves` for a document that was never saved.
 *  - LegacySwap: `*.swap` in the cache folder `autosaves`. UTN itself never wrote these; an older install or a tool
 *                that did is still honoured, but only when the file is a loadable document. Anything else is ignored.
 *
 * Nothing here ever modifies or deletes a file: a corrupted candidate is skipped (and stays on disk), and removal is
 * left to the user's explicit choice once a restore succeeded.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <ctime>
#include <vector>

#include "filesystem.h"  // for path

namespace RecoveryFiles {
enum class Kind { Emergency, Autosave, LegacySwap };

struct Candidate {
    fs::path file;
    Kind kind = Kind::Autosave;
    fs::path original;       ///< The saved document it belongs to. Empty when there is none.
    std::time_t modified = 0;
};

struct Sources {
    fs::path emergencyFile;
    fs::path autosaveFolder;
    std::vector<fs::path> documents;  ///< Saved documents whose neighbouring autosave file may exist
};

/// At most this many candidates are returned, and at most this many directory entries / documents are examined
constexpr size_t MAX_CANDIDATES = 16;
constexpr size_t MAX_ENTRIES_EXAMINED = 200;
constexpr size_t MAX_DOCUMENTS = 32;

/// Where AutosaveJob writes the autosave of the saved document `document`
fs::path autosavePathFor(const fs::path& document);

/// True if `file` loads as a document. Never throws.
bool isLoadable(const fs::path& file);

/// Valid candidates, newest first, without duplicates. Read-only.
std::vector<Candidate> discover(const Sources& sources);
}  // namespace RecoveryFiles
