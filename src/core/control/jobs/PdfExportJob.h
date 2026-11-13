/*
 * Xournal++
 *
 * A job to export PDF
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "BaseExportJob.h"  // for BaseExportJob
#include "filesystem.h"     // for path

class Control;

class PdfExportJob: public BaseExportJob {
public:
    PdfExportJob(Control* control);

protected:
    ~PdfExportJob() override;

public:
    void run() override;

    /// Choose the destination without the file chooser (Save As with the PDF type). False if it can't be used.
    bool setDestination(const fs::path& file) { return testAndSetFilepath(file); }

protected:
    void addFilterToDialog(GtkFileChooser* dialog) override;
    void setExtensionFromFilter(fs::path& p, const char* filterName) const override;
};
