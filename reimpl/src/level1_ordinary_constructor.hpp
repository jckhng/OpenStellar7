#pragma once
#include "stellar7/level1_periodic_spawn_work.hpp"
namespace stellar7::detail {
enum class OrdinaryConstructorDomain { level1, level2PostWave, level2GuardianEmission };
struct OrdinaryConstructorProfile {
    bool guardian = false;
    const std::vector<Level1Entity>* retainedEntities = nullptr;
    std::optional<std::size_t> releaseOperationOffset;
    OrdinaryConstructorDomain domain = OrdinaryConstructorDomain::level1;
};
//Internal4370 entry..RET; caller argument/CALL/cleanup excluded. Inputs already
//validated by the explicit periodic/arrival owner; selector release requires the explicit profile.
Level1PeriodicSpawnWork ordinaryConstructorOperations(const Level1PeriodicSpawnInput& input,
    const StellarRng& rng,const std::vector<Model3d>& models,std::uint8_t type, OrdinaryConstructorProfile* profile = nullptr);
}
