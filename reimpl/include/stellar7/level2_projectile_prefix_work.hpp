#pragma once

#include "stellar7/simulation_prefix_work.hpp"

#include <optional>

namespace stellar7 {

struct Level2HostileProjectileModel {
    static constexpr std::int16_t type = 19;
    static constexpr std::uint16_t sharedCount = 44;
    static constexpr std::uint16_t radius = 441;
    static constexpr std::uint8_t lifetime = 84;
    static constexpr std::uint8_t cooldown = 12;
    static constexpr std::int16_t armorDamage = 80;

    std::uint16_t firstRegistryCapacity;
    Level1EnemyProjectileModel model19;
};

//0DDC can append only while active<=2; the initial constructor has two Type13.
inline constexpr std::size_t maximumLevel2PeriodicLiveType13 = 3;

inline constexpr std::int16_t level2MaximumFrameDamage =
    Level1State::projectilePoolSize*Level2HostileProjectileModel::armorDamage+2*128;

//Transient decoded shared metadata plus compact FF/fixed projection keys.
//No physical storage or owner identity is duplicated persistently.
struct Level2PeriodicOrdinaryProfile {
    std::uint16_t firstRegistryCapacity;
    Level1EnemyProjectileModel model1;
    Level1EnemyProjectileModel model4;
    Level1EnemyProjectileModel model17;
    Level1EnemyProjectileModel model20;
    std::vector<std::size_t> physicalKeys;
};

struct Level2ProjectilePrefixProfile {
    std::uint16_t loopTick;
    std::int16_t armor;
    std::uint16_t difficulty;
    //Absent retains the existing Type23-only launch/update admission.
    std::optional<Level2HostileProjectileModel> hostile = std::nullopt;
    Level1PlayerCombatDomain combatDomain = Level1PlayerCombatDomain::survivingOnly;
    std::optional<Level2PeriodicOrdinaryProfile> periodic = std::nullopt;
};

//Source01D0 through BEFORE02A3. Seven retained constructor records, actual
//49F8/OLD identities, player Type23 pool and FC effects. An explicit decoded
//hostile descriptor additionally admits Type8->19 birth, motion, owner-aware
//contact, damage80, retirement and FD reuse. Bonus creation/abilities and
//expiryAndLoss explicitly admits incoming armor0..680 and all remaining OLD
//work after lethal hits; worst result is-896. Default armor>=200 stays strict.
//Unsupported kinematics remain excluded. Terminal constructor
//records remain physical storage; only the live allocation list loses them.
//Shared typed structs describe source records, not Level1 membership admission.
Level1TargetPrefixWork level2ProjectilePrefixOperations(const Level1TargetPrefixInput& input,
    const Level2ProjectilePrefixProfile& profile, const BitmapFont& scoreFont,
    const CockpitPageState& pages);

} // namespace stellar7
