#include "stellar7/level1_frame.hpp"

#include <cassert>
#include <stdexcept>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace {
using namespace stellar7;

template<class Function>
void rejects(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

GameplayAssets assets() {
    GameplayAssets result;
    result.horizonFrames.assign(4, {256, 16, std::vector<std::uint8_t>(256 * 16, 7)});
    result.artGroups.resize(5);
    result.artGroups[3] = {{120, 56, std::vector<std::uint8_t>(120 * 56, 7)},
        {4, 1, {0, 0, 0, 0}}, {120, 18, std::vector<std::uint8_t>(120 * 18, 3)}};
    result.artGroups[4].assign(15, {4, 1, {0, 0, 0, 0}});
    result.artGroups[4][6].pixels.back() = 9;
    for (std::size_t index = 7; index < 15; ++index) {
        result.artGroups[4][index] = {8, 1, std::vector<std::uint8_t>(8, 13)};
    }
    result.models.resize(30);
    result.models[1].collisionRadius = 1342;
    result.models[11].collisionRadius = 748;
    result.models[11].flags = 1;
    result.models[11].animationLimits = {8};
    result.models[29].collisionRadius = 862;
    result.sharedModelRegistry[0] = 256;
    result.sharedModelRegistry[1] = 256;
    return result;
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    auto data = assets();
    Level1State level;
    level.reloadAfterFailure(StellarRng{});
    auto actors = level.entities();
    //Synthetic geometry-free composition control: every main model is culled.
    for (auto& actor : actors) {
        actor.position.y = -16384;
    }
    auto ground = level.ground();
    auto rng = level.rng();
    Level1RenderState frame{ground, rng, level.player(), actors, level.activeListOrder(), 0};
    const CockpitSettings settings{90, {}};
    Level1PresentationState presentation{{140, 4}, false};
    auto pages = CockpitPageState::fromRecoveredMenu();
    IndexedPages pixels;
    for (const std::uint16_t page : {0xa000, 0xa400, 0xa800}) {
        for (std::int16_t y = 0; y < 200; ++y) {
            for (std::int16_t x = 0; x < 320; ++x) {
                pixels.write(page, {x, y}, 93);
            }
        }
    }
    RadarState radar(data.artGroups[3]);
    for (std::size_t index = 0; index < actors.size(); ++index) {
        radar.initializeAllocation(index);
    }
    pixels.apply(radar.initializeLevel1(680, {}, 0,
        {pages.cachePage(), pages.backPage(), pages.frontPage(), pages.backPage(), pages.cachePage()}));
    const auto first = planLevel1Frame(data, frame, presentation, 680, settings, pages, radar, pixels);
    assert(rng.calls() == 16 && first.ground.rng.calls() == 28);
    assert(first.animationAfterMain.variant == 1 && first.animationAfterMain.countdown == 5);
    assert(first.animationAfterRadar.variant == 1 && first.animationAfterRadar.countdown == 4);
    assert(first.presentationAfter.indicator.x == 144 && first.presentationAfter.alternateSightPhase);
    assert(first.pixels.read(0xa800, {80, 24}) == 254);
    assert(first.pixels.read(0xa800, {80, 67}) == 233);
    assert(first.pixels.read(0xa800, {80, 68}) == 7);
    assert(first.pixels.read(0xa800, {80, 100}) == 246);
    assert(first.pixels.read(0xa000, {80, 24}) == pixels.read(0xa000, {80, 24}));
    assert(first.radar.cpuWorkQualified && !first.radar.pageCopy && first.radarActors.size() == 6);
    assert(pixels.read(0xa800, {80, 24}) == 93 && radar.projection(1).cachedCenter.y == 0);
    for (std::int16_t y = 0; y < 200; ++y) {
        for (std::int16_t x = 0; x < 320; ++x) {
            assert(first.pixels.read(0xa400, {x, y}) == pixels.read(0xa400, {x, y}));
        }
    }
    commitLevel1Frame(first, frame, pages, radar, pixels);
    assert(rng.calls() == 28 && actors[1].stateBytes4dTo52[3] == 0 && actors[1].modelAnimationVariant == 0);
    assert(pages.frontPage() == 0xa000 && pages.backPage() == 0xa800 && pages.drawPage() == 0xa800);
    assert(radar.projection(1).cachedCenter.y == 199000);
    rng.next(); //The separately owned periodic first tail consumes one word.
    actors[1].stateBytes4dTo52[3] = first.animationAfterRadar.countdown;
    actors[1].modelAnimationVariant = first.animationAfterRadar.variant;
    presentation = first.presentationAfter;
    pages.flip();
    pages.beginFrame();
    frame.loopTick = 1;
    const auto adjacent = planLevel1Frame(data, frame, presentation, 680, settings, pages, radar, pixels);
    assert(adjacent.animationAfterMain.countdown == 3 && adjacent.animationAfterRadar.countdown == 2);
    assert(adjacent.pages.frontPage() == 0xa800 && adjacent.pages.backPage() == 0xa000);
    assert(adjacent.pixels.read(0xa800, {80, 24}) == pixels.read(0xa800, {80, 24}));
    assert(adjacent.pixels.read(0xa000, {80, 24}) == 254);
    auto oldRoles = CockpitPageState{};
    rejects([&]() { planLevel1Frame(data, frame, presentation, 680, settings, oldRoles, radar, pixels); });
    actors[1].primaryType11AnimationSelector = false;
    rejects([&]() { planLevel1Frame(data, frame, presentation, 680, settings, pages, radar, pixels); });
    actors[1].primaryType11AnimationSelector = true;
    data.horizonFrames.pop_back();
    rejects([&]() { planLevel1Frame(data, frame, presentation, 680, settings, pages, radar, pixels); });
}
