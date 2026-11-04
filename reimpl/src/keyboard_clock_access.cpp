#include "keyboard_clock_access.hpp"

#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::uint16_t ringBegin = 0x1e;
constexpr std::uint16_t ringEnd = 0x3e;
constexpr std::uint16_t entryBytes = 2;
constexpr std::uint16_t upWord = 0x4800;
constexpr std::uint16_t firstInputTick = 4;
constexpr std::uint16_t lastRingOffset = ringEnd - entryBytes;
constexpr std::uint8_t releasedUp = 0x70;
constexpr std::uint8_t heldUp = 0x71;
constexpr std::uint8_t upRole = 7;

std::size_t slot(const std::uint16_t offset) {
    return (offset - ringBegin) / entryBytes;
}
}

KeyboardClockAccess::KeyboardClockAccess(FrameController& owner) : owner_(owner) {
    owner_.requireAdmission();
    if (owner_.keyboardBound_ || owner_.origin_ != FrameControllerOrigin::restoredDiagnosticInitial
        || owner_.nextLoopTick_ != firstInputTick || owner_.keyboard_.head != 0x36
        || owner_.keyboard_.tail != 0x36 || owner_.keyboard_.metadata48 != releasedUp
        || owner_.keyboard_.axisRoles != std::array<std::uint8_t,2>{} || owner_.keyboard_.lastKey != 0) {
        throw std::invalid_argument("physical controller binding requires released diagnostic raw4");
    }
    owner_.keyboardBound_ = true;
    validate();
}

void KeyboardClockAccess::validate() const {
    owner_.requireAdmission();
    const auto& state = owner_.keyboard_;
    if (!owner_.keyboardBound_ || state.ringBegin != ringBegin || state.ringEnd != ringEnd
        || state.metadataAlt38 != 0 || (state.metadata48 != releasedUp && state.metadata48 != heldUp)
        || (state.axisRoles[0] != 0 && state.axisRoles[0] != upRole) || state.axisRoles[1] != 0
        || (state.lastKey != 0 && state.lastKey != upWord)
        || state.head < ringBegin || state.head >= ringEnd || state.head % entryBytes != 0
        || state.tail < ringBegin || state.tail >= ringEnd || state.tail % entryBytes != 0) {
        throw std::invalid_argument("unsupported resident keyboard owner domain");
    }
}

void KeyboardClockAccess::validateScan(const std::uint8_t scan) const {
    validate();
    if (scan == 0xe0) {
        return;
    }
    const auto& state = owner_.keyboard_;
    const auto initialMake = state.metadata48 == releasedUp && state.axisRoles[0] == 0 && state.lastKey == 0;
    const auto admittedRepeat = state.metadata48 == heldUp && state.axisRoles[0] == upRole
        && state.lastKey == upWord;
    if ((scan == 0x48 && (initialMake || admittedRepeat) && state.head == state.tail)
        || (scan == 0xc8 && state.metadata48 == heldUp
            && state.axisRoles[0] == upRole && state.lastKey == upWord && state.head == state.tail)) {
        return;
    }
    throw std::invalid_argument("unsupported resident Up packet sequence");
}

KeyboardMakeRingPath KeyboardClockAccess::makeRingPath() const {
    validateScan(0x48);
    return owner_.keyboard_.head == lastRingOffset
        ? KeyboardMakeRingPath::emptyTailWrap : KeyboardMakeRingPath::emptyLinear;
}

void KeyboardClockAccess::validateEdge(const KeyboardEdge edge) const {
    if (edge != KeyboardEdge::press && edge != KeyboardEdge::release) {
        throw std::invalid_argument("unsupported physical Up edge");
    }
    if (edge == KeyboardEdge::press && owner_.keyboard_.metadata48 != releasedUp) {
        throw std::invalid_argument("host Up press requires released controller");
    }
    validateScan(edge == KeyboardEdge::press ? 0x48 : 0xc8);
}

std::uint8_t KeyboardClockAccess::metadata() const {
    validate();
    return owner_.keyboard_.metadata48;
}

void KeyboardClockAccess::storeMetadata(const std::uint8_t value) {
    validate();
    if (value != releasedUp && value != heldUp) {
        throw std::invalid_argument("unsupported scan48 metadata write");
    }
    owner_.keyboard_.metadata48 = value;
}

void KeyboardClockAccess::storeRole(const std::uint8_t value) {
    validate();
    if (value != 0 && value != upRole) {
        throw std::invalid_argument("unsupported Up axis role");
    }
    owner_.keyboard_.axisRoles[0] = value;
}

void KeyboardClockAccess::storeLastKey(const std::uint16_t value) {
    validate();
    if (value != 0 && value != upWord) {
        throw std::invalid_argument("unsupported resident last key");
    }
    owner_.keyboard_.lastKey = value;
}

void KeyboardClockAccess::storeQueuedWord(const std::uint16_t value) {
    validate();
    auto& state = owner_.keyboard_;
    const bool full = state.head == lastRingOffset ? state.tail == ringBegin : state.head + entryBytes == state.tail;
    if (value != upWord || full || state.head != state.tail) {
        throw std::invalid_argument("Up enqueue requires admitted empty BIOS ring");
    }
    state.slots[slot(state.tail)] = value;
}

void KeyboardClockAccess::advanceTail() {
    validate();
    auto& state = owner_.keyboard_;
    if (state.head != state.tail || state.slots[slot(state.tail)] != upWord) {
        throw std::invalid_argument("BIOS tail advance has no source Up slot write");
    }
    state.tail = state.tail == lastRingOffset ? ringBegin : static_cast<std::uint16_t>(state.tail + entryBytes);
}

void KeyboardClockAccess::consumeHead() {
    validate();
    auto& state = owner_.keyboard_;
    const auto nextHead = state.head == lastRingOffset ? ringBegin : static_cast<std::uint16_t>(state.head + entryBytes);
    if (state.tail != nextHead || state.slots[slot(state.head)] != upWord) {
        throw std::invalid_argument("BIOS read requires one Up word");
    }
    state.head = nextHead;
}

void KeyboardClockAccess::storeConsumedScan(const std::uint16_t value) {
    validate();
    if (value != 0 && value != 0x48) {
        throw std::invalid_argument("unsupported foreground consumed scan");
    }
    owner_.keyboard_.consumedScan = value;
}

} // namespace stellar7
