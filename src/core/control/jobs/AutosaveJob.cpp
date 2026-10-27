#include "AutosaveJob.h"

#include <glib.h>  // for g_message, g_warning

#include "control/Control.h"              // for Control
#include "control/RecoveryFiles.h"        // for autosavePathFor
#include "control/UtnPerf.h"              // for utn::perf
#include "control/jobs/Job.h"             // for JOB_TYPE_AUTOSAVE, JobType
#include "control/xojfile/SaveHandler.h"  // for SaveHandler
#include "model/Document.h"               // for Document
#include "undo/UndoRedoHandler.h"         // for UndoRedoHandler
#include "util/PathUtil.h"                // for clearExtensions, getAutosav...
#include "util/XojMsgBox.h"               // for XojMsgBox
#include "util/i18n.h"                    // for FS, _F

#include "filesystem.h"  // for path

AutosaveJob::AutosaveJob(Control* control): control(control) {}

AutosaveJob::~AutosaveJob() = default;

void AutosaveJob::afterRun() {
    std::string msg = FS(_F("Error while autosaving: {1}") % this->error);
    XojMsgBox::showErrorToUser(control->getGtkWindow(), msg);
}

void AutosaveJob::run() {
    utn::perf::ScopedTimer autosaveTimer(utn::perf::Metric::Autosave);
    SaveHandler handler;

    // The undo position is recorded by Control::autosaveCallback on the main thread before this job is scheduled.
    Document* doc = control->getDocument();

    fs::path filepath;
    {
        // Keep the shared lock as short as possible: it blocks the exclusive lock that pen-up needs.
        // (the metric covers the wait for the lock as well as the time it is held)
        utn::perf::ScopedTimer lockHoldTimer(utn::perf::Metric::AutosaveLockHold);
        doc->lock_shared();
        filepath = doc->getFilepath();

        // The names are the ones RecoveryFiles::discover() looks for at the next start
        if (filepath.empty()) {
            filepath = Util::getAutosaveFilepath();
            Util::clearExtensions(filepath);
            filepath += ".autosave.xopp";
        } else {
            filepath = RecoveryFiles::autosavePathFor(filepath);
        }

        handler.prepareSave(doc, filepath);
        doc->unlock_shared();
    }

    g_message("%s", FS(_F("Autosaving to {1}") % filepath.string()).c_str());

    // Writes a uniquely named temporary file and replaces the previous autosave in one step: the last good autosave
    // is never moved aside or removed first, so a crash or error at any point leaves it intact.
    handler.saveAtomically(filepath);

    doc->lock();
    handler.updateDocumentInfo(doc);
    doc->unlock();

    this->error = handler.getErrorMessage();
    if (!this->error.empty()) {
        callAfterRun();
    } else {
        control->setLastAutosaveFile(filepath);
    }
}

auto AutosaveJob::getType() -> JobType { return JOB_TYPE_AUTOSAVE; }
