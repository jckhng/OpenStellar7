#pragma once

#include "stellar7/normal_clock.hpp"

namespace stellar7 {

enum class StartupFontsProfile { normalMemory };
enum class StartupFontsKind { instructions, loadFont, retainFont, allocateProbe,
    releaseProbe, countdownWait };
enum class StartupFontRole { none, first, second };

struct StartupFontsSegment {
    StartupFontsKind kind;
    std::vector<NormalWorkOperation> operations;
    std::uint32_t sourcePc;
    StartupFontRole font = StartupFontRole::none;
    std::uint32_t value = 0;
};

struct StartupFontsWork {
    std::vector<StartupFontsSegment> segments;
    std::uint32_t waitPc = 0x0824 * 16 + 0x663b;
    std::uint32_t returnPc = 0x0824 * 16 + 0x6201;
};

// Fresh12CA/128C zero, successful two native font providers and first allocation
// probe. Operations precede effects. Whole providers own no invented DOS time.
// Ends before663B's first countdown read call; it does not wait for a callback.
StartupFontsWork startupFontsOperations(StartupFontsProfile profile);

}  // namespace stellar7
