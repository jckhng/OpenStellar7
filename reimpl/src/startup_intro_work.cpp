#include "stellar7/startup_intro_work.hpp"

#include <stdexcept>

namespace stellar7 {

StartupIntroSkipWork startupIntroSkipOperations(const NativeIntroKey key, const StartupCapabilityProfile profile) {
    if (key != NativeIntroKey::escape || (profile != StartupCapabilityProfile::noMouseOrJoystick
        && profile != StartupCapabilityProfile::portablePointer)) {
        throw std::invalid_argument("native intro skip requires Escape and absent mouse/joystick");
    }
    StartupIntroSkipWork result{};
    result.sourceKey = 0x011b;
    result.capability=profile;
    if(profile==StartupCapabilityProfile::portablePointer) {
        //Whole5F4C is an explicit native Escape/capability provider on this path.
        //No absent-mouse queries or guest CPU duration are certified here.
        return result;
    }
    const auto plain = [&](const std::uint64_t count) {
        result.query.push_back({NormalWorkKind::instructions, count});
    };
    plain(10); //5F4C..5F5C, including CALL03EE; whole host-key provider is untimed.
    plain(4); //5F61..5F69: Escape AH1 sets SI1.
    plain(11); //5F6E..5F83: no settings-toggle key.
    plain(2); //5FAA/5FAC: DI0 skips8B34.
    plain(3); //5FB3..5FB6 CALL2685(0).
    plain(10); //2685..26B0, absent joystick flag; no port201 read.
    plain(6); //5FBB..5FC6 CALL2685(1), even after Escape.
    plain(10);
    plain(5); //5FCB..5FD3 CALL4FEF(0).
    plain(10); //4FEF..5011, absent mouse flag.
    plain(6); //5FD8..5FE3 CALL4FEF(1).
    plain(10);
    plain(3); //5FE8..5FED, no mouse button.
    plain(6); //5FF2..5FF9 returns SI1.
    return result;
}

} // namespace stellar7
