#include "stellar7/blackout_work.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::uint16_t firstDisplayPage = 0xa000;
constexpr std::uint16_t secondDisplayPage = 0xa400;
constexpr std::uint16_t recoveredDisplayPage = 0xa800;
constexpr std::int16_t displayWidth = 320;
constexpr std::int16_t displayHeight = 200;
constexpr std::uint8_t initialIntensity = 63;
constexpr std::uint8_t intensityStep = 4;
constexpr std::uint64_t blackoutSetupDispatches = 15;
constexpr std::uint64_t outerPaletteCallDispatches = 7;
constexpr std::uint64_t residentPaletteSetupDispatches = 11;
constexpr std::uint64_t residentPaletteReturnDispatches = 3;
constexpr std::uint64_t outerPaletteReturnDispatches = 3;
constexpr std::uint64_t afterLoopDispatches = 2;
constexpr std::uint64_t rectangleSetupDispatches = 8;
constexpr std::uint64_t rectangleReturnDispatches = 2;
constexpr std::uint64_t blackoutReturnDispatches = 6;
constexpr std::int64_t maximumNormalHorizonCycles = 120000;

void requireFront(const std::uint16_t frontPage, const BlackoutPageLayout layout) {
    const auto secondPage = layout == BlackoutPageLayout::compatibility ? secondDisplayPage : recoveredDisplayPage;
    if ((layout != BlackoutPageLayout::compatibility && layout != BlackoutPageLayout::recoveredStartup)
        || (frontPage != firstDisplayPage && frontPage != secondPage)) {
        throw std::invalid_argument("unsupported blackout front page");
    }
}
}

BlackoutSourcePlan blackoutSourcePlan(const BlackoutSurfaceState& surface, const std::uint16_t frontPage,
    const BlackoutPageLayout layout) {
    requireFront(frontPage,layout);
    if (surface.driver != BlackoutDriver::modeX || surface.scale != 0 || surface.remap != 0
        || surface.rasterStyle != 1 || surface.paletteEnabled != 1
        || (surface.clipping != BlackoutClip::disabled && surface.clipping != BlackoutClip::enabled)) {
        throw std::invalid_argument("unsupported source blackout surface profile");
    }
    BlackoutSourcePlan result{};
    for (std::size_t i = 0; i < blackoutPaletteSteps; ++i) {
        result.beforePalette[i] = outerPaletteCallDispatches + residentPaletteSetupDispatches
            + (i == 0 ? blackoutSetupDispatches : residentPaletteReturnDispatches + outerPaletteReturnDispatches);
    }
    result.clear.push_back({NormalWorkKind::instructions, residentPaletteReturnDispatches
        + outerPaletteReturnDispatches + afterLoopDispatches + rectangleSetupDispatches});
    const auto body = fullFrontClearOperations();
    result.clear.insert(result.clear.end(), body.begin(), body.end());
    result.clear.push_back({NormalWorkKind::instructions, rectangleReturnDispatches + blackoutReturnDispatches});
    return result;
}

BlackoutClockTiming blackoutClockTiming(const SoundBlasterClockPhase& phase,
    const BlackoutWorkState& state, const std::uint16_t frontPage, const std::int64_t horizonCycles,
    const BlackoutPageLayout layout) {
    const auto source = blackoutSourcePlan(state.surface, frontPage,layout);
    BlackoutClockTiming result{};
    result.state = state;
    auto incoming = phase;
    auto palette = state.palette;
    for (std::size_t i = 0; i < blackoutPaletteSteps; ++i) {
        try {
            auto& step = result.steps[i];
            step.before = normalWorkTiming(incoming, {{NormalWorkKind::instructions, source.beforePalette[i]}},
                std::min(horizonCycles,maximumNormalHorizonCycles));
            step.intensity = static_cast<std::uint8_t>(initialIntensity - i * intensityStep);
            step.palette = paletteClockTiming(step.before.phase, palette, {0, 256, 0, step.intensity}, horizonCycles);
            palette = step.palette.state;
            incoming = step.palette.timing.phase;
        } catch (const std::invalid_argument& error) {
            throw std::invalid_argument("blackout palette "+std::to_string(i)+": "+error.what());
        }
    }
    try {
        result.clear = normalWorkTiming(incoming, source.clear,std::min(horizonCycles,maximumNormalHorizonCycles));
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument(std::string("blackout clear: ")+error.what());
    }
    result.state.palette = std::move(palette);
    result.state.surface.firstColor = 0;
    result.state.surface.secondColor = 0;
    result.state.surface.sequencer = BlackoutSequencerState{2, 0};
    return result;
}

void applyBlackoutClear(IndexedPages& pages, const std::uint16_t frontPage, const BlackoutPageLayout layout) {
    requireFront(frontPage,layout);
    for (std::int16_t y = 0; y < displayHeight; ++y) {
        for (std::int16_t x = 0; x < displayWidth; ++x) {
            pages.write(frontPage, {x, y}, 0);
        }
    }
}

}
