#pragma once
#include "spatial_scene.hpp"
#include "spatial_options.hpp"
#include "enhanced_view.hpp"
#include "stellar7/destruction_presentation.hpp"

namespace stellar7 {
inline constexpr std::int32_t spatialPilotEyeHeight=600;
struct SpatialCockpitLayout {
    float cabinScale=1;
    std::int32_t eyeHeight=spatialPilotEyeHeight;
};
ViewTransform spatialPilotCamera(const Level1Entity& player);
SpatialPoint spatialFromWorld(SpatialPoint point,const ViewTransform& camera);
SpatialPoint spatialToWorld(SpatialPoint point,const ViewTransform& camera);
std::vector<SpatialLight> spatialExplosionLights(const std::vector<DestructionCue>& cues,std::uint64_t tick,
    std::size_t level,const ViewTransform& camera);
SpatialScene makeSpatialView(const SpatialScene& source,const IndexedScreen& indexed,EnhancedView view,const SpatialOptions& options,
    const SpatialCockpitLayout& layout={});
}
