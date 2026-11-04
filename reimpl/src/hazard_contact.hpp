#pragma once

#include "stellar7/level1.hpp"

#include <functional>

namespace stellar7 {

//0784: clear health,481A retirement,active--,0E7C(50),then caller's backout.
inline bool retireContactHazard(Level1Entity& actor, std::int16_t& activeEnemies,
    const std::function<void()>& retire, const std::function<void(std::int16_t)>& damage) {
    constexpr std::int16_t firstHazardType = 14;
    constexpr std::int16_t lastHazardType = 16;
    constexpr std::int16_t hazardDamageUnits = 50;
    if (actor.type < firstHazardType || actor.type > lastHazardType) {
        return false;
    }

    actor.stateBytes4dTo52[1] = 0;
    retire();
    --activeEnemies;
    damage(hazardDamageUnits);
    return true;
}

} // namespace stellar7
