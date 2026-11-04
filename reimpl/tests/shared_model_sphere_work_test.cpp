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
std::uint64_t dispatches(const WorldModelCullWork& work) {
    std::uint64_t result=0;
    for(const auto& operation:work.operations){assert(operation.kind==NormalWorkKind::instructions);result+=operation.count;}
    return result;
}
Model3d projectile() {
    Model3d model{};model.collisionRadius=441;
    ModelGroup group{};group.vertexBlock=1;group.animationOffset=-1;
    group.vertices={{{0,0,0}}};group.primitives={ModelPrimitive{}};
    model.lods={{0,128,{group}}};return model;
}
}
int main() {
    using namespace stellar7;
    //Actual shared descriptors1/11/17/20/23/29; the sphere kernel has no type switch.
    for(const auto radius:{134U,441U,748U,862U,1342U}) {
        auto model=projectile();model.collisionRadius=radius;
        const FixedPosition outside{static_cast<std::int32_t>(radius+2000),10000,0};
        const auto source=sharedModelSphereCullOperations(outside,model);
        const auto legacy=worldModelCullOperations(outside,radius);
        assert(source.culled==legacy.culled && source.clipped==legacy.clipped
            && source.projectedRadius==legacy.projectedRadius && dispatches(source)==dispatches(legacy));
        for(const auto x:{-static_cast<std::int32_t>(radius),-1,0,1,static_cast<std::int32_t>(radius)}) {
            const FixedPosition center{x,10000,0};
            const auto work=sharedModelSphereCullOperations(center,model);
            assert(!work.culled && !work.clipped && work.projectedRadius==radius*128/(10000-radius));
            if(radius==134) {
                const auto primary=level1ProjectileModelCullOperations(center);
                assert(primary.culled==work.culled && primary.clipped==work.clipped
                    && primary.projectedRadius==work.projectedRadius && dispatches(primary)==dispatches(work));
            }
            rejects([&]{worldModelCullOperations(center,radius);});
        }
    }
    const auto model=projectile();
    const FixedPosition actual{-112,753,0};
    const auto cull=sharedModelSphereCullOperations(actual,model);
    //CB7A/4ABF signed-X and signed-Z branches, then CE9A right clipping.
    assert(!cull.culled && cull.clipped && cull.projectedRadius==180 && dispatches(cull)==623);
    rejects([&]{level1CombatProjectileModelCullOperations(actual,17);});
    const auto identity=makeFixedMatrixQ14(0,0,0);
    const WorldModelViewInput view{identity,actual};
    const auto setup=level1SharedCombatProjectileModelSetupOperations(actual,{0,0,50378},180,17,model,256,256,view);
    assert(setup.selectedLodIndex==0 && setup.groupOrder==std::vector<std::size_t>{0}
        && setup.matrix.mode==1 && setup.translation==actual);
    rejects([&]{level1CombatProjectileModelSetupOperations(actual,{0,0,50378},180,17,model,256,256,view);});
    rejects([&]{level1SharedCombatProjectileModelSetupOperations(actual,{0,0,50378},179,17,model,256,256,view);});
    rejects([&]{level1SharedCombatProjectileModelSetupOperations(actual,{1,0,50378},180,17,model,256,256,view);});
    rejects([&]{level1SharedCombatProjectileModelSetupOperations(actual,{0,1,50378},180,17,model,256,256,view);});
    rejects([&]{level1SharedCombatProjectileModelSetupOperations(actual,{0,0,50378},180,29,model,256,256,view);});
    for(const auto center:{FixedPosition{0,-1000,0},FixedPosition{0,-400,0}}) {
        assert(sharedModelSphereCullOperations(center,model).culled);
    }
    const auto near=sharedModelSphereCullOperations({0,100,0},model);
    assert(!near.culled && near.clipped && near.projectedRadius==256);
    for(const auto change:{0,1,2,3,4}) {
        auto bad=model;
        if(change==0){bad.collisionRadius=0;}else if(change==1){bad.collisionRadius=16300;}
        else if(change==2){bad.scaleShift=1;}else if(change==3){bad.origin[0]=1;}else{bad.flags=2;}
        rejects([&]{sharedModelSphereCullOperations(actual,bad);});
    }
    rejects([&]{sharedModelSphereCullOperations({31560,753,0},model);});
    GameplayAssets assets;assets.models.resize(18);assets.models[17]=model;assets.modelColorTables.resize(2);
    WorldSceneActor actor{{0,actual},{0,0,50378},17,441,0,0};actor.inlineAnimation=std::array<std::uint8_t,6>{};
    const ViewTransform main{{},identity,{24,294,24,145},128};
    const auto scene=worldSceneOperations(assets,main,{actor},{256,256,0,0},
        {256,-256,362},WorldSceneMatrixProfile::level1Combat);
    assert(scene.centers.size()==1 && scene.centers[0]==actual && !scene.culled[0] && scene.pixels.empty());
    rejects([&]{worldSceneOperations(assets,main,{actor},{256,256,0,0});});
}
