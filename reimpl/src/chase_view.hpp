#pragma once
#include "stellar7/gameplay.hpp"
#include "world_presentation.hpp"

namespace stellar7 {
// Presentation-only camera and HUD; neither owns nor advances gameplay state.
ViewTransform chaseCamera(const Level1Entity& player,const std::vector<Level1Entity>& actors,ChaseOptions options={});
double enhancedSurfaceLighting(const std::vector<FixedPosition>& polygon,const FixedPosition& light);
std::optional<PresentationFace> vehicleShadow(const Level1Entity& actor,const ViewTransform& view,
    const std::vector<Rgb>& palette);
void drawChaseHud(std::vector<std::uint8_t>& pixels,const GameplayAssets& assets,
    const GameplayModel& model,const ViewTransform& view,const std::vector<Rgb>& palette);
// Attach a new immutable overlay at an existing source publication boundary.
void refreshChaseHud(IndexedScreen& frame,const GameplayAssets& assets,const GameplayModel& model);
}
