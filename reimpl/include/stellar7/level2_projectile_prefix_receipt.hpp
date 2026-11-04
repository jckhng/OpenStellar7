#pragma once

#include "stellar7/level2_projectile_prefix_work.hpp"

namespace stellar7 {

//Transient validation inputs and completed source work; no persistent pool,
//visibility, timer or renderer owner. The model recomputes before adoption.
struct Level2ProjectilePrefixReceipt {
    Level1TargetPrefixWork work;
    BitmapFont scoreFont;
    CockpitPageState pages;
    std::optional<Level2HostileProjectileModel> hostile = std::nullopt;
    Level1PlayerCombatDomain combatDomain = Level1PlayerCombatDomain::survivingOnly;
    std::optional<Level2PeriodicOrdinaryProfile> periodic = std::nullopt;
};

} // namespace stellar7
