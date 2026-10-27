/*
 * Ultimate Teacher Notepad
 *
 * P0 recovery safety: a signal handler must not serialise a document, take a lock or touch the file system. A graceful
 * stop request is only noted in the handler and acted on from the normal program flow; a fatal signal leaves every
 * existing recovery file alone.
 *
 * Based on Xournal++ GPLv2+
 */
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include <glib.h>
#include <gtest/gtest.h>

#include "control/CrashHandler.h"
#include "control/xojfile/LoadHandler.h"
#include "control/xojfile/SaveHandler.h"
#include "model/Document.h"
#include "model/Layer.h"
#include "model/Stroke.h"
#include "model/XojPage.h"
#include "util/PathUtil.h"

namespace {
struct CrashPage: XojPage {
    CrashPage(): XojPage(400, 300) {}
};

std::unique_ptr<Document> modifiedDocument(int strokes = 1) {
    auto doc = std::make_unique<Document>(nullptr);
    auto page = std::make_shared<CrashPage>();
    for (int i = 0; i < strokes; ++i) {
        auto stroke = std::make_unique<Stroke>();
        stroke->setWidth(4);
        stroke->setColor(Color{10, 20, 30, 255});
        stroke->addPoint(Point(10, 10 + 30 * i));
        stroke->addPoint(Point(90, 40 + 30 * i));
        page->getSelectedLayer()->addElement(std::move(stroke));
    }
    doc->lock();
    doc->addPage(page);
    doc->unlock();
    return doc;
}

std::string bytesOf(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
}

fs::path freshFolder(const char* name) {
    fs::path folder = fs::temp_directory_path() / (std::string("utn-crash-") + name + "-" + std::to_string(g_get_real_time()));
    fs::remove_all(folder);
    fs::create_directories(folder);
    return folder;
}

bool hasStrokes(const fs::path& file) {
    try {
        auto doc = LoadHandler{}.loadDocument(file);
        return doc && doc->getPageCount() == 1 && doc->getPage(0)->getSelectedLayer()->getElements().size() == 1;
    } catch (...) {
        return false;
    }
}

std::string ownExecutable() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH * 4];
    const DWORD n = GetModuleFileNameW(nullptr, buffer, DWORD(std::size(buffer)));
    return fs::path(std::wstring(buffer, n)).string();
#else
    return fs::read_symlink("/proc/self/exe").string();
#endif
}

struct ChildResult {
    bool exitedCleanly = false;
    std::string err;
};

ChildResult runChild(const char* mode, const fs::path& file) {
    gchar** env = g_get_environ();
    env = g_environ_setenv(env, "UTN_CRASH_CHILD", mode, TRUE);
    env = g_environ_setenv(env, "UTN_CRASH_FILE", file.string().c_str(), TRUE);
    const std::string exe = ownExecutable();
    const char* argv[] = {exe.c_str(), "--gtest_filter=CrashChild.run", nullptr};
    gchar* out = nullptr;
    gchar* err = nullptr;
    gint status = 0;
    GError* error = nullptr;
    ChildResult result;
    if (g_spawn_sync(nullptr, const_cast<gchar**>(argv), env, G_SPAWN_DEFAULT, nullptr, nullptr, &out, &err, &status,
                     &error)) {
        result.exitedCleanly = g_spawn_check_wait_status(status, nullptr);
        result.err = err ? err : "";
        if (out) result.err += out;
    } else {
        result.err = error->message;
        g_clear_error(&error);
    }
    g_free(out);
    g_free(err);
    g_strfreev(env);
    return result;
}
}  // namespace

// Runs inside the child processes started by the tests below. Skipped in a normal run.
TEST(CrashChild, run) {
    const char* mode = std::getenv("UTN_CRASH_CHILD");
    const char* file = std::getenv("UTN_CRASH_FILE");
    if (!mode || !file) {
        GTEST_SKIP() << "only meaningful when started by the CrashHandler tests";
    }
    auto doc = modifiedDocument();
    setEmergencyDocument(doc.get());
    setEmergencyFileForTesting(fs::path(file));
    installCrashHandlers();

    if (std::string(mode) == "fatal") {
        std::raise(SIGSEGV);
        FAIL() << "the default action of a fatal signal must end the process";
    }

    // graceful stop request
    std::raise(SIGTERM);
    EXPECT_FALSE(fs::exists(file)) << "the handler itself must not write the document";
    EXPECT_EQ(takeShutdownRequest(), SIGTERM);
    EXPECT_EQ(takeShutdownRequest(), 0) << "a request is delivered once";
    emergencySave();  // what the main loop does once it is told
    EXPECT_TRUE(hasStrokes(file));
    setEmergencyDocument(nullptr);
}

TEST(CrashHandler, gracefulTerminationNotifiesNormalCodeAndSavesThereNotInTheHandler) {
    auto folder = freshFolder("term");
    const auto file = folder / "emergencysave.xopp";
    auto result = runChild("term", file);
    EXPECT_TRUE(result.exitedCleanly) << result.err;
    EXPECT_TRUE(hasStrokes(file));
    fs::remove_all(folder);
}

TEST(CrashHandler, fatalSignalNeverSerialisesTheDocument) {
    auto folder = freshFolder("fatal-new");
    const auto file = folder / "emergencysave.xopp";
    auto result = runChild("fatal", file);
    EXPECT_FALSE(result.exitedCleanly) << "the process must still die from the fatal signal";
    EXPECT_FALSE(fs::exists(file));
    for (const auto& entry: fs::directory_iterator(folder)) {
        ADD_FAILURE() << "unexpected file left behind: " << entry.path();
    }
    EXPECT_NE(result.err.find("[Crash Handler]"), std::string::npos) << "a diagnostic line is still expected";
    fs::remove_all(folder);
}

TEST(CrashHandler, fatalSignalLeavesAnExistingRecoveryFileUntouched) {
    auto folder = freshFolder("fatal-old");
    const auto file = folder / "emergencysave.xopp";
    {
        auto older = modifiedDocument(3);
        SaveHandler saver;
        saver.prepareSave(older.get(), file);
        saver.saveTo(file);
        ASSERT_TRUE(saver.getErrorMessage().empty());
    }
    const auto before = bytesOf(file);
    auto result = runChild("fatal", file);
    EXPECT_FALSE(result.exitedCleanly);
    EXPECT_EQ(bytesOf(file), before);
    fs::remove_all(folder);
}

TEST(CrashHandler, failedEmergencySaveKeepsTheEarlierRecoveryFile) {
    auto folder = freshFolder("keep");
    const auto file = folder / "emergencysave.xopp";
    auto older = modifiedDocument(3);
    {
        SaveHandler saver;
        saver.prepareSave(older.get(), file);
        saver.saveTo(file);
        ASSERT_TRUE(saver.getErrorMessage().empty());
    }
    const auto before = bytesOf(file);

    auto newer = modifiedDocument();
    setEmergencyDocument(newer.get());
    setEmergencyFileForTesting(file);
    SaveHandler::setReplaceStepForTesting([](const fs::path&, const fs::path&, std::string& error) {
        error = "simulated failure";
        return false;
    });
    emergencySave();
    SaveHandler::setReplaceStepForTesting(nullptr);
    setEmergencyDocument(nullptr);
    setEmergencyFileForTesting({});

    EXPECT_EQ(bytesOf(file), before);
    size_t files = 0;
    for ([[maybe_unused]] const auto& entry: fs::directory_iterator(folder)) ++files;
    EXPECT_EQ(files, 1u) << "a failed save must not leave temporary files";
    fs::remove_all(folder);
}
