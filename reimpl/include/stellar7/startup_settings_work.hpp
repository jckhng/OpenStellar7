#pragma once

#include "stellar7/cockpit_state.hpp"
#include "stellar7/highscore.hpp"
#include "stellar7/startup_context_work.hpp"

namespace stellar7 {

enum class StartupCapabilityProfile { noMouseOrJoystick, portablePointer };
enum class StartupSettingsEffectKind { mouseActive, mouseOption, joystickActive,
    viewAccumulator, viewWidth, viewScale, joystickOption };
enum class StartupSettingsHostServiceKind { releaseStorage, loadSettings, mouseAbsentQuery, portablePointerQuery, portablePointerPosition, portableJoystickQuery };

struct StartupSettingsEffect {
    StartupSettingsEffectKind kind;
    std::int16_t value;
    std::uint32_t sourcePc;
    std::uint64_t afterDispatch;
};

struct StartupSettingsHostService {
    StartupSettingsHostServiceKind kind;
    std::uint64_t afterDispatch;
};

enum class StartupJoystickPresence { absent, present };

struct StartupSettingsWork {
    CockpitSettings configuration;
    HighscoreTable loadedTable;
    StartupViewDescriptor viewDescriptor;
    std::int16_t viewAccumulator;
    std::vector<NormalWorkOperation> operations;
    std::vector<StartupSettingsEffect> effects;
    std::vector<StartupSettingsHostService> hostServices;
    StartupCapabilityProfile capability = StartupCapabilityProfile::noMouseOrJoystick;
    std::optional<StartupJoystickPresence> nativeJoystick;
};

// Source-owned41BB..4059 work around untimed native storage/capability services.
// Rejects calibration and joystick/PIT programming before emitting work.
StartupSettingsWork startupSettingsOperations(const StartupContextState& context,
    const CockpitSettings& configuration, const HighscoreTable& table,
    StartupCapabilityProfile profile);

StartupSettingsWork playableJoystickSettingsOperations(const StartupContextState& context,
    const CockpitSettings& configuration,const HighscoreTable& table,StartupCapabilityProfile profile,
    StartupJoystickPresence joystick);

}  // namespace stellar7
