#pragma once

#include "stellar7/frame_tail_work.hpp"
#include "stellar7/level2_periodic_spawn_work.hpp"
#include "stellar7/level1_arrival_work.hpp"

#include <functional>

namespace stellar7 {

//Explicit off-scheduler admission; borrows the renderer's existing lifecycle receipt.
struct Level2OrdinaryTailReceipt {
    const Level2LifecycleTailWork* lifecycle = nullptr;
};

//Borrowed only through this synchronous renderer finish/model commit.
struct Level2PeriodicTailReceipt {
    Level2PeriodicSpawnWork work;
    const std::vector<Model3d>* ownedShared = nullptr;
    std::optional<Level1ArrivalWork> arrival;
    std::uint8_t cachedEnemyFlag = 0;
};

struct Level2PeriodicFrameTailWork {
    Level2LifecycleTailWork lifecycle;
    std::optional<Level2PeriodicSpawnWork> periodic;
    std::optional<std::size_t> periodicOperationOffset;
    std::optional<Level1ArrivalWork> arrival;
    std::optional<Level2PeriodicCacheReset> arrivalCacheReset;
};

//Planner receives recentered coordinates at033C, with returned-loop metadata.
//It projects canonical physical FF records and runs the whole0DDC child on copies.
using Level2PeriodicTailPlanner = std::function<Level2PeriodicSpawnWork(const FrameTailState&)>;
using Level2ArrivalTailPlanner = std::function<Level1ArrivalWork(const FrameTailState&,
    const Level2PeriodicSpawnWork&,std::uint8_t)>;

//Explicit periodic child domain; old zero-threshold wrappers remain unchanged.
//No first fade or armor-bound expansion. Arrival uses the cached radar flag.
Level2PeriodicFrameTailWork level2PeriodicFrameTailOperations(const FrameTailState& state,
    const FrameTailGuards& guards, const Level2PeriodicTailPlanner& plan,
    const std::optional<Level1TailFireAction>& fire = {},
    const std::vector<std::vector<NormalWorkOperation>>& readyLampOperations = {},
    const Level2ArrivalTailPlanner& arrival = {});

} // namespace stellar7
