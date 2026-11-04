#include "playable_detail_render.hpp"
#include "stellar7/indexed_render.hpp"

#include <array>
#include <cassert>
#include <stdexcept>
#include <vector>

//Root can compile this negative control before promoting the implementation.
//It calls the existing full-detail entry points and fails at legal setting70.
#ifdef STELLAR7_DETAIL_RED_CONTROL
#define playableCockpitOperations cockpitOperations
#define playableHorizonExecution horizonExecution
#define playableLevel2HorizonExecution level2HorizonExecution
#endif

namespace {
using namespace stellar7;

template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

IndexedBitmap bitmap(const std::uint16_t width, const std::uint16_t height,
    const std::uint8_t color) {
    return {width, height, std::vector<std::uint8_t>(width * height, color)};
}

void sameOperations(const std::vector<NormalWorkOperation>& a,
    const std::vector<NormalWorkOperation>& b) {
    assert(a.size() == b.size());
    for (std::size_t index = 0; index < a.size(); ++index) {
        assert(a[index].kind == b[index].kind && a[index].count == b[index].count
            && a[index].value == b[index].value);
    }
}

void samePixels(const std::vector<HorizonPixelWrite>& a,
    const std::vector<HorizonPixelWrite>& b) {
    assert(a.size() == b.size());
    for (std::size_t index = 0; index < a.size(); ++index) {
        assert(a[index].point == b[index].point && a[index].color == b[index].color);
    }
}

std::array<std::uint8_t, 320 * 200> frame(const HorizonExecutionWork& work) {
    std::array<std::uint8_t, 320 * 200> result{};
    for (const auto& pixel : work.pixels) {
        result.at(pixel.point.y * 320 + pixel.point.x) = pixel.color;
    }
    return result;
}

std::uint64_t dispatches(const std::vector<NormalWorkOperation>& operations) {
    std::uint64_t result = 0;
    for (const auto& operation : operations) {
        if (operation.kind == NormalWorkKind::instructions) { result += operation.count; }
    }
    return result;
}
}

int main() {
    using namespace stellar7;
    CockpitWorkBitmaps art;
    art.strip = bitmap(56, 18, 3);
    art.indicator = bitmap(8, 8, 4);
    art.matchingPageBackground = bitmap(120, 18, 5);
    for (auto& overlay : art.overlays) { overlay = bitmap(8, 8, 7); }
    for (const auto relation : {CockpitPageRelation::matching, CockpitPageRelation::differing}) {
        CockpitWorkState state{180, 4, 70, relation};
        const auto low = playableCockpitOperations(state, art);
        assert(low.state.indicatorX == 176 && low.state.indicatorVelocity == -4);
        assert(low.effects.size() == (relation == CockpitPageRelation::matching ? 4 : 3));
        assert(low.operations.back().kind == NormalWorkKind::instructions && low.operations.back().count == 5);
        for (const auto& effect : low.effects) {
            if (const auto* placement = std::get_if<CockpitBitmapPlacement>(&effect)) {
                assert(placement->bitmap == CockpitBitmapRole::matchingPageBackground
                    || placement->bitmap == CockpitBitmapRole::strip
                    || placement->bitmap == CockpitBitmapRole::indicator);
            }
        }
        //The inactive overlay branch must not even read malformed bitmap metadata.
        auto inactive = art;
        inactive.overlays = {};
        sameOperations(low.operations, playableCockpitOperations(state, inactive).operations);
        state.stage = 51;
        sameOperations(low.operations, playableCockpitOperations(state, art).operations);
        state.stage = 71;
        const auto high = playableCockpitOperations(state, art);
        assert(high.effects.size() == low.effects.size() + art.overlays.size());
        assert(high.state.indicatorX == low.state.indicatorX);
        sameOperations(high.operations, cockpitOperations(state, art).operations);
        for (const std::int16_t setting : {81, 82}) {
            state.stage = setting;
            sameOperations(playableCockpitOperations(state, art).operations, cockpitOperations(state, art).operations);
        }
        state.stage = 70;
        rejects([&] { cockpitOperations(state, art); });
        for (const std::int16_t setting : {-1, 0, 20, 50, 83}) {
            state.stage = setting;
            rejects([&] { playableCockpitOperations(state, art); });
        }
    }

    for (const bool level2 : {false, true}) {
        const auto height = static_cast<std::uint16_t>(level2 ? 18 : 16);
        std::vector<IndexedBitmap> frames;
        for (std::uint8_t page = 0; page < 4; ++page) { frames.push_back(bitmap(256, height, 20 + page)); }
        frames.push_back(bitmap(level2 ? 72 : 32, level2 ? 38 : 27, 200));
        HorizonWorkState state{24, 145, 4096, 70, 256, height,
            level2 ? HorizonBackground::solid : HorizonBackground::gradient,
            HorizonOverlay::enabled, static_cast<std::uint8_t>(level2 ? 64 : 233),
            static_cast<std::uint8_t>(level2 ? 78 : 246)};
        const auto native = level2 ? playableLevel2HorizonExecution : playableHorizonExecution;
        const auto strict = level2 ? level2HorizonExecution : horizonExecution;
        const auto low = native(state, frames);
        assert(frame(low)[24 * 320 + 24] != 200);
        auto disabled = state;
        disabled.overlay = HorizonOverlay::disabled;
        const auto noOverlay = native(disabled, frames);
        samePixels(low.pixels, noOverlay.pixels);
        //Source6C85/8A,6C8C/90,6C92/97 execute six instructions before rejecting.
        assert(dispatches(low.operations) == dispatches(noOverlay.operations) + 6);
        state.stage = 51;
        sameOperations(low.operations, native(state, frames).operations);
        state.stage = 71;
        const auto high = native(state, frames);
        assert(frame(high)[24 * 320 + 24] == 200);
        sameOperations(high.operations, strict(state, frames).operations);
        samePixels(high.pixels, strict(state, frames).pixels);
        for (const std::int16_t setting : {81, 82}) {
            state.stage = setting;
            sameOperations(native(state, frames).operations, strict(state, frames).operations);
            samePixels(native(state, frames).pixels, strict(state, frames).pixels);
        }
        state.stage = 70;
        rejects([&] { strict(state, frames); });
        for (const std::int16_t yaw : {-80, 0, 16, 20480}) {
            state.cameraYaw = yaw;
            const auto lowYaw = native(state, frames);
            state.stage = 71;
            sameOperations(lowYaw.operations, native(state, frames).operations);
            samePixels(lowYaw.pixels, native(state, frames).pixels);
            state.stage = 70;
        }
        state.stage = 50;
        rejects([&] { native(state, frames); });
        state.stage = 83;
        rejects([&] { native(state, frames); });
    }
}
