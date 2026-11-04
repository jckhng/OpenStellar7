#include "stellar7/native_session.hpp"
#include "playable_menu_dialog.hpp"

#include <stdexcept>

namespace stellar7 {
bool NativeGameplaySession::soundsEnabled() const {return audio_&&audio_->playableSoundsEnabled();}
namespace {
constexpr std::size_t maximumPacketBudget=10000;
constexpr std::int64_t flipHorizon=240000;
}

NativeSessionDelivery NativeGameplaySession::advancePlayablePreferences(const GameplayInput& requested,
    const std::size_t workPacketBudget) {
    auto input=requested;
    const auto controls=audio_->joystickMenuInput({input.forward,input.reverse,input.confirm||input.fire,input.left,input.right,input.exit,input.pointer});
    input.forward=controls.up;input.reverse=controls.down;input.left=controls.left;input.right=controls.right;input.confirm=controls.confirm;
    const auto counter=joystickCounterSample(audio_->joystickSample_);
    if(!livePreferences_||!playableCampaign_||!audio_->menuResources_||!highscoreTable_
        ||!audio_->workPhase_||!pixels_||!recoveredStartupPalette_
        ||!hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::fullGraphicDetail)
        ||workPacketBudget==0||workPacketBudget>maximumPacketBudget
        ||input.systemAction!=GameplaySystemAction::none) {
        throw std::invalid_argument("live preferences require retained playable owners and modal input");
    }
    NativeSessionDelivery delivery;delivery.firstSample=elapsedSamples();
    auto phase=*audio_->workPhase_;
    if(!menuDialog_) {
        menuDialog_=std::make_shared<PlayableMenuDialog>(PlayableMenuPage::preferences,assets_,audio_->pointerCapability_,
            audio_->playableDetailProfile()?NativeDetailCapability::fullRange:NativeDetailCapability::unavailable,
            audio_->menuFeatures_,PreferenceLayout::inGame,PlayableModelDetail{},audio_->joystick_.get());
        pages_.beginModal();
    }
    if(audio_->takeJoystickCancellation()) {menuDialog_->cancelInteraction();}
    const auto result=menuDialog_->advance({input.forward,input.reverse,input.confirm||input.fire,
        input.left,input.right,input.exit,input.pointer},audio_->menuResources_->configuration,*highscoreTable_,highscoreStore_.get(),audio_->joystick_?&counter:nullptr);
    audio_->synchronizePlayableMusic(phase);
    if(result==MenuDialogResult::pending) {
        menuDialog_->paint(*pixels_,assets_,audio_->menuResources_->configuration,*highscoreTable_,
            pages_.backPage(),pages_.frontPage());
    } else {
        if(result==MenuDialogResult::preferencesSaved) {audio_->menuPreferencesApplied_=true;}
        //7CF1..7D22:8AFC,3010,50C4(0),8F1C,7970(0). The page/model
        //helper belongs to the detail renderer; it must preserve radar/allocations.
        restorePlayablePreferencesPages();
        const auto& pending=model_.level2AudioEvents();
        if(level2Cursor_>pending.size()) {throw std::logic_error("live preferences lost its retained audio cursor");}
        while(level2Cursor_<pending.size()) {
            const auto event=pending[level2Cursor_];
            if(audio_->playableSoundsEnabled()) {phase.callback=synth_.triggerPlayableEvent(event.eventId);}
            delivery.events.push_back(event);++level2Cursor_;
        }
        menuDialog_.reset();livePreferences_=false;
    }
    auto flipped=clock_.renderPageFlip(phase,{PageFlipSynchronization::enabled,pages_.frontPage(),pages_.backPage(),
        0x3d4,PageFlipLayout::recoveredStartup,PageFlipWaitDomain::retainedProgram},flipHorizon);
    delivery.samples=std::move(flipped.work.audio.samples);audio_->workPhase_=flipped.work.timing.phase;
    frame_.pixels=*retainedPagePixels(NativePageRole::front);
    frame_.palette=expandVgaDacPalette(std::vector<std::uint8_t>(recoveredStartupPalette_->dac.colors.begin(),
        recoveredStartupPalette_->dac.colors.end()));
    delivery.presentations.push_back({elapsedSamples(),frame_});
    if(livePreferences_&&audio_->playablePointerEnabled()) {delivery.pointerPosition=menuDialog_->pointerPosition();}
    return delivery;
}
} //namespace stellar7
