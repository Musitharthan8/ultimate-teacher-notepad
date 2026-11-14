/*
 * Ultimate Teacher Notepad
 *
 * The File > PDF Tools dialogs: merge PDFs, extract pages, split a PDF, and organise (reorder, turn, remove) pages. Each
 * writes a NEW PDF through utn::pdftools and never changes the source files. The logic lives in pdf/tools/ (and is
 * unit tested); these functions are the windows around it.
 */

#pragma once

#include <gtk/gtk.h>

#include "filesystem.h"

namespace utn::pdftoolsui {

/// `startFolder` is where file choosers open first (may be empty)
void showMergeDialog(GtkWindow* parent, const fs::path& startFolder);

/// `currentPdf` (the open document's background PDF, may be empty) is the suggested source; the teacher can pick another
void showExtractDialog(GtkWindow* parent, const fs::path& currentPdf);
void showSplitDialog(GtkWindow* parent, const fs::path& currentPdf);
void showOrganiseDialog(GtkWindow* parent, const fs::path& currentPdf);

}  // namespace utn::pdftoolsui
