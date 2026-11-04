#include "stellar7/frame_tail_work.hpp"

#include <cassert>
#include <stdexcept>

namespace {
std::uint64_t dispatches(const stellar7::FrameTailWork& work) {
    std::uint64_t total = 0;
    for (const auto operation : work.operations) {
        total += operation.count;
    }
    return total;
}

void reject(const stellar7::FrameTailState& state, const stellar7::FrameTailGuards& guards) {
    bool rejected = false;
    try {
        stellar7::frameTailOperations(state, guards);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}
}

int main() {
    using namespace stellar7;
    FrameTailState state{5, {112, -224, 0}, 0, 0, {}, {}};
    for (int actor = 0; actor < 7; ++actor) {
        state.actors.push_back({13, {1000 + actor * 100, -1000 - actor * 100, 0}});
    }
    FrameTailGuards guards{};
    guards.armor = 680;
    guards.keyboardHead = 0x1e;
    guards.keyboardTail = 0x1e;
    guards.missionActive = true;
    const auto zero = frameTailOperations(state, guards);
    assert(dispatches(zero) == 463);
    assert(zero.state.loopTick == 6);
    assert(zero.state.playerPosition.x == 0 && zero.state.playerPosition.y == 0);
    assert(zero.state.actors[0].position.x == 888 && zero.state.actors[0].position.y == -776);
    assert(state.playerPosition.x == 112 && state.loopTick == 5);
    std::array<std::uint64_t, 3> flags{};
    for (std::size_t index = 0; index < zero.operations.size(); ++index) {
        const auto operation = zero.operations[index];
        if (operation.kind == NormalWorkKind::cli) {
            ++flags[0];
        } else if (operation.kind == NormalWorkKind::sti) {
            ++flags[1];
            assert(operation.count == 1);
            assert(zero.operations[index + 1].kind == NormalWorkKind::instructions);
            assert(zero.operations[index + 1].count == 1); // RETF follows STI shadow.
        } else if (operation.kind == NormalWorkKind::popFlagsEnabled) {
            ++flags[2];
        } else {
            assert(operation.kind == NormalWorkKind::instructions);
        }
    }
    assert((flags == std::array<std::uint64_t, 3>{10, 9, 1}));

    FrameTailState periodic{16, {}, 0, 0, {}, {
        {13, {-22896, 26992, 0}}, {8, {12288, 24576, 0}},
        {8, {24576, -8192, 0}}, {13, {10608, -22896, 0}},
        {30, {-8192, 20480, 0}}, {30, {8192, -20480, 0}},
        {8, {-20480, 4096, 0}}}};
    auto periodicGuards = guards;
    periodicGuards.periodic = FrameTailPeriodicInput{1, 5, 5, 12, 0, 82, 32, 0};
    const auto periodicWork = frameTailOperations(periodic, periodicGuards);
    assert(dispatches(periodicWork) == 618);
    assert(periodicWork.state.loopTick == 17);
    assert(periodicWork.state.actors[0].position.x == -22896);
    assert(periodicWork.state.scheduler == FrameTailScheduler::zeroThresholdDraw);
    assert(periodic.scheduler == FrameTailScheduler::none);
    auto heldPeriodic = periodic;
    heldPeriodic.forwardVelocity = 120;
    auto heldPeriodicGuards = periodicGuards;
    heldPeriodicGuards.heldKeys[4] = true;
    const auto heldPeriodicWork = frameTailOperations(heldPeriodic, heldPeriodicGuards);
    assert(dispatches(heldPeriodicWork) == 639);
    assert(heldPeriodicWork.state.forwardVelocity == 130);
    assert(heldPeriodicWork.state.loopTick == 17);
    assert(heldPeriodicWork.state.scheduler == FrameTailScheduler::zeroThresholdDraw);
    for (const auto tick : {std::uint16_t{32}, std::uint16_t{48}, std::uint16_t{64}}) {
        auto capped = periodic;
        capped.loopTick = tick;
        capped.forwardVelocity = 200;
        auto cappedGuards = periodicGuards;
        cappedGuards.heldKeys[4] = true;
        const auto empty = frameTailOperations(capped, cappedGuards);
        assert(dispatches(empty) == 647 && empty.state.forwardVelocity == 200);
        assert(empty.state.loopTick == tick + 1 && empty.state.scheduler == FrameTailScheduler::zeroThresholdDraw);
        cappedGuards.keyboardQueue = FrameTailKeyboardQueue::forwardMake;
        cappedGuards.keyboardTail += 2;
        const auto queued = frameTailOperations(capped, cappedGuards);
        assert(dispatches(queued) == 816 && queued.state.forwardVelocity == 200);
        assert(queued.keyboardHead == cappedGuards.keyboardTail && queued.consumedScan == 0x48);
        cappedGuards.keyboardHead = 0x3c;
        cappedGuards.keyboardTail = 0x1e;
        const auto wrapped = frameTailOperations(capped, cappedGuards);
        assert(dispatches(wrapped) == 817 && wrapped.keyboardHead == 0x1e);
        capped.loopTick = 80;
        reject(capped, cappedGuards);
        capped.loopTick = tick;
        capped.forwardVelocity = 199;
        reject(capped, cappedGuards);
    }
    heldPeriodic.loopTick = 32;
    reject(heldPeriodic, heldPeriodicGuards);
    heldPeriodic.loopTick = 16;
    heldPeriodic.forwardVelocity = 121;
    reject(heldPeriodic, heldPeriodicGuards);
    heldPeriodic.forwardVelocity = 120;
    heldPeriodic.yawVelocity = 1;
    reject(heldPeriodic, heldPeriodicGuards);
    heldPeriodic.yawVelocity = 0;
    heldPeriodicGuards.keyboardQueue = FrameTailKeyboardQueue::forwardMake;
    heldPeriodicGuards.keyboardTail += 2;
    reject(heldPeriodic, heldPeriodicGuards);
    periodicGuards.periodic->rngIndexA = 0;
    periodicGuards.periodic->rngIndexB = 0;
    assert(dispatches(frameTailOperations(periodic, periodicGuards)) == 620);
    for (const auto coordinate : {32768, 32769, 65536, -32768, -32769, -65537}) {
        periodic.actors[0].position.x = coordinate;
        const auto normalized = frameTailOperations(periodic, periodicGuards);
        const auto expected = coordinate > 32768 ? coordinate - 65536
            : coordinate < -32768 ? coordinate + 65536 : coordinate;
        assert(normalized.state.actors[0].position.x == expected);
    }
    const std::array<FixedPosition, 7> control{{
        {65536, 32768, 0}, {32769, -65537, 0}, {-32768, -32769, 0},
        {0, -1, 0}, {131072, -131072, 0}, {1, 65535, 0}, {-65536, 32767, 0}}};
    for (std::size_t actor = 0; actor < control.size(); ++actor) {
        periodic.actors[actor].position = control[actor];
    }
    const auto normalizedControl = frameTailOperations(periodic, periodicGuards);
    assert(dispatches(normalizedControl) == 624);
    assert(normalizedControl.state.actors[4].position.x == 65536);
    assert(normalizedControl.state.actors[4].position.y == -65536);
    reject(periodic, guards);
    auto unsupportedPeriodic = periodicGuards;
    unsupportedPeriodic.periodic->enemyFlag.reset();
    reject(periodic, unsupportedPeriodic);
    unsupportedPeriodic = periodicGuards;
    unsupportedPeriodic.periodic->enemyFlag = 1;
    reject(periodic, unsupportedPeriodic);
    unsupportedPeriodic = periodicGuards;
    unsupportedPeriodic.periodic->spawnThreshold = 1;
    reject(periodic, unsupportedPeriodic);
    unsupportedPeriodic = periodicGuards;
    unsupportedPeriodic.periodic->rngIndexA = 1;
    reject(periodic, unsupportedPeriodic);
    unsupportedPeriodic = periodicGuards;
    unsupportedPeriodic.armor = 199;
    reject(periodic, unsupportedPeriodic);
    unsupportedPeriodic = periodicGuards;
    unsupportedPeriodic.periodic->spawnedEnemies = 12;
    reject(periodic, unsupportedPeriodic);

    state.actors.resize(1);
    state.yawVelocity = 80;
    state.forwardVelocity = 10;
    state.weaponTimers = {0, 2, 2};
    const auto edge = frameTailOperations(state, guards);
    assert(edge.state.yawVelocity == 0 && edge.state.forwardVelocity == 0);
    assert((edge.state.weaponTimers == std::array<std::uint8_t, 3>{0, 1, 1}));
    assert(dispatches(edge) == 333);
    state.yawVelocity = -80;
    state.forwardVelocity = -10;
    state.weaponTimers = {2, 0, 3};
    const auto negative = frameTailOperations(state, guards);
    assert(negative.state.yawVelocity == 0 && negative.state.forwardVelocity == 0);
    assert(dispatches(negative) == 331);
    state.yawVelocity = 79;
    state.forwardVelocity = 9;
    state.weaponTimers = {2, 3, 0};
    assert(dispatches(frameTailOperations(state, guards)) == 329);

    auto bad = guards;
    bad.heldKeys[8] = true;
    reject(state, bad);
    bad = guards;
    bad.heldKeys[7] = true;
    reject(state, bad);
    bad = guards;
    bad.abilities[3] = 1;
    reject(state, bad);
    bad = guards;
    bad.keyboardTail += 2;
    reject(state, bad);
    bad = guards;
    bad.firstFrame = true;
    reject(state, bad);
    bad = guards;
    bad.contactSlow = true;
    reject(state, bad);
    bad = guards;
    bad.success = true;
    reject(state, bad);
    bad = guards;
    bad.armor = -1;
    reject(state, bad);
    state.weaponTimers[0] = 1;
    reject(state, guards);
    state.weaponTimers[0] = 2;
    state.loopTick = 16;
    reject(state, guards);
    state.loopTick = 5;
    state.actors[0].type = 7;
    reject(state, guards);
}
