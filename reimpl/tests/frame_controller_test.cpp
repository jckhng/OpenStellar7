#include "stellar7/frame_controller.hpp"

#include <cassert>
#include <stdexcept>

namespace {
template<class Function>
void reject(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}
}

int main() {
    using namespace stellar7;
    FrameControllerInitial initial{};
    initial.origin = FrameControllerOrigin::restoredDiagnosticInitial;
    initial.loopTick = 0xffff;
    initial.keyboardHead = initial.keyboardTail = 0x36;
    FrameController owner;
    reject([&] { owner.augmentGuards({}); });
    reject([&] { owner.consumeEmptyTail(0); });
    reject([&] { owner.delivery(FrameControllerDelivery::keyboard); });
    reject([&] { owner.expectedLoopTick(); });
    reject([&] { owner.origin(); });
    owner.initialize(initial);
    assert(owner.origin() == FrameControllerOrigin::restoredDiagnosticInitial);
    FrameTailGuards guards{};
    guards.armor = 680;
    guards.firstFrame = guards.contactSlow = guards.success = guards.missionActive = true;
    guards.heldKeys.fill(true);
    guards.abilities.fill(1);
    guards.joystickEnabled = guards.extraUpdate = true;
    auto retained = owner.augmentGuards(guards);
    assert(retained.keyboardHead == 0x36 && retained.keyboardTail == 0x36);
    assert(retained.armor == 680 && retained.firstFrame && retained.contactSlow &&
           retained.success && retained.missionActive);
    for (auto held : retained.heldKeys) { assert(!held); }
    for (auto timer : retained.abilities) { assert(timer == 0); }
    assert(!retained.joystickEnabled && !retained.extraUpdate);
    reject([&] { owner.consumeEmptyTail(0); });
    owner.consumeEmptyTail(0xffff);
    reject([&] { owner.consumeEmptyTail(0xffff); });
    owner.consumeEmptyTail(0);
    assert(owner.expectedLoopTick() == 1);
    assert(owner.augmentGuards({}).keyboardHead == 0x36);
    FrameTailGuards admitted{};
    admitted.armor = 680;
    admitted.missionActive = true;
    FrameTailState tail{};
    tail.loopTick = 1;
    const auto work = frameTailOperations(tail, owner.augmentGuards(admitted));
    assert(work.state.loopTick == 2);
    owner.consumeEmptyTail(tail.loopTick);
    assert(owner.expectedLoopTick() == work.state.loopTick);
    reject([&] { owner.initialize(initial); });

    for (auto event : {FrameControllerDelivery::keyboard, FrameControllerDelivery::configuration,
                       FrameControllerDelivery::specialAction}) {
        FrameController invalidated;
        invalidated.initialize(initial);
        invalidated.delivery(event);
        reject([&] { invalidated.augmentGuards({}); });
        reject([&] { invalidated.consumeEmptyTail(0xffff); });
        reject([&] { invalidated.initialize(initial); });
    }

    auto rejectInitial = [](const FrameControllerInitial& input) {
        FrameController fresh;
        reject([&] { fresh.initialize(input); });
    };
    for (std::size_t index = 0; index < initial.heldKeys.size(); ++index) {
        auto active = initial; active.heldKeys[index] = true; rejectInitial(active);
    }
    for (std::size_t index = 0; index < initial.abilities.size(); ++index) {
        auto active = initial; active.abilities[index] = 1; rejectInitial(active);
    }
    auto active = initial; active.queuedScanBytes = 1; rejectInitial(active);
    active = initial; active.repeatActive = true; rejectInitial(active);
    active = initial; active.transferScheduled = true; rejectInitial(active);
    active = initial; active.port60Pending = true; rejectInitial(active);
    active = initial; active.joystickEnabled = true; rejectInitial(active);
    active = initial; active.extraUpdate = true; rejectInitial(active);
    active = initial; active.keyboardTail = 0x38; rejectInitial(active);
    active = initial; active.origin = static_cast<FrameControllerOrigin>(99); rejectInitial(active);
    for (auto head : {0x1c, 0x1f, 0x3e}) {
        active = initial; active.keyboardHead = active.keyboardTail = head; rejectInitial(active);
    }
    active = initial; active.origin = FrameControllerOrigin::sourceColdDefaults; rejectInitial(active);
    active.keyboardHead = active.keyboardTail = 0x1e;
    FrameController cold;
    cold.initialize(active);
    assert(cold.augmentGuards({}).keyboardHead == 0x1e);

    // Resident input is imported once at the initial returned-render boundary.
    FrameController forward;
    reject([&] { forward.restoreForwardAtRenderReturn(); });
    auto arrival = initial;
    arrival.loopTick = 4;
    forward.initialize(arrival);
    forward.restoreForwardAtRenderReturn();
    reject([&] { forward.restoreForwardAtRenderReturn(); });
    const auto held = forward.augmentGuards({});
    assert(held.keyboardHead == 0x36 && held.keyboardTail == 0x36);
    for (std::size_t i = 0; i < held.heldKeys.size(); ++i) {
        assert(held.heldKeys[i] == (i == 4));
    }
    forward.consumeEmptyTail(4);
    assert(forward.augmentGuards({}).heldKeys[4] && forward.expectedLoopTick() == 5);
    forward.delivery(FrameControllerDelivery::keyboard);
    reject([&] { forward.augmentGuards({}); });
    FrameController early;
    arrival.loopTick = 3;
    early.initialize(arrival);
    reject([&] { early.restoreForwardAtRenderReturn(); });
    assert(!early.augmentGuards({}).heldKeys[4]);
    FrameController coldForward;
    arrival.origin = FrameControllerOrigin::sourceColdDefaults;
    arrival.loopTick = 4;
    arrival.keyboardHead = arrival.keyboardTail = 0x1e;
    coldForward.initialize(arrival);
    reject([&] { coldForward.restoreForwardAtRenderReturn(); });
    assert(!coldForward.augmentGuards({}).heldKeys[4]);

    initial.loopTick = 0;
    FrameController restored;
    restored.initialize(initial);
    for (std::uint16_t tick = 0; tick < 16; ++tick) {
        assert(restored.expectedLoopTick() == tick);
        restored.consumeEmptyTail(tick);
        const auto empty = restored.augmentGuards({});
        assert(empty.keyboardHead == 0x36 && empty.keyboardTail == 0x36);
    }

    FrameController native;
    FrameControllerInitial nativeInitial{};
    nativeInitial.origin = FrameControllerOrigin::sourceColdDefaults;
    nativeInitial.keyboardHead = nativeInitial.keyboardTail = 0x1e;
    native.initialize(nativeInitial);
    std::array<bool, 9> movingKeys{};
    movingKeys[4] = true; //Up.
    movingKeys[6] = true; //Left.
    const auto preview = native.nativeGuards({},movingKeys);
    assert(preview.heldKeys == movingKeys && preview.keyboardHead == preview.keyboardTail);
    assert(!preview.sourceKeyboardEffects && native.expectedLoopTick() == 0);
    for (const auto heldKey : native.augmentGuards({}).heldKeys) { assert(!heldKey); }
    Level1FrameTailWork completed{{},StellarRng{},{},{},false,0x1e,0};
    completed.state.loopTick = 1;
    auto badTail = completed;
    badTail.consumedScan = 0x48;
    reject([&] { native.completeNativeTail(0,badTail,movingKeys); });
    assert(native.expectedLoopTick() == 0 && !native.augmentGuards({}).heldKeys[4]);
    native.completeNativeTail(0,completed,movingKeys);
    assert(native.expectedLoopTick() == 1 && native.augmentGuards({}).heldKeys == movingKeys);
    assert((native.keyboardState().metadata48 & 1) != 0 && native.keyboardState().head == 0x1e);
    reject([&] { native.completeNativeTail(0,completed,movingKeys); });
    completed.state.loopTick = 2;
    native.completeNativeTail(1,completed,{});
    for (const auto heldKey : native.augmentGuards({}).heldKeys) { assert(!heldKey); }
    assert((native.keyboardState().metadata48 & 1) == 0 && native.expectedLoopTick() == 2);
    reject([&] { restored.nativeGuards({},movingKeys); });
}
