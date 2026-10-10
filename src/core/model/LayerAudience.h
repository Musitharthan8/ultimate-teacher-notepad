/*
 * Ultimate Teacher Notepad
 *
 * Who sees which layer: the teacher, the students, or both. One place for the rules so the teacher canvas,
 * Student View, Hide & Reveal and file loading cannot disagree.
 *
 * Based on Xournal++ GPLv2+
 */

#pragma once

#include <optional>
#include <string_view>

#include "model/Layer.h"  // for Layer, LayerAudience

namespace utn {

/// Name given to new answers layers. Kept so files remain readable in UTN 0.2 builds.
inline constexpr std::string_view ANSWERS_LAYER_NAME = "UTN Reveal";

/// XOPP attribute values for LayerAudience; Everyone is written as no attribute
std::string_view audienceToString(LayerAudience audience);
std::optional<LayerAudience> audienceFromString(std::string_view value);

/**
 * Audience implied by a UTN 0.2 layer name, for files saved before the utnAudience attribute existed:
 * "UTN Reveal…" layers were hidden answers and "UTN Teacher…" layers were teacher-only.
 */
LayerAudience audienceFromLegacyName(std::string_view name);

/// Whether the projected student display may show this layer
bool visibleToStudents(const Layer& layer);

/// Answers that are currently hidden from students
bool isHiddenAnswers(const Layer& layer);

/**
 * Prepare a freshly loaded layer: apply the legacy name rule when the file had no audience attribute, and start
 * every answers layer hidden so opening a lesson never shows answers.
 */
void prepareLoadedLayer(Layer& layer, std::optional<LayerAudience> savedAudience);

}  // namespace utn
