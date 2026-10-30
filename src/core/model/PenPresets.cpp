#include "PenPresets.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include "util/i18n.h"

namespace utn {

namespace {
constexpr int MAX_SIZE_STEP = 4;
constexpr int MAX_ALPHA = 255;
constexpr std::size_t MAX_SAVED_BYTES = 16384;  // far more than 27 presets need; anything bigger is not ours
constexpr std::size_t MAX_SAVED_RECORDS = 64;
constexpr std::size_t MAX_ID_BYTES = 40;
constexpr std::size_t FIELD_COUNT = 6;
constexpr const char* SAVED_VERSION = "v1";
constexpr const char* SAVED_EMPTY = "none";

std::string trimmed(const std::string& text) {
    const char* blanks = " \t\r\n";
    const std::size_t first = text.find_first_not_of(blanks);
    if (first == std::string::npos) {
        return {};
    }
    return text.substr(first, text.find_last_not_of(blanks) - first + 1);
}

/// Cuts at a byte limit without splitting a UTF-8 sequence
std::string limitedBytes(std::string text, std::size_t limit) {
    if (text.size() <= limit) {
        return text;
    }
    std::size_t cut = limit;
    while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) {
        --cut;
    }
    text.resize(cut);
    return text;
}

std::string lowered(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
    return text;
}

void normalise(PenPreset& preset) {
    preset.rgb &= 0xFFFFFFu;
    preset.size = std::clamp(preset.size, 0, MAX_SIZE_STEP);
    preset.opacity = std::clamp(preset.opacity, 0, MAX_ALPHA);
}

const char* toolWord(PresetTool tool) { return tool == PresetTool::Pen ? "pen" : "highlighter"; }

/// The separators of the saved form never appear inside a field
std::string escaped(const std::string& text) {
    static constexpr char HEX[] = "0123456789ABCDEF";
    std::string out;
    for (const unsigned char c: text) {
        if (c == '%' || c == '|' || c == ';' || c == '\n' || c == '\r') {
            out += '%';
            out += HEX[c >> 4];
            out += HEX[c & 0xF];
        } else {
            out += static_cast<char>(c);
        }
    }
    return out;
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool unescaped(const std::string& text, std::string& out) {
    out.clear();
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '%') {
            out += text[i];
            continue;
        }
        if (i + 2 >= text.size()) {  // a '%' needs two hex digits after it
            return false;
        }
        const int high = hexValue(text[i + 1]);
        const int low = hexValue(text[i + 2]);
        if (high < 0 || low < 0) {
            return false;
        }
        out += static_cast<char>(high * 16 + low);
        i += 2;
    }
    return true;
}

std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = text.find(separator, start);
        parts.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos) {
            return parts;
        }
        start = end + 1;
    }
}

bool parseInt(const std::string& text, int& value) {
    if (text.empty() || text.size() > 6) {
        return false;
    }
    int result = 0;
    for (const char c: text) {
        if (c < '0' || c > '9') {
            return false;
        }
        result = result * 10 + (c - '0');
    }
    value = result;
    return true;
}

bool validId(const std::string& id) {
    return !id.empty() && id.size() <= MAX_ID_BYTES &&
           std::all_of(id.begin(), id.end(), [](unsigned char c) { return std::isalnum(c) || c == '-' || c == '_'; });
}

bool parseRecord(const std::string& record, PenPreset& preset) {
    const std::vector<std::string> fields = split(record, '|');
    if (fields.size() != FIELD_COUNT) {
        return false;
    }
    std::string id;
    std::string name;
    if (!unescaped(fields[0], id) || !unescaped(fields[1], name) || !validId(id)) {
        return false;
    }
    name = limitedBytes(trimmed(name), PenPresetStore::MAX_NAME_BYTES);
    if (name.empty()) {
        return false;
    }
    PenPreset parsed;
    parsed.id = id;
    parsed.name = name;
    if (fields[2] == "pen") {
        parsed.tool = PresetTool::Pen;
    } else if (fields[2] == "highlighter") {
        parsed.tool = PresetTool::Highlighter;
    } else {
        return false;
    }
    if (fields[3].size() != 6 || std::any_of(fields[3].begin(), fields[3].end(), [](char c) { return hexValue(c) < 0; })) {
        return false;
    }
    parsed.rgb = static_cast<uint32_t>(std::stoul(fields[3], nullptr, 16));
    if (!parseInt(fields[4], parsed.size) || !parseInt(fields[5], parsed.opacity)) {
        return false;
    }
    normalise(parsed);
    preset = std::move(parsed);
    return true;
}
}  // namespace

bool matchesTool(const PenPreset& preset, PresetTool tool, uint32_t rgb, int size, int opacity) {
    return preset.tool == tool && (preset.rgb & 0xFFFFFFu) == (rgb & 0xFFFFFFu) && preset.size == size &&
           preset.opacity == opacity;
}

std::vector<PenPreset> PenPresetStore::builtIns() {
    std::vector<PenPreset> defaults(3);
    defaults[0] = {RED_MARKING_ID, _("Red marking pen"), PresetTool::Pen, 0xD93025, 2, 255};
    defaults[1] = {BLUE_EXPLANATION_ID, _("Blue explanation pen"), PresetTool::Pen, 0x1A5FD0, 2, 255};
    defaults[2] = {YELLOW_HIGHLIGHTER_ID, _("Yellow highlighter"), PresetTool::Highlighter, 0xFFE94A, 3, 128};
    return defaults;
}

PenPresetStore::PenPresetStore(): presets(builtIns()) {}

const PenPreset* PenPresetStore::find(const std::string& id) const {
    const auto it = std::find_if(presets.begin(), presets.end(), [&](const PenPreset& p) { return p.id == id; });
    return it == presets.end() ? nullptr : &*it;
}

std::string PenPresetStore::uniqueName(const std::string& wanted, const std::string& exceptId) const {
    const std::string base = limitedBytes(trimmed(wanted), MAX_NAME_BYTES - 4);
    const auto taken = [&](const std::string& candidate) {
        return std::any_of(presets.begin(), presets.end(), [&](const PenPreset& p) {
            return p.id != exceptId && lowered(p.name) == lowered(candidate);
        });
    };
    if (!taken(base)) {
        return limitedBytes(trimmed(wanted), MAX_NAME_BYTES);
    }
    for (int n = 2;; ++n) {
        const std::string candidate = base + " " + std::to_string(n);
        if (!taken(candidate)) {
            return candidate;
        }
    }
}

std::string PenPresetStore::add(PenPreset preset) {
    if (trimmed(preset.name).empty() || presets.size() >= MAX_PRESETS) {
        return {};
    }
    normalise(preset);
    int n = 1;
    while (find("custom-" + std::to_string(n))) {
        ++n;
    }
    preset.id = "custom-" + std::to_string(n);
    preset.name = uniqueName(preset.name, preset.id);
    presets.push_back(preset);
    return preset.id;
}

bool PenPresetStore::update(const std::string& id, PenPreset preset) {
    const auto it = std::find_if(presets.begin(), presets.end(), [&](const PenPreset& p) { return p.id == id; });
    if (it == presets.end() || trimmed(preset.name).empty()) {
        return false;
    }
    normalise(preset);
    preset.id = id;
    preset.name = uniqueName(preset.name, id);
    *it = std::move(preset);
    return true;
}

bool PenPresetStore::rename(const std::string& id, std::string name) {
    const auto it = std::find_if(presets.begin(), presets.end(), [&](const PenPreset& p) { return p.id == id; });
    if (it == presets.end() || trimmed(name).empty()) {
        return false;
    }
    it->name = uniqueName(name, id);
    return true;
}

bool PenPresetStore::remove(const std::string& id) {
    const auto it = std::remove_if(presets.begin(), presets.end(), [&](const PenPreset& p) { return p.id == id; });
    if (it == presets.end()) {
        return false;
    }
    presets.erase(it, presets.end());
    return true;
}

void PenPresetStore::restoreDefaults() {
    std::size_t position = 0;
    for (PenPreset& builtIn: builtIns()) {
        const auto existing =
                std::find_if(presets.begin(), presets.end(), [&](const PenPreset& p) { return p.id == builtIn.id; });
        if (existing != presets.end()) {
            *existing = std::move(builtIn);
        } else {
            presets.insert(presets.begin() + static_cast<std::ptrdiff_t>(std::min(position, presets.size())),
                           std::move(builtIn));
        }
        ++position;
    }
}

std::string PenPresetStore::serialise() const {
    std::string saved = SAVED_VERSION;
    if (presets.empty()) {
        return saved + ";" + SAVED_EMPTY;
    }
    static constexpr char HEX[] = "0123456789abcdef";
    for (const PenPreset& p: presets) {
        std::string colour(6, '0');
        for (int i = 0; i < 6; ++i) {
            colour[5 - i] = HEX[(p.rgb >> (4 * i)) & 0xF];
        }
        saved += ";" + escaped(p.id) + "|" + escaped(p.name) + "|" + toolWord(p.tool) + "|" + colour + "|" +
                 std::to_string(p.size) + "|" + std::to_string(p.opacity);
    }
    return saved;
}

PenPresetStore PenPresetStore::parse(const std::string& saved) {
    PenPresetStore defaults;
    if (saved.empty() || saved.size() > MAX_SAVED_BYTES) {
        return defaults;
    }
    const std::vector<std::string> records = split(saved, ';');
    if (records.empty() || records[0] != SAVED_VERSION || records.size() > MAX_SAVED_RECORDS + 1) {
        return defaults;
    }
    PenPresetStore store;
    store.presets.clear();
    if (records.size() == 2 && records[1] == SAVED_EMPTY) {
        return store;  // the teacher deleted every preset: that stays deleted
    }
    for (std::size_t i = 1; i < records.size(); ++i) {
        PenPreset preset;
        if (records[i].empty() || !parseRecord(records[i], preset) || store.find(preset.id)) {
            continue;
        }
        store.presets.push_back(std::move(preset));
    }
    return store.presets.empty() ? defaults : store;
}

}  // namespace utn
