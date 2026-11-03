/*
 * Ultimate Teacher Notepad
 *
 * Rules of the Lesson Navigator that need no GTK. A bookmark is a page label (the label already survives saving, undo,
 * and inserting, deleting and moving pages), so nothing new has to be stored.
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

/// Whether the Previous and Next page buttons have anywhere to go. At either end a button does nothing, so it must not
/// look pressable. An empty document, or an index past the end, offers no move.
bool canGoPrevious(size_t current, size_t pageCount);
bool canGoNext(size_t current, size_t pageCount);

/// The label a bookmark puts on a page that has none
inline constexpr const char* BOOKMARK_LABEL = "Bookmark";

/// A page number typed by a person (counting from 1) as a page index (counting from 0). Nothing but digits and
/// surrounding spaces is a number; 0, anything past the last page and anything too large for a size_t are refused.
std::optional<size_t> parsePageNumber(std::string_view text, size_t pageCount);

enum class BookmarkChange { Add, Remove, KeepLabel };

struct BookmarkToggle {
    BookmarkChange change;
    std::string label;  ///< The label the page should have afterwards
};

/// What bookmarking a page with this label does. A label of the teacher's own (CW, Answer Key) already marks the page
/// in the lesson map: it is kept, never replaced or removed by the bookmark button.
BookmarkToggle toggleBookmark(const std::string& currentLabel);

/// The lesson map lists every page that has a label
bool isMarked(const std::string& label);

/// The nearest marked page after (@p direction > 0) or before (< 0) @p current, if there is one
std::optional<size_t> adjacentMarked(const std::vector<bool>& marked, size_t current, int direction);

/// Recently visited pages, newest first, each once, without the page being shown, at most @p limit of them
std::vector<size_t> recentPages(const std::vector<size_t>& newestFirst, size_t current, size_t limit);

}  // namespace utn
