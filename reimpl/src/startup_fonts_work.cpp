#include "stellar7/startup_fonts_work.hpp"

#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::uint32_t gameBase = 0x0824 * 16;
constexpr std::uint32_t normalMemoryProbeBytes = 0x2e400;
}

StartupFontsWork startupFontsOperations(const StartupFontsProfile profile) {
    if (profile != StartupFontsProfile::normalMemory) {
        throw std::invalid_argument("unsupported native startup font/memory profile");
    }
    StartupFontsWork result{};
    const auto append = [&](const StartupFontsKind kind, const std::uint16_t pc,
                            const std::uint64_t dispatches) -> StartupFontsSegment& {
        result.segments.push_back({kind, {{NormalWorkKind::instructions, dispatches}}, gameBase + pc});
        return result.segments.back();
    };
    //61C1/C6 fresh-font check,61C8 PUSH stream,61CC CALL4524.
    append(StartupFontsKind::loadFont, 0x61cc, 4).font = StartupFontRole::first;
    append(StartupFontsKind::retainFont, 0x61d4, 2).font = StartupFontRole::first;
    append(StartupFontsKind::loadFont, 0x61db, 2).font = StartupFontRole::second;
    append(StartupFontsKind::retainFont, 0x61e3, 2).font = StartupFontRole::second;
    append(StartupFontsKind::instructions, 0x61e6, 4); // Both handles nonzero.

    //61FB CALL;665C..666C eight instructions;6671..667E six instructions.
    // Native provider succeeds on the first large allocation.128C becomes1.
    append(StartupFontsKind::allocateProbe, 0x667e, 1 + 8 + 6).value = normalMemoryProbeBytes;
    //6683 cleanup,6686 save DX,6689 OR,668B JZ(not taken),668D/90 PUSH,6691 CALL.
    append(StartupFontsKind::releaseProbe, 0x6691, 7).value = normalMemoryProbeBytes;
    //6696 cleanup,6699/9A POP,669B MOV SP,669D POP BP,669E RET.
    append(StartupFontsKind::instructions, 0x6696, 6);
    //61FE CALL6638;6638 PUSH BP;6639 MOV BP,SP. No countdown polling yet.
    append(StartupFontsKind::countdownWait, 0x663b, 3);
    return result;
}

}  // namespace stellar7
