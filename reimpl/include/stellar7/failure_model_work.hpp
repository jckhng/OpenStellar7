#pragma once

#include "stellar7/failure_view_work.hpp"
#include "stellar7/world_model_work.hpp"
#include "stellar7/world_scene_work.hpp"
#include "stellar7/failure_effect_work.hpp"

namespace stellar7 {

// Explicit failure family; compatibility scene/model/raster entries keep their guards.
WorldModelCullWork failureModelCullOperations(FixedPosition center,
    std::uint32_t collisionRadius, Viewport viewport);

WorldModelSetupWork failureModelSetupOperations(FixedPosition center,
    std::array<std::uint16_t, 3> actorAngles, std::uint16_t projectedRadius,
    std::int16_t type, const Model3d& model, std::uint16_t registryScale,
    std::uint16_t registryCap, const WorldModelViewInput& view);

WorldModelGeometryWork failureModelGeometryOperations(const ModelLod& lod,
    const WorldModelGeometryInput& input,
    const std::optional<FixedPosition>& untransformedActorPosition = std::nullopt);

struct FailurePolygonWork {
    std::vector<NormalWorkOperation> operations;
    std::vector<WorldPixelWrite> pixels;
};

enum class FailurePolygonStyle { outline, filled };

FailurePolygonWork failurePolygonOperations(const std::vector<ProjectedPoint>& points,
    Viewport viewport, PolygonClipMode clipping, FailurePolygonStyle style,
    std::uint8_t primary, std::uint8_t secondary);

struct FailureModelSceneItem {
    std::uint16_t allocationIdentity;
    WorldSceneActor actor;
};

struct FailureModelObservation {
    std::uint16_t allocationIdentity;
    std::optional<Level1AnimationState> animation;
};

struct FailureModelSceneWork {
    WorldSceneWork scene;
    std::vector<FailureModelObservation> observations;
};

// Model-only B9A5..BC3B traversal, preserving source ordering. The mixed variant
// is declared once the independent bitmap child's typed state is available.
FailureModelSceneWork failureModelSceneOperations(const GameplayAssets& assets,
    const ViewTransform& view, const std::vector<FailureModelSceneItem>& items,
    WorldSceneProfile profile, std::array<std::int16_t, 3> transformedLight);

struct FailureBitmapSceneItem {
    std::uint16_t allocationIdentity;
    WorldActorWorkInput actor;
    std::uint32_t collisionRadius;
    FailureEffectState state;
};
using FailureSceneItem = std::variant<FailureModelSceneItem, FailureBitmapSceneItem>;

struct FailureEffectObservation {
    FailureEffectState state;
    FixedPosition cachedCenter;
    bool projected;
    bool retired;
    std::vector<FailureEffectStateWrite> stateWrites;
    std::optional<FailureEffectRemoval> removal;
};
struct FailureSceneObservation {
    std::uint16_t allocationIdentity;
    std::optional<Level1AnimationState> animation;
    std::optional<FailureEffectObservation> effect;
};
struct FailureSceneWork {
    WorldSceneWork scene;
    std::vector<RadarPixelWrite> pixels;
    std::vector<FailureSceneObservation> observations;
    std::vector<std::uint16_t> allocationIdentities;
};

enum class FailureEffectProfile { bitmap };
//B9A5..BC3B. Input order is the retained presenter list; registry identities are
//the separate481A allocation list. Returned copies are observations, not owners.
FailureSceneWork failureSceneOperations(const GameplayAssets& assets, const ViewTransform& view,
    const std::vector<FailureSceneItem>& items, WorldSceneProfile profile,
    std::array<std::int16_t, 3> transformedLight, std::uint16_t page,
    const std::vector<std::uint16_t>& allocationIdentities,
    FailureEffectProfile effectProfile = FailureEffectProfile::bitmap);

}
