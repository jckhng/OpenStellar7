#pragma once
#include "stellar7/level1_periodic_spawn_work.hpp"
#include <variant>

namespace stellar7 {
struct Level1ArrivalInput {
    std::uint8_t enemyFlag;
    std::optional<std::size_t> craftEntityIndex;
    std::optional<std::size_t> guardianEntityIndex;
    Level1Entity player;
    Level1PeriodicSpawnInput pool;
};
enum class Level1ArrivalOutcome { enemies, craftPresent, guardianPresent, unfinished, born };
struct Level1ArrivalAudioChild {
    std::uint16_t eventId = 17;
    std::uint16_t callerBegin = 0x4705;
    std::uint16_t callerEnd = 0x470f;
    std::uint16_t callerDispatches = 4;
};
using Level1ArrivalStep = std::variant<std::vector<NormalWorkOperation>,Level1ArrivalAudioChild,Level1SelectorReleaseCall>;
struct Level1ArrivalWork {
    StellarRng rng;
    Level1ArrivalOutcome outcome;
    std::optional<Level1PeriodicSpawnReceipt> birth;
    std::vector<Level1ArrivalStep> steps;
};
//46C8 entry..4778 RET, excluding033F CALL. Audio child owns4705..470C;
//before/after vectors exclude those four caller dispatches and all audio work.
//First free slot must have no old selector owner; release/pool exhaustion reject.
//4370 reset observation belongs to birth.poolSlot/actor identity exactly once.
Level1ArrivalWork level1ArrivalOperations(const Level1ArrivalInput& input,
    const StellarRng& rng,const std::vector<Model3d>& models);
//Same46C8/4370 path with canonical retired11 selector release before reset.
Level1ArrivalWork level1RetainedArrivalOperations(const Level1ArrivalInput& input,
    const StellarRng& rng,const std::vector<Model3d>& models);
//Explicit Level2 first46C8 component: limit12, no prior selector owner.
//Caller supplies cached radar132E and post0DDC physical pool/counters.
//Birth requires total12; existing-identity exits admit totals through14.
//The same receipt owns shared7, two RNG words and one timed event17 boundary.
Level1ArrivalWork level2ArrivalOperations(const Level1ArrivalInput& input,
    const StellarRng& rng,const std::vector<Model3d>& models);
//Explicit post-wave0DDC domain: spawned10..12 only; limit return consumes no RNG.
//Default periodic API and its <=10 validation remain unchanged.
Level1PeriodicSpawnWork level1PostWavePeriodicOperations(const Level1PeriodicSpawnInput& input,
    const StellarRng& rng,const std::vector<Model3d>& models);
} // namespace stellar7
