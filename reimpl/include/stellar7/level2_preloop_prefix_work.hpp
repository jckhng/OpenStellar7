#pragma once

#include "stellar7/simulation_prefix_work.hpp"

namespace stellar7 {

struct Level2PreloopPrefixInput {
    StationaryLevel2PrefixInput prefix;
    std::vector<std::size_t> allocationOrder;
};

struct Level2PreloopVisibilityReceipt {
    FixedPosition incomingLastListCamera;
    // Half-open operation range of3768, including its late camera/list writes.
    std::size_t operationBegin;
    std::size_t operationEnd;
};

struct Level2PreloopPrefixWork {
    Level1Entity player;
    std::vector<Level1Entity> actors;
    std::vector<std::size_t> activeOrder;
    FixedPosition lastListCamera;
    bool refreshRequested;
    std::vector<NormalWorkOperation> operations;
    Level2PreloopVisibilityReceipt visibility;
};

// Controlled original calls with the captured Type31 player and empty OLD list.
// Final activeOrder/lastListCamera/refreshRequested are the3768 receipt state.
Level2PreloopPrefixWork level2PreloopPrefixOperations(const Level2PreloopPrefixInput& input);

// Actual48D4 return:49F4 cleared player Type to0; no later player state is imported.
Level2PreloopPrefixWork level2ResetPreloopPrefixOperations(const Level2PreloopPrefixInput& input);

} // namespace stellar7
