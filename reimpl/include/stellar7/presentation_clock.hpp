#pragma once

#include "stellar7/audio.hpp"
#include "stellar7/blackout_work.hpp"
#include "stellar7/cockpit_state.hpp"
#include "stellar7/gameplay.hpp"
#include "stellar7/normal_clock.hpp"
#include "stellar7/page_flip_clock.hpp"
#include "stellar7/palette_clock.hpp"
#include "stellar7/render_work.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace stellar7 {

class KeyboardClockAccess;
class RetainedFrameExecutor;
enum class SuccessPaletteCompletionSource;
struct ClockedSuccessPaletteWork;
struct SuccessPaletteResidentCursor;
struct ClockedSuccessPaletteStepWork;

struct EntryFadeStep {
    std::uint64_t uploadNanoseconds = 0;
    std::uint64_t releaseNanoseconds = 0;
    std::uint64_t elapsedNanoseconds = 0;
    std::size_t beforeUploadSamples = 0;
    std::size_t maskedSamples = 0;
    std::size_t afterUploadSamples = 0;
};

// Pinned normal-core instruction budget and measured VGA period. IRQ/service
// overhead and the original loading-time video/PIT phase remain unrecovered.
std::array<EntryFadeStep, levelEntryFadeSteps> levelEntrySchedule(
    std::uint64_t initialVideoPhaseNanoseconds
);
bool startsLevelEntry(GameplayPhase previous, GameplayPhase current) noexcept;

enum class FrameSynchronization { disabled, verticalRetrace };

enum class SuccessPaletteChannel { green, blue };
enum class SuccessPaletteExecution { initial, advance };

struct SuccessPaletteState {
    SuccessPaletteChannel channel = SuccessPaletteChannel::green;
    std::array<std::uint8_t, 48> values{};
    std::array<std::int8_t, 48> directions{};
};

SuccessPaletteState initialSuccessPalette(SuccessPaletteChannel channel);

struct NativeSuccessPaletteWork {
    SuccessPaletteState state;
    std::vector<std::int16_t> samples;
    std::vector<PalettePortWrite> writes;
    std::vector<PaletteColorCommit> commits;
    std::uint64_t elapsedNanoseconds = 0;
    std::uint64_t highReads = 0;
};

struct NativePaletteWork {
    PaletteWorkState palette;
    std::vector<std::int16_t> samples;
    std::vector<PalettePortWrite> writes;
    std::vector<PaletteColorCommit> commits;
    std::uint64_t elapsedNanoseconds = 0;
    std::uint64_t highReads = 0;
};

using NativeBlackoutStep = NativePaletteWork;

struct NativeBlackoutWork {
    BlackoutWorkState state;
    std::array<NativeBlackoutStep, blackoutPaletteSteps> steps;
    std::vector<std::int16_t> clearSamples;
    std::uint64_t elapsedNanoseconds = 0;
};

struct ClockedPresentationWork {
    NormalWorkTiming timing;
    ClockedSoundBlasterAudio audio;
};

struct ClockedCockpitWork {
    CockpitWorkState state;
    ClockedPresentationWork work;
};

struct ClockedPageFlipWork {
    PageFlipClockState state;
    ClockedPresentationWork work;
    std::uint64_t lowReads;
    std::uint64_t highReads;
};

struct ClockedPaletteWork {
    PaletteWorkState state;
    ClockedPresentationWork work;
    std::uint64_t highReads;
    std::int64_t waitEntryCycles;
    std::int64_t uploadStartCycles;
    std::int64_t uploadEndCycles;
    std::vector<PalettePortWrite> writes;
    std::vector<PaletteColorCommit> commits;
};

struct ClockedBlackoutPaletteWork {
    ClockedPresentationWork before;
    ClockedPaletteWork palette;
    std::uint8_t intensity;
};

struct ClockedBlackoutWork {
    BlackoutWorkState state;
    std::array<ClockedBlackoutPaletteWork, blackoutPaletteSteps> steps;
    ClockedPresentationWork clear;
};

// Resident page-flip call only, not the work preceding it. Base budget excludes
// IRQ service; callers must supply the incoming video phase, not a target FPS.
std::uint64_t pageFlipNanoseconds(
    std::uint64_t initialVideoPhaseNanoseconds, FrameSynchronization synchronization
);

struct NativeIrqWait {
    std::uint16_t remaining;
};

class PresentationClock {
public:
    explicit PresentationClock(SoundBlasterSynth& synth) : synth_(synth) {}
    PresentationClock(const PresentationClock&) = delete;
    PresentationClock& operator=(const PresentationClock&) = delete;
    PresentationClock(PresentationClock&&) = delete;
    PresentationClock& operator=(PresentationClock&&) = delete;

    std::array<EntryFadeStep, levelEntryFadeSteps> entrySchedule() const;
    std::vector<std::int16_t> renderEntryStep(const EntryFadeStep& step);
    std::vector<std::int16_t> renderEntry();
    std::vector<std::int16_t> renderFrame();
    std::vector<std::int16_t> renderSamples(std::size_t count);
    void validateNativeIrqWait() const;
    std::size_t nativeIrqWaitSamples(std::uint16_t count) const;
    //10A8's already-expanded raw PIT count, on this retained native sample epoch.
    std::vector<std::int16_t> renderIrqWait(NativeIrqWait& wait, std::size_t sampleBudget);
    // Resident516B on the existing native sample epoch; not a strict phase join.
    // Incoming DAC is write mode, component0, after a completed native upload.
    NativeSuccessPaletteWork renderSuccessPalette(const SuccessPaletteState& state,
        SuccessPaletteExecution execution);
    // Resident success child on the active work clock; legacy sample path stays separate.
    ClockedSuccessPaletteWork renderSuccessPalette(const SoundBlasterClockPhase& phase,
        const SuccessPaletteState& state, const PaletteWorkState& palette,
        SuccessPaletteExecution execution, SuccessPaletteCompletionSource completion,
        std::int64_t horizonCycles);
    ClockedSuccessPaletteStepWork renderSuccessPaletteStep(const SoundBlasterClockPhase& phase,
        const SuccessPaletteResidentCursor& cursor, const PaletteWorkState& palette,
        std::uint64_t foregroundBudget, std::int64_t horizonCycles);
    //9D4C on the retained native sample/VGA epoch; no strict CPU/PIC phase join.
    NativeBlackoutWork renderNativeBlackout(const BlackoutWorkState& state,
        CockpitPageState& roles, IndexedPages& pages);
    //3546's complete full-intensity request, using the same native palette leaf.
    NativePaletteWork renderMenuPalette(const PaletteWorkState& state);
    ClockedPresentationWork renderWork(const SoundBlasterClockPhase& phase,
        const std::vector<NormalWorkOperation>& operations, std::int64_t horizonCycles);
    ClockedCockpitWork renderCockpit(const SoundBlasterClockPhase& phase,
        const CockpitWorkState& state, const CockpitWorkBitmaps& bitmaps, std::int64_t horizonCycles);
    ClockedPresentationWork renderHorizon(const SoundBlasterClockPhase& phase,
        const HorizonWorkState& state, std::int64_t horizonCycles);
    ClockedPageFlipWork renderPageFlip(const SoundBlasterClockPhase& phase,
        const PageFlipClockState& state, std::int64_t horizonCycles);
    //Resident516B first128 colors(384 raw bytes), retaining source/output banks.
    ClockedPaletteWork renderPlayableDecoyPalette(const SoundBlasterClockPhase& phase,
        const PaletteWorkState& state,std::int64_t horizonCycles);
    ClockedPaletteWork renderPalette(const SoundBlasterClockPhase& phase,
        const PaletteWorkState& state, const PaletteRequest& request, std::int64_t horizonCycles);
    ClockedBlackoutWork renderBlackout(const SoundBlasterClockPhase& phase, const BlackoutWorkState& state,
        CockpitPageState& roles, IndexedPages& pages, std::int64_t horizonCycles,
        BlackoutPageLayout layout = BlackoutPageLayout::compatibility);
    ClockedBlackoutWork renderBlackout(const SoundBlasterClockPhase& phase, const BlackoutWorkState& state,
        std::uint16_t frontPage, IndexedPages& pages, std::int64_t horizonCycles,
        BlackoutPageLayout layout = BlackoutPageLayout::compatibility);
    std::uint64_t elapsedSamples() const noexcept { return elapsedSamples_; }

private:
    friend class RetainedFrameExecutor;
    KeyboardClockAccess* keyboardAccess_ = nullptr;
    ClockedPresentationWork renderTiming(NormalWorkTiming timing);
    NativePaletteWork renderNativePalette(const PaletteWorkState& state, const PaletteRequest& request,
        std::uint64_t prefixDispatches, std::uint64_t suffixDispatches);
    SoundBlasterSynth& synth_;
    std::uint64_t elapsedSamples_ = 0;
    std::uint64_t workCycles_ = 0;
    bool workClockActive_ = false;
    std::optional<std::uint64_t> residentPaletteNanoseconds_;
};

}  // namespace stellar7
