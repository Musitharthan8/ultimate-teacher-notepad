/*
 * Ultimate Teacher Notepad
 *
 * Undo History panel rules. The history itself is the existing UndoRedoHandler's: this only decides which of its steps
 * are listed (newest first, bounded), where the current point is (between what Redo would redo and what Undo would
 * undo), what each row says, and when the list must stay out of sight. Free of GTK so it is unit tested.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace utn {

/// At most this many steps are listed
inline constexpr std::size_t UNDO_HISTORY_LIMIT = 50;

enum class HistorySide {
    Undo,  ///< done: Undo takes it away
    Redo   ///< undone: Redo brings it back
};

struct HistoryRow {
    HistorySide side = HistorySide::Undo;
    /// How many Undo (or Redo) presses reach it: 1 is what Undo (Redo) does next
    std::size_t steps = 1;
    std::string text;  ///< The step as people read it ("Write text")
};

struct UndoHistoryView {
    /// Newest first: the undone steps (farthest from the current point first), then the done ones (nearest first)
    std::vector<HistoryRow> rows;
    /// Index into rows where the current point is: rows before it are undone, rows from it on are done
    std::size_t positionIndex = 0;
    std::size_t olderHidden = 0;  ///< Done steps past the end of the list
    std::size_t newerHidden = 0;  ///< Undone steps past the top of the list
};

/**
 * The rows for a history. @p undoNewestFirst are the texts of the done steps, what Undo undoes first; @p redoNextFirst
 * the undone steps, what Redo redoes first. The totals may be larger than the texts handed over. Both sides get half of
 * @p limit; a side that needs less leaves the rest to the other.
 */
UndoHistoryView buildUndoHistory(const std::vector<std::string>& undoNewestFirst, std::size_t undoTotal,
                                 const std::vector<std::string>& redoNextFirst, std::size_t redoTotal,
                                 std::size_t limit = UNDO_HISTORY_LIMIT);

/// The step's own description, trimmed; a step without one is "Change"
std::string stepText(const std::string& raw);

/// What the row reads: "Write text (next Undo)", "Change color (next Redo, undone)", "Delete"
std::string rowLabel(const HistoryRow& row);

/// The row's name for screen readers: its label first, then what activating it does
std::string rowAccessibleName(const HistoryRow& row);

/// Whether the panel may show and use the history now
enum class HistoryGate {
    Open,
    Busy,         ///< a save or export runs: readable, but nothing may run
    Covered,      ///< the Blank cover hides the teacher's screen: nothing is shown
    QuietPresent  ///< Present with its tools put away: nothing is shown
};

/// Privacy first: covered or quiet Present win over busy
HistoryGate historyGate(bool busy, bool coverUp, bool quietPresent);

/// True when the steps must not be listed at all
bool gateHidesList(HistoryGate gate);

/// Why the panel cannot be used, in words starting "Unavailable"; empty when it can
std::string gateReason(HistoryGate gate);

}  // namespace utn
