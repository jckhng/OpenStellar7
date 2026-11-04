#pragma once
#include "stellar7/world_model_work.hpp"
#include "stellar7/world_traversal.hpp"
namespace stellar7 {
//Source-owned shared descriptor, precision0, pitch0, main camera modes0/1.
//No actor identity, allocation, callback, or animation mutation is owned here.
WorldModelSetupWork level1OwnedModelSetupOperations(FixedPosition center,
    std::array<std::uint16_t,3> angles,std::uint16_t projectedRadius,
    const Model3d& model,std::uint16_t registryScale,std::uint16_t registryCap,
    const WorldModelViewInput& view);
WorldModelGeometryWork level1OwnedModelGeometryOperations(const ModelLod& lod,
    const WorldModelGeometryInput& input,std::array<std::uint16_t,3> angles,
    const WorldModelViewInput& view);
}

namespace stellar7 {
//Source49F6 precision1, flat camera, shared descriptor and unchanged full center.
//The precision0 and generic entry points retain their narrower contracts.
WorldModelCullWork level1OwnedWideModelCullOperations(FixedPosition center,const Model3d& model);
WorldModelSetupWork level1OwnedWideModelSetupOperations(FixedPosition center,
    std::array<std::uint16_t,3> angles,std::uint16_t projectedRadius,
    const Model3d& model,std::uint16_t registryScale,std::uint16_t registryCap,
    const WorldModelViewInput& view);
WorldModelGeometryWork level1OwnedWideModelGeometryOperations(const ModelLod& lod,
    const WorldModelGeometryInput& input,std::array<std::uint16_t,3> angles,
    const WorldModelViewInput& view);
}

namespace stellar7 {
//BA88 through CB7A CALLF; ordinary flags0/callback0, unsigned low-word divisor.
//The enclosing scene retains the existing deferred callback gate separately.
WorldModelCallerWork level1OwnedWideModelCallerOperations(FixedPosition center,const Model3d& model,
    std::uint8_t flags,std::uint16_t callback);
}
