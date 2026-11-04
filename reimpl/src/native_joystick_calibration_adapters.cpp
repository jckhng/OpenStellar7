#include "stellar7/native_session.hpp"
#include "native_joystick_calibration_run.hpp"
#include "playable_menu_dialog.hpp"
#include <stdexcept>

namespace stellar7 {
namespace {
NativeJoystickCancellation cancellation(const bool cancelled) {
    return cancelled?NativeJoystickCancellation::cancelled:NativeJoystickCancellation::none;
}
bool saveMenuCalibration(CockpitSettings& configuration,HighscoreTable& scores,HighscoreStore* store) {
    auto candidate=scores;candidate.setConfiguration(configuration);
    if(store) {
        const auto saved=store->saveAndReload(candidate);
        if(saved.status!=HighscoreSaveStatus::saved||!saved.reloaded) {return false;}
        candidate=*saved.reloaded;
    }
    configuration=decodeCockpitSettings(std::vector<std::uint8_t>(candidate.header().begin(),candidate.header().end()));
    scores=std::move(candidate);return true;
}
void appendBriefingCallback(NativeSessionDelivery& delivery,PresentationClock& clock,SoundBlasterClockPhase& phase,const std::uint16_t raw) {
    constexpr std::int64_t workHorizon=120000;
    auto work=clock.renderWork(phase,{{NormalWorkKind::playableJoystickBriefingRefresh,1,raw}},workHorizon);
    if(work.audio.firstSample!=delivery.firstSample+delivery.samples.size()) {throw std::logic_error("calibration Briefing callback lost PCM epoch");}
    phase=work.timing.phase;delivery.samples.insert(delivery.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
}
bool neutral(const NativeMenuInput& input) {
    return !input.up&&!input.down&&!input.left&&!input.right&&!input.confirm&&!input.cancel
        &&!(input.pointer&&(input.pointer->primary||input.pointer->secondary));
}
}
bool NativeStartupSequence::joystickCalibrationActive() const noexcept {
    return audio_&&audio_->joystickCalibrationRun_&&audio_->joystickCalibrationRun_->active();
}
bool NativeStartupSequence::joystickCalibrationRejected() const noexcept {
    return audio_&&audio_->joystickCalibrationRun_&&audio_->joystickCalibrationRun_->rejected();
}
JoystickCalibrationPoint NativeStartupSequence::joystickCalibrationPoint() const noexcept {
    return audio_&&audio_->joystickCalibrationRun_?audio_->joystickCalibrationRun_->point():JoystickCalibrationPoint::idle;
}
bool NativeGameplaySession::joystickCalibrationActive() const noexcept {
    return audio_&&audio_->joystickCalibrationRun_&&audio_->joystickCalibrationRun_->active();
}
bool NativeGameplaySession::joystickCalibrationRejected() const noexcept {
    return audio_&&audio_->joystickCalibrationRun_&&audio_->joystickCalibrationRun_->rejected();
}
JoystickCalibrationPoint NativeGameplaySession::joystickCalibrationPoint() const noexcept {
    return audio_&&audio_->joystickCalibrationRun_?audio_->joystickCalibrationRun_->point():JoystickCalibrationPoint::idle;
}
NativeSessionDelivery NativeGameplaySession::advanceJoystickCalibration(const GameplayInput& input,const std::size_t budget) {
    if(!joystickCalibrationActive()||!audio_->workPhase_||!pixels_||!recoveredStartupPalette_) {
        throw std::invalid_argument("calibration requires retained native session owners");
    }
    NativeMenuInput controls{input.forward,input.reverse,input.confirm||input.fire,input.left,input.right,input.exit,input.pointer};
    if(!audio_->playablePointerEnabled()) {controls.pointer.reset();}
    return audio_->joystickCalibrationRun_->advance(assets_,*pixels_,pages_.frontPage(),frame_,clock_,*audio_->workPhase_,
        audio_->joystickSample_,controls,cancellation(audio_->takeJoystickCancellation()),budget);
}
std::optional<NativeSessionDelivery> NativeStartupSequence::pollMenuJoystickCalibration(const NativeMenuInput& requested,const std::size_t budget) {
    if(!joystickCalibrationActive()) {
        if(!requested.calibrateJoystick||!audio_||!audio_->joystick_) {return std::nullopt;}
        if(menuDialog_&&!menuDialog_->calibrationAvailable()) {return std::nullopt;}
        auto effective=audio_->joystickMenuInput(requested);
        if(!audio_->playablePointerEnabled()) {effective.pointer.reset();}
        if(!neutral(effective)) {return std::nullopt;} //5A44..5A52: no action/direction/confirmation.
        if(!audio_->workPhase_) {throw std::invalid_argument("menu calibration requires retained clock phase");}
        audio_->joystickCalibrationRun_=std::make_shared<NativeJoystickCalibrationRun>(*audio_->joystick_,
            NativeJoystickCalibrationCaller::menu,*audio_->workPhase_);
    }
    const auto front=menuPageSegment(NativeMenuPage::front);
    auto controls=requested;if(!audio_->playablePointerEnabled()) {controls.pointer.reset();}
    auto delivery=audio_->joystickCalibrationRun_->advance(game_,pages_,front,frame_,audio_->clock_,*audio_->workPhase_,
        audio_->joystickSample_,controls,cancellation(audio_->takeJoystickCancellation()),budget);
    if(!joystickCalibrationActive()) {
        if(!saveMenuCalibration(audio_->menuResources_->configuration,*audio_->currentScores_,highscoreStore_.get())) {
            audio_->joystickCalibrationRun_->menuSaveFailed();
        } else if(menuDialog_) {
            const auto raw=menuDialog_->menuCalibrationSaved(audio_->menuResources_->configuration);
            if(raw) {appendBriefingCallback(delivery,audio_->clock_,*audio_->workPhase_,*raw);}
        }
    }
    return delivery;
}
std::optional<NativeSessionDelivery> NativeGameplaySession::pollMenuJoystickCalibration(const NativeMenuInput& requested,const std::size_t budget) {
    if(model_.phase()!=GameplayPhase::nativeMenu||livePreferences_) {return std::nullopt;}
    if(!joystickCalibrationActive()) {
        if(!requested.calibrateJoystick||!audio_->joystick_) {return std::nullopt;}
        if(menuDialog_&&!menuDialog_->calibrationAvailable()) {return std::nullopt;}
        auto effective=audio_->joystickMenuInput(requested);
        if(!audio_->playablePointerEnabled()) {effective.pointer.reset();}if(!neutral(effective)) {return std::nullopt;}
        if(!audio_->workPhase_) {throw std::invalid_argument("title calibration requires retained clock phase");}
        audio_->joystickCalibrationRun_=std::make_shared<NativeJoystickCalibrationRun>(*audio_->joystick_,
            NativeJoystickCalibrationCaller::menu,*audio_->workPhase_);
    }
    GameplayInput input;input.left=requested.left;input.right=requested.right;input.forward=requested.up;input.reverse=requested.down;
    input.exit=requested.cancel;input.confirm=requested.confirm;input.pointer=requested.pointer;
    auto delivery=advanceJoystickCalibration(input,budget);
    if(!joystickCalibrationActive()) {
        if(!saveMenuCalibration(audio_->menuResources_->configuration,*highscoreTable_,highscoreStore_.get())) {
            audio_->joystickCalibrationRun_->menuSaveFailed();
        } else if(menuDialog_) {
            const auto raw=menuDialog_->menuCalibrationSaved(audio_->menuResources_->configuration);
            if(raw) {appendBriefingCallback(delivery,clock_,*audio_->workPhase_,*raw);}
        }
    }
    return delivery;
}
} //namespace stellar7
