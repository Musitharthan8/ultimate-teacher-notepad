#include "SaveBackup.h"

#include <cstring>
#include <fstream>
#include <system_error>

#include <glib.h>

#include "control/xojfile/SaveHandler.h"
#include "util/PathUtil.h"
#include "util/i18n.h"

namespace {
bool sameBytes(const fs::path& a, const fs::path& b) {
    std::error_code ec;
    const auto sizeA = fs::file_size(a, ec);
    if (ec) return false;
    const auto sizeB = fs::file_size(b, ec);
    if (ec || sizeA != sizeB) return false;
    std::ifstream inA(a, std::ios::binary);
    std::ifstream inB(b, std::ios::binary);
    if (!inA || !inB) return false;
    char bufA[16384];
    char bufB[16384];
    while (inA && inB) {
        inA.read(bufA, sizeof(bufA));
        inB.read(bufB, sizeof(bufB));
        if (inA.gcount() != inB.gcount() || std::memcmp(bufA, bufB, size_t(inA.gcount())) != 0) return false;
    }
    return inA.eof() && inB.eof();
}

void removeQuietly(const fs::path& file) {
    std::error_code ec;
    fs::remove(file, ec);
    if (ec) {
        g_warning("Could not remove %s: %s", char_cast(file.u8string().c_str()), ec.message().c_str());
    }
}

/// Makes a private copy of the destination. Returns an empty path (and the reason) when that is not possible.
fs::path stageCandidate(const fs::path& target, std::string& error) {
    std::error_code ec;
    if (!fs::exists(target, ec)) {
        if (ec) error = ec.message();
        return {};  // nothing to protect
    }
    fs::path candidate = Util::createExclusiveSiblingFile(target, ".utn-bak", error);
    if (candidate.empty()) {
        return {};
    }
    fs::copy_file(target, candidate, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        error = ec.message();
        removeQuietly(candidate);
        return {};
    }
    return candidate;
}
}  // namespace

namespace SaveBackup {
Result saveWithBackup(SaveHandler& handler, const fs::path& target, bool createBackup, ProgressListener* listener) {
    Result result;
    const fs::path backup = fs::path(target) += "~";

    fs::path candidate;
    if (createBackup) {
        std::string error;
        candidate = stageCandidate(target, error);
        if (candidate.empty() && !error.empty()) {
            // Nothing was written or removed: the destination and any backup are exactly as they were
            result.saveAttempted = false;
            result.error = FS(_F("Save file error, can't backup: {1}") % error);
            return result;
        }
    }

    handler.saveAtomically(target, listener);
    const bool saved = handler.getErrorMessage().empty();
    if (!saved) {
        result.error = FS(_F("Save file error: {1}") % handler.getErrorMessage());
    }

    if (candidate.empty()) {
        return result;
    }

    if (saved) {
        // The new file is in place: the copy of the previous version and an outdated backup are no longer needed
        removeQuietly(candidate);
        removeQuietly(backup);
        return result;
    }

    std::error_code ec;
    if (!fs::exists(backup, ec)) {
        fs::rename(candidate, backup, ec);
        if (ec) {
            g_warning("Could not keep the previous version as %s: %s", char_cast(backup.u8string().c_str()),
                      ec.message().c_str());
            result.error += "\n" + FS(_F("The previous version was kept as \"{1}\"") % candidate.u8string());
        }
    } else if (sameBytes(target, candidate)) {
        removeQuietly(candidate);  // an older backup stays; the destination itself is intact
    } else {
        // The destination changed or vanished and an older backup occupies the usual name: keep both
        result.error += "\n" + FS(_F("The previous version was kept as \"{1}\"") % candidate.u8string());
    }
    return result;
}
}  // namespace SaveBackup
