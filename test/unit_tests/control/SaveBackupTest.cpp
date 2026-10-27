/*
 * Ultimate Teacher Notepad
 *
 * P0 recovery safety: a failed Save must never cost the teacher the previous document or an older backup.
 *
 * Based on Xournal++ GPLv2+
 */
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include <glib.h>
#include <gtest/gtest.h>

#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveBackup.h"
#include "control/xojfile/SaveHandler.h"
#include "model/Document.h"
#include "model/Layer.h"
#include "model/Stroke.h"
#include "model/XojPage.h"

namespace {
struct BackupPage: XojPage {
    BackupPage(): XojPage(400, 300) {}
};

std::unique_ptr<Document> documentWith(int strokes) {
    auto doc = std::make_unique<Document>(nullptr);
    auto page = std::make_shared<BackupPage>();
    for (int i = 0; i < strokes; ++i) {
        auto stroke = std::make_unique<Stroke>();
        stroke->setWidth(3);
        stroke->setColor(Color{1, 2, 3, 255});
        stroke->addPoint(Point(10, 10 + 20 * i));
        stroke->addPoint(Point(80, 30 + 20 * i));
        page->getSelectedLayer()->addElement(std::move(stroke));
    }
    doc->lock();
    doc->addPage(page);
    doc->unlock();
    return doc;
}

void writeDocument(const fs::path& file, int strokes) {
    auto doc = documentWith(strokes);
    SaveHandler saver;
    saver.prepareSave(doc.get(), file);
    saver.saveTo(file);
    ASSERT_TRUE(saver.getErrorMessage().empty()) << saver.getErrorMessage();
}

std::string bytesOf(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
}

size_t strokeCount(const fs::path& file) {
    auto doc = LoadHandler{}.loadDocument(file);
    return doc->getPage(0)->getSelectedLayer()->getElements().size();
}

std::set<std::string> namesIn(const fs::path& folder) {
    std::set<std::string> names;
    for (const auto& entry: fs::directory_iterator(folder)) names.insert(entry.path().filename().string());
    return names;
}

fs::path freshFolder(const char* name) {
    fs::path folder = fs::temp_directory_path() / (std::string("utn-backup-") + name + "-" + std::to_string(g_get_real_time()));
    fs::remove_all(folder);
    fs::create_directories(folder);
    return folder;
}

struct Fixture {
    fs::path folder;
    fs::path target;
    fs::path backup;
    explicit Fixture(const char* name): folder(freshFolder(name)), target(folder / "lesson.xopp") {
        backup = fs::path(target) += "~";
    }
    ~Fixture() {
        SaveHandler::setReplaceStepForTesting(nullptr);
        std::error_code ignored;
        fs::remove_all(folder, ignored);
    }
    SaveBackup::Result save(int strokes, bool createBackup = true) {
        auto doc = documentWith(strokes);
        SaveHandler handler;
        handler.prepareSave(doc.get(), target);
        return SaveBackup::saveWithBackup(handler, target, createBackup);
    }
};

bool failingReplace(const fs::path&, const fs::path&, std::string& error) {
    error = "simulated replace failure";
    return false;
}

/// A replacement that dies half way: the destination is gone and nothing took its place
bool interruptedReplace(const fs::path&, const fs::path& to, std::string& error) {
    std::error_code ignored;
    fs::remove(to, ignored);
    error = "simulated interruption";
    return false;
}
}  // namespace

TEST(SaveBackup, successfulSaveReplacesTheFileAndLeavesNoBackupBehind) {
    Fixture f("ok");
    writeDocument(f.target, 1);
    auto result = f.save(2);
    EXPECT_TRUE(result.error.empty()) << result.error;
    EXPECT_EQ(strokeCount(f.target), 2u);
    EXPECT_EQ(namesIn(f.folder), std::set<std::string>{"lesson.xopp"});
}

TEST(SaveBackup, firstSaveOfANewFileNeedsNoBackup) {
    Fixture f("new");
    auto result = f.save(1);
    EXPECT_TRUE(result.error.empty()) << result.error;
    EXPECT_EQ(namesIn(f.folder), std::set<std::string>{"lesson.xopp"});
}

TEST(SaveBackup, failedReplacementKeepsThePreviousFileAndMakesItTheBackup) {
    Fixture f("fail");
    writeDocument(f.target, 1);
    const auto before = bytesOf(f.target);
    SaveHandler::setReplaceStepForTesting(&failingReplace);
    auto result = f.save(2);
    EXPECT_FALSE(result.error.empty());
    EXPECT_TRUE(result.saveAttempted);
    EXPECT_NE(result.error.find("simulated replace failure"), std::string::npos) << result.error;
    EXPECT_EQ(bytesOf(f.target), before);
    EXPECT_EQ(bytesOf(f.backup), before);
    EXPECT_EQ(strokeCount(f.target), 1u);
    EXPECT_EQ(strokeCount(f.backup), 1u);
    EXPECT_EQ(namesIn(f.folder), (std::set<std::string>{"lesson.xopp", "lesson.xopp~"}));
}

// The defect found in review: the older backup was overwritten with the current file before the new save succeeded.
TEST(SaveBackup, failedReplacementNeverOverwritesAnOlderValidBackup) {
    Fixture f("older");
    writeDocument(f.backup, 1);  // an older version, kept from an earlier failed save
    writeDocument(f.target, 2);  // the current version
    const auto olderBytes = bytesOf(f.backup);
    const auto currentBytes = bytesOf(f.target);
    ASSERT_NE(olderBytes, currentBytes);

    SaveHandler::setReplaceStepForTesting(&failingReplace);
    auto result = f.save(3);

    EXPECT_FALSE(result.error.empty());
    EXPECT_EQ(bytesOf(f.backup), olderBytes) << "the older backup was destroyed by a save that failed";
    EXPECT_EQ(bytesOf(f.target), currentBytes);
    EXPECT_EQ(strokeCount(f.backup), 1u);
    EXPECT_EQ(strokeCount(f.target), 2u);
    EXPECT_EQ(namesIn(f.folder), (std::set<std::string>{"lesson.xopp", "lesson.xopp~"}))
            << "no private candidate may be left once the destination is known to be intact";
}

TEST(SaveBackup, failureFollowedByRetryEndsWithTheNewFileAndNoBackup) {
    Fixture f("retry");
    writeDocument(f.target, 1);
    SaveHandler::setReplaceStepForTesting(&failingReplace);
    EXPECT_FALSE(f.save(2).error.empty());
    SaveHandler::setReplaceStepForTesting(nullptr);

    auto result = f.save(3);
    EXPECT_TRUE(result.error.empty()) << result.error;
    EXPECT_EQ(strokeCount(f.target), 3u);
    EXPECT_EQ(namesIn(f.folder), std::set<std::string>{"lesson.xopp"});
}

TEST(SaveBackup, interruptedReplacementKeepsThePreviousDocumentAsTheBackup) {
    Fixture f("interrupted");
    writeDocument(f.target, 2);
    const auto before = bytesOf(f.target);
    SaveHandler::setReplaceStepForTesting(&interruptedReplace);
    auto result = f.save(3);
    EXPECT_FALSE(result.error.empty());
    EXPECT_FALSE(fs::exists(f.target));
    EXPECT_EQ(bytesOf(f.backup), before);
    EXPECT_EQ(strokeCount(f.backup), 2u);
}

TEST(SaveBackup, interruptedReplacementKeepsBothTheOlderBackupAndThePreviousDocument) {
    Fixture f("interrupted-both");
    writeDocument(f.backup, 1);
    writeDocument(f.target, 2);
    const auto olderBytes = bytesOf(f.backup);
    const auto currentBytes = bytesOf(f.target);
    SaveHandler::setReplaceStepForTesting(&interruptedReplace);
    auto result = f.save(3);
    EXPECT_FALSE(result.error.empty());
    EXPECT_EQ(bytesOf(f.backup), olderBytes);

    // The destination is gone, so the private copy is the only one of the current version: kept and named
    size_t recovered = 0;
    for (const auto& entry: fs::directory_iterator(f.folder)) {
        const auto name = entry.path().filename().string();
        if (name == "lesson.xopp~") continue;
        EXPECT_NE(name.find(".utn-bak"), std::string::npos) << name;
        EXPECT_EQ(bytesOf(entry.path()), currentBytes);
        EXPECT_NE(result.error.find(name), std::string::npos) << "the error must say where the copy is: " << result.error;
        ++recovered;
    }
    EXPECT_EQ(recovered, 1u);
}

#ifdef _WIN32
// A destination that cannot even be read for the backup: the save must stop before anything is replaced or removed.
TEST(SaveBackup, lockedDestinationStopsBeforeTouchingAnythingAndKeepsTheOlderBackup) {
    Fixture f("locked");
    writeDocument(f.backup, 1);
    writeDocument(f.target, 2);
    const auto olderBytes = bytesOf(f.backup);
    const auto currentBytes = bytesOf(f.target);

    HANDLE lock = CreateFileW(f.target.wstring().c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(lock, INVALID_HANDLE_VALUE);
    auto result = f.save(3);
    CloseHandle(lock);

    EXPECT_FALSE(result.error.empty());
    EXPECT_FALSE(result.saveAttempted) << "no backup could be made, so nothing may have been written";
    EXPECT_EQ(bytesOf(f.backup), olderBytes);
    EXPECT_EQ(bytesOf(f.target), currentBytes);
    EXPECT_EQ(namesIn(f.folder), (std::set<std::string>{"lesson.xopp", "lesson.xopp~"}));
}
#endif

TEST(SaveBackup, saveWithoutBackupStillReportsFailureAndKeepsTheFile) {
    Fixture f("nobackup");
    writeDocument(f.target, 1);
    const auto before = bytesOf(f.target);
    SaveHandler::setReplaceStepForTesting(&failingReplace);
    auto result = f.save(2, /*createBackup=*/false);
    EXPECT_FALSE(result.error.empty());
    EXPECT_EQ(bytesOf(f.target), before);
    EXPECT_EQ(namesIn(f.folder), std::set<std::string>{"lesson.xopp"});
}
