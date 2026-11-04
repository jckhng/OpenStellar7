#include "stellar7/radar_work.hpp"

#include <cassert>
#include <stdexcept>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace {
using namespace stellar7;

std::uint64_t operationHash(const std::vector<NormalWorkOperation>& operations) {
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t hash = 14695981039346656037ULL;
    std::uint64_t pending = 0;
    const auto append = [&](const auto kind, const auto count) {
        hash = (hash ^ static_cast<std::uint64_t>(kind)) * prime;
        hash = (hash ^ count) * prime;
    };
    for (const auto& operation : operations) {
        if (operation.kind == NormalWorkKind::instructions) {
            pending += operation.count;
            continue;
        }
        if (pending != 0) {
            append(NormalWorkKind::instructions, pending);
            pending = 0;
        }
        append(operation.kind, operation.count);
    }
    if (pending != 0) { append(NormalWorkKind::instructions, pending); }
    return hash;
}

template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace stellar7;
    const WorldViewSetupInput view{{0, 0, 200000}, {0xc000, 0, 0}, {},
        0x88, 7, {112, 205, 146, 180}, {256, -256, 362}};
    const IndexedBitmap panel{120, 56, std::vector<std::uint8_t>(120 * 56, 7)};
    Model3d lift{};
    lift.flags = 1;
    lift.animationLimits = {8};
    std::vector<RadarActorInput> actors{
        {{0, {-2048, 2048, 0}}, 1342, 1, -1, 0, WorldActorRenderProfile::noHandles, 0, 0, 0},
        {{0, {8192, 8192, 1000}}, 748, 11, -1, 0x130c,
            WorldActorRenderProfile::heapSelectorDeferredAnimation, 0, 0, 0},
        {{0, {16384, 16384, 0}}, 862, 29, -2, 0, WorldActorRenderProfile::noHandles, 0, 0, 0},
        {{0, {16384, -16384, 0}}, 862, 29, -2, 0, WorldActorRenderProfile::noHandles, 0, 0, 0},
        {{0, {-16384, 16384, 0}}, 862, 29, -2, 0, WorldActorRenderProfile::noHandles, 0, 0, 0},
        {{0, {-16384, -16384, 0}}, 862, 29, -2, 0, WorldActorRenderProfile::noHandles, 0, 0, 0}};
    const RadarRetainedState launchState{680, 42, {0xa400, 0xa800, 0xa000, 0xa800, 0xa400}};
    const auto launch = level1RadarOperations(launchState, view, {}, 0, actors, {}, panel, {228, 138}, lift, {5, 5, 1});
    // Actual retained P27 loop0 original: ordinary/fixed marker work is unchanged.
    assert(operationHash(launch.radar.operations) == 800411033709352007ULL);
    assert(launch.animation.countdown == 4 && launch.radar.pixels.size() == 6726);
    for (std::size_t index = 0; index < actors.size(); ++index) {
        actors[index].projectedX = launch.radar.actors[index].projectedX;
        actors[index].projectedY = launch.radar.actors[index].projectedY;
        actors[index].projectedRadius = launch.radar.actors[index].projectedRadius;
    }
    actors[0].actor.position = {-1841, 1961, 0};
    actors[1].actor.position = {7927, 8139, 1000};
    const RadarActorInput shot{{0, {}}, 134, 23, -3, 0,
        WorldActorRenderProfile::inlineSelectorNoCallback, 0, 0, 0};
    actors.push_back(shot);
    const RadarRetainedState listedState{680, 42, {0xa400, 0xa000, 0xa800, 0xa000, 0xa400}};
    const auto listed = level1ProjectileRadarOperations(listedState, view, {}, 0, actors, {}, panel,
        {228, 138}, lift, {5, 3, 1});
    // Original loop1 takes6A6E directly to6A7C for FD, skipping the fixed Type25 gate.
    assert(operationHash(listed.radar.operations) == 455291447209597337ULL);
    assert(listed.animation.countdown == 2 && listed.animation.variant == 1);
    assert(listed.radar.pixels.size() == 6727 && listed.radar.pixels.back().color == 238);
    assert(listed.radar.pixels.back().point.x == 154 && listed.radar.pixels.back().point.y == 163);
    const auto& projected = listed.radar.actors.back();
    assert(projected.projected && projected.flags == 0 && projected.projectedX == 158 && projected.projectedY == 0);
    assert((projected.cachedCenter == FixedPosition{0, 200000, 0}));
    rejects([&] { level1RadarOperations(listedState, view, {}, 0, actors, {}, panel, {228, 138}, lift, {5, 3, 1}); });

    // The same FD branch belongs to existing sourceWork Type19/23 callers.
    const RadarRetainedState compatibility{680, 42, {0xa800, 0xa000, 0xa400, 0xa000, 0xa800}};
    const auto primary = radarOperations(compatibility, view, {}, 0, {shot}, {}, panel, {228, 138});
    auto other = shot;
    other.type = 19;
    const auto secondary = radarOperations(compatibility, view, {}, 0, {other}, {}, panel, {228, 138});
    assert(operationHash(primary.operations) == operationHash(secondary.operations));
    assert(primary.cpuWorkQualified && secondary.cpuWorkQualified);
    bool sourceCaller = false;
    for (std::size_t index = 1; index < primary.operations.size(); ++index) {
        sourceCaller = sourceCaller || (primary.operations[index - 1].kind == NormalWorkKind::instructions
            && primary.operations[index - 1].count == 6 && primary.operations[index].kind == NormalWorkKind::instructions
            && primary.operations[index].count == 15);
    }
    assert(sourceCaller);
    other.category = -2;
    rejects([&] { radarOperations(compatibility, view, {}, 0, {other}, {}, panel, {228, 138}); });
    other = shot;
    other.type = 22;
    rejects([&] { radarOperations(compatibility, view, {}, 0, {other}, {}, panel, {228, 138}); });
}
