#include "stellar7/native_session.hpp"
#include "level1_reward_model_bank.hpp"
#include "playable_menu_dialog.hpp"
#include "playable_menu_pages.hpp"
#include "playable_pointer.hpp"
#include "playable_attract.hpp"
#include "playable_story.hpp"
#include "retained_menu_clock_domain.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int16_t screenWidth=320;
constexpr std::int16_t screenHeight=200;
constexpr std::int64_t workHorizon=120000;
constexpr std::int64_t flipHorizon=2*workHorizon;
constexpr std::uint16_t beginWaitIrqs=480;
}

bool NativeGameplaySession::menuBackAvailable() const noexcept {
    return livePreferences_||(menuDialog_&&menuDialog_->backAvailable());
}
std::optional<NativeAttractProgress> NativeGameplaySession::attractProgress() const noexcept {
    return audio_->attract_ ? std::optional{audio_->attract_->progress()} : std::nullopt;
}
bool NativeGameplaySession::attractAcceptsSourceKey() const noexcept {
    return audio_->attract_&&audio_->attract_->acceptsSourceKey();
}
NativeSessionDelivery NativeGameplaySession::advancePlayableAttract(const GameplayInput& input,const std::size_t budget) {
    if(!audio_->attract_ || !audio_->startupAssets_ || !audio_->menuResources_ || !audio_->menuResources_->streamsClosed
        || model_.phase()!=GameplayPhase::nativeMenu || !pixels_ || !recoveredStartupPalette_) {
        throw std::logic_error("returned attract lost its actual retained menu caller");
    }
    NativeMenuInput menuInput{input.forward,input.reverse,input.confirm||input.fire,input.left,input.right,input.exit,input.pointer};
    menuInput.sourceKey=input.sourceKey;
    const bool buttons=audio_->joystickButtonHeld() || (input.pointer&&(input.pointer->primary||input.pointer->secondary));
    auto delivery=audio_->attract_->advance({*pixels_,pages_,*recoveredStartupPalette_,synth_,clock_,*audio_->workPhase_,
        assets_,*audio_->startupAssets_,audio_->menuResources_->configuration,audio_->attractCreditsSkip_,model_.playableModelDetail(0)},
        {menuInput,input.sourceKey,buttons},budget);
    if(audio_->attract_->returnedToMenu()) {
        audio_->attract_.reset();menu_.emplace();menuPressed_.reset();menuCursor_.reset();menuArmed_=true;
        auto store=clock_.renderWork(*audio_->workPhase_,{{NormalWorkKind::playableAttractCountdownStore,1,menuCountdownRawIrqs}},workHorizon);
        *audio_->workPhase_=store.timing.phase;delivery.samples.insert(delivery.samples.end(),store.audio.samples.begin(),store.audio.samples.end());
        paintPlayableMainMenu(*pixels_,assets_.artGroups.at(0),assets_.titleFont,*menu_,menuPressed_,pages_.backPage(),pages_.frontPage());
    }
    endingPalette_=*recoveredStartupPalette_;
    frame_.pixels=pixels_->snapshot(pages_.frontPage());
    frame_.palette=expandVgaDacPalette(std::vector<std::uint8_t>(recoveredStartupPalette_->dac.colors.begin(),recoveredStartupPalette_->dac.colors.end()));
    return delivery;
}

NativeSessionDelivery NativeGameplaySession::advancePlayableTitle(const GameplayInput& requested,
    const std::size_t workPacketBudget) {
    if (!playableCampaign_ || !audio_->workPhase_ || !recoveredStartupPalette_ || !pixels_ || !radar_
        || !assets_.cockpitSettings || workPacketBudget==0 || workPacketBudget>10000) {
        throw std::invalid_argument("playable title requires its retained campaign/device owners");
    }
    auto input=requested;
    if(audio_->takeJoystickCancellation()) {
        menuPressed_.reset();menuCursor_.reset();menuArmed_=true;
        if(menuDialog_) {menuDialog_->cancelInteraction();}
        if(audio_->attract_) {audio_->attract_->cancelInteraction();}
        input={};
    }
    if(audio_->attract_) {return advancePlayableAttract(input,workPacketBudget);}
    NativeSessionDelivery delivery;
    delivery.firstSample=clock_.elapsedSamples();
    auto phase=*audio_->workPhase_;
    auto palette=*recoveredStartupPalette_;
    const auto append=[&](ClockedPresentationWork work) {
        if (work.audio.firstSample!=delivery.firstSample+delivery.samples.size()) {
            throw std::logic_error("playable title lost its retained PCM epoch");
        }
        phase=work.timing.phase;
        delivery.samples.insert(delivery.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
    };
    const auto publish=[&]() {
        frame_.pixels=*retainedPagePixels(NativePageRole::front);
        frame_.palette=expandVgaDacPalette(std::vector<std::uint8_t>(palette.dac.colors.begin(),palette.dac.colors.end()));
        delivery.presentations.push_back({delivery.firstSample+delivery.samples.size(),frame_});
    };
    const auto upload=[&](const std::uint8_t intensity) {
        auto work=clock_.renderPalette(phase,palette,{0,vgaPaletteColors,0,intensity},flipHorizon);
        palette=work.state;append(std::move(work.work));publish();
    };
    const auto commit=[&]() {audio_->workPhase_=phase;*recoveredStartupPalette_=palette;endingPalette_=palette;};
    const auto previous=model_.phase();
    if (previous==GameplayPhase::endingReturn || previous==GameplayPhase::campaignEnded) {
        if (synth_.sampledVoiceObservation().phase!=SampledVoicePhase::unloaded) {
            throw std::invalid_argument("playable title requires released ending speech");
        }
        if (previous==GameplayPhase::endingReturn) {
            model_.validateTitleReturn();
        } else if (!model_.failureFromLevel2()
            || (model_.recoveredContinueDecision()!=ContinueDecision::no
                && model_.recoveredContinueDecision()!=ContinueDecision::cancelled
                && !(model_.remainingContinues()==0
                    && model_.recoveredContinueDecision()==ContinueDecision::pending))) {
            throw std::invalid_argument("playable title lost its completed game-over caller");
        }
        const auto scoreCaller=model_.playableHighscoreCaller();
        const bool returningScore=scoreCaller && (*scoreCaller==PlayableHighscoreCaller::endingTitle
            || *scoreCaller==PlayableHighscoreCaller::gameOverTitle);
        if (!returningScore && previous==GameplayPhase::campaignEnded && !gameOverScenePlayed_
            && hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::storyPanels)
            && assets_.storyScenes && assets_.storyScenes->defeat) {
            activeStory_=StoryScene::defeat;
            story_=std::make_shared<PlayableStoryPresenter>(*assets_.storyScenes,*activeStory_,
                RadarPages{pages_.cachePage(),pages_.backPage(),pages_.frontPage(),pages_.drawPage(),pages_.copyPage()});
            gameOverScenePlayed_=true;
            return advancePlayableStory(input,workPacketBudget);
        }
        if (input.left || input.right || input.forward || input.reverse || input.fire || input.confirm || input.exit) {
            throw std::invalid_argument("playable title return requires released input");
        }
        if (!returningScore) {
            if (scoreCaller || !highscoreTable_) {throw std::invalid_argument("playable title requires its unconsumed330A caller/table");}
            synth_.validateCampaignMusicLoad(assets_.presentationMusicSequence);
            if (phase.callback && phase.callback->domain==SoundBlasterCallbackDomain::sequence) {
                phase.callback=synth_.stopPlayableSequence();
                phase.callback=synth_.deselectPlayableSequence();
            } else if (!phase.callback || phase.callback->domain!=SoundBlasterCallbackDomain::level2RetryNoHandle) {
                throw std::invalid_argument("playable title lost its stopped sequence owner");
            }
            //04BF restarts menu music before the shared04D1 rank child.
            phase.callback=synth_.selectPlayableSequence(assets_.presentationMusicSequence,PlayableSequenceMode::repeat,audio_->playableMusicPublication());
        phase.playableMusicOff=!audio_->playableMusicEnabled();phase.playableSuccessCountdownWritten=false;
            phase.sequenceSelection.reset();phase.successAudio.reset();phase.level2RetryMusic.reset();
            if (highscoreTable_->rank(model_.score())) {
                std::copy(assets_.cockpitDacPalette.begin(),assets_.cockpitDacPalette.end(),palette.source.begin());
                model_.beginPlayableHighscoreNameEntry();nameEntry_.emplace(nameCaret_);
                nameEntryPages_=false;nameOkPressed_=false;incomingNameOkPressed_=false;
                pendingNameInput_={};renderNameEntryPages();upload(vgaComponentMaximum);
                if(audio_->playablePointerEnabled()) {delivery.pointerPosition=ProjectedPoint{161,139};}
                commit();return delivery;
            }
        }
        campaignResources_.reset();successPalette_.reset();
        nameEntry_.reset();nameEntryPages_=false;hallPages_=false;continuePages_=false;failurePages_=false;
        model_.finishPlayableTitleReturn();gameOverScenePlayed_=false;
        std::copy(assets_.cockpitDacPalette.begin(),assets_.cockpitDacPalette.end(),palette.source.begin());
        menu_.emplace();menuPressed_.reset();menuCursor_.reset();menuArmed_=true;
        pages_.beginModal();
        pixels_->apply(mainMenuPageEffects(assets_.artGroups.at(0),assets_.titleFont,pages_.backPage()));
        pixels_->apply(std::vector<IndexedPageEffect>{RadarPageCopy{pages_.backPage(),pages_.frontPage(),
            {0,0},screenWidth,screenHeight}});
        renderMenuCursor();upload(vgaComponentMaximum);
        pageProjection_=NativePageProjection::retainedMenu;
        if(audio_->playablePointerEnabled()) {
            delivery.pointerPosition=ProjectedPoint{static_cast<std::int16_t>(menu_->cursorX()),
                static_cast<std::int16_t>(menu_->cursorY())};
        }
        if(hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::automaticAttract)) {
            append(clock_.renderWork(phase,{{NormalWorkKind::playableAttractCountdownStore,1,menuCountdownRawIrqs}},workHorizon));
        }
        commit();return delivery;
    }
    if (previous!=GameplayPhase::nativeMenu || !menu_) {
        throw std::invalid_argument("playable menu polling requires its retained widget owner");
    }
    const auto counter=joystickCounterSample(audio_->joystickSample_);
    const auto confirm=input.confirm || input.fire;
    const bool pointer=audio_->pointerCapability_==NativePointerCapability::portablePointer;
    const bool pointerCancel=pointer&&input.pointer&&input.pointer->phase==NativePointerPhase::cancel;
    const bool rawPointer=!pointerCancel&&audio_->playablePointerEnabled()&&input.pointer.has_value();
    std::optional<std::size_t> pointerActivated;
    const auto pollMenu=[&]() {
        model_.finishMenuPoll();
        auto flipped=clock_.renderPageFlip(phase,{PageFlipSynchronization::enabled,pages_.frontPage(),pages_.backPage(),
            0x3d4,PageFlipLayout::recoveredStartup,PageFlipWaitDomain::retainedProgram},flipHorizon);
        append(std::move(flipped.work));publish();commit();
        if(audio_->playablePointerEnabled()) {
            delivery.pointerPosition=menuDialog_?menuDialog_->pointerPosition():menuCursor_.value_or(
                ProjectedPoint{static_cast<std::int16_t>(menu_->cursorX()),static_cast<std::int16_t>(menu_->cursorY())});
        }
    };
    const auto repaintMenu=[&]() {
        paintPlayableMainMenu(*pixels_,assets_.artGroups.at(0),assets_.titleFont,*menu_,menuPressed_,
            pages_.backPage(),pages_.frontPage(),menuCursor_);
        if(!pointer) {menuCursor_=ProjectedPoint{static_cast<std::int16_t>(menu_->cursorX()),
            static_cast<std::int16_t>(menu_->cursorY())};}
    };
    const auto paintDialog=[&]() {
        if(!audio_->menuResources_||!highscoreTable_) {
            throw std::invalid_argument("playable submenu requires retained menu resources/table");
        }
        menuDialog_->paint(*pixels_,assets_,audio_->menuResources_->configuration,*highscoreTable_,
            pages_.backPage(),pages_.frontPage());
    };
    const auto attemptAttract=[&]() {
        if(!hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::automaticAttract)) {return false;}
        auto decision=clock_.renderWork(phase,{{NormalWorkKind::playableAttractMenuDecision,1}},workHorizon);
        const auto captured=decision.timing.menuDecisionCountdownRead;append(std::move(decision));
        if(!captured) {throw std::logic_error("returned title lost actual1611 captured countdown");}
        if(*captured!=0 || !assets_.attractVoiceAvailable) {return false;}
        audio_->attract_=std::make_shared<PlayableAttractRunner>(*captured,true);
        menuPressed_.reset();menuCursor_.reset();menuArmed_=true;commit();
        auto child=advancePlayableAttract({},workPacketBudget);
        if(child.firstSample!=delivery.firstSample+delivery.samples.size()) {throw std::logic_error("returned attract lost menu PCM prefix");}
        delivery.samples.insert(delivery.samples.end(),child.samples.begin(),child.samples.end());
        delivery.presentations.insert(delivery.presentations.end(),child.presentations.begin(),child.presentations.end());
        return true;
    };
    NativeMenuInput calibrationInput{input.forward,input.reverse,input.confirm||input.fire,input.left,input.right,input.exit,input.pointer};
    calibrationInput.calibrateJoystick=input.systemAction==GameplaySystemAction::calibrateJoystick;
    if(calibrationInput.calibrateJoystick) {input.systemAction=GameplaySystemAction::none;}
    if(joystickCalibrationActive()||calibrationInput.calibrateJoystick) {
        commit();
        if(auto modal=pollMenuJoystickCalibration(calibrationInput,workPacketBudget)) {
            if(modal->firstSample!=delivery.firstSample+delivery.samples.size()) {throw std::logic_error("title calibration lost retained PCM prefix");}
            delivery.samples.insert(delivery.samples.end(),modal->samples.begin(),modal->samples.end());
            delivery.presentations.insert(delivery.presentations.end(),modal->presentations.begin(),modal->presentations.end());
            phase=*audio_->workPhase_;palette=*recoveredStartupPalette_;
            if(joystickCalibrationActive()) {return delivery;}
            input={};
            if(menuDialog_) {paintDialog();pollMenu();return delivery;}
            if(attemptAttract()) {return delivery;}
            repaintMenu();pollMenu();return delivery;
        }
    }
    if(menuDialog_) {
        const auto result=menuDialog_->advance({input.forward,input.reverse,confirm,input.left,input.right,input.exit,input.pointer},
            audio_->menuResources_->configuration,*highscoreTable_,highscoreStore_.get(),audio_->joystick_?&counter:nullptr);
        audio_->synchronizePlayableMusic(phase);
        if(result==MenuDialogResult::pending) {paintDialog();}
        else {
            if(result==MenuDialogResult::preferencesSaved) {audio_->menuPreferencesApplied_=true;}
            menuDialog_.reset();menu_.emplace();menuPressed_.reset();menuCursor_.reset();menuArmed_=true;
            repaintMenu();
            append(clock_.renderWork(phase,{{NormalWorkKind::startupMenuReentryCountdown,1,menuCountdownRawIrqs}},workHorizon));
        }
        pollMenu();return delivery;
    }
    if(pointerCancel) {
        menuPressed_=pointerWidgetPoll(menuPressed_,mainPointerHitBoxes(),
            menuCursor_.value_or(ProjectedPoint{}),PointerButton::cancel).pressed;
        repaintMenu();if(attemptAttract()) {return delivery;}pollMenu();return delivery;
    }
    if(pointer&&!input.forward&&!input.reverse&&!input.left&&!input.right) {
        menuArmed_=true;
        if(rawPointer) {menuCursor_=sourcePointerPosition(input.pointer->position);}
        const auto position=menuCursor_.value_or(ProjectedPoint{static_cast<std::int16_t>(menu_->cursorX()),
            static_cast<std::int16_t>(menu_->cursorY())});
        const auto down=confirm||(rawPointer&&(input.pointer->primary||input.pointer->secondary));
        const auto poll=pointerWidgetPoll(menuPressed_,mainPointerHitBoxes(),position,
            down?PointerButton::held:PointerButton::released);
        menuPressed_=poll.pressed;pointerActivated=poll.activated;
        repaintMenu();
        if(pointerActivated) {menuPressed_=pointerActivated;}
        else {if(attemptAttract()) {return delivery;}pollMenu();return delivery;}
    }
    if ((pointer ? pointerActivated.has_value()
        : !confirm && menuPressed_ && !((input.forward || input.reverse) && menuArmed_))) {
        if(attemptAttract()) {return delivery;}
        if (*menuPressed_!=0) {
            const auto selected=*menuPressed_;menuPressed_.reset();
            if(selected==4) {delivery.exitRequested=true;return delivery;}
            if(!audio_->menuResources_||!highscoreTable_) {
                throw std::invalid_argument("playable submenu requires retained menu resources/table");
            }
            constexpr std::array<PlayableMenuPage,4> dialogs{PlayableMenuPage::main,PlayableMenuPage::briefing,
                PlayableMenuPage::preferences,PlayableMenuPage::scores};
            menuDialog_=std::make_shared<PlayableMenuDialog>(dialogs.at(selected),assets_,audio_->pointerCapability_,
                audio_->playableDetailProfile()?(hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::fullGraphicDetail)
                    ?NativeDetailCapability::fullRange:NativeDetailCapability::bounded51To81):NativeDetailCapability::unavailable,
                audio_->menuFeatures_,PreferenceLayout::title,model_.playableModelDetail(0),audio_->joystick_.get());
            paintDialog();pollMenu();return delivery;
        }
        if (assets_.campaignLevels.empty() || assets_.campaignLevels[0].resourceName!="LEVEL1.RES") {
            throw std::invalid_argument("playable Begin lost its actual Level1 bank");
        }
        renderMenuWidget(0,MenuWidgetState::released);renderMenuCursor();
        if (hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::fullGraphicDetail)) {
            model_.synchronizePlayableDetail(audio_->playableDetail());
        }
        model_.preparePlayableMenuBegin(audio_->playableDifficulty());
        auto fade=clock_.renderBlackout(phase,{palette,{}},pages_,*pixels_,flipHorizon,
            BlackoutPageLayout::recoveredStartup);
        for (auto& step:fade.steps) {
            append(std::move(step.before));palette=step.palette.state;
            append(std::move(step.palette.work));
        }
        append(std::move(fade.clear));palette=fade.state.palette;publish();
        waitPlayableIrqs(delivery,phase,beginWaitIrqs,workPacketBudget);
        if (audio_->playableMusicEnabled()) {
            phase.callback=synth_.stopPlayableSequence();
            phase.callback=synth_.deselectPlayableSequence();
        } else {phase.callback=synth_.releaseSuspendedPlayableSequence();}
        const auto sharedCount=level1RewardModelCapacity(assets_);
        const std::vector<Model3d> shared(assets_.models.begin(),assets_.models.begin()+sharedCount);
        const auto& resources=assets_.campaignLevels[0];
        model_.completePlayableMenuBegin(shared,resources.models);
        initializePages(NativeSessionStart::level1);
        phase.callback=synth_.selectPlayableSequence(resources.musicSequence,PlayableSequenceMode::repeat,audio_->playableMusicPublication());
        phase.playableMusicOff=!audio_->playableMusicEnabled();phase.playableSuccessCountdownWritten=false;
        phase.callback=synth_.resetPlayableDirectEffects();
        campaignResources_=CampaignResourceBorrow{0,&resources.bitmapGroups,&resources.models,&resources.musicSequence};
        level2Cursor_=0;level1Cursor_=0;failureCursor_=0;
        menu_.reset();menuPressed_.reset();menuCursor_.reset();
        pageProjection_=NativePageProjection::retainedPlayableLoader;
        upload(0);commit();endingPalette_.reset();return delivery;
    }
    if (menuArmed_ && (input.forward || input.reverse)) {
        if(menuPressed_) {renderMenuWidget(*menuPressed_,MenuWidgetState::released);menuPressed_.reset();}
        if (input.forward) {menu_->moveUp();} else {menu_->moveDown();}
        menuArmed_=false;renderMenuCursor();
    } else {
        if (!input.forward && !input.reverse) {menuArmed_=true;}
        if (confirm && !menuPressed_) {
            menuPressed_=menu_->selection();renderMenuWidget(*menuPressed_,MenuWidgetState::pressed);renderMenuCursor();
        }
    }
    if(attemptAttract()) {return delivery;}
    model_.finishMenuPoll();
    auto flipped=clock_.renderPageFlip(phase,{PageFlipSynchronization::enabled,pages_.frontPage(),pages_.backPage(),
        0x3d4,PageFlipLayout::recoveredStartup,PageFlipWaitDomain::retainedProgram},flipHorizon);
    append(std::move(flipped.work));publish();commit();
    if(audio_->playablePointerEnabled()) {delivery.pointerPosition=menuCursor_;}
    return delivery;
}

} //namespace stellar7
