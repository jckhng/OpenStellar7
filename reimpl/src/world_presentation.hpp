#pragma once
#include "stellar7/level1.hpp"
#include "stellar7/resource_codec.hpp"
#include "enhanced_view.hpp"
#include "chase_options.hpp"
#include "stellar7/destruction_presentation.hpp"
#include "spatial_scene.hpp"
#include <memory>
#include <optional>

namespace stellar7 {
struct PresentationPoint { double x,y; };
struct PresentationIdentity {
    std::size_t allocation,group,shape,variant;
    std::int16_t type;
    std::uint16_t lod;
    std::optional<std::uint64_t> incarnation;
    bool operator==(const PresentationIdentity& other) const;
};
struct PresentationFace {
    std::vector<PresentationPoint> points;
    Viewport clip;
    std::uint8_t color;
    bool filled;
    std::optional<PresentationIdentity> identity;
    double lighting=1,opacity=1;
    std::optional<Rgb> tint;
    std::vector<SpatialPoint> spatialPoints;
    float spatialStrokeRadius=0;
    float rasterStrokeWidth=0; // Native pixels; zero retains the ordinary one-output-pixel pen.
};
// Clip projected enhanced geometry before narrowing to the original raster ABI.
std::vector<ProjectedPoint> boundedPresentationPolygon(const PresentationFace& face);
struct PresentationPatch { std::size_t offset; std::vector<std::uint8_t> pixels; };
struct PresentationDraw {
    std::vector<PresentationPatch> patches;
    std::optional<PresentationFace> face;
};
struct WorldPresentation {
    std::vector<std::uint8_t> base,source;
    std::vector<PresentationDraw> draws;
    std::size_t level;
    unsigned phase,tick;
    std::shared_ptr<const WorldPresentation> chase;
    std::vector<DestructionCue> destructions;
    std::uint64_t worldTick=0;
    ChaseOptions camera;
    std::shared_ptr<const SpatialScene> spatial;
};

// Opt-in on the calling thread. Capturing observes the existing renderer once.
enum class PresentationGeometry {projected,spatial};
class WorldPresentationCapture {
public:
    explicit WorldPresentationCapture(ChaseOptions camera={},PresentationGeometry geometry=PresentationGeometry::projected);
    ~WorldPresentationCapture();
    WorldPresentationCapture(const WorldPresentationCapture&)=delete;
    WorldPresentationCapture& operator=(const WorldPresentationCapture&)=delete;
    static bool enabled();
    static ChaseOptions camera();
    static bool spatialEnabled();
private:
    bool previous_;
    ChaseOptions previousCamera_;
    bool previousSpatial_;
};
class WorldPresentationRecorder {
public:
    WorldPresentationRecorder(const std::vector<std::uint8_t>& pixels,std::size_t level,unsigned phase,unsigned tick,
        std::optional<ViewTransform> spatialView={});
    bool capturesSpatial() const {return spatial_!=nullptr;}
    void spatialSurface(SpatialSurface surface);
    void spatialBillboard(SpatialBillboard billboard);
    void spatialWorld(SpatialWorld world,std::vector<SpatialLight> lights);
    void spatialOverlay(const std::vector<std::uint8_t>& before,const std::vector<std::uint8_t>& after,std::size_t opaqueBegin);
    void beforeFace(const std::vector<std::uint8_t>& pixels,PresentationFace face);
    void afterFace(const std::vector<std::uint8_t>& pixels);
    void recordPixels(const std::vector<std::uint8_t>& pixels);
    void recordDestructions(const std::vector<DestructionCue>& cues,std::uint64_t worldTick);
    std::shared_ptr<const WorldPresentation> finish(const std::vector<std::uint8_t>& pixels,
        std::shared_ptr<const WorldPresentation> chase={});
private:
    void changes(const std::vector<std::uint8_t>& pixels,PresentationDraw& draw);
    std::shared_ptr<WorldPresentation> frame_;
    std::vector<std::uint8_t> previous_;
    std::shared_ptr<SpatialScene> spatial_;
};

// RGB presentation only. Scale1 returns the exact indexed/DAC expansion.
std::vector<std::uint32_t> renderWorldPresentation(const IndexedScreen& frame,unsigned scale,
    const IndexedScreen* next=nullptr,double fraction=0,EnhancedView view=EnhancedView::cockpit);
} // namespace stellar7
