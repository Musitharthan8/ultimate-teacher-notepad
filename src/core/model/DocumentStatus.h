/*
 * Ultimate Teacher Notepad
 *
 * What the teacher is told about the state of the document. The facts come from the real operations (the undo history,
 * save, export, recovery) and are never guessed from the window title. Free of GTK, so every rule is unit tested.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>
#include <vector>

namespace utn {

enum class ExportStatus { None, Running, Completed, Failed };

/// The facts. Saving and exporting are separate operations: neither changes the other's state.
struct DocumentStatus {
    bool dirty = false;     ///< the undo history holds edits that are not in the file
    bool untitled = false;  ///< the document has never been saved to a path
    bool saving = false;
    std::string saveError;  ///< set by a failed save; only a new attempt or a successful save clears it
    ExportStatus exportStatus = ExportStatus::None;
    std::string exportDetail;  ///< the target of a completed export, or why it failed
    std::string recoveryPath;  ///< a recovery file is waiting to be offered
    std::vector<std::string> attention;
};

/// The one short thing the bar says, most urgent first
enum class StatusState {
    Saved,
    Modified,
    Untitled,
    Saving,
    SaveFailed,
    RecoveryAvailable,
    Exporting,
    Exported,
    ExportFailed,
    NeedsAttention
};

enum class Severity { Ok, Info, Warning, Error };

struct StatusSummary {
    StatusState state = StatusState::Saved;
    Severity severity = Severity::Ok;
    std::string glyph;           ///< a shape as well as a colour, so colour is never the only signal
    std::string label;           ///< the short word beside the glyph
    std::string accessibleName;  ///< the full phrase a screen reader says
    std::string cssClass;        ///< utn-status-ok / -info / -warning / -error
    std::vector<std::string> details;  ///< every active condition, one line each
    bool offersSave = false;     ///< saving now would help (and none is running)
};

/// Decides the primary state and the details from the facts
StatusSummary summarise(const DocumentStatus& status);

/// Holds the facts and is changed only through what really happens
class DocumentStatusModel {
public:
    /// From the undo history and the document path. Never clears a failed save: only a new save does.
    void setDirty(bool dirty, bool untitled);

    /// A different document is now open: whatever happened to the last one (a failed save, an export, an autosave
    /// problem) says nothing about this one. A recovery file waiting on disk is not about any one document, so it stays.
    void resetForNewDocument();

    void beginSave();
    void saveSucceeded();
    void saveFailed(std::string message);

    void beginExport();
    void exportSucceeded(std::string target);
    void exportFailed(std::string message);
    void dismissExport();

    void setRecovery(std::string path);
    void clearRecovery();

    /// Something in the document needs the teacher's eyes (a missing PDF, an image that did not insert)
    void addAttention(std::string message);
    /// Drops one message once its cause is gone (the next autosave worked)
    void removeAttention(const std::string& message);
    void clearAttention();

    const DocumentStatus& get() const { return status; }

private:
    DocumentStatus status;
};

}  // namespace utn
