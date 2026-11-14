#include "PdfExportJob.h"

#include <memory>   // for unique_ptr, allocator
#include <string>   // for string
#include <utility>  // for move

#include "control/Control.h"               // for Control
#include "control/jobs/BaseExportJob.h"    // for BaseExportJob
#include "model/Document.h"                // for Document
#include "pdf/base/XojPdfExport.h"         // for XojPdfExport
#include "pdf/base/XojPdfExportFactory.h"  // for XojPdfExportFactory
#include "util/PathUtil.h"                 // for clearExtensions
#include "util/i18n.h"                     // for _

PdfExportJob::PdfExportJob(Control* control): BaseExportJob(control, _("PDF Export")) {}

PdfExportJob::~PdfExportJob() = default;

void PdfExportJob::addFilterToDialog(GtkFileChooser* dialog) {
    addFileFilterToDialog(dialog, _("PDF files"), "application/pdf");
}

void PdfExportJob::setExtensionFromFilter(fs::path& file, const char* /*filterName*/) const {
    // Remove any pre-existing extension and adds .pdf
    Util::clearExtensions(file, ".pdf");
    file += ".pdf";
}

void PdfExportJob::run() {
    Document* doc = control->getDocument();

    doc->lock_shared();
    std::unique_ptr<XojPdfExport> pdfe = XojPdfExportFactory::createExport(doc, control);
    doc->unlock_shared();

    // Write beside the destination and rename on success: a failed export leaves no partial file, never replaces
    // an existing file before the new one is complete, and replaces (rather than writes through) a symlink
    fs::path partial = this->filepath;
    partial += ".utn-part";
    std::error_code ec;
    if (!pdfe->createPdf(partial, false)) {
        this->errorMsg = pdfe->getLastError();
        fs::remove(partial, ec);
    } else {
        fs::rename(partial, this->filepath, ec);
        if (ec) {
            this->errorMsg = _("The PDF could not be saved to the chosen file: ") + ec.message();
            fs::remove(partial, ec);
        }
    }
    // Success is reported too, so the document status can say the export finished
    if (control->getWindow()) {
        callAfterRun();
    }
}
