#include "ClassroomMode.h"

#include <algorithm>

#include "util/i18n.h"

namespace utn {

namespace {
const char* effectName(ClassroomEffect effect) {
    switch (effect) {
        case ClassroomEffect::Spotlight:
            return _("Spotlight");
        case ClassroomEffect::Curtain:
            return _("Curtain");
        case ClassroomEffect::Freeze:
            return _("Freeze");
        case ClassroomEffect::Blank:
            return _("Blank");
    }
    return "";
}
}  // namespace

ClassroomSummary summariseClassroom(const ClassroomState& state) {
    ClassroomSummary summary;
    if (state.curtain) {
        summary.effects.push_back(ClassroomEffect::Curtain);
    } else if (state.spotlight) {
        summary.effects.push_back(ClassroomEffect::Spotlight);
    }
    if (state.frozen) {
        summary.effects.push_back(ClassroomEffect::Freeze);
    }
    if (state.blanked) {
        summary.effects.push_back(ClassroomEffect::Blank);
    }

    if (summary.effects.empty()) {
        summary.label = _("Classroom is normal");
        return summary;
    }
    summary.label = _("On: ");
    for (size_t i = 0; i < summary.effects.size(); ++i) {
        summary.label += (i == 0 ? "" : ", ");
        summary.label += effectName(summary.effects[i]);
    }
    return summary;
}

bool studentsSeeThePage(const ClassroomSummary& summary) {
    return std::find(summary.effects.begin(), summary.effects.end(), ClassroomEffect::Blank) == summary.effects.end();
}

}  // namespace utn
