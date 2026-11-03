/*
 * Ultimate Teacher Notepad
 *
 * Teacher Annotation Palette: the few named colours each teaching tool offers. Pen, shapes and text share one ink
 * language (Black, Blue, Red, Green, Purple, Orange, Teal, Pink: one row of eight); the highlighter has see-through classroom colours (Yellow, Pink,
 * Cyan, Green). Anything else is the teacher's Custom colour from the existing chooser.
 *
 * This is a list of choices, not a colour state: the tool colour lives in ToolHandler only. Free of GTK so every rule
 * is unit tested.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "util/i18n.h"

namespace utn::palette {

enum class Kind {
    Ink,          ///< pen, shapes, text, Feedback and Answer Box text, selections
    Highlighter,  ///< highlighter; its opacity stays the tool's own
};

struct Swatch {
    const char* name;  ///< untranslated
    uint32_t rgb;      ///< 0xRRGGBB
};

inline constexpr std::array<Swatch, 8> INK{{
        {N_("Black"), 0x000000U},
        {N_("Blue"), 0x1E5BD8U},
        {N_("Red"), 0xD32F2FU},
        {N_("Green"), 0x2E7D32U},
        {N_("Purple"), 0x6A1B9AU},
        {N_("Orange"), 0xEF6C00U},
        {N_("Teal"), 0x00897BU},
        {N_("Pink"), 0xD63384U},
}};

inline constexpr std::array<Swatch, 4> HIGHLIGHTER{{
        {N_("Yellow"), 0xFFEB3BU},
        {N_("Pink"), 0xFF7EB6U},
        {N_("Cyan"), 0x00D5E8U},
        {N_("Green"), 0x8BE04EU},
}};

/// How many named swatches @p kind offers (Custom not counted)
std::size_t count(Kind kind);

/// The @p index-th swatch of @p kind; @p index must be below count(kind)
const Swatch& swatch(Kind kind, std::size_t index);

/// The palette a tool shows: the highlighter (and the laser highlighter) get the highlighter colours, every other
/// colour tool the ink colours
Kind kindFor(bool highlighter);

/// The swatch that stands for @p argb (0xAARRGGBB or 0xRRGGBB). Only the hue counts, so a highlighter's opacity never
/// hides its swatch. No value: the colour is the teacher's Custom colour.
std::optional<std::size_t> find(Kind kind, uint32_t argb);

/// "#RRGGBB", upper case, for a Custom colour's name
std::string hex(uint32_t argb);

/// The words for a colour: the swatch's untranslated name, or an empty string for a Custom colour
const char* nameOf(Kind kind, uint32_t argb);

/// A new hue with the tool's opacity kept: the hue of @p chosen, the alpha of @p current (both 0xAARRGGBB)
uint32_t withOpacityOf(uint32_t chosen, uint32_t current);

}  // namespace utn::palette
