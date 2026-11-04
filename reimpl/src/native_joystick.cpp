#include "stellar7/native_session.hpp"
#include "playable_joystick.hpp"
#include <stdexcept>
#include <utility>

namespace stellar7 {
NativeAudioOwner::~NativeAudioOwner()=default;

void NativeAudioOwner::acceptJoystickSample(const std::optional<NativeJoystickSample>& sample) {
    if(!sample) {return;}
    if(!joystick_||!hasNativeMenuFeature(menuFeatures_,NativeMenuFeatures::joystickControls)) {
        throw std::invalid_argument("joystick sample requires its retained native controller");
    }
    joystickCounterSample(*sample); //Validate before mutating the last hardware packet.
    const bool cancelled=sample->phase==NativeJoystickPhase::cancel;
    pendingJoystickCancel_=pendingJoystickCancel_||cancelled||(joystickSample_.connected&&!sample->connected);
    joystickSample_=*sample;
    if(cancelled) {joystickSample_.connected=false;joystickSample_.buttons=0;}
    if(!sample->connected||cancelled) {joystick_->cancelButtons();}
}

bool NativeAudioOwner::takeJoystickCancellation() noexcept {
    return std::exchange(pendingJoystickCancel_,false);
}

StartupSettingsWork NativeAudioOwner::joystickSettings(const CockpitSettings& configuration,
    const HighscoreTable& table,const StartupCapabilityProfile profile) {
    if(!joystick_||!context_) {throw std::invalid_argument("joystick settings require their native owner");}
    const auto counter=joystickCounterSample(joystickSample_);
    bool present=false;
    if(configuration.options[1]!=0) {present=joystick_->enable(counter);}
    else {joystick_->disable();}
    return playableJoystickSettingsOperations(context_->context.state,configuration,table,profile,
        present?StartupJoystickPresence::present:StartupJoystickPresence::absent);
}

bool NativeAudioOwner::playableJoystickProfile() const {
    if(!joystick_||!settings_||!menuResources_||!settings_->settings.nativeJoystick
        ||!hasNativeMenuFeature(menuFeatures_,NativeMenuFeatures::joystickControls)) {return false;}
    const auto initial=settings_->settings.configuration.options[1];
    const auto current=menuResources_->configuration.options[1];
    return (initial==0||initial==1)&&(current==0||current==1);
}

NativeMenuInput NativeAudioOwner::joystickMenuInput(NativeMenuInput input) const {
    if(!joystick_) {return input;}
    const auto poll=joystick_->poll(joystickCounterSample(joystickSample_));
    if(!poll.connected) {return input;}
    if(!input.up&&!input.down&&!input.left&&!input.right) {
        input.up=(poll.directions&1)!=0;input.down=(poll.directions&2)!=0;
        input.left=(poll.directions&4)!=0;input.right=(poll.directions&8)!=0;
    }
    input.confirm=input.confirm||poll.buttons!=0;
    return input;
}

bool NativeAudioOwner::joystickButtonHeld() const {
    return joystick_&&joystick_->poll(joystickCounterSample(joystickSample_)).buttons!=0;
}

void NativeAudioOwner::joystickGameplayInput(GameplayInput& input) {
    if(!joystick_) {return;}
    if(takeJoystickCancellation()) {joystick_->cancelButtons();return;}
    const auto poll=joystick_->poll(joystickCounterSample(joystickSample_));
    if(!poll.connected) {return;}
    if(!input.left&&!input.right&&!input.forward&&!input.reverse) {input.analog=joystick_->analog(poll.normalized);}
    switch(joystick_->advanceButtons(poll.buttons)) {
    case JoystickButtonAction::none:break;
    case JoystickButtonAction::fire:input.fire=true;break;
    case JoystickButtonAction::activateSelected:input.joystickAction=GameplaySystemAction::activateSelected;break;
    case JoystickButtonAction::cycleSelected:input.joystickAction=GameplaySystemAction::cycleSelected;break;
    }
}

void NativeStartupSequence::setJoystickSample(const NativeJoystickSample sample) {
    if(!audio_) {throw std::logic_error("joystick sample requires started owner");}
    audio_->acceptJoystickSample(sample);
}
}
