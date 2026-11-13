/*
 * UTN: pure helpers that find sentence and paragraph boundaries in the text layout of a PDF page.
 * Header only and free of poppler/GTK so they can be unit-tested.
 *
 * `text` holds one code point per entry and `boxes` the bounding box of each one (same indexing, y grows downwards),
 * which is exactly what poppler_page_get_text() and poppler_page_get_text_layout() return.
 */
#pragma once

#include <algorithm>  // for max, min
#include <cmath>      // for abs
#include <cstddef>    // for size_t
#include <limits>     // for numeric_limits
#include <string>     // for u32string
#include <utility>    // for pair
#include <vector>     // for vector

namespace xoj::pdf::textunits {

struct Box {
    double x1 = 0;
    double y1 = 0;
    double x2 = 0;
    double y2 = 0;
};

/// Inclusive index range [first, last] of characters
using CharRange = std::pair<size_t, size_t>;

inline bool isSpace(char32_t c) {
    return c == U' ' || c == U'\n' || c == U'\r' || c == U'\t' || c == 0xA0 || c == 0x2028 || c == 0x2029;
}

inline bool isCloser(char32_t c) {
    return c == U')' || c == U']' || c == U'}' || c == U'"' || c == U'\'' || c == 0x201D || c == 0x2019 || c == 0xBB;
}

inline bool isLower(char32_t c) { return (c >= U'a' && c <= U'z') || (c >= 0xDF && c <= 0xFF && c != 0xF7); }

/// Abbreviation (lower case, no dot) that does not end a sentence when followed by a period
inline bool isAbbreviation(const std::u32string& t, size_t dot) {
    size_t b = dot;
    while (b > 0 && !isSpace(t[b - 1]) && t[b - 1] != U'(' && t[b - 1] != U'"') {
        b--;
    }
    std::u32string w;
    for (size_t i = b; i < dot; i++) {
        w.push_back(t[i] >= U'A' && t[i] <= U'Z' ? static_cast<char32_t>(t[i] + 32) : t[i]);
    }
    if (w.size() == 1) {  // initial such as "J." (but not the word "a" or "I" at the end of a sentence)
        return t[b] >= U'A' && t[b] <= U'Z' && t[b] != U'I' && t[b] != U'A';
    }
    static const char32_t* const words[] = {U"mr",  U"mrs", U"ms",  U"dr",  U"prof", U"vs", U"etc",
                                            U"e.g", U"i.e", U"fig", U"no",  U"st",   U"cf", U"approx"};
    for (const char32_t* a: words) {
        if (w == a) {
            return true;
        }
    }
    return false;
}

/// True when the character at i ends a sentence
inline bool endsSentence(const std::u32string& t, size_t i) {
    const char32_t c = t[i];
    if (c != U'.' && c != U'!' && c != U'?' && c != 0x2026) {
        return false;
    }
    size_t j = i + 1;
    while (j < t.size() && (t[j] == c || isCloser(t[j]))) {  // "?!", "...", closing quote or bracket
        j++;
    }
    if (j >= t.size()) {
        return true;
    }
    if (!isSpace(t[j])) {  // "3.5", "example.com"
        return false;
    }
    size_t k = j;
    while (k < t.size() && isSpace(t[k])) {
        k++;
    }
    if (k >= t.size()) {
        return true;
    }
    if (c == U'.') {
        if (isLower(t[k]) || isAbbreviation(t, i)) {
            return false;
        }
    }
    return true;
}

/// The sentence containing character idx. Empty text returns {0, 0}.
inline CharRange sentenceRange(const std::u32string& t, size_t idx) {
    if (t.empty()) {
        return {0, 0};
    }
    idx = std::min(idx, t.size() - 1);
    while (idx > 0 && isSpace(t[idx])) {
        idx--;
    }
    size_t first = 0;
    for (size_t i = idx; i-- > 0;) {
        if (endsSentence(t, i)) {
            first = i + 1;
            break;
        }
    }
    while (first < idx && isSpace(t[first])) {
        first++;
    }
    size_t last = t.size() - 1;
    for (size_t i = idx; i < t.size(); i++) {
        if (endsSentence(t, i)) {
            last = i;
            while (last + 1 < t.size() && (t[last + 1] == t[i] || isCloser(t[last + 1]))) {
                last++;
            }
            break;
        }
    }
    while (last > first && isSpace(t[last])) {
        last--;
    }
    return {first, last};
}

/// Index of the character under (x, y), or the nearest one
inline size_t charIndexAt(const std::vector<Box>& boxes, double x, double y) {
    size_t best = 0;
    double bestD = std::numeric_limits<double>::max();
    for (size_t i = 0; i < boxes.size(); i++) {
        const Box& b = boxes[i];
        const double dx = x < b.x1 ? b.x1 - x : (x > b.x2 ? x - b.x2 : 0.0);
        const double dy = y < b.y1 ? b.y1 - y : (y > b.y2 ? y - b.y2 : 0.0);
        const double d = dx * dx + dy * dy;
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    return best;
}

/**
 * The paragraph containing character idx. Lines are runs of characters at the same height. A paragraph ends
 * between two lines when the next line starts higher up (new column), is separated by more than 0.6 line heights
 * of blank space, or is indented by more than one line height relative to the line above.
 * text and boxes must have the same size, otherwise {0, 0} is returned.
 */
inline CharRange paragraphRange(const std::u32string& t, const std::vector<Box>& boxes, size_t idx) {
    if (t.empty() || t.size() != boxes.size()) {
        return {0, 0};
    }
    struct Line {
        size_t first, last;
        double top, bottom, left;
    };
    std::vector<Line> lines;
    size_t idxLine = 0;
    idx = std::min(idx, t.size() - 1);
    for (size_t i = 0; i < t.size(); i++) {
        const Box& b = boxes[i];
        if (isSpace(t[i]) || b.y2 - b.y1 <= 0) {
            continue;
        }
        const double cy = (b.y1 + b.y2) / 2;
        if (lines.empty() || std::abs(cy - (lines.back().top + lines.back().bottom) / 2) >
                                     0.5 * (lines.back().bottom - lines.back().top)) {
            lines.push_back({i, i, b.y1, b.y2, b.x1});
        } else {
            Line& l = lines.back();
            l.last = i;
            l.top = std::min(l.top, b.y1);
            l.bottom = std::max(l.bottom, b.y2);
            l.left = std::min(l.left, b.x1);
        }
        if (i <= idx) {
            idxLine = lines.size() - 1;
        }
    }
    if (lines.empty()) {
        return {idx, idx};
    }
    const auto breaksBefore = [&](size_t n) {  // is there a paragraph break between lines n-1 and n
        const Line& a = lines[n - 1];
        const Line& b = lines[n];
        const double h = std::max(a.bottom - a.top, b.bottom - b.top);
        return (b.top + b.bottom) / 2 < (a.top + a.bottom) / 2 || b.top - a.bottom > 0.6 * h || b.left - a.left > h;
    };
    size_t from = idxLine;
    while (from > 0 && !breaksBefore(from)) {
        from--;
    }
    size_t to = idxLine;
    while (to + 1 < lines.size() && !breaksBefore(to + 1)) {
        to++;
    }
    return {lines[from].first, lines[to].last};
}

}  // namespace xoj::pdf::textunits
