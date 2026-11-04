#pragma once

#include "stellar7/frame_tail_work.hpp"

#include <array>
#include <cstdint>

namespace stellar7 {

inline constexpr std::size_t frameKeyboardSlotCount = 16;

enum class FrameControllerOrigin { sourceColdDefaults, restoredDiagnosticInitial };
enum class FrameControllerDelivery { keyboard, configuration, specialAction };

struct FrameControllerInitial {
    FrameControllerOrigin origin;
    std::uint16_t loopTick;
    std::uint16_t keyboardHead;
    std::uint16_t keyboardTail;
    std::array<bool, 9> heldKeys;
    std::uint16_t queuedScanBytes;
    bool repeatActive;
    bool transferScheduled;
    bool port60Pending;
    bool joystickEnabled;
    bool extraUpdate;
    std::array<std::uint16_t, 6> abilities;
    std::array<std::uint16_t, frameKeyboardSlotCount> keyboardSlots{};
    std::uint8_t biosModifierFlags = 0;
    std::uint8_t keyRemap = 0;
    std::uint8_t enhancedMode = 0;
};

struct FrameControllerKeyboardState {
    std::uint8_t metadata48 = 0x70;
    std::uint8_t metadataAlt38 = 0;
    std::array<std::uint8_t, 2> axisRoles{};
    std::uint16_t lastKey = 0;
    std::uint16_t consumedScan = 0;
    std::uint16_t head = 0;
    std::uint16_t tail = 0;
    std::uint16_t ringBegin = 0;
    std::uint16_t ringEnd = 0;
    std::array<std::uint16_t, frameKeyboardSlotCount> slots{};
};

class KeyboardClockAccess;

// Owns admitted resident key state and the BIOS queue.
class FrameController {
public:
    FrameController() = default;
    FrameController(const FrameController&) = delete;
    FrameController& operator=(const FrameController&) = delete;
    FrameController(FrameController&&) = delete;
    FrameController& operator=(FrameController&&) = delete;
    void initialize(const FrameControllerInitial& initial);
    FrameTailGuards augmentGuards(FrameTailGuards guards) const;
    // Native held-key provider, distinct from physical IRQ/BDA delivery.
    FrameTailGuards nativeGuards(FrameTailGuards guards, const std::array<bool, 9>& keys) const;
    void completeNativeTail(std::uint16_t loopTick, const Level1FrameTailWork& work,
        const std::array<bool, 9>& keys);
    void completeNativeTail(std::uint16_t loopTick, const FrameTailWork& work,
        const std::array<bool, 9>& keys);
    void restartNativeLoop();
    void consumeEmptyTail(std::uint16_t loopTick);
    void completeTail(std::uint16_t loopTick, const FrameTailWork& work);
    // Diagnostic import remains a held-key projection, with no BDA enqueue.
    void restoreForwardAtRenderReturn();
    void delivery(FrameControllerDelivery event);
    FrameControllerKeyboardState keyboardState() const;
    std::uint16_t expectedLoopTick() const;
    FrameControllerOrigin origin() const;

private:
    friend class KeyboardClockAccess;

    void requireAdmission() const;
    void completeNativeTail(std::uint16_t loopTick, std::uint16_t returnedLoopTick,
        std::uint16_t keyboardHead, std::uint16_t consumedScan, const std::array<bool, 9>& keys);
    bool forwardHeld() const;

    bool initialized_ = false;
    bool invalidated_ = false;
    bool keyboardBound_ = false;
    std::uint16_t nextLoopTick_ = 0;
    FrameControllerKeyboardState keyboard_{};
    //Scan48 remains owned by metadata48; these are the other eight queried keys.
    std::array<bool, 8> nativeOtherKeys_{};
    FrameControllerOrigin origin_ = FrameControllerOrigin::sourceColdDefaults;
};

} // namespace stellar7
