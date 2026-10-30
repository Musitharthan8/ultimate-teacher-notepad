#include "DocumentStatus.h"

#include <algorithm>
#include <utility>

#include "util/i18n.h"

namespace utn {

void DocumentStatusModel::setDirty(bool dirty, bool untitled) {
    status.dirty = dirty;
    status.untitled = untitled;
}

void DocumentStatusModel::resetForNewDocument() {
    status.saving = false;
    status.saveError.clear();
    status.exportStatus = ExportStatus::None;
    status.exportDetail.clear();
    status.attention.clear();
}

void DocumentStatusModel::beginSave() {
    status.saving = true;
    status.saveError.clear();  // a new attempt replaces the old failure
}

void DocumentStatusModel::saveSucceeded() {
    status.saving = false;
    status.saveError.clear();
}

void DocumentStatusModel::saveFailed(std::string message) {
    status.saving = false;
    status.saveError = message.empty() ? std::string(_("The document could not be saved.")) : std::move(message);
}

void DocumentStatusModel::beginExport() {
    status.exportStatus = ExportStatus::Running;
    status.exportDetail.clear();
}

void DocumentStatusModel::exportSucceeded(std::string target) {
    status.exportStatus = ExportStatus::Completed;
    status.exportDetail = std::move(target);
}

void DocumentStatusModel::exportFailed(std::string message) {
    status.exportStatus = ExportStatus::Failed;
    status.exportDetail = message.empty() ? std::string(_("The export did not finish.")) : std::move(message);
}

void DocumentStatusModel::dismissExport() {
    status.exportStatus = ExportStatus::None;
    status.exportDetail.clear();
}

void DocumentStatusModel::setRecovery(std::string path) { status.recoveryPath = std::move(path); }

void DocumentStatusModel::clearRecovery() { status.recoveryPath.clear(); }

void DocumentStatusModel::addAttention(std::string message) {
    const bool alreadyListed =
            std::find(status.attention.begin(), status.attention.end(), message) != status.attention.end();
    if (message.empty() || alreadyListed) {
        return;
    }
    status.attention.push_back(std::move(message));
}

void DocumentStatusModel::removeAttention(const std::string& message) {
    status.attention.erase(std::remove(status.attention.begin(), status.attention.end(), message),
                           status.attention.end());
}

void DocumentStatusModel::clearAttention() { status.attention.clear(); }

namespace {
struct Presentation {
    Severity severity;
    const char* glyph;
    const char* label;
    const char* accessibleName;
};

Presentation presentationOf(StatusState state) {
    switch (state) {
        case StatusState::Saved:
            return {Severity::Ok, "\xE2\x9C\x93", _("Saved"), _("Document status: everything is saved")};
        case StatusState::Modified:
            return {Severity::Warning, "\xE2\x97\x8F", _("Unsaved changes"), _("Document status: there are unsaved changes")};
        case StatusState::Untitled:
            return {Severity::Warning, "\xE2\x97\x8F", _("Not saved yet"), _("Document status: this document has never been saved")};
        case StatusState::Saving:
            return {Severity::Info, "\xE2\x86\xBB", _("Saving…"), _("Document status: saving in progress")};
        case StatusState::SaveFailed:
            return {Severity::Error, "\xE2\x9A\xA0", _("Save failed"), _("Document status: saving failed, your changes are not saved")};
        case StatusState::RecoveryAvailable:
            return {Severity::Warning, "\xE2\x86\xBA", _("Recovery available"), _("Document status: a recovery file is available")};
        case StatusState::Exporting:
            return {Severity::Info, "\xE2\x86\x97", _("Exporting…"), _("Document status: export in progress")};
        case StatusState::Exported:
            return {Severity::Ok, "\xE2\x9C\x93", _("Exported"), _("Document status: the export finished")};
        case StatusState::ExportFailed:
            return {Severity::Error, "\xE2\x9A\xA0", _("Export failed"), _("Document status: the export failed")};
        case StatusState::NeedsAttention:
            return {Severity::Warning, "!", _("Needs attention"), _("Document status: something in the document needs attention")};
    }
    return {Severity::Ok, "", "", ""};
}

const char* cssClassOf(Severity severity) {
    switch (severity) {
        case Severity::Ok: return "utn-status-ok";
        case Severity::Info: return "utn-status-info";
        case Severity::Warning: return "utn-status-warning";
        case Severity::Error: return "utn-status-error";
    }
    return "utn-status-ok";
}

/// Most urgent first: losing edits, then a missing export, then what is merely worth knowing
StatusState primaryStateOf(const DocumentStatus& status) {
    if (status.saving) {
        return StatusState::Saving;
    }
    if (!status.saveError.empty()) {
        return StatusState::SaveFailed;
    }
    if (status.exportStatus == ExportStatus::Failed) {
        return StatusState::ExportFailed;
    }
    if (status.exportStatus == ExportStatus::Running) {
        return StatusState::Exporting;
    }
    if (status.untitled) {
        return StatusState::Untitled;
    }
    if (status.dirty) {
        return StatusState::Modified;
    }
    if (!status.recoveryPath.empty()) {
        return StatusState::RecoveryAvailable;
    }
    if (!status.attention.empty()) {
        return StatusState::NeedsAttention;
    }
    if (status.exportStatus == ExportStatus::Completed) {
        return StatusState::Exported;
    }
    return StatusState::Saved;
}

/// The save state on its own, for the details: independent of what an export did
const char* saveLineOf(const DocumentStatus& status) {
    if (status.saving) {
        return _("Document: saving…");
    }
    if (!status.saveError.empty()) {
        return _("Document: save failed");
    }
    if (status.untitled) {
        return _("Document: not saved yet");
    }
    if (status.dirty) {
        return _("Document: unsaved changes");
    }
    return _("Document: Saved");
}
}  // namespace

StatusSummary summarise(const DocumentStatus& status) {
    StatusSummary summary;
    summary.state = primaryStateOf(status);
    const Presentation presentation = presentationOf(summary.state);
    summary.severity = presentation.severity;
    summary.glyph = presentation.glyph;
    summary.label = presentation.label;
    summary.accessibleName = presentation.accessibleName;
    summary.cssClass = cssClassOf(presentation.severity);
    summary.offersSave = !status.saving && (status.dirty || status.untitled || !status.saveError.empty());

    summary.details.emplace_back(saveLineOf(status));
    if (!status.saveError.empty()) {
        summary.details.push_back(status.saveError);
    }
    switch (status.exportStatus) {
        case ExportStatus::None:
            break;
        case ExportStatus::Running:
            summary.details.emplace_back(_("Export: in progress"));
            break;
        case ExportStatus::Completed:
            summary.details.push_back(std::string(_("Export: finished")) + " - " + status.exportDetail);
            break;
        case ExportStatus::Failed:
            summary.details.push_back(std::string(_("Export: failed")) + " - " + status.exportDetail);
            break;
    }
    if (!status.recoveryPath.empty()) {
        summary.details.push_back(std::string(_("Recovery file available")) + " - " + status.recoveryPath);
    }
    for (const std::string& message: status.attention) {
        summary.details.push_back(message);
    }
    return summary;
}

}  // namespace utn
