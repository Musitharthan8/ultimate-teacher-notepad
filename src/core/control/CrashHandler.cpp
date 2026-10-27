#include "CrashHandler.h"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <glib.h>

#include "control/xojfile/SaveHandler.h"  // for SaveHandler
#include "model/Document.h"
#include "util/PathUtil.h"
#include "util/VersionInfo.h"

#include "filesystem.h"  // for path

// Everything reachable from a signal handler in this file is limited to what the C standard and POSIX allow there:
// lock free atomics, signal(), raise(), write() and _Exit(). No allocation, no mutex, no stdio or iostream, no GLib or
// GTK, no std::filesystem and no document access. See the comment above crashHandler().
static_assert(std::atomic<int>::is_always_lock_free, "signal handlers need lock free atomics");

static std::atomic<Document*> document = nullptr;
static std::atomic<int> alreadyCrashed = 0;
static std::atomic<int> shutdownRequest = 0;
static fs::path emergencyFileOverride;

/// Built before the handlers are installed, so the handler only has to write() it
static std::string crashBanner;

static std::function<void(int)> shutdownCallback;
static guint shutdownWatch = 0;

namespace {
void writeStderr(const char* text, size_t length) {
#ifdef _WIN32
    [[maybe_unused]] auto ignored = _write(2, text, static_cast<unsigned>(length));
#else
    [[maybe_unused]] auto ignored = ::write(2, text, length);
#endif
}

void writeStderr(const char* text) { writeStderr(text, std::strlen(text)); }

void writeStderr(int number) {
    char digits[16];
    size_t n = 0;
    unsigned value = number < 0 ? 0u : static_cast<unsigned>(number);
    do {
        digits[n++] = static_cast<char>('0' + value % 10);
        value /= 10;
    } while (value != 0 && n < sizeof(digits));
    char text[16];
    for (size_t i = 0; i < n; ++i) {
        text[i] = digits[n - 1 - i];
    }
    writeStderr(text, n);
}
}  // namespace

/// SIGINT and SIGTERM: only note the request. The main loop does the work (see watchForShutdownRequests()).
extern "C" void shutdownSignalHandler(int sig) {
    int none = 0;
    if (!shutdownRequest.compare_exchange_strong(none, sig)) {
        // A request is already waiting: the program is not reacting. Let the next signal end it the usual way.
        std::signal(sig, SIG_DFL);
        std::raise(sig);
        return;
    }
    std::signal(sig, shutdownSignalHandler);  // some C runtimes reset the disposition after each delivery
}

void setEmergencyDocument(Document* doc) { document = doc; }

void setEmergencyFileForTesting(fs::path file) { emergencyFileOverride = std::move(file); }

int takeShutdownRequest() { return shutdownRequest.exchange(0); }

void watchForShutdownRequests(std::function<void(int)> callback) {
    unwatchForShutdownRequests();
    shutdownCallback = std::move(callback);
    shutdownWatch = g_timeout_add(
            200,
            +[](gpointer) -> gboolean {
                if (int sig = takeShutdownRequest()) {
                    g_warning("Shutdown requested with signal %i", sig);
                    if (shutdownCallback) {
                        shutdownCallback(sig);
                    }
                }
                return G_SOURCE_CONTINUE;
            },
            nullptr);
}

void unwatchForShutdownRequests() {
    if (shutdownWatch) {
        g_source_remove(shutdownWatch);
        shutdownWatch = 0;
    }
    shutdownCallback = nullptr;
}

/// Normal program flow only (never call from a signal handler): serialises the document and replaces the file at once.
void emergencySave() {
    Document* doc = document;
    if (doc == nullptr) {
        return;
    }

    std::cerr << "Trying to emergency save the current open document..." << std::endl;

    auto const filepath =
            emergencyFileOverride.empty() ? Util::getConfigFile("emergencysave.xopp") : emergencyFileOverride;

    SaveHandler handler;
    doc->lock_shared();
    handler.prepareSave(doc, filepath);
    doc->unlock_shared();
    // An earlier emergency save is replaced in one step: dying again half way through must not destroy it
    handler.saveAtomically(filepath);

    if (!handler.getErrorMessage().empty()) {
        std::cerr << "Error: " << handler.getErrorMessage() << std::endl;
    } else {
        std::cerr << "Successfully saved document to \"" << char_cast(filepath.u8string()) << "\"" << std::endl;
    }
}


/**
 * Fatal signal: the process state can no longer be trusted (a corrupted heap, a lock held by the very code that
 * faulted, ...). Serialising the document here would allocate, lock and touch the file system, which can deadlock or
 * write garbage over a good recovery file, so nothing of the sort is attempted. What survives a crash is the last
 * periodic autosave (and an earlier emergency file); both are found again by the recovery check at the next start.
 *
 * Only a diagnostic made of pre-built text is written, then the signal gets its default action, which also gives the
 * operating system a chance to produce a crash dump.
 */
extern "C" void crashHandler(int sig) {
    const int crash = ++alreadyCrashed;

    if (crash == 1) {
        writeStderr("\n\n\n*************************************************************************\n\n");
        writeStderr("[Crash Handler] Crashed with signal ");
        writeStderr(sig);
        writeStderr("\n");
        writeStderr(crashBanner.data(), crashBanner.size());
        writeStderr("\nUnsaved changes are not written from a crashed process. Restart to recover the last "
                    "autosave.\n");
    }

    std::signal(sig, SIG_DFL);
    std::raise(sig);
    _Exit(crash + 1);  // only reached if the default action did not end the process
}

void installCrashHandlers() {
    crashBanner = xoj::util::getVersionInfo();

    std::signal(SIGTERM, shutdownSignalHandler);
    std::signal(SIGINT, shutdownSignalHandler);
    std::signal(SIGSEGV, crashHandler);
    std::signal(SIGFPE, crashHandler);
    std::signal(SIGILL, crashHandler);
    std::signal(SIGABRT, crashHandler);
#ifdef SIGTRAP
    std::signal(SIGTRAP, crashHandler);
#endif
}
