/*
 * Ultimate Teacher Notepad
 *
 * The editable page list behind the "Organise Pages" dialog: reorder, turn and remove pages, with undo. Pure logic (no
 * GTK, no files), so it is unit tested; the dialog only shows it and calls these functions.
 */

#pragma once

#include <cstddef>
#include <vector>

#include "PdfTools.h"

namespace utn::pdftools {

class PageOrganiser {
public:
    /// Starts with every page of `file` in its own order, unturned
    PageOrganiser(const fs::path& file, size_t pageCount);

    size_t size() const { return current.size(); }
    const Piece& at(size_t i) const { return current.at(i); }
    const std::vector<Piece>& pieces() const { return current; }

    /// Each returns false (and changes nothing) when it cannot be done
    bool moveUp(size_t i);
    bool moveDown(size_t i);
    /// Moves the page at `from` so it ends up at index `to`
    bool moveTo(size_t from, size_t to);
    /// Turn by quarter turns: +1 is clockwise 90 degrees, -1 anticlockwise; the page's total stays within 0..270
    bool rotate(size_t i, int quarterTurns);
    /// Take the page out of the result. The last page cannot be removed (a PDF needs at least one).
    bool remove(size_t i);

    bool canUndo() const { return !history.empty(); }
    bool undo();
    /// Back to the starting list; can itself be undone
    void reset();
    /// True when the list differs from the one it started with
    bool changed() const;

private:
    void remember();

    std::vector<Piece> original;
    std::vector<Piece> current;
    std::vector<std::vector<Piece>> history;
};

}  // namespace utn::pdftools
