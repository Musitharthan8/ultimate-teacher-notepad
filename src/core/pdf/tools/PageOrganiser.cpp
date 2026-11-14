#include "PageOrganiser.h"

#include <algorithm>
#include <utility>

namespace utn::pdftools {

PageOrganiser::PageOrganiser(const fs::path& file, size_t pageCount) {
    for (size_t i = 0; i < pageCount; ++i) {
        original.push_back({file, i, 0});
    }
    current = original;
}

void PageOrganiser::remember() { history.push_back(current); }

bool PageOrganiser::moveUp(size_t i) { return i > 0 && i < current.size() && moveTo(i, i - 1); }

bool PageOrganiser::moveDown(size_t i) { return i + 1 < current.size() && moveTo(i, i + 1); }

bool PageOrganiser::moveTo(size_t from, size_t to) {
    if (from >= current.size() || to >= current.size() || from == to) {
        return false;
    }
    remember();
    Piece moved = current[from];
    current.erase(current.begin() + static_cast<std::ptrdiff_t>(from));
    current.insert(current.begin() + static_cast<std::ptrdiff_t>(to), std::move(moved));
    return true;
}

bool PageOrganiser::rotate(size_t i, int quarterTurns) {
    if (i >= current.size() || quarterTurns % 4 == 0) {
        return false;
    }
    remember();
    const int turns = ((quarterTurns % 4) + 4) % 4;
    current[i].rotateDegrees = (current[i].rotateDegrees + 90 * turns) % 360;
    return true;
}

bool PageOrganiser::remove(size_t i) {
    if (i >= current.size() || current.size() == 1) {
        return false;
    }
    remember();
    current.erase(current.begin() + static_cast<std::ptrdiff_t>(i));
    return true;
}

bool PageOrganiser::undo() {
    if (history.empty()) {
        return false;
    }
    current = std::move(history.back());
    history.pop_back();
    return true;
}

void PageOrganiser::reset() {
    if (!changed()) {
        return;
    }
    remember();
    current = original;
}

bool PageOrganiser::changed() const {
    if (current.size() != original.size()) {
        return true;
    }
    for (size_t i = 0; i < current.size(); ++i) {
        if (current[i].pageIndex != original[i].pageIndex || current[i].rotateDegrees != original[i].rotateDegrees) {
            return true;
        }
    }
    return false;
}

}  // namespace utn::pdftools
