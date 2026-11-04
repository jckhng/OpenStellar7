#pragma once

#include "stellar7/scene_script.hpp"
#include "stellar7/palette_clock.hpp"

namespace stellar7 {

inline constexpr std::size_t endingScenePageCount = 43;
inline constexpr std::int16_t initialSceneVoiceSegment = -3333; //Initial DS1BDA=F2FB.

struct EndingSceneAssets {
    SceneResource scene;
    std::vector<std::uint8_t> sampledVoice;
};

EndingSceneAssets decodeEndingSceneAssets(const std::vector<std::uint8_t>& scene,
    const std::vector<std::uint8_t>& voice);
void validateEndingSceneAssets(const EndingSceneAssets& assets);
PaletteWorkState selectScenePalette(PaletteWorkState state, const std::vector<Rgb>& palette);
void applySceneFrontWipe(IndexedPages& pages, std::uint16_t frontPage,
    RadarPageLayout layout = RadarPageLayout::compatibility);

} // namespace stellar7
