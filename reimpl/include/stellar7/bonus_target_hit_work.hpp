#pragma once

#include "stellar7/type42_bonus_work.hpp"

namespace stellar7 {

struct Level1TargetBonusContext {
    std::array<Level1BonusFixedSlot,level1BonusFixedPoolSize> fixedSlots;
    std::size_t nextEntityIndex;
    std::uint16_t firstBlockCapacity;
    const Model3d& model42;
};

struct Level1BonusTargetHitWork {
    Level1TargetHitWork hit;
    std::optional<Level1Type42BonusWork> bonus;
    //Operation index within hit.steps[0], at the whole90A4 entry.
    std::size_t bonusOperationOffset = 0;
};

//Explicit initialOrdinary1176 profile: whole90A4 may emit Type42 through its
//existing qualified child. Bonus append precedes FC append and481A target swap.
//All old1176 ownership/audio boundaries remain unchanged. No canonical mutation.
Level1BonusTargetHitWork level1BonusTargetHitOperations(const Level1TargetHitInput& input,
    const Level1TargetBonusContext& context);

} // namespace stellar7
