#include "stellar7/ending_scene.hpp"
#include "stellar7/sampled_voice.hpp"

#include <stdexcept>

namespace stellar7 {

PaletteWorkState selectScenePalette(PaletteWorkState state, const std::vector<Rgb>& palette) {
    if (palette.size() != vgaPaletteColors) {
        throw std::invalid_argument("scene selection requires a full VGA palette");
    }
    std::size_t index = 0;
    for (const auto color : palette) {
        for (const auto expanded : {color.red, color.green, color.blue}) {
            const auto value = static_cast<std::uint8_t>(expanded >> 2);
            if (static_cast<std::uint8_t>((value << 2) | (value >> 4)) != expanded) {
                throw std::invalid_argument("scene palette is not a lossless six-bit DAC expansion");
            }
            state.source[index++] = value;
        }
    }
    //0736 uploads the raw selected bank, then copies it to output and source.
    //It does not reapply or replace the last resident1B38 request.
    return completedFullPaletteUpload(state, {0, vgaPaletteColors, 0, vgaComponentMaximum});
}

void applySceneFrontWipe(IndexedPages& pages, const std::uint16_t frontPage, const RadarPageLayout layout) {
    constexpr std::int16_t width = 320, height = 200, steps = width / 2;
    const auto secondPage=layout==RadarPageLayout::recoveredStartup ? 0xa800 : 0xa400;
    if ((layout!=RadarPageLayout::compatibility && layout!=RadarPageLayout::recoveredStartup)
        || (frontPage != 0xa000 && frontPage != secondPage)) {
        throw std::invalid_argument("scene front wipe requires a physical display page");
    }
    //9DBC's nested fills remain ordered even after the first covers the display.
    for (std::int16_t inset = 0; inset < steps; ++inset) {
        const auto top = static_cast<std::int16_t>((height / 2) * inset / steps);
        for (std::int16_t y = top; y < height - top; ++y) {
            for (std::int16_t x = inset; x < width - inset; ++x) {
                pages.write(frontPage, {x, y}, 0);
            }
        }
    }
}

void validateEndingSceneAssets(const EndingSceneAssets& assets) {
    constexpr std::size_t paletteColors = 256;
    constexpr std::size_t bitmapFrames = 16;
    constexpr std::size_t screenPixels = 320 * 200;
    const auto& scene = assets.scene;
    if (scene.program.pages.size() != endingScenePageCount || scene.assets.size() != 3
        || !std::holds_alternative<std::vector<Rgb>>(scene.assets[0])
        || !std::holds_alternative<std::vector<IndexedBitmap>>(scene.assets[1])
        || !std::holds_alternative<std::vector<std::uint8_t>>(scene.assets[2])
        || std::get<0>(scene.assets[0]).size() != paletteColors
        || std::get<1>(scene.assets[1]).size() != bitmapFrames
        || std::get<2>(scene.assets[2]).size() != screenPixels) {
        throw std::invalid_argument("ending requires the SCENED page/palette/bitmap/screen resource domain");
    }
    const RadarPages pages{0xa800, 0xa400, 0xa000, 0xa400, 0xa000};
    ScenePageInterpreter staged(scene, pages, 0, 0);
    const auto first = staged.execute(0);
    if (!first.present || first.delayIrqCounts != 0) {
        throw std::invalid_argument("SCENED entry requires its zero-delay first page");
    }
    decodeSampledVoice(assets.sampledVoice);
}

EndingSceneAssets decodeEndingSceneAssets(const std::vector<std::uint8_t>& scene,
    const std::vector<std::uint8_t>& voice) {
    constexpr std::size_t endingVoiceGroup = 2; // B50E(3), one-based SSM selection.
    const auto sequences = decodeSoundSequenceGroup(voice, endingVoiceGroup);
    if (sequences.size() != 1) {
        throw std::invalid_argument("ending requires the third VOICE SSM's single segment");
    }
    EndingSceneAssets assets{decodeSceneResource(scene), sequences.front()};
    validateEndingSceneAssets(assets);
    return assets;
}

} // namespace stellar7
