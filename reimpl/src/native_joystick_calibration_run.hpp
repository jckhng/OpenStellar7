#pragma once
#include "playable_joystick_calibration.hpp"
#include "stellar7/native_startup.hpp"

namespace stellar7 {
enum class NativeJoystickCalibrationCaller {ordinary,menu};
enum class NativeJoystickCancellation {none,cancelled};

//One retained modal adapter; all presentation/device owners are borrowed.
class NativeJoystickCalibrationRun {
public:
    NativeJoystickCalibrationRun(PlayableJoystick& joystick,NativeJoystickCalibrationCaller caller,
        const SoundBlasterClockPhase& phase);
    bool active() const noexcept {return active_;}
    bool rejected() const noexcept {return rejected_;}
    void menuSaveFailed();
    JoystickCalibrationPoint point() const noexcept;
    NativeJoystickCalibrationCaller caller() const noexcept {return caller_;}
    NativeSessionDelivery advance(const GameplayAssets& assets,IndexedPages& pixels,std::uint16_t frontPage,
        IndexedScreen& frame,PresentationClock& clock,SoundBlasterClockPhase& phase,
        NativeJoystickSample sample,NativeMenuInput input,NativeJoystickCancellation cancellation,std::size_t budget);
private:
    PlayableJoystick* joystick_;
    NativeJoystickCalibrationCaller caller_;
    std::optional<std::uint16_t> savedMenuCountdown_;
    std::shared_ptr<PlayableJoystickCalibration> presenter_;
    bool active_=true,painted_=false,waiting_=false,rejected_=false,saveFailed_=false;
    enum class NoticePhase {press,release,debounce};
    NoticePhase notice_=NoticePhase::press;
};
} //namespace stellar7
