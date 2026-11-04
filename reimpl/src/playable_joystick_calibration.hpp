#pragma once

#include "playable_joystick.hpp"
#include "stellar7/indexed_pages.hpp"

namespace stellar7 {
enum class JoystickCalibrationActivation {released,held};
enum class JoystickCalibrationWait {pending,returned};
enum class JoystickCalibrationPhase {waitingPress,waitingRelease,waitingDebounce,complete,unavailable,rejected};

struct JoystickCalibrationStep {
    JoystickCalibrationPhase phase;
    JoystickCalibrationPoint point;
    bool promptChanged=false;
    std::uint16_t waitRawIrqs=0;
};

//88F8/88C4. Borrows the retained controller; caller owns pages, PIT and audio.
class PlayableJoystickCalibration {
public:
    explicit PlayableJoystickCalibration(PlayableJoystick& joystick);
    JoystickCalibrationPhase phase() const noexcept {return phase_;}
    JoystickCalibrationPoint point() const noexcept;
    JoystickCalibrationStep advance(const JoystickCounterSample& sample,
        JoystickCalibrationActivation activation,JoystickCalibrationWait wait=JoystickCalibrationWait::pending);
private:
    PlayableJoystick* joystick_;
    PlayableJoystick original_;
    JoystickCalibrationPhase phase_=JoystickCalibrationPhase::waitingPress;
};

//89E0 reuses source52AA/STELART12 and primary12CA/secondary12CC fonts.
std::vector<IndexedPageEffect> joystickCalibrationPageEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& primary,const BitmapFont& secondary,JoystickCalibrationPoint point,std::uint16_t page);
std::vector<IndexedPageEffect> joystickDisabledPageEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& primary,std::uint16_t page);
//Explicit native validation notice; these two lines are not original DS text.
std::vector<IndexedPageEffect> joystickCalibrationRejectedPageEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& primary,const BitmapFont& secondary,std::uint16_t page);
//Native recoverable storage policy, separate from source calibration prompts.
std::vector<IndexedPageEffect> joystickCalibrationSaveFailedPageEffects(const std::vector<IndexedBitmap>& art,
    const BitmapFont& primary,const BitmapFont& secondary,std::uint16_t page);
} //namespace stellar7
