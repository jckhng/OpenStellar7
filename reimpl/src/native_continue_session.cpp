#include "normal_work_packets.hpp"
#include "stellar7/native_session.hpp"
#include "retained_level1_clock_domain.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int64_t workHorizon = 120000;
constexpr std::size_t maximumPacketBudget = 10000;
constexpr unsigned damageFadeLength = 8;
constexpr std::int16_t screenWidth = 320;
constexpr std::int16_t screenHeight = 200;
enum class ExecutionPass { preflight, live };

std::vector<std::vector<NormalWorkOperation>> packets(const std::vector<NormalWorkOperation>& operations,
    const std::size_t budget) {
    return normalWorkPackets(operations,budget);
}

GameplayInput semanticInput(const ContinueKeyboardPacket& input, const ContinueOutcome outcome) {
    const bool releasedWidget = outcome == ContinueOutcome::yes || outcome == ContinueOutcome::no;
    return {input.directions[0] || input.directions[3] || input.directions[6],
        input.directions[2] || input.directions[5] || input.directions[7],
        input.directions[3] || input.directions[4] || input.directions[5],
        input.directions[0] || input.directions[1] || input.directions[2],
        input.space,input.peekScan == 1 && !releasedWidget,input.enter};
}

ContinueDecision decision(const ContinueOutcome outcome) {
    switch (outcome) {
    case ContinueOutcome::pending: return ContinueDecision::pending;
    case ContinueOutcome::yes: return ContinueDecision::yes;
    case ContinueOutcome::no: return ContinueDecision::no;
    case ContinueOutcome::cancelled: return ContinueDecision::cancelled;
    default: throw std::invalid_argument("unsupported recovered Continue outcome");
    }
}
}

NativeSessionDelivery NativeGameplaySession::advanceRecoveredContinue(const ContinueKeyboardPacket& input,
    const std::size_t workPacketBudget) {
    const bool entering = model_.phase() == GameplayPhase::failurePostExplosionBlackout;
    const bool level2=model_.failureFromLevel2() && model_.campaignLevel()==1 && !level1Pages_
        && recoveredLevel2Transition_ && recoveredLevel2Frame_ && recoveredFailure_ && recoveredFailure_->finished
        && recoveredLevel2Frame_->returnedLoopTick==model_.level2LoopTick() && failurePages_
        && assets_.campaignLevels.size()>1 && campaignResources_ && campaignResources_->level==1
        && campaignResources_->models==&assets_.campaignLevels[1].models
        && campaignResources_->bitmaps==&assets_.campaignLevels[1].bitmapGroups
        && campaignResources_->music==&assets_.campaignLevels[1].musicSequence
        && pageProjection_==(entering ? NativePageProjection::retainedLevel1DestructionClear
            : NativePageProjection::retainedLevel2Continue);
    const bool sourcePages=model_.failureFromLevel2() ? level2 : level1Pages_;
    if (!recoveredLoader_ || !recoveredController_ || !audio_->workPhase_ || !recoveredStartupPalette_
        || !audio_->resources_ || !audio_->fonts_ || !audio_->menuEntry_ || !audio_->workPhase_->timer
        || !pixels_ || !radar_ || !sourcePages || initialized_ || model_.remainingContinues() == 0
        || workPacketBudget == 0 || workPacketBudget > maximumPacketBudget
        || (!entering && (model_.phase() != GameplayPhase::continuePrompt || !recoveredContinueState_))
        || (entering && recoveredContinueState_ && recoveredContinueState_->stage != ContinueStage::inactive)
        || (!entering && recoveredContinueState_ && recoveredContinueState_->stage == ContinueStage::finished)) {
        throw std::invalid_argument("recovered Continue requires retained post-destruction owners and an active retry budget");
    }
    auto state = recoveredContinueState_.value_or(ContinueState{});
    state.scratch = audio_->resources_->scratchBuffer;
    state.transfer = audio_->resources_->transferBuffer;
    state.countdown = audio_->workPhase_->timer->globalCount;
    if (entering) {
        if (model_.damageFlashActive() && model_.damageFlashAge() >= damageFadeLength) {
            throw std::invalid_argument("recovered Continue lost its retained damage fade");
        }
        if (!recoveredContinueState_) {
            state.pointer = audio_->menuEntry_->pointer;
            state.oldRectangle = audio_->menuEntry_->oldRectangle;
        }
        state.backPage = pages_.backPage();
        state.frontPage = pages_.frontPage();
        state.fadeTicks = static_cast<std::uint16_t>(model_.damageFlashActive()
            ? damageFadeLength-model_.damageFlashAge() : 0);
        //The admitted Begin released its widget; its native profile has BC2=1 and no mouse/joystick.
        //12CE remains loader-owned zero throughout the admitted movement/fire path.
    }
    if (state.backPage != pages_.backPage() || state.frontPage != pages_.frontPage()) {
        throw std::logic_error("recovered Continue changed physical page roles");
    }
    auto plan = entering ? continueEntryOperations(state,*pixels_,assets_.artGroups.at(0),
        audio_->fonts_->firstFont,static_cast<std::uint16_t>(model_.remainingContinues()))
        : continuePollOperations(state,*pixels_,assets_.artGroups.at(0),audio_->fonts_->firstFont,input);
    auto staged = std::make_unique<GameplayModel>(model_);
    if (entering) { staged->enterRecoveredContinue(); }
    else if (state.stage == ContinueStage::polling) { staged->pollRecoveredContinue(semanticInput(input,plan.outcome)); }
    const auto observed = staged->recoveredContinueDecision();
    if (plan.outcome != ContinueOutcome::pending && observed != decision(plan.outcome)) {
        throw std::logic_error("recovered Continue source and semantic decisions differ");
    }
    if (plan.state.stage == ContinueStage::cancelCleanup && observed != ContinueDecision::cancelled) {
        throw std::logic_error("recovered Continue lost Escape cleanup ownership");
    }
    if (observed != ContinueDecision::cancelled) {
        const auto selected = plan.state.selected == 0 ? ContinueChoice::yes : ContinueChoice::no;
        const auto pressed = plan.state.pressed == 0 ? std::optional<ContinueChoice>{}
            : std::optional<ContinueChoice>{plan.state.pressed == 1 ? ContinueChoice::yes : ContinueChoice::no};
        if (selected != staged->continueChoice() || pressed != staged->pressedContinueChoice()) {
            throw std::logic_error("recovered Continue lost the shared choice/pressed owner");
        }
    }
    std::size_t providerOffset = 0;
    for (const auto& provider : plan.providers) {
        if (provider.guestElapsedQualified || provider.stepOffset < providerOffset
            || provider.stepOffset >= plan.steps.size()) {
            throw std::logic_error("recovered Continue has an invalid native provider boundary");
        }
        providerOffset = provider.stepOffset;
    }

    NativeContinueRun run;
    run.firstSample = clock_.elapsedSamples();
    run.entered = entering;
    run.outcome = plan.outcome;
    run.providers = plan.providers;
    auto phase = *audio_->workPhase_;
    auto palette = *recoveredStartupPalette_;
    auto pixels = *pixels_;
    std::vector<NativeTimedFrame> presentations;
    const auto execute = [&](const ExecutionPass pass) {
        phase = *audio_->workPhase_;
        palette = *recoveredStartupPalette_;
        pixels = *pixels_;
        const auto publish = [&]() {
            if (pass != ExecutionPass::live) { return; }
            IndexedScreen indexed;
            indexed.pixels.reserve(screenWidth*screenHeight);
            for (std::int16_t y = 0; y < screenHeight; ++y) {
                for (std::int16_t x = 0; x < screenWidth; ++x) {
                    indexed.pixels.push_back(pixels.read(pages_.frontPage(),{x,y}));
                }
            }
            indexed.palette = expandVgaDacPalette(std::vector<std::uint8_t>(palette.dac.colors.begin(),palette.dac.colors.end()));
            presentations.push_back({clock_.elapsedSamples(),std::move(indexed)});
        };
        const auto append = [&](ClockedPresentationWork work) {
            phase = work.timing.phase;
            run.samples.insert(run.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
            run.work.push_back(std::move(work));
        };
        for (const auto& step : plan.steps) {
            for (const auto& packet : packets(step.operations,workPacketBudget)) {
                if (pass == ExecutionPass::preflight) { phase = normalWorkTiming(phase,packet,workHorizon).phase; }
                else { append(clock_.renderWork(phase,packet,workHorizon)); }
            }
            if (step.palette) {
                if (pass == ExecutionPass::preflight) {
                    const auto result = paletteClockTiming(phase,palette,*step.palette,retainedCampaignPaletteHorizon(phase,workHorizon));
                    phase = result.timing.phase;
                    palette = result.state;
                } else {
                    auto result = clock_.renderPalette(phase,palette,*step.palette,retainedCampaignPaletteHorizon(phase,workHorizon));
                    palette = result.state;
                    append(result.work);
                    run.palettes.push_back(std::move(result));
                    publish();
                }
            }
            pixels.apply(step.effects);
            if (!step.effects.empty()) { publish(); }
        }
        //Qualify the composed physical writes before touching the live clock.
        for (const auto page : {pages_.frontPage(),pages_.backPage(),pages_.cachePage()}) {
            for (std::int16_t y = 0; y < screenHeight; ++y) {
                for (std::int16_t x = 0; x < screenWidth; ++x) {
                    if (pixels.known(page,{x,y}) != plan.pages.known(page,{x,y})
                        || (pixels.known(page,{x,y}) && pixels.read(page,{x,y}) != plan.pages.read(page,{x,y}))) {
                        throw std::logic_error("recovered Continue lost an ordered page effect");
                    }
                }
            }
        }
    };
    execute(ExecutionPass::preflight);
    execute(ExecutionPass::live);

    model_ = std::move(*staged);
    pixels_ = std::move(pixels);
    *recoveredStartupPalette_ = std::move(palette);
    audio_->workPhase_ = std::move(phase);
    //8730 selects FRONT; every5936 ends8373 with DRAW/COPY both BACK. No flip.
    pages_.beginModal();
    if (entering) { pages_.selectFront(); }
    plan.state.countdown = audio_->workPhase_->timer->globalCount;
    run.state = plan.state;
    audio_->resources_->scratchBuffer = std::move(plan.state.scratch);
    audio_->resources_->transferBuffer = std::move(plan.state.transfer);
    recoveredContinueState_ = std::move(plan.state);
    pageProjection_ = level2 ? NativePageProjection::retainedLevel2Continue : NativePageProjection::retainedLevel1Continue;
    frame_.pixels = *retainedPagePixels(NativePageRole::front);
    const auto& colors = recoveredStartupPalette_->dac.colors;
    frame_.palette = expandVgaDacPalette(std::vector<std::uint8_t>(colors.begin(),colors.end()));
    presentations.push_back({clock_.elapsedSamples(),frame_});
    NativeSessionDelivery delivery{run.firstSample,{},run.samples,{}};
    delivery.presentations = std::move(presentations);
    recoveredContinue_ = std::move(run);
    return delivery;
}
} // namespace stellar7
