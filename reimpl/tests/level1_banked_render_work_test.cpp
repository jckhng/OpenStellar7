#include "stellar7/level1_banked_render_work.hpp"
#include "stellar7/world_model_work.hpp"
#include "stellar7/world_scene_work.hpp"

#include <cassert>
#include <stdexcept>

namespace {
template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    assert(rejected);
}
std::uint64_t dispatches(const stellar7::WorldModelGeometryWork& work) {
    std::uint64_t result = 0;
    const auto add = [&](const auto& operations) {
        for (const auto& operation : operations) {
            result += operation.kind == stellar7::NormalWorkKind::instructions ? operation.count : 1;
        }
    };
    add(work.suffix);
    for (const auto& polygon : work.polygons) { add(polygon.prefix); }
    return result;
}
}

int main() {
    using namespace stellar7;
    constexpr std::array<std::uint16_t, 3> angles{0, 65265, 18432};
    const auto matrix = makeFixedMatrixQ14(angles[0], angles[1], angles[2]);
    // Original34AE and38FF controls, including CX5240's two-product residue.
    assert(matrix.mode == 2);
    assert((matrix.values == std::array<std::int16_t, 9>{-3197, 16065, 428,
        -16068, -3198, 0, 82, -419, 16378}));
    const auto camera = worldViewSetupOperations({{}, {}, {0, 0, 65216},
        0x88, 7, {24, 294, 24, 145}, {256, -256, 362}});
    const auto combined = composeFixedMatricesQ14(matrix, camera.view.matrix, 0x5240);
    assert(combined.mode == 3);
    assert((combined.values == std::array<std::int16_t, 9>{-3688, 15958, 428,
        -15965, -3692, 0, 94, -418, 16378}));
    assert(combined.values != composeFixedMatricesQ14(matrix, camera.view.matrix, 0).values);

    ModelGroup group{};
    group.vertexBlock = 10;
    group.animationOffset = -1;
    group.vertices = {{{-103, 217, -359}}, {{103, -217, 359}},
        {{-173, 47, -239}}, {{71, 191, 133}}};
    ModelPrimitive primitive{};
    primitive.shapes = {{0xa1, {8, 8, 1, 1}, 0, {1, 2, 3}}};
    group.primitives = {primitive};
    const ModelLod lod{255, 128, {group}};
    WorldModelGeometryInput geometry{};
    geometry.matrix = matrix;
    geometry.translation = {0, 1000, 0};
    geometry.groupOrder = {0};
    geometry.animationSelectors = {0};
    geometry.shadeLevels[8] = 1;
    geometry.lightingAdjustments[13] = 3;
    for (std::size_t index = 0; index < geometry.colorRemap.size(); ++index) {
        geometry.colorRemap[index] = static_cast<std::uint8_t>(index);
    }
    rejects([&] { worldModelGeometryOperations(lod, geometry); });
    geometry.matrixProfile = WorldModelMatrixProfile::level1Banked;
    const auto full = worldModelGeometryOperations(lod, geometry);
    assert(full.polygons.size() == 1 && full.polygons[0].primaryColor == 11);
    // Original3E12 outputs include third-axis products and signed floor shifts.
    const auto& points = full.polygons[0].points;
    assert(points.size() == 3);
    assert(points[0].x == 180 && points[0].y == 44);
    assert(points[1].x == 157 && points[1].y == 121);
    assert(points[2].x == 134 && points[2].y == 68);
    auto yaw = geometry;
    yaw.matrix.mode = 1;
    yaw.matrixProfile = WorldModelMatrixProfile::identityOrYaw;
    assert(dispatches(full) == dispatches(worldModelGeometryOperations(lod, yaw)) + 4 * 33);
    geometry.matrix = combined;
    const auto mixed = worldModelGeometryOperations(lod, geometry);
    assert(mixed.polygons[0].points[1].x == 158);
    geometry.matrixProfile = static_cast<WorldModelMatrixProfile>(255);
    rejects([&] { worldModelGeometryOperations(lod, geometry); });
    geometry.matrixProfile = WorldModelMatrixProfile::level1Banked;
    geometry.vertexPrecision = 1;
    rejects([&] { worldModelGeometryOperations(lod, geometry); });
    geometry.vertexPrecision = 0;
    geometry.animationSelectors.clear();
    rejects([&] { worldModelGeometryOperations(lod, geometry); });

    ModelGroup origin{};
    origin.vertices = {{{103, -217, 359}}};
    auto opposite = origin;
    opposite.vertices[0] = {-103, 217, -359};
    Model3d lift{};
    lift.collisionRadius = 748;
    lift.lods = {{30, 128, {origin, opposite}}};
    const FixedPosition center{5012, 7556, 1000};
    const WorldModelViewInput view{makeFixedMatrixQ14(0, 0, 0), center};
    rejects([&] { worldModelSetupOperations(center, angles, 14, 11, lift, 256, 256, view); });
    const auto setup = level1BankedModelSetupOperations(center, angles, 14, 11, lift, 256, 256, view);
    assert(setup.matrix.values == matrix.values && setup.vertexPrecision == 0);
    assert((setup.groupDepths == std::vector<std::int32_t>{2345832, -2359400}));
    assert((setup.groupOrder == std::vector<std::size_t>{0, 1}));
    rejects([&] { level1BankedModelSetupOperations(center, {1, angles[1], angles[2]}, 14, 11, lift, 256, 256, view); });
    rejects([&] { level1BankedModelSetupOperations(center, {0, 0, angles[2]}, 14, 11, lift, 256, 256, view); });
    rejects([&] { level1BankedModelSetupOperations(center, angles, 14, 1, lift, 256, 256, view); });
    auto badView = view;
    badView.matrix = matrix;
    rejects([&] { level1BankedModelSetupOperations(center, angles, 14, 11, lift, 256, 256, badView); });
    rejects([&] { worldSceneOperations({}, camera.view, {}, {256, 256, 0, 0}, {},
        static_cast<WorldSceneMatrixProfile>(255)); });
}
