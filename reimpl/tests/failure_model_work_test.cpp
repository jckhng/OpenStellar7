#include "stellar7/failure_model_work.hpp"
#include "stellar7/raster_line_work.hpp"
#include <cassert>
#include <stdexcept>
#include <utility>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif
namespace {
using namespace stellar7;
std::uint64_t workHash(const std::vector<NormalWorkOperation>& operations) {
    std::uint64_t hash = 14695981039346656037ULL;
    std::uint64_t pending = 0;
    const auto add = [&](const std::uint64_t kind, const std::uint64_t count) {
        hash = (hash ^ kind) * 1099511628211ULL;
        hash = (hash ^ count) * 1099511628211ULL;
    };
    for (const auto operation : operations) {
        if (operation.kind == NormalWorkKind::instructions) {
            pending += operation.count;
        } else {
            if (pending != 0) { add(0, pending); pending = 0; }
            add(static_cast<unsigned>(operation.kind), operation.count);
        }
    }
    if (pending != 0) { add(0, pending); }
    return hash;
}
std::uint64_t pixelHash(const std::vector<WorldPixelWrite>& pixels) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const auto pixel : pixels) {
        for (const auto value : {std::int64_t{pixel.point.x}, std::int64_t{pixel.point.y}, std::int64_t{pixel.color}}) {
            hash = (hash ^ static_cast<std::uint64_t>(value)) * 1099511628211ULL;
        }
    }
    return hash;
}
template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    assert(rejected);
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace stellar7;
    constexpr Viewport viewport{0, 318, 0, 199};
    //Pinned original receipt SHA256 97982fe851cded574ac2d80ae938e7086b39377de0ed536c1e337f415199afb1
    { //near-front
        ModelLod lod{};
        lod.threshold = 255; lod.flags = 128;
        ModelGroup group{}; group.animationOffset = -1; group.vertexBlock = 1;
        group.vertices = {{-40,256,-20},{40,256,-20},{0,256,40}};
        ModelPrimitive primitive{};
        primitive.shapes.push_back({0x80, {72, 72, 0, 0}, 255, {0, 1, 2}});
        group.primitives.push_back(primitive); lod.groups.push_back(group);
        WorldModelGeometryInput input{};
        input.matrix = makeFixedMatrixQ14(0, 0, 0); input.groupOrder = {0}; input.screenCenter = {159, 99};
        for (unsigned index = 0; index < 256; ++index) { input.colorRemap[index] = static_cast<std::uint8_t>(index); }
        const auto result = failureModelGeometryOperations(lod, input);
        assert(result.polygons.size() == 1);
        assert((result.polygons.front().points == std::vector<ProjectedPoint>{{139,109},{179,109},{159,79}}));
    }
    { //near-crossing
        ModelLod lod{};
        lod.threshold = 255; lod.flags = 128;
        ModelGroup group{}; group.animationOffset = -1; group.vertexBlock = 1;
        group.vertices = {{-40,64,-20},{40,256,-20},{0,256,40}};
        ModelPrimitive primitive{};
        primitive.shapes.push_back({0x80, {72, 72, 0, 0}, 255, {0, 1, 2}});
        group.primitives.push_back(primitive); lod.groups.push_back(group);
        WorldModelGeometryInput input{};
        input.matrix = makeFixedMatrixQ14(0, 0, 0); input.groupOrder = {0}; input.screenCenter = {159, 99};
        for (unsigned index = 0; index < 256; ++index) { input.colorRemap[index] = static_cast<std::uint8_t>(index); }
        const auto result = failureModelGeometryOperations(lod, input);
        assert(result.polygons.size() == 1);
        assert((result.polygons.front().points == std::vector<ProjectedPoint>{{132,99},{145,119},{179,109},{159,79}}));
        rejects([&] { worldModelGeometryOperations(lod, input); });
    }
    { //near-all-behind
        ModelLod lod{};
        lod.threshold = 255; lod.flags = 128;
        ModelGroup group{}; group.animationOffset = -1; group.vertexBlock = 1;
        group.vertices = {{-40,64,-20},{40,64,-20},{0,64,40}};
        ModelPrimitive primitive{};
        primitive.shapes.push_back({0x80, {72, 72, 0, 0}, 255, {0, 1, 2}});
        group.primitives.push_back(primitive); lod.groups.push_back(group);
        WorldModelGeometryInput input{};
        input.matrix = makeFixedMatrixQ14(0, 0, 0); input.groupOrder = {0}; input.screenCenter = {159, 99};
        for (unsigned index = 0; index < 256; ++index) { input.colorRemap[index] = static_cast<std::uint8_t>(index); }
        const auto result = failureModelGeometryOperations(lod, input);
        assert(result.polygons.size() == 1);
        assert((result.polygons.front().points == std::vector<ProjectedPoint>{}));
        rejects([&] { worldModelGeometryOperations(lod, input); });
    }
    { //ship-center
        const auto result = failureModelCullOperations({0,1800,0}, 723, viewport);
        assert(result.culled == false);
        assert(workHash(result.operations) == 590070540331891651ULL);
    }
    { //debris-negative-altitude
        const auto result = failureModelCullOperations({-300,1800,-200}, 171, viewport);
        assert(result.culled == false);
        assert(workHash(result.operations) == 590022161820250367ULL);
    }
    { //sphere-near-clamp
        const auto result = failureModelCullOperations({0,200,0}, 723, viewport);
        assert(result.culled == false);
        assert(workHash(result.operations) == 590077137401660917ULL);
    }
    { //sphere-shallow
        const auto result = failureModelCullOperations({0,-700,0}, 723, viewport);
        assert(result.culled == true);
        assert(workHash(result.operations) == 590656580029728114ULL);
    }
    { //sphere-right
        const auto result = failureModelCullOperations({10000,1800,0}, 723, viewport);
        assert(result.culled == true);
        assert(workHash(result.operations) == 590405891378496006ULL);
    }
    { //sphere-upper
        const auto result = failureModelCullOperations({0,1800,10000}, 723, viewport);
        assert(result.culled == true);
        assert(workHash(result.operations) == 590331124587777658ULL);
    }
    { //empty
        const auto result = failurePolygonOperations({}, viewport, PolygonClipMode::Enabled, FailurePolygonStyle::filled, 72, 72);
        assert(workHash(result.operations) == 7656119027321427042ULL);
        assert(result.pixels.size() == 0);
        assert(pixelHash(result.pixels) == 14695981039346656037ULL);
    }
    { //point
        const auto result = failurePolygonOperations({{159,99}}, viewport, PolygonClipMode::Enabled, FailurePolygonStyle::filled, 72, 72);
        assert(workHash(result.operations) == 11441649973227049214ULL);
        assert(result.pixels.size() == 0);
        assert(pixelHash(result.pixels) == 14695981039346656037ULL);
    }
    { //line-crossing
        const auto result = failurePolygonOperations({{-20,99},{30,105}}, viewport, PolygonClipMode::Enabled, FailurePolygonStyle::filled, 72, 72);
        assert(workHash(result.operations) == 17280798311775996162ULL);
        assert(result.pixels.size() == 31);
        assert(pixelHash(result.pixels) == 1284434481273345041ULL);
    }
    { //solid
        const auto result = failurePolygonOperations({{80,60},{240,60},{160,140}}, viewport, PolygonClipMode::Enabled, FailurePolygonStyle::filled, 72, 72);
        assert(workHash(result.operations) == 11820348059758536341ULL);
        assert(result.pixels.size() == 6561);
        assert(pixelHash(result.pixels) == 10069041788907332403ULL);
    }
    { //solid-outline
        const auto result = failurePolygonOperations({{80,60},{240,60},{160,140}}, viewport, PolygonClipMode::Enabled, FailurePolygonStyle::filled, 72, 156);
        assert(workHash(result.operations) == 5456509256085172563ULL);
        assert(result.pixels.size() == 6884);
        assert(pixelHash(result.pixels) == 8455150666438500401ULL);
    }
    { //outline
        const auto result = failurePolygonOperations({{80,60},{240,60},{160,140}}, viewport, PolygonClipMode::Enabled, FailurePolygonStyle::outline, 72, 156);
        assert(workHash(result.operations) == 8314940377675918360ULL);
        assert(result.pixels.size() == 323);
        assert(pixelHash(result.pixels) == 9136433716913905571ULL);
    }
    { //collapsed-color
        const auto result = failurePolygonOperations({{80,60},{240,60},{160,60}}, viewport, PolygonClipMode::Enabled, FailurePolygonStyle::filled, 72, 156);
        assert(workHash(result.operations) == 8808491955149450365ULL);
        assert(result.pixels.size() == 161);
        assert(pixelHash(result.pixels) == 14355384597127658579ULL);
    }
    { //empty-horizontal
        const auto result = failurePolygonOperations({{-30,50},{-10,60},{-20,70}}, viewport, PolygonClipMode::Enabled, FailurePolygonStyle::filled, 72, 72);
        assert(workHash(result.operations) == 17520358208684009707ULL);
        assert(result.pixels.size() == 0);
        assert(pixelHash(result.pixels) == 14695981039346656037ULL);
    }
    { //empty-vertical
        const auto result = failurePolygonOperations({{30,-50},{50,-60},{40,-70}}, viewport, PolygonClipMode::Enabled, FailurePolygonStyle::filled, 72, 72);
        assert(workHash(result.operations) == 15584703743434539636ULL);
        assert(result.pixels.size() == 0);
        assert(pixelHash(result.pixels) == 14695981039346656037ULL);
    }
    rejects([&] { failureModelCullOperations({0, 1800, 0}, 723, {24, 294, 24, 145}); });
    rejects([&] { failurePolygonOperations({}, {0, 319, 0, 199}, PolygonClipMode::Enabled, FailurePolygonStyle::filled, 72, 72); });
    rejects([&] { collapsedPolygonOperations({{159, 99}}, viewport, PolygonClipMode::Disabled); });
    rejects([&] { worldModelCullOperations({0, 1800, 0}, 723); });
    { //C787 signed Z belongs to setup even when the actor matrix is identity.
        Model3d model{};
        model.collisionRadius = 171;
        ModelLod lod{}; lod.flags = 128;
        ModelGroup group{}; group.vertices = {{{0, 0, 0}}};
        lod.groups.push_back(group); model.lods.push_back(lod);
        const auto matrix = makeFixedMatrixQ14(0, 0, 0);
        const auto setup = [&](const std::int32_t z) {
            const FixedPosition center{0, 1800, z};
            const auto cull = failureModelCullOperations(center, model.collisionRadius, viewport);
            return failureModelSetupOperations(center, {}, cull.projectedRadius, 36, model,
                256, 256, {matrix, center});
        };
        const auto count = [](const auto& operations) {
            std::uint64_t result = 0;
            for (const auto operation : operations) {
                result += operation.kind == NormalWorkKind::instructions ? operation.count : 1;
            }
            return result;
        };
        const auto positive = setup(200);
        const auto negative = setup(-200);
        assert(count(negative.operations) == count(positive.operations) + 4);
        assert(positive.groupOrder == negative.groupOrder);
    }
    { //5985 tests actor identity5074, independently of the camera matrix.
        ModelLod lod{}; lod.flags = 128;
        ModelGroup group{}; group.animationOffset = -1; group.vertexBlock = 1;
        group.vertices = {{{16384, 0, 0}}, {{-40, 0, -20}}, {{40, 0, -20}}, {{0, 0, 40}}};
        ModelPrimitive primitive{};
        primitive.shapes.push_back({0x81, {72, 72, 0, 0}, 0, {1, 2, 3}});
        group.primitives.push_back(primitive); lod.groups.push_back(group);
        WorldModelGeometryInput input{};
        input.matrix = makeFixedMatrixQ14(0, 0, 0xc000);
        input.translation = {0, 1800, 0}; input.groupOrder = {0}; input.screenCenter = {159, 99};
        for (unsigned index = 0; index < 256; ++index) { input.colorRemap[index] = static_cast<std::uint8_t>(index); }
        const auto raw = failureModelGeometryOperations(lod, input, FixedPosition{-1800, 0, 0});
        const auto transformed = failureModelGeometryOperations(lod, input);
        assert(raw.polygons.size() == 1 && transformed.polygons.size() == 1);
        assert(raw.polygons.front().points == transformed.polygons.front().points);
        const auto directFacing = [](const auto& operations) {
            for (const auto operation : operations) {
                if (operation.kind == NormalWorkKind::instructions && operation.count == 66) { return true; }
            }
            return false;
        };
        assert(directFacing(raw.polygons.front().prefix)); //5985 prefix7 +5906 body59.
        assert(!directFacing(transformed.polygons.front().prefix));
    }
    return 0;
}
