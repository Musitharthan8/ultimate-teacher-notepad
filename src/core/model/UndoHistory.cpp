#include "UndoHistory.h"

#include <algorithm>  // for min, max

#include "util/i18n.h"  // for _, FS, _F

namespace utn {

UndoHistoryView buildUndoHistory(const std::vector<std::string>& undoNewestFirst, std::size_t undoTotal,
                                 const std::vector<std::string>& redoNextFirst, std::size_t redoTotal,
                                 std::size_t limit) {
    const std::size_t undoAvailable = std::min(undoNewestFirst.size(), undoTotal);
    const std::size_t redoAvailable = std::min(redoNextFirst.size(), redoTotal);
    // Half each; a side that needs less leaves the rest to the other
    const std::size_t half = limit / 2;
    std::size_t redoShown = std::min(redoAvailable, std::max(half, limit - std::min(limit, undoAvailable)));
    std::size_t undoShown = std::min(undoAvailable, limit - redoShown);

    UndoHistoryView view;
    view.rows.reserve(redoShown + undoShown);
    for (std::size_t i = redoShown; i > 0; --i) {
        view.rows.push_back({HistorySide::Redo, i, stepText(redoNextFirst[i - 1])});
    }
    view.positionIndex = redoShown;
    for (std::size_t i = 0; i < undoShown; ++i) {
        view.rows.push_back({HistorySide::Undo, i + 1, stepText(undoNewestFirst[i])});
    }
    view.olderHidden = undoTotal - undoShown;
    view.newerHidden = redoTotal - redoShown;
    return view;
}

std::string stepText(const std::string& raw) {
    const char* space = " \t\r\n";
    const auto first = raw.find_first_not_of(space);
    if (first == std::string::npos) {
        return _("Change");
    }
    std::string text = raw.substr(first, raw.find_last_not_of(space) - first + 1);
    // GroupUndoAction's word for a group with nothing in it; never meant for people
    if (text == "!! NOTHING !!") {
        return _("Change");
    }
    return text;
}

std::string rowLabel(const HistoryRow& row) {
    if (row.side == HistorySide::Undo) {
        return row.steps == 1 ? FS(_F("{1} (next Undo)") % row.text) : row.text;
    }
    return row.steps == 1 ? FS(_F("{1} (next Redo, undone)") % row.text) : FS(_F("{1} (undone)") % row.text);
}

std::string rowAccessibleName(const HistoryRow& row) {
    const std::string steps = row.steps == 1 ? std::string(_("1 step")) : FS(_F("{1} steps") % row.steps);
    if (row.side == HistorySide::Undo) {
        return FS(_F("{1}: activate to undo back to before it, {2}") % rowLabel(row) % steps);
    }
    return FS(_F("{1}: activate to Redo up to and including it, {2}") % rowLabel(row) % steps);
}

HistoryGate historyGate(bool busy, bool coverUp, bool quietPresent) {
    if (coverUp) {
        return HistoryGate::Covered;
    }
    if (quietPresent) {
        return HistoryGate::QuietPresent;
    }
    return busy ? HistoryGate::Busy : HistoryGate::Open;
}

bool gateHidesList(HistoryGate gate) { return gate == HistoryGate::Covered || gate == HistoryGate::QuietPresent; }

std::string gateReason(HistoryGate gate) {
    switch (gate) {
        case HistoryGate::Open:
            return {};
        case HistoryGate::Busy:
            return _("Unavailable while UTN is busy saving or exporting");
        case HistoryGate::Covered:
            return _("Unavailable while Blank covers your screen");
        case HistoryGate::QuietPresent:
            return _("Unavailable in Present until the tools are shown");
    }
    return {};
}

}  // namespace utn
