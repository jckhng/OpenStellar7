#include "playable_decoy.hpp"

#include <algorithm>
#include <stdexcept>

namespace stellar7 {
void copyPlayableDecoy(Level1Entity& proxy, const Level1Entity& player) noexcept {
    const auto identity = proxy.allocationIndex;
    proxy = player;
    proxy.allocationIndex = identity;
}

void refreshPlayableDecoyPosition(Level1Entity& proxy, const Level1Entity& player) noexcept {
    proxy.position = player.position;
}

void recenterPlayableDecoy(Level1Entity& proxy, const FixedPosition player) noexcept {
    proxy.position.x = static_cast<std::int32_t>(static_cast<std::uint32_t>(proxy.position.x)
        - static_cast<std::uint32_t>(player.x));
    proxy.position.y = static_cast<std::int32_t>(static_cast<std::uint32_t>(proxy.position.y)
        - static_cast<std::uint32_t>(player.y));
    normalizeWorldPosition(proxy.position);
}

const Level1Entity& playableBehaviorTarget(const Level1Entity& player,
    const Level1Entity& proxy, const std::int16_t decoyTimer) noexcept {
    return decoyTimer != 0 ? proxy : player;
}

void refreshPlayableTarget(Level1Entity& actor, const Level1Entity& target) noexcept {
    actor.presenterDistance = approximatePlanarDistance(target.position, actor.position);
    actor.targetHeading = presenterTargetHeading(actor.position, target.position);
}

PlayableDecoyPaletteBytes playableDecoyPaletteBytes() noexcept {
    PlayableDecoyPaletteBytes result{};
    //1026/113A: four16-color copies,24*3 ramp,8*8 ramp,two16-color copies.
    for (std::size_t color = 0; color < playableDecoyPaletteColors; ++color) {
        const auto red = color < 64 ? (color % 16) * 4
            : color < 88 ? (color - 64) * 3
            : color < 96 ? (color - 88) * 8 : (color % 16) * 4;
        result[color * vgaPaletteComponents] = static_cast<std::uint8_t>(red);
    }
    return result;
}

std::vector<IndexedPageEffect> playableDecoyStripEffects(const PlayableDecoyStrip mode,
    const std::vector<IndexedBitmap>& artGroup3, const std::uint16_t front,
    const std::uint16_t back, const std::uint16_t cache) {
    constexpr ProjectedPoint position{128, 1};
    constexpr std::uint16_t stripWidth = 64;
    constexpr std::uint16_t stripHeight = 23;
    const auto ordinaryPage = [](const auto page) { return page == 0xa000 || page == 0xa800; };
    if (!ordinaryPage(front) || !ordinaryPage(back) || front == back || cache != 0xa400
        || (mode != PlayableDecoyStrip::restore && mode != PlayableDecoyStrip::visible)) {
        throw std::invalid_argument("decoy strip requires retained startup page roles");
    }
    std::vector<IndexedPageEffect> result;
    if (mode == PlayableDecoyStrip::visible) {
        if (artGroup3.size() <= 4) { throw std::invalid_argument("missing source decoy strip"); }
        const auto& bitmap = artGroup3[4];
        if (bitmap.width != stripWidth || bitmap.height != stripHeight
            || bitmap.pixels.size() != std::size_t{stripWidth} * stripHeight) {
            throw std::invalid_argument("unsupported source decoy strip dimensions");
        }
        for (std::uint16_t row = 0; row < stripHeight; ++row) {
            for (std::uint16_t column = 0; column < stripWidth; ++column) {
                result.emplace_back(RadarPixelWrite{{static_cast<std::int16_t>(position.x + column),
                    static_cast<std::int16_t>(position.y + row)},
                    bitmap.pixels[std::size_t{row} * stripWidth + column], back});
            }
        }
    } else {
        result.emplace_back(RadarPageCopy{front, back, position, stripWidth, stripHeight});
    }
    result.emplace_back(RadarPageCopy{back, cache, position, stripWidth, stripHeight});
    return result;
}
} //namespace stellar7
