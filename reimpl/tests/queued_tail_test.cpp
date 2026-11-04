#include "stellar7/frame_tail_work.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#ifdef NDEBUG
#error "queued_tail_test requires assertions; compile without NDEBUG"
#endif

namespace {
using namespace stellar7;
using InstructionStream = std::vector<NormalWorkKind>;
enum class RingReadPath { linear, wrap };

constexpr std::size_t forwardHeldKeyIndex = 4;
constexpr std::size_t fixtureActorCount = 7;
constexpr std::size_t residentHeldKeyQueryCount = 8;
constexpr std::size_t forwardQueryIndex = 4;
constexpr std::size_t scanLookupEntryCount = 18;
constexpr std::uint16_t initialKeyboardHead = 0x36;
constexpr std::uint16_t queuedKeyboardTail = 0x38;
constexpr std::uint16_t wrappedKeyboardHead = 0x3c;
constexpr std::uint16_t wrappedKeyboardTail = 0x1e;
constexpr std::uint16_t forwardMakeScan = 0x48;
constexpr std::uint16_t unsupportedHead = 0x40;
constexpr std::uint16_t unsupportedTail = 0x42;
constexpr std::uint16_t twoWordTail = 0x3a;
constexpr std::uint16_t oddHead = 0x37;
constexpr std::uint16_t oddTail = 0x39;
constexpr std::size_t expectedQueuedDispatches = 653;
constexpr std::size_t expectedEmptyForwardDispatches = 484;
constexpr std::size_t expectedReleaseFromTenDispatches = 465;
constexpr std::size_t expectedReleaseFromZeroDispatches = 463;

void appendInstructions(InstructionStream& stream, const std::uint64_t count) {
    for (std::uint64_t index = 0; index < count; ++index) {
        stream.push_back(NormalWorkKind::instructions);
    }
}

void appendKeyQuery(InstructionStream& stream) {
    stream.push_back(NormalWorkKind::cli);
    appendInstructions(stream, 7); // Resident key query body, 0438..0447.
    stream.push_back(NormalWorkKind::sti);
    appendInstructions(stream, 1); // RETF follows the STI shadow.
}

void appendDamp(InstructionStream& stream, const std::int16_t velocity,
    const std::int16_t quantum) {
    const auto magnitude = velocity < 0 ? -static_cast<int>(velocity)
        : static_cast<int>(velocity);
    if (magnitude < quantum) {
        appendInstructions(stream, 2);
        return;
    }
    if (velocity > 0) {
        appendInstructions(stream, 4);
        return;
    }
    appendInstructions(stream, 3);
}

void appendQueuedScanPath(InstructionStream& stream) {
    appendInstructions(stream, 3); // 9ecd..9ed2: cleanup, OR, branch.
    appendInstructions(stream, 2); // 9ed7..9edc: scan-present test.
    appendInstructions(stream, 3); // 9ede..9ee2: Alt scan query call.
    appendKeyQuery(stream); // 9a:0437 query, CLI/STI and RETF.
    appendInstructions(stream, 3); // 9ee7..9eec: cleanup, OR, branch.
    appendInstructions(stream, 1); // 9f0a: clear table index.
    appendInstructions(stream, 1); // 9f0c: load first scan-table entry.
    for (std::size_t entry = 0; entry < scanLookupEntryCount; ++entry) {
        appendInstructions(stream, 3); // 9f0f..9f15: load, compare, match branch.
        appendInstructions(stream, 5); // 9f17..9f1e: unmatched entry advance/test.
    }
    appendInstructions(stream, 1); // 9f20: leave unmatched scan lookup.
    appendInstructions(stream, 4); // 9f2f..9f3d: slowdown and direction tests.
}

InstructionStream expectedTailStream(const bool forwardHeld, const bool queuedForward,
    const std::int16_t forwardVelocity, const std::int16_t yawVelocity,
    const RingReadPath ringPath = RingReadPath::linear) {
    InstructionStream stream;
    appendInstructions(stream, 4); // 0312 CALL and loop guard.
    appendInstructions(stream, 18); // Timer clearing and grounded height.
    appendInstructions(stream, 5); // Extra/first-frame checks and 9e10 call.
    appendInstructions(stream, 9); // 9e10..9e1e, including 8bd0.
    appendInstructions(stream, 2); // 8bd0 setup.

    for (std::size_t query = 0; query < residentHeldKeyQueryCount; ++query) {
        appendInstructions(stream, 3); // Scan argument and far call.
        appendKeyQuery(stream);
        appendInstructions(stream, 3); // Stack cleanup and result branch.
        if (forwardHeld && query == forwardQueryIndex) {
            appendInstructions(stream, 1); // 8c2f: forward scan bit.
        }
    }

    appendInstructions(stream, 3); // 8c6b..8c6e.
    appendInstructions(stream, 3); // 9e21..9e27; far call to the ring reader.
    appendInstructions(stream, 1); // PUSHF before the BIOS ring read.
    stream.push_back(NormalWorkKind::cli);
    appendInstructions(stream, queuedForward ? (ringPath == RingReadPath::wrap ? 15 : 14) : 8); // 0410 executes at wrap.
    stream.push_back(NormalWorkKind::popFlagsEnabled);
    appendInstructions(stream, 1); // 041c RETF.
    appendInstructions(stream, 6); // 9e2c..9e39; joystick disabled.
    appendInstructions(stream, 3); // 9ec4..9ec8; space query call.
    appendKeyQuery(stream);

    if (queuedForward) {
        appendQueuedScanPath(stream);
    } else if (forwardHeld) {
        appendInstructions(stream, 9); // 9ecd..9f3d with no queued scan.
    } else {
        appendInstructions(stream, 10); // 9ecd..9f3f.
        appendInstructions(stream, 11); // a052..a06d.
        appendDamp(stream, forwardVelocity, 10);
        appendInstructions(stream, 9); // a083..a097.
        appendDamp(stream, yawVelocity, 80);
    }

    if (forwardHeld) {
        appendInstructions(stream, 6); // 9f42..9f76; no yaw or reverse keys.
        appendInstructions(stream, 9); // 9f82..9f96.
        appendDamp(stream, yawVelocity, 80);
        appendInstructions(stream, 9); // 9fab..9fbf.
        appendInstructions(stream, 10); // 9fd5..a020; add forward velocity.
        appendInstructions(stream, 9); // a024..a038; velocity bound check.
    }
    appendInstructions(stream, 9); // a0ad..a0ec; return from input control.
    appendInstructions(stream, 6); // 0332 CALL and actor loop setup.
    appendInstructions(stream, 1); // 0c77: first actor setup.
    for (std::size_t actor = 0; actor < fixtureActorCount; ++actor) {
        appendInstructions(stream, actor + 1 == fixtureActorCount ? 22 : 23);
    }
    appendInstructions(stream, 15); // Player recenter and return.
    appendInstructions(stream, 9); // Nonperiodic cooldown path.
    appendInstructions(stream, 2); // Timer slot zero unchanged.
    for (std::size_t slot = 1; slot < 3; ++slot) {
        appendInstructions(stream, 6); // Timer slot check.
        appendInstructions(stream, 3); // Timer slot unchanged.
    }
    appendInstructions(stream, 6); // Tick increment and loop jump.
    return stream;
}

InstructionStream expand(const FrameTailWork& work) {
    InstructionStream stream;
    for (const auto& operation : work.operations) {
        for (std::uint64_t dispatch = 0; dispatch < operation.count; ++dispatch) {
            stream.push_back(operation.kind);
        }
    }
    return stream;
}

void assertExactStream(const FrameTailWork& work, const InstructionStream& expected,
    const std::size_t expectedDispatches) {
    assert(expected.size() == expectedDispatches);
    assert(expand(work) == expected);
}

FrameTailState fixtureState(const std::uint16_t loopTick) {
    FrameTailState state{loopTick, {112, -224, 0}, 0, 0, {}, {}};
    for (std::size_t actor = 0; actor < fixtureActorCount; ++actor) {
        const auto offset = static_cast<std::int32_t>(actor) * 100;
        state.actors.push_back({13, {1000 + offset, -1000 - offset, 0}});
    }
    return state;
}

FrameTailGuards emptyQueueGuards(const std::uint16_t head) {
    FrameTailGuards guards{};
    guards.armor = 680;
    guards.keyboardHead = head;
    guards.keyboardTail = head;
    guards.missionActive = true;
    return guards;
}

FrameTailGuards forwardMakeGuards() {
    auto guards = emptyQueueGuards(initialKeyboardHead);
    guards.keyboardTail = queuedKeyboardTail;
    guards.keyboardQueue = FrameTailKeyboardQueue::forwardMake;
    guards.heldKeys[forwardHeldKeyIndex] = true;
    return guards;
}

bool sameState(const FrameTailState& left, const FrameTailState& right) {
    if (left.loopTick != right.loopTick
        || left.playerPosition.x != right.playerPosition.x
        || left.playerPosition.y != right.playerPosition.y
        || left.playerPosition.z != right.playerPosition.z
        || left.yawVelocity != right.yawVelocity
        || left.forwardVelocity != right.forwardVelocity
        || left.weaponTimers != right.weaponTimers
        || left.scheduler != right.scheduler
        || left.actors.size() != right.actors.size()) {
        return false;
    }
    for (std::size_t actor = 0; actor < left.actors.size(); ++actor) {
        if (left.actors[actor].type != right.actors[actor].type
            || left.actors[actor].position.x != right.actors[actor].position.x
            || left.actors[actor].position.y != right.actors[actor].position.y
            || left.actors[actor].position.z != right.actors[actor].position.z) {
            return false;
        }
    }
    return true;
}

bool samePeriodicInput(const std::optional<FrameTailPeriodicInput>& left,
    const std::optional<FrameTailPeriodicInput>& right) {
    if (left.has_value() != right.has_value()) {
        return false;
    }
    if (!left) {
        return true;
    }
    return left->campaignLevel == right->campaignLevel
        && left->activeEnemies == right->activeEnemies
        && left->spawnedEnemies == right->spawnedEnemies
        && left->enemyLimit == right->enemyLimit
        && left->spawnThreshold == right->spawnThreshold
        && left->rngIndexA == right->rngIndexA
        && left->rngIndexB == right->rngIndexB
        && left->enemyFlag == right->enemyFlag;
}

bool sameGuards(const FrameTailGuards& left, const FrameTailGuards& right) {
    return left.armor == right.armor
        && left.abilities == right.abilities
        && left.heldKeys == right.heldKeys
        && left.keyboardHead == right.keyboardHead
        && left.keyboardTail == right.keyboardTail
        && left.keyboardQueue == right.keyboardQueue
        && left.joystickEnabled == right.joystickEnabled
        && left.firstFrame == right.firstFrame
        && left.extraUpdate == right.extraUpdate
        && left.contactSlow == right.contactSlow
        && left.success == right.success
        && left.missionActive == right.missionActive
        && samePeriodicInput(left.periodic, right.periodic);
}

void rejectWithoutMutation(const FrameTailState& state, const FrameTailGuards& guards) {
    const auto stateBefore = state;
    const auto guardsBefore = guards;
    bool rejected = false;
    try {
        static_cast<void>(frameTailOperations(state, guards));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
    assert(sameState(state, stateBefore));
    assert(sameGuards(guards, guardsBefore));
}

void assertForwardState(const FrameTailState& state, const std::uint16_t loopTick,
    const std::int16_t velocity) {
    assert(state.loopTick == loopTick);
    assert(state.playerPosition.x == 0);
    assert(state.playerPosition.y == 0);
    assert(state.playerPosition.z == 0);
    assert(state.yawVelocity == 0);
    assert(state.forwardVelocity == velocity);
    assert((state.weaponTimers == std::array<std::uint8_t, 3>{0, 0, 0}));
    assert(state.actors.size() == fixtureActorCount);
    for (std::size_t actor = 0; actor < fixtureActorCount; ++actor) {
        const auto offset = static_cast<std::int32_t>(actor) * 100;
        assert(state.actors[actor].type == 13);
        assert(state.actors[actor].position.x == 888 + offset);
        assert(state.actors[actor].position.y == -776 - offset);
        assert(state.actors[actor].position.z == 0);
    }
}

void rejectUnsupportedQueueStates() {
    const auto state = fixtureState(4);
    const auto valid = forwardMakeGuards();

    auto invalid = valid;
    invalid.heldKeys[forwardHeldKeyIndex] = false;
    rejectWithoutMutation(state, invalid);

    invalid = valid;
    invalid.keyboardHead = initialKeyboardHead;
    invalid.keyboardTail = initialKeyboardHead;
    rejectWithoutMutation(state, invalid);

    invalid = emptyQueueGuards(initialKeyboardHead);
    invalid.keyboardTail = queuedKeyboardTail;
    rejectWithoutMutation(state, invalid);

    invalid = valid;
    invalid.keyboardHead = oddHead;
    invalid.keyboardTail = oddTail;
    rejectWithoutMutation(state, invalid);

    invalid = valid;
    invalid.keyboardHead = unsupportedHead;
    invalid.keyboardTail = unsupportedTail;
    rejectWithoutMutation(state, invalid);

    invalid = valid;
    invalid.keyboardHead = wrappedKeyboardHead;
    invalid.keyboardTail = wrappedKeyboardTail + 2;
    rejectWithoutMutation(state, invalid);

    invalid = valid;
    invalid.keyboardTail = twoWordTail;
    rejectWithoutMutation(state, invalid);

    invalid = valid;
    invalid.keyboardQueue = static_cast<FrameTailKeyboardQueue>(2);
    rejectWithoutMutation(state, invalid);

    auto periodicState = fixtureState(16);
    auto periodicGuards = valid;
    periodicGuards.periodic = FrameTailPeriodicInput{1, 5, 5, 12, 0, 82, 32, 0};
    rejectWithoutMutation(periodicState, periodicGuards);
}
}

int main() {
    using namespace stellar7;

    auto emptyForwardState = fixtureState(4);
    auto emptyForwardGuards = emptyQueueGuards(initialKeyboardHead);
    emptyForwardGuards.heldKeys[forwardHeldKeyIndex] = true;
    const auto emptyForward = frameTailOperations(emptyForwardState, emptyForwardGuards);
    assertExactStream(emptyForward,
        expectedTailStream(true, false, 0, 0), expectedEmptyForwardDispatches);
    assertForwardState(emptyForward.state, 5, 10);
    assert(emptyForward.keyboardHead == initialKeyboardHead);
    assert(emptyForward.consumedScan == 0);
    assert(sameState(emptyForwardState, fixtureState(4)));

    auto state = fixtureState(4);
    auto guards = forwardMakeGuards();
    const auto queued = frameTailOperations(state, guards);
    assertExactStream(queued,
        expectedTailStream(true, true, 0, 0), expectedQueuedDispatches);
    assertForwardState(queued.state, 5, 10);
    assert(queued.keyboardHead == queuedKeyboardTail);
    assert(queued.consumedScan == forwardMakeScan);
    assert(sameState(state, fixtureState(4)));
    assert(guards.keyboardHead == initialKeyboardHead);
    assert(guards.keyboardTail == queuedKeyboardTail);

    auto wrappedGuards = guards;
    wrappedGuards.keyboardHead = wrappedKeyboardHead;
    wrappedGuards.keyboardTail = wrappedKeyboardTail;
    const auto wrapped = frameTailOperations(state, wrappedGuards);
    assertExactStream(wrapped, expectedTailStream(true, true, 0, 0, RingReadPath::wrap), expectedQueuedDispatches + 1);
    assert(wrapped.keyboardHead == wrappedKeyboardTail && wrapped.consumedScan == forwardMakeScan);
    assert(sameState(wrapped.state, queued.state));
    wrappedGuards.sourceKeyboardEffects = true;
    const auto sourceWrapped = frameTailOperations(state, wrappedGuards);
    bool consumed = false;
    for (std::size_t index = 1; index < sourceWrapped.operations.size(); ++index) {
        if (sourceWrapped.operations[index].kind == NormalWorkKind::keyboardConsumeHead) {
            assert(sourceWrapped.operations[index - 1].kind == NormalWorkKind::instructions);
            assert(sourceWrapped.operations[index - 1].count == 13); //03f0..040e plus0410, before0415 effect.
            consumed = true;
        }
    }
    assert(consumed && expand(sourceWrapped).size() == expectedQueuedDispatches + 1);

    state = queued.state;
    // Synthetic release is the fixture's no-IRQ credit from source qualification.
    guards = emptyQueueGuards(queued.keyboardHead);
    guards.heldKeys[forwardHeldKeyIndex] = false;
    const auto releasedAtTickFive = frameTailOperations(state, guards);
    assertExactStream(releasedAtTickFive,
        expectedTailStream(false, false, 10, 0), expectedReleaseFromTenDispatches);
    assertForwardState(releasedAtTickFive.state, 6, 0);
    assert(releasedAtTickFive.keyboardHead == queuedKeyboardTail);
    assert(releasedAtTickFive.consumedScan == 0);

    state = releasedAtTickFive.state;
    guards.keyboardHead = releasedAtTickFive.keyboardHead;
    guards.keyboardTail = releasedAtTickFive.keyboardHead;
    const auto releasedAtTickSix = frameTailOperations(state, guards);
    assertExactStream(releasedAtTickSix,
        expectedTailStream(false, false, 0, 0), expectedReleaseFromZeroDispatches);
    assertForwardState(releasedAtTickSix.state, 7, 0);
    assert(releasedAtTickSix.keyboardHead == queuedKeyboardTail);
    assert(releasedAtTickSix.consumedScan == 0);

    rejectUnsupportedQueueStates();
}
