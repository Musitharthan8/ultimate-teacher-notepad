/*
 * Ultimate Teacher Notepad
 *
 * Based on Xournal++ GPLv2+
 */

#include "LayerAudience.h"

#include <string>
#include <vector>

namespace utn {

auto audienceToString(LayerAudience audience) -> std::string_view {
    switch (audience) {
        case LayerAudience::TeacherOnly:
            return "teacher";
        case LayerAudience::Answers:
            return "answers";
        case LayerAudience::Everyone:
            break;
    }
    return "everyone";
}

auto audienceFromString(std::string_view value) -> std::optional<LayerAudience> {
    if (value == "teacher") {
        return LayerAudience::TeacherOnly;
    }
    if (value == "answers") {
        return LayerAudience::Answers;
    }
    if (value == "everyone") {
        return LayerAudience::Everyone;
    }
    return std::nullopt;  // unknown value from a newer version: treated as missing
}

auto audienceFromLegacyName(std::string_view name) -> LayerAudience {
    if (name.substr(0, ANSWERS_LAYER_NAME.size()) == ANSWERS_LAYER_NAME) {
        return LayerAudience::Answers;
    }
    constexpr std::string_view teacherPrefix = "UTN Teacher";
    if (name.substr(0, teacherPrefix.size()) == teacherPrefix) {
        return LayerAudience::TeacherOnly;
    }
    return LayerAudience::Everyone;
}

auto visibleToStudents(const Layer& layer) -> bool {
    switch (layer.getAudience()) {
        case LayerAudience::TeacherOnly:
            return false;
        case LayerAudience::Answers:  // shown only while revealed
        case LayerAudience::Everyone:
            return layer.isVisible();
    }
    return false;
}

auto isHiddenAnswers(const Layer& layer) -> bool {
    return layer.getAudience() == LayerAudience::Answers && !layer.isVisible();
}

auto writableLayerIdAfterHide(const std::vector<Layer*>& layers) -> std::optional<Layer::Index> {
    for (size_t i = layers.size(); i > 0; --i) {
        if (layers[i - 1]->getAudience() != LayerAudience::Answers) {
            return i;  // layer IDs count from 1; 0 would select the bottom layer, which may be hidden answers
        }
    }
    return std::nullopt;
}

auto visibleLayerIdForInk(const std::vector<Layer*>& layers, Layer::Index selected) -> std::optional<Layer::Index> {
    if (layers.empty()) {
        return std::nullopt;
    }
    const Layer::Index current = selected == 0 ? 1 : selected;  // as XojPage::getSelectedLayer reads it
    if (current <= layers.size() && layers[current - 1]->isVisible()) {
        return std::nullopt;
    }
    for (size_t i = layers.size(); i > 0; --i) {
        if (layers[i - 1]->isVisible() && layers[i - 1]->getAudience() != LayerAudience::Answers) {
            return i;
        }
    }
    return std::nullopt;
}

void prepareLoadedLayer(Layer& layer, std::optional<LayerAudience> savedAudience) {
    const LayerAudience audience =
            savedAudience ? *savedAudience : (layer.hasName() ? audienceFromLegacyName(layer.getName()) : LayerAudience::Everyone);
    layer.setAudience(audience);
    if (audience == LayerAudience::Answers) {
        layer.setVisible(false);
    }
}

}  // namespace utn
