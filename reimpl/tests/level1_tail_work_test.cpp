#include "stellar7/frame_tail_work.hpp"

#include <cassert>
#include <stdexcept>

namespace {
using namespace stellar7;

std::vector<NormalWorkOperation> compact(const std::vector<NormalWorkOperation>& operations) {
    std::vector<NormalWorkOperation> result;
    for (const auto operation : operations) {
        if (!result.empty() && result.back().kind == operation.kind && result.back().value == operation.value) {
            result.back().count += operation.count;
        } else {
            result.push_back(operation);
        }
    }
    return result;
}

void sameOperations(const std::vector<NormalWorkOperation>& actual,
    const std::vector<NormalWorkOperation>& expected) {
    const auto merged = compact(actual);
    assert(merged.size() == expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        assert(merged[index].kind == expected[index].kind);
        assert(merged[index].count == expected[index].count && merged[index].value == expected[index].value);
    }
}

void sameRng(StellarRng actual, StellarRng expected) {
    assert(actual.indexA() == expected.indexA() && actual.indexB() == expected.indexB());
    assert(actual.last() == expected.last() && actual.calls() == expected.calls());
    for (std::size_t word = 0; word < 2 * StellarRng::wordCount; ++word) {
        assert(actual.next() == expected.next());
    }
}

void reject(const FrameTailState& state, const FrameTailGuards& guards, const StellarRng& rng) {
    bool rejected = false;
    try {
        level1FrameTailOperations(state, guards, rng);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

std::vector<NormalWorkOperation> expectedAfter(const bool first,
    const std::array<bool, 9>& held = {}, const std::uint64_t terminalCount = 0) {
    std::vector<NormalWorkOperation> result{{NormalWorkKind::instructions, first ? 15U : 41U}};
    for (unsigned query = 0; query < 8; ++query) {
        result.push_back({NormalWorkKind::cli, 1});
        result.push_back({NormalWorkKind::instructions, 7});
        result.push_back({NormalWorkKind::sti, 1});
        result.push_back({NormalWorkKind::instructions, (query == 7 ? 11U : 7U) + (held[query] ? 1U : 0U)});
    }
    result.push_back({NormalWorkKind::cli, 1});
    result.push_back({NormalWorkKind::instructions, 8});
    result.push_back({NormalWorkKind::popFlagsEnabled, 1});
    result.push_back({NormalWorkKind::instructions, 10});
    result.push_back({NormalWorkKind::cli, 1});
    result.push_back({NormalWorkKind::instructions, 7});
    result.push_back({NormalWorkKind::sti, 1});
    result.push_back({NormalWorkKind::instructions, terminalCount != 0 ? terminalCount : (first ? 379U : 238U)});
    return result;
}
}

int main() {
    using namespace stellar7;
    FrameTailState first{0, {}, 0, 0, {}, {
        {1, {-2048, 2048, 0}}, {11, {8192, 8192, 1000}},
        {29, {16384, 16384, 0}}, {29, {16384, -16384, 0}},
        {29, {-16384, 16384, 0}}, {29, {-16384, -16384, 0}}}};
    StellarRng incoming;
    for (unsigned draw = 0; draw < 28; ++draw) {
        incoming.next();
    }
    FrameTailGuards guards{};
    guards.armor = 680;
    guards.keyboardHead = guards.keyboardTail = 0x1e;
    guards.firstFrame = true;
    guards.missionActive = true;
    guards.periodic = FrameTailPeriodicInput{0, 2, 2, 10, 10, incoming.indexA(), incoming.indexB(), 0};
    auto expectedRng = incoming;
    expectedRng.next();
    const auto completed = level1FrameTailOperations(first, guards, incoming);
    assert(completed.entryFade && completed.state.loopTick == 1);
    assert(completed.state.scheduler == FrameTailScheduler::none);
    assert(completed.keyboardHead == guards.keyboardHead && completed.consumedScan == 0);
    sameOperations(completed.beforeEntryFade, {{NormalWorkKind::instructions, 28}});
    sameOperations(completed.afterEntryFade, expectedAfter(true));
    sameRng(completed.rng, expectedRng);
    assert(incoming.calls() == 28 && first.loopTick == 0);
    assert(completed.state.playerPosition == FixedPosition{});
    for (std::size_t index = 0; index < first.actors.size(); ++index) {
        assert(completed.state.actors[index].type == first.actors[index].type);
        assert(completed.state.actors[index].position == first.actors[index].position);
    }

    auto adjacentRng = completed.rng;
    for (unsigned draw = 0; draw < 13; ++draw) {
        adjacentRng.next();
    }
    auto adjacentGuards = guards;
    adjacentGuards.firstFrame = false;
    adjacentGuards.periodic.reset();
    const auto adjacent = level1FrameTailOperations(completed.state, adjacentGuards, adjacentRng);
    assert(!adjacent.entryFade && adjacent.beforeEntryFade.empty() && adjacent.state.loopTick == 2);
    sameOperations(adjacent.afterEntryFade, expectedAfter(false));
    sameRng(adjacent.rng, adjacentRng);

    auto effectsGuards = guards;
    effectsGuards.sourceKeyboardEffects = true;
    const auto effects = level1FrameTailOperations(first, effectsGuards, incoming);
    unsigned scanStores = 0;
    for (const auto operation : effects.afterEntryFade) {
        if (operation.kind == NormalWorkKind::keyboardStoreScan) {
            assert(operation.count == 1 && operation.value == 0);
            ++scanStores;
        }
    }
    assert(scanStores == 1);

    std::array<std::uint16_t, StellarRng::wordCount> table{};
    table.back() = 100;
    const StellarRng otherRng(table, 0x30, 0x6e, 0, 0);
    auto otherGuards = guards;
    otherGuards.periodic->rngIndexA = otherRng.indexA();
    otherGuards.periodic->rngIndexB = otherRng.indexB();
    const auto other = level1FrameTailOperations(first, otherGuards, otherRng);
    assert(other.rng.calls() == 1 && other.rng.last() == 100);
    sameOperations(other.afterEntryFade, expectedAfter(true));
    table.back() = 5; // Low below threshold, zero high byte bypasses enemy selection.
    const StellarRng zeroHigh(table, 0x30, 0x6e, 0, 0);
    const auto zeroHighWork = level1FrameTailOperations(first, otherGuards, zeroHigh);
    sameOperations(zeroHighWork.afterEntryFade, expectedAfter(true, {}, 381));
    assert(zeroHighWork.rng.calls() == 1 && zeroHighWork.rng.last() == 5);
    table.back() = 0x0105; // Opens the source's unsupported positive-high spawn branch.
    const StellarRng spawning(table, 0x30, 0x6e, 0, 0);
    reject(first, otherGuards, spawning);
    assert(spawning.calls() == 0);

    table.fill(0);
    table[0] = 100;
    const StellarRng zeroIndices(table, 0, 0, 0, 0);
    auto zeroIndicesGuards = guards;
    zeroIndicesGuards.firstFrame = false;
    zeroIndicesGuards.heldKeys[1] = true;
    zeroIndicesGuards.periodic->rngIndexA = zeroIndicesGuards.periodic->rngIndexB = 0;
    auto zeroIndicesState = first;
    zeroIndicesState.loopTick = 64;
    zeroIndicesState.forwardVelocity = -200;
    const auto zeroIndicesWork = level1FrameTailOperations(zeroIndicesState, zeroIndicesGuards, zeroIndices);
    sameOperations(zeroIndicesWork.afterEntryFade, expectedAfter(false, zeroIndicesGuards.heldKeys, 407));
    auto expectedZeroIndices = zeroIndices;
    expectedZeroIndices.next();
    sameRng(zeroIndicesWork.rng, expectedZeroIndices);

    // Original0312->03E8 controls use resident bytes, empty BDA, and live RNG.
    struct Control {
        std::uint16_t tick;
        std::uint16_t heldMask;
        std::int16_t yaw, speed;
        FixedPosition player;
        std::array<std::uint8_t, 3> timers;
        bool firstFrame;
        std::int16_t returnedYaw, returnedSpeed;
        std::uint64_t terminalCount;
    };
    const Control controls[]{
        {2, 0x10, 0, 0, {}, {}, false, 0, 10, 258},
        {3, 0x02, 0, 0, {}, {}, false, 0, -10, 257},
        {4, 0x40, 0, 73, {}, {}, false, 80, 73, 246},
        {5, 0x80, 0, -73, {}, {}, false, -80, -73, 248},
        {6, 0xd2, 0, 0, {}, {}, false, 80, -10, 247},
        {7, 0x50, 450, 195, {}, {}, false, 512, 200, 263},
        {8, 0x82, -450, -195, {}, {}, false, -512, -200, 262},
        {9, 0x10, -80, -200, {}, {}, false, 0, -190, 259},
        {10, 0x02, 80, 200, {}, {}, false, 0, 190, 259},
        {11, 0x10, -79, 0, {}, {}, false, 0, 10, 258},
        {12, 0, 80, 10, {}, {}, false, 0, 0, 242},
        {13, 0, -80, -10, {}, {}, false, 0, 0, 240},
        {14, 0, 79, -9, {}, {}, false, 0, 0, 238},
        {15, 0x10, 0, 0, {321, -654, 0}, {2, 3, 255}, false, 0, 10, 264},
        {16, 0x10, 0, 120, {}, {}, false, 0, 130, 399},
        {32, 0x80, -512, 200, {50000, -50000, 0}, {}, false, -512, 200, 405},
        {65535, 0x10, 0, 200, {}, {}, false, 0, 200, 266},
        {0, 0, 0, 0, {}, {}, false, 0, 0, 379},
        {0, 0x50, 0, 0, {}, {}, true, 80, 10, 389},
    };
    for (const auto& control : controls) {
        auto state = first;
        state.loopTick = control.tick;
        state.yawVelocity = control.yaw;
        state.forwardVelocity = control.speed;
        state.playerPosition = control.player;
        state.weaponTimers = control.timers;
        auto input = guards;
        input.firstFrame = control.firstFrame;
        for (std::size_t key = 0; key < input.heldKeys.size(); ++key) {
            input.heldKeys[key] = (control.heldMask & (1U << key)) != 0;
        }
        if ((control.tick & 15) != 0) {
            input.periodic.reset();
        }
        const auto result = level1FrameTailOperations(state, input, incoming);
        sameOperations(result.afterEntryFade,
            expectedAfter(control.firstFrame, input.heldKeys, control.terminalCount));
        assert(result.entryFade == control.firstFrame);
        if (control.firstFrame) {
            sameOperations(result.beforeEntryFade, {{NormalWorkKind::instructions, 28}});
        } else {
            assert(result.beforeEntryFade.empty());
        }
        assert(result.state.loopTick == static_cast<std::uint16_t>(control.tick + 1));
        assert(result.state.yawVelocity == control.returnedYaw && result.state.forwardVelocity == control.returnedSpeed);
        assert(result.state.playerPosition == FixedPosition{});
        auto expected = incoming;
        if ((control.tick & 15) == 0) {
            expected.next();
        }
        sameRng(result.rng, expected);
        for (std::size_t slot = 0; slot < control.timers.size(); ++slot) {
            assert(result.state.weaponTimers[slot] == (control.timers[slot] == 0 ? 0 : control.timers[slot] - 1));
        }
        if (control.tick == 15) {
            assert((result.state.actors[0].position == FixedPosition{-2369, 2702, 0}));
            assert((result.state.actors[5].position == FixedPosition{-16705, -15730, 0}));
        } else if (control.tick == 32) {
            const FixedPosition expectedPositions[]{{13488, -13488, 0}, {23728, -7344, 1000},
                {31920, 848, 0}, {31920, -31920, 0}, {-848, 848, 0}, {-848, -31920, 0}};
            for (std::size_t actor = 0; actor < result.state.actors.size(); ++actor) {
                assert(result.state.actors[actor].position == expectedPositions[actor]);
            }
        }
    }

    for (unsigned mutation = 0; mutation < 17; ++mutation) {
        auto state = first;
        auto invalid = guards;
        if (mutation == 0) { state.playerPosition.z = 1; }
        if (mutation == 1) { state.loopTick = 2; }
        if (mutation == 2) { invalid.joystickEnabled = true; }
        if (mutation == 3) { state.forwardVelocity = 201; }
        if (mutation == 4) { state.yawVelocity = -513; }
        if (mutation == 5) { state.weaponTimers[0] = 1; }
        if (mutation == 6) { state.actors[0].type = 7; }
        if (mutation == 7) { state.actors.pop_back(); }
        if (mutation == 8) { invalid.keyboardTail += 2; }
        if (mutation == 9) { invalid.keyboardQueue = FrameTailKeyboardQueue::forwardMake; }
        if (mutation == 10) { invalid.abilities[0] = 1; }
        if (mutation == 11) { invalid.extraUpdate = true; }
        if (mutation == 12) { invalid.periodic.reset(); }
        if (mutation == 13) { invalid.periodic->spawnThreshold = 0; }
        if (mutation == 14) { invalid.periodic->activeEnemies = 3; }
        if (mutation == 15) { invalid.periodic->enemyFlag = 1; }
        if (mutation == 16) { invalid.periodic->rngIndexA ^= 2; }
        reject(state, invalid, incoming);
    }
    for (const std::size_t key : {0U, 2U, 3U, 5U, 8U}) {
        auto held = guards;
        held.heldKeys[key] = true;
        reject(first, held, incoming);
    }
    auto fire = adjacentGuards;
    fire.heldKeys[8] = true;
    reject(adjacent.state, fire, adjacent.rng);
}
