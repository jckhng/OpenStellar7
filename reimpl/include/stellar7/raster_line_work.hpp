#pragma once
#include "stellar7/render_work.hpp"

namespace stellar7 {
struct RasterLineWork {
    std::vector<ProjectedPoint> pixels;
    std::vector<NormalWorkOperation> operations;
};

// Native Mode-X profile; endpoints inside viewport, sequencer index2 retained.
// 21b4 body through RETF, including resident0283/0205 exactly once.
RasterLineWork rasterLineOperations(ProjectedPoint first, ProjectedPoint second,
    Viewport viewport, PolygonClipMode clipping);

//B0B4 direct21B4 child; unclipped beam endpoints may leave gameplay viewport.
RasterLineWork level1CompletionBeamLineOperations(ProjectedPoint first, ProjectedPoint second,
    Viewport viewport, PolygonClipMode clipping);

// Source698a armor column: evenX112..196, Y189..195, style0/unclipped21b4 body.
RasterLineWork rasterArmorColumnOperations(std::int16_t x);

// Explicit Level1 combat armor[-320,680]: evenX72..196, same source body.
RasterLineWork rasterCombatArmorColumnOperations(std::int16_t x);
RasterLineWork rasterLevel2LifecycleArmorColumnOperations(std::int16_t x);

// Combat model's NativeScale uniform two-point3467 path, including21B4 clipping.
RasterLineWork level1CombatLinePolygonOperations(std::array<ProjectedPoint, 2> points, Viewport viewport);

// Uniform NativeScale, clippingDisabled3467 CALLF through RETF; collapsed only.
RasterLineWork collapsedPolygonOperations(const std::vector<ProjectedPoint>& points,
    Viewport viewport, PolygonClipMode clipping);
// Explicit owned Level1/failure NativeScale uniform3467 CALLF through RETF.
// Three..20 source points, ordinary or failure viewport, signed coordinates
// bounded to16383. Includes horizontal short-copy and vertical short-return
// clipping, direct two-point dispatch and collapsed cleanup; filled areas reject.
RasterLineWork ownedCollapsedPolygonOperations(const std::vector<ProjectedPoint>& points,
    Viewport viewport, PolygonClipMode clipping);

struct FailurePolygonWork;
enum class FailurePolygonStyle;

//Shared native3467 color/style family for the owned ordinary and failure viewports.
//Uses the existing failure polygon result/style types; the failure entry stays strict.
FailurePolygonWork ownedPolygonOperations(const std::vector<ProjectedPoint>& points,
    Viewport viewport, PolygonClipMode clipping, FailurePolygonStyle style,
    std::uint8_t primary, std::uint8_t secondary);
}
