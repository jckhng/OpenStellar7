#pragma once
#include "stellar7/failure_model_work.hpp"
namespace stellar7 {
constexpr std::int16_t retainedFailureFixedType=29;
constexpr std::uint32_t retainedFailureFixedRadius=862;
FailureSceneWork retainedFailureSceneOperations(const GameplayAssets&,const ViewTransform&,
    const std::vector<FailureSceneItem>&,WorldSceneProfile,std::array<std::int16_t,3>,
    std::uint16_t,const std::vector<std::uint16_t>&);
WorldModelCullWork failureFixedWideCullOperations(FixedPosition,const Model3d&);
WorldModelSetupWork failureFixedWideSetupOperations(FixedPosition,std::array<std::uint16_t,3>,
    std::uint16_t,const Model3d&,std::uint16_t,std::uint16_t,const WorldModelViewInput&);
WorldModelGeometryWork failureFixedWideGeometryOperations(const ModelLod&,
    const WorldModelGeometryInput&,const WorldModelViewInput&);
} //namespace stellar7

namespace stellar7 {
FailureSceneWork retainedLevel2FailureSceneOperations(const GameplayAssets&,const ViewTransform&,
    const std::vector<FailureSceneItem>&,WorldSceneProfile,std::array<std::int16_t,3>,
    std::uint16_t,const std::vector<std::uint16_t>&);
WorldModelCullWork level2FailureModelCullOperations(FixedPosition,const Model3d&);
WorldModelSetupWork level2FailureModelSetupOperations(FixedPosition,std::array<std::uint16_t,3>,
    std::uint16_t,std::int16_t,const Model3d&,std::uint16_t,std::uint16_t,const WorldModelViewInput&);
WorldModelGeometryWork level2FailureModelGeometryOperations(const ModelLod&,
    const WorldModelGeometryInput&,std::array<std::uint16_t,3>,const WorldModelViewInput&,std::int16_t);
} // namespace stellar7
