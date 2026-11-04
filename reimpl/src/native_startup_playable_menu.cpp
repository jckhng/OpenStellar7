#include "stellar7/native_startup.hpp"
#include "playable_menu_pages.hpp"
#include "playable_menu_dialog.hpp"
#include "playable_attract.hpp"
#include "retained_menu_clock_domain.hpp"
#include "playable_pointer.hpp"

#include <stdexcept>

namespace stellar7 {
namespace {
void appendMenuDelivery(NativeSessionDelivery& delivery,NativeSessionDelivery child) {
    if(child.firstSample!=delivery.firstSample+delivery.samples.size()) {throw std::logic_error("native menu lost retained PCM prefix");}
    delivery.samples.insert(delivery.samples.end(),child.samples.begin(),child.samples.end());
    delivery.presentations.insert(delivery.presentations.end(),child.presentations.begin(),child.presentations.end());
    delivery.exitRequested=child.exitRequested;delivery.pointerPosition=child.pointerPosition;
}
}
bool NativeStartupSequence::menuBackAvailable() const noexcept {
    return menuDialog_&&menuDialog_->backAvailable();
}
std::optional<NativeAttractProgress> NativeStartupSequence::attractProgress() const noexcept {
    return audio_&&audio_->attract_ ? std::optional{audio_->attract_->progress()} : std::nullopt;
}
bool NativeStartupSequence::attractAcceptsSourceKey() const noexcept {
    return audio_&&audio_->attract_&&audio_->attract_->acceptsSourceKey();
}
NativeSessionDelivery NativeStartupSequence::advancePlayableAttract(const NativeMenuInput input,const std::size_t budget) {
    if(!audio_->attract_ || !audio_->startupAssets_ || !audio_->menuResources_ || !audio_->menuResources_->streamsClosed) {
        throw std::logic_error("native attract lost its closed-stream menu caller");
    }
    if(!storyRoles_) {storyRoles_=CockpitPageState::fromRecoveredMenu();}
    audio_->attractPagesTransferred_=true;
    auto& palette=audio_->menuEntry_->palette;
    const bool buttons=audio_->joystickButtonHeld() || (input.pointer&&(input.pointer->primary||input.pointer->secondary));
    auto delivery=audio_->attract_->advance({pages_,*storyRoles_,palette,audio_->synth_,audio_->clock_,*audio_->workPhase_,
        game_,*audio_->startupAssets_,audio_->menuResources_->configuration,audio_->attractCreditsSkip_,{}},
        {input,input.sourceKey,buttons},budget);
    if(audio_->attract_->returnedToMenu()) {
        audio_->attract_.reset();menu_=MenuModel{};pressedMenu_.reset();menuCursor_.reset();menuArmed_=true;
        auto store=audio_->clock_.renderWork(*audio_->workPhase_,{{NormalWorkKind::playableAttractCountdownStore,1,menuCountdownRawIrqs}},120000);
        *audio_->workPhase_=store.timing.phase;delivery.samples.insert(delivery.samples.end(),store.audio.samples.begin(),store.audio.samples.end());
        paintPlayableMainMenu(pages_,game_.artGroups.at(0),game_.titleFont,menu_,pressedMenu_,
            storyRoles_->backPage(),storyRoles_->frontPage());
    }
    frame_.pixels=pages_.snapshot(storyRoles_->frontPage());
    frame_.palette=expandVgaDacPalette(std::vector<std::uint8_t>(palette.dac.colors.begin(),palette.dac.colors.end()));
    return delivery;
}
NativeSessionDelivery NativeStartupSequence::pollPlayableMenu(const NativeMenuInput requested,
    const std::size_t workPacketBudget,const NativeDeliverySink& sink) {
    if(phase_!=NativeStartupPhase::awaitingMenuDecision||!audio_||!audio_->workPhase_
        ||!audio_->menuEntry_||!audio_->currentScores_||workPacketBudget==0||workPacketBudget>10000) {
        throw std::invalid_argument("playable menu requires its retained startup entry");
    }
    auto input=audio_->joystickMenuInput(requested);
    const bool cancelled=audio_->takeJoystickCancellation();
    if(cancelled) {
        pressedMenu_.reset();menuCursor_.reset();menuArmed_=true;
        if(menuDialog_) {menuDialog_->cancelInteraction();}
        if(audio_->attract_) {audio_->attract_->cancelInteraction();}
        input={};
    }
    if(audio_->attract_) {return advancePlayableAttract(input,workPacketBudget);}
    const auto counter=joystickCounterSample(audio_->joystickSample_);
    NativeSessionDelivery delivery;delivery.firstSample=elapsedSamples();
    bool calibrationReturned=false;
    if(joystickCalibrationActive()) {
        if(auto modal=pollMenuJoystickCalibration(requested,workPacketBudget)) {appendMenuDelivery(delivery,std::move(*modal));}
        if(joystickCalibrationActive()) {return delivery;}
        input={};calibrationReturned=true;
    }
    if(!calibrationReturned&&menuDialog_&&!cancelled&&requested.calibrateJoystick) {
        if(auto modal=pollMenuJoystickCalibration(requested,workPacketBudget)) {
            appendMenuDelivery(delivery,std::move(*modal));
            if(joystickCalibrationActive()) {return delivery;}
            input={};calibrationReturned=true;
        }
    }
    const bool automatic=hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::automaticAttract);
    const bool pointer=audio_->pointerCapability_==NativePointerCapability::portablePointer;
    const bool pointerCancel=pointer&&input.pointer&&input.pointer->phase==NativePointerPhase::cancel;
    const bool rawPointer=!pointerCancel&&audio_->playablePointerEnabled()&&input.pointer.has_value();
    if(!automatic&&!cancelled&&!calibrationReturned&&!input.calibrateJoystick
        &&!pointer&&!menuDialog_&&menu_.selection()==0&&!input.up&&!input.down) {
        executeRecoveredMenu(NativeMenuInput{false,false,input.confirm},workPacketBudget,sink,
            NativeMenuBeginProfile::enterPressRelease,MenuAttractProvider::unavailable);
        delivery.samples=audio_->menuInput_->samples;return delivery;
    }
    const auto back=menuPageSegment(NativeMenuPage::back),front=menuPageSegment(NativeMenuPage::front);
    const bool hadDialog=static_cast<bool>(menuDialog_);
    bool returnedFromDialog=false;
    std::optional<std::size_t> activated;
    if(calibrationReturned) {
        //The callback returnedFF with its saved countdown; no released widget.
    } else if(menuDialog_) {
        if(!audio_->menuResources_) {throw std::invalid_argument("submenu lost effective menu configuration");}
        const auto result=menuDialog_->advance(input,audio_->menuResources_->configuration,
            *audio_->currentScores_,highscoreStore_.get(),audio_->joystick_?&counter:nullptr);
        audio_->synchronizePlayableMusic(*audio_->workPhase_);
        if(result!=MenuDialogResult::pending) {
            if(result==MenuDialogResult::preferencesSaved) {audio_->menuPreferencesApplied_=true;}
            menuDialog_.reset();returnedFromDialog=true;
            menu_=MenuModel{};pressedMenu_.reset();menuCursor_.reset();menuArmed_=true;
        }
    } else if(pointerCancel) {
        pressedMenu_=pointerWidgetPoll(pressedMenu_,mainPointerHitBoxes(),
            menuCursor_.value_or(ProjectedPoint{}),PointerButton::cancel).pressed;
    } else if(pointer&&!input.up&&!input.down&&!input.left&&!input.right) {
        menuArmed_=true;
        if(rawPointer) {menuCursor_=sourcePointerPosition(input.pointer->position);}
        const auto position=menuCursor_.value_or(ProjectedPoint{static_cast<std::int16_t>(menu_.cursorX()),
            static_cast<std::int16_t>(menu_.cursorY())});
        const auto down=input.confirm||(rawPointer&&(input.pointer->primary||input.pointer->secondary));
        const auto poll=pointerWidgetPoll(pressedMenu_,mainPointerHitBoxes(),position,
            down?PointerButton::held:PointerButton::released);
        pressedMenu_=poll.pressed;activated=poll.activated;
    } else if(menuArmed_&&(input.up||input.down)) {
        pressedMenu_.reset();menuCursor_.reset();
        if(input.up) {menu_.moveUp();} else {menu_.moveDown();}
        menuArmed_=false;
    } else {
        if(!input.up&&!input.down) {menuArmed_=true;}
        if(input.confirm&&!pressedMenu_) {pressedMenu_=menu_.selection();}
        else if(!input.confirm&&pressedMenu_) {activated=pressedMenu_;pressedMenu_.reset();}
    }
    if(!hadDialog&&!activated&&!calibrationReturned&&!input.up&&!input.down&&!input.left&&!input.right&&!input.confirm) {
        if(auto modal=pollMenuJoystickCalibration(requested,workPacketBudget)) {
            appendMenuDelivery(delivery,std::move(*modal));
            if(joystickCalibrationActive()) {return delivery;}
            input={};calibrationReturned=true;
        }
    }
    if(automatic&&!hadDialog) {
        auto decision=audio_->clock_.renderWork(*audio_->workPhase_,{{NormalWorkKind::playableAttractMenuDecision,1}},120000);
        const auto captured=decision.timing.menuDecisionCountdownRead;
        *audio_->workPhase_=decision.timing.phase;delivery.samples.insert(delivery.samples.end(),decision.audio.samples.begin(),decision.audio.samples.end());
        if(!captured) {throw std::logic_error("native menu lost actual1611 captured countdown");}
        if(*captured==0&&game_.attractVoiceAvailable) {
            audio_->attract_=std::make_shared<PlayableAttractRunner>(*captured,true);
            pressedMenu_.reset();menuCursor_.reset();menuArmed_=true;
            appendMenuDelivery(delivery,advancePlayableAttract({},workPacketBudget));return delivery;
        }
    }
    if(activated) {
        const auto selected=*activated;
        if(selected==4) {delivery.exitRequested=true;return delivery;}
        if(selected==0) {
            if(sink&&!delivery.samples.empty()) {sink(delivery);}
            pressedMenu_=0;
            executeRecoveredMenu(std::nullopt,workPacketBudget,sink,
                pointer ? NativeMenuBeginProfile::portablePointer : NativeMenuBeginProfile::playableWidget);
            NativeSessionDelivery child;child.firstSample=audio_->begin_->firstSample;child.samples=audio_->begin_->samples;
            appendMenuDelivery(delivery,std::move(child));return delivery;
        }
        constexpr std::array<PlayableMenuPage,4> dialogs{PlayableMenuPage::main,PlayableMenuPage::briefing,
            PlayableMenuPage::preferences,PlayableMenuPage::scores};
        menuDialog_=std::make_shared<PlayableMenuDialog>(dialogs.at(selected),game_,audio_->pointerCapability_,
            audio_->playableDetailProfile()?(hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::fullGraphicDetail)
                ?NativeDetailCapability::fullRange:NativeDetailCapability::bounded51To81):NativeDetailCapability::unavailable,
            audio_->menuFeatures_,PreferenceLayout::title,PlayableModelDetail{},audio_->joystick_.get());
    }
    if(menuDialog_) {menuDialog_->paint(pages_,game_,audio_->menuResources_->configuration,*audio_->currentScores_,back,front);}
    else {paintPlayableMainMenu(pages_,game_.artGroups.at(0),game_.titleFont,menu_,pressedMenu_,back,front,menuCursor_);}
    if(returnedFromDialog) {
        auto reset=audio_->clock_.renderWork(*audio_->workPhase_,
            {{NormalWorkKind::startupMenuReentryCountdown,1,menuCountdownRawIrqs}},120000);
        *audio_->workPhase_=reset.timing.phase;
        delivery.samples.insert(delivery.samples.end(),reset.audio.samples.begin(),reset.audio.samples.end());
    }
    auto work=audio_->clock_.renderPageFlip(*audio_->workPhase_,{PageFlipSynchronization::enabled,front,back,
        0x3d4,PageFlipLayout::recoveredStartup,PageFlipWaitDomain::retainedProgram},240000);
    *audio_->workPhase_=work.work.timing.phase;
    delivery.samples.insert(delivery.samples.end(),work.work.audio.samples.begin(),work.work.audio.samples.end());
    if(audio_->playablePointerEnabled()) {
        delivery.pointerPosition=menuDialog_?menuDialog_->pointerPosition():menuCursor_.value_or(
            ProjectedPoint{static_cast<std::int16_t>(menu_.cursorX()),static_cast<std::int16_t>(menu_.cursorY())});
    }
    frame_.pixels=pages_.snapshot(front);
    frame_.palette=expandVgaDacPalette(std::vector<std::uint8_t>(audio_->menuEntry_->palette.dac.colors.begin(),
        audio_->menuEntry_->palette.dac.colors.end()));return delivery;
}
}
