#pragma once

#include "stellar7/gameplay.hpp"
#include "stellar7/playable_system_effects.hpp"

#include <array>
#include <vector>

namespace stellar7 {

enum class PlayableSystemDomain { jumpOnly, timedBundle, decoyBundle, fullBundle };
enum class PlayableBombRefresh { none, deploy };
enum class PlayableProxyRefresh { none, copyPlayerRecord };
enum class PlayableScannerRefresh { none, active, inactive };
enum class PlayableShield { inactive, active };
enum class PlayablePropulsion { ordinary, turbo };

//A local snapshot of canonical Level1State storage and retained player Z.
//Do not persist another system array or alias jump velocity to actor.zVelocity.
struct PlayableJumpState {
    std::array<std::int8_t,7> statuses;
    std::array<std::int16_t,7> timers;
    std::int16_t selectedSystem;
    std::int32_t playerZ;
};

struct PlayableJumpResult {
    PlayableJumpState state;
    std::vector<PlayableSystemEffect> effects;
    PlayableScannerRefresh scannerRefresh = PlayableScannerRefresh::none;
    PlayableProxyRefresh proxyRefresh = PlayableProxyRefresh::none;
    PlayableBombRefresh bombRefresh = PlayableBombRefresh::none;
};

void validatePlayableJumpState(const PlayableJumpState& state,
    PlayableSystemDomain domain = PlayableSystemDomain::jumpOnly);
PlayableJumpResult advancePlayableJump(const PlayableJumpState& state,
    PlayableSystemDomain domain = PlayableSystemDomain::jumpOnly);
PlayableJumpResult applyPlayableJumpAction(const PlayableJumpState& state,GameplaySystemAction action,
    PlayableSystemDomain domain = PlayableSystemDomain::jumpOnly);
PlayableJumpResult normalizePlayableSystemsAfterReset(const PlayableJumpState& state,
    PlayableSystemDomain domain = PlayableSystemDomain::jumpOnly);

struct PlayablePrimaryProfile {
    std::uint8_t selector;
    std::uint8_t lifetime;
    std::uint8_t cooldown;
    std::uint16_t event;
    bool requiresCannonSlot;
};
PlayablePrimaryProfile playablePrimaryProfile(std::int16_t superTimer);
std::int16_t playableArmorAfterDamage(std::int16_t armor,std::int16_t units,PlayableShield shield);
std::int16_t advancePlayableForwardVelocity(std::int16_t incoming,const GameplayInput& input,
    PlayablePropulsion propulsion);

} //namespace stellar7
