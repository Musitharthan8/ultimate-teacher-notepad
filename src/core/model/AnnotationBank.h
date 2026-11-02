/*
 * Ultimate Teacher Notepad
 *
 * Annotation Bank: reusable marks a teacher puts onto the current page with one click (a tick, a cross, a star, "See
 * me", "Well done" and the teacher's own short comments). Every entry becomes an ordinary text element, so documents
 * hold nothing new. Free of GTK so every rule is unit tested; the saved form never trusts what it reads.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace utn {

enum class AnnotationStyle {
    Comment,  ///< written at the size of a Feedback comment
    Symbol,   ///< one mark, such as a tick, written larger
};

struct Annotation {
    std::string label;  ///< what its button reads, e.g. "✓ Tick"; also the button's accessible name
    std::string text;   ///< what goes onto the page
    AnnotationStyle style = AnnotationStyle::Comment;
    uint32_t rgb = 0;  ///< 0xRRGGBB
};

class AnnotationBank {
public:
    static constexpr std::size_t MAX_ENTRIES = 24;
    static constexpr std::size_t MAX_LABEL_BYTES = 40;
    static constexpr std::size_t MAX_TEXT_BYTES = 120;

    /// The built-in entries: tick, cross, star, "See me", "Well done"
    AnnotationBank();

    static std::vector<Annotation> defaults();
    const std::vector<Annotation>& all() const { return entries; }

    enum class AddResult { Added, Empty, Full, Duplicate };
    /**
     * Adds a comment of the teacher's own. Line breaks and other control characters become spaces, the text is trimmed
     * and cut to MAX_TEXT_BYTES (never inside a UTF-8 sequence); the label is the text, cut to MAX_LABEL_BYTES.
     * Empty (or not UTF-8) text, a full bank and a comment already in the bank are refused.
     */
    AddResult addComment(const std::string& text, uint32_t rgb);
    bool remove(std::size_t index);
    /// Back to the built-in entries; the teacher's own comments go
    void restoreDefaults();

    /// The saved form: versioned and self-contained, safe to keep in a settings attribute
    std::string serialise() const;
    /**
     * Reads the saved form. Entries that do not make sense (unknown style, bad colour, empty, too long, control
     * characters, not UTF-8, repeated) are dropped and at most MAX_ENTRIES are kept; anything unreadable, or nothing
     * readable, gives the built-in entries. A bank the teacher emptied stays empty.
     */
    static AnnotationBank parse(const std::string& saved);

private:
    std::vector<Annotation> entries;
};

/// A rectangle in page coordinates
struct AnnotationArea {
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;
};

struct AnnotationPoint {
    double x = 0;
    double y = 0;
};

/**
 * Where the top-left corner of an annotation of @p width x @p height goes: centred in the part of the page on screen
 * (@p visible, page coordinates; nothing given means the whole page), then moved as little as needed to keep it on
 * the page. An annotation larger than the page starts at the page's top or left edge.
 */
AnnotationPoint placeAnnotation(const std::optional<AnnotationArea>& visible, double pageWidth, double pageHeight,
                                double width, double height);

}  // namespace utn
