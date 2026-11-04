#pragma once
#include "stellar7/gameplay.hpp"
#include "stellar7/level1_transition_work.hpp"
#include "stellar7/level2_loader_pages_work.hpp"
#include "stellar7/weapon_lamp_work.hpp"

namespace stellar7 {
struct RecoveredLevel1SuccessTailWork {
    Level1ArrivalTailWork tail;
    std::optional<WeaponLampWork> lamp;
};
struct RecoveredLevel2PoolResetWork {
    Level2PoolResetWork reset;
    //Existing RadarState keys for source records actually cleared by48D4.
    std::vector<std::size_t> projectionResetKeys;
};
struct RecoveredLevel2PreloopWork {
    Level2GroundInitializationWork ground;
    StationaryLevel2PrefixInput firstPrefix;
    std::vector<std::size_t> allocationOrder;
    bool firstEntryDisplay = true;
};
} //namespace stellar7
