#pragma once

#include "stellar7/presentation_clock.hpp"
#include "stellar7/driver_sequence_work.hpp"
#include "stellar7/radar_state.hpp"

namespace stellar7 {

struct Level1MusicActivation {
    DriverSequenceWork activation;
    std::vector<ClockedPresentationWork> work;
    SoundBlasterClockPhase phase;
    std::uint64_t firstSample;
    std::vector<std::int16_t> samples;
};

//5098..50AC after the two loader cockpit calls. Earlier loader work belongs to the caller.
Level1MusicActivation activateLevel1Music(PresentationClock& clock, SoundBlasterSynth& synth,
    const SoundBlasterClockPhase& phase);
SoundBlasterClockPhase level1MusicActivationPhase(const SoundBlasterClockPhase& phase,
    const SoundBlasterCallbackState& prepared);

struct Level1DirectReset {
    std::vector<ClockedPresentationWork> work;
    SoundBlasterClockPhase phase;
    std::uint64_t firstSample;
    std::vector<std::int16_t> samples;
};

//01A8..01B1, after the loader black palette. Rejects a driver callback inside206B.
Level1DirectReset resetLevel1DirectEffects(PresentationClock& clock, SoundBlasterSynth& synth,
    const SoundBlasterClockPhase& phase);
SoundBlasterClockPhase level1DirectResetPhase(const SoundBlasterClockPhase& phase);

struct Level1RetryMusicStop {
    std::vector<ClockedPresentationWork> work;
    SoundBlasterClockPhase phase;
    std::uint64_t firstSample;
    std::vector<std::int16_t> samples;
};

//51A6 callee only: first0464 stop and subsequent4F50 already-zero call.
//The descriptor free018C and reload6E6C remain explicit native providers.
SoundBlasterClockPhase level1RetryMusicStopPhase(const SoundBlasterClockPhase& phase);
Level1RetryMusicStop stopLevel1RetryMusic(PresentationClock& clock, SoundBlasterSynth& synth,
    const SoundBlasterClockPhase& phase);
//5098..50AC, same source activation as Begin, on the retained stopped song2 owner.
SoundBlasterClockPhase level1RetryMusicActivationPhase(const SoundBlasterClockPhase& phase,
    const SoundBlasterCallbackState& prepared);
Level1MusicActivation activateLevel1RetryMusic(PresentationClock& clock, SoundBlasterSynth& synth,
    const SoundBlasterClockPhase& phase);

enum class Level1LoaderPageDomain { freshBegin, recoveredRetry };

struct Level1LoaderPages {
    CockpitPageState pages;
    IndexedPages pixels;
    RadarState radar;
    CockpitWorkBitmaps bitmaps;
    std::array<CockpitWork,2> cockpit;
    std::vector<NormalWorkOperation> operations;
};

//50C4(0), including both6D46 calls, empty radar and final copy; excludes outer5092 CALL.
//The resource decode/cache replacement is an explicit untimed native bootstrap provider.
Level1LoaderPages planLevel1LoaderPages(const GameplayAssets& assets, const GameplayModel& model,
    const CockpitPageState& pages, const IndexedPages& pixels,
    Level1LoaderPageDomain domain = Level1LoaderPageDomain::freshBegin);
Level1LoaderPages planPlayableLevel1LoaderPages(const GameplayAssets& assets, const GameplayModel& model,
    const CockpitPageState& pages, const IndexedPages& pixels, std::int16_t detail,
    Level1LoaderPageDomain domain = Level1LoaderPageDomain::freshBegin);

} // namespace stellar7
