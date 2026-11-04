#include "normal_work_packets.hpp"
#include "stellar7/native_session.hpp"
#include "stellar7/level2_music_work.hpp"
#include "stellar7/campaign_loader.hpp"
#include "level1_reward_model_bank.hpp"

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
constexpr std::int8_t ordinaryCategory = -1;
constexpr auto pageLayout = CockpitPageLayout::recoveredStartup;
enum class ExecutionPass { preflight, live };

std::vector<std::vector<NormalWorkOperation>> packets(const std::vector<NormalWorkOperation>& operations,
    const std::size_t budget) {
    return normalWorkPackets(operations,budget);
}

std::array<bool,9> heldKeys(const GameplayInput& input) {
    if (input.exit || input.confirm) {
        throw std::invalid_argument("transition tail admits movement/steering/fire only");
    }
    std::array<bool,9> keys{};
    keys[1]=input.reverse; keys[4]=input.forward; keys[6]=input.left; keys[7]=input.right; keys[8]=input.fire;
    return keys;
}

std::uint8_t retainedEnemyFlag(const Level1State& state) {
    //698A starts1 and clears for every FF in OLD45D2. The modal return/updater
    //does not change categories. This projects that receipt without redrawing.
    const auto objects=state.radarPresentationObjects();
    return std::any_of(objects.begin(),objects.end(),[](const auto& item) {
        return item.category==ordinaryCategory;
    }) ? 0 : 1;
}

void validateFont(const ScoreFontProviderRead& font) {
    if (font.guestElapsedQualified || (font.selection!=ScoreFontSelection::score && font.selection!=ScoreFontSelection::primary)) {
        throw std::logic_error("invalid transition font provider receipt");
    }
}
}

NativeSessionDelivery NativeGameplaySession::advanceRecoveredLevel2Transition(const std::size_t workPacketBudget,
    const GameplayInput& input) {
    const auto destinationLevel=campaignExitDestination(model_.level1State().levelIndex(),CampaignExit::greenReward);
    if (!recoveredLoader_ || !recoveredController_ || !audio_->workPhase_ || !recoveredStartupPalette_
        || !pixels_ || !radar_ || !assets_.cockpitSettings || !level1Pages_ || initialized_
        || recoveredLevel2Transition_ || !recoveredSuccess_ || !recoveredSuccess_->finished
        || !recoveredSuccessState_ || recoveredSuccessState_->control.stage!=SuccessControlStage::finished
        || model_.phase()!=GameplayPhase::successPresentation || model_.level1State().levelIndex()!=0
        || model_.level1State().missionActive() || !model_.level1State().successState()
        || recoveredController_->expectedLoopTick()!=model_.level1State().loopTick()
        || level1Cursor_!=model_.level1State().audioEvents().size()
        || workPacketBudget==0 || workPacketBudget>maximumPacketBudget
        || assets_.campaignLevels.size()<=destinationLevel) {
        throw std::invalid_argument("Level2 transition requires actual resumed inactive0312 and retained owners");
    }
    const auto keys=heldKeys(input);
    const auto guards=recoveredController_->nativeGuards({},keys);
    const auto sourceLoop=model_.level1State().loopTick();
    const auto sourceTicks=model_.ticks();
    const auto sharedCount=level1RewardModelCapacity(assets_);
    const std::vector<Model3d> shared(assets_.models.begin(),assets_.models.begin()+sharedCount);
    const auto& resources=assets_.campaignLevels.at(destinationLevel);
    if (resources.resourceName!="LEVEL2.RES" || resources.models.size()!=5 || resources.musicSequence.empty()) {
        throw std::invalid_argument("Level2 transition requires its owned destination resource bank");
    }

    //All semantic stages are projections on this copy. No live owner is touched
    //until every child/provider/page/clock contract has passed preflight.
    auto staged=std::make_unique<GameplayModel>(model_);
    auto tail=staged->advanceRecoveredLevel1SuccessTail(assets_,pages_,guards,retainedEnemyFlag(model_.level1State()));
    const auto& tailWork=tail.tail.periodic.tail;
    if (tailWork.entryFade || (tail.tail.periodic.periodic
            && (tail.tail.periodic.periodic->provider || tail.tail.periodic.periodic->spawn
                || tail.tail.periodic.periodic->outcome!=Level1PeriodicSpawnOutcome::limit))
        || !tail.tail.periodic.beforePeriodicProvider.empty()
        || (tail.tail.arrival && tail.tail.arrival->birth) || tailWork.missionActive
        || tailWork.failureRequested || tailWork.keyboardHead!=guards.keyboardHead || tailWork.consumedScan!=0
        || tailWork.state.loopTick!=static_cast<std::uint16_t>(sourceLoop+1) || staged->ticks()!=sourceTicks+1) {
        throw std::logic_error("inactive transition tail acquired an unsupported child or continuation");
    }
    auto tailPages=tail.lamp ? tail.lamp->pages : pages_;
    const auto introduction=recoveredIntroduction_.value_or(static_cast<std::uint16_t>(assets_.cockpitSettings->options[introductionOption]));
    const auto control=staged->recoveredLevel2TransitionOperations(tailPages,introduction);
    const auto& events=staged->level1State().audioEvents();
    const std::vector<Level1AudioEvent> deliveredEvents(events.begin()+level1Cursor_,events.end());
    std::size_t event=0;
    const auto checkEvents=[&](const auto& operations) {
        for (const auto& operation:operations) {
            if (operation.kind!=NormalWorkKind::gameplayPrimaryFireEvent
                && operation.kind!=NormalWorkKind::gameplayPlayerCombatEvent) { continue; }
            if (operation.count!=1 || event>=deliveredEvents.size()
                || deliveredEvents[event].loopTick!=sourceLoop || deliveredEvents[event].eventId!=operation.value) {
                throw std::logic_error("transition tail audio history differs from its timed child");
            }
            ++event;
        }
    };
    checkEvents(tailWork.beforeEntryFade); checkEvents(tailWork.afterEntryFade);
    if (event!=deliveredEvents.size()) { throw std::logic_error("transition tail audio has no source owner"); }

    staged->beginRecoveredLevel2Load();
    auto reset=staged->resetRecoveredLevel2Pools(shared,resources.models);
    const auto constructors=staged->constructRecoveredLevel2Actors(shared,resources.models);
    const auto loaderInput=staged->prepareRecoveredLevel2LoaderPages(*assets_.cockpitSettings);
    auto resetRadar=*radar_;
    for (const auto key:reset.projectionResetKeys) { resetRadar.initializeAllocation(key); }
    auto loaderPixels=*pixels_;
    if (tail.lamp) { loaderPixels.apply(tail.lamp->effects); }
    const auto loader=planLevel2LoaderPages(assets_,loaderInput,tailPages,loaderPixels,resetRadar);
    staged->completeRecoveredLevel2MusicActivation();
    const auto score=scoreOperations(staged->score(),0,0,assets_.scoreFont,loader.pages,ScoreAwardDomain::transitionDisplay);
    const auto& systems=staged->level1State();
    const auto status=statusPageEffects(systems.systemStatusLevels(),systems.systemTimers(),systems.selectedSystem(),
        assets_.artGroups.at(statusArtGroup),score.pages.backPage(),score.pages.frontPage(),pageLayout);
    const auto preloop=staged->completeRecoveredLevel2Preloop();
    if (staged->phase()!=GameplayPhase::level2PreloopReturn || staged->campaignLevel()!=destinationLevel
        || staged->ticks()!=sourceTicks+1 || staged->level2LoopTick()!=0 || !preloop.firstEntryDisplay
        || !preloop.firstPrefix.activeOrder.empty() || !preloop.firstPrefix.refreshRequested
        || preloop.allocationOrder!=std::vector<std::size_t>({0,1,2,3,4,5,6})
        || staged->remainingContinues()!=model_.remainingContinues()
        || staged->score()!=control.observation.score || staged->playerArmor()!=control.observation.armor
        || systems.systemTimers()!=control.observation.systemTimers) {
        throw std::logic_error("Level2 transition staging differs from its source preloop observation");
    }
    std::array<std::size_t,7> initialSlots{};
    std::array<std::int16_t,7> initialTypes{};
    auto incarnations=actorIncarnations_;
    auto actorTypes=actorTypes_;
    for (std::size_t index=0;index<initialSlots.size();++index) {
        const auto slot=staged->level2PresentationPoolSlot(index);
        if (!slot) { throw std::logic_error("Level2 constructor lost its existing physical slot"); }
        initialSlots[index]=*slot; initialTypes[index]=staged->level2PresentationEntities().at(index).type;
        if (*slot<ordinaryPoolSize_) {
            const auto incarnation=staged->level2PresentationActorIncarnation(index);
            if (!incarnation || *incarnation==0) { throw std::logic_error("Level2 constructor lost its physical incarnation"); }
            incarnations[*slot]=*incarnation; actorTypes[*slot]=initialTypes[index];
        }
    }

    NativeLevel2TransitionRun run;
    run.firstSample=clock_.elapsedSamples(); run.sourceLoopTick=sourceLoop; run.sourceGlobalTick=sourceTicks;
    run.destinationGlobalTick=staged->ticks(); run.control=control; run.preloop=preloop;
    run.projectionResetKeys=reset.projectionResetKeys;
    auto phase=*audio_->workPhase_;
    auto palette=*recoveredStartupPalette_;
    auto roles=pages_;
    auto pixels=*pixels_;
    auto radar=*radar_;
    auto oscillator=successPalette_;
    auto borrowed=campaignResources_;
    std::vector<NativeTimedFrame> presentations;
    const auto execute=[&](const ExecutionPass pass) {
        phase=*audio_->workPhase_; palette=*recoveredStartupPalette_; roles=pages_; pixels=*pixels_; radar=*radar_;
        oscillator=successPalette_; borrowed=campaignResources_;
        const auto append=[&](ClockedPresentationWork work) {
            phase=work.timing.phase;
            run.samples.insert(run.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
            run.work.push_back(std::move(work));
        };
        const auto render=[&](const std::vector<NormalWorkOperation>& operations) {
            for (const auto& packet:packets(operations,workPacketBudget)) {
                if (pass==ExecutionPass::preflight) { phase=normalWorkTiming(phase,packet,workHorizon).phase; }
                else { append(clock_.renderWork(phase,packet,workHorizon)); }
            }
        };
        const auto publish=[&]() {
            IndexedScreen indexed;
            indexed.pixels.reserve(screenWidth*screenHeight);
            for (std::int16_t y=0;y<screenHeight;++y) {
                for (std::int16_t x=0;x<screenWidth;++x) { indexed.pixels.push_back(pixels.read(roles.frontPage(),{x,y})); }
            }
            indexed.palette=expandVgaDacPalette(std::vector<std::uint8_t>(palette.dac.colors.begin(),palette.dac.colors.end()));
            if (pass==ExecutionPass::live) { presentations.push_back({clock_.elapsedSamples(),std::move(indexed)}); }
        };
        const auto provider=[&](const RetryWorkSegment& segment) {
            if (segment.guestElapsedQualified) { throw std::logic_error("qualified source child treated as native provider"); }
            if (pass==ExecutionPass::live) { run.providers.push_back(segment); }
        };
        const auto music=[&](const Level2MusicChild child) { render(level2MusicOperations(child)); };
        render(tailWork.beforeEntryFade); render(tailWork.afterEntryFade);
        if (tail.lamp) { pixels.apply(tail.lamp->effects); roles=tail.lamp->pages; publish(); }
        if (pass==ExecutionPass::live) { recoveredController_->completeNativeTail(sourceLoop,tailWork,keys); }
        bool resetDone=false,constructorsDone=false,pagesDone=false,musicDone=false,groundDone=false;
        for (std::size_t index=0;index<control.segments.size();++index) {
            const auto& segment=control.segments[index];
            render(segment.operations);
            switch (segment.kind) {
            case RetryWorkKind::ordinary: break;
            case RetryWorkKind::releasePaletteProvider:
                provider(segment); oscillator.reset(); break;
            case RetryWorkKind::detachLevelResourceProvider:
                provider(segment); borrowed.reset(); break;
            case RetryWorkKind::releaseDetachedResourceProvider:
            case RetryWorkKind::releaseMusic:
            case RetryWorkKind::selectScoreFontProvider:
            case RetryWorkKind::modelResourcesProvider:
                provider(segment); break;
            case RetryWorkKind::levelResourceBootstrapProvider:
                provider(segment);
                borrowed=CampaignResourceBorrow{destinationLevel,&resources.bitmapGroups,&resources.models,&resources.musicSequence};
                break;
            case RetryWorkKind::stopPendingSound:
                music(Level2MusicChild::emptyStop); break;
            case RetryWorkKind::modelReloadProvider:
                if (index!=control.poolResetSegment || resetDone) { throw std::logic_error("duplicate or reordered48D4 child"); }
                for (const auto& step:reset.reset.steps) {
                    if (const auto* operations=std::get_if<std::vector<NormalWorkOperation>>(&step)) { render(*operations); }
                    else if (pass==ExecutionPass::live) { run.selectorReleases.push_back(std::get<Level2SelectorReleaseReceipt>(step)); }
                }
                //Only now may the destination key convention adopt real reset ancestry.
                for (const auto key:reset.projectionResetKeys) { radar.initializeAllocation(key); }
                resetDone=true; break;
            case RetryWorkKind::levelMusicResourcesProvider:
                provider(segment); phase=bindLevel2MusicResource(phase,resources.musicSequence); break;
            case RetryWorkKind::retainedConstructorCompletionProvider:
                if (index!=control.constructorsSegment || !resetDone || constructorsDone) { throw std::logic_error("reordered Level2 constructors"); }
                render(constructors.operations); constructorsDone=true; break;
            case RetryWorkKind::loaderPages:
                if (index!=control.loaderPagesSegment || !constructorsDone || pagesDone) { throw std::logic_error("reordered50C4 pages"); }
                provider(segment); //Decoded cockpit resource replacement remains a native provider.
                render(loader.operations); pixels=loader.pixels; roles=loader.pages; radar=loader.radar;
                pagesDone=true; publish(); break;
            case RetryWorkKind::musicActivation:
                if (!pagesDone || musicDone) { throw std::logic_error("reordered5098 activation"); }
                music(Level2MusicChild::activation); musicDone=true;
                if (pass==ExecutionPass::live) { recoveredController_->restartNativeLoop(); }
                break;
            case RetryWorkKind::palette:
                if (!musicDone || !segment.palette) { throw std::logic_error("missing Level2 loader palette"); }
                if (pass==ExecutionPass::preflight) {
                    const auto upload=paletteClockTiming(phase,palette,*segment.palette,workHorizon);
                    phase=upload.timing.phase; palette=upload.state;
                } else {
                    auto upload=clock_.renderPalette(phase,palette,*segment.palette,workHorizon);
                    palette=upload.state; append(upload.work); run.palettes.push_back(std::move(upload));
                }
                publish(); break;
            case RetryWorkKind::directReset:
                music(Level2MusicChild::directReset); break;
            case RetryWorkKind::scoreHudProvider: {
                std::size_t offset=0;
                if (score.fontSelections.size()!=2 || score.fontSelections[0].selection!=ScoreFontSelection::score
                    || score.fontSelections[1].selection!=ScoreFontSelection::primary) { throw std::logic_error("invalid transition score font order"); }
                for (const auto& font:score.fontSelections) {
                    if (font.operationOffset<offset || font.operationOffset>score.operations.size()) { throw std::logic_error("invalid transition font offset"); }
                    render(std::vector<NormalWorkOperation>(score.operations.begin()+offset,score.operations.begin()+font.operationOffset));
                    validateFont(font);
                    if (pass==ExecutionPass::live) { run.scoreFontProviders.push_back(font); }
                    offset=font.operationOffset;
                }
                render(std::vector<NormalWorkOperation>(score.operations.begin()+offset,score.operations.end()));
                pixels.apply(score.effects); roles=score.pages;
                //ScoreWork keeps incoming FRONT/BACK roles; its explicit selectors
                //and8F40/9030 leave DRAW=BACK/COPY=FRONT at this provider return.
                roles.selectScoreCopy(); publish(); break;
            }
            case RetryWorkKind::retainedGroundInitializationProvider:
                if (index!=control.groundSegment || groundDone) { throw std::logic_error("duplicate or reordered4C16 child"); }
                render(preloop.ground.operations); groundDone=true; break;
            case RetryWorkKind::statusHudProvider:
                if (!groundDone) { throw std::logic_error("status preceded ground initialization"); }
                provider(segment); pixels.apply(status); publish(); break;
            case RetryWorkKind::controllerRebindProvider:
                if (!groundDone || segment.sourceEntry!=0x01d0 || segment.sourceResume!=0x01d0) { throw std::logic_error("transition crossed its before01D0 boundary"); }
                //Metadata adoption only. Source loop reset was already counted at50AC.
                break;
            default: throw std::logic_error("unsupported Level2 transition child");
            }
            if (pass==ExecutionPass::live && segment.kind!=RetryWorkKind::ordinary) {
                run.stages.push_back({segment.kind,segment.sourceEntry,clock_.elapsedSamples()});
            }
        }
        if (!resetDone || !constructorsDone || !pagesDone || !musicDone || !groundDone) { throw std::logic_error("incomplete Level2 transition envelope"); }
        publish();
    };
    execute(ExecutionPass::preflight);
    execute(ExecutionPass::live);

    model_=std::move(*staged); pages_=roles; pixels_=std::move(pixels); radar_=std::move(radar);
    cockpitBitmaps_=loader.bitmaps; incomingIndicator_=loaderInput.cockpit.states[2];
    audio_->workPhase_=std::move(phase); *recoveredStartupPalette_=std::move(palette);
    successPalette_=std::move(oscillator); campaignResources_=std::move(borrowed);
    recoveredSuccessState_.reset();
    initialActorCount_=initialSlots.size();
    initialPoolSlots_.fill(static_cast<std::size_t>(-1));initialActorTypes_.fill(-1);
    std::copy(initialSlots.begin(),initialSlots.end(),initialPoolSlots_.begin());
    std::copy(initialTypes.begin(),initialTypes.end(),initialActorTypes_.begin());
    actorIncarnations_=incarnations; actorTypes_=actorTypes;
    level1Pages_=false; failurePages_=false; continuePages_=false; pageScore_=model_.score(); pageStatus_=model_.level1State().systemStatusLevels();
    pageLampVisible_=model_.weaponIndicatorVisible(); pageLampReady_=model_.weaponReadyIndicator();
    recoveredFrame_.reset(); level1Cursor_=model_.level1State().audioEvents().size(); level2Cursor_=model_.level2AudioEvents().size();
    failureCursor_=0; pageProjection_=NativePageProjection::retainedRecoveredLevel2Preloop;
    frame_=presentations.back().indexed;
    NativeSessionDelivery delivery{run.firstSample,{},run.samples,deliveredEvents};
    delivery.presentations=std::move(presentations); recoveredLevel2Transition_=std::move(run);
    return delivery;
}
} //namespace stellar7
