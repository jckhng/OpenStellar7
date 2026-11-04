#pragma once
#include "stellar7/simulation_prefix_work.hpp"
namespace stellar7 {
struct Level2SolidContactWork {
    Level1SolidContactWork contact;
    //At this operation boundary execute08C2/08C5/08C6, whole0E7C(raw-25),08C9.
    //These four caller dispatches and0E7C are excluded from contact.operations.
    std::optional<std::size_t> damageChildOffset;
};
//Explicit owned shared-model radius; source1/4/8/13 FF or30 FE. Same backout,
//rebound/0074 and event9 body; no guessed iteration bound or new state owner.
Level2SolidContactWork level2SolidContactOperations(const Level1Entity& player,
    const Level1Entity& target,std::size_t targetIndex,std::uint32_t ownedRadius);
struct Level2Type4DamageWork {
    std::int16_t armor;
    std::uint16_t fadeTicks;
    std::vector<NormalWorkOperation> operations;
    Level1PrefixPaletteStep palette;
};
//Whole0E7C for raw-25, unshielded/uncompleted retained Level2 domain. Caller is
//excluded. Event6 owns0E7F..0E86; actual resident palette child remains explicit.
//Keeps the existing896/HUD admission; rejects a result outside that domain.
Level2Type4DamageWork level2Type4DamageOperations(std::int16_t armor,
    Level1PlayerCombatDomain domain);
}
