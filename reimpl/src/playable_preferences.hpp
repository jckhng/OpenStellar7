#pragma once

#include "stellar7/cockpit_state.hpp"
#include "stellar7/indexed_render.hpp"

#include <array>
#include <optional>
#include <string_view>

namespace stellar7 {

// Private proposal: functional playable UI, no source CPU-time certification.
enum class PreferenceControl : std::size_t { detail, mouse, joystick, music, sounds, difficulty, story, exit, count };
enum class PreferenceLayout { title, inGame };
enum class PreferenceDirection { up, down, left, right };
enum class PreferenceChange { decrement, increment };

struct PreferenceWidget {
    ProjectedPoint origin;
    std::uint16_t style;
    std::array<PreferenceControl, 4> neighbors;
};

struct PreferenceRequest {
    PreferenceControl control;
    std::int16_t value;
};

// The caller owns current configuration and validates/applies each request.
// An empty reason admits a real consumer; a nonempty reason keeps its value.
using PreferenceAvailability = std::array<std::string_view, static_cast<std::size_t>(PreferenceControl::count)>;

const std::array<PreferenceWidget, static_cast<std::size_t>(PreferenceControl::count)>& preferenceWidgets(PreferenceLayout layout=PreferenceLayout::title);
PreferenceControl preferenceNeighbor(PreferenceControl current, PreferenceDirection direction, PreferenceLayout layout=PreferenceLayout::title);
ProjectedPoint preferencePointer(PreferenceControl current, PreferenceLayout layout=PreferenceLayout::title);
std::string_view preferenceValue(const CockpitSettings& settings, PreferenceControl control);
PreferenceAvailability fixedPlayablePreferences();
std::optional<PreferenceRequest> preferenceRequest(const CockpitSettings& current,
    PreferenceControl control, PreferenceChange change, const PreferenceAvailability& availability);
std::vector<IndexedPageEffect> preferencesPageEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& titleFont, const BitmapFont& valueFont, const CockpitSettings& current,
    PreferenceControl selected, std::optional<PreferenceControl> pressed,
    std::string_view notice, std::uint16_t page, PreferenceLayout layout=PreferenceLayout::title);

} // namespace stellar7
