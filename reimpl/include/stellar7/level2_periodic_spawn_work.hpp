#pragma once

#include "stellar7/level1_object_ref.hpp"
#include "stellar7/level1_periodic_spawn_work.hpp"

namespace stellar7 {

//Call-time projection of the existing source4C16 physical ordinary pool.
struct Level2PeriodicSpawnSlot {
    Level1Entity actor{};
    std::optional<std::size_t> entityIndex;
};

struct Level2PeriodicSpawnInput {
    std::int16_t activeEnemies = 0;
    std::int16_t spawnedEnemies = 0;
    std::uint8_t difficulty = 0;
    std::vector<Level1ObjectRef> allocationOrder;
    std::size_t nextEntityIndex = 0;
    std::array<Level2PeriodicSpawnSlot, 16> ordinaryPool{};
};

struct Level2PeriodicCacheReset {
    //Boundary after whole49F4, before43DF. Caller resets this existing radar key.
    std::size_t operationOffset;
    std::uint16_t sourceResume = 0x43df;
    std::size_t physicalKey;
};

struct Level2PeriodicSpawnReceipt {
    std::uint8_t physicalSlot;
    std::size_t logicalIndex;
    Level1Entity actor;
    std::size_t appendedAllocationIndex;
    std::int16_t activeEnemies;
    std::int16_t spawnedEnemies;
    bool refreshRequested;
    Level2PeriodicCacheReset cacheReset;
};

struct Level2PeriodicSpawnWork {
    StellarRng rng;
    Level1PeriodicSpawnOutcome outcome;
    //0DDC table selection precedes4370's incoming-spawned7..9 override to4.
    std::optional<std::uint8_t> selectedType;
    std::optional<std::uint8_t> effectiveType;
    std::optional<Level2PeriodicSpawnReceipt> spawn;
    std::vector<Level1ObjectRef> allocationOrder;
    std::vector<NormalWorkOperation> operations;
};

//Whole0DDC..0E4F, excluding caller033C CALL. Explicit Level2/difficulty0;
//source tables select1/4/8/13. Shared models are the owned decoded44-entry bank.
//No13AC/137E provider: unsupported selector ancestry is rejected, not omitted.
//Adopt RNG even on no birth. Reuse mapped logical identity, append only unmapped
//physical slots; preserve OLD aliases. Model owns cache reset/incarnation commit.
Level2PeriodicSpawnWork level2PeriodicSpawnOperations(const Level2PeriodicSpawnInput& input,
    const StellarRng& rng, const std::vector<Model3d>& ownedShared);

} // namespace stellar7
