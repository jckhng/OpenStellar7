#include "stellar7/native_startup.hpp"
#include "stellar7/ui_fill_work.hpp"
#include "playable_attract.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int64_t creditsWorkHorizon = 120000;
constexpr std::size_t maximumPacketBudget = 10000;
constexpr std::uint16_t sourceCreditsCountdown = 7200;
constexpr std::uint16_t nativeEscape = 0x011b;
constexpr std::uint16_t screenWidth = 320;
constexpr std::uint16_t screenHeight = 200;
//Immutable12E8..12F8; no preceding admitted operation changes these UI colors.
constexpr CreditsColors creditsColors{{{147,141,135,128,135,141,147},147,135,128},156};
enum class Execution { qualify, render };

std::vector<std::vector<NormalWorkOperation>> drawingPackets(
    const std::vector<NormalWorkOperation>& operations, const std::size_t budget) {
    std::vector<std::vector<NormalWorkOperation>> packets(1);
    std::size_t used = 0;
    for (auto operation : operations) {
        if (operation.kind == NormalWorkKind::instructions) {
            while (operation.count != 0) {
                if (used == budget) { packets.emplace_back(); used = 0; }
                const auto count = std::min<std::uint64_t>(operation.count, budget - used);
                packets.back().push_back({operation.kind, count, operation.value});
                used += static_cast<std::size_t>(count);
                operation.count -= count;
            }
            continue;
        }
        //Never split a REP: its restart state belongs to one source instruction.
        const auto cost = std::max<std::uint64_t>(1, operation.count);
        if (cost > budget) {
            throw std::invalid_argument("credits packet cannot split a source string instruction");
        }
        if (used + cost > budget) { packets.emplace_back(); used = 0; }
        packets.back().push_back(operation);
        used += static_cast<std::size_t>(cost);
    }
    return packets;
}
}

const NativeStartupCreditsRun* NativeStartupSequence::recoveredCredits() const noexcept {
    return audio_ && audio_->credits_ ? &*audio_->credits_ : nullptr;
}

bool NativeStartupSequence::creditsActive() const noexcept {
    return phase_==NativeStartupPhase::awaitingMenuConstruction && audio_
        && hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::storyPanels)
        && hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::automaticAttract);
}

NativeSessionDelivery NativeStartupSequence::pollPlayableCredits(const NativeMenuInput input,
    const std::size_t budget,const NativeDeliverySink& sink) {
    if (!creditsActive() || !audio_->workPhase_ || !audio_->menuResources_
        || !audio_->workPhase_->playableStartup || budget==0 || budget>maximumPacketBudget) {
        throw std::invalid_argument("cold credits require their retained full presentation caller");
    }
    NativeSessionDelivery delivery;delivery.firstSample=elapsedSamples();
    if (audio_->attractCreditsSkip_!=0 && !coldCredits_) {
        const std::vector<NormalWorkOperation> operations{{NormalWorkKind::playableStartupCreditsSkip,1,
            static_cast<std::uint16_t>(audio_->attractCreditsSkip_)}};
        normalWorkTiming(*audio_->workPhase_,operations,creditsWorkHorizon);
        NativeStartupCreditsRun run;run.firstSample=delivery.firstSample;run.driverBefore=audio_->synth_.driverState();
        auto work=audio_->clock_.renderWork(*audio_->workPhase_,operations,creditsWorkHorizon);
        *audio_->workPhase_=work.timing.phase;
        delivery.samples=work.audio.samples;
        run.samples=delivery.samples;run.finalPhase=*audio_->workPhase_;run.palette=audio_->menuResources_->palette;
        run.driverAfter=audio_->synth_.driverState();run.work.push_back(std::move(work));
        audio_->credits_=std::move(run);phase_=NativeStartupPhase::awaitingMenuEntry;
        StoryDeliveryStream stream(sink);stream.publish(delivery);
        return delivery;
    }
    if (!storyRoles_) { storyRoles_=CockpitPageState::fromRecoveredMenu(); }
    auto& palette=audio_->menuResources_->palette;
    if (!coldCredits_) {
        coldCredits_=std::make_shared<PlayableAttractCredits>(CreditsPresentationCaller::coldStartup);
        NativeStartupCreditsRun run;run.firstSample=delivery.firstSample;run.driverBefore=audio_->synth_.driverState();
        run.drawPage=storyRoles_->frontPage();run.copyPage=storyRoles_->backPage();
        audio_->credits_=std::move(run);
    }
    AttractPresentation presentation({pages_,*storyRoles_,palette,audio_->synth_,audio_->clock_,*audio_->workPhase_,
        game_,resources_,audio_->menuResources_->configuration,audio_->attractCreditsSkip_,{}},delivery,budget,sink);
    const std::uint16_t key=input.sourceKey!=0 ? input.sourceKey : input.cancel ? nativeEscape : input.confirm ? 0x1c0d
        : input.up ? 0x4800 : input.down ? 0x5000 : input.left ? 0x4b00 : input.right ? 0x4d00 : 0;
    coldCredits_->advance(presentation,{input,key,false});
    auto& run=*audio_->credits_;
    run.samples.insert(run.samples.end(),delivery.samples.begin(),delivery.samples.end());
    run.finalPhase=*audio_->workPhase_;run.palette=palette;run.driverAfter=audio_->synth_.driverState();
    if (key!=0) { run.sourceKey=key; }
    if (run.displayedPage.empty() && !coldCredits_->complete()) { run.displayedPage=pages_.snapshot(storyRoles_->frontPage()); }
    frame_.pixels=pages_.snapshot(storyRoles_->frontPage());
    frame_.palette=expandVgaDacPalette(std::vector<std::uint8_t>(palette.dac.colors.begin(),palette.dac.colors.end()));
    if (coldCredits_->complete()) { coldCredits_.reset();phase_=NativeStartupPhase::awaitingMenuEntry; }
    return delivery;
}

void NativeStartupSequence::skipCredits(const NativeIntroKey key, const std::size_t workPacketBudget) {
    if (phase_ != NativeStartupPhase::awaitingMenuConstruction || !audio_ || !audio_->workPhase_
        || !audio_->menuResources_ || !audio_->fonts_) {
        throw std::logic_error("credits require the retained menu-resource return");
    }
    if (key != NativeIntroKey::escape || workPacketBudget == 0 || workPacketBudget > maximumPacketBudget
        || game_.artGroups.empty()) {
        throw std::invalid_argument("credits require explicit native Escape and bounded source packets");
    }
    const auto front = menuPageSegment(NativeMenuPage::front);
    const auto back = menuPageSegment(NativeMenuPage::back);
    NativeStartupCreditsRun run{};
    run.sourceKey = nativeEscape;
    run.copyPage = back;
    run.drawPage = front;
    run.textColor = creditsColors.panel.middle;
    run.surface = {{0,319,0,199},PolygonClipMode::Enabled,1,0};
    run.drawing = creditsDrawingOperations(game_.artGroups[0], audio_->fonts_->firstFont,
        audio_->fonts_->secondFont, creditsColors, back);
    const auto packets = drawingPackets(run.drawing.operations, workPacketBudget);
    std::vector<NormalWorkOperation> copy{{NormalWorkKind::instructions, 10 + 1}}; //B281..B293 +22A1 JMPF.
    const auto copyBody = planarCopyOperations(0,screenWidth,screenHeight);
    copy.insert(copy.end(),copyBody.begin(),copyBody.end());
    std::vector<NormalWorkOperation> clear{{NormalWorkKind::instructions, 1 + 8}}; //B2B8 CALL6622 and setup.
    const auto clearBody = uiFillOperations({0,0,screenWidth,screenHeight},0,front);
    clear.insert(clear.end(),clearBody.operations.begin(),clearBody.operations.end());
    clear.push_back({NormalWorkKind::instructions,2}); //6633 cleanup/RET; clipping remains enabled.

    const auto execute = [&](const Execution execution) {
        const bool render = execution == Execution::render;
        auto phase = *audio_->workPhase_;
        auto palette = audio_->menuResources_->palette;
        const auto append = [&](ClockedPresentationWork work) {
            phase = work.timing.phase;
            run.samples.insert(run.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
            run.work.push_back(std::move(work));
        };
        const auto normal = [&](const std::vector<NormalWorkOperation>& operations) {
            if (render) {
                append(audio_->clock_.renderWork(phase,operations,creditsWorkHorizon));
            } else {
                phase = normalWorkTiming(phase,operations,creditsWorkHorizon).phase;
            }
        };
        const auto paletteHorizon = [&]() {
            return hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::storyPanels)
                &&phase.callback&&phase.callback->program ? 2*creditsWorkHorizon : creditsWorkHorizon;
        };
        const auto blackout = [&]() {
            BlackoutWorkState surface{};
            surface.palette = palette;
            surface.surface.clipping = BlackoutClip::enabled;
            if (!render) {
                auto timing = blackoutClockTiming(phase,surface,front,paletteHorizon(),storyRoles_?BlackoutPageLayout::recoveredStartup:BlackoutPageLayout::compatibility);
                phase = timing.clear.phase;
                palette = std::move(timing.state.palette);
                return;
            }
            auto work = audio_->clock_.renderBlackout(phase,surface,front,pages_,paletteHorizon(),storyRoles_?BlackoutPageLayout::recoveredStartup:BlackoutPageLayout::compatibility);
            for (auto& step : work.steps) {
                append(std::move(step.before));
                run.paletteUploads.push_back(step.palette.state);
                run.paletteRequests.push_back({0,256,0,step.intensity});
                append(std::move(step.palette.work));
            }
            append(std::move(work.clear));
            palette = std::move(work.state.palette);
        };

        normal({{NormalWorkKind::instructions,3}}); //41BE clear4E44;41C4 RET;0144 CALLB12A.
        for (const auto& packet : packets) { normal(packet); }
        if (render) { pages_.apply(run.drawing.effects); }
        normal({{NormalWorkKind::instructions,1}}); //B27E CALL9D4C.
        blackout();
        normal(copy);
        if (render) {
            pages_.apply(std::vector<IndexedPageEffect>{RadarPageCopy{back,front,{0,0},screenWidth,screenHeight}});
            run.displayedPage = menuPagePixels(NativeMenuPage::front);
        }
        normal({{NormalWorkKind::instructions,2}}); //B298 cleanup/CALL9D9E.
        for (std::uint8_t intensity = 0; intensity <= 60; intensity += 4) {
            normal({{NormalWorkKind::instructions,intensity == 0 ? 20U : 25U}});
            const PaletteRequest request{0,256,0,intensity};
            if (render) {
                auto work = audio_->clock_.renderPalette(phase,palette,request,paletteHorizon());
                palette = work.state;
                run.paletteUploads.push_back(work.state);
                run.paletteRequests.push_back(request);
                append(std::move(work.work));
            } else {
                auto timing = paletteClockTiming(phase,palette,request,paletteHorizon());
                phase = timing.timing.phase;
                palette = std::move(timing.state);
            }
        }
        normal({{NormalWorkKind::instructions,9}}); //Resident return, loop exit and9DBA/9DBB.
        normal({{phase.playableStartup ? NormalWorkKind::playableStartupCreditsCountdown : NormalWorkKind::startupCreditsCountdown,1,sourceCreditsCountdown},
            {phase.playableStartup ? NormalWorkKind::playableStartupCreditsInput : NormalWorkKind::startupCreditsFirstInput,1,nativeEscape}});
        if (render) { run.countdownRead = *run.work.back().timing.creditsCountdownRead; }
        normal({{NormalWorkKind::instructions,1}}); //B2B5 CALL9D4C.
        blackout();
        normal(clear);
        if (render) { pages_.apply(clearBody.effects); }
        normal({{NormalWorkKind::instructions,2}}); //B2BB PUSH/CALL1A22, whole native provider untimed.
        palette = completedFullPaletteUpload(palette,{0,256,0,63}); //0736 restores both banks and DAC.
        normal({{NormalWorkKind::instructions,6}}); //B2C4 cleanup andB2C7..B2CC RET ->0147.
        return std::make_pair(std::move(phase),std::move(palette));
    };
    execute(Execution::qualify); //Reject unsupported callback/EOF work before changing live owners.
    run.firstSample = elapsedSamples();
    run.driverBefore = audio_->synth_.driverState();
    auto completed = execute(Execution::render);
    run.finalPhase = std::move(completed.first);
    run.palette = std::move(completed.second);
    run.driverAfter = audio_->synth_.driverState();
    frame_.pixels = menuPagePixels(NativeMenuPage::front);
    frame_.palette = expandVgaDacPalette(std::vector<std::uint8_t>(
        run.palette.dac.colors.begin(),run.palette.dac.colors.end()));
    audio_->workPhase_ = run.finalPhase;
    audio_->credits_ = std::move(run);
    phase_ = NativeStartupPhase::awaitingMenuEntry;
}

} // namespace stellar7
