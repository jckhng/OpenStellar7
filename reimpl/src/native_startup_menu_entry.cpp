#include "stellar7/native_startup.hpp"
#include "stellar7/ui_pointer_work.hpp"
#include "playable_startup_clock_domain.hpp"

#include <algorithm>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int64_t menuWorkHorizon = 120000;
constexpr std::int64_t menuPaletteHorizon = 2 * menuWorkHorizon;
constexpr std::size_t maximumPacketBudget = 10000;
constexpr std::uint16_t menuCountdown = 30000;
constexpr std::size_t widgetScratchBytes = 500;
constexpr std::size_t widgetTemplateOffset = 0x26;
constexpr CockpitCopyRegion pointerRectangle{163,69,16,21};
constexpr Viewport display{0,319,0,199};
enum class Execution { qualify, render };

std::vector<std::vector<NormalWorkOperation>> workPackets(
    const std::vector<NormalWorkOperation>& operations, const std::size_t budget) {
    std::vector<std::vector<NormalWorkOperation>> result(1);
    std::size_t used = 0;
    for (auto operation : operations) {
        if (operation.kind == NormalWorkKind::instructions) {
            while (operation.count != 0) {
                if (used == budget) { result.emplace_back(); used = 0; }
                const auto count = std::min<std::uint64_t>(operation.count,budget-used);
                result.back().push_back({operation.kind,count,operation.value});
                used += static_cast<std::size_t>(count);
                operation.count -= count;
            }
            continue;
        }
        const auto cost = std::max<std::uint64_t>(1,operation.count);
        if (cost > budget) {
            throw std::invalid_argument("menu packet cannot split a source string instruction");
        }
        if (used + cost > budget) { result.emplace_back(); used = 0; }
        result.back().push_back(operation);
        used += static_cast<std::size_t>(cost);
    }
    return result;
}
}

const NativeStartupMenuEntryRun* NativeStartupSequence::recoveredMenuEntry() const noexcept {
    return audio_ && audio_->menuEntry_ ? &*audio_->menuEntry_ : nullptr;
}

void NativeStartupSequence::enterMenu(const NativeMenuEntryProfile profile,
    const std::size_t workPacketBudget) {
    if (phase_ != NativeStartupPhase::awaitingMenuEntry || !audio_ || !audio_->workPhase_
        || !audio_->credits_ || !audio_->fonts_ || !audio_->resources_) {
        throw std::logic_error("menu entry requires the retained credits return");
    }
    if (profile != NativeMenuEntryProfile::keyboardOnly || workPacketBudget == 0
        || workPacketBudget > maximumPacketBudget || game_.artGroups.empty()
        || audio_->resources_->scratchBuffer.size() != widgetScratchBytes
        || audio_->resources_->scratchBuffer[widgetTemplateOffset] != 0 || audio_->resources_->scratchBuffer[widgetTemplateOffset+1] != 0
        || game_.artGroups[0].empty() || game_.artGroups[0][0].width != pointerRectangle.width
        || game_.artGroups[0][0].height + 1 != pointerRectangle.height) {
        throw std::invalid_argument("menu entry requires the native neutral keyboard-only profile");
    }
    const auto front = menuPageSegment(NativeMenuPage::front);
    const auto back = menuPageSegment(NativeMenuPage::back);
    NativeStartupMenuEntryRun run{};
    run.drawing = menuDrawingOperations(game_.artGroups[0],audio_->fonts_->firstFont,
        audio_->resources_->scratchBuffer,back);
    const auto pointer = uiBitmapOperations(game_.artGroups[0][0],{pointerRectangle.x,pointerRectangle.y},display,back);
    const auto execute = [&](const Execution execution) {
        const bool render = execution == Execution::render;
        auto phase = *audio_->workPhase_;
        auto palette = audio_->credits_->palette;
        auto pages = pages_;
        auto transfer = audio_->resources_->transferBuffer;
        const auto append = [&](ClockedPresentationWork work) {
            phase = work.timing.phase;
            run.samples.insert(run.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
            run.work.push_back(std::move(work));
        };
        const auto normal = [&](const std::vector<NormalWorkOperation>& operations) {
            for (const auto& packet : workPackets(operations,workPacketBudget)) {
                if (render) { append(audio_->clock_.renderWork(phase,packet,menuWorkHorizon)); }
                else { phase = normalWorkTiming(phase,packet,menuWorkHorizon).phase; }
            }
        };
        const auto plain = [&](const std::uint64_t count) {
            normal({{NormalWorkKind::instructions,count}});
        };
        const auto fullPalette = [&]() {
            plain(8 + 11); //3546 through CALLF; resident1B38 before loaded077A.
            const auto horizon=playableStartupProgram(phase) ? menuPaletteHorizon : menuWorkHorizon;
            if (render) {
                auto work = audio_->clock_.renderPalette(phase,palette,{0,256,0,63},horizon);
                palette = work.state;
                run.paletteUploads.push_back(palette);
                append(std::move(work.work));
            } else {
                // Arithmetic plus the next retrace can exceed a foreground packet.
                auto timing = paletteClockTiming(phase,palette,{0,256,0,63},horizon);
                phase = timing.timing.phase;
                palette = std::move(timing.state);
            }
            plain(3 + 2); //Resident return,3557 cleanup and355A RET.
        };
        const auto copy = [&](const CockpitCopyRegion rectangle) {
            plain(1); //Resident22A1 JMPF; outer CALLF already belongs to caller.
            normal(planarCopyOperations(rectangle.x,rectangle.width,rectangle.height));
            const auto alignedX = static_cast<std::int16_t>(rectangle.x & ~3);
            const auto alignedWidth = static_cast<std::uint16_t>(
                4 * ((rectangle.width + 2 * (rectangle.x & 3) + 3) / 4));
            pages.apply(std::vector<IndexedPageEffect>{RadarPageCopy{back,front,
                {alignedX,rectangle.y},alignedWidth,static_cast<std::uint16_t>(rectangle.height)}});
        };

        plain(1 + 7); //0147 CALL1578;1578..1586 including CALL3546.
        fullPalette();
        plain(8); //1589..1603, actual immutable2C2=0 bypasses attract.
        //5230/5C14 retain all69 invariant caller dispatches. The native no-mouse
        //provider leaves the invalid preconstruction pointer unspecified. It
        //does not read or invent allocation-prefix bytes or time unknown5B9E.
        plain(35 + 34);
        plain(2); //1606 cleanup;1609 CALL16B0.
        normal(run.drawing.operations); //16B0 through17DE, native font0 selection.
        pages.apply(run.drawing.effects);
        plain(3 + 8); //17E1..17E7;8484 setup through CALLF22A1.
        copy({0,0,320,200});
        plain(2); //8495 cleanup/8498 RET.
        plain(5 + 19); //17EA..17F2 and complete in-bounds5B9E(160,110).
        plain(4 + 32 + 19); //17F5..17FB,5C14(0,count5),5B9E(163,69).
        plain(2); //17FE cleanup;1801 CALL3546.
        fullPalette();
        normal({{phase.playableStartup ? NormalWorkKind::playableStartupMenuEntryCountdown : NormalWorkKind::startupMenuEntryCountdown,1,menuCountdown}});
        plain(2); //180A RET;160C CALL5936.

        //Explicit native03EE=0 and ten0437 released queries are untimed
        //providers. Caller paths remain ordered; mouse/joystick/music absent.
        plain(12); //5936..5951,03EE then held Enter query.
        plain(8); //5956..5967,BC2=1 then held Space query.
        plain(6); //596C..597C including CALL8BD0.
        plain(53); //8BD0, eight held queries in source order.
        plain(5 + 6); //597F..59C3 and ordinary64A0.
        plain(10 + 13); //59C6..5A32 and neutral5AC8; DRAW=COPY=BACK.
        plain(2); //5A36 result;5A38 CALL82EA.
        plain(20); //82EA..8324 including CALLF22A5, no bottom clip.
        auto saved = uiPointerSaveOperations(pages,pointerRectangle,back,transfer);
        normal(saved.operations);
        transfer = std::move(saved.transfer);
        plain(7); //8329..833B including CALLF2C1B.
        normal(pointer.operations);
        pages.apply(pointer.effects);
        plain(8); //8340..8356 old rectangle CALLF.
        copy({0,0,16,10});
        plain(6); //835B..8368 current rectangle CALLF.
        copy(pointerRectangle);
        plain(10); //836D..8388 restore CALLF.
        const auto restored = uiPointerRestoreOperations(pointerRectangle,back,transfer);
        normal(restored.operations);
        pages.apply(restored.effects);
        plain(12); //838D..83B6, immutable oldY0 does not take overflow arm.
        plain(10); //5A3B..5A7B neutral return ->160F.
        return std::make_tuple(std::move(phase),std::move(palette),std::move(pages),std::move(transfer));
    };
    execute(Execution::qualify); //Reject unsupported source work before live audio mutation.
    run.firstSample = elapsedSamples();
    run.driverBefore = audio_->synth_.driverState();
    auto completed = execute(Execution::render);
    run.finalPhase = std::move(std::get<0>(completed));
    run.palette = std::move(std::get<1>(completed));
    pages_ = std::move(std::get<2>(completed));
    run.transfer = std::move(std::get<3>(completed));
    run.scratch = run.drawing.scratch;
    audio_->resources_->scratchBuffer = run.scratch;
    audio_->resources_->transferBuffer = run.transfer;
    run.driverAfter = audio_->synth_.driverState();
    frame_.pixels = menuPagePixels(NativeMenuPage::front);
    frame_.palette = expandVgaDacPalette(std::vector<std::uint8_t>(
        run.palette.dac.colors.begin(),run.palette.dac.colors.end()));
    audio_->workPhase_ = run.finalPhase;
    audio_->menuEntry_ = std::move(run);
    phase_ = NativeStartupPhase::awaitingMenuDecision;
}

} // namespace stellar7
