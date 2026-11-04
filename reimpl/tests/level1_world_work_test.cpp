#include "stellar7/level1_world_work.hpp"
#include "stellar7/world_scene_work.hpp"
#include "stellar7/world_model_work.hpp"

#include <cassert>
#include <stdexcept>

namespace {
template<class Function>
void rejects(Function function) {
    bool rejected = false;
    try { function(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
std::uint64_t dispatches(const std::vector<stellar7::NormalWorkOperation>& operations) {
    std::uint64_t result = 0;
    for (const auto& operation : operations) {
        result += operation.kind == stellar7::NormalWorkKind::instructions ? operation.count : 1;
    }
    return result;
}
std::uint64_t dispatches(const stellar7::WorldModelGeometryWork& work) {
    auto result = dispatches(work.suffix);
    for (const auto& polygon : work.polygons) { result += dispatches(polygon.prefix); }
    return result;
}
}
int main() {
    using namespace stellar7;
    Model3d animated{};
    animated.flags = 1;
    animated.collisionRadius = 748;
    animated.animationLimits = {8};
    const auto initial = level1AnimationOperations({5, 0, 0}, animated);
    assert(initial.state.countdown == 5 && initial.state.variant == 1 && dispatches(initial.operations) == 144);
    const auto firstRadar = level1AnimationOperations(initial.state, animated);
    const auto secondMain = level1AnimationOperations(firstRadar.state, animated);
    const auto secondRadar = level1AnimationOperations(secondMain.state, animated);
    assert(firstRadar.state.countdown == 4 && secondMain.state.countdown == 3 && secondRadar.state.countdown == 2);
    const auto wrapped = level1AnimationOperations({5, 0, 7}, animated);
    assert(wrapped.state.countdown == 5 && wrapped.state.variant == 0 && dispatches(wrapped.operations) == 145);
    Level1AnimationState state{5, 4, 1};
    for (const auto countdown : {3, 2, 1, 0}) {
        const auto work = level1AnimationOperations(state, animated);
        assert(work.state.countdown == countdown && work.state.variant == 1 && work.state.reload == 5);
        assert(work.operations.size() == 1 && work.operations[0].kind == NormalWorkKind::instructions
            && work.operations[0].count == 15);
        state = work.state;
    }
    rejects([&]() { level1AnimationOperations({5, 6, 1}, animated); });
    rejects([&]() { level1AnimationOperations({5, 4, 8}, animated); });
    rejects([&]() { level1AnimationOperations({4, 4, 1}, animated); });
    auto wrongModel = animated;
    wrongModel.animationLimits = {7};
    rejects([&]() { level1AnimationOperations({5, 4, 1}, wrongModel); });

    //Every culled actor still owns its caller/return and Type11's callback.
    GameplayAssets assets;
    assets.models.resize(31);
    assets.models[1].collisionRadius = 1342;
    assets.models[11] = animated;
    assets.models[29].collisionRadius = 862;
    const ViewTransform view{{}, makeFixedMatrixQ14(0, 0, 0), {24, 294, 24, 145}, 128};
    std::vector<WorldSceneActor> actors{
        {{0, {16384, -16384, 0}}, {}, 1, 1342, 0, 0},
        {{0, {8192, -16384, 1000}}, {}, 11, 748, 0x130c, 0, Level1AnimationState{5, 4, 1}},
        {{0, {-16384, -16384, 0}}, {}, 29, 862, 0, 0}
    };
    const auto scene = worldSceneOperations(assets, view, actors, {256, 256, 0, 0});
    assert(scene.centers.size() == actors.size() && scene.culled == std::vector<bool>({true, true, true}));
    assert(scene.pixels.empty() && dispatches(scene.operations) == 808);
    assert(!scene.animationStates[0] && scene.animationStates[1]->countdown == 3 && !scene.animationStates[2]);
    auto unsupported = actors;
    unsupported[1].heapAnimation.reset();
    rejects([&]() { worldSceneOperations(assets, view, unsupported, {256, 256, 0, 0}); });
    unsupported = actors;
    unsupported[0].drawCallback = 0x130c;
    rejects([&]() { worldSceneOperations(assets, view, unsupported, {256, 256, 0, 0}); });

    WorldModelGeometryInput geometry{};
    geometry.matrix.values = {16384, 0, 0, 0, 16384, 0, 0, 0, 16384};
    geometry.matrix.mode = 1;
    geometry.groupOrder = {0};
    for (std::size_t index = 0; index < geometry.colorRemap.size(); ++index) {
        geometry.colorRemap[index] = static_cast<std::uint8_t>(index);
    }
    ModelGroup group{};
    group.vertexBlock = 10;
    group.animationOffset = 0;
    group.vertices = {{{0, -256, 0}}, {{-10, 1000, 0}}, {{10, 1000, 0}}, {{0, 1000, 10}}};
    ModelPrimitive first{};
    first.shapes = {{0x81, {8, 8, 1, 1}, 0, {1, 2, 3}}};
    auto second = first;
    second.shapes[0].colors = {9, 9, 1, 1};
    group.primitives = {first, second};
    ModelLod lod{255, 128, {group}};
    const auto defaultVariant = worldModelGeometryOperations(lod, geometry);
    geometry.animationSelectors = {1};
    const auto selected = worldModelGeometryOperations(lod, geometry);
    assert(defaultVariant.polygons[0].primaryColor == 8 && selected.polygons[0].primaryColor == 9);
    assert(dispatches(selected) == dispatches(defaultVariant) + 9);
    geometry.animationSelectors = {2};
    const auto fallback = worldModelGeometryOperations(lod, geometry);
    assert(fallback.polygons[0].primaryColor == 8 && dispatches(fallback) == dispatches(defaultVariant) + 10);
    lod.groups[0].animationOffset = -1;
    const auto noSelector = worldModelGeometryOperations(lod, geometry);
    assert(noSelector.polygons[0].primaryColor == 8 && dispatches(noSelector) == dispatches(defaultVariant) + 3);
    lod.groups[0].animationOffset = 1;
    rejects([&]() { worldModelGeometryOperations(lod, geometry); });

    ModelGroup originGroup{};
    originGroup.vertices = {{{0, 0, 0}}};
    Model3d tank{};
    tank.collisionRadius = 1342;
    tank.lods = {{10, 128, {originGroup, originGroup}},
        {30, 128, {originGroup, originGroup}}, {255, 128, {originGroup, originGroup}}};
    const auto ordinaryLastLod = worldModelSetupOperations({-1841, 2016, 0}, {}, 254, 1, tank, 256, 256);
    const auto exhaustedLods = worldModelSetupOperations({-1841, 1961, 0}, {}, 256, 1, tank, 256, 256);
    assert(ordinaryLastLod.selectedLodIndex == 2 && exhaustedLods.selectedLodIndex == 2);
    assert(ordinaryLastLod.groupOrder == exhaustedLods.groupOrder);
    assert(dispatches(exhaustedLods.operations) == dispatches(ordinaryLastLod.operations) + 5);
    rejects([&]() { worldModelSetupOperations({-1841, 1961, 0}, {}, 256, 8, tank, 256, 256); });
}
