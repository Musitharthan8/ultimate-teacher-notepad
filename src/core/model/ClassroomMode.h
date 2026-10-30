/*
 * Ultimate Teacher Notepad
 *
 * What the Classroom controls say is on. Spotlight, Curtain, Freeze and Blank each live in their own owner (the tool
 * handler, Student View); this only names them together, so the control surface, its status line and its "Back to
 * normal" button agree. Free of GTK so every rule is unit tested.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <string>
#include <vector>

namespace utn {

enum class ClassroomEffect { Spotlight, Curtain, Freeze, Blank };

/// The raw state of each effect, as its owner reports it
struct ClassroomState {
    bool spotlight = false;
    bool curtain = false;
    bool frozen = false;
    bool blanked = false;
};

struct ClassroomSummary {
    /// The active effects, in a fixed order (Spotlight or Curtain, Freeze, Blank)
    std::vector<ClassroomEffect> effects;
    /// Said in words, never by colour alone: "Classroom is normal" or "On: Spotlight, Freeze"
    std::string label;

    bool anyActive() const { return !effects.empty(); }
};

/// Spotlight and Curtain are one overlay in the tool handler. If both flags arrive, the Curtain (the stronger cover) wins.
ClassroomSummary summariseClassroom(const ClassroomState& state);

/// Whether students are looking at the page. Blank hides it whatever else is on.
bool studentsSeeThePage(const ClassroomSummary& summary);

}  // namespace utn
