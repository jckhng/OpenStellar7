#include "stellar7/startup_presentation_work.hpp"

#include "stellar7/render_work.hpp"

#include <utility>

namespace stellar7 {
namespace {
constexpr std::uint32_t gameBase = 0x0824 * 16;
constexpr std::uint64_t paletteResidentPrefix = 11;
constexpr std::uint64_t paletteResidentSuffix = 3;
constexpr std::uint16_t startupCountdown = 240;
constexpr std::uint8_t paletteStep = 4;
}

StartupPresentationWork startupPresentationOperations() {
    StartupPresentationWork result{};
    const auto append = [&](const StartupPresentationKind kind, const std::uint16_t pc,
                            const std::uint64_t dispatches) -> StartupPresentationSegment& {
        result.segments.push_back({kind, {}, gameBase + pc});
        if (dispatches != 0) {
            result.segments.back().operations.push_back({NormalWorkKind::instructions, dispatches});
        }
        return result.segments.back();
    };
    const auto provider = [&](const std::uint16_t pc, const StartupPresentationProvider service) {
        auto& segment = append(StartupPresentationKind::provider, pc, 2); // PUSH/CALL.
        segment.provider = service;
    };

    append(StartupPresentationKind::directReset, 0x6118, 3).value = 0x16; // MOV16/PUSH/CALL; child external.
    append(StartupPresentationKind::instructions, 0x611d, 1);
    append(StartupPresentationKind::loadSequences, 0x6124, 2); // PUSH/CALL0086; native provider.
    append(StartupPresentationKind::retainLoadedHead, 0x612c, 2); // Cleanup/store418E.

    provider(0x6133, StartupPresentationProvider::selectPalette);
    append(StartupPresentationKind::instructions, 0x6138, 1);
    // Caller613B plus355C's seven pre-leaf instructions and resident1B38 setup.
    append(StartupPresentationKind::palette, 0x3565, 1 + 7 + paletteResidentPrefix).request = {0, 256, 0, 0};
    append(StartupPresentationKind::instructions, 0x356a, paletteResidentSuffix + 2);

    provider(0x6142, StartupPresentationProvider::loadScreen);
    result.segments.back().page = StartupPresentationPage::back;
    append(StartupPresentationKind::instructions, 0x6147, 1);
    provider(0x614e, StartupPresentationProvider::acquireGraphics);
    append(StartupPresentationKind::instructions, 0x6153, 2); // Cleanup/MOV SI,AX.

    for (const auto page : {StartupPresentationPage::front, StartupPresentationPage::cache}) {
        const auto pc = static_cast<std::uint16_t>(page == StartupPresentationPage::front ? 0x6158 : 0x6161);
        // Two role assignments, caller CALL, eight8484 instructions through CALL22A1,
        // and resident22A1 JMPF. Both destinations copy the loaded BACK screen;
        // the fill routine belongs to the separate blackout caller.
        auto& segment = append(StartupPresentationKind::copy, pc, 2 + 1 + 8 + 1);
        segment.page = page;
        const auto body = planarCopyOperations(0,320,200);
        segment.operations.insert(segment.operations.end(), body.begin(), body.end());
        segment.operations.push_back({NormalWorkKind::instructions, 2});
    }

    for (std::uint8_t intensity = 0; intensity <= 60; intensity += paletteStep) {
        // First includes caller616A and9D9E PUSH/SUB; later includes prior
        // resident return, cleanup/increment/compare/JNG before the next call.
        const auto prefix = intensity == 0 ? 1 + 2 + 7 + paletteResidentPrefix
            : paletteResidentSuffix + 4 + 7 + paletteResidentPrefix;
        auto& segment = append(StartupPresentationKind::palette, 0x9daa, prefix);
        segment.request = {0, 256, 0, intensity};
    }
    append(StartupPresentationKind::instructions, 0x9daf, paletteResidentSuffix + 4 + 2);
    append(StartupPresentationKind::instructions, 0x616d, 2); // Enabled palette branch.
    provider(0x6178, StartupPresentationProvider::releasePalette);
    append(StartupPresentationKind::instructions, 0x617d, 1);
    provider(0x6184, StartupPresentationProvider::reloadPalette);
    append(StartupPresentationKind::instructions, 0x6189, 2); // Cleanup/store12C6.
    append(StartupPresentationKind::instructions, 0x618f, 4); // Both success tests.
    // Fresh0B94: CALL51A6, CMP/JZ, clear0B94, RETF; no op0E child.
    append(StartupPresentationKind::instructions, 0x61a4, 5);
    append(StartupPresentationKind::activateSequence, 0x61b0, 4); // SUB/PUSH/PUSH/CALL; child external.
    append(StartupPresentationKind::retainSelectedHead, 0x61b8, 2); // Cleanup/store485A.
    auto& countdown = append(StartupPresentationKind::countdown, 0x61bb, 0);
    countdown.value = startupCountdown;
    countdown.operations.push_back({NormalWorkKind::timerGlobalCount, 1, startupCountdown});
    return result;
}

}  // namespace stellar7

