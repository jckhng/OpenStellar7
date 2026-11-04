#include "stellar7/combat_scene_work.hpp"
#include "stellar7/world_traversal.hpp"
#include <cassert>
#include <stdexcept>
using namespace stellar7;
namespace {
template<class Action> void rejects(Action action) {
    bool rejected=false;
    try {action();} catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
}
GameplayAssets assets() {
    GameplayAssets result{};result.models.resize(44);result.modelColorTables.resize(2);
    for(std::size_t index=0;index<256;++index) {result.modelColorTables[0][index]=static_cast<std::uint8_t>(index);}
    ModelGroup group{};group.animationOffset=-1;group.vertexBlock=1;
    group.vertices={{{0,-256,0}},{{-10,100,0}},{{10,100,0}},{{0,100,10}}};
    ModelPrimitive primitive{};primitive.shapes={{0x80,{8,8,1,1},255,{1,2,3}}};group.primitives={primitive};
    for(const std::int16_t type:{1,4,8,13,30}) {
        auto& model=result.models[type];model.flags=type==8 || type==1 || type==4 ? 1 : 0;
        model.collisionRadius=type==1?1342:type==4?928:type==8?860:type==13?559:868;
        model.lods={{255,128,{group,group}}};
    }
    return result;
}
}
int main() {
    const auto data=assets();const auto identity=makeFixedMatrixQ14(0,0,0);
    const ViewTransform view{{},identity,{24,294,24,145},128};
    constexpr WorldSceneProfile profile{256,256,0,0};
    std::vector<WorldSceneActor> waveLifts;
    std::vector<std::optional<Level1CombatEffectView>> waveEffects;
    std::vector<Level1ObjectRef> waveAllocations;
    for(std::size_t index=0;index<3;++index) {
        waveLifts.push_back({{0,{static_cast<std::int32_t>(index*1000)-1000,10000,90}},{},13,559,0,0});
        waveEffects.push_back(std::nullopt);
        waveAllocations.push_back({Level1PresentationKind::entity,index});
    }
    rejects([&] {level2CombatSceneOperations(data,view,waveLifts,waveEffects,profile,{256,-256,362},0xa000,waveAllocations);});
    const auto wave=level2PeriodicSceneOperations(data,view,waveLifts,waveEffects,profile,{256,-256,362},0xa000,waveAllocations);
    assert(wave.scene.centers.size()==3 && !wave.scene.operations.empty());
    waveLifts.push_back(waveLifts.back());waveEffects.push_back(std::nullopt);
    waveAllocations.push_back({Level1PresentationKind::entity,3});
    rejects([&] {level2PeriodicSceneOperations(data,view,waveLifts,waveEffects,profile,{256,-256,362},0xa000,waveAllocations);});
    for(const auto altitude:{45,90,990,1000}) {
        const FixedPosition position{-100,10000,altitude};
        const WorldSceneActor actor{{0,position},{},13,559,0,0};
        rejects([&] {level2CombatSceneOperations(data,view,{actor},{std::nullopt},profile,{256,-256,362},0xa000,{{Level1PresentationKind::entity,0}});});
        const auto scene=level2PeriodicSceneOperations(data,view,{actor},{std::nullopt},profile,{256,-256,362},0xa000,{{Level1PresentationKind::entity,0}});
        assert(scene.scene.centers.size()==1 && scene.scene.centers.front()==position);
        assert(!scene.scene.culled.front() && !scene.scene.operations.empty());
    }
    for(const auto altitude:{-1,1001}) {
        const WorldSceneActor actor{{0,{-100,10000,altitude}},{},13,559,0,0};
        rejects([&] {level2PeriodicSceneOperations(data,view,{actor},{std::nullopt},profile,{256,-256,362},0xa000,{{Level1PresentationKind::entity,0}});});
    }
    for(const std::int16_t type:{1,4,8,30}) {
        const WorldSceneActor actor{{0,{-100,10000,45}},{},type,data.models[type].collisionRadius,0,0};
        rejects([&] {level2PeriodicSceneOperations(data,view,{actor},{std::nullopt},profile,{256,-256,362},0xa000,{{Level1PresentationKind::entity,0}});});
    }
}
