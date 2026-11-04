#include "normal_work_packets.hpp"
#include "stellar7/native_session.hpp"
#include "retained_level1_clock_domain.hpp"
#include "stellar7/level1_loader_work.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int64_t workHorizon = 120000;
constexpr std::size_t maximumPacketBudget = 10000;
constexpr std::size_t introductionOption = 5;
constexpr std::size_t statusArtGroup = 4;
constexpr std::int16_t screenWidth = 320;
constexpr std::int16_t screenHeight = 200;
constexpr auto pageLayout = CockpitPageLayout::recoveredStartup;
enum class ExecutionPass { preflight, live };

std::vector<std::vector<NormalWorkOperation>> packets(const std::vector<NormalWorkOperation>& operations,
    const std::size_t budget) {
    return normalWorkPackets(operations,budget);
}
}

NativeSessionDelivery NativeGameplaySession::resumeRecoveredLevel1Retry(const std::size_t workPacketBudget) {
    if (!recoveredLoader_ || !recoveredController_ || !audio_->workPhase_ || !recoveredStartupPalette_
        || !pixels_ || !radar_ || !assets_.cockpitSettings || !level1Pages_ || initialized_
        || !recoveredContinue_ || !recoveredContinueState_
        || recoveredContinueState_->stage != ContinueStage::finished
        || recoveredContinue_->outcome != ContinueOutcome::yes
        || model_.phase() != GameplayPhase::continuePrompt
        || model_.recoveredContinueDecision() != ContinueDecision::yes
        || workPacketBudget == 0 || workPacketBudget > maximumPacketBudget) {
        throw std::invalid_argument("recovered retry requires the retained accepted Level1 Yes boundary");
    }
    //Validate the same controller before any live work; no new queue or held-key history.
    recoveredController_->nativeGuards({},{});
    Level1RetryProfile profile{};
    profile.decision = recoveredContinue_->outcome;
    profile.score = model_.score();
    profile.remainingContinues = static_cast<std::uint16_t>(model_.remainingContinues());
    profile.introduction = recoveredIntroduction_.value_or(
        static_cast<std::uint16_t>(assets_.cockpitSettings->options[introductionOption]));
    profile.currentHighscores = highscoreTable_;
    profile.currentSequencePublished = audio_->workPhase_->sequenceSelection
        && audio_->workPhase_->sequenceSelection->published;
    profile.selectedSystem = model_.level1State().selectedSystem();
    profile.systemLevels = model_.level1State().systemStatusLevels();
    profile.systemTimers = model_.level1State().systemTimers();
    profile.frontPage = pages_.frontPage();
    profile.backPage = pages_.backPage();
    profile.cachePage = pages_.cachePage();
    auto control = level1RankMissRetryOperations(profile);
    auto staged = std::make_unique<GameplayModel>(model_);
    staged->resumeRecoveredContinueRetry(); //The only constructor/ground RNG execution.
    const auto& observation = control.observation;
    const auto& level = staged->level1State();
    if (staged->ticks() != model_.ticks() || level.loopTick() != observation.loopTick
        || staged->remainingContinues() != observation.remainingContinues
        || staged->score() != observation.score || staged->playerArmor() != observation.armor
        || level.systemTimers() != observation.systemTimers) {
        throw std::logic_error("recovered retry model differs from the source reset observation");
    }
    auto loader = planLevel1LoaderPages(assets_,*staged,pages_,*pixels_,Level1LoaderPageDomain::recoveredRetry);
    const auto score = scorePageEffects(staged->score(),assets_.scoreFont,loader.pages.backPage(),
        loader.pages.frontPage(),pageLayout);
    const auto status = statusPageEffects(level.systemStatusLevels(),level.systemTimers(),level.selectedSystem(),
        assets_.artGroups.at(statusArtGroup),loader.pages.backPage(),loader.pages.frontPage(),pageLayout);
    staged->completeRecoveredLevel1Retry();

    NativeRetryRun run;
    run.firstSample = clock_.elapsedSamples();
    run.control = control;
    auto phase = *audio_->workPhase_;
    auto palette = *recoveredStartupPalette_;
    auto pixels = *pixels_;
    auto roles = pages_;
    std::vector<NativeTimedFrame> presentations;
    const auto execute = [&](const ExecutionPass pass) {
        phase = *audio_->workPhase_;
        palette = *recoveredStartupPalette_;
        pixels = *pixels_;
        roles = pages_;
        const auto append = [&](ClockedPresentationWork work) {
            phase = work.timing.phase;
            run.samples.insert(run.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
            run.work.push_back(std::move(work));
        };
        const auto render = [&](const std::vector<NormalWorkOperation>& operations) {
            for (const auto& packet : packets(operations,workPacketBudget)) {
                if (pass == ExecutionPass::preflight) { phase = normalWorkTiming(phase,packet,workHorizon).phase; }
                else { append(clock_.renderWork(phase,packet,workHorizon)); }
            }
        };
        const auto publish = [&]() {
            if (pass != ExecutionPass::live) { return; }
            IndexedScreen indexed;
            indexed.pixels.reserve(screenWidth*screenHeight);
            for (std::int16_t y = 0; y < screenHeight; ++y) {
                for (std::int16_t x = 0; x < screenWidth; ++x) {
                    indexed.pixels.push_back(pixels.read(roles.frontPage(),{x,y}));
                }
            }
            indexed.palette = expandVgaDacPalette(std::vector<std::uint8_t>(palette.dac.colors.begin(),palette.dac.colors.end()));
            presentations.push_back({clock_.elapsedSamples(),std::move(indexed)});
        };
        for (const auto& segment : control.segments) {
            render(segment.operations);
            if (!segment.guestElapsedQualified) {
                //The approved native resource/state/HUD provider policy assigns no guessed guest interval.
                if (pass == ExecutionPass::live) { run.providers.push_back(segment); }
                if (segment.kind == RetryWorkKind::scoreHudProvider) { pixels.apply(score); publish(); }
                if (segment.kind == RetryWorkKind::statusHudProvider) { pixels.apply(status); publish(); }
                continue;
            }
            switch (segment.kind) {
            case RetryWorkKind::ordinary:
                break;
            case RetryWorkKind::stopPendingSound:
                if (pass == ExecutionPass::preflight) { phase = level1RetryMusicStopPhase(phase); }
                else {
                    auto stopped = stopLevel1RetryMusic(clock_,synth_,phase);
                    for (auto& work : stopped.work) { append(std::move(work)); }
                    phase = std::move(stopped.phase);
                }
                break;
            case RetryWorkKind::loaderPages:
                render(loader.operations);
                pixels = loader.pixels;
                roles = loader.pages;
                publish();
                break;
            case RetryWorkKind::musicActivation:
                if (pass == ExecutionPass::preflight) {
                    if (!phase.callback || !phase.sequenceSelection || !phase.sequenceSelection->level1RetryStoppedHandle) {
                        throw std::logic_error("retry preflight lost its source-cleared backing handle");
                    }
                    const auto prepared = synth_.previewRecoveredLevel1Retry(*phase.callback,
                        *phase.sequenceSelection->level1RetryStoppedHandle);
                    phase = level1RetryMusicActivationPhase(phase,prepared);
                } else {
                    auto activation = activateLevel1RetryMusic(clock_,synth_,phase);
                    for (auto& work : activation.work) { append(std::move(work)); }
                    phase = std::move(activation.phase);
                }
                break;
            case RetryWorkKind::palette:
                if (!segment.palette) { throw std::logic_error("missing retry palette child"); }
                if (pass == ExecutionPass::preflight) {
                    const auto uploaded = paletteClockTiming(phase,palette,*segment.palette,retainedLevel1PaletteHorizon(phase,workHorizon));
                    phase = uploaded.timing.phase;
                    palette = uploaded.state;
                } else {
                    auto uploaded = clock_.renderPalette(phase,palette,*segment.palette,retainedLevel1PaletteHorizon(phase,workHorizon));
                    palette = uploaded.state;
                    append(uploaded.work);
                    run.blackPalette = std::move(uploaded);
                    publish();
                }
                break;
            case RetryWorkKind::directReset:
                if (pass == ExecutionPass::preflight) { phase = level1DirectResetPhase(phase); }
                else {
                    auto reset = resetLevel1DirectEffects(clock_,synth_,phase);
                    for (auto& work : reset.work) { append(std::move(work)); }
                    phase = std::move(reset.phase);
                }
                break;
            default:
                throw std::logic_error("unsupported qualified retry child");
            }
        }
    };
    execute(ExecutionPass::preflight);
    execute(ExecutionPass::live);
    recoveredController_->restartNativeLoop();
    model_ = std::move(*staged);
    pages_ = roles;
    pixels_ = std::move(pixels);
    radar_ = loader.radar;
    cockpitBitmaps_ = std::move(loader.bitmaps);
    incomingIndicator_ = model_.lastCockpitInitialization().states[2];
    pageScore_ = model_.score();
    pageStatus_ = model_.level1State().systemStatusLevels();
    audio_->workPhase_ = std::move(phase);
    *recoveredStartupPalette_ = std::move(palette);
    recoveredIntroduction_ = observation.introduction;
    level1Cursor_ = model_.level1State().audioEvents().size();
    failureCursor_ = 0;
    failurePages_ = false;
    continuePages_ = false;
    recoveredContinueState_->stage = ContinueStage::inactive;
    recoveredFrame_.reset();
    pageProjection_ = NativePageProjection::retainedLevel1Loader;
    frame_.pixels = *retainedPagePixels(NativePageRole::front);
    const auto& colors = recoveredStartupPalette_->dac.colors;
    frame_.palette = expandVgaDacPalette(std::vector<std::uint8_t>(colors.begin(),colors.end()));
    presentations.push_back({clock_.elapsedSamples(),frame_});
    NativeSessionDelivery delivery{run.firstSample,{},run.samples,{}};
    delivery.presentations = std::move(presentations);
    recoveredRetry_ = std::move(run);
    return delivery;
}
} // namespace stellar7
