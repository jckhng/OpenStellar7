#pragma once

#include "stellar7/bonus_target_hit_work.hpp"
#include "stellar7/simulation_prefix_work.hpp"

namespace stellar7 {

struct Level1BonusResetProjection {
    std::size_t entityIndex;
    std::size_t fixedPoolSlot;
    std::uint16_t sourceResetEntry = 0x49f4;
    std::uint16_t firstResetSlotCount = 8;
};

struct Level1PrefixBonusBirth {
    std::size_t operationOffset;
    Level1Type42BonusReceipt receipt;
    //Publish the earlier loader49F4 zero-cache observation once. This is not
    //new source work at4506 and never appears for a retained reused fixed slot.
    std::optional<Level1BonusResetProjection> resetProjection;
};

struct Level1BonusTargetPrefixWork {
    Level1TargetPrefixWork prefix;
    std::vector<Level1PrefixBonusBirth> bonusBirths;
};

//Whole0982 FE42 exception through097E JMP, excluding0A6D loop control.
//Includes caller9 dispatches around complete3B3A, with updated-angle branching.
KinematicsWork level1BonusFixedUpdateOperations(const Level1Entity& actor);

//Explicit source bonus profile. Derives fixed45B0 order from stable FE entity
//storage; unmaterialized slots0..7 have loader reset ancestry,8..15 are unknown.
//At most one FE42, four fixed29 and ten ordinary storage records (15 total).
//Retains old45D2 terminal/update order and executes FE42 spin at0991..097E;
//contacts/pickup/player/other bonus families remain rejected. Result entities,
//registry and work already include each birth; observations are never replayed.
Level1BonusTargetPrefixWork level1BonusTargetPrefixOperations(const Level1TargetPrefixInput& input,
    const Level1TargetPrefixProfile& profile, const BitmapFont& scoreFont,
    const CockpitPageState& pages, std::uint16_t firstBlockCapacity, const Model3d& model42);

} // namespace stellar7
