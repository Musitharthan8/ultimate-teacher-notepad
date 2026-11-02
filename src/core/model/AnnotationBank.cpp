#include "AnnotationBank.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include <glib.h>

#include "util/i18n.h"

namespace utn {

namespace {
constexpr std::size_t MAX_SAVED_BYTES = 16384;  // 24 entries of at most 160 bytes each fit many times over
constexpr std::size_t MAX_SAVED_RECORDS = 64;    // more records than this were never written by us
constexpr std::size_t FIELD_COUNT = 4;
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
    return trimmed(text);
}

std::string lowered(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
    return text;
}

bool hasControlCharacter(const std::string& text) {
    return std::any_of(text.begin(), text.end(), [](unsigned char c) { return c < 0x20 || c == 0x7F; });
}

bool validUtf8(const std::string& text) { return g_utf8_validate(text.data(), static_cast<gssize>(text.size()), nullptr); }

const char* styleWord(AnnotationStyle style) { return style == AnnotationStyle::Symbol ? "symbol" : "comment"; }

/// The separators of the saved form never appear inside a field
std::string escaped(const std::string& text) {
    static constexpr char HEX[] = "0123456789ABCDEF";
    std::string out;
    for (const unsigned char c: text) {
        if (c == '%' || c == '|' || c == ';' || c < 0x20 || c == 0x7F) {
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

/// A field as read: unescaped, UTF-8, free of control characters, not blank and within @p limit. Never cut: an entry
/// that does not fit was not written by us.
bool readField(const std::string& field, std::size_t limit, std::string& out) {
    if (!unescaped(field, out) || out.size() > limit || !validUtf8(out) || hasControlCharacter(out)) {
        return false;
    }
    out = trimmed(out);
    return !out.empty();
}

bool parseRecord(const std::string& record, Annotation& entry) {
    const std::vector<std::string> fields = split(record, '|');
    if (fields.size() != FIELD_COUNT) {
        return false;
    }
    Annotation parsed;
    if (fields[0] == "comment") {
        parsed.style = AnnotationStyle::Comment;
    } else if (fields[0] == "symbol") {
        parsed.style = AnnotationStyle::Symbol;
    } else {
        return false;
    }
    if (fields[1].size() != 6 ||
        std::any_of(fields[1].begin(), fields[1].end(), [](char c) { return hexValue(c) < 0; })) {
        return false;
    }
    parsed.rgb = static_cast<uint32_t>(std::stoul(fields[1], nullptr, 16));
    if (!readField(fields[2], AnnotationBank::MAX_LABEL_BYTES, parsed.label) ||
        !readField(fields[3], AnnotationBank::MAX_TEXT_BYTES, parsed.text)) {
        return false;
    }
    entry = std::move(parsed);
    return true;
}

bool sameText(const Annotation& a, const std::string& text) { return lowered(a.text) == lowered(text); }
}  // namespace

std::vector<Annotation> AnnotationBank::defaults() {
    // Colours of the shared ink swatches (UtnWidgets.h), except the star: a dark amber, readable on white paper
    return {
            {std::string("✓ ") + _("Tick"), "✓", AnnotationStyle::Symbol, 0x2E7D32},
            {std::string("✗ ") + _("Cross"), "✗", AnnotationStyle::Symbol, 0xD32F2F},
            {std::string("★ ") + _("Star"), "★", AnnotationStyle::Symbol, 0xB26A00},
            {_("See me"), _("See me"), AnnotationStyle::Comment, 0xD32F2F},
            {_("Well done"), _("Well done"), AnnotationStyle::Comment, 0x2E7D32},
    };
}

AnnotationBank::AnnotationBank(): entries(defaults()) {}

AnnotationBank::AddResult AnnotationBank::addComment(const std::string& text, uint32_t rgb) {
    if (!validUtf8(text)) {
        return AddResult::Empty;
    }
    std::string flat = text;
    std::replace_if(flat.begin(), flat.end(), [](unsigned char c) { return c < 0x20 || c == 0x7F; }, ' ');
    // Runs of spaces left by line breaks and tabs become one
    flat.erase(std::unique(flat.begin(), flat.end(), [](char a, char b) { return a == ' ' && b == ' '; }), flat.end());
    flat = limitedBytes(trimmed(flat), MAX_TEXT_BYTES);
    if (flat.empty()) {
        return AddResult::Empty;
    }
    if (entries.size() >= MAX_ENTRIES) {
        return AddResult::Full;
    }
    if (std::any_of(entries.begin(), entries.end(), [&](const Annotation& a) { return sameText(a, flat); })) {
        return AddResult::Duplicate;
    }
    entries.push_back({limitedBytes(flat, MAX_LABEL_BYTES), flat, AnnotationStyle::Comment, rgb & 0xFFFFFFU});
    return AddResult::Added;
}

bool AnnotationBank::remove(std::size_t index) {
    if (index >= entries.size()) {
        return false;
    }
    entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

void AnnotationBank::restoreDefaults() { entries = defaults(); }

std::string AnnotationBank::serialise() const {
    std::string saved = SAVED_VERSION;
    if (entries.empty()) {
        return saved + ";" + SAVED_EMPTY;
    }
    static constexpr char HEX[] = "0123456789abcdef";
    for (const Annotation& a: entries) {
        std::string colour(6, '0');
        for (int i = 0; i < 6; ++i) {
            colour[5 - i] = HEX[(a.rgb >> (4 * i)) & 0xF];
        }
        saved += std::string(";") + styleWord(a.style) + "|" + colour + "|" + escaped(a.label) + "|" + escaped(a.text);
    }
    return saved;
}

AnnotationBank AnnotationBank::parse(const std::string& saved) {
    AnnotationBank defaultBank;
    if (saved.empty() || saved.size() > MAX_SAVED_BYTES) {
        return defaultBank;
    }
    const std::vector<std::string> records = split(saved, ';');
    if (records.empty() || records[0] != SAVED_VERSION || records.size() > MAX_SAVED_RECORDS + 1) {
        return defaultBank;
    }
    AnnotationBank bank;
    bank.entries.clear();
    if (records.size() == 2 && records[1] == SAVED_EMPTY) {
        return bank;  // the teacher removed every entry: that stays so
    }
    for (std::size_t i = 1; i < records.size() && bank.entries.size() < MAX_ENTRIES; ++i) {
        Annotation entry;
        if (records[i].empty() || !parseRecord(records[i], entry) ||
            std::any_of(bank.entries.begin(), bank.entries.end(),
                        [&](const Annotation& a) { return sameText(a, entry.text); })) {
            continue;
        }
        bank.entries.push_back(std::move(entry));
    }
    return bank.entries.empty() ? defaultBank : bank;
}

AnnotationPoint placeAnnotation(const std::optional<AnnotationArea>& visible, double pageWidth, double pageHeight,
                                double width, double height) {
    const AnnotationArea area = visible ? *visible : AnnotationArea{0, 0, pageWidth, pageHeight};
    const auto along = [](double start, double size, double page, double extent) {
        const double centred = start + size / 2 - extent / 2;
        return std::max(0.0, std::min(centred, page - extent));  // larger than the page: 0, its top or left edge
    };
    return {along(area.x, area.width, pageWidth, width), along(area.y, area.height, pageHeight, height)};
}

}  // namespace utn
