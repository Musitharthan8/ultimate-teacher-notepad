/*
 * Ultimate Teacher Notepad
 *
 * OCR for scanned PDFs: turns a PDF made of page pictures into a searchable PDF with an invisible text layer, so
 * selection, find, the highlighter's snap-to-word and sentence select work on it like on any text PDF.
 *
 * Uses Tesseract (Apache-2.0) and Leptonica (BSD-2). Everything here is compiled out without ENABLE_OCR; the
 * functions then report that OCR is unavailable, so callers need no #ifdef.
 */

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "filesystem.h"

namespace utn::ocr {

struct Options {
    /// Tesseract language codes joined with '+', e.g. "eng" or "eng+fra"
    std::string language = "eng";
    /// Resolution pages are rendered at before recognition; 300 is Tesseract's sweet spot
    int dpi = 300;
    /// Folder holding the *.traineddata files; empty means: look in the usual places (see findTessdataDir())
    fs::path tessdataDir;
};

struct Result {
    bool ok = false;
    /// Human readable reason when !ok
    std::string error;
    int pages = 0;
    bool cancelled = false;
};

/// Called after each page with (pagesDone, pagesTotal). Return false to cancel.
using Progress = std::function<bool(int done, int total)>;

/// True when this build has OCR support at all
bool compiledIn();

/// The folder with language data, or an empty path when none is installed. Looks at TESSDATA_PREFIX, folders next
/// to the executable, and the folder Tesseract was installed to at build time.
fs::path findTessdataDir();

/// Language codes with data in the folder (without "osd"), sorted; empty when OCR is unavailable.
std::vector<std::string> availableLanguages(const fs::path& tessdataDir = {});

/// Number of pages of the PDF that already carry text (so are not scans). -1 when the PDF cannot be opened.
int countPagesWithText(const fs::path& pdf);

/// Reads `input`, recognises every page and writes a searchable copy to `output`. The output appears only when the
/// whole run succeeded: it is written under a temporary name first, so a failure or cancel leaves no partial file and
/// an existing `output` is never replaced by half a result. `input` is never modified.
/// Pages are rendered and rebuilt as pictures, so pages that already have real text lose their vector quality: callers
/// should only offer this for scans (see countPagesWithText()).
Result makeSearchable(const fs::path& input, const fs::path& output, const Options& options = {},
                      const Progress& progress = {});

}  // namespace utn::ocr
