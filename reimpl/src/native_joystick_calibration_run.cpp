#include "native_joystick_calibration_run.hpp"
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::size_t nativePollIterations=512;
constexpr std::int64_t workHorizon=120000;
constexpr std::uint16_t releaseDebounceRawIrqs=10;
}
NativeJoystickCalibrationRun::NativeJoystickCalibrationRun(PlayableJoystick& joystick,
    const NativeJoystickCalibrationCaller caller,const SoundBlasterClockPhase& phase)
    :joystick_(&joystick),caller_(caller) {
    if(!phase.playableJoystickControls||!phase.timer||phase.timer->activeMask==0) {
        throw std::invalid_argument("calibration requires its retained native PIT owner");
    }
    if(caller==NativeJoystickCalibrationCaller::menu) {savedMenuCountdown_=phase.timer->globalCount;}
    if(joystick.enabled()) {presenter_=std::make_shared<PlayableJoystickCalibration>(joystick);}
}
void NativeJoystickCalibrationRun::menuSaveFailed() {
    if(active_||caller_!=NativeJoystickCalibrationCaller::menu) {throw std::logic_error("save retry requires completed menu calibration");}
    active_=true;painted_=false;waiting_=false;saveFailed_=true;notice_=NoticePhase::press;
}
JoystickCalibrationPoint NativeJoystickCalibrationRun::point() const noexcept {
    return presenter_?presenter_->point():JoystickCalibrationPoint::idle;
}
NativeSessionDelivery NativeJoystickCalibrationRun::advance(const GameplayAssets& assets,IndexedPages& pixels,
    const std::uint16_t frontPage,IndexedScreen& frame,PresentationClock& clock,SoundBlasterClockPhase& phase,
    const NativeJoystickSample sample,const NativeMenuInput input,const NativeJoystickCancellation cancellation,const std::size_t budget) {
    if(!active_||!joystick_||!phase.playableJoystickControls||!phase.timer||phase.timer->activeMask==0||budget==0||budget>10000) {
        throw std::invalid_argument("calibration requires retained native presentation owners");
    }
    NativeSessionDelivery delivery;delivery.firstSample=clock.elapsedSamples();
    const auto append=[&](ClockedPresentationWork work) {
        if(work.audio.firstSample!=delivery.firstSample+delivery.samples.size()) {throw std::logic_error("calibration lost PCM epoch");}
        phase=work.timing.phase;delivery.samples.insert(delivery.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
    };
    const auto paint=[&](const bool rejected) {
        auto effects=saveFailed_ ? joystickCalibrationSaveFailedPageEffects(assets.artGroups.at(0),assets.titleFont,assets.scoreFont,frontPage)
            : presenter_ ? joystickCalibrationPageEffects(assets.artGroups.at(0),assets.titleFont,assets.scoreFont,presenter_->point(),frontPage)
            : rejected ? joystickCalibrationRejectedPageEffects(assets.artGroups.at(0),assets.titleFont,assets.scoreFont,frontPage)
                : joystickDisabledPageEffects(assets.artGroups.at(0),assets.titleFont,frontPage);
        painted_=true;pixels.apply(effects);frame.pixels=pixels.snapshot(frontPage);
        delivery.presentations.push_back({clock.elapsedSamples(),frame});
    };
    const bool cancelled=cancellation==NativeJoystickCancellation::cancelled;
    if(cancelled) {joystick_->cancelButtons();if(!presenter_) {notice_=NoticePhase::press;}}
    const bool held=!cancelled&&(input.confirm||joystick_->poll(joystickCounterSample(sample)).buttons!=0
        ||(input.pointer&&(input.pointer->primary||input.pointer->secondary)));
    bool returned=false;
    if(waiting_) {
        auto work=clock.renderWork(phase,{{NormalWorkKind::playableJoystickDebouncePoll,nativePollIterations}},workHorizon);
        returned=work.timing.countdownReturned;append(std::move(work));
        if(returned) {waiting_=false;}
    } else {
        //88CB/D4 CALL8C70,OR,Jcc; the host query is an untimed native provider.
        append(clock.renderWork(phase,{{NormalWorkKind::instructions,3*nativePollIterations}},workHorizon));
    }
    bool wait=false;
    if(presenter_) {
        auto counter=joystickCounterSample(sample);
        if(cancelled) {counter.connected=false;}
        const auto step=presenter_->advance(counter,
            held?JoystickCalibrationActivation::held:JoystickCalibrationActivation::released,
            returned?JoystickCalibrationWait::returned:JoystickCalibrationWait::pending);
        wait=step.waitRawIrqs!=0;
        if(step.phase==JoystickCalibrationPhase::complete) {
            presenter_.reset();active_=false;
        } else if(step.phase==JoystickCalibrationPhase::rejected||step.phase==JoystickCalibrationPhase::unavailable) {
            rejected_=step.phase==JoystickCalibrationPhase::rejected;
            if(step.phase==JoystickCalibrationPhase::unavailable) {joystick_->cancelButtons();}
            presenter_.reset();notice_=NoticePhase::press;paint(rejected_);
        } else if(step.promptChanged||!painted_) {paint(false);}
        //First call paints the first prompt; no pages/LOD/radar are reinitialized.
    } else {
        if(notice_==NoticePhase::press&&held) {notice_=NoticePhase::release;}
        else if(notice_==NoticePhase::release&&!held) {notice_=NoticePhase::debounce;wait=true;}
        else if(notice_==NoticePhase::debounce&&returned) {
            if(held) {notice_=NoticePhase::release;}
            else {active_=false;}
        }
        if(active_&&!painted_) {paint(rejected_);}
    }
    if(wait) {
        append(clock.renderWork(phase,{{NormalWorkKind::playableJoystickDebounceStore,1,releaseDebounceRawIrqs}},workHorizon));
        waiting_=true;
    }
    if(!active_&&savedMenuCountdown_) {phase.timer->globalCount=*savedMenuCountdown_;} //5A54/5A69.
    return delivery;
}
} //namespace stellar7
