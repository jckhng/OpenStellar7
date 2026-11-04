#include "stellar7/native_session.hpp"
#include "normal_work_packets.hpp"
#include "playable_system_pages.hpp"
#include "playable_decoy.hpp"
#include "playable_jump.hpp"
#include "native_joystick_calibration_run.hpp"
#include "stellar7/campaign_loader.hpp"
#include "stellar7/success_palette_clock.hpp"
#include "stellar7/success_palette_step.hpp"
#include "stellar7/success_audio_work.hpp"
#include "level1_reward_model_bank.hpp"
#include "chase_view.hpp"

#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int64_t workHorizon=120000;
constexpr std::int64_t flipHorizon=2*workHorizon;
constexpr std::size_t maximumPacketBudget=10000;
constexpr std::size_t residentPalettePacketBudget=2048;
constexpr std::size_t level3Index=2;
constexpr std::array<NativeSessionStart,7> campaignStarts{NativeSessionStart::level1,NativeSessionStart::level2,
    NativeSessionStart::level3,NativeSessionStart::level4,NativeSessionStart::level5,
    NativeSessionStart::level6,NativeSessionStart::level7};
constexpr std::size_t statusArtGroup=4;
constexpr std::int16_t screenWidth=320;
constexpr std::int16_t screenHeight=200;
}

NativeSessionDelivery NativeGameplaySession::advancePlayableCampaign(const GameplayInput& requested,
    const std::size_t workPacketBudget, const NameEntryInput& text,const NativeDeliverySink& sink) {
    if(requested.analog||requested.joystickAction!=GameplaySystemAction::none) {throw std::invalid_argument("interpreted joystick input requires its retained owner");}
    if(requested.systemAction==GameplaySystemAction::calibrateJoystick&&!audio_->joystick_) {throw std::invalid_argument("calibration requires its native capability");}
    if(requested.sourceKey!=0&&!audio_->attract_) {throw std::invalid_argument("source key requires an active native attract consumer");}
    auto input=requested;audio_->acceptJoystickSample(input.joystick);input.joystick.reset();
    if(joystickCalibrationActive()&&model_.phase()!=GameplayPhase::nativeMenu) {return advanceJoystickCalibration(input,workPacketBudget);}
    if (input.systemAction != GameplaySystemAction::none && input.systemAction != GameplaySystemAction::jump
        && input.systemAction != GameplaySystemAction::activateSelected
        && input.systemAction != GameplaySystemAction::shield && input.systemAction != GameplaySystemAction::scanner
        && input.systemAction != GameplaySystemAction::superPrimary && input.systemAction != GameplaySystemAction::turbo
        && input.systemAction != GameplaySystemAction::cycleSelected
        && input.systemAction != GameplaySystemAction::decoy && input.systemAction != GameplaySystemAction::bomb
        && input.systemAction != GameplaySystemAction::preferences
        && input.systemAction != GameplaySystemAction::calibrateJoystick) {
        throw std::invalid_argument("unsupported queued system action");
    }
    if(story_) {return advancePlayableStory(input,workPacketBudget,sink);}
    if(storyDestination_ && storyRetry_) {
        if(input.confirm || input.fire || input.exit) {throw std::invalid_argument("Story retry requires released input");}
        storyDestination_.reset();storyRetry_=false;
        return reloadPlayableRetry(workPacketBudget);
    }
    const auto entered=model_.phase();
    const bool ordinary = entered == GameplayPhase::level1 || entered == GameplayPhase::level2
        || entered == GameplayPhase::level3Native || entered == GameplayPhase::playable
        || entered == GameplayPhase::level5 || entered == GameplayPhase::level3PreloopReturn
        || entered == GameplayPhase::playablePreloopReturn;
    if (input.systemAction != GameplaySystemAction::none && !ordinary
        &&!(entered==GameplayPhase::nativeMenu&&input.systemAction==GameplaySystemAction::calibrateJoystick)) {
        throw std::invalid_argument("queued system action requires an ordinary playable frame");
    }
    if(input.systemAction==GameplaySystemAction::preferences
        &&!hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::fullGraphicDetail)) {
        throw std::invalid_argument("unsupported queued system action");
    }
    if(livePreferences_) {
        if(input.systemAction==GameplaySystemAction::calibrateJoystick) {input.systemAction=GameplaySystemAction::none;}
        if(!text.characters.empty()) {throw std::invalid_argument("live preferences reject name-entry input");}
        return advancePlayablePreferences(input,workPacketBudget);
    }
    if(input.systemAction==GameplaySystemAction::preferences) {
        if((!playableCampaign_&&entered!=GameplayPhase::level1)||!ordinary||!audio_->menuResources_||!highscoreTable_
            ||!audio_->workPhase_||!pixels_||!recoveredStartupPalette_||workPacketBudget==0||workPacketBudget>maximumPacketBudget) {
            throw std::invalid_argument("live preferences require an adopted ordinary playable frame");
        }
        auto frameInput=input;frameInput.systemAction=GameplaySystemAction::none;
        //The native request is consumed at the completed ordinary-frame boundary.
        //9F2B's guest callback is inside that frame's keyboard tail.
        auto delivery=advancePlayableCampaign(frameInput,workPacketBudget,text,sink);
        const auto returned=model_.phase();
        if(returned!=GameplayPhase::level2&&returned!=GameplayPhase::level3Native
            &&returned!=GameplayPhase::playable&&returned!=GameplayPhase::level5) {return delivery;}
        livePreferences_=true;
        const auto modal=advancePlayablePreferences({},workPacketBudget);
        if(modal.firstSample!=delivery.firstSample+delivery.samples.size()) {
            throw std::logic_error("live preferences lost its completed-frame PCM prefix");
        }
        delivery.samples.insert(delivery.samples.end(),modal.samples.begin(),modal.samples.end());
        delivery.presentations.insert(delivery.presentations.end(),modal.presentations.begin(),modal.presentations.end());
        delivery.pointerPosition=modal.pointerPosition;
        return delivery;
    }
    if(input.systemAction==GameplaySystemAction::calibrateJoystick&&ordinary) {
        if(!ordinary||!audio_->joystick_||!audio_->workPhase_||workPacketBudget==0||workPacketBudget>maximumPacketBudget) {
            throw std::invalid_argument("calibration requires its native ordinary owner");
        }
        auto frameInput=input;frameInput.systemAction=GameplaySystemAction::none;
        auto delivery=advancePlayableCampaign(frameInput,workPacketBudget,text,sink);
        const auto returned=model_.phase();
        if(returned!=GameplayPhase::level2&&returned!=GameplayPhase::level3Native&&returned!=GameplayPhase::playable&&returned!=GameplayPhase::level5) {return delivery;}
        audio_->joystickCalibrationRun_=std::make_shared<NativeJoystickCalibrationRun>(*audio_->joystick_,
            NativeJoystickCalibrationCaller::ordinary,*audio_->workPhase_);
        const auto modal=advanceJoystickCalibration({},workPacketBudget);
        if(modal.firstSample!=delivery.firstSample+delivery.samples.size()) {throw std::logic_error("calibration lost completed-frame PCM prefix");}
        delivery.samples.insert(delivery.samples.end(),modal.samples.begin(),modal.samples.end());
        delivery.events.insert(delivery.events.end(),modal.events.begin(),modal.events.end());
        delivery.presentations.insert(delivery.presentations.end(),modal.presentations.begin(),modal.presentations.end());return delivery;
    }
    if(ordinary) {audio_->joystickGameplayInput(input);}
    else if(audio_->joystick_ && (entered==GameplayPhase::nativeMenu || entered==GameplayPhase::continuePrompt
        || entered==GameplayPhase::highscoreNameEntry || entered==GameplayPhase::highscoreHall)) {
        //Automatic success/loader phases have no menu-confirm consumer.
        const auto modal=audio_->joystickMenuInput({input.forward,input.reverse,input.confirm||input.fire,input.left,input.right,input.exit,input.pointer});
        input.forward=modal.up;input.reverse=modal.down;input.left=modal.left;input.right=modal.right;input.confirm=modal.confirm;
    }
    if(ordinary&&(input.systemAction==GameplaySystemAction::bomb||input.systemAction==GameplaySystemAction::activateSelected
        ||input.joystickAction!=GameplaySystemAction::none)) {
        const PlayableJumpState before{model_.level1State().systemStatusLevels(),model_.level1State().systemTimers(),
            model_.level1State().selectedSystem(),model_.level2Player().position.z};
        const auto advanced=advancePlayableJump(before,PlayableSystemDomain::fullBundle);
        const auto joystick=applyPlayableJumpAction(advanced.state,input.joystickAction,PlayableSystemDomain::fullBundle);
        const auto keyboard=applyPlayableJumpAction(joystick.state,input.systemAction,PlayableSystemDomain::fullBundle);
        if((joystick.bombRefresh==PlayableBombRefresh::deploy||keyboard.bombRefresh==PlayableBombRefresh::deploy)
            &&audio_->playableSoundsEnabled()) {validatePlayableBombEvent();}
    }
    if (!playableCampaign_ && entered==GameplayPhase::level1) {
        if (workPacketBudget==0 || workPacketBudget>maximumPacketBudget || input.confirm || input.exit
            || !text.characters.empty()) {
            throw std::invalid_argument("playable Level1 adoption requires a valid gameplay request");
        }
        adoptPlayableLevel1();
    }
    if (playableCampaign_ && (entered==GameplayPhase::campaignEnded || entered==GameplayPhase::nativeMenu
        || entered==GameplayPhase::endingReturn)) {
        return advancePlayableTitle(input,workPacketBudget);
    }
    if (playableCampaign_ && (entered==GameplayPhase::failurePreExplosionBlackout
        || entered==GameplayPhase::failureExplosion || entered==GameplayPhase::failurePostExplosionBlackout
        || entered==GameplayPhase::continuePrompt || entered==GameplayPhase::highscoreNameEntry
        || entered==GameplayPhase::highscoreHall)) {
        return advancePlayableLifecycle(input,text,workPacketBudget);
    }
    if (playableCampaign_ && (entered==GameplayPhase::endingEntry || entered==GameplayPhase::endingPresentation
        || entered==GameplayPhase::endingTeardown)) {
        return advancePlayableEnding(input,workPacketBudget);
    }
    if (input.exit || input.confirm || !text.characters.empty()) {
        std::ostringstream detail;
        detail << "playable campaign input has no active consumer: phase=" << static_cast<int>(entered)
            << " confirm=" << input.confirm << " exit=" << input.exit << " text=" << text.characters.size();
        throw std::invalid_argument(detail.str());
    }
    if (workPacketBudget==0 || workPacketBudget>maximumPacketBudget) {
        throw std::invalid_argument("playable campaign work budget is outside its supported range");
    }
    if (!audio_->workPhase_ || !recoveredStartupPalette_ || !pixels_ || !radar_ || level1Pages_
        || !assets_.cockpitSettings || !campaignResources_) {
        std::ostringstream detail;
        detail << "playable campaign requires its retained gameplay/device owners: phase=" << static_cast<int>(entered)
            << " work=" << audio_->workPhase_.has_value() << " palette=" << recoveredStartupPalette_.has_value()
            << " pixels=" << pixels_.has_value() << " radar=" << radar_.has_value()
            << " level1Pages=" << level1Pages_ << " settings=" << assets_.cockpitSettings.has_value()
            << " bank=" << campaignResources_.has_value();
        throw std::invalid_argument(detail.str());
    }
    if (!playableCampaign_) {
        if (campaignResources_->level!=model_.campaignLevel() || !campaignResources_->models) {
            throw std::invalid_argument("playable preparation requires its actual retained model bank");
        }
        const auto sharedCount=level1RewardModelCapacity(assets_);
        const std::vector<Model3d> shared(assets_.models.begin(),assets_.models.begin()+sharedCount);
        model_.preparePlayableCampaignModels(shared,*campaignResources_->models);
    }
    if (model_.phase()==GameplayPhase::level2PreloopReturn) {
        return advanceRecoveredLevel2Frame(FrameCopyProfile::modeXNoop,workPacketBudget,{});
    }
    NativeSessionDelivery delivery;
    delivery.firstSample=clock_.elapsedSamples();
    auto phase=*audio_->workPhase_;
    auto palette=*recoveredStartupPalette_;
    phase.playableCampaign=true;
    playableCampaign_=true;
    const auto append=[&](ClockedPresentationWork work) {
        if (work.audio.firstSample!=delivery.firstSample+delivery.samples.size()) {
            throw std::logic_error("playable campaign lost its retained PCM epoch");
        }
        phase=work.timing.phase;
        delivery.samples.insert(delivery.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
    };
    const auto publish=[&]() {
        frame_.pixels=*retainedPagePixels(NativePageRole::front);
        frame_.palette=expandVgaDacPalette(std::vector<std::uint8_t>(palette.dac.colors.begin(),palette.dac.colors.end()));
        refreshChaseHud(frame_,assets_,model_);
        delivery.presentations.push_back({clock_.elapsedSamples(),frame_});
    };
    const auto upload=[&](const PaletteRequest request) {
        auto work=clock_.renderPalette(phase,palette,request,flipHorizon);
        palette=work.state;
        append(std::move(work.work));
        publish();
    };
    const auto events=[&]() {
        const auto& pending=model_.level2AudioEvents();
        if (level2Cursor_>pending.size()) {throw std::logic_error("playable campaign lost its audio cursor");}
        while(level2Cursor_<pending.size()) {
            const auto event=pending[level2Cursor_];
            if (audio_->playableSoundsEnabled()) { phase.callback=synth_.triggerPlayableEvent(event.eventId); }
            delivery.events.push_back(event);
            ++level2Cursor_;
        }
    };
    const auto systems=[&](const std::vector<PlayableSystemEffect>& effects) {
        bool changed = false;
        for (const auto& effect : effects) {
            if (const auto* audio = std::get_if<SystemAudio>(&effect)) {
                const auto& pending = model_.level2AudioEvents();
                if (level2Cursor_ >= pending.size() || pending[level2Cursor_].eventId != audio->event) {
                    throw std::logic_error("playable system audio lost its ordered retained cursor");
                }
                const auto event = pending[level2Cursor_];
                if (audio_->playableSoundsEnabled()) { phase.callback = synth_.triggerPlayableEvent(event.eventId); }
                delivery.events.push_back(event);
                ++level2Cursor_;
                continue;
            }
            if(const auto* strip=std::get_if<SystemDecoyStrip>(&effect)) {
                if(strip->mode!=DecoyStripMode::visible && strip->mode!=DecoyStripMode::restore) {
                    throw std::invalid_argument("unknown decoy strip effect");
                }
                const auto mode=strip->mode==DecoyStripMode::visible
                    ? PlayableDecoyStrip::visible : PlayableDecoyStrip::restore;
                if(mode==PlayableDecoyStrip::restore) {pages_.selectScoreCopy();}
                else {pages_.beginFrame();}
                pixels_->apply(playableDecoyStripEffects(mode,assets_.artGroups.at(3),
                    pages_.frontPage(),pages_.backPage(),pages_.cachePage()));
                pages_.selectBackCopy();pages_.selectCockpitDraw();
                changed=true;
                continue;
            }
            if(const auto* color=std::get_if<SystemDecoyPalette>(&effect)) {
                if(color->mode==DecoyPaletteMode::decoy) {
                    auto work=clock_.renderPlayableDecoyPalette(phase,palette,flipHorizon);
                    palette=work.state;append(std::move(work.work));
                    publish();
                } else if(color->mode==DecoyPaletteMode::restore) {
                    upload({0,playableDecoyPaletteColors,0,vgaComponentMaximum});
                } else {throw std::invalid_argument("unknown decoy palette effect");}
                continue;
            }
            if (!std::holds_alternative<SystemCopy>(effect)) { pages_.beginModal(); }
            pixels_->apply(playableSystemPageEffects(effect,assets_.artGroups.at(statusArtGroup),
                pages_.backPage(),pages_.frontPage(),CockpitPageLayout::recoveredStartup));
            if (std::holds_alternative<SystemPanel>(effect) || std::holds_alternative<SystemCopy>(effect)) {
                pages_.selectScoreCopy();
            }
            changed = true;
        }
        if (changed) { pageStatus_ = model_.level1State().systemStatusLevels(); }
        return changed;
    };
    const auto commit=[&]() {
        playableCampaign_=true;
        audio_->workPhase_=phase;
        *recoveredStartupPalette_=palette;
    };
    const auto successPalette=[&](const SuccessPaletteExecution execution) {
        const auto completion=audio_->playableMusicEnabled()
            ? SuccessPaletteCompletionSource::soundHandle : SuccessPaletteCompletionSource::timer;
        SuccessPaletteResidentCursor cursor{*successPalette_,execution,completion,SuccessPaletteResidentStage::entry};
        std::optional<SuccessPaletteBoundary> boundary;
        while (!boundary) {
            auto work=clock_.renderSuccessPaletteStep(phase,cursor,palette,
                std::min(workPacketBudget,residentPalettePacketBudget),workHorizon);
            cursor=work.cursor;palette=work.palette.state;boundary=work.boundary;
            append(std::move(work.palette.work));
        }
        successPalette_=cursor.state;
        publish();
        return *boundary;
    };
    const auto previous=model_.phase();
    if (previous==GameplayPhase::level2SuccessPresentation) {
        if (!successPalette_) {throw std::invalid_argument("playable success lost its retained palette owner");}
        const auto finished=successPalette(SuccessPaletteExecution::advance)==SuccessPaletteBoundary::presentationComplete;
        model_.setSuccessAudioActive(!finished);
        if (finished) {
            if (systems(model_.resumePlayableCampaignSuccess(input))) { publish(); }
            events();
            pages_.beginFrame();successPalette_.reset();
            pageProjection_=NativePageProjection::retainedLevel2SuccessReturn;
        } else {
            model_.tick({});
        }
        commit();
        return delivery;
    }
    if (previous==GameplayPhase::level2SuccessReturn) {
        const auto destination=model_.pendingPlayableCampaignDestination();
        if (destination>=campaignStarts.size()) {
            model_.completePlayableCampaignEnding();
            commit();
            return advancePlayableEnding(input,workPacketBudget);
        }
        if(schedulePlayableStory(destination,false)) {
            commit();return advancePlayableStory(input,workPacketBudget,sink);
        }
        if (destination!=level3Index) {
            if (assets_.campaignLevels.size()<=destination || campaignResources_->level!=model_.campaignLevel()
                || assets_.campaignLevels[destination].resourceName!="LEVEL"+std::to_string(destination+1)+".RES") {
                throw std::invalid_argument("playable campaign loader requires its actual destination resource");
            }
            const auto sharedCount=level1RewardModelCapacity(assets_);
            const std::vector<Model3d> shared(assets_.models.begin(),assets_.models.begin()+sharedCount);
            const auto& resource=assets_.campaignLevels[destination];
            synth_.validateCampaignMusicLoad(resource.musicSequence);
            pages_.selectFront();
            model_.completePlayableCampaignLoad(destination,shared,resource.models);
            initializePages(campaignStarts[destination]);
            phase.callback=synth_.selectPlayableSequence(resource.musicSequence,PlayableSequenceMode::repeat,audio_->playableMusicPublication());
        phase.playableMusicOff=!audio_->playableMusicEnabled();phase.playableSuccessCountdownWritten=false;
            phase.sequenceSelection.reset();phase.successAudio.reset();phase.level2RetryMusic.reset();
            campaignResources_=CampaignResourceBorrow{destination,&resource.bitmapGroups,&resource.models,&resource.musicSequence};
            level2Cursor_=model_.level2AudioEvents().size();failureCursor_=0;
            successPalette_.reset();recoveredSuccessState_.reset();
            pageProjection_=NativePageProjection::retainedPlayableLoader;
            upload({0,vgaPaletteColors,0,0});
            commit();
            return delivery;
        }
        if (model_.campaignLevel()!=1 || assets_.campaignLevels.size()<=level3Index
            || campaignResources_->level!=1 || assets_.campaignLevels[level3Index].resourceName!="LEVEL3.RES") {
            throw std::invalid_argument("playable Level3 loader requires the actual retained Level2 return");
        }
        const auto sharedCount=level1RewardModelCapacity(assets_);
        const std::vector<Model3d> shared(assets_.models.begin(),assets_.models.begin()+sharedCount);
        const auto& resource=assets_.campaignLevels[level3Index];
        validateLevel3LoaderActors(shared,resource.models);
        synth_.validateCampaignMusicLoad(resource.musicSequence);
        pages_.selectFront();
        model_.completeLevel3Load(shared,resource.models);
        initializePages(NativeSessionStart::level3);
        phase.callback=synth_.selectPlayableSequence(resource.musicSequence,PlayableSequenceMode::repeat,audio_->playableMusicPublication());
        phase.playableMusicOff=!audio_->playableMusicEnabled();phase.playableSuccessCountdownWritten=false;
        // The source selection receipts belong to the released level. The
        // native provider owns this replacement; the PIT/PIC epoch continues.
        phase.sequenceSelection.reset();phase.successAudio.reset();phase.level2RetryMusic.reset();
        campaignResources_=CampaignResourceBorrow{level3Index,&resource.bitmapGroups,&resource.models,&resource.musicSequence};
        level2Cursor_=model_.level2AudioEvents().size();failureCursor_=0;
        successPalette_.reset();recoveredSuccessState_.reset();
        pageProjection_=NativePageProjection::retainedLevel3Loader;
        upload({0,vgaPaletteColors,0,0});
        commit();
        return delivery;
    }
    if (previous==GameplayPhase::level3LoaderReturn || previous==GameplayPhase::playableLoaderReturn) {
        if (previous==GameplayPhase::level3LoaderReturn) {model_.validateLevel3Preloop();}
        phase.callback=synth_.resetPlayableDirectEffects();
        const auto& initialSystems=model_.level1State();
        pixels_->apply(scorePageEffects(model_.score(),assets_.scoreFont,pages_.backPage(),pages_.frontPage(),CockpitPageLayout::recoveredStartup));
        const auto systemEffects = previous==GameplayPhase::level3LoaderReturn
            ? model_.completeLevel3Preloop() : model_.completePlayableCampaignPreloop();
        systems(systemEffects);
        pages_.selectArmorCopy();pageScore_=model_.score();pageStatus_=initialSystems.systemStatusLevels();
        pageProjection_=previous==GameplayPhase::level3LoaderReturn ? NativePageProjection::retainedLevel3Preloop
            : NativePageProjection::retainedPlayablePreloop;
        publish();commit();
        return delivery;
    }
    const auto first=previous==GameplayPhase::level3PreloopReturn || previous==GameplayPhase::playablePreloopReturn;
    if (previous!=GameplayPhase::level2 && previous!=GameplayPhase::level3Native
        && previous!=GameplayPhase::level5 && previous!=GameplayPhase::playable && !first) {
        throw std::invalid_argument("unsupported playable campaign continuation");
    }
    incomingIndicator_={static_cast<std::int16_t>(model_.level2CockpitIndicatorX()),
        static_cast<std::int16_t>(model_.level2CockpitIndicatorVelocity())};
    model_.advancePlayableCampaign(input);
    if (model_.phase()==GameplayPhase::endingEntry) {
        events();commit();
        auto ending=advancePlayableEnding(input,workPacketBudget);
        if (ending.firstSample!=delivery.firstSample+delivery.samples.size()) {
            throw std::logic_error("playable ending child lost its retained PCM prefix");
        }
        delivery.entry.insert(delivery.entry.end(),ending.entry.begin(),ending.entry.end());
        delivery.samples.insert(delivery.samples.end(),ending.samples.begin(),ending.samples.end());
        delivery.events.insert(delivery.events.end(),ending.events.begin(),ending.events.end());
        delivery.presentations.insert(delivery.presentations.end(),ending.presentations.begin(),ending.presentations.end());
        return delivery;
    }
    events();
    if (model_.phase()==GameplayPhase::level2SuccessPresentation) {
        renderSuccessPages();
        if (!successPalette_ || assets_.soundSequences.empty()) {
            throw std::invalid_argument("playable success requires its owned sound and palette");
        }
        phase.callback=synth_.selectPlayableSequence(assets_.soundSequences.front(),PlayableSequenceMode::once,audio_->playableMusicPublication());
        phase.playableMusicOff=!audio_->playableMusicEnabled();phase.playableSuccessCountdownWritten=false;
        phase.sequenceSelection.reset();phase.successAudio.reset();phase.level2RetryMusic.reset();
        model_.setSuccessAudioActive(true);
        successPalette(SuccessPaletteExecution::initial);
        if (!audio_->playableMusicEnabled()) {
            append(clock_.renderWork(phase,{{NormalWorkKind::playableSuccessCountdownStore,1,successRawCountdown}},workHorizon));
        }
        commit();
        return delivery;
    }
    // The cockpit plan includes02D2 CALL6D46, resident stubs and RET.
    // Other native semantic/render providers still have unmeasured CPU work.
    std::vector<NormalWorkOperation> cockpitWork;
    if (!renderLevel2Pages(&cockpitWork)) {throw std::invalid_argument("unsupported playable campaign page projection");}
    if (hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::fullGraphicDetail)) {
        for (const auto& packet : normalWorkPackets(cockpitWork,workPacketBudget)) {
            append(clock_.renderWork(phase,packet,workHorizon));
        }
    }
    constexpr std::uint8_t level6Index = 5;
    const auto type24Bank = model_.campaignLevel()==level3Index || model_.campaignLevel()==level6Index;
    if (type24Bank && (model_.phase()==GameplayPhase::level3Native
        || model_.phase()==GameplayPhase::playable || model_.pendingLevel2FailureTail())) {
        model_.finishLevel3Radar();
    }
    const PageFlipClockState flip{PageFlipSynchronization::enabled,pages_.frontPage(),pages_.backPage(),
        0x3d4,PageFlipLayout::recoveredStartup,PageFlipWaitDomain::retainedProgram};
    auto flipped=clock_.renderPageFlip(phase,flip,flipHorizon);
    append(std::move(flipped.work));publish();
    if (first) {
        for (std::uint8_t intensity=0;intensity<64;intensity+=4) {upload({0,vgaPaletteColors,0,intensity});}
    }
    if (model_.phase()!=GameplayPhase::level2SuccessPresentation) {
        if (systems(model_.finishPlayableCampaignFrame())) { publish(); }
        events();
    }
    commit();
    return delivery;
}
} // namespace stellar7
