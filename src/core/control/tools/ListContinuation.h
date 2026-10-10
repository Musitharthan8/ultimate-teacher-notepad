/*
 * Ultimate Teacher Notepad
 *
 * Decides what Enter inserts after a bulleted or numbered line in the UTN text editor.
 * Pure string logic, so it can be unit tested without GTK.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <cctype>
#include <string>
#include <string_view>

namespace utn::list {

struct Continuation {
    std::string prefix;  ///< Marker for the next line, e.g. "• " or "4. ". Empty when the line is not a list item
    bool emptyItem = false;  ///< The line holds only its marker. Enter ends the list and removes the marker
};

/// Lists longer than this many digits are treated as ordinary text, which keeps the number from overflowing
inline constexpr size_t MAX_LIST_NUMBER_DIGITS = 9;

/**
 * Continuation for the text before the cursor on the current line.
 * Bullets are "• " and numbered items are "N. " where N is a positive integer.
 */
inline Continuation listContinuation(std::string_view line) {
    Continuation result;
    constexpr std::string_view BULLET = "• ";  // the bullet is three bytes in UTF-8
    if (line.starts_with(BULLET)) {
        result.prefix = std::string(BULLET);
        result.emptyItem = line.size() == BULLET.size();
        return result;
    }

    size_t digits = 0;
    while (digits < line.size() && std::isdigit(static_cast<unsigned char>(line[digits]))) {
        ++digits;
    }
    const bool numbered = digits > 0 && digits <= MAX_LIST_NUMBER_DIGITS && digits + 1 < line.size() &&
                          line[digits] == '.' && line[digits + 1] == ' ';
    if (!numbered) {
        return result;
    }

    const unsigned long long number = std::stoull(std::string(line.substr(0, digits)));
    if (number == 0) {
        return result;  // "0. " is not a list item we should continue
    }
    result.prefix = std::to_string(number + 1) + ". ";
    result.emptyItem = line.size() == digits + 2;
    return result;
}

}  // namespace utn::list
