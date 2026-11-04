#pragma once
#include "stellar7/world_model_work.hpp"

namespace stellar7 {
//Explicit shared model42: source altitude100/collision330, no callback.
WorldModelCullWork level1BonusModelCullOperations(FixedPosition center);
WorldModelSetupWork level1BonusModelSetupOperations(FixedPosition center,
    std::array<std::uint16_t,3> actorAngles, std::uint16_t projectedRadius,
    const Model3d& model, std::uint16_t registryScale, std::uint16_t registryCap,
    const WorldModelViewInput& view);
WorldModelGeometryWork level1BonusModelGeometryOperations(const ModelLod& lod,
    const WorldModelGeometryInput& input, std::array<std::uint16_t,3> actorAngles,
    const WorldModelViewInput& view);
} // namespace stellar7
