#pragma once

#include "stellar7/frame_controller.hpp"
#include "stellar7/page_flip_clock.hpp"
#include "stellar7/palette_clock.hpp"

namespace stellar7 {

enum class KeyboardMakeRingPath { emptyLinear, emptyTailWrap };

// The clock borrows the existing controller owner for source-positioned effects.
class KeyboardClockAccess {
public:
    explicit KeyboardClockAccess(FrameController& owner);
    KeyboardClockAccess(const KeyboardClockAccess&) = delete;
    KeyboardClockAccess& operator=(const KeyboardClockAccess&) = delete;
    KeyboardClockAccess(KeyboardClockAccess&&) = delete;
    KeyboardClockAccess& operator=(KeyboardClockAccess&&) = delete;
    void validate() const;
    void validateEdge(KeyboardEdge edge) const;
    void validateScan(std::uint8_t scan) const;
    KeyboardMakeRingPath makeRingPath() const;
    std::uint8_t metadata() const;
    void storeMetadata(std::uint8_t value);
    void storeRole(std::uint8_t value);
    void storeLastKey(std::uint16_t value);
    void storeQueuedWord(std::uint16_t value);
    void advanceTail();
    void consumeHead();
    void storeConsumedScan(std::uint16_t value);

private:
    FrameController& owner_;
};

NormalWorkTiming normalWorkTimingBound(const SoundBlasterClockPhase& phase,
    const std::vector<NormalWorkOperation>& operations, std::int64_t horizonCycles, KeyboardClockAccess* access);
PageFlipClockTiming pageFlipClockTimingBound(const SoundBlasterClockPhase& phase,
    const PageFlipClockState& state, std::int64_t horizonCycles, KeyboardClockAccess* access);
PaletteClockTiming paletteClockTimingBound(const SoundBlasterClockPhase& phase,
    const PaletteWorkState& state, const PaletteRequest& request, std::int64_t horizonCycles, KeyboardClockAccess* access);

PaletteClockTiming playableDecoyPaletteClockTimingBound(const SoundBlasterClockPhase& phase,
    const PaletteWorkState& state,std::int64_t horizonCycles,KeyboardClockAccess* access);

struct SuccessPaletteClockTiming;
struct SuccessPaletteState;
enum class SuccessPaletteExecution;
enum class SuccessPaletteCompletionSource;
SuccessPaletteClockTiming successPaletteClockTimingBound(const SoundBlasterClockPhase& phase,
    const SuccessPaletteState& state, const PaletteWorkState& palette,
    SuccessPaletteExecution execution, SuccessPaletteCompletionSource completion,
    std::int64_t horizonCycles, KeyboardClockAccess* access);

} // namespace stellar7
