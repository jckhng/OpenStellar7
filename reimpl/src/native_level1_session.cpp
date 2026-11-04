#include "normal_work_packets.hpp"
#include "playable_startup_provider.hpp"
#include "stellar7/audio_program_work.hpp"
#include "stellar7/native_session.hpp"
#include "playable_detail_render.hpp"
#include "retained_level1_clock_domain.hpp"
#include "stellar7/level1_loader_work.hpp"
#include "stellar7/level1_frame.hpp"
#include "stellar7/audio_direct_work.hpp"
#include "stellar7/enemy20_audio_work.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int64_t workHorizon = 120000;
constexpr std::int64_t programPageFlipHorizon = 2 * workHorizon;
constexpr std::size_t maximumPacketBudget = 10000;
constexpr std::int16_t screenWidth = 320;
constexpr std::int16_t screenHeight = 200;
constexpr auto recoveredLayout = CockpitPageLayout::recoveredStartup;

SoundBlasterVolumeRow playerHitVolumeRow(const GameplayAssets& assets) {
    constexpr std::size_t imageGroup = 5;
    constexpr std::size_t imageFrame = 13;
    constexpr std::size_t imageWidth = 80;
    constexpr std::size_t imageHeight = 65;
    constexpr std::size_t planes = 4;
    constexpr std::size_t rowBytes = imageWidth / planes;
    constexpr std::size_t planeBytes = rowBytes * imageHeight;
    constexpr std::size_t firstPlanarByte = 1243;
    const auto& image = assets.artGroups.at(imageGroup).at(imageFrame);
    if (image.width != imageWidth || image.height != imageHeight || image.pixels.size() != imageWidth * imageHeight) {
        throw std::invalid_argument("recovered Level1 hit audio requires its source image storage");
    }
    SoundBlasterVolumeRow result{gameplayPlayerHitCallerVolume,{}};
    for (std::size_t index = 0; index < result.values.size(); ++index) {
        const auto planar = firstPlanarByte + index;
        const auto plane = planar / planeBytes;
        const auto local = planar % planeBytes;
        const auto row = local / rowBytes;
        const auto column = planes * (local % rowBytes) + plane;
        result.values[index] = image.pixels.at(row * imageWidth + column);
    }
    return result;
}

std::vector<NormalWorkOperation> deploymentAudioOperations(const Level1DeploymentAudioChild& child) {
    if (child.kind == Level1DeploymentAudioKind::directEvent6 && child.eventId == 6
        && child.callerBegin == 0xadb1 && child.callerEnd == 0xadbb && child.callerDispatches == 4) {
        return {{NormalWorkKind::gameplayGuardianFireEvent,1,child.eventId}};
    }
    if (child.kind == Level1DeploymentAudioKind::stopEvent17 && child.eventId == 17
        && child.callerBegin == 0xb071 && child.callerEnd == 0xb078 && child.callerDispatches == 3) {
        return {{NormalWorkKind::gameplayArrivalStopEvent,1,child.eventId}};
    }
    throw std::logic_error("unsupported deployment audio child ownership");
}

std::vector<std::vector<NormalWorkOperation>> packets(const std::vector<NormalWorkOperation>& operations,
    const std::size_t budget) {
    return normalWorkPackets(operations,budget);
}
}

NativeSessionDelivery NativeGameplaySession::initializeRecoveredLevel1(const std::size_t workPacketBudget) {
    if (!recoveredStartupPalette_ || !audio_->workPhase_ || recoveredLoader_ || initialized_
        || !pixels_ || !assets_.cockpitSettings || workPacketBudget == 0 || workPacketBudget > maximumPacketBudget
        || model_.phase() != GameplayPhase::level1 || model_.ticks() != 0
        || model_.level1State().rng().calls() != 16 || !model_.level1State().activeListOrder().empty()
        || assets_.cockpitDacPalette.size() != vgaPaletteBytes
        || !std::equal(assets_.cockpitDacPalette.begin(),assets_.cockpitDacPalette.end(),recoveredStartupPalette_->source.begin())) {
        throw std::invalid_argument("recovered Level1 initialization requires its fresh stopped startup owners/resources");
    }
    const auto hitVolume = playerHitVolumeRow(assets_);
    const auto enemy20Window = level1Enemy20CallerWindow(assets_.soundSequences.at(0));
    constexpr std::array<std::int16_t,6> mcbLoadedOptions{{1,0,1,1,0,0}};
    constexpr std::array<std::int16_t,6> mcbEffectiveOptions{{0,0,1,1,0,0}};
    constexpr std::int16_t loadedViewSize=81;
    constexpr std::int16_t fullViewSize=82;
    const auto enemy20Mcb=audio_->menuResources_ && audio_->menuResources_->streamsClosed
        && assets_.cockpitSettings->options==mcbLoadedOptions
        && audio_->menuResources_->configuration.options==mcbEffectiveOptions
        && (audio_->menuResources_->configuration.viewSize==loadedViewSize
            || audio_->menuResources_->configuration.viewSize==fullViewSize || audio_->playableDetailProfile())
        ? SoundBlasterEnemy20McbByte::fromCache(audio_->menuResources_->cache) : std::nullopt;
    const auto beginPages = (hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::storyPanels) || audio_->attractPagesTransferred_)
        ? NativeLevel1BeginPages::storyTransferred : NativeLevel1BeginPages::ordinary;
    const auto plan = hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::fullGraphicDetail)
        ? planFullDetailPlayableLevel1LoaderPages(assets_,model_,pages_,*pixels_,renderSettings().viewSize,
            Level1LoaderPageDomain::freshBegin,beginPages)
        : beginPages == NativeLevel1BeginPages::storyTransferred
            ? planStoryLevel1LoaderPages(assets_,model_,pages_,*pixels_,renderSettings().viewSize)
        : audio_->playablePreferences_ ? planPlayableLevel1LoaderPages(assets_,model_,pages_,*pixels_,renderSettings().viewSize)
        : planLevel1LoaderPages(assets_,model_,pages_,*pixels_);
    auto finalPixels = plan.pixels;
    finalPixels.apply(scorePageEffects(0,assets_.scoreFont,plan.pages.backPage(),plan.pages.frontPage(),recoveredLayout));
    const auto& state = model_.level1State();
    finalPixels.apply(statusPageEffects(state.systemStatusLevels(),state.systemTimers(),state.selectedSystem(),
        assets_.artGroups.at(4),plan.pages.backPage(),plan.pages.frontPage(),recoveredLayout));
    const bool nativeMusic = audio_->workPhase_->playableStartup;
    const auto publication = audio_->playableMusicPublication();
    const auto prepared = synth_.previewRecoveredLevel1(publication);
    auto controller = std::make_unique<FrameController>();
    // Native empty-input provider boundary; not imported BIOS queue ancestry.
    controller->initialize({FrameControllerOrigin::sourceColdDefaults,0,0x1e,0x1e,{},0,
        false,false,false,false,false,{}});

    // Native resource/state/HUD providers are explicit bootstrap limits. No elapsed
    // DOS cost is assigned to them; known source drawing/music/palette work follows.
    const auto beforeDrawing = std::vector<NormalWorkOperation>{{NormalWorkKind::instructions,10 + 5 + 3}};
    const auto afterDrawing = std::vector<NormalWorkOperation>{{NormalWorkKind::instructions,1}};
    const auto beforePalette = std::vector<NormalWorkOperation>{{NormalWorkKind::instructions,6 + 11}};
    const auto afterPalette = std::vector<NormalWorkOperation>{{NormalWorkKind::instructions,3 + 6}};
    const auto preloopProviders = std::vector<NormalWorkOperation>{{NormalWorkKind::instructions,3 + 1 + 1 + 1 + 3}};
    const PaletteRequest black{0,vgaPaletteColors,0,0};
    auto phase = *audio_->workPhase_;
    auto palette = *recoveredStartupPalette_;
    const auto qualify = [&](const auto& operations) {
        for (const auto& packet : packets(operations,workPacketBudget)) {
            phase = normalWorkTiming(phase,packet,workHorizon).phase;
        }
    };
    qualify(beforeDrawing);
    qualify(plan.operations);
    qualify(afterDrawing);
    phase = nativeMusic ? playableStartupLevel1Preparation(phase,prepared)
        : level1MusicActivationPhase(phase,prepared);
    qualify(beforePalette);
    const auto blackTiming = paletteClockTiming(phase,palette,black,workHorizon);
    phase = blackTiming.timing.phase;
    qualify(afterPalette);
    if (nativeMusic) {
        phase.callback = soundBlasterPlayableDirectResetState(*phase.callback);
        phase.sequenceSelection->level1DirectResetCompleted = true;
    } else {
        phase = level1DirectResetPhase(phase);
    }
    phase.callback->program->playerHitVolumeRow = hitVolume;
    phase.callback->program->enemy20CallerWindow = enemy20Window;
    phase.callback->program->enemy20McbByte = enemy20Mcb;
    qualify(preloopProviders);

    NativeLevel1LoaderRun run;
    run.playableAudio = nativeMusic;
    run.firstSample = clock_.elapsedSamples();
    phase = *audio_->workPhase_;
    const auto append = [&](ClockedPresentationWork work) {
        phase = work.timing.phase;
        run.samples.insert(run.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
        run.work.push_back(std::move(work));
    };
    const auto render = [&](const auto& operations) {
        for (const auto& packet : packets(operations,workPacketBudget)) { append(clock_.renderWork(phase,packet,workHorizon)); }
    };
    render(beforeDrawing);
    render(plan.operations);
    render(afterDrawing);
    if (nativeMusic) {
        //Native17E5 allocation/publication provider; no selected0325 or elapsed CPU receipt is minted.
        const auto allocated = synth_.prepareRecoveredLevel1(publication);
        phase = playableStartupLevel1Preparation(phase,allocated);
        if (publication == PlayableMusicPublication::current) {
            phase.callback = synth_.setPlayableMusicPublication(publication);
        } else {
            phase.callback = synth_.callbackWorkState();
        }
    } else {
        auto music = activateLevel1Music(clock_,synth_,phase);
        for (auto& work : music.work) { append(std::move(work)); }
    }
    render(beforePalette);
    auto uploaded = clock_.renderPalette(phase,palette,black,workHorizon);
    palette = uploaded.state;
    append(uploaded.work);
    run.blackPalette = std::move(uploaded);
    render(afterPalette);
    if (nativeMusic) {
        phase.callback = synth_.resetPlayableDirectEffects();
        phase.sequenceSelection->level1DirectResetCompleted = true;
    } else {
        auto reset = resetLevel1DirectEffects(clock_,synth_,phase);
        for (auto& work : reset.work) { append(std::move(work)); }
        // The reset's partial state commit has no extra dispatch; retain its returned phase.
        phase = reset.phase;
    }
    //The driver lookup wraps into this retained image allocation; publish its owned row.
    synth_.configurePlayerHitVolumeRow(hitVolume);
    phase.callback->program->playerHitVolumeRow = synth_.callbackWorkState().program->playerHitVolumeRow;
    phase = bindLevel1Enemy20Audio(synth_,phase);
    if (enemy20Mcb) {
        phase = bindLevel1Enemy20McbAudio(synth_,phase,*enemy20Mcb);
    }
    render(preloopProviders);
    audio_->workPhase_ = phase;
    *recoveredStartupPalette_ = std::move(palette);
    pages_ = plan.pages;
    pixels_ = std::move(finalPixels);
    radar_ = plan.radar;
    cockpitBitmaps_ = plan.bitmaps;
    incomingIndicator_ = model_.lastCockpitInitialization().states[2];
    pageScore_ = model_.score();
    pageStatus_ = state.systemStatusLevels();
    level1Cursor_ = state.audioEvents().size();
    pageProjection_ = NativePageProjection::retainedLevel1Loader;
    frame_.pixels = *retainedPagePixels(NativePageRole::front);
    const auto& colors = recoveredStartupPalette_->dac.colors;
    frame_.palette = expandVgaDacPalette(std::vector<std::uint8_t>(colors.begin(),colors.end()));
    NativeSessionDelivery delivery{run.firstSample,{},run.samples,{}};
    recoveredLoader_ = std::move(run);
    recoveredController_ = std::move(controller);
    return delivery;
}

NativeSessionDelivery NativeGameplaySession::advanceRecoveredLevel1Frame(const FrameCopyProfile copyProfile,
    const std::size_t workPacketBudget, const GameplayInput& input) {
    if (input.systemAction != GameplaySystemAction::none) {
        throw std::invalid_argument("source Level1 frames reject queued system actions");
    }
    if (!recoveredLoader_ || !recoveredController_ || !audio_->workPhase_ || !recoveredStartupPalette_
        || !pixels_ || !radar_ || !assets_.cockpitSettings || initialized_
        || workPacketBudget == 0 || workPacketBudget > maximumPacketBudget
        || recoveredController_->expectedLoopTick() != model_.level1State().loopTick()
        || level1Cursor_ != model_.level1State().audioEvents().size()) {
        throw std::invalid_argument("recovered Level1 frame requires completed loader and retained owners");
    }
    if (input.exit || input.confirm) {
        throw std::invalid_argument("recovered Level1 frame requires supported movement/steering input");
    }
    constexpr std::size_t downKey = 1;
    constexpr std::size_t upKey = 4;
    constexpr std::size_t leftKey = 6;
    constexpr std::size_t rightKey = 7;
    constexpr std::size_t fireKey = 8;
    std::array<bool, 9> keys{};
    keys[downKey] = input.reverse;
    keys[upKey] = input.forward;
    keys[leftKey] = input.left;
    keys[rightKey] = input.right;
    keys[fireKey] = input.fire;
    auto staged = std::make_unique<GameplayModel>(model_);
    const auto plan = staged->advanceRecoveredLevel1Frame(assets_,*assets_.cockpitSettings,
        pages_,*radar_,*pixels_,recoveredController_->nativeGuards({},keys));
    return executeRecoveredLevel1Work(std::move(staged),plan,copyProfile,workPacketBudget,keys);
}

NativeSessionDelivery NativeGameplaySession::resumeRecoveredLevel1Success(const FrameCopyProfile copyProfile,
    const std::size_t workPacketBudget) {
    if (!recoveredLoader_ || !recoveredController_ || !audio_->workPhase_ || !recoveredStartupPalette_
        || !pixels_ || !radar_ || !assets_.cockpitSettings || initialized_
        || !recoveredSuccess_ || !recoveredSuccess_->finished
        || model_.phase() != GameplayPhase::successPresentation || !model_.level1State().missionActive()
        || workPacketBudget == 0 || workPacketBudget > maximumPacketBudget
        || recoveredController_->expectedLoopTick() != model_.level1State().loopTick()
        || level1Cursor_ != model_.level1State().audioEvents().size()) {
        throw std::invalid_argument("recovered success parent requires the actual retained modal return");
    }
    auto staged = std::make_unique<GameplayModel>(model_);
    const auto plan = staged->resumeRecoveredLevel1Success(assets_,*assets_.cockpitSettings,pages_,*radar_,*pixels_);
    if (plan.boundary != Level1GameplayFrameBoundary::successReturned || plan.successPaused) {
        throw std::logic_error("recovered success parent did not return at inactive0312");
    }
    return executeRecoveredLevel1Work(std::move(staged),plan,copyProfile,workPacketBudget,{});
}

NativeSessionDelivery NativeGameplaySession::executeRecoveredLevel1Work(std::unique_ptr<GameplayModel> staged,
    const Level1GameplayFrameWork& plan, const FrameCopyProfile copyProfile,
    const std::size_t workPacketBudget, const std::array<bool,9>& keys) {
    const bool prefixOnly = plan.boundary != Level1GameplayFrameBoundary::completeFrame;
    if ((plan.boundary == Level1GameplayFrameBoundary::successPaused) != plan.successPaused.has_value()) {
        throw std::logic_error("recovered frame pause metadata differs from its source boundary");
    }
    const auto postcopy = frameCopyOperations(copyProfile);
    const auto& stagedEvents = staged->level1State().audioEvents();
    std::size_t eventOrdinal = level1Cursor_;
    const auto checkEvents = [&](const auto& operations) {
        for (const auto& operation : operations) {
            if (operation.kind != NormalWorkKind::gameplayPrimaryFireEvent
                && operation.kind != NormalWorkKind::gameplayEnemyFireEvent
                && operation.kind != NormalWorkKind::gameplayPlayerHitEvent
                && operation.kind != NormalWorkKind::gameplayPlayerCombatEvent
                && operation.kind != NormalWorkKind::gameplayTargetHitEvent
                && operation.kind != NormalWorkKind::gameplayBonusPickupEvent
                && operation.kind != NormalWorkKind::gameplayArrivalEvent
                && operation.kind != NormalWorkKind::gameplayGuardianFireEvent
                && operation.kind != NormalWorkKind::gameplayGuardianDefeatEvent
                && operation.kind != NormalWorkKind::gameplaySolidContactEvent) { continue; }
            if (operation.count != 1 || eventOrdinal >= stagedEvents.size()
                || stagedEvents[eventOrdinal].loopTick != model_.level1State().loopTick()
                || stagedEvents[eventOrdinal].eventId != operation.value) {
                throw std::logic_error("recovered Level1 audio event order differs from its timed work");
            }
            ++eventOrdinal;
        }
    };
    const std::vector<Level1AudioEvent> deliveredEvents(stagedEvents.begin()+level1Cursor_,stagedEvents.end());
    const PageFlipClockState flipState{PageFlipSynchronization::enabled,plan.render.pages.backPage(),
        plan.render.pages.frontPage(),0x3d4,PageFlipLayout::recoveredStartup,PageFlipWaitDomain::retainedProgram};
    auto phase = *audio_->workPhase_;
    auto palette = *recoveredStartupPalette_;
    const auto validateFont = [](const ScoreFontProviderRead& font) {
        if (font.guestElapsedQualified || (font.selection != ScoreFontSelection::score
            && font.selection != ScoreFontSelection::primary)) {
            throw std::logic_error("invalid recovered score font provider");
        }
    };
    //One source sequence drives copied preflight and live execution; child work is excluded from flat prefix.
    const auto execute = [&](auto render, auto upload, auto display, auto provide, auto scoreComplete, auto drawingComplete, auto provideFont, auto provideDeployment, auto releaseSelector) {
        const auto& prefix = plan.projectilePrefix ? plan.projectilePrefix->operations : plan.prefix.operations;
        const auto scoreChild = [&](const ScoreWork& score) {
            if (score.fontSelections.size() != 2 || score.fontSelections[0].selection != ScoreFontSelection::score
                || score.fontSelections[1].selection != ScoreFontSelection::primary
                || score.drawPage != pages_.backPage() || score.copyPage != pages_.frontPage()) {
                throw std::logic_error("invalid recovered score child ownership");
            }
            std::size_t offset = 0;
            for (const auto& font : score.fontSelections) {
                if (font.operationOffset < offset || font.operationOffset > score.operations.size()) {
                    throw std::logic_error("invalid recovered score font work order");
                }
                render(std::vector<NormalWorkOperation>(score.operations.begin()+offset,score.operations.begin()+font.operationOffset));
                validateFont(font);
                provideFont(font);
                offset = font.operationOffset;
            }
            render(std::vector<NormalWorkOperation>(score.operations.begin()+offset,score.operations.end()));
            scoreComplete(score);
        };
        std::size_t offset = 0, paletteIndex = 0, scoreIndex = 0, drawingIndex = 0, deploymentIndex = 0;
        while (paletteIndex < plan.prefixPalettes.size() || scoreIndex < plan.prefixScores.size()
            || drawingIndex < plan.prefixDrawings.size() || deploymentIndex < plan.prefixDeploymentChildren.size()) {
            auto next = prefix.size() + 1;
            if (paletteIndex < plan.prefixPalettes.size()) {
                next = std::min(next,plan.prefixPalettes[paletteIndex].operationOffset);
            }
            if (scoreIndex < plan.prefixScores.size()) {
                next = std::min(next,plan.prefixScores[scoreIndex].operationOffset);
            }
            if (drawingIndex < plan.prefixDrawings.size()) {
                next = std::min(next,plan.prefixDrawings[drawingIndex].operationOffset);
            }
            if (deploymentIndex < plan.prefixDeploymentChildren.size()) {
                next = std::min(next,plan.prefixDeploymentChildren[deploymentIndex].operationOffset);
            }
            if (next < offset || next > prefix.size()) {
                throw std::logic_error("recovered Level1 prefix child has invalid work order");
            }
            render(std::vector<NormalWorkOperation>(prefix.begin()+offset,prefix.begin()+next));
            //Included drawing returns precede an excluded child at the same offset.
            while (drawingIndex < plan.prefixDrawings.size()
                && plan.prefixDrawings[drawingIndex].operationOffset == next) {
                drawingComplete(plan.prefixDrawings[drawingIndex++]);
            }
            const auto hasPalette = paletteIndex < plan.prefixPalettes.size()
                && plan.prefixPalettes[paletteIndex].operationOffset == next;
            const auto hasScore = scoreIndex < plan.prefixScores.size()
                && plan.prefixScores[scoreIndex].operationOffset == next;
            const auto hasDeployment = deploymentIndex < plan.prefixDeploymentChildren.size()
                && plan.prefixDeploymentChildren[deploymentIndex].operationOffset == next;
            if (static_cast<unsigned>(hasPalette) + static_cast<unsigned>(hasScore)
                + static_cast<unsigned>(hasDeployment) > 1) {
                throw std::logic_error("ambiguous recovered excluded child source order");
            }
            if (hasPalette) { upload(plan.prefixPalettes[paletteIndex++].request); }
            if (hasScore) { scoreChild(plan.prefixScores[scoreIndex++].work); }
            while (deploymentIndex < plan.prefixDeploymentChildren.size()
                && plan.prefixDeploymentChildren[deploymentIndex].operationOffset == next) {
                const auto& child = plan.prefixDeploymentChildren[deploymentIndex++].child;
                if (const auto* heap = std::get_if<Level1DeploymentHeapChild>(&child)) {
                    provideDeployment(*heap);
                } else if (const auto* audio = std::get_if<Level1DeploymentAudioChild>(&child)) {
                    render(deploymentAudioOperations(*audio));
                } else if (const auto* death = std::get_if<Level1GuardianDeathAudio>(&child)) {
                    if (death->event != 12 || death->callerBegin != 0x11f1
                        || death->callerEnd != 0x11fb || death->callerDispatches != 4) {
                        throw std::logic_error("guardian defeat child changed its source caller");
                    }
                    render(std::vector<NormalWorkOperation>{{NormalWorkKind::gameplayGuardianDefeatEvent,1,death->event}});
                } else if (const auto* success = std::get_if<Level1RewardSuccessChild>(&child)) {
                    if (!plan.successPaused || next != prefix.size() || success->argument != 0
                        || success->sourceEntry != 0x6f52 || success->callerBegin != 0x0802
                        || success->callerEnd != 0x0809 || success->callerDispatches != 3) {
                        throw std::logic_error("success child requires its suspended parent boundary");
                    }
                    //The modal executor owns this caller only after the prefix is committed.
                } else {
                    throw std::logic_error("unsupported recovered prefix child");
                }
            }
            offset = next;
        }
        render(std::vector<NormalWorkOperation>(prefix.begin()+offset,prefix.end()));
        if (prefixOnly) { return; }
        render(plan.render.operations);
        display();
        render(postcopy.operations);
        render(plan.tail.beforeEntryFade);
        if (plan.tail.entryFade) {
            for (std::uint16_t intensity = 0; intensity < 64; intensity += 4) {
                render(std::vector<NormalWorkOperation>{{NormalWorkKind::instructions,intensity == 0 ? 20U : 25U}});
                upload(PaletteRequest{0,vgaPaletteColors,0,static_cast<std::uint8_t>(intensity)});
            }
            render(std::vector<NormalWorkOperation>{{NormalWorkKind::instructions,9}});
        }
        const auto renderTail=[&](const auto& operations,const Level1TailSelectorSegment segment) {
            std::size_t offset=0;
            for(const auto& release:plan.tail.selectorReleases) {
                if(release.segment!=segment) {continue;}
                if(release.operationOffset<offset || release.operationOffset>operations.size()) {
                    throw std::logic_error("invalid ordinary selector release source cut");
                }
                render(std::vector<NormalWorkOperation>(operations.begin()+offset,operations.begin()+release.operationOffset));
                releaseSelector(release.call);offset=release.operationOffset;
            }
            render(std::vector<NormalWorkOperation>(operations.begin()+offset,operations.end()));
        };
        renderTail(plan.beforePeriodicProvider,Level1TailSelectorSegment::beforePeriodicProvider);
        if (plan.periodic && plan.periodic->provider) { provide(*plan.periodic->provider); }
        renderTail(plan.tail.afterEntryFade,Level1TailSelectorSegment::afterEntryFade);
    };
    const auto validateProvider = [&](const Level1SpawnProviderCall& provider) {
        if (!plan.periodic || !plan.periodic->spawn || provider.guestElapsedQualified
            || provider.requestedBytes != 1 || provider.sourceEntry != 0xdcc6
            || provider.entityIndex != plan.periodic->spawn->actor.allocationIndex
            || plan.periodic->spawn->actor.type != 11) {
            throw std::logic_error("invalid retained Level1 selector provider receipt");
        }
    };
    const auto validateDeployment = [&](const Level1DeploymentHeapChild& provider) {
        const auto& retained = model_.level1State().entities();
        const auto& next = staged->level1State();
        if (provider.guestElapsedQualified || provider.entityIndex >= next.entities().size()
            || provider.entityIndex != next.postWaveObjectAllocationIndex()
            || next.entities()[provider.entityIndex].type != 44) {
            throw std::logic_error("invalid retained guardian selector provider");
        }
        if (provider.kind == Level1DeploymentHeapKind::allocateSelector
            && provider.requestedBytes == 3 && provider.sourceEntry == 0xdcc6) { return; }
        if (provider.kind == Level1DeploymentHeapKind::releaseSelector
            && provider.requestedBytes == 0 && provider.sourceEntry == 0xdcca
            && provider.entityIndex < retained.size()) {
            const auto& prior = retained[provider.entityIndex];
            if (prior.primaryType11AnimationSelector
                || prior.renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation
                || prior.renderProfile == WorldActorRenderProfile::heapSelectorNoCallback) { return; }
        }
        throw std::logic_error("guardian selector provider lacks retained allocation ancestry");
    };
    const auto validateSelectorRelease=[&](const Level1SelectorReleaseCall& release) {
        const auto& retained=model_.level1State().entities();
        const auto& next=staged->level1State().entities();
        if(release.guestElapsedQualified || release.sourceEntry!=0xdcca || !plan.projectilePrefix
            || release.entityIndex>=retained.size() || release.entityIndex>=next.size()
            || release.entityIndex>=plan.projectilePrefix->entities.size()) {
            throw std::logic_error("ordinary selector release lacks its retained source identity");
        }
        const auto& prior=retained[release.entityIndex];
        const auto& dead=plan.projectilePrefix->entities[release.entityIndex];
        const auto& replacement=next[release.entityIndex];
        if(prior.type!=11 || prior.allocationIndex!=release.entityIndex
            || dead.allocationIndex!=release.entityIndex || !level1RetiredType11Selector(dead)
            || replacement.allocationIndex!=release.entityIndex || replacement.stateBytes4dTo52[0]!=255
            || !replacement.active() || (replacement.type!=1 && replacement.type!=3
                && replacement.type!=11 && replacement.type!=7)) {
            throw std::logic_error("ordinary13AC provider lacks canonical retired Type11 ancestry");
        }
    };
    execute(checkEvents,[](PaletteRequest) {},[]() {},validateProvider,
        [](const ScoreWork&) {},[](const Level1PrefixDrawingStep&) {},validateFont,validateDeployment,validateSelectorRelease);
    if (eventOrdinal != stagedEvents.size()) {
        throw std::logic_error("recovered Level1 audio event has no timed owner");
    }
    execute([&](const auto& operations) {
        for (const auto& packet : packets(operations,workPacketBudget)) {
            phase = normalWorkTiming(phase,packet,workHorizon).phase;
        }
    },[&](const PaletteRequest request) {
        const auto result = paletteClockTiming(phase,palette,request,retainedLevel1PaletteHorizon(phase,workHorizon));
        phase = result.timing.phase;
        palette = result.state;
    },[&]() { phase = pageFlipClockTiming(phase,flipState,programPageFlipHorizon).timing.phase; },validateProvider,[](const ScoreWork&) {},[](const Level1PrefixDrawingStep&) {},validateFont,validateDeployment,validateSelectorRelease);

    NativeLevel1FrameRun run;
    run.firstSample = clock_.elapsedSamples();
    run.loopTick = model_.level1State().loopTick();
    run.prefixRngCalls = plan.projectilePrefix ? plan.projectilePrefix->rng.calls() : plan.prefix.rng.calls();
    run.groundRngCalls = prefixOnly ? staged->level1State().rng().calls() : plan.render.ground.rng.calls();
    run.prefixOnly = prefixOnly;
    phase = *audio_->workPhase_;
    palette = *recoveredStartupPalette_;
    auto prefixPixels = *pixels_;
    std::vector<NativeTimedFrame> presentations;
    bool displayed = false;
    const auto publish = [&]() {
        const auto& source = displayed ? plan.render.pixels : prefixPixels;
        const auto visiblePage = displayed ? plan.render.pages.backPage() : pages_.frontPage();
        IndexedScreen indexed;
        indexed.pixels.reserve(screenWidth * screenHeight);
        for (std::int16_t y = 0; y < screenHeight; ++y) {
            for (std::int16_t x = 0; x < screenWidth; ++x) { indexed.pixels.push_back(source.read(visiblePage,{x,y})); }
        }
        indexed.palette = expandVgaDacPalette(std::vector<std::uint8_t>(palette.dac.colors.begin(),palette.dac.colors.end()));
        presentations.push_back({clock_.elapsedSamples(),std::move(indexed)});
    };
    const auto append = [&](ClockedPresentationWork work) {
        phase = work.timing.phase;
        run.samples.insert(run.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
        run.work.push_back(std::move(work));
    };
    execute([&](const auto& operations) {
        for (const auto& packet : packets(operations,workPacketBudget)) { append(clock_.renderWork(phase,packet,workHorizon)); }
    },[&](const PaletteRequest request) {
        auto uploaded = clock_.renderPalette(phase,palette,request,retainedLevel1PaletteHorizon(phase,workHorizon));
        palette = uploaded.state;
        append(uploaded.work);
        publish();
        run.palettes.push_back(std::move(uploaded));
    },[&]() {
        auto flipped = clock_.renderPageFlip(phase,flipState,programPageFlipHorizon);
        append(flipped.work);
        displayed = true;
        publish();
        run.flip = std::move(flipped);
    },[&](const Level1SpawnProviderCall& provider) {
        validateProvider(provider);
        //Native selector storage provider has no assigned guest elapsed cost.
        run.periodicProvider = provider;
    },[&](const ScoreWork& score) { prefixPixels.apply(score.effects); },
    [&](const Level1PrefixDrawingStep& drawing) { prefixPixels.apply(drawing.effects); },
    [&](const ScoreFontProviderRead& font) { run.scoreFontProviders.push_back(font); },
    [&](const Level1DeploymentHeapChild& provider) {
        validateDeployment(provider);
        //Native selector provider; omitted CRT guest elapsed remains unqualified.
        run.deploymentProviders.push_back({provider.entityIndex,provider.requestedBytes,
            provider.sourceEntry,provider.guestElapsedQualified});
    },[&](const Level1SelectorReleaseCall& release) {
        validateSelectorRelease(release);run.selectorReleases.push_back(release);
    });
    if (!prefixOnly) { recoveredController_->completeNativeTail(run.loopTick,plan.tail,keys); }
    model_ = std::move(*staged);
    pages_ = plan.render.pages;
    if (!prefixOnly) { pages_.flip(); }
    pixels_ = plan.render.pixels;
    if (plan.lamp) {
        pages_ = plan.lamp->pages;
        pixels_->apply(plan.lamp->effects);
    }
    radar_ = plan.render.radarState;
    if (!prefixOnly) { incomingIndicator_ = plan.render.presentationAfter.indicator; }
    audio_->workPhase_ = std::move(phase);
    *recoveredStartupPalette_ = std::move(palette);
    if (!prefixOnly) { pageProjection_ = NativePageProjection::retainedNativeLevel1; }
    frame_.pixels = *retainedPagePixels(NativePageRole::front);
    const auto& colors = recoveredStartupPalette_->dac.colors;
    frame_.palette = expandVgaDacPalette(std::vector<std::uint8_t>(colors.begin(),colors.end()));
    NativeSessionDelivery delivery{run.firstSample,{},run.samples,deliveredEvents};
    delivery.presentations = std::move(presentations);
    pageScore_ = model_.score();
    level1Cursor_ = model_.level1State().audioEvents().size();
    recoveredFrame_ = std::move(run);
    return delivery;
}

} // namespace stellar7
