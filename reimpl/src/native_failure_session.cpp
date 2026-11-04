#include "normal_work_packets.hpp"
#include "stellar7/native_session.hpp"
#include "retained_level1_clock_domain.hpp"
#include "stellar7/failure_gameplay_work.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int64_t workHorizon = 120000;
constexpr std::int64_t flipHorizon = 2 * workHorizon;
constexpr std::size_t maximumPacketBudget = 10000;
constexpr std::uint16_t debrisFirstBlockCapacity = 44;
constexpr std::int16_t screenWidth = 320;
constexpr std::int16_t screenHeight = 200;
constexpr Viewport fullHardwareViewport{0,319,0,199};
constexpr auto pageLayout = BlackoutPageLayout::recoveredStartup;
enum class ExecutionPass { preflight, live };

std::vector<std::vector<NormalWorkOperation>> packets(const std::vector<NormalWorkOperation>& operations,
    const std::size_t budget) {
    return normalWorkPackets(operations,budget);
}
}

NativeSessionDelivery NativeGameplaySession::advanceRecoveredFailure(const FrameCopyProfile copyProfile,
    const std::size_t workPacketBudget) {
    const auto entering=model_.phase()==GameplayPhase::failurePreExplosionBlackout;
    const auto level2Pages=model_.campaignLevel()==1 && recoveredLevel2Transition_ && recoveredLevel2Frame_
        && recoveredLevel2Frame_->returnedLoopTick==model_.level2LoopTick()
        && campaignResources_ && campaignResources_->level==1
        && assets_.campaignLevels.size()>1 && campaignResources_->models==&assets_.campaignLevels[1].models
        && campaignResources_->bitmaps==&assets_.campaignLevels[1].bitmapGroups
        && campaignResources_->music==&assets_.campaignLevels[1].musicSequence
        && (entering ? pageProjection_==NativePageProjection::retainedRecoveredLevel2Frame
            : failurePages_ && pageProjection_==NativePageProjection::retainedLevel1Destruction);
    const auto sourcePages=model_.failureFromLevel2() ? level2Pages : level1Pages_;
    if (!recoveredLoader_ || !recoveredController_ || !audio_->workPhase_ || !recoveredStartupPalette_
        || !pixels_ || !radar_ || initialized_ || !sourcePages
        || workPacketBudget == 0 || workPacketBudget > maximumPacketBudget
        || (model_.phase() != GameplayPhase::failurePreExplosionBlackout
            && model_.phase() != GameplayPhase::failureExplosion)) {
        throw std::invalid_argument("recovered destruction requires retained Level1 owners and a consumed lethal tail");
    }
    const auto postcopy = failureFrameCopyOperations(copyProfile);
    auto staged = std::make_unique<GameplayModel>(model_);
    const auto plan = staged->advanceRecoveredFailureStep(assets_,pages_.backPage(),failureBackgroundClip_);
    if (entering != plan.entry.has_value()) {
        throw std::logic_error("destruction plan changed its source entry phase");
    }
    if (entering && failureHeapGeneration_ == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("failure allocation generation overflow");
    }
    const auto generation = failureHeapGeneration_ + (entering ? 1 : 0);
    const auto& stagedEvents = staged->failureAudioEvents();
    const auto firstEvent = entering ? 0 : failureCursor_;
    if (firstEvent > stagedEvents.size() || (!entering && firstEvent != model_.failureAudioEvents().size())) {
        throw std::logic_error("recovered destruction lost its audio cursor");
    }
    auto eventOrdinal = firstEvent;
    for (const auto& step : plan.control.steps) {
        for (const auto operation : step.operations) {
            if (operation.kind != NormalWorkKind::failureExplosionEvent
                && operation.kind != NormalWorkKind::failureLevel2ExplosionEvent) { continue; }
            if (operation.count != 1 || operation.value != 6 || eventOrdinal >= stagedEvents.size()
                || stagedEvents[eventOrdinal].eventId != operation.value
                || stagedEvents[eventOrdinal].loopTick != static_cast<std::uint16_t>(staged->ticks())) {
                throw std::logic_error("recovered destruction audio differs from its timed owner");
            }
            ++eventOrdinal;
        }
    }
    if (eventOrdinal != stagedEvents.size()) {
        throw std::logic_error("recovered destruction audio has no timed owner");
    }

    NativeFailureFrameRun run;
    run.firstSample = clock_.elapsedSamples();
    run.sourceLoopTick = plan.control.expected.sourceLoopTick;
    run.completedDisplays = plan.control.control.display;
    run.entered = entering;
    run.finished = plan.kind == FailureGameplayStepKind::leave;
    std::vector<NativeTimedFrame> presentations;
    auto phase = *audio_->workPhase_;
    auto palette = *recoveredStartupPalette_;
    auto roles = pages_;
    auto pixels = *pixels_;
    auto hardwareClip = failureBackgroundClip_;

    const auto execute = [&](const ExecutionPass pass) {
        phase = *audio_->workPhase_;
        palette = *recoveredStartupPalette_;
        roles = pages_;
        pixels = *pixels_;
        hardwareClip = failureBackgroundClip_;
        std::array<bool,GameplayModel::failureDebrisCount> allocations{};
        if (!entering) { allocations.fill(model_.failureDebris().size() == allocations.size()); }
        const auto publish = [&]() {
            if (pass != ExecutionPass::live) { return; }
            IndexedScreen indexed;
            indexed.pixels.reserve(screenWidth * screenHeight);
            for (std::int16_t y = 0; y < screenHeight; ++y) {
                for (std::int16_t x = 0; x < screenWidth; ++x) {
                    indexed.pixels.push_back(pixels.read(roles.frontPage(),{x,y}));
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
        const auto render = [&](const std::vector<NormalWorkOperation>& operations) {
            for (const auto& packet : packets(operations,workPacketBudget)) {
                if (pass == ExecutionPass::preflight) {
                    phase = normalWorkTiming(phase,packet,workHorizon).phase;
                } else {
                    append(clock_.renderWork(phase,packet,workHorizon));
                }
            }
        };
        const auto instructions = [&](const std::uint64_t count) {
            render({{NormalWorkKind::instructions,count}});
        };
        const auto upload = [&](const PaletteRequest request) {
            if (pass == ExecutionPass::preflight) {
                const auto result = paletteClockTiming(phase,palette,request,retainedCampaignPaletteHorizon(phase,workHorizon));
                phase = result.timing.phase;
                palette = result.state;
                return;
            }
            auto result = clock_.renderPalette(phase,palette,request,retainedCampaignPaletteHorizon(phase,workHorizon));
            palette = result.state;
            append(result.work);
            publish();
            run.palettes.push_back(std::move(result));
        };
        const auto blackout = [&]() {
            BlackoutSurfaceState surface;
            surface.clipping = BlackoutClip::disabled;
            const auto source = blackoutSourcePlan(surface,roles.frontPage(),pageLayout);
            for (std::size_t index = 0; index < blackoutPaletteSteps; ++index) {
                instructions(source.beforePalette[index]);
                upload({0,vgaPaletteColors,0,static_cast<std::uint8_t>(63-index*4)});
            }
            render(source.clear);
            roles.selectFront();
            applyBlackoutClear(pixels,roles.frontPage(),pageLayout);
            publish();
        };
        const auto provider = [&](const FailureHeapWork& heap) {
            const auto slot = heap.provider.identity.slot;
            if (slot >= allocations.size() || heap.provider.identity.generation != generation
                || heap.provider.guestElapsedQualified
                || allocations[slot] == (heap.provider.kind == FailureHeapProviderKind::allocate)) {
                throw std::logic_error("invalid recovered destruction allocation lifetime");
            }
            render(heap.beforeProvider);
            //The staged typed allocation is the native provider. CRT elapsed is unqualified.
            allocations[slot] = heap.provider.kind == FailureHeapProviderKind::allocate;
            if (pass == ExecutionPass::live) { run.providers.push_back(heap.provider); }
            render(heap.afterProvider);
        };
        const auto control = [&](const FailureControlWork& work) {
            for (const auto& step : work.steps) {
                render(step.operations);
                if (!step.child) { continue; }
                const auto& child = *step.child;
                switch (child.kind) {
                case FailureControlChildKind::loadedPalette:
                    upload({static_cast<std::uint16_t>(child.arguments[0]),static_cast<std::uint16_t>(child.arguments[1]),
                        static_cast<std::uint8_t>(child.arguments[2]),static_cast<std::uint8_t>(child.arguments[3])});
                    break;
                case FailureControlChildKind::blackout:
                    blackout();
                    break;
                case FailureControlChildKind::pageClear:
                    if (child.ordinal > 1) { throw std::logic_error("invalid destruction page clear"); }
                    if (child.ordinal == 0) { roles.selectFront(); } else { roles.beginFrame(); }
                    instructions(8); //6622..662E, through the rectangle CALLF.
                    render(fullFrontClearOperations());
                    instructions(2); //6633 cleanup/RET.
                    applyBlackoutClear(pixels,roles.drawPage(),pageLayout);
                    publish();
                    break;
                case FailureControlChildKind::modelViewport:
                    instructions(14); //4034..4054; the model bridge owns the resulting view.
                    break;
                case FailureControlChildKind::hardwareViewport:
                    instructions(6); //5BFE..5C12.
                    hardwareClip = fullHardwareViewport;
                    break;
                case FailureControlChildKind::horizon:
                    if (!plan.horizon) { throw std::logic_error("missing destruction horizon"); }
                    roles.beginFrame();
                    render(plan.horizon->operations);
                    for (const auto pixel : plan.horizon->pixels) { pixels.write(roles.drawPage(),pixel.point,pixel.color); }
                    hardwareClip = plan.horizon->viewport;
                    break;
                case FailureControlChildKind::heapCreate: {
                    const auto& actor = work.expected.debris.at(child.ordinal);
                    provider(failureDebrisCreateOperations({generation,static_cast<std::uint16_t>(child.ordinal)},actor.type,
                        debrisFirstBlockCapacity,assets_.models.at(actor.type)));
                    break;
                }
                case FailureControlChildKind::heapRelease:
                    provider(failureDebrisReleaseOperations({generation,static_cast<std::uint16_t>(child.ordinal)}));
                    break;
                case FailureControlChildKind::scene:
                    if (!plan.scene || !plan.view) { throw std::logic_error("missing destruction scene"); }
                    render(plan.sceneOperations);
                    for (const auto pixel : plan.scene->pixels) { pixels.write(pixel.page,pixel.point,pixel.color); }
                    hardwareClip = plan.view->view.viewport;
                    break;
                case FailureControlChildKind::pageFlip: {
                    const PageFlipClockState state{PageFlipSynchronization::enabled,roles.backPage(),roles.frontPage(),0x3d4,
                        PageFlipLayout::recoveredStartup,PageFlipWaitDomain::retainedProgram};
                    if (pass == ExecutionPass::preflight) {
                        phase = pageFlipClockTiming(phase,state,flipHorizon).timing.phase;
                    } else {
                        auto result = clock_.renderPageFlip(phase,state,flipHorizon);
                        append(result.work);
                        run.flip = std::move(result);
                    }
                    roles.flip();
                    publish();
                    break;
                }
                case FailureControlChildKind::frameClear:
                    render(postcopy.operations);
                    break;
                case FailureControlChildKind::entryFade:
                    for (std::uint16_t intensity = 0; intensity < 64; intensity += 4) {
                        instructions(intensity == 0 ? 20 : 25); //9D9E loop and resident1B38 prefix.
                        upload({0,vgaPaletteColors,0,static_cast<std::uint8_t>(intensity)});
                    }
                    instructions(9); //Last resident return, loop test and9DBA..9DBB.
                    break;
                }
            }
        };
        if (plan.entry) { control(*plan.entry); }
        control(plan.control);
        const bool ownsDebris = std::all_of(allocations.begin(),allocations.end(),[](const bool allocated) { return allocated; });
        const bool released = std::none_of(allocations.begin(),allocations.end(),[](const bool allocated) { return allocated; });
        if (run.finished ? !released : !ownsDebris) {
            throw std::logic_error("destruction source and native allocation lifetimes differ");
        }
    };
    execute(ExecutionPass::preflight);
    execute(ExecutionPass::live);

    model_ = std::move(*staged);
    pages_ = roles;
    pixels_ = std::move(pixels);
    failureBackgroundClip_ = hardwareClip;
    failureHeapGeneration_ = generation;
    failurePages_ = true;
    audio_->workPhase_ = std::move(phase);
    *recoveredStartupPalette_ = std::move(palette);
    failureCursor_ = model_.failureAudioEvents().size();
    pageProjection_ = run.finished ? NativePageProjection::retainedLevel1DestructionClear
        : NativePageProjection::retainedLevel1Destruction;
    frame_.pixels = *retainedPagePixels(NativePageRole::front);
    const auto& colors = recoveredStartupPalette_->dac.colors;
    frame_.palette = expandVgaDacPalette(std::vector<std::uint8_t>(colors.begin(),colors.end()));
    NativeSessionDelivery delivery{run.firstSample,{},run.samples,
        std::vector<Level1AudioEvent>(model_.failureAudioEvents().begin()+firstEvent,model_.failureAudioEvents().end())};
    delivery.presentations = std::move(presentations);
    recoveredFailure_ = std::move(run);
    return delivery;
}

} // namespace stellar7
