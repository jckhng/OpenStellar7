#pragma once

#include "stellar7/level1_projectile_allocation.hpp"

#include <cstddef>

namespace stellar7 {

enum class Level1CombatEffectProfile { ordinary, systemBonus };

//Actual live49F8 immediately before optional tail fire. Ordinary includes every
//live entity: FF actors and FE29/42. The bonus profile is explicit and trailing.
//Old45D2 is independent: removed targets/terminal FC still receive recenter work.
struct Level1CombatEffectMembership {
    std::size_t allocationCount;
    std::size_t ordinaryAllocations;
    std::size_t effectAllocations;
    Level1CombatEffectProfile profile = Level1CombatEffectProfile::ordinary;
    std::size_t bonusAllocations = 0; //Live FE42 only; old45D2 is independent.
    Level1ProjectileAllocationDomain projectileAllocationDomain = Level1ProjectileAllocationDomain::freshOnly;
};

} // namespace stellar7
