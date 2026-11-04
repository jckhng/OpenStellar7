#include "stellar7/level1_projectile_render_work.hpp"
#include "stellar7/world_scene_work.hpp"
#include <cassert>
#include <stdexcept>

namespace {
using namespace stellar7;
template<class Action> void rejects(Action action) {
    bool rejected=false;
    try { action(); } catch (const std::invalid_argument&) { rejected=true; }
    assert(rejected);
}
void sameOperations(const WorldModelSetupWork& first,const WorldModelSetupWork& second) {
    assert(first.operations.size()==second.operations.size());
    for (std::size_t index=0;index<first.operations.size();++index) {
        assert(first.operations[index].kind==second.operations[index].kind);
        assert(first.operations[index].count==second.operations[index].count);
        assert(first.operations[index].value==second.operations[index].value);
    }
}
Model3d liftModel() {
    Model3d model{};model.flags=1;model.collisionRadius=748;model.boundingRadius=7282;
    model.animationLimits={8};
    ModelLod lod{10,128,{}};
    for (const auto origin:{std::array<std::int16_t,3>{0,-1000,0},
            {0,-200,-100},{2000,0,0},{-2000,0,0}}) {
        ModelGroup group{};group.animationOffset=-1;group.vertexBlock=lod.groups.size()+1;
        group.vertices={origin};group.primitives={ModelPrimitive{}};lod.groups.push_back(group);
    }
    model.lods={lod};return model;
}
WorldModelSetupWork setup(const Model3d& model,const FixedPosition center,
    const std::uint16_t yaw,const WorldModelViewInput& view) {
    const auto cull=level1CombatClippedLiftModelCullOperations(center);
    assert(!cull.culled);
    return level1CombatYawLiftModelSetupOperations(center,{0,0,yaw},cull.projectedRadius,model,256,256,view);
}
}
int main() {
    using namespace stellar7;
    const auto model=liftModel();
    const auto identity=makeFixedMatrixQ14(0,0,0);
    for (const auto altitude:{0,1,150,999,1000}) {
        for (const auto yaw:{std::uint16_t{0},std::uint16_t{16384},std::uint16_t{35584},std::uint16_t{65535}}) {
            const FixedPosition center{-3012,10772,altitude};
            const WorldModelViewInput view{identity,center};
            const auto work=setup(model,center,yaw,view);
            assert(work.translation==center && work.selectedLodIndex==0 && work.groupOrder.size()==4);
            assert(work.matrix.values==makeFixedMatrixQ14(0,0,yaw).values && work.vertexPrecision==0);
            if (altitude==0 || altitude==1000) {
                const auto legacy=worldModelSetupOperations(center,{0,0,yaw},9,11,model,256,256,view);
                sameOperations(work,legacy);
                assert(work.groupDepths==legacy.groupDepths && work.groupOrder==legacy.groupOrder);
            } else {
                rejects([&]{worldModelSetupOperations(center,{0,0,yaw},9,11,model,256,256,view);});
            }
            rejects([&]{level1CombatClippedLiftModelSetupOperations(center,{0,0,yaw},9,model,256,256,view);});
        }
    }
    for (const auto x:{-748,0,748}) {
        const FixedPosition center{x,10772,150};
        assert(setup(model,center,35584,{identity,center}).translation==center);
    }
    const FixedPosition actual{-3012,10772,150};
    const WorldModelViewInput view{identity,actual};
    assert(level1CombatClippedLiftModelCullOperations(actual).projectedRadius==9);
    const auto actualSetup=setup(model,actual,35584,view);
    assert(actualSetup.matrix.mode==1);
    for (const auto altitude:{-1,1001}) {
        const FixedPosition center{-3012,10772,altitude};
        rejects([&]{level1CombatYawLiftModelSetupOperations(center,{0,0,35584},9,model,256,256,{identity,center});});
    }
    rejects([&]{level1CombatYawLiftModelSetupOperations(actual,{1,0,35584},9,model,256,256,view);});
    rejects([&]{level1CombatYawLiftModelSetupOperations(actual,{0,1,35584},9,model,256,256,view);});
    rejects([&]{level1CombatYawLiftModelSetupOperations(actual,{0,0,35584},8,model,256,256,view);});
    rejects([&]{level1CombatYawLiftModelSetupOperations(actual,{0,0,35584},9,model,44,256,view);});
    auto wrong=model;wrong.collisionRadius=1342;
    rejects([&]{level1CombatYawLiftModelSetupOperations(actual,{0,0,35584},9,wrong,256,256,view);});
    wrong=model;wrong.scaleShift=1;
    rejects([&]{level1CombatYawLiftModelSetupOperations(actual,{0,0,35584},9,wrong,256,256,view);});
    rejects([&]{level1CombatYawLiftModelSetupOperations(actual,{0,0,35584},9,model,256,256,{makeFixedMatrixQ14(0,1,0),actual});});
    const FixedPosition wide{-3012,31500,150};
    rejects([&]{level1CombatYawLiftModelSetupOperations(wide,{0,0,35584},3,model,256,256,{identity,wide});});
    const auto camera=makeFixedMatrixQ14(0,0,65216);
    const auto cameraCenter=transformToView(actual,{{},camera,{},0});
    const auto cameraSetup=setup(model,cameraCenter,35584,{camera,actual});
    assert(cameraSetup.matrix.mode<=1 && cameraSetup.translation==cameraCenter);

    GameplayAssets assets;assets.models.resize(12);assets.models[11]=model;
    assets.modelColorTables.resize(2);
    const ViewTransform mainView{{},identity,{24,294,24,145},128};
    WorldSceneActor actor{{0,actual},{0,0,35584},11,748,0x130c,0,Level1AnimationState{5,4,2}};
    const auto scene=worldSceneOperations(assets,mainView,{actor},{256,256,0,0},
        {256,-256,362},WorldSceneMatrixProfile::level1Combat);
    assert(scene.centers.size()==1 && scene.centers[0]==actual && !scene.culled[0]);
    assert(scene.animationStates[0] && scene.animationStates[0]->countdown==3 && scene.animationStates[0]->variant==2);
    assert(scene.pixels.empty());
    rejects([&]{worldSceneOperations(assets,mainView,{actor},{256,256,0,0});});
}
