#include "normal_work_packets.hpp"
#include "stellar7/native_session.hpp"
#include "stellar7/level2_music_work.hpp"
#include "level1_reward_model_bank.hpp"
#include "retained_level1_clock_domain.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int64_t workHorizon = 120000;
constexpr std::int64_t paletteHorizon = 2 * workHorizon;
constexpr std::size_t maximumPacketBudget = 10000;
constexpr std::size_t introductionOption = 5;
constexpr std::size_t statusArtGroup = 4;
constexpr std::size_t destinationLevel = 1;
constexpr std::int16_t screenWidth = 320;
constexpr std::int16_t screenHeight = 200;
constexpr auto pageLayout = CockpitPageLayout::recoveredStartup;
enum class ExecutionPass { preflight, live };

std::vector<std::vector<NormalWorkOperation>> packets(const std::vector<NormalWorkOperation>& operations,
    const std::size_t budget) {
    return normalWorkPackets(operations,budget);
}


}

NativeSessionDelivery NativeGameplaySession::resumeRecoveredLevel2Retry(const std::size_t workPacketBudget) {
    if (!recoveredLoader_ || !recoveredController_ || !audio_->workPhase_ || !recoveredStartupPalette_
        || !pixels_ || !radar_ || !assets_.cockpitSettings || level1Pages_ || initialized_
        || !recoveredLevel2Transition_ || !recoveredLevel2Frame_ || !recoveredFailure_ || !recoveredFailure_->finished
        || !failurePages_ || !model_.failureFromLevel2() || model_.campaignLevel()!=destinationLevel
        || recoveredLevel2Frame_->returnedLoopTick!=model_.level2LoopTick()
        || recoveredController_->expectedLoopTick()!=model_.level2LoopTick()
        || !recoveredContinue_ || !recoveredContinueState_ || recoveredContinueState_->stage!=ContinueStage::finished
        || recoveredContinue_->outcome!=ContinueOutcome::yes || model_.phase()!=GameplayPhase::continuePrompt
        || model_.recoveredContinueDecision()!=ContinueDecision::yes
        || pageProjection_!=NativePageProjection::retainedLevel2Continue
        || workPacketBudget==0 || workPacketBudget>maximumPacketBudget
        || assets_.campaignLevels.size()<=destinationLevel || !campaignResources_ || campaignResources_->level!=destinationLevel
        || campaignResources_->bitmaps!=&assets_.campaignLevels[destinationLevel].bitmapGroups
        || campaignResources_->models!=&assets_.campaignLevels[destinationLevel].models
        || campaignResources_->music!=&assets_.campaignLevels[destinationLevel].musicSequence) {
        throw std::invalid_argument("Level2 retry requires its current finished destruction/Yes and retained destination owners");
    }
    recoveredController_->nativeGuards({},{}); //Validate the retained controller without consuming another tail.
    const auto sourceLoop=recoveredFailure_->sourceLoopTick;
    const auto sourceTicks=model_.ticks();
    Level1RetryProfile profile{};
    profile.levelIndex=destinationLevel;profile.decision=recoveredContinue_->outcome;
    profile.score=model_.score();profile.remainingContinues=static_cast<std::uint16_t>(model_.remainingContinues());
    profile.introduction=recoveredIntroduction_.value_or(static_cast<std::uint16_t>(assets_.cockpitSettings->options[introductionOption]));
    profile.currentHighscores=highscoreTable_;
    profile.currentSequencePublished=audio_->workPhase_->sequenceSelection && audio_->workPhase_->sequenceSelection->published;
    profile.selectedSystem=model_.level1State().selectedSystem();
    profile.systemLevels=model_.level1State().systemStatusLevels();profile.systemTimers=model_.level1State().systemTimers();
    profile.frontPage=pages_.frontPage();profile.backPage=pages_.backPage();profile.cachePage=pages_.cachePage();
    //330A ranks the CURRENT score/table before040F/0417 can clear any source owner.
    const auto control=level2RankMissRetryOperations(profile);
    const auto sharedCount=level1RewardModelCapacity(assets_);
    const std::vector<Model3d> shared(assets_.models.begin(),assets_.models.begin()+sharedCount);
    const auto& resources=assets_.campaignLevels[destinationLevel];
    if(resources.resourceName!="LEVEL2.RES" || resources.models.size()!=5 || resources.musicSequence.empty()) {
        throw std::invalid_argument("Level2 retry requires its owned decoded destination bank");
    }
    auto staged=std::make_unique<GameplayModel>(model_);
    staged->beginRecoveredLevel2Retry();
    auto reset=staged->resetRecoveredLevel2RetryPools(shared,resources.models);
    const auto constructors=staged->constructRecoveredLevel2Actors(shared,resources.models);
    const auto loaderInput=staged->prepareRecoveredLevel2RetryLoaderPages(*assets_.cockpitSettings);
    auto resetRadar=*radar_;
    for(const auto key:reset.projectionResetKeys) {resetRadar.initializeAllocation(key);}
    const auto loader=planLevel2LoaderPages(assets_,loaderInput,pages_,*pixels_,resetRadar);
    staged->completeRecoveredLevel2MusicActivation();
    const auto score=scorePageEffects(staged->score(),assets_.scoreFont,loader.pages.backPage(),
        loader.pages.frontPage(),pageLayout);
    const auto& systems=staged->level1State();
    const auto status=statusPageEffects(systems.systemStatusLevels(),systems.systemTimers(),systems.selectedSystem(),
        assets_.artGroups.at(statusArtGroup),loader.pages.backPage(),loader.pages.frontPage(),pageLayout);
    const auto preloop=staged->completeRecoveredLevel2Preloop();
    const auto& observation=control.observation;
    if(staged->phase()!=GameplayPhase::level2PreloopReturn || staged->campaignLevel()!=destinationLevel
        || staged->ticks()!=sourceTicks || staged->level2LoopTick()!=observation.loopTick || !preloop.firstEntryDisplay
        || !preloop.firstPrefix.activeOrder.empty() || !preloop.firstPrefix.refreshRequested
        || preloop.allocationOrder!=std::vector<std::size_t>({0,1,2,3,4,5,6})
        || staged->remainingContinues()!=observation.remainingContinues || staged->score()!=observation.score
        || staged->playerArmor()!=observation.armor || systems.systemTimers()!=observation.systemTimers
        || !(preloop.firstPrefix.player.position==staged->level2Player().position)) {
        throw std::logic_error("Level2 retry staging differs from its source same-level preloop observation");
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

    NativeLevel2RetryRun run;
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
        phase=beginLevel2RetryMusic(*audio_->workPhase_); palette=*recoveredStartupPalette_; roles=pages_; pixels=*pixels_; radar=*radar_;
        oscillator=successPalette_; borrowed=campaignResources_;
        const auto append=[&](ClockedPresentationWork work) {
            phase=work.timing.phase;
            run.samples.insert(run.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
            run.work.push_back(std::move(work));
        };
        const auto render=[&](const std::vector<NormalWorkOperation>& operations) {
            for (const auto& packet:packets(operations,workPacketBudget)) {
                try {
                    if (pass==ExecutionPass::preflight) { phase=normalWorkTiming(phase,packet,workHorizon).phase; }
                    else { append(clock_.renderWork(phase,packet,workHorizon)); }
                } catch(const std::invalid_argument& error) {
                    throw std::invalid_argument("Level2 retry normal work kind="+std::to_string(static_cast<unsigned>(packet.front().kind))
                        +" retry-stage="+std::to_string(static_cast<unsigned>(phase.level2RetryMusic->stage))+": "+error.what());
                }
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
        const auto music=[&](const Level2RetryMusicChild child) { render(level2RetryMusicOperations(child)); };
        bool resetDone=false,constructorsDone=false,pagesDone=false,musicDone=false,groundDone=false;
        bool releasedMusic=false,stoppedMusic=false,emptyStopped=false,resourceBound=false,directReset=false,rebound=false;
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
            case RetryWorkKind::selectScoreFontProvider:
            case RetryWorkKind::modelResourcesProvider:
                provider(segment); break;
            case RetryWorkKind::releaseMusic:
                if(releasedMusic) {throw std::logic_error("duplicate retry018C release provider");}
                provider(segment);releasedMusic=true;break;
            case RetryWorkKind::levelResourceBootstrapProvider:
                provider(segment);
                borrowed=CampaignResourceBorrow{destinationLevel,&resources.bitmapGroups,&resources.models,&resources.musicSequence};
                break;
            case RetryWorkKind::stopPendingSound:
                if(segment.sourceResume==0x0469 && releasedMusic && !stoppedMusic) {
                    music(Level2RetryMusicChild::stop);stoppedMusic=true;
                } else if(segment.sourceResume==0x4f55 && stoppedMusic && !emptyStopped) {
                    music(Level2RetryMusicChild::emptyStop);emptyStopped=true;
                } else {throw std::logic_error("reordered retry0464/4F50 music children");}
                break;
            case RetryWorkKind::modelReloadProvider:
                if (index!=control.modelReloadSegment || !emptyStopped || resetDone) { throw std::logic_error("duplicate or reordered48D4 child"); }
                for (const auto& step:reset.reset.steps) {
                    if (const auto* operations=std::get_if<std::vector<NormalWorkOperation>>(&step)) { render(*operations); }
                    else if (pass==ExecutionPass::live) { run.selectorReleases.push_back(std::get<Level2SelectorReleaseReceipt>(step)); }
                }
                //Only now may the destination key convention adopt real reset ancestry.
                for (const auto key:reset.projectionResetKeys) { radar.initializeAllocation(key); }
                resetDone=true; break;
            case RetryWorkKind::levelMusicResourcesProvider:
                if(!resetDone || resourceBound) {throw std::logic_error("reordered retry6E6C resource binding");}
                provider(segment);phase=bindLevel2RetryMusicResource(phase,resources.musicSequence);resourceBound=true;break;
            case RetryWorkKind::retainedConstructorCompletionProvider:
                if (!resourceBound || !resetDone || constructorsDone) { throw std::logic_error("reordered Level2 constructors"); }
                render(constructors.operations); constructorsDone=true; break;
            case RetryWorkKind::loaderPages:
                if (index!=control.loaderPagesSegment || !constructorsDone || pagesDone) { throw std::logic_error("reordered50C4 pages"); }
                render(loader.operations); pixels=loader.pixels; roles=loader.pages; radar=loader.radar;
                pagesDone=true; publish(); break;
            case RetryWorkKind::musicActivation:
                if (!pagesDone || musicDone) { throw std::logic_error("reordered5098 activation"); }
                music(Level2RetryMusicChild::activation); musicDone=true;
                if (pass==ExecutionPass::live) { recoveredController_->restartNativeLoop(); }
                break;
            case RetryWorkKind::palette:
                if (!musicDone || !segment.palette) { throw std::logic_error("missing Level2 loader palette"); }
                if (pass==ExecutionPass::preflight) {
                    const auto upload=paletteClockTiming(phase,palette,*segment.palette,paletteHorizon);
                    phase=upload.timing.phase; palette=upload.state;
                } else {
                    auto upload=clock_.renderPalette(phase,palette,*segment.palette,paletteHorizon);
                    palette=upload.state; append(upload.work); run.palettes.push_back(std::move(upload));
                }
                publish(); break;
            case RetryWorkKind::directReset:
                if(!musicDone || directReset) {throw std::logic_error("reordered retry01A8 direct reset");}
                music(Level2RetryMusicChild::directReset);directReset=true;break;
            case RetryWorkKind::scoreHudProvider:
                if(!directReset) {throw std::logic_error("retry score preceded01A8 return");}
                //The zero-score7970 body remains a native HUD provider, as in Level1 retry.
                provider(segment);pixels.apply(score);
                roles.selectScoreCopy();publish();break;
            case RetryWorkKind::retainedGroundInitializationProvider:
                if (!directReset || groundDone) { throw std::logic_error("duplicate or reordered4C16 child"); }
                render(preloop.ground.operations); groundDone=true; break;
            case RetryWorkKind::statusHudProvider:
                if (!groundDone) { throw std::logic_error("status preceded ground initialization"); }
                provider(segment); pixels.apply(status); publish(); break;
            case RetryWorkKind::controllerRebindProvider:
                if (!groundDone || segment.sourceEntry!=0x01d0 || segment.sourceResume!=0x01d0) { throw std::logic_error("transition crossed its before01D0 boundary"); }
                if(index!=control.controllerRebindSegment || rebound) {throw std::logic_error("duplicate retry01D0 rebind receipt");}
                rebound=true; //Metadata only;50AC already reset the actual controller once.
                break;
            default: throw std::logic_error("unsupported Level2 transition child");
            }
            if (pass==ExecutionPass::live && segment.kind!=RetryWorkKind::ordinary) {
                run.stages.push_back({segment.kind,segment.sourceEntry,clock_.elapsedSamples()});
            }
        }
        if (!resetDone || !constructorsDone || !pagesDone || !musicDone || !groundDone || !directReset || !rebound) { throw std::logic_error("incomplete Level2 transition envelope"); }
        publish();
    };
    execute(ExecutionPass::preflight);
    execute(ExecutionPass::live);

    model_=std::move(*staged); pages_=roles; pixels_=std::move(pixels); radar_=std::move(radar);
    cockpitBitmaps_=loader.bitmaps; incomingIndicator_=loaderInput.cockpit.states[2];
    audio_->workPhase_=std::move(phase); *recoveredStartupPalette_=std::move(palette);
    successPalette_=std::move(oscillator); campaignResources_=std::move(borrowed);
    initialActorCount_=initialSlots.size();
    initialPoolSlots_.fill(static_cast<std::size_t>(-1));initialActorTypes_.fill(-1);
    std::copy(initialSlots.begin(),initialSlots.end(),initialPoolSlots_.begin());
    std::copy(initialTypes.begin(),initialTypes.end(),initialActorTypes_.begin());
    actorIncarnations_=incarnations; actorTypes_=actorTypes;
    level1Pages_=false; failurePages_=false; continuePages_=false; pageScore_=model_.score(); pageStatus_=model_.level1State().systemStatusLevels();
    pageLampVisible_=model_.weaponIndicatorVisible(); pageLampReady_=model_.weaponReadyIndicator();
    recoveredLevel2Frame_.reset();
    recoveredContinueState_->stage=ContinueStage::inactive;
    recoveredIntroduction_=observation.introduction;
    level1Cursor_=model_.level1State().audioEvents().size(); level2Cursor_=model_.level2AudioEvents().size();
    failureCursor_=0; pageProjection_=NativePageProjection::retainedRecoveredLevel2Preloop;
    frame_=presentations.back().indexed;
    NativeSessionDelivery delivery{run.firstSample,{},run.samples,{}};
    delivery.presentations=std::move(presentations); recoveredLevel2Retry_=std::move(run);
    return delivery;
}
} //namespace stellar7
