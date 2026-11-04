#include "stellar7/combat_scene_work.hpp"
#include "stellar7/level1_owned_view_work.hpp"
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
    GameplayAssets result{};
    result.models.resize(44);result.modelColorTables.resize(2);
    for(std::size_t index=0;index<256;++index) {result.modelColorTables[0][index]=static_cast<std::uint8_t>(index);}
    ModelGroup group{};group.animationOffset=-1;group.vertexBlock=1;
    group.vertices={{{0,-256,0}},{{-10,100,0}},{{10,100,0}},{{0,100,10}}};
    ModelPrimitive primitive{};primitive.shapes={{0x80,{8,8,1,1},255,{1,2,3}}};group.primitives={primitive};
    for(const std::int16_t type:{1,4,8,13,30}) {
        auto& model=result.models[type];model.flags=1;
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
    for(const std::int16_t type:{1,4,8,13,30}) {
        const auto& model=data.models[type];const FixedPosition center{0,40000,0};
        const WorldSceneActor actor{{0,center},{},type,model.collisionRadius,0,0};
        rejects([&] {worldModelCallerOperations(center,model.collisionRadius,0,0);});
        const auto caller=level1OwnedWideModelCallerOperations(center,model,0,0);
        assert(caller.minimumDepth>=32768);
        rejects([&] {level2CombatSceneOperations(data,view,{actor},{std::nullopt},profile,{256,-256,362},0xa000,{{Level1PresentationKind::entity,0}});});
        const auto scene=level2PeriodicSceneOperations(data,view,{actor},{std::nullopt},profile,{256,-256,362},0xa000,{{Level1PresentationKind::entity,0}});
        assert(scene.scene.centers.size()==1 && scene.scene.centers.front()==center);
        assert(scene.scene.culled.size()==1 && !scene.scene.culled.front());
        assert(!scene.scene.operations.empty());
    }
    const WorldSceneActor newType{{0,{0,10000,0}},{},4,928,0,0};
    rejects([&] {level2CombatSceneOperations(data,view,{newType},{std::nullopt},profile,{256,-256,362},0xa000,{{Level1PresentationKind::entity,0}});});
    assert(!level2PeriodicSceneOperations(data,view,{newType},{std::nullopt},profile,{256,-256,362},0xa000,{{Level1PresentationKind::entity,0}}).scene.operations.empty());
}

