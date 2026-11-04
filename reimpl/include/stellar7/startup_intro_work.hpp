#pragma once

#include "stellar7/startup_settings_work.hpp"

namespace stellar7 {

enum class NativeIntroKey { none, escape };

struct StartupIntroSkipWork {
    std::vector<NormalWorkOperation> query;
    std::uint16_t sourceKey = 0;
    StartupCapabilityProfile capability = StartupCapabilityProfile::noMouseOrJoystick;
};

//5F4C(0), with whole03EE supplied by the native host-key provider. The four
// actual2685/4FEF absent-device queries are included; caller/blackout excluded.
StartupIntroSkipWork startupIntroSkipOperations(NativeIntroKey key, StartupCapabilityProfile profile);

} // namespace stellar7
