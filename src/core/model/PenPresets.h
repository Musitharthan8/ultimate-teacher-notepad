/*
 * Ultimate Teacher Notepad
 *
 * Favourite pen presets: a named pen or highlighter setup a teacher applies in one click. Free of GTK so every rule is
 * unit tested. A preset only holds what the tools really support (colour, size, opacity), and the saved form never
 * trusts what it reads.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace utn {

enum class PresetTool { Pen, Highlighter };

struct PenPreset {
    std::string id;    ///< stable across renames; empty until the store gives it one
    std::string name;
    PresetTool tool = PresetTool::Pen;
    uint32_t rgb = 0;  ///< 0xRRGGBB
    int size = 2;      ///< the tool size steps, 0 (very fine) to 4 (very thick)
    int opacity = 255;  ///< 0 to 255; a highlighter's see-through strength, always 255 for a pen
};

/// True when the tool is in exactly the state the preset describes. A preset is shown as selected only through this,
/// so it can never look selected without being applied, nor stay selected after the tool is changed by hand.
bool matchesTool(const PenPreset& preset, PresetTool tool, uint32_t rgb, int size, int opacity);

class PenPresetStore {
public:
    static constexpr const char* RED_MARKING_ID = "red-marking";
    static constexpr const char* BLUE_EXPLANATION_ID = "blue-explanation";
    static constexpr const char* YELLOW_HIGHLIGHTER_ID = "yellow-highlighter";
    static constexpr std::size_t MAX_PRESETS = 24;
    static constexpr std::size_t MAX_NAME_BYTES = 40;

    /// The three built-in presets
    PenPresetStore();

    const std::vector<PenPreset>& all() const { return presets; }
    const PenPreset* find(const std::string& id) const;

    /// @return the new id, or empty when the name is empty or the limit is reached. The name is trimmed and made unique.
    std::string add(PenPreset preset);
    /// Replaces the settings and the name; the id and the position stay
    bool update(const std::string& id, PenPreset preset);
    bool rename(const std::string& id, std::string name);
    bool remove(const std::string& id);
    /// Brings back deleted and edited built-ins; the teacher's own presets stay
    void restoreDefaults();

    /// The saved form: versioned and self-contained, safe to keep in a settings attribute
    std::string serialise() const;
    /// Reads the saved form. Records that do not make sense are dropped, values are clamped, and anything unreadable
    /// gives the built-in presets.
    static PenPresetStore parse(const std::string& saved);

private:
    static std::vector<PenPreset> builtIns();
    std::string uniqueName(const std::string& wanted, const std::string& exceptId) const;

    std::vector<PenPreset> presets;
};

}  // namespace utn
