// Controlled source profile SHA256: 3af7ac9bd58e2b0e43685c9346d33107c36d4c745f483d3ca6a7f39673de2785
// Source snapshot SHA256: 0246e750a3530342909ad8a01b0f83647321c973df8d7fec755a76460dbe61b0
#include "stellar7/frame_tail_work.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#if defined(NDEBUG)
#error "Assertions must remain enabled for this test."
#endif

namespace {
using stellar7::FrameTailGuards;
using stellar7::FrameTailPeriodicInput;
using stellar7::FrameTailState;
using stellar7::NormalWorkKind;
using KindRun = std::pair<std::uint8_t, std::uint16_t>;

constexpr std::uint8_t instructionCode = 0;
constexpr std::uint8_t cliCode = 5;
constexpr std::uint8_t stiCode = 6;
constexpr std::uint8_t popFlagsEnabledCode = 7;
constexpr std::uint8_t invalidKindCode = 0xff;
// Scan order is 4f, 50, 51, 47, 48, 49, 4b, 4d, 39.
// Slot 4 is scan 48 (forward); slot 6 is scan 4b (Left).
constexpr std::size_t forwardKeySlot = 4;
constexpr std::size_t leftKeySlot = 6;
constexpr std::uint16_t emptyKeyboardRing = 0x1e;
constexpr std::uint16_t initialLoopTick = 5;
constexpr std::uint16_t expectedLoopTick = 6;

struct Fixture {
    const char* name;
    bool forwardHeld;
    std::int16_t initialForwardVelocity;
    std::int16_t initialYawVelocity;
    std::array<std::uint8_t, 3> initialTimers;
    std::size_t actorCount;
    std::int16_t expectedYawVelocity;
    std::int16_t expectedForwardVelocity;
    std::array<std::uint8_t, 3> expectedTimers;
    std::uint16_t expectedDispatches;
};

constexpr std::array<Fixture, 8> fixtures{{
    {"first-forward-released", false, 0, 0, {0, 0, 0}, 7, 0, 0, {0, 0, 0}, 463},
    {"first-forward-forward-held", true, 0, 0, {0, 0, 0}, 7, 0, 10, {0, 0, 0}, 484},
    {"exact-positive-limit-released", false, 190, 80, {0, 2, 2}, 1, 0, 180, {0, 1, 1}, 333},
    {"exact-positive-limit-forward-held", true, 190, 80, {0, 2, 2}, 1, 0, 200, {0, 1, 1}, 352},
    {"positive-clamp-released", false, 191, -80, {2, 0, 3}, 0, 0, 181, {1, 0, 2}, 310},
    {"positive-clamp-forward-held", true, 191, -80, {2, 0, 3}, 0, 0, 200, {1, 0, 2}, 337},
    {"negative-speed-yaw-released", false, -200, -512, {2, 3, 0}, 7, -432, -190, {1, 2, 0}, 469},
    {"negative-speed-yaw-forward-held", true, -200, -512, {2, 3, 0}, 7, -432, -190, {1, 2, 0}, 489},
}};

// Source ops values 0..3 map to NormalWorkKind instructions/CLI/STI/POPF
// values 0/5/6/7. Runs preserve the complete original ordered dispatch stream.
const std::array<std::vector<KindRun>, fixtures.size()> expectedSourceOps{{
    // first-forward-released; source ops run-length encoded
    std::vector<KindRun>{ {0, 41}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 11}, {5, 1}, {0, 8}, {7, 1}, {0, 10}, {5, 1}, {0, 7}, {6, 1}, {0, 261} },
    // first-forward-forward-held; source ops run-length encoded
    std::vector<KindRun>{ {0, 41}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 8}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 11}, {5, 1}, {0, 8}, {7, 1}, {0, 10}, {5, 1}, {0, 7}, {6, 1}, {0, 281} },
    // exact-positive-limit-released; source ops run-length encoded
    std::vector<KindRun>{ {0, 41}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 11}, {5, 1}, {0, 8}, {7, 1}, {0, 10}, {5, 1}, {0, 7}, {6, 1}, {0, 131} },
    // exact-positive-limit-forward-held; source ops run-length encoded
    std::vector<KindRun>{ {0, 41}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 8}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 11}, {5, 1}, {0, 8}, {7, 1}, {0, 10}, {5, 1}, {0, 7}, {6, 1}, {0, 149} },
    // positive-clamp-released; source ops run-length encoded
    std::vector<KindRun>{ {0, 41}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 11}, {5, 1}, {0, 8}, {7, 1}, {0, 10}, {5, 1}, {0, 7}, {6, 1}, {0, 108} },
    // positive-clamp-forward-held; source ops run-length encoded
    std::vector<KindRun>{ {0, 41}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 8}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 11}, {5, 1}, {0, 8}, {7, 1}, {0, 10}, {5, 1}, {0, 7}, {6, 1}, {0, 134} },
    // negative-speed-yaw-released; source ops run-length encoded
    std::vector<KindRun>{ {0, 41}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 11}, {5, 1}, {0, 8}, {7, 1}, {0, 10}, {5, 1}, {0, 7}, {6, 1}, {0, 267} },
    // negative-speed-yaw-forward-held; source ops run-length encoded
    std::vector<KindRun>{ {0, 41}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 8}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 7}, {5, 1}, {0, 7}, {6, 1}, {0, 11}, {5, 1}, {0, 8}, {7, 1}, {0, 10}, {5, 1}, {0, 7}, {6, 1}, {0, 286} },
}};

constexpr std::array<std::int16_t, 3> actorTypes{13, 8, 30};
constexpr std::array<stellar7::FixedPosition, 7> expectedActorPositions{{
    {888, -776, 0}, {988, -876, 0}, {1088, -976, 0},
    {1188, -1076, 0}, {1288, -1176, 0}, {1388, -1276, 0},
    {1488, -1376, 0}
}};

FrameTailState makeState(const Fixture& fixture) {
    FrameTailState state{
        initialLoopTick,
        {112, -224, 0},
        fixture.initialYawVelocity,
        fixture.initialForwardVelocity,
        fixture.initialTimers,
        {}};
    for (std::size_t index = 0; index < fixture.actorCount; ++index) {
        state.actors.push_back({
            actorTypes[index % actorTypes.size()],
            {1000 + static_cast<std::int32_t>(index) * 100,
                -1000 - static_cast<std::int32_t>(index) * 100, 0}});
    }
    return state;
}

FrameTailGuards makeGuards(const bool forwardHeld) {
    FrameTailGuards guards{};
    guards.armor = 680;
    guards.heldKeys[forwardKeySlot] = forwardHeld;
    guards.keyboardHead = emptyKeyboardRing;
    guards.keyboardTail = emptyKeyboardRing;
    guards.missionActive = true;
    return guards;
}

bool sameState(const FrameTailState& left, const FrameTailState& right) {
    if (left.loopTick != right.loopTick
        || !(left.playerPosition == right.playerPosition)
        || left.yawVelocity != right.yawVelocity
        || left.forwardVelocity != right.forwardVelocity
        || left.weaponTimers != right.weaponTimers
        || left.scheduler != right.scheduler
        || left.actors.size() != right.actors.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.actors.size(); ++index) {
        if (left.actors[index].type != right.actors[index].type
            || !(left.actors[index].position == right.actors[index].position)) {
            return false;
        }
    }
    return true;
}

bool samePeriodic(
    const FrameTailPeriodicInput& left,
    const FrameTailPeriodicInput& right
) {
    return left.campaignLevel == right.campaignLevel
        && left.activeEnemies == right.activeEnemies
        && left.spawnedEnemies == right.spawnedEnemies
        && left.enemyLimit == right.enemyLimit
        && left.spawnThreshold == right.spawnThreshold
        && left.rngIndexA == right.rngIndexA
        && left.rngIndexB == right.rngIndexB
        && left.enemyFlag == right.enemyFlag;
}

bool sameGuards(const FrameTailGuards& left, const FrameTailGuards& right) {
    if (left.armor != right.armor
        || left.abilities != right.abilities
        || left.heldKeys != right.heldKeys
        || left.keyboardHead != right.keyboardHead
        || left.keyboardTail != right.keyboardTail
        || left.joystickEnabled != right.joystickEnabled
        || left.firstFrame != right.firstFrame
        || left.extraUpdate != right.extraUpdate
        || left.contactSlow != right.contactSlow
        || left.success != right.success
        || left.missionActive != right.missionActive
        || left.periodic.has_value() != right.periodic.has_value()) {
        return false;
    }
    return !left.periodic || samePeriodic(*left.periodic, *right.periodic);
}

std::uint8_t sourceKindCode(const NormalWorkKind kind) {
    switch (kind) {
    case NormalWorkKind::instructions:
        return instructionCode;
    case NormalWorkKind::cli:
        return cliCode;
    case NormalWorkKind::sti:
        return stiCode;
    case NormalWorkKind::popFlagsEnabled:
        return popFlagsEnabledCode;
    default:
        assert(false && "Unexpected operation kind in frame-tail plan");
        return invalidKindCode;
    }
}

std::vector<std::uint8_t> expandOperations(
    const std::vector<stellar7::NormalWorkOperation>& operations
) {
    std::vector<std::uint8_t> expanded;
    for (const auto& operation : operations) {
        assert(operation.count > 0);
        const auto code = sourceKindCode(operation.kind);
        assert(code != invalidKindCode);
        assert(operation.count <= expanded.max_size() - expanded.size());
        expanded.insert(expanded.end(), static_cast<std::size_t>(operation.count), code);
    }
    return expanded;
}

std::vector<std::uint8_t> expandRuns(const std::vector<KindRun>& runs) {
    std::vector<std::uint8_t> expanded;
    for (const auto& [code, count] : runs) {
        assert(count > 0);
        expanded.insert(expanded.end(), count, code);
    }
    return expanded;
}

void assertReturnedState(const stellar7::FrameTailWork& work, const Fixture& fixture) {
    assert(work.state.loopTick == expectedLoopTick);
    assert(work.state.yawVelocity == fixture.expectedYawVelocity);
    assert(work.state.forwardVelocity == fixture.expectedForwardVelocity);
    assert(work.state.weaponTimers == fixture.expectedTimers);
    assert((work.state.playerPosition == stellar7::FixedPosition{0, 0, 0}));
    assert(work.state.scheduler == stellar7::FrameTailScheduler::none);
    assert(work.state.actors.size() == fixture.actorCount);
    for (std::size_t index = 0; index < fixture.actorCount; ++index) {
        assert(work.state.actors[index].type == actorTypes[index % actorTypes.size()]);
        assert(work.state.actors[index].position == expectedActorPositions[index]);
    }
}

void assertAcceptedFixtures() {
    for (std::size_t index = 0; index < fixtures.size(); ++index) {
        const auto& fixture = fixtures[index];
        auto state = makeState(fixture);
        auto guards = makeGuards(fixture.forwardHeld);
        const auto originalState = state;
        const auto originalGuards = guards;
        const auto work = stellar7::frameTailOperations(state, guards);
        const auto actualOps = expandOperations(work.operations);
        const auto expectedOps = expandRuns(expectedSourceOps[index]);
        assert(actualOps == expectedOps);
        assert(actualOps.size() == fixture.expectedDispatches);
        assertReturnedState(work, fixture);
        assert(sameState(state, originalState));
        assert(sameGuards(guards, originalGuards));
    }
}

void rejectWithoutMutation(const FrameTailState& state, const FrameTailGuards& guards) {
    const auto originalState = state;
    const auto originalGuards = guards;
    bool rejected = false;
    try {
        static_cast<void>(stellar7::frameTailOperations(state, guards));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
    assert(sameState(state, originalState));
    assert(sameGuards(guards, originalGuards));
}

void assertRejectedControls() {
    const auto baseFixture = fixtures.front();
    const auto baseState = makeState(baseFixture);
    const auto releasedGuards = makeGuards(false);

    for (std::size_t slot = 0; slot < releasedGuards.heldKeys.size(); ++slot) {
        if (slot == forwardKeySlot) {
            continue;
        }
        auto guards = releasedGuards;
        guards.heldKeys[slot] = true;
        rejectWithoutMutation(baseState, guards);
    }

    auto forwardAndLeft = makeGuards(true);
    forwardAndLeft.heldKeys[leftKeySlot] = true;
    rejectWithoutMutation(baseState, forwardAndLeft);

    auto nonemptyRing = releasedGuards;
    ++nonemptyRing.keyboardTail;
    rejectWithoutMutation(baseState, nonemptyRing);

    auto periodicState = baseState;
    periodicState.loopTick = 16;
    auto periodicForward = makeGuards(true);
    periodicForward.periodic = FrameTailPeriodicInput{1, 5, 5, 12, 0, 82, 32, 0};
    rejectWithoutMutation(periodicState, periodicForward);

    auto outOfRange = baseState;
    outOfRange.forwardVelocity = 201;
    rejectWithoutMutation(outOfRange, releasedGuards);
    outOfRange = baseState;
    outOfRange.forwardVelocity = -201;
    rejectWithoutMutation(outOfRange, releasedGuards);
    outOfRange = baseState;
    outOfRange.yawVelocity = 513;
    rejectWithoutMutation(outOfRange, releasedGuards);
    outOfRange = baseState;
    outOfRange.yawVelocity = -513;
    rejectWithoutMutation(outOfRange, releasedGuards);

    auto fire = releasedGuards;
    fire.heldKeys[8] = true;
    rejectWithoutMutation(baseState, fire);

    auto unsupportedActor = baseState;
    unsupportedActor.actors.front().type = 7;
    rejectWithoutMutation(unsupportedActor, releasedGuards);
}
}

int main() {
    assertAcceptedFixtures();
    assertRejectedControls();
}





