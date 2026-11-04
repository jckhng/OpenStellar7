#pragma once

#include "stellar7/render_work.hpp"

namespace stellar7 {

struct WorldModelCallerWork {
    std::int32_t minimumDepth;
    std::uint16_t projectedRadius;
    std::vector<NormalWorkOperation> operations;
};

// ba88 -> cb7a, ordinary flags0/callback0, shift7/near128; model body excluded.
WorldModelCallerWork worldModelCallerOperations(FixedPosition center,
    std::uint32_t collisionRadius, std::uint8_t flags, std::uint16_t callback);
WorldModelCallerWork worldModelCallerOperations(FixedPosition center,
    std::uint32_t collisionRadius, std::uint8_t flags, std::uint16_t callback, WorldSourceFlagsDomain domain);

enum class WorldActorContinuation { nextActor, returnToCaller };

// bbee -> b9d0 or caller850c. Excludes model RETF; callback0/global suffix0.
std::vector<NormalWorkOperation> worldModelReturnOperations(WorldActorContinuation continuation);

// b9d0 -> ba88 for subsequent ordinary actors. First actor includes list setup.
WorldFirstCenterWork worldNextCenterOperations(const ViewTransform& view, const WorldActorWorkInput& actor);
WorldFirstCenterWork worldNextCenterOperations(const ViewTransform& view, const WorldActorWorkInput& actor,
    WorldSourceFlagsDomain domain);

}
