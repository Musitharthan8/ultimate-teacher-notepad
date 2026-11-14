/*
 * Ultimate Teacher Notepad
 *
 * PDF tools for lesson material: merge several PDFs, extract or split pages, and reorder, rotate or delete pages.
 * Built on QPDF. Every tool writes a NEW file; inputs are never modified, an existing output is only replaced by a
 * complete result (written under a temporary name first), and the output may never be one of the inputs.
 * Without ENABLE_QPDF every call reports that PDF tools are unavailable, so callers need no #ifdef.
 */

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "filesystem.h"

namespace utn::pdftools {

/// One page of the result: page `pageIndex` (0-based) of `file`, turned by `rotateDegrees` (0, 90, 180 or 270,
/// added to any rotation the page already has).
struct Piece {
    fs::path file;
    size_t pageIndex = 0;
    int rotateDegrees = 0;
};

struct Result {
    bool ok = false;
    /// Human readable reason when !ok
    std::string error;
    /// Pages in the written file
    size_t pages = 0;
};

/// True when this build can do PDF tools at all
bool available();

/// Number of pages, or -1 when the file cannot be read (missing, damaged, or password protected). `error` is filled
/// with the reason when given.
long pageCount(const fs::path& pdf, std::string* error = nullptr);

/// Turns text like "1-3, 5, 8-" into 0-based page indices, in the order written (so "3,1" keeps that order and "2,2"
/// repeats a page). Pages count from 1; "8-" means page 8 to the end, "-3" means the first three, "all" every page.
/// Returns an empty vector and sets `error` on anything that is not a page of this document.
std::vector<size_t> parsePageRanges(const std::string& text, size_t pageCount, std::string* error = nullptr);

/// Builds `output` from the given pieces, in order. The general operation behind everything below.
Result assemble(const std::vector<Piece>& pieces, const fs::path& output);

/// All pages of each input, one after the other
Result mergeFiles(const std::vector<fs::path>& inputs, const fs::path& output);

/// The pages named by `ranges` (see parsePageRanges), in that order
Result extractPages(const fs::path& input, const std::string& ranges, const fs::path& output);

/// Splits into files of `pagesPerFile` pages named "<stem> (part 1).pdf", "<stem> (part 2).pdf"... inside `folder`.
/// Existing files are never replaced: if any target name is taken nothing is written. `written` lists the new files.
Result splitByPageCount(const fs::path& input, size_t pagesPerFile, const fs::path& folder, const std::string& stem,
                        std::vector<fs::path>* written = nullptr);

}  // namespace utn::pdftools
