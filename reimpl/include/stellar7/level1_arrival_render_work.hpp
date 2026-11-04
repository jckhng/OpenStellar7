#pragma once
#include "stellar7/world_model_work.hpp"
namespace stellar7 {
WorldModelCullWork level1ArrivalModelCullOperations(FixedPosition center);
WorldModelSetupWork level1ArrivalModelSetupOperations(FixedPosition center,
    std::array<std::uint16_t,3> angles,std::uint16_t projectedRadius,const Model3d& model,
    std::uint16_t registryScale,std::uint16_t registryCap,const WorldModelViewInput& view);
WorldModelGeometryWork level1ArrivalModelGeometryOperations(const ModelLod& lod,
    const WorldModelGeometryInput& input,std::array<std::uint16_t,3> angles,const WorldModelViewInput& view);
}
