#pragma once
#include "stellar7/level1_deployment_render_work.hpp"
namespace stellar7 {
//Source C877 sorting translation; full-coordinate55EB vertices remain independent.
WorldModelSetupWork level1DepartureModelSetupOperations(FixedPosition center,
    std::array<std::uint16_t,3> angles,std::uint16_t projectedRadius,const Model3d& model,
    std::uint16_t registryScale,std::uint16_t registryCap,const WorldModelViewInput& view);
WorldModelGeometryWork level1DepartureModelGeometryOperations(const ModelLod& lod,
    const WorldModelGeometryInput& input,std::array<std::uint16_t,3> angles,
    const WorldModelViewInput& view);
} // namespace stellar7
