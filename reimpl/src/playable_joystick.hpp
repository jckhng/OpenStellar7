#pragma once

#include "stellar7/native_joystick.hpp"

namespace stellar7 {

//Values returned by source2420.255 is the timeout sentinel; presence is the
//post-discharge primary-axis test. This is not a host API coordinate packet.
struct JoystickCounterSample {
    bool connected=false;
    std::uint8_t x=255;
    std::uint8_t y=255;
    std::uint8_t buttons=0;
};

//Explicit portable provider: floor(hostAxis*254/65535), reserving255 for timeout.
JoystickCounterSample joystickCounterSample(NativeJoystickSample sample);

//2494 arithmetic only. Native counter packets do not acquire guest PIT time.
std::uint16_t joystickCounterDelay(std::uint16_t pitElapsed);

enum class JoystickButtonAction { none, fire, activateSelected, cycleSelected };

struct JoystickCalibration {
    JoystickAxes center{};
    JoystickAxes upperLeft{-127,-127};
    JoystickAxes lowerRight{127,127};
};

struct JoystickPoll {
    bool connected=false;
    JoystickAxes normalized{}; //25A8 output, signed-127..127.
    std::uint16_t directions=0; //2625: up1,down2,left4,right8.
    std::uint8_t buttons=0;
};

//One retained primary-stick driver/game calibration and41A2 button history.
//Value-copying supports transactional native calibration cancellation.
class PlayableJoystick {
public:
    bool enable(JoystickCounterSample sample);
    void disable() noexcept;
    bool enabled() const noexcept {return active_;}
    JoystickPoll poll(JoystickCounterSample sample) const;
    JoystickAxes analog(JoystickAxes normalized) const; //8A48 nonlinear output.
    JoystickButtonAction advanceButtons(std::uint8_t buttons);
    //Native focus/disconnect cancellation; emits no source release action.
    void cancelButtons() noexcept {buttonHistory_=0;}
    void beginCalibration();
    JoystickCalibrationPoint captureCalibration(JoystickCounterSample sample);
    JoystickCalibrationPoint calibrationPoint() const noexcept {return point_;}
    const JoystickCalibration& calibration() const noexcept {return calibration_;}

private:
    bool active_=false;
    std::uint8_t centerX_=0;
    std::uint8_t centerY_=0;
    std::uint16_t gainX_=0;
    std::uint16_t gainY_=0;
    std::uint8_t buttonHistory_=0;
    JoystickCalibration calibration_{};
    JoystickCalibrationPoint point_=JoystickCalibrationPoint::idle;
};

} //namespace stellar7
