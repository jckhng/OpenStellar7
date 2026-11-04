#pragma once

#include "stellar7/palette_clock.hpp"

namespace stellar7 {

enum class StartupPresentationPage { none, front, back, cache };
enum class StartupPresentationKind { instructions, provider, loadSequences, directReset,
    activateSequence, retainLoadedHead, retainSelectedHead, countdown, palette, copy };
enum class StartupPresentationProvider { none, selectPalette, loadScreen,
    acquireGraphics, releasePalette, reloadPalette };

struct StartupPresentationSegment {
    StartupPresentationKind kind;
    std::vector<NormalWorkOperation> operations;
    std::uint32_t sourcePc;
    PaletteRequest request{0, 0, 0, 0};
    StartupPresentationPage page = StartupPresentationPage::none;
    StartupPresentationProvider provider = StartupPresentationProvider::none;
    std::uint16_t value = 0;
};

struct StartupPresentationWork {
    std::vector<StartupPresentationSegment> segments;
    // 60C9..60CF selects BACK as both copy and draw before this entry.
    StartupPresentationPage initialDraw = StartupPresentationPage::back;
    StartupPresentationPage finalDraw = StartupPresentationPage::cache;
};

// 6114 ->61C1, successful fresh SBL/graphics, palette enabled, prior song0.
// Segment operations precede its effect/child. Palette operations precede the
// loaded palette leaf; audio bodies and untimed native providers are external.
StartupPresentationWork startupPresentationOperations();

}  // namespace stellar7
