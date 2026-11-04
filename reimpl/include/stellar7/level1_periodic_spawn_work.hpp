#pragma once

#include "stellar7/level1.hpp"
#include "stellar7/normal_clock.hpp"
#include "stellar7/resource_codec.hpp"

#include <optional>

namespace stellar7 {

//Projection of the existing ordinary pool in source4C16 order. No second owner.
struct Level1OrdinarySpawnSlot {
    std::uint8_t health = 0;
    bool hasAnimation = false;
    std::optional<std::size_t> entityIndex;
};

enum class Level1SelectorReuseDomain { rejectAnimated, retainedType11 };
struct Level1SelectorReleaseCall {
    std::size_t entityIndex;
    std::uint16_t sourceEntry = 0xdcca;
    bool guestElapsedQualified = false;
};
struct Level1SelectorReleaseBoundary {
    std::size_t operationOffset;
    Level1SelectorReleaseCall call;
};

//Only the existing retired FF11 heap selector is admitted by the new domain.
bool level1RetiredType11Selector(const Level1Entity& actor) noexcept;
struct Level1PeriodicSpawnInput {
    std::int16_t activeEnemies;
    std::int16_t spawnedEnemies;
    std::uint8_t enemyLimit;
    std::uint16_t allocationCount;
    std::size_t nextEntityIndex;
    std::array<Level1OrdinarySpawnSlot, 16> ordinaryPool;
    Level1SelectorReuseDomain selectorReuse = Level1SelectorReuseDomain::rejectAnimated;
    //Borrowed only while planning; no returned receipt retains this pointer.
    const std::vector<Level1Entity>* retainedEntities = nullptr;
};

enum class Level1PeriodicSpawnOutcome { limit, threshold, highZero, noType, poolFull, spawned };

struct Level1PeriodicSpawnReceipt {
    std::uint8_t poolSlot;
    Level1Entity actor;
    std::uint16_t appendedAllocationIndex;
    std::int16_t activeEnemies;
    std::int16_t spawnedEnemies;
    bool refreshRequested;
};

struct Level1SpawnProviderCall {
    std::size_t entityIndex;
    std::uint16_t requestedBytes;
    std::uint16_t sourceEntry;
    bool guestElapsedQualified;
};

struct Level1PeriodicSpawnWork {
    StellarRng rng;
    Level1PeriodicSpawnOutcome outcome;
    std::optional<std::uint8_t> selectedType;
    std::optional<Level1PeriodicSpawnReceipt> spawn;
    std::vector<NormalWorkOperation> beforeProvider;
    std::optional<Level1SpawnProviderCall> provider;
    std::vector<NormalWorkOperation> afterProvider;
    //DCCA is excluded at this offset in beforeProvider. DCC6(FFFF) stays source work.
    std::optional<Level1SelectorReleaseBoundary> selectorRelease;
};

//Complete0DDC entry through RET, excluding caller033C CALL. Level1 tables and
//ordinary constructors1/3/11 only; first free slot must have no prior animation.
//Type11's3956 wraps nativeDCC6(1): beforeProvider, provider, afterProvider order.
//Provider returns non-null zeroed native selector storage or terminates; omitted
//CRT guest time is explicitly unqualified. Existing entity selector fields own it.
//No allocation/model mutation. Commit returned actor/list/counters/RNG once.
Level1PeriodicSpawnWork level1PeriodicSpawnOperations(const Level1PeriodicSpawnInput& input,
    const StellarRng& rng, const std::vector<Model3d>& sharedModels);

//Same complete0DDC/4370 body; explicit canonical retired11 release provider.
//The old symbol rejects selectorReuse/borrowed metadata and remains strict.
Level1PeriodicSpawnWork level1RetainedPeriodicSpawnOperations(const Level1PeriodicSpawnInput& input,
    const StellarRng& rng, const std::vector<Model3d>& sharedModels);
void validateLevel1SelectorProjection(const Level1PeriodicSpawnInput& input);

} // namespace stellar7
