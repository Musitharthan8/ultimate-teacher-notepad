/*
 * Ultimate Teacher Notepad
 *
 * Makes the document's scanned background PDF searchable (OCR) in the background, then switches the document to the
 * searchable copy. Undo puts the original PDF back.
 */

#pragma once

#include <string>

#include "BlockingJob.h"
#include "filesystem.h"

class Control;

class OcrJob: public BlockingJob {
public:
    OcrJob(Control* control, fs::path sourcePdf, fs::path outputPdf, std::string language, bool attachPdf);

protected:
    ~OcrJob() override;

public:
    void run() override;

protected:
    void afterRun() override;

private:
    fs::path source;
    fs::path output;
    std::string language;
    bool attachPdf;
    std::string errorMsg;
};
