/*
 * Ultimate Teacher Notepad
 *
 * Command Palette search. A command here is only a description (name, words that find it, whether it works right now);
 * running it is left to the action it stands for, so the palette never has a second copy of what a button does.
 * Free of GTK so the ranking is unit tested.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace utn {

struct PaletteCommand {
    std::string id;
    std::string title;     ///< What a teacher would call it: "Next page", "Pen"
    std::string category;  ///< "Page", "Tool", "Workspace", "Classroom", ...
    std::string keywords;  ///< Other words that should find it, space separated
    std::string shortcut;  ///< As shown to people, empty if none
    bool enabled = true;
    std::string unavailableReason;  ///< Why it cannot run now, for disabled commands
    int priority = 0;               ///< Higher first among equal matches; teacher-facing commands rank above the rest
};

/**
 * Indices of the commands that match @p query, best first. Every word of the query must start a word of the title,
 * the keywords or the category, or appear inside the title. Commands that cannot run now still match but come after
 * those that can. Equal matches keep priority, then the order they were given in. An empty query matches everything.
 */
std::vector<size_t> rankCommands(const std::vector<PaletteCommand>& commands, std::string_view query);

/// The words shown on the right of a row: category and shortcut, or "<unavailableWord>: reason" when it cannot run.
/// accessibleLabel() must always contain these, so what is read out matches what is seen.
std::string visibleDetail(const PaletteCommand& command, std::string_view unavailableWord);

/// "Next page, Page, shortcut Page Down" or "Delete page, Page, unavailable: Only one page"
std::string accessibleLabel(const PaletteCommand& command);

/// A query that is only a page number, optionally led by "go", "to", "page", "pg" or "p" ("12", "page 12", "go to page
/// 3"), as a page index in a document of @p pageCount pages
std::optional<size_t> pageNumberFromQuery(std::string_view query, size_t pageCount);

}  // namespace utn
