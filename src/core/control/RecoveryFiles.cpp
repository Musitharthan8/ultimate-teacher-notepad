#include "RecoveryFiles.h"

#include <algorithm>
#include <set>
#include <string>
#include <system_error>

#include <glib.h>
#include <glib/gstdio.h>

#include "control/xojfile/LoadHandler.h"
#include "model/Document.h"
#include "util/PathUtil.h"
#include "util/StringUtils.h"

namespace RecoveryFiles {
namespace {
constexpr auto AUTOSAVE_SUFFIX = ".autosave.xopp";
constexpr auto SWAP_SUFFIX = ".swap";
/// A recovery file is never larger than the documents UTN is used with; skip anything absurd instead of loading it
constexpr std::uintmax_t MAX_FILE_SIZE = std::uintmax_t(1) << 30;

bool endsWith(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string lowered(const std::string& s) { return StringUtils::toLowerCase(s); }

std::string identityOf(const fs::path& path) {
    std::error_code ec;
    fs::path canonical = fs::weakly_canonical(path, ec);
    const auto utf8 = (ec ? path : canonical).lexically_normal().u8string();
    std::string key(reinterpret_cast<const char*>(utf8.data()), utf8.size());
#ifdef _WIN32
    key = lowered(key);  // paths are not case sensitive here
#endif
    return key;
}

std::time_t modificationTime(const fs::path& path) {
    GStatBuf st;
    if (g_stat(Util::toGFilename(path).c_str(), &st) != 0) {
        return 0;
    }
    return static_cast<std::time_t>(st.st_mtime);
}

bool isRegularFile(const fs::path& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

struct Collector {
    std::vector<Candidate> found;
    std::set<std::string> seen;

    bool full() const { return found.size() >= MAX_CANDIDATES; }

    void offer(const fs::path& file, Kind kind, const fs::path& original) {
        if (full() || !isRegularFile(file)) return;
        if (!seen.insert(identityOf(file)).second) return;
        std::error_code ec;
        const auto size = fs::file_size(file, ec);
        if (ec || size > MAX_FILE_SIZE) return;
        if (!isLoadable(file)) {
            g_warning("Ignoring unreadable recovery file %s (it is left in place)", char_cast(file.u8string().c_str()));
            return;
        }
        found.push_back({file, kind, original, modificationTime(file)});
    }
};
}  // namespace

fs::path autosavePathFor(const fs::path& document) {
    fs::path p = document;
    p.replace_filename(fs::path(".") += p.filename());
    Util::clearExtensions(p);
    p += AUTOSAVE_SUFFIX;
    return p;
}

bool isLoadable(const fs::path& file) {
    try {
        LoadHandler loader;
        auto doc = loader.loadDocument(file);
        return doc != nullptr && doc->getPageCount() > 0;
    } catch (...) {
        return false;
    }
}

std::vector<Candidate> discover(const Sources& sources) {
    Collector collector;

    if (!sources.emergencyFile.empty()) {
        collector.offer(sources.emergencyFile, Kind::Emergency, {});
    }

    // Autosaves of saved documents sit next to them under a name that is known exactly: no directory scan needed
    size_t documents = 0;
    for (const auto& document: sources.documents) {
        if (document.empty() || ++documents > MAX_DOCUMENTS || collector.full()) break;
        const fs::path autosave = autosavePathFor(document);
        if (!isRegularFile(autosave)) continue;
        // Saved after the autosave was written: the autosave is outdated and would only mislead
        if (isRegularFile(document) && modificationTime(autosave) < modificationTime(document)) continue;
        collector.offer(autosave, Kind::Autosave, document);
    }

    // Autosaves of documents that were never saved (and legacy swap files) live in one folder
    if (!sources.autosaveFolder.empty()) {
        std::error_code ec;
        fs::directory_iterator it(sources.autosaveFolder, ec);
        size_t examined = 0;
        for (; !ec && it != fs::directory_iterator() && examined < MAX_ENTRIES_EXAMINED && !collector.full();
             it.increment(ec), ++examined) {
            const std::string name = lowered(it->path().filename().string());
            if (endsWith(name, AUTOSAVE_SUFFIX)) {
                collector.offer(it->path(), Kind::Autosave, {});
            } else if (endsWith(name, SWAP_SUFFIX)) {
                collector.offer(it->path(), Kind::LegacySwap, {});
            }
        }
    }

    std::stable_sort(collector.found.begin(), collector.found.end(),
                     [](const Candidate& a, const Candidate& b) { return a.modified > b.modified; });
    return std::move(collector.found);
}
}  // namespace RecoveryFiles
