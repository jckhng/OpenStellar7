#include "stellar7/frame_controller.hpp"

#include <algorithm>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::uint16_t keyboardRingBegin = 0x1e;
constexpr std::uint16_t keyboardRingEnd = 0x3e;
constexpr std::uint16_t keyboardEntryBytes = 2;
constexpr std::uint16_t initialRenderReturnTick = 4;
constexpr std::uint8_t releasedUpMetadata = 0x70;
constexpr std::uint8_t releasedAltMetadata = 0x00;
constexpr std::uint8_t keyDownBit = 0x01;
constexpr std::uint16_t forwardMakeWord = 0x4800;
constexpr std::uint16_t forwardMakeScan = 0x48;
constexpr std::size_t forwardKeyIndex = 4;

bool validRingOffset(const std::uint16_t value) {
    return value >= keyboardRingBegin && value < keyboardRingEnd
        && (value - keyboardRingBegin) % keyboardEntryBytes == 0;
}

std::size_t ringIndex(const std::uint16_t value) {
    return static_cast<std::size_t>((value - keyboardRingBegin) / keyboardEntryBytes);
}
}

void FrameController::initialize(const FrameControllerInitial& initial) {
    if (initialized_) {
        throw std::invalid_argument("frame controller already initialized");
    }
    if (initial.origin != FrameControllerOrigin::sourceColdDefaults &&
        initial.origin != FrameControllerOrigin::restoredDiagnosticInitial) {
        throw std::invalid_argument("unsupported controller origin");
    }
    const auto head = initial.keyboardHead;
    if (head != initial.keyboardTail || !validRingOffset(head) ||
        (initial.origin == FrameControllerOrigin::sourceColdDefaults && head != keyboardRingBegin)) {
        throw std::invalid_argument("controller requires an admitted empty keyboard ring");
    }
    if (std::any_of(initial.heldKeys.begin(), initial.heldKeys.end(), [](bool held) { return held; }) ||
        initial.queuedScanBytes != 0 || initial.repeatActive || initial.transferScheduled ||
        initial.port60Pending || initial.joystickEnabled || initial.extraUpdate || initial.biosModifierFlags != 0
        || initial.keyRemap != 0 || initial.enhancedMode != 0 ||
        std::any_of(initial.abilities.begin(), initial.abilities.end(),
                    [](std::uint16_t timer) { return timer != 0; })) {
        throw std::invalid_argument("unsupported active controller state");
    }

    keyboard_ = {};
    keyboard_.metadata48 = releasedUpMetadata;
    keyboard_.metadataAlt38 = releasedAltMetadata;
    keyboard_.head = head;
    keyboard_.tail = initial.keyboardTail;
    keyboard_.ringBegin = keyboardRingBegin;
    keyboard_.ringEnd = keyboardRingEnd;
    keyboard_.slots = initial.keyboardSlots;
    nextLoopTick_ = initial.loopTick;
    origin_ = initial.origin;
    initialized_ = true;
}

bool FrameController::forwardHeld() const {
    return (keyboard_.metadata48 & keyDownBit) != 0;
}

void FrameController::restoreForwardAtRenderReturn() {
    requireAdmission();
    if (keyboardBound_ || origin_ != FrameControllerOrigin::restoredDiagnosticInitial ||
        nextLoopTick_ != initialRenderReturnTick || forwardHeld()) {
        throw std::invalid_argument("resident forward requires one diagnostic raw4 return");
    }
    keyboard_.metadata48 = static_cast<std::uint8_t>(keyboard_.metadata48 | keyDownBit);
}

void FrameController::requireAdmission() const {
    if (!initialized_ || invalidated_) {
        throw std::invalid_argument("frame controller is outside empty-input admission");
    }
}

FrameTailGuards FrameController::augmentGuards(FrameTailGuards guards) const {
    requireAdmission();
    guards.heldKeys.fill(false);
    for (std::size_t index = 0; index < guards.heldKeys.size(); ++index) {
        if (index == forwardKeyIndex) { continue; }
        guards.heldKeys[index] = nativeOtherKeys_[index < forwardKeyIndex ? index : index - 1];
    }
    guards.heldKeys[forwardKeyIndex] = forwardHeld();
    guards.abilities.fill(0);
    guards.keyboardHead = keyboard_.head;
    guards.keyboardTail = keyboard_.tail;
    guards.joystickEnabled = false;
    guards.extraUpdate = false;
    guards.sourceKeyboardEffects = keyboardBound_;
    guards.sourceKeyboardEffects = keyboardBound_;

    if (keyboard_.head == keyboard_.tail) {
        guards.keyboardQueue = FrameTailKeyboardQueue::empty;
        return guards;
    }
    const auto nextHead = keyboard_.head == keyboardRingEnd - keyboardEntryBytes
        ? keyboardRingBegin : static_cast<std::uint16_t>(keyboard_.head + keyboardEntryBytes);
    if (!forwardHeld() || !validRingOffset(keyboard_.head) ||
        !validRingOffset(keyboard_.tail) || keyboard_.tail != nextHead ||
        keyboard_.slots[ringIndex(keyboard_.head)] != forwardMakeWord) {
        throw std::invalid_argument("unsupported resident keyboard queue");
    }
    guards.keyboardQueue = FrameTailKeyboardQueue::forwardMake;
    return guards;
}

FrameTailGuards FrameController::nativeGuards(FrameTailGuards guards,
    const std::array<bool, 9>& keys) const {
    requireAdmission();
    if (keyboardBound_ || origin_ != FrameControllerOrigin::sourceColdDefaults
        || keyboard_.head != keyboard_.tail) {
        throw std::invalid_argument("native keys require an unbound empty native controller");
    }
    guards = augmentGuards(guards);
    guards.heldKeys = keys;
    return guards;
}

void FrameController::completeNativeTail(const std::uint16_t loopTick,
    const Level1FrameTailWork& work, const std::array<bool, 9>& keys) {
    completeNativeTail(loopTick,work.state.loopTick,work.keyboardHead,work.consumedScan,keys);
}

void FrameController::completeNativeTail(const std::uint16_t loopTick,
    const FrameTailWork& work, const std::array<bool, 9>& keys) {
    completeNativeTail(loopTick,work.state.loopTick,work.keyboardHead,work.consumedScan,keys);
}

void FrameController::completeNativeTail(const std::uint16_t loopTick,
    const std::uint16_t returnedLoopTick, const std::uint16_t keyboardHead,
    const std::uint16_t consumedScan, const std::array<bool, 9>& keys) {
    nativeGuards({},keys);
    const auto nextTick = static_cast<std::uint16_t>(loopTick + 1);
    if (loopTick != nextLoopTick_ || returnedLoopTick != nextTick
        || keyboardHead != keyboard_.head || consumedScan != 0) {
        throw std::invalid_argument("native tail requires the next completed empty-input read");
    }
    for (std::size_t index = 0; index < keys.size(); ++index) {
        if (index == forwardKeyIndex) { continue; }
        nativeOtherKeys_[index < forwardKeyIndex ? index : index - 1] = keys[index];
    }
    keyboard_.metadata48 = static_cast<std::uint8_t>((keyboard_.metadata48 & ~keyDownBit)
        | (keys[forwardKeyIndex] ? keyDownBit : 0));
    keyboard_.lastKey = 0;
    keyboard_.consumedScan = 0;
    nextLoopTick_ = nextTick;
}

void FrameController::restartNativeLoop() {
    nativeGuards({},{});
    nextLoopTick_ = 0;
}

void FrameController::consumeEmptyTail(const std::uint16_t loopTick) {
    requireAdmission();
    if (keyboardBound_ || loopTick != nextLoopTick_) {
        throw std::invalid_argument("empty keyboard tail consumed out of order");
    }
    //03ee returns zero without moving head when the queue is empty.
    nextLoopTick_ = static_cast<std::uint16_t>(nextLoopTick_ + 1);
}

void FrameController::completeTail(const std::uint16_t loopTick, const FrameTailWork& work) {
    requireAdmission();
    const auto nextTick = static_cast<std::uint16_t>(loopTick + 1);
    if (!keyboardBound_ || loopTick != nextLoopTick_ || work.state.loopTick != nextTick ||
        work.keyboardHead != keyboard_.head || work.consumedScan != keyboard_.consumedScan) {
        throw std::invalid_argument("source keyboard tail effects were not committed in order");
    }
    nextLoopTick_ = nextTick;
}

void FrameController::delivery(const FrameControllerDelivery event) {
    if (!initialized_) {
        throw std::invalid_argument("frame controller not initialized");
    }
    switch (event) {
    case FrameControllerDelivery::keyboard:
    case FrameControllerDelivery::configuration:
    case FrameControllerDelivery::specialAction:
        invalidated_ = true;
        return;
    }
    throw std::invalid_argument("unsupported controller delivery");
}

FrameControllerKeyboardState FrameController::keyboardState() const {
    requireAdmission();
    return keyboard_;
}

std::uint16_t FrameController::expectedLoopTick() const {
    requireAdmission();
    return nextLoopTick_;
}

FrameControllerOrigin FrameController::origin() const {
    requireAdmission();
    return origin_;
}

} // namespace stellar7
