/*
 * Ultimate Teacher Notepad
 *
 * P0 recovery safety: whatever a crashed session left behind must be found again at the next start, validated, listed
 * once, and left untouched until the teacher chooses.
 *
 * Based on Xournal++ GPLv2+
 */
#include <chrono>
#include <fstream>
#include <iterator>
#include <memory>
#include <set>
#include <string>

#include <glib.h>
#include <gtest/gtest.h>

#include "control/RecoveryFiles.h"
#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "model/Document.h"
#include "model/Layer.h"
#include "model/Stroke.h"
#include "model/XojPage.h"

using namespace RecoveryFiles;

namespace {
struct RecoveryPage: XojPage {
    RecoveryPage(): XojPage(400, 300) {}
};

std::unique_ptr<Document> documentWith(int strokes) {
    auto doc = std::make_unique<Document>(nullptr);
    auto page = std::make_shared<RecoveryPage>();
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

/// Writes the way the application does: SaveHandler::saveAtomically
void writeAtomically(const fs::path& file, int strokes) {
    auto doc = documentWith(strokes);
    SaveHandler saver;
    saver.prepareSave(doc.get(), file);
    saver.saveAtomically(file);
}

std::string bytesOf(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
}

void writeBytes(const fs::path& file, const std::string& bytes) {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << bytes;
}

size_t strokeCount(const fs::path& file) {
    auto doc = LoadHandler{}.loadDocument(file);
    return doc->getPage(0)->getSelectedLayer()->getElements().size();
}

bool failingReplace(const fs::path&, const fs::path&, std::string& error) {
    error = "simulated replace failure";
    return false;
}

struct Fixture {
    fs::path folder;
    fs::path documents;
    fs::path autosaves;
    explicit Fixture(const char* name) {
        folder = fs::temp_directory_path() / (std::string("utn-recovery-") + name + "-" + std::to_string(g_get_real_time()));
        fs::remove_all(folder);
        documents = folder / "documents";
        autosaves = folder / "autosaves";
        fs::create_directories(documents);
        fs::create_directories(autosaves);
    }
    ~Fixture() {
        SaveHandler::setReplaceStepForTesting(nullptr);
        std::error_code ignored;
        fs::remove_all(folder, ignored);
    }
    Sources sources() const {
        Sources s;
        s.emergencyFile = folder / "emergencysave.xopp";
        s.autosaveFolder = autosaves;
        return s;
    }
};

std::set<std::string> filesOf(const std::vector<Candidate>& list) {
    std::set<std::string> names;
    for (const auto& c: list) names.insert(c.file.filename().string());
    return names;
}
}  // namespace

TEST(RecoveryFiles, autosaveNamingMatchesTheHiddenNeighbourFile) {
    EXPECT_EQ(autosavePathFor(fs::path("C:/classes/lesson.xopp")).filename().string(), ".lesson.autosave.xopp");
    EXPECT_EQ(autosavePathFor(fs::path("C:/classes/lesson.xopp")).parent_path(), fs::path("C:/classes"));
    EXPECT_EQ(autosavePathFor(fs::path("notes.xoj")).filename().string(), ".notes.autosave.xopp");
}

TEST(RecoveryFiles, autosaveOfASavedDocumentIsFoundAfterARestart) {
    Fixture f("saved");
    const auto doc = f.documents / "lesson.xopp";
    writeAtomically(doc, 1);
    fs::last_write_time(doc, fs::file_time_type::clock::now() - std::chrono::hours(1));
    writeAtomically(autosavePathFor(doc), 3);  // the process dies right after this

    auto s = f.sources();
    s.documents = {doc};
    auto found = discover(s);

    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0].kind, Kind::Autosave);
    EXPECT_EQ(found[0].original, doc);
    EXPECT_EQ(strokeCount(found[0].file), 3u) << "restoring must give the autosaved content";
}

TEST(RecoveryFiles, autosaveOfAnUnsavedDocumentIsFoundInTheCacheFolder) {
    Fixture f("unsaved");
    writeAtomically(f.autosaves / "4242.autosave.xopp", 2);
    auto found = discover(f.sources());
    ASSERT_EQ(found.size(), 1u);
    EXPECT_TRUE(found[0].original.empty());
    EXPECT_EQ(strokeCount(found[0].file), 2u);
}

TEST(RecoveryFiles, anAutosaveThatFailedToReplaceLeavesTheEarlierOneDiscoverable) {
    Fixture f("replace-fails");
    const auto autosave = f.autosaves / "99.autosave.xopp";
    writeAtomically(autosave, 1);
    const auto before = bytesOf(autosave);

    SaveHandler::setReplaceStepForTesting(&failingReplace);
    writeAtomically(autosave, 4);
    SaveHandler::setReplaceStepForTesting(nullptr);

    auto found = discover(f.sources());
    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(bytesOf(found[0].file), before);
    EXPECT_EQ(strokeCount(found[0].file), 1u);
    EXPECT_EQ(filesOf(found), std::set<std::string>{"99.autosave.xopp"}) << "no temporary file may be offered";
}

TEST(RecoveryFiles, corruptedCandidatesAreSkippedKeptOnDiskAndDoNotHideValidOnes) {
    Fixture f("corrupt");
    writeAtomically(f.autosaves / "1.autosave.xopp", 2);
    writeBytes(f.autosaves / "2.autosave.xopp", "this is not a document");
    writeBytes(f.autosaves / "3.autosave.xopp", "");
    const auto whole = bytesOf(f.autosaves / "1.autosave.xopp");
    writeBytes(f.autosaves / "4.autosave.xopp", whole.substr(0, whole.size() / 2));  // cut off mid-file
    writeBytes(f.folder / "emergencysave.xopp", "garbage");

    std::vector<Candidate> found;
    ASSERT_NO_THROW(found = discover(f.sources()));

    EXPECT_EQ(filesOf(found), std::set<std::string>{"1.autosave.xopp"});
    EXPECT_TRUE(fs::exists(f.autosaves / "2.autosave.xopp")) << "a corrupted file is never deleted by discovery";
    EXPECT_TRUE(fs::exists(f.autosaves / "3.autosave.xopp"));
    EXPECT_TRUE(fs::exists(f.autosaves / "4.autosave.xopp"));
    EXPECT_EQ(bytesOf(f.folder / "emergencysave.xopp"), "garbage");
}

TEST(RecoveryFiles, anEmergencyFileIsOfferedWhenItIsValid) {
    Fixture f("emergency");
    writeAtomically(f.folder / "emergencysave.xopp", 1);
    auto found = discover(f.sources());
    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0].kind, Kind::Emergency);
}

TEST(RecoveryFiles, aValidLegacySwapFileIsHonouredAndAnInvalidOneIgnored) {
    Fixture f("swap");
    writeAtomically(f.autosaves / "old.swap", 2);
    writeBytes(f.autosaves / "broken.swap", "not a document");
    writeAtomically(f.documents / "elsewhere.swap", 1);  // outside the cache folder: not looked at

    auto s = f.sources();
    auto found = discover(s);
    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0].file.filename().string(), "old.swap");
    EXPECT_EQ(found[0].kind, Kind::LegacySwap);
    EXPECT_TRUE(fs::exists(f.autosaves / "broken.swap"));
}

TEST(RecoveryFiles, theSameFileReachedTwiceIsListedOnce) {
    Fixture f("dups");
    const auto doc = f.documents / "lesson.xopp";
    writeAtomically(doc, 1);
    fs::last_write_time(doc, fs::file_time_type::clock::now() - std::chrono::hours(1));
    writeAtomically(autosavePathFor(doc), 2);

    auto s = f.sources();
    s.autosaveFolder = f.documents;  // the scan of the folder reaches the same autosave file as the document list
    s.documents = {doc, doc, f.documents / "." / "lesson.xopp"};
    auto found = discover(s);
    EXPECT_EQ(found.size(), 1u);
}

TEST(RecoveryFiles, anAutosaveOlderThanTheSavedDocumentIsStaleAndNotOffered) {
    Fixture f("stale");
    const auto doc = f.documents / "lesson.xopp";
    writeAtomically(autosavePathFor(doc), 2);
    fs::last_write_time(autosavePathFor(doc), fs::file_time_type::clock::now() - std::chrono::hours(2));
    writeAtomically(doc, 1);  // saved normally afterwards

    auto s = f.sources();
    s.documents = {doc};
    EXPECT_TRUE(discover(s).empty());
    EXPECT_TRUE(fs::exists(autosavePathFor(doc))) << "a stale file is left alone, not deleted";
}

TEST(RecoveryFiles, anAutosaveWhoseDocumentWasDeletedIsStillOffered) {
    Fixture f("orphan");
    const auto doc = f.documents / "gone.xopp";
    writeAtomically(autosavePathFor(doc), 2);
    auto s = f.sources();
    s.documents = {doc};
    EXPECT_EQ(discover(s).size(), 1u);
}

TEST(RecoveryFiles, newestCandidateComesFirst) {
    Fixture f("order");
    writeAtomically(f.autosaves / "1.autosave.xopp", 1);
    writeAtomically(f.autosaves / "2.autosave.xopp", 2);
    fs::last_write_time(f.autosaves / "1.autosave.xopp", fs::file_time_type::clock::now() - std::chrono::hours(3));
    auto found = discover(f.sources());
    ASSERT_EQ(found.size(), 2u);
    EXPECT_EQ(found[0].file.filename().string(), "2.autosave.xopp");
}

TEST(RecoveryFiles, discoveryNeverModifiesAnything) {
    Fixture f("readonly");
    const auto doc = f.documents / "lesson.xopp";
    writeAtomically(doc, 1);
    writeAtomically(f.autosaves / "7.autosave.xopp", 3);
    writeAtomically(f.folder / "emergencysave.xopp", 2);
    const auto docBytes = bytesOf(doc);
    const auto autosaveBytes = bytesOf(f.autosaves / "7.autosave.xopp");
    const auto emergencyBytes = bytesOf(f.folder / "emergencysave.xopp");

    auto s = f.sources();
    s.documents = {doc};
    EXPECT_EQ(discover(s).size(), 2u);

    EXPECT_EQ(bytesOf(doc), docBytes) << "declining to restore must leave the teacher's document alone";
    EXPECT_EQ(bytesOf(f.autosaves / "7.autosave.xopp"), autosaveBytes);
    EXPECT_EQ(bytesOf(f.folder / "emergencysave.xopp"), emergencyBytes);
}

TEST(RecoveryFiles, discoveryIsBounded) {
    Fixture f("bounded");
    for (size_t i = 0; i < MAX_ENTRIES_EXAMINED + 50; ++i) {
        writeBytes(f.autosaves / (std::to_string(i) + ".autosave.xopp"), "x");
    }
    writeAtomically(f.autosaves / "zz.autosave.xopp", 1);
    auto s = f.sources();
    for (size_t i = 0; i < MAX_DOCUMENTS + 20; ++i) s.documents.push_back(f.documents / (std::to_string(i) + ".xopp"));
    EXPECT_LE(discover(s).size(), MAX_CANDIDATES);
}
