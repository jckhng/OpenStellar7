#pragma once

#include "stellar7/normal_clock.hpp"

namespace stellar7 {

enum class PageFlipSynchronization { disabled, enabled };
enum class PageFlipLayout { compatibility, recoveredStartup };
enum class PageFlipWaitDomain { bounded, retainedProgram };

struct PageFlipClockState {
    PageFlipSynchronization synchronization;
    std::uint16_t backPage;
    std::uint16_t frontPage;
    std::uint16_t crtcPort;
    PageFlipLayout layout = PageFlipLayout::compatibility;
    PageFlipWaitDomain waitDomain = PageFlipWaitDomain::bounded;
};

struct PageFlipClockTiming {
    NormalWorkTiming timing;
    PageFlipClockState state;
    std::uint64_t lowReads;
    std::uint64_t highReads;
};

//8518 through8522: caller,22b1 stub and resident1023 through RETF.
// Explicit Mode-X A000/A400 or recovered A000/A800 roles;3D4 CRTC, excludes02e2 cleanup.
// Status reads own retrace timing only, not attribute-controller flip-flop state.
PageFlipClockTiming pageFlipClockTiming(const SoundBlasterClockPhase& phase,
    const PageFlipClockState& state, std::int64_t horizonCycles);

}  // namespace stellar7
