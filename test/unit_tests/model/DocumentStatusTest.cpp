/*
 * Ultimate Teacher Notepad
 *
 * The document status model: what the teacher is told about the state of the document. It is fed by the real save,
 * export and recovery operations and decides one short primary state, with the full detail behind it.
 *
 * Based on Xournal++ GPLv2+
 */

#include <algorithm>
#include <string>

#include <gtest/gtest.h>

#include "model/DocumentStatus.h"

using namespace utn;

namespace {
bool detailMentions(const StatusSummary& summary, const std::string& text) {
    return std::any_of(summary.details.begin(), summary.details.end(),
                       [&](const std::string& line) { return line.find(text) != std::string::npos; });
}
}  // namespace

TEST(DocumentStatus, ACleanSavedDocumentIsSaved) {
    DocumentStatusModel model;
    model.setDirty(false, false);
    const auto summary = summarise(model.get());
    EXPECT_EQ(summary.state, StatusState::Saved);
    EXPECT_EQ(summary.severity, Severity::Ok);
    EXPECT_FALSE(summary.offersSave);
}

TEST(DocumentStatus, EditsMakeItUnsaved) {
    DocumentStatusModel model;
    model.setDirty(true, false);
    const auto summary = summarise(model.get());
    EXPECT_EQ(summary.state, StatusState::Modified);
    EXPECT_EQ(summary.severity, Severity::Warning);
    EXPECT_TRUE(summary.offersSave);
}

TEST(DocumentStatus, ADocumentThatWasNeverSavedSaysSo) {
    DocumentStatusModel model;
    model.setDirty(false, true);
    EXPECT_EQ(summarise(model.get()).state, StatusState::Untitled);
    model.setDirty(true, true);
    EXPECT_EQ(summarise(model.get()).state, StatusState::Untitled) << "never saved wins over modified: it is the bigger risk";
}

TEST(DocumentStatus, SavingIsShownWhileItRuns) {
    DocumentStatusModel model;
    model.setDirty(true, false);
    model.beginSave();
    const auto summary = summarise(model.get());
    EXPECT_EQ(summary.state, StatusState::Saving);
    EXPECT_EQ(summary.severity, Severity::Info);
    EXPECT_FALSE(summary.offersSave) << "a save is already running";
}

TEST(DocumentStatus, AFailedSaveNeverLooksSaved) {
    DocumentStatusModel model;
    model.setDirty(true, false);
    model.beginSave();
    model.saveFailed("disk full");
    auto summary = summarise(model.get());
    EXPECT_EQ(summary.state, StatusState::SaveFailed);
    EXPECT_EQ(summary.severity, Severity::Error);
    EXPECT_TRUE(summary.offersSave) << "the way forward is to try again";
    EXPECT_TRUE(detailMentions(summary, "disk full")) << "and the teacher is told why";

    // Whatever the edit history says afterwards, the failure stays until a save really succeeds
    model.setDirty(false, false);
    EXPECT_EQ(summarise(model.get()).state, StatusState::SaveFailed);
    model.setDirty(true, false);
    EXPECT_EQ(summarise(model.get()).state, StatusState::SaveFailed);
}

TEST(DocumentStatus, ANewSaveAttemptReplacesTheFailureAndASuccessClearsIt) {
    DocumentStatusModel model;
    model.setDirty(true, false);
    model.beginSave();
    model.saveFailed("locked by another program");
    model.beginSave();
    EXPECT_EQ(summarise(model.get()).state, StatusState::Saving);
    model.saveSucceeded();
    model.setDirty(false, false);
    EXPECT_EQ(summarise(model.get()).state, StatusState::Saved);
    EXPECT_FALSE(detailMentions(summarise(model.get()), "locked"));
}

TEST(DocumentStatus, AFailureWithoutAMessageStillSaysItFailed) {
    DocumentStatusModel model;
    model.setDirty(true, false);
    model.saveFailed("");
    const auto summary = summarise(model.get());
    EXPECT_EQ(summary.state, StatusState::SaveFailed);
    EXPECT_FALSE(summary.details.empty());
}

TEST(DocumentStatus, ExportAndSaveAreSeparateOperations) {
    DocumentStatusModel model;
    model.setDirty(false, false);
    model.beginExport();
    EXPECT_EQ(summarise(model.get()).state, StatusState::Exporting);
    model.exportSucceeded("lesson.pdf");
    auto summary = summarise(model.get());
    EXPECT_EQ(summary.state, StatusState::Exported);
    EXPECT_TRUE(detailMentions(summary, "lesson.pdf"));
    EXPECT_TRUE(detailMentions(summary, "Saved")) << "the save state is still reported beside the export";

    // An export never makes an unsaved document look saved, nor the reverse
    model.setDirty(true, false);
    summary = summarise(model.get());
    EXPECT_EQ(summary.state, StatusState::Modified);
    EXPECT_TRUE(detailMentions(summary, "lesson.pdf"));
}

TEST(DocumentStatus, AFailedExportIsAnErrorAndLeavesTheSaveStateAlone) {
    DocumentStatusModel model;
    model.setDirty(false, false);
    model.beginExport();
    model.exportFailed("the folder is read-only");
    auto summary = summarise(model.get());
    EXPECT_EQ(summary.state, StatusState::ExportFailed);
    EXPECT_EQ(summary.severity, Severity::Error);
    EXPECT_TRUE(detailMentions(summary, "read-only"));
    EXPECT_TRUE(detailMentions(summary, "Saved")) << "the document itself is saved";
    model.dismissExport();
    EXPECT_EQ(summarise(model.get()).state, StatusState::Saved);
}

TEST(DocumentStatus, ASaveFailureOutranksAnExportFailure) {
    DocumentStatusModel model;
    model.setDirty(true, false);
    model.exportFailed("x");
    model.saveFailed("y");
    EXPECT_EQ(summarise(model.get()).state, StatusState::SaveFailed) << "losing edits is worse than a missing PDF";
    EXPECT_TRUE(detailMentions(summarise(model.get()), "x")) << "but nothing is hidden";
}

TEST(DocumentStatus, RecoveryAvailableIsShownUntilHandled) {
    DocumentStatusModel model;
    model.setDirty(false, false);
    model.setRecovery("lesson.autosave.xopp");
    auto summary = summarise(model.get());
    EXPECT_EQ(summary.state, StatusState::RecoveryAvailable);
    EXPECT_EQ(summary.severity, Severity::Warning);
    EXPECT_TRUE(detailMentions(summary, "lesson.autosave.xopp"));
    model.clearRecovery();
    EXPECT_EQ(summarise(model.get()).state, StatusState::Saved);
}

TEST(DocumentStatus, ContentNeedingAttentionIsNeverSilentlyDropped) {
    DocumentStatusModel model;
    model.setDirty(false, false);
    model.addAttention("The PDF this worksheet was made from is missing");
    model.addAttention("The PDF this worksheet was made from is missing");
    model.addAttention("");
    auto summary = summarise(model.get());
    EXPECT_EQ(summary.state, StatusState::NeedsAttention);
    EXPECT_EQ(std::count_if(summary.details.begin(), summary.details.end(),
                            [](const std::string& line) { return line.find("PDF") != std::string::npos; }),
              1)
            << "the same message is listed once, and an empty one is ignored";
    model.clearAttention();
    EXPECT_EQ(summarise(model.get()).state, StatusState::Saved);
}

TEST(DocumentStatus, OneAttentionMessageCanBeResolvedWithoutHidingTheOthers) {
    DocumentStatusModel model;
    model.addAttention("autosave failed");
    model.addAttention("pdf missing");
    model.removeAttention("autosave failed");
    model.removeAttention("never added");
    const auto summary = summarise(model.get());
    EXPECT_EQ(summary.state, StatusState::NeedsAttention);
    EXPECT_FALSE(detailMentions(summary, "autosave"));
    EXPECT_TRUE(detailMentions(summary, "pdf missing"));
    model.removeAttention("pdf missing");
    EXPECT_EQ(summarise(model.get()).state, StatusState::Saved);
}

TEST(DocumentStatus, ANewDocumentDoesNotInheritTheLastOnesProblemsButKeepsTheRecoveryFile) {
    DocumentStatusModel model;
    model.setDirty(true, false);
    model.saveFailed("disk full");
    model.exportFailed("read-only");
    model.addAttention("autosave failed");
    model.setRecovery("lesson.autosave.xopp");
    model.resetForNewDocument();
    model.setDirty(false, false);
    const auto summary = summarise(model.get());
    EXPECT_EQ(summary.state, StatusState::RecoveryAvailable) << "the file on disk is still waiting";
    EXPECT_FALSE(detailMentions(summary, "disk full"));
    EXPECT_FALSE(detailMentions(summary, "read-only"));
    EXPECT_FALSE(detailMentions(summary, "autosave failed"));
    model.clearRecovery();
    EXPECT_EQ(summarise(model.get()).state, StatusState::Saved);
}

TEST(DocumentStatus, UnsavedChangesOutrankRecoveryAndAttention) {
    DocumentStatusModel model;
    model.setDirty(true, false);
    model.setRecovery("a");
    model.addAttention("b");
    const auto summary = summarise(model.get());
    EXPECT_EQ(summary.state, StatusState::Modified);
    EXPECT_TRUE(detailMentions(summary, "a"));
    EXPECT_TRUE(detailMentions(summary, "b"));
}

TEST(DocumentStatus, EveryStateHasAShortLabelAGlyphAndAnAccessibleName) {
    // Colour is never the only signal: each state is a word and a glyph, and a screen reader hears the sentence
    for (StatusState state: {StatusState::Saved, StatusState::Modified, StatusState::Untitled, StatusState::Saving,
                             StatusState::SaveFailed, StatusState::RecoveryAvailable, StatusState::Exporting,
                             StatusState::Exported, StatusState::ExportFailed, StatusState::NeedsAttention}) {
        DocumentStatus status;
        switch (state) {
            case StatusState::Modified: status.dirty = true; break;
            case StatusState::Untitled: status.untitled = true; break;
            case StatusState::Saving: status.saving = true; break;
            case StatusState::SaveFailed: status.saveError = "e"; break;
            case StatusState::RecoveryAvailable: status.recoveryPath = "r"; break;
            case StatusState::Exporting: status.exportStatus = ExportStatus::Running; break;
            case StatusState::Exported: status.exportStatus = ExportStatus::Completed; break;
            case StatusState::ExportFailed: status.exportStatus = ExportStatus::Failed; break;
            case StatusState::NeedsAttention: status.attention.push_back("a"); break;
            case StatusState::Saved: break;
        }
        const auto summary = summarise(status);
        EXPECT_EQ(summary.state, state);
        EXPECT_FALSE(summary.label.empty());
        EXPECT_FALSE(summary.glyph.empty());
        EXPECT_FALSE(summary.accessibleName.empty());
        EXPECT_NE(summary.accessibleName, summary.label) << "the accessible name is a full phrase, not the badge word";
        EXPECT_FALSE(summary.cssClass.empty());
    }
}
