#include "PageNavigation.h"

#include <algorithm>
#include <cstdint>  // for SIZE_MAX

namespace utn {

bool canGoPrevious(size_t current, size_t pageCount) { return current > 0 && current < pageCount; }

bool canGoNext(size_t current, size_t pageCount) { return current + 1 < pageCount; }

std::optional<size_t> parsePageNumber(std::string_view text, size_t pageCount) {
    while (!text.empty() && text.front() == ' ') {
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ') {
        text.remove_suffix(1);
    }
    if (text.empty() || pageCount == 0) {
        return std::nullopt;
    }
    size_t number = 0;
    for (char c: text) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        const size_t digit = static_cast<size_t>(c - '0');
        // number never exceeds pageCount here, so this cannot overflow unless pageCount itself is near the limit
        if (number > (SIZE_MAX - digit) / 10) {
            return std::nullopt;
        }
        number = number * 10 + digit;
        if (number > pageCount) {
            return std::nullopt;
        }
    }
    if (number == 0) {
        return std::nullopt;
    }
    return number - 1;
}

BookmarkToggle toggleBookmark(const std::string& currentLabel) {
    if (currentLabel.empty()) {
        return {BookmarkChange::Add, BOOKMARK_LABEL};
    }
    if (currentLabel == BOOKMARK_LABEL) {
        return {BookmarkChange::Remove, {}};
    }
    return {BookmarkChange::KeepLabel, currentLabel};
}

bool isMarked(const std::string& label) { return !label.empty(); }

std::optional<size_t> adjacentMarked(const std::vector<bool>& marked, size_t current, int direction) {
    if (direction > 0) {
        for (size_t i = current + 1; i < marked.size(); ++i) {
            if (marked[i]) {
                return i;
            }
        }
    } else if (direction < 0) {
        for (size_t i = std::min(current, marked.size()); i > 0; --i) {
            if (marked[i - 1]) {
                return i - 1;
            }
        }
    }
    return std::nullopt;
}

std::vector<size_t> recentPages(const std::vector<size_t>& newestFirst, size_t current, size_t limit) {
    std::vector<size_t> out;
    for (size_t page: newestFirst) {
        if (out.size() >= limit) {
            break;
        }
        if (page == current || std::find(out.begin(), out.end(), page) != out.end()) {
            continue;
        }
        out.push_back(page);
    }
    return out;
}

}  // namespace utn
