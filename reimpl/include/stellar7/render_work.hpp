#pragma once

#include "stellar7/level1.hpp"
#include "stellar7/normal_clock.hpp"
#include "stellar7/resource_codec.hpp"

#include <cstdint>
#include <array>
#include <vector>
#include <variant>
#include <optional>

namespace stellar7 {

struct RasterSpan {
    std::int16_t left;
    std::int16_t right;
};

struct SpanWork {
    std::uint64_t dispatches;
    std::uint64_t stringIterations;
    std::uint64_t portInstructions;
};

// Resident106a body through RETF; excludes caller, REP restarts, IRQs and I/O delays.
SpanWork planarCopyWork(std::uint16_t x, std::uint16_t width, std::uint16_t height);

// Same domain, preserving each VGA write and row's REP position through RETF.
std::vector<NormalWorkOperation> planarCopyOperations(std::uint16_t x, std::uint16_t width, std::uint16_t height);

enum class PlanarBlitMode { transparent, opaque, generalTransparent };

// Unclipped resident0a47/1dd6/1280 bodies through RETF;1280 is unmirrored.
// Width must be divisible by4; opaque X must be aligned.
// Excludes caller, REP restarts, IRQs and I/O delays.
SpanWork planarBitmapWork(const IndexedBitmap& bitmap, std::int16_t x, PlanarBlitMode mode);

// Same domain through RETF. Pixels are row-major; loads follow resident source-plane order.
std::vector<NormalWorkOperation> planarBitmapOperations(const IndexedBitmap& bitmap, std::int16_t x, PlanarBlitMode mode);

//6d46 control only: stage>70, differing cached/draw pages, nine admitted callees.
// Includes the near caller, resident dispatch stubs and wrapper RET, not callees.
std::uint64_t cockpitControlDispatches(std::int16_t indicatorX) noexcept;

enum class CockpitPageRelation { differing, matching };

struct CockpitWorkState {
    std::int16_t indicatorX;
    std::int16_t indicatorVelocity;
    std::int16_t stage;
    CockpitPageRelation pages;
};

struct CockpitWorkBitmaps {
    IndexedBitmap strip;
    IndexedBitmap indicator;
    std::array<IndexedBitmap, 6> overlays;
    IndexedBitmap matchingPageBackground;
};

enum class CockpitBitmapRole { strip, indicator, overlay0, overlay1, overlay2, overlay3, overlay4, overlay5,
    matchingPageBackground };

struct CockpitCopyRegion {
    std::int16_t x, y, width, height;
};

struct CockpitBitmapPlacement {
    CockpitBitmapRole bitmap;
    std::int16_t x, y;
    PlanarBlitMode mode;
};

using CockpitEffect = std::variant<CockpitCopyRegion, CockpitBitmapPlacement>;

struct CockpitWork {
    CockpitWorkState state;
    std::vector<NormalWorkOperation> operations;
    std::vector<CockpitEffect> effects;
};

//6d46 stage>70: near caller, ordered control/stubs, bitmap/copy bodies and RET.
// Matching pages first draw matchingPageBackground; no IRQ or phase inputs enter this plan.
CockpitWork cockpitOperations(const CockpitWorkState& state, const CockpitWorkBitmaps& bitmaps);

enum class HorizonBackground { solid, gradient };
enum class HorizonOverlay { disabled, enabled };

struct HorizonWorkState {
    std::int16_t viewTop;
    std::int16_t viewBottom;
    std::int16_t cameraYaw;
    std::int16_t stage;
    std::uint16_t bitmapWidth;
    std::uint16_t bitmapHeight;
    HorizonBackground background;
    HorizonOverlay overlay;
    std::uint8_t skyColor = 0;
    std::uint8_t groundColor = 0;
};

// Ordinary6b5a/6ba6: zero yaw, stage>70, view24..145 and uniform256-wide pages.
// Solid campaign sky or Level1 gradient; gradient rows saturate at color254.
// Includes near caller, viewport setup, sky/ground fills, three clipped blits and RET.
// Fixed planar filled/equal-color/default-scale profile; excludes IRQs and REP restarts.
std::vector<NormalWorkOperation> horizonOperations(const HorizonWorkState& state);

struct HorizonPixelWrite {
    ProjectedPoint point;
    std::uint8_t color;
};

struct HorizonExecutionWork {
    std::vector<NormalWorkOperation> operations;
    std::vector<HorizonPixelWrite> pixels;
    Viewport viewport;
    std::uint8_t lineColor;
    std::uint8_t fillColor;
};

//Same source boundary with ordered indexed stores, including zero panorama pixels.
//Gradient row order is bottom-to-top; opaque panorama stores follow source planes.
HorizonExecutionWork horizonExecution(const HorizonWorkState& state, const std::vector<IndexedBitmap>& frames);
HorizonExecutionWork level2HorizonExecution(const HorizonWorkState& state, const std::vector<IndexedBitmap>& frames);

// Unclipped2e25/0205 full320x200 front fill: Mode-X, equal zero colors, no remap/scale.
// Includes body/RETF;6622/9d4c callers and phase-dependent REP/IRQ costs excluded.
std::vector<NormalWorkOperation> fullFrontClearOperations();

struct GroundControlState {
    std::array<GroundPoint, Level1GroundState::pointCount> points;
    std::int16_t turn;
    std::int16_t speed;
    std::int32_t cameraHeight;
};

struct GroundDrawControl {
    ProjectedPoint screen;
    std::uint16_t shadeIndex;
    std::vector<NormalWorkOperation> prefix;
    std::vector<NormalWorkOperation> suffix;
};

struct GroundControlWork {
    std::vector<NormalWorkOperation> prefix;
    std::array<GroundDrawControl, Level1GroundState::pointCount> draws;
    std::vector<NormalWorkOperation> suffix;
};

//84ea caller and stationary4c46 control, six visible points, zero camera height.
// Each draw requires its1280 bitmap body between prefix and suffix; bodies excluded.
// Includes2c1b JMPF stub; excludes reset/RNG, IRQs, REP restarts and world traversal.
GroundControlWork groundControlOperations(const GroundControlState& state);

// Same stationary domain, clip enabled and selected bitmaps inside the viewport.
// Complete84ea ->84fd ground call; includes six1280 bodies, excludes world/IRQs.
std::vector<NormalWorkOperation> groundClippedOperations(const GroundControlState& state,
    const std::array<IndexedBitmap, 8>& bitmaps, Viewport viewport);

struct GroundPixelWrite {
    ProjectedPoint point;
    std::uint8_t color;
};

struct GroundExecutionWork {
    Level1GroundState ground;
    StellarRng rng;
    std::vector<NormalWorkOperation> operations;
    std::vector<GroundPixelWrite> pixels;
};

// Flat4c46, speed -200..200, including resets; retains ground/RNG through84fd.
// Input ground/RNG are pre-call state; replace semantic advance with this result.
GroundExecutionWork groundClippedExecution(const GroundControlState& state, const StellarRng& rng,
    const std::array<IndexedBitmap, 8>& bitmaps, Viewport viewport);

struct WorldViewSetupInput {
    FixedPosition origin;
    std::array<std::uint16_t, 3> viewAngles;
    std::array<std::uint16_t, 3> actorAngles;
    std::uint8_t flags;
    std::uint8_t projectionShift;
    Viewport viewport;
    FixedPosition light;
};

struct WorldViewSetupWork {
    ViewTransform view;
    FixedPosition transformedLight;
    std::vector<NormalWorkOperation> operations;
};

// Zero-angle b7ac through b994 RETF; excludes b996 caller, IRQs and REP restarts.
WorldViewSetupWork worldViewSetupOperations(const WorldViewSetupInput& state);

// Constructor radar view: pitchc000, zero camera pitch/roll and 16-unit yaw; same b994 RETF boundary.
WorldViewSetupWork radarViewSetupOperations(const WorldViewSetupInput& state);

// Same source matrix with full yaw; no CPU-work qualification or operations.
WorldViewSetupWork radarViewSetupPixels(const WorldViewSetupInput& state);

struct WorldActorWorkInput {
    std::uint8_t flags;
    FixedPosition position;
};

struct WorldFirstCenterWork {
    FixedPosition relativePosition;
    FixedPosition center;
    std::vector<NormalWorkOperation> operations;
};

// b9a5 through ba88: nonempty list, ordinary first actor, no global callback.
enum class WorldSourceFlagsDomain { ordinary, flagsWithoutCallback };

WorldFirstCenterWork worldFirstCenterOperations(const ViewTransform& view, const WorldActorWorkInput& actor);
WorldFirstCenterWork worldFirstCenterOperations(const ViewTransform& view, const WorldActorWorkInput& actor,
    WorldSourceFlagsDomain domain);

struct WorldModelCullWork {
    bool culled;
    bool clipped;
    std::uint16_t projectedRadius;
    std::vector<NormalWorkOperation> operations;
};

// Qualified flags0/profile0 cb7a entry through RETF or geometry entry cbca.
WorldModelCullWork worldModelCullOperations(FixedPosition center, std::uint32_t collisionRadius);

//Explicit immutable shared-model descriptor, precision0 arithmetic and main viewport.
//Reuses signed sphere projection/culling; the legacy radius-only API stays strict.
WorldModelCullWork sharedModelSphereCullOperations(FixedPosition center, const Model3d& model);

//Explicit ordinary Level1 Type1 sphere crossing camera X, flat/unclipped precision0.
//The legacy worldModelCullOperations admission is unchanged.
WorldModelCullWork level1AxisTankModelCullOperations(FixedPosition center);

struct WorldModelSetupWork {
    FixedMatrixQ14 matrix;
    FixedPosition translation;
    std::size_t selectedLodIndex;
    std::vector<std::size_t> groupOrder;
    std::vector<std::int32_t> groupDepths;
    std::uint8_t vertexPrecision;
    std::uint8_t smallRadius;
    std::vector<NormalWorkOperation> operations;
};

struct WorldModelViewInput {
    FixedMatrixQ14 matrix;
    FixedPosition relativePosition;
};

// cbca through cec1: shared registry256/256, yaw-only, first LOD/detail0.
WorldModelSetupWork worldModelSetupOperations(FixedPosition center, std::array<std::uint16_t, 3> actorAngles,
    std::uint16_t projectedRadius, std::int16_t type, const Model3d& model,
    std::uint16_t registryScale, std::uint16_t registryCap,
    const std::optional<WorldModelViewInput>& view = std::nullopt);

WorldModelSetupWork level1AxisTankModelSetupOperations(FixedPosition center,
    std::array<std::uint16_t, 3> actorAngles, std::uint16_t projectedRadius,
    const Model3d& model, std::uint16_t registryScale, std::uint16_t registryCap,
    const WorldModelViewInput& view);

//Explicit Type20/FD source model, inherited bank and altitude0..1000.
WorldModelCullWork level1AirborneProjectileModelCullOperations(FixedPosition center);
WorldModelSetupWork level1AirborneProjectileModelSetupOperations(FixedPosition center,
    std::array<std::uint16_t,3> angles,std::uint16_t projectedRadius,const Model3d& model,
    std::uint16_t registryScale,std::uint16_t registryCap,const WorldModelViewInput& view);

WorldModelSetupWork level2ConstructorModelSetupOperations(FixedPosition center,
    std::array<std::uint16_t,3> angles, std::uint16_t projectedRadius, std::int16_t type,
    const Model3d& model, std::uint16_t registryScale, std::uint16_t registryCap,
    const WorldModelViewInput& view);

// Resident3568:0466 solid-color path; excludes REP restarts, IRQs and I/O delays.
SpanWork solidSpanWork(const std::vector<RasterSpan>& rows);

// Shared3a0c writer: includes its RET, excludes caller/IRQ/scheduler work.
SpanWork edgeWriterWork(std::uint16_t rowCount);

enum class PolygonEdgeSide { Left, Right };

struct RasterEdge {
    std::vector<ProjectedPoint> samples;
    SpanWork work;
};

// Filled-edge dispatcher37d7 ->37b1; post-clipping Y domain0..399.
RasterEdge rasterEdge(ProjectedPoint first, ProjectedPoint second, PolygonEdgeSide side);

struct RasterPolygon {
    std::int16_t firstY;
    std::vector<RasterSpan> rows;
    std::vector<ProjectedPoint> normalizedPoints;
    SpanWork edgeWork;
    // Complete3549 ->3874 base work, including edgeWork; excludes IRQs/REP restarts.
    SpanWork preparationWork;
};

// Post-clipping filled polygon only; excludes line/collapsed paths and clip work.
RasterPolygon rasterPolygon(const std::vector<ProjectedPoint>& points);

enum class PolygonClipMode { Enabled, Disabled };

struct RasterClip {
    std::vector<ProjectedPoint> points;
    SpanWork work;
};

// Optional3533 ->353f viewport clip; includes CALLF/RETF, excludes IRQs/restarts.
RasterClip clipPolygon(const std::vector<ProjectedPoint>& points, Viewport viewport, PolygonClipMode mode);

enum class PolygonScale { Native, Double };
enum class PolygonColorRelation { Same, Different };

struct RasterInput {
    std::vector<ProjectedPoint> points;
    std::vector<ProjectedPoint> outlinePoints;
    Viewport viewport;
    SpanWork work;
};

// Filled3467 ->3533 setup plus caller CALLF; excludes clipping/IRQs/REP restarts.
RasterInput preparePolygon(const std::vector<ProjectedPoint>& points, Viewport viewport,
    PolygonScale scale, PolygonColorRelation colors);

struct RasterSolidPolygon {
    RasterPolygon polygon;
    Viewport returnedViewport;
    SpanWork prefixWork;
    SpanWork suffixWork;
    SpanWork work;
};

// Noncollapsed uniform solid call: caller CALLF through RETF; excludes IRQs/restarts.
RasterSolidPolygon solidPolygon(const std::vector<ProjectedPoint>& points, Viewport viewport,
    PolygonScale scale, PolygonClipMode clipping);

enum class VertexCacheState { Empty, Populated };

// FUN_26e6_57d9 includes the signed-16 transform on a cache miss.
std::uint64_t vertexCacheDispatches(VertexCacheState state, std::uint16_t matrixMode) noexcept;

// FUN_26e6_4fc2 base dispatches; excludes its caller, IRQs and scheduler charges.
std::uint64_t vectorTransformDispatches(
    FixedPosition relativePosition,
    const FixedMatrixQ14& matrix
) noexcept;

}  // namespace stellar7
