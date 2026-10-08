/*
 * Xournal++
 *
 * The main application
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2
 */

#include "control/CrashHandler.h"  // for installCrashHandlers
#include "control/XournalMain.h"   // for run

#ifdef __APPLE__
#include "osx/setup-env.h"
#endif

#ifdef _WIN32
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

#include <glib.h>

#include "util/PathUtil.h"
#include "filesystem.h"

#include "win32/console.h"
#endif

#ifdef _WIN32
namespace {
void setupUtnRuntime() {
    const auto root = Util::getExePath().parent_path();
    const auto fonts = root / "etc/fonts/fonts.conf";
    if (fs::exists(fonts)) {
        const auto path = Util::toGFilename(fonts);
        g_setenv("FONTCONFIG_FILE", path.c_str(), true);
    }
    const auto schemas = root / "share/glib-2.0/schemas";
    if (fs::exists(schemas)) {
        const auto path = Util::toGFilename(schemas);
        g_setenv("GSETTINGS_SCHEMA_DIR", path.c_str(), true);
    }
    std::ifstream input(root / "share/utn-loaders.cache.in");
    if (!input) {
        return;  // Developer builds use the MSYS2 runtime.
    }
    std::string cache{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    std::string path = Util::toGFilename(root).c_str();
    std::replace(path.begin(), path.end(), '\\', '/');
    constexpr auto token = "@UTN_ROOT@";
    for (size_t pos = 0; (pos = cache.find(token, pos)) != std::string::npos; pos += path.size()) {
        cache.replace(pos, std::char_traits<char>::length(token), path);
    }
    const auto folder = Util::getConfigFolder().parent_path() / "utn-runtime";
    std::error_code error;
    fs::create_directories(folder, error);
    if (error) {
        g_warning("Could not prepare UTN image loaders: %s", error.message().c_str());
        return;
    }
    const auto file = folder / "loaders.cache";
    const auto filename = Util::toGFilename(file);
    GError* writeError = nullptr;
    if (g_file_set_contents(filename.c_str(), cache.data(), cache.size(), &writeError)) {
        g_setenv("GDK_PIXBUF_MODULE_FILE", filename.c_str(), true);
    } else {
        g_warning("Could not write UTN image loaders: %s", writeError->message);
        g_clear_error(&writeError);
    }
}
}  // namespace
#endif

auto main(int argc, char* argv[]) -> int {
#ifdef _WIN32
    // Attach to the console here. Otherwise, gspawn-win32-helper will create annoying console popups,
    // e.g. in the LaTeX tool after every change in the formula
    attachConsole();
#endif

    // init crash handler
    installCrashHandlers();

#ifdef DEV_CALL_LOG
    Log::initlog();
#endif

#ifdef _WIN32
    // Switch to the FontConfig backend for Pango - See #3371
    _putenv_s("PANGOCAIRO_BACKEND", "fc");
    setupUtnRuntime();
#endif

#ifdef __APPLE__
    // Setup the environment variables, in particular so that the pixbuf loaders are found
    setupEnvironment();
#endif

    // Use this two line to test the crash handler...
    // int* crash = nullptr;
    // *crash = 0;

    int result = XournalMain::run(argc, argv);

#ifdef DEV_CALL_LOG
    Log::closelog();
#endif

    return result;
}
