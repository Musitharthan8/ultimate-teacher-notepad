#include "OcrJob.h"

#include <utility>

#include "control/Control.h"                          // for Control
#include "control/PageBackgroundChangeController.h"  // for PageBackgroundChangeController
#include "gui/MainWindow.h"                          // for MainWindow
#include "pdf/ocr/PdfOcr.h"                          // for makeSearchable
#include "util/XojMsgBox.h"                          // for showErrorToUser
#include "util/i18n.h"                               // for _, FS, _F

OcrJob::OcrJob(Control* control, fs::path sourcePdf, fs::path outputPdf, std::string language, bool attachPdf):
        BlockingJob(control, _("Making the PDF searchable")),
        source(std::move(sourcePdf)),
        output(std::move(outputPdf)),
        language(std::move(language)),
        attachPdf(attachPdf) {}

OcrJob::~OcrJob() = default;

void OcrJob::run() {
    utn::ocr::Options options;
    options.language = this->language;
    const auto result = utn::ocr::makeSearchable(this->source, this->output, options, [this](int done, int total) {
        control->setCurrentState(static_cast<size_t>(done) * 100 / static_cast<size_t>(total));
        return true;
    });
    if (!result.ok) {
        this->errorMsg = result.error;
    }
    if (control->getWindow()) {
        callAfterRun();
    }
}

void OcrJob::afterRun() {
    if (!this->errorMsg.empty()) {
        XojMsgBox::showErrorToUser(control->getGtkWindow(), this->errorMsg);
        return;
    }
    // Switch the document to the searchable copy; this adds an undo step that restores the original PDF
    control->getPageBackgroundChangeController()->changePdfPagesBackground(this->output, this->attachPdf);
    control->getWindow()->showToast(FS(_F("The PDF is now searchable. A copy was saved as \"{1}\"") %
                                       this->output.filename().u8string()),
                                    /*offerUndo=*/true);
}
