#include "playable_joystick.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::uint8_t supportedButtons=3;
constexpr std::uint8_t primaryButton=1;
constexpr std::uint8_t secondaryButton=2;
constexpr std::uint8_t counterTimeout=255;
constexpr std::uint32_t hostAxisMaximum=65535;
constexpr std::uint32_t nativeCounterMaximum=254;
constexpr std::uint16_t driverGainNumerator=0x7f00;
constexpr std::uint16_t counterDelayNumerator=0x6fcc;
constexpr std::int16_t normalizedMaximum=127;
constexpr std::uint8_t normalizedDeadzone=8;
constexpr std::int16_t digitalThreshold=30;
constexpr std::uint16_t directionUp=1,directionDown=2,directionLeft=4,directionRight=8;
constexpr std::int16_t emptyCalibrationExtent=128;
constexpr std::int16_t analogDivisor=8;
//Immutable original DS13D8..13E8, not a fitted response curve.
constexpr std::array<std::int16_t,9> analogTable{0,16,32,48,64,80,96,112,127};

void validateButtons(const std::uint8_t buttons) {
    if((buttons&~supportedButtons)!=0) {throw std::invalid_argument("unsupported joystick button bits");}
}

std::int16_t normalizeAxis(const std::uint8_t count,const std::uint8_t center,const std::uint16_t gain) {
    const auto difference=static_cast<std::int16_t>(static_cast<int>(count)-center);
    const bool negative=difference<0;
    auto magnitude=static_cast<std::uint16_t>(negative?-difference:difference);
    if(gain!=0) {
        //245C discards MUL's DX before SHR AX,8. Preserve that16-bit wrap.
        magnitude=static_cast<std::uint16_t>(static_cast<std::uint32_t>(magnitude)*gain)>>8;
    }
    auto low=static_cast<std::uint8_t>(magnitude);
    low=std::min(low,static_cast<std::uint8_t>(normalizedMaximum));
    if(low<normalizedDeadzone) {low=0;}
    return static_cast<std::int16_t>(negative?-static_cast<int>(low):low);
}

std::uint16_t directions(const JoystickAxes axes) {
    std::uint16_t mask=0;
    if(axes.x<-digitalThreshold) {mask|=directionLeft;}
    else if(axes.x>=digitalThreshold) {mask|=directionRight;}
    if(axes.y<-digitalThreshold) {mask|=directionUp;}
    else if(axes.y>=digitalThreshold) {mask|=directionDown;}
    return mask;
}

std::int16_t magnitude(const std::int16_t value) {
    return static_cast<std::int16_t>(value<0?-value:value);
}

std::int16_t sarThree(const std::int16_t value) {
    //C++ signed division truncates toward0; source SAR rounds negative down.
    if(value>=0) {return static_cast<std::int16_t>(value/analogDivisor);}
    return static_cast<std::int16_t>(-((-static_cast<int>(value)+analogDivisor-1)/analogDivisor));
}

std::int16_t analogAxis(const std::int16_t value,const std::int16_t center,const std::int16_t extent) {
    if(value<-normalizedMaximum||value>normalizedMaximum) {
        throw std::invalid_argument("joystick analog requires source-normalized axes");
    }
    const auto divisor=sarThree(extent);
    if(divisor==0) {throw std::invalid_argument("joystick calibration produces source divide by zero");}
    const auto clipped=std::min(magnitude(value),extent);
    const auto index=static_cast<int>(clipped)/divisor;
    if(index<0||index>=static_cast<int>(analogTable.size())) {
        throw std::invalid_argument("joystick calibration leaves the recovered analog table");
    }
    //8A48 compares the saved center for SIGN, but does not subtract it before ABS.
    const auto output=analogTable[static_cast<std::size_t>(index)];
    return static_cast<std::int16_t>(center>value?-output:output);
}

std::int16_t finishExtent(const std::int16_t upperLeft,std::int16_t lowerRight) {
    const auto upperMagnitude=magnitude(upperLeft);
    if(magnitude(lowerRight)>upperMagnitude) {lowerRight=upperMagnitude;}
    //89xx retains signed lowerRight when its magnitude did not exceed upperLeft.
    return lowerRight==0?emptyCalibrationExtent:lowerRight;
}
}

JoystickCounterSample joystickCounterSample(const NativeJoystickSample sample) {
    validateButtons(sample.buttons);
    if(sample.phase!=NativeJoystickPhase::sample&&sample.phase!=NativeJoystickPhase::cancel) {
        throw std::invalid_argument("unsupported native joystick phase");
    }
    if(!sample.connected||sample.phase==NativeJoystickPhase::cancel) {return {};}
    return {true,static_cast<std::uint8_t>(static_cast<std::uint32_t>(sample.x)*nativeCounterMaximum/hostAxisMaximum),
        static_cast<std::uint8_t>(static_cast<std::uint32_t>(sample.y)*nativeCounterMaximum/hostAxisMaximum),sample.buttons};
}

std::uint16_t joystickCounterDelay(const std::uint16_t pitElapsed) {
    if(pitElapsed==0) {throw std::invalid_argument("joystick counter calibration has zero PIT delta");}
    return std::max<std::uint16_t>(1,counterDelayNumerator/pitElapsed);
}

bool PlayableJoystick::enable(const JoystickCounterSample sample) {
    validateButtons(sample.buttons);
    centerX_=sample.x;centerY_=sample.y;
    if(sample.x!=0) {gainX_=driverGainNumerator/sample.x;}
    if(sample.y!=0) {gainY_=driverGainNumerator/sample.y;}
    active_=sample.connected&&sample.x!=counterTimeout;
    return active_;
}

void PlayableJoystick::disable() noexcept {active_=false;}

JoystickPoll PlayableJoystick::poll(const JoystickCounterSample sample) const {
    validateButtons(sample.buttons);
    if(!active_||!sample.connected) {return {};}
    const JoystickAxes normalized{normalizeAxis(sample.x,centerX_,gainX_),normalizeAxis(sample.y,centerY_,gainY_)};
    return {true,normalized,directions(normalized),sample.buttons};
}

JoystickAxes PlayableJoystick::analog(const JoystickAxes normalized) const {
    if(!active_) {throw std::invalid_argument("joystick analog requires its enabled primary driver");}
    return {analogAxis(normalized.x,calibration_.center.x,calibration_.lowerRight.x),
        analogAxis(normalized.y,calibration_.center.y,calibration_.lowerRight.y)};
}

JoystickButtonAction PlayableJoystick::advanceButtons(const std::uint8_t buttons) {
    validateButtons(buttons);
    if(!active_) {return JoystickButtonAction::none;}
    if((buttons&primaryButton)!=0) {
        if((buttons&secondaryButton)==0) {return JoystickButtonAction::fire;}
        const auto action=(buttonHistory_&secondaryButton)==0?JoystickButtonAction::activateSelected:JoystickButtonAction::none;
        buttonHistory_=secondaryButton;
        return action;
    }
    if((buttons&secondaryButton)!=0) {
        if(buttonHistory_==0) {buttonHistory_=primaryButton;}
        return JoystickButtonAction::none;
    }
    const auto action=buttonHistory_==primaryButton?JoystickButtonAction::cycleSelected:JoystickButtonAction::none;
    buttonHistory_=0;
    return action;
}

void PlayableJoystick::beginCalibration() {
    if(!active_) {throw std::invalid_argument("joystick calibration requires its enabled primary driver");}
    point_=JoystickCalibrationPoint::upperLeft;
}

JoystickCalibrationPoint PlayableJoystick::captureCalibration(const JoystickCounterSample sample) {
    const auto sampled=poll(sample);
    if(!sampled.connected) {throw std::invalid_argument("joystick calibration requires a connected sample");}
    switch(point_) {
    case JoystickCalibrationPoint::upperLeft:
        calibration_.upperLeft=sampled.normalized;point_=JoystickCalibrationPoint::lowerRight;return point_;
    case JoystickCalibrationPoint::lowerRight:
        calibration_.lowerRight=sampled.normalized;point_=JoystickCalibrationPoint::center;return point_;
    case JoystickCalibrationPoint::center:
        if(sample.x==counterTimeout) {throw std::invalid_argument("joystick calibration center timed out");}
        calibration_.center=sampled.normalized;
        enable(sample); //8966 samples through OLD calibration before896E replaces the driver center/gain.
        calibration_.lowerRight={finishExtent(calibration_.upperLeft.x,calibration_.lowerRight.x),
            finishExtent(calibration_.upperLeft.y,calibration_.lowerRight.y)};
        point_=JoystickCalibrationPoint::complete;return point_;
    default:
        throw std::invalid_argument("joystick calibration has no pending capture point");
    }
}

} //namespace stellar7
