#pragma once

#include <cstdint>

namespace stellar7 {

//Native host coordinates, independent of DOS port201 counter samples.
//Buttons use bit0=primary and bit1=secondary; other bits are unsupported.
enum class NativeJoystickPhase { sample, cancel };

struct NativeJoystickSample {
    bool connected=false;
    std::uint16_t x=0;
    std::uint16_t y=0;
    std::uint8_t buttons=0;
    NativeJoystickPhase phase=NativeJoystickPhase::sample;
};

enum class JoystickCalibrationPoint { idle, upperLeft, lowerRight, center, complete };

struct JoystickAxes {
    std::int16_t x=0;
    std::int16_t y=0;
};

} //namespace stellar7
