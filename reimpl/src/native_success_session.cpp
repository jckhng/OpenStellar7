#include "normal_work_packets.hpp"
#include "stellar7/native_session.hpp"
#include "stellar7/success_session.hpp"
#include "stellar7/success_audio_work.hpp"
#include "stellar7/success_initial_work.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int64_t workHorizon = 120000;
constexpr std::size_t maximumPacketBudget = 10000;
constexpr std::uint64_t residentForegroundBudget = 10000;
constexpr std::int16_t screenWidth = 320;
constexpr std::int16_t screenHeight = 200;
constexpr std::uint16_t successEntry = 0x6f52;
constexpr std::uint16_t contactCallerBegin = 0x0802;
constexpr std::uint16_t contactCallerEnd = 0x0809;
constexpr std::uint16_t contactCallerDispatches = 3;
constexpr std::uint16_t type25Argument = 0;
constexpr std::uint64_t cockpitCallerDispatches = 1;
constexpr std::uint64_t sightCallerDispatches = 2;
constexpr std::uint64_t radarCallerDispatches = 1;
constexpr std::uint16_t successViewportHeight = 146;
enum class ExecutionPass { preflight, live };

std::vector<std::vector<NormalWorkOperation>> packets(const std::vector<NormalWorkOperation>& operations,
    const std::size_t budget) {
    return normalWorkPackets(operations,budget);
}

std::vector<NormalWorkOperation> callee(const std::vector<NormalWorkOperation>& operations,
    const std::uint64_t callerDispatches) {
    auto result = operations;
    if (result.empty() || result.front().kind != NormalWorkKind::instructions
        || result.front().count < callerDispatches) {
        throw std::invalid_argument("success display child lacks its qualified caller prefix");
    }
    result.front().count -= callerDispatches;
    if (result.front().count == 0) { result.erase(result.begin()); }
    return result;
}

SuccessControlPages observation(const CockpitPageState& pages) {
    return {pages.backPage(),pages.frontPage(),pages.cachePage(),pages.drawPage(),pages.copyPage()};
}

void store(CockpitPageState& pages, const SuccessControlStore& effect) {
    if (effect.kind == SuccessControlStoreKind::warningClear) {
        //129C is a write-only source receipt; model entry owns that receipt.
        if (effect.value != 0 || effect.sourcePc != 0x6f5b) {
            throw std::invalid_argument("unexpected success warning store");
        }
        return;
    }
    if (effect.kind == SuccessControlStoreKind::copyPage) {
        if (effect.value == pages.cachePage()) { pages.selectCockpitCopy(); }
        else if (effect.value == pages.backPage()) { pages.selectBackCopy(); }
        else { throw std::invalid_argument("unexpected success copy selection"); }
        return;
    }
    if (effect.kind != SuccessControlStoreKind::drawPage) {
        throw std::invalid_argument("unexpected success store kind");
    }
    if (effect.value == pages.cachePage()) { pages.selectCockpitDraw(); }
    else if (effect.value == pages.backPage()) { pages.beginFrame(); }
    else if (effect.value == pages.frontPage()) { pages.selectFront(); }
    else { throw std::invalid_argument("unexpected success draw selection"); }
}
}

NativeSessionDelivery NativeGameplaySession::advanceRecoveredSuccess(const std::size_t workPacketBudget) {
    const bool entering = !recoveredSuccessState_;
    if (!recoveredLoader_ || !recoveredController_ || !audio_->workPhase_ || !recoveredStartupPalette_
        || !pixels_ || !radar_ || !assets_.cockpitSettings || !level1Pages_ || initialized_
        || model_.level1State().levelIndex() != 0 || workPacketBudget == 0 || workPacketBudget > maximumPacketBudget
        || assets_.soundSequences.empty()
        || (!entering && (model_.phase() != GameplayPhase::successPresentation || !successPalette_
            || !((recoveredSuccessState_->control.stage == SuccessControlStage::polling
                    && !recoveredSuccessState_->paletteContinuation)
                || (recoveredSuccessState_->control.stage == SuccessControlStage::awaitingPalette
                    && recoveredSuccessState_->paletteContinuation))))) {
        throw std::invalid_argument("recovered success requires the suspended Level1 contact and retained owners");
    }
    auto state = recoveredSuccessState_.value_or(NativeSuccessState{});
    auto staged = std::make_unique<GameplayModel>(model_);
    std::optional<SuccessDisplayWork> display;
    if (entering) {
        const auto contact = model_.recoveredSuccessContact();
        if (!contact || contact->sourceEntry != successEntry || contact->argument != type25Argument
            || contact->callerBegin != contactCallerBegin || contact->callerEnd != contactCallerEnd
            || contact->callerDispatches != contactCallerDispatches || successPalette_) {
            throw std::invalid_argument("recovered success requires the actual Type25 green caller");
        }
        state.caller = *contact;
        state.control.channel = SuccessPaletteChannel::green;
        state.control.completion = SuccessPaletteCompletionSource::soundHandle;
        display = staged->beginRecoveredSuccess(assets_,pages_,*radar_,cockpitBitmaps_);
        if (staged->phase() != GameplayPhase::successPresentation || !display->radar.cpuWorkQualified
            || display->cockpit.state.pages != CockpitPageRelation::matching
            || staged->ticks() != model_.ticks()
            || staged->level1State().loopTick() != model_.level1State().loopTick()
            || staged->level1State().rng().calls() != model_.level1State().rng().calls()
            || staged->level1State().rng().indexA() != model_.level1State().rng().indexA()
            || staged->level1State().rng().indexB() != model_.level1State().rng().indexB()
            || staged->score() != model_.score() || staged->playerArmor() != model_.playerArmor()
            || display->cockpit.state.indicatorX != staged->level2CockpitIndicatorX()
            || display->cockpit.state.indicatorVelocity != staged->level2CockpitIndicatorVelocity()
            || display->sight.alternatePhase != staged->alternateSightPhase()) {
            throw std::invalid_argument("recovered success model lacks its qualified display children");
        }
    }
    if (state.control.channel != SuccessPaletteChannel::green
        || state.control.completion != SuccessPaletteCompletionSource::soundHandle) {
        throw std::invalid_argument("unsupported recovered Type25 success branch");
    }
    if (state.paletteContinuation) {
        const auto& cursor = *state.paletteContinuation;
        const bool initial = cursor.execution == SuccessPaletteExecution::initial;
        if (!successPalette_ || (!initial && cursor.execution != SuccessPaletteExecution::advance)
            || !audio_->workPhase_->successAudio
            || audio_->workPhase_->successAudio->stage != (initial ? SuccessAudioStage::playing : SuccessAudioStage::countdownStored)
            || cursor.completion != state.control.completion
            || (cursor.stage != SuccessPaletteResidentStage::waitRetrace
                && cursor.stage != SuccessPaletteResidentStage::upload)
            || cursor.state.channel != successPalette_->channel
            || cursor.state.values != successPalette_->values
            || cursor.state.directions != successPalette_->directions) {
            throw std::invalid_argument("recovered success lost its pending resident child");
        }
    }
    //These helpers include ordinary callers already owned by the success envelope.
    const auto cockpit = display ? callee(display->cockpit.operations,cockpitCallerDispatches)
        : std::vector<NormalWorkOperation>{};
    const auto sight = display ? callee(display->sight.operations,sightCallerDispatches)
        : std::vector<NormalWorkOperation>{};
    const auto radar = display ? callee(display->radar.operations,radarCallerDispatches)
        : std::vector<NormalWorkOperation>{};

    NativeSuccessRun run;
    run.firstSample = clock_.elapsedSamples();
    run.entered = entering;
    auto phase = *audio_->workPhase_;
    auto palette = *recoveredStartupPalette_;
    auto roles = pages_;
    auto pixels = *pixels_;
    auto oscillator = successPalette_;
    auto control = state.control;
    auto continuation = state.paletteContinuation;
    std::vector<NativeTimedFrame> presentations;
    const auto execute = [&](const ExecutionPass pass) {
        phase = *audio_->workPhase_;
        palette = *recoveredStartupPalette_;
        roles = pages_;
        pixels = *pixels_;
        oscillator = successPalette_;
        control = state.control;
        continuation = state.paletteContinuation;
        if (entering) { phase = bindSuccessAudioResource(phase,assets_.soundSequences.front()); }
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
            //Reads also preflight all displayed physical pixels before live audio.
            IndexedScreen indexed;
            indexed.pixels.reserve(screenWidth*screenHeight);
            for (std::int16_t y = 0; y < screenHeight; ++y) {
                for (std::int16_t x = 0; x < screenWidth; ++x) {
                    indexed.pixels.push_back(pixels.read(roles.frontPage(),{x,y}));
                }
            }
            indexed.palette = expandVgaDacPalette(std::vector<std::uint8_t>(palette.dac.colors.begin(),palette.dac.colors.end()));
            if (pass == ExecutionPass::live) { presentations.push_back({clock_.elapsedSamples(),std::move(indexed)}); }
        };
        const auto input = [&]() { return SuccessControlInput{control,observation(roles),0,true}; };
        std::optional<SuccessPaletteClockTiming> returned;
        const auto resumePalette = [&](const SuccessPaletteResidentCursor& incoming) {
            SuccessPaletteResidentCursor cursor;
            std::optional<SuccessPaletteBoundary> boundary;
            if (pass == ExecutionPass::preflight) {
                auto value = successPaletteStepTiming(phase,incoming,palette,residentForegroundBudget,workHorizon);
                phase = value.palette.timing.phase;
                palette = value.palette.state;
                cursor = std::move(value.cursor);
                boundary = value.boundary;
            } else {
                auto value = clock_.renderSuccessPaletteStep(phase,incoming,palette,residentForegroundBudget,workHorizon);
                palette = value.palette.state;
                cursor = value.cursor;
                boundary = value.boundary;
                append(value.palette.work);
                if (boundary) {
                    run.palettes.push_back({value.cursor.state,std::move(value.palette),*boundary});
                }
            }
            oscillator = cursor.state;
            if (!boundary) {
                continuation = std::move(cursor);
                return;
            }
            continuation.reset();
            SuccessPaletteClockTiming receipt;
            receipt.state = cursor.state;
            receipt.boundary = *boundary;
            receipt.palette.timing.phase = phase;
            receipt.palette.state = palette;
            returned = std::move(receipt);
            if (*boundary != SuccessPaletteBoundary::presentationComplete) { publish(); }
        };
        const auto executePlan = [&](const SuccessControlWork& plan) {
            for (std::size_t index = 0; index < plan.steps.size(); ++index) {
                if (plan.paletteInitializationStepOffset == index) { oscillator = plan.initializedPalette; }
                const auto& step = plan.steps[index];
                render(step.operations);
                for (const auto& effect : step.stores) { store(roles,effect); }
                if (!step.child) { continue; }
                const auto& child = *step.child;
                if (pass == ExecutionPass::live) { run.children.push_back(child); }
                switch (child.kind) {
                case SuccessControlChildKind::entryStop:
                    render(successAudioOperations(SuccessAudioChild::entryStop));
                    break;
                case SuccessControlChildKind::activateSuccess:
                    render(successAudioOperations(SuccessAudioChild::activation));
                    break;
                case SuccessControlChildKind::countdownStore:
                    if (child.arguments[0] != successRawCountdown) {
                        throw std::logic_error("success envelope lost atomic7072 ownership");
                    }
                    render(successAudioOperations(SuccessAudioChild::countdownStore));
                    break;
                case SuccessControlChildKind::returnStop:
                    render(successAudioOperations(SuccessAudioChild::returnStop));
                    break;
                case SuccessControlChildKind::cockpit:
                    if (!display) { throw std::logic_error("success cockpit repeated outside entry"); }
                    render(cockpit);
                    pixels.apply(cockpitPageEffects(display->cockpit,cockpitBitmaps_,roles.cachePage(),roles.drawPage()));
                    break;
                case SuccessControlChildKind::sight:
                    if (!display) { throw std::logic_error("success sight repeated outside entry"); }
                    render(sight);
                    for (const auto& pixel : display->sight.pixels) { pixels.write(roles.drawPage(),pixel.point,pixel.color); }
                    break;
                case SuccessControlChildKind::radar:
                    if (!display) { throw std::logic_error("success radar repeated outside entry"); }
                    render(radar);
                    pixels.apply(display->radar);
                    if (display->radar.pageCopy) { roles.selectArmorCopy(); }
                    break;
                case SuccessControlChildKind::planarCopy: {
                    const auto& a = child.arguments;
                    if (a[0] != 0 || a[1] != 0 || a[2] != screenWidth
                        || (a[3] != successViewportHeight && a[3] != screenHeight)
                        || a[4] != roles.copyPage() || a[5] != roles.drawPage()) {
                        throw std::invalid_argument("success copy changed its source rectangle or page selectors");
                    }
                    render({{NormalWorkKind::instructions,1}}); //22A1 resident JMPF106A.
                    render(planarCopyOperations(0,screenWidth,static_cast<std::uint16_t>(a[3])));
                    pixels.apply(std::vector<IndexedPageEffect>{RadarPageCopy{roles.copyPage(),roles.drawPage(),
                        {0,0},screenWidth,static_cast<std::uint16_t>(a[3])}});
                    if (roles.drawPage() == roles.frontPage()) { publish(); }
                    break;
                }
                case SuccessControlChildKind::residentPalette: {
                    if (!oscillator) { throw std::logic_error("success palette lost its source initialization"); }
                    const auto execution = static_cast<SuccessPaletteExecution>(child.arguments[0]);
                    if (execution != SuccessPaletteExecution::initial && execution != SuccessPaletteExecution::advance) {
                        throw std::invalid_argument("unsupported success palette child execution");
                    }
                    resumePalette({*oscillator,execution,control.completion,SuccessPaletteResidentStage::entry});
                    break;
                }
                default:
                    throw std::invalid_argument("unmapped success control child");
                }
            }
            control = plan.control;
            if (observation(roles).draw != plan.pages.draw || observation(roles).copy != plan.pages.copy) {
                throw std::logic_error("success control and retained page selectors differ");
            }
        };
        const bool initial = entering
            || (continuation && continuation->execution == SuccessPaletteExecution::initial);
        if (entering) {
            render({{NormalWorkKind::instructions,2}}); //0802 PUSHDI /0803 CALL6F52.
            executePlan(successInitialEntryOperations(input()));
        } else if (continuation) {
            const auto pending = *continuation;
            resumePalette(pending);
        } else {
            executePlan(successPollOperations(input()));
        }
        if (!continuation) {
            if (!returned) { throw std::logic_error("success lacks its actual resident child return"); }
            executePlan(initial ? successInitialPaletteReturnOperations(input(),*returned)
                : successPaletteReturnOperations(input(),*returned));
        }
        if (control.stage == SuccessControlStage::finished) {
            render({{NormalWorkKind::instructions,1}}); //0806 ADDSP,2; parent resumes0809.
        }
        publish();
    };
    execute(ExecutionPass::preflight);
    const auto expectedStage = control.stage;
    const auto expectedContinuation = continuation;
    if (expectedStage == SuccessControlStage::finished) { staged->finishRecoveredSuccess(); }
    execute(ExecutionPass::live);
    if (control.stage != expectedStage || continuation.has_value() != expectedContinuation.has_value()
        || (continuation && (continuation->stage != expectedContinuation->stage
            || continuation->execution != expectedContinuation->execution
            || continuation->completion != expectedContinuation->completion
            || continuation->state.values != expectedContinuation->state.values
            || continuation->state.directions != expectedContinuation->state.directions))) {
        throw std::logic_error("success live branch differs from preflight");
    }

    model_ = std::move(*staged);
    pixels_ = std::move(pixels);
    pages_ = roles;
    if (display) {
        radar_ = std::move(display->radarState);
        incomingIndicator_ = {display->cockpit.state.indicatorX,display->cockpit.state.indicatorVelocity};
    }
    successPalette_ = std::move(oscillator);
    *recoveredStartupPalette_ = std::move(palette);
    audio_->workPhase_ = std::move(phase);
    state.control = control;
    state.paletteContinuation = std::move(continuation);
    recoveredSuccessState_ = state;
    run.state = state;
    run.finished = control.stage == SuccessControlStage::finished;
    run.suspended = state.paletteContinuation.has_value();
    pageProjection_ = NativePageProjection::retainedLevel1Success;
    frame_ = presentations.back().indexed;
    NativeSessionDelivery delivery{run.firstSample,{},run.samples,{}};
    delivery.presentations = std::move(presentations);
    recoveredSuccess_ = std::move(run);
    return delivery;
}
} // namespace stellar7
