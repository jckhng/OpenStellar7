#include "stellar7/completion_beam.hpp"
#include "stellar7/raster_line_work.hpp"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr Viewport mainViewport{24, 294, 24, 145};
constexpr std::int32_t projectionScale = 128;
constexpr std::int32_t nearDepth = 128;
constexpr std::int32_t maximumRadius = 256;
constexpr std::int32_t maximumRelativeCoordinate = 1000000;
constexpr int firstLine = -5;
constexpr int beamLengthBase = 77;
constexpr std::uint8_t firstBeamTimer = 70;
constexpr std::uint8_t lastBeamTimer = 30;

std::int32_t divide(const std::int32_t coordinate, std::uint32_t denominator) {
    const auto numerator = static_cast<std::int64_t>(coordinate) * projectionScale;
    auto magnitude = static_cast<std::uint64_t>(numerator < 0 ? -numerator : numerator);
    while (denominator > 0xffffU) {
        denominator >>= 1U;
        magnitude >>= 1U;
    }
    const auto quotient = static_cast<std::int32_t>(magnitude / denominator);
    return numerator < 0 ? -quotient : quotient;
}
}

void generateCompletionBeam(CompletionBeamPresentation& beam, StellarRng& rng,
    const CompletionBeamView context) {
    if (context == CompletionBeamView::radar || !beam.projected) {
        return;
    }
    if (beam.timer < lastBeamTimer || beam.timer > firstBeamTimer) {
        throw std::invalid_argument("unsupported completion beam timer");
    }
    for (auto& color : beam.colors) {
        color = static_cast<std::uint8_t>((rng.next() & 15U) + 128U);
    }
}

CompletionBeamPresentation presentCompletionBeam(const Level1Entity& actor,
    const ViewTransform& view, StellarRng& rng, const CompletionBeamView context) {
    CompletionBeamPresentation result{};
    if (context == CompletionBeamView::radar) {
        return result;
    }
    if (actor.type != 7 || (actor.flags != 8 && actor.flags != 12)
        || actor.stateBytes4dTo52[5] != 3 || actor.behaviorTimer < lastBeamTimer
        || actor.behaviorTimer > firstBeamTimer || actor.collisionRadius >= 32768
        || !(view.viewport == mainViewport) || view.perspectiveScale != projectionScale) {
        throw std::invalid_argument("unsupported completion beam main presentation");
    }
    const auto relativeX = static_cast<std::int64_t>(actor.position.x) - view.origin.x;
    const auto relativeY = static_cast<std::int64_t>(actor.position.y) - view.origin.y;
    const auto relativeZ = static_cast<std::int64_t>(actor.position.z) - view.origin.z;
    if (std::abs(relativeX) > maximumRelativeCoordinate || std::abs(relativeY) > maximumRelativeCoordinate
        || std::abs(relativeZ) > maximumRelativeCoordinate) {
        throw std::invalid_argument("unsupported completion beam coordinates");
    }
    result.center = transformToView(actor.position, view);
    result.timer = actor.behaviorTimer;
    if (result.center.y < nearDepth) {
        return result;
    }
    const auto x = mainViewport.left + (mainViewport.right - mainViewport.left) / 2
        + divide(result.center.x, result.center.y);
    const auto y = mainViewport.top + (mainViewport.bottom - mainViewport.top) / 2
        - divide(result.center.z, result.center.y);
    if (x < mainViewport.left || x > mainViewport.right || y < mainViewport.top || y > mainViewport.bottom) {
        return result;
    }
    result.projected = true;
    result.anchor = {static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)};
    const auto denominator = std::max(nearDepth, result.center.y - static_cast<std::int32_t>(actor.collisionRadius));
    result.radius = static_cast<std::uint16_t>(std::min(maximumRadius,
        divide(static_cast<std::int32_t>(actor.collisionRadius), denominator)));
    generateCompletionBeam(result, rng, context);
    return result;
}

std::vector<CompletionBeamPixel> completionBeamPixels(const CompletionBeamPresentation& beam,
    const Viewport viewport) {
    if (!(viewport == mainViewport)) {
        throw std::invalid_argument("unsupported completion beam viewport");
    }
    std::vector<CompletionBeamPixel> pixels;
    if (!beam.projected) {
        return pixels;
    }
    if (beam.timer < lastBeamTimer || beam.timer > firstBeamTimer) {
        throw std::invalid_argument("unsupported completion beam timer");
    }
    const auto length = beamLengthBase - static_cast<std::int8_t>(beam.timer);
    for (std::size_t index = 0; index < beam.colors.size(); ++index) {
        const auto offset = firstLine + static_cast<int>(index);
        const auto x = beam.anchor.x + offset;
        const auto start = beam.anchor.y + std::abs(offset);
        const auto end = beam.anchor.y + length;
        if (x < viewport.left || x > viewport.right || std::min(start, end) > viewport.bottom
            || std::max(start, end) < viewport.top) {
            continue;
        }
        const auto first = static_cast<std::int16_t>(std::clamp(start, int{viewport.top}, int{viewport.bottom}));
        const auto last = static_cast<std::int16_t>(std::clamp(end, int{viewport.top}, int{viewport.bottom}));
        const auto line = rasterLineOperations({static_cast<std::int16_t>(x), first},
            {static_cast<std::int16_t>(x), last}, viewport, PolygonClipMode::Enabled);
        for (const auto point : line.pixels) {
            pixels.push_back({point, beam.colors[index]});
        }
    }
    return pixels;
}

}
