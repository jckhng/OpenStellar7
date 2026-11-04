#include "stellar7/level1_arrival_render_work.hpp"
#include "stellar7/level1_frame.hpp"
#include <algorithm>
#include <cassert>
#include <memory>
#include <stdexcept>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif
namespace {
using namespace stellar7;
template<class Function>void rejects(Function function) {
    bool rejected=false;try { function(); }catch(const std::exception&) { rejected=true; }assert(rejected);
}
Model3d craftModel() {
    Model3d model{};model.flags=1;model.boundingRadius=2522;model.collisionRadius=13052;
    model.animationLimits={4};ModelLod lod{};lod.flags=128;
    constexpr std::array<std::uint8_t,9> origins{0,19,26,35,44,57,68,152,157};
    for (std::size_t index=0;index<origins.size();++index) {
        ModelGroup group{};group.vertexBlock=101;group.originVertex=origins[index];group.vertices.resize(179);
        group.animationOffset=index==6 ? 0 : -1;
        group.vertices[1]={-256,0,-256};group.vertices[2]={256,0,-256};group.vertices[3]={0,0,256};
        for (unsigned variant=0;variant<(index==6 ? 4U : 1U);++variant) {
            ModelPrimitive primitive{};const auto color=static_cast<std::uint8_t>(20+variant);
            primitive.shapes={{0x80,{color,color,1,1},255,{1,2,3}}};group.primitives.push_back(primitive);
        }
        lod.groups.push_back(group);
    }
    model.lods={lod};return model;
}
GameplayAssets data() {
    GameplayAssets result{};result.models.resize(44);result.models[7]=craftModel();
    result.models[1].collisionRadius=1342;result.models[11].collisionRadius=748;
    result.models[17].collisionRadius=441;
    result.models[11].flags=1;result.models[11].animationLimits={8};result.models[29].collisionRadius=862;
    result.modelColorTables.resize(2);result.sharedModelRegistry={256,256};
    for (std::size_t i=0;i<256;++i) { result.modelColorTables[0][i]=static_cast<std::uint8_t>(i); }
    result.horizonFrames.assign(4,{256,16,std::vector<std::uint8_t>(4096,7)});result.artGroups.resize(5);
    result.artGroups[3]={{120,56,std::vector<std::uint8_t>(6720,7)},{4,1,{0,0,0,0}},{120,18,std::vector<std::uint8_t>(2160,3)}};
    result.artGroups[4].assign(15,{4,1,{0,0,0,0}});
    for (std::size_t i=7;i<15;++i) { result.artGroups[4][i]={8,1,std::vector<std::uint8_t>(8,13)}; }
    return result;
}
void leaves() {
    auto assets=data();const auto& model=assets.models[7];
    assert(level1ArrivalModelCullOperations({0,-28672,5000}).culled);
    const FixedPosition center{0,28672,5000};const auto cull=level1ArrivalModelCullOperations(center);
    assert(!cull.culled&&cull.clipped);
    rejects([&] { worldModelCullOperations(center,13052); });
    const WorldModelViewInput view{makeFixedMatrixQ14(0,0,0),center};
    const std::array<std::uint16_t,3> bank{0,0x500,0};
    const auto setup=level1ArrivalModelSetupOperations(center,bank,cull.projectedRadius,model,256,256,view);
    assert(setup.vertexPrecision==1&&setup.matrix.mode==2&&setup.translation.y==14336&&setup.groupOrder.size()==9);
    auto input=worldModelGeometryInput(assets,setup,center,PolygonClipMode::Enabled);
    input.matrixProfile=WorldModelMatrixProfile::level1Arrival;input.animationSelectors={3};
    const auto geometry=level1ArrivalModelGeometryOperations(model.lods[0],input,bank,view);
    assert(geometry.polygons.size()==9);
    unsigned words=0,byte=0;
    for (const auto& polygon:geometry.polygons) {
        assert(polygon.points.size()==3);
        for (const auto& op:polygon.prefix) {
            assert(op.count<1000000);
            if (op.kind==NormalWorkKind::repeatString&&op.count==89) { ++words; }
            if (op.kind==NormalWorkKind::repeatString&&op.count==1) { ++byte; }
        }
    }
    assert(words==1&&byte==1);
    auto legacy=input;legacy.matrixProfile=WorldModelMatrixProfile::identityOrYaw;
    rejects([&] { worldModelGeometryOperations(model.lods[0],legacy); });
    rejects([&] { worldModelSetupOperations(center,bank,cull.projectedRadius,7,model,256,256,view); });
    auto bad=input;bad.animationSelectors={4};
    rejects([&] { level1ArrivalModelGeometryOperations(model.lods[0],bad,bank,view); });
    bad=input;bad.matrix.values[0]^=1;
    rejects([&] { level1ArrivalModelGeometryOperations(model.lods[0],bad,bank,view); });
    bad=input;++bad.translation.y;
    rejects([&] { level1ArrivalModelGeometryOperations(model.lods[0],bad,bank,view); });
    auto beamBank=bank;beamBank[0]=1;
    rejects([&] { level1ArrivalModelSetupOperations(center,beamBank,cull.projectedRadius,model,256,256,view); });
    rejects([&] { level1ArrivalModelCullOperations({0,20000,4999}); });
}
void membership() {
    auto assets=data();Level1State initial;initial.reloadAfterFailure(StellarRng{});
    auto actors=initial.entities();for (auto& actor:actors) { actor.position.y=-16384; }
    auto player=initial.player();player.yaw=0;
    auto& craft=actors[0];craft.type=7;craft.position={0,-28672,5000};craft.collisionRadius=13052;
    craft.pitch=craft.roll=craft.yaw=0;craft.flags=0;craft.stateBytes4dTo52={255,100,0,0,5,0};
    craft.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;
    actors[1].stateBytes4dTo52[1]=0;
    const std::vector<Level1ObjectRef> order{{Level1PresentationKind::entity,0},
        {Level1PresentationKind::entity,2},{Level1PresentationKind::entity,3},
        {Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5}};
    auto ground=initial.ground();auto rng=initial.rng();std::array<Level1Projectile,8> shots{};
    const Level1ProjectileRenderState input{ground,rng,player,actors,shots,order,2};
    Level1CombatRenderContext context{order,{},Level1CombatLiftMembership::allowRetiredInitial};
    const auto pages=CockpitPageState::fromRecoveredMenu();auto pixels=std::make_unique<IndexedPages>();
    RadarState radar(assets.artGroups[3]);for (std::size_t i=0;i<actors.size();++i) { radar.initializeAllocation(i); }
    pixels->apply(radar.initializeLevel1(680,{},0,{pages.cachePage(),pages.backPage(),pages.frontPage(),pages.backPage(),pages.cachePage()}));
    const auto plan=[&]() { return planLevel1EffectCombatFrame(assets,input,context,{{140,4},false},680,
        {90,{}},pages,radar,*pixels); };
    rejects([&] { plan(); });context.arrivalMembership=Level1CombatArrivalMembership::approachCraft;
    auto result=std::unique_ptr<Level1CombatFramePlan>(new Level1CombatFramePlan(plan()));
    assert(result->frame.radarActors.size()==5&&result->frame.radarActors[0].type==7);
    assert(result->frame.radarActors[0].callback==0&&result->frame.radar.enemyFlag==0);
    assert(!result->frame.scene.animationStates[0]&&craft.stateBytes4dTo52[2]==0&&rng.calls()==16);
    auto shotOrder=order;shotOrder.push_back({Level1PresentationKind::projectile,0});
    auto& shot=shots[0];shot.type=17;shot.collisionRadius=441;shot.position={0,-16384,0};
    shot.forwardVelocity=500;shot.stateBytes4dTo54={253,54,0,0,0,0,0,0};
    shot.ownerIsPlayer=false;shot.ownerAllocationIndex=0;radar.initializeAllocation(16);
    const Level1ProjectileRenderState shotInput{ground,rng,player,actors,shots,shotOrder,2};
    Level1CombatRenderContext shotContext{shotOrder,{},Level1CombatLiftMembership::allowRetiredInitial,
        Level1CombatBonusMembership::none,Level1CombatArrivalMembership::approachCraft};
    auto withShot=std::unique_ptr<Level1CombatFramePlan>(new Level1CombatFramePlan(
        planLevel1EffectCombatFrame(assets,shotInput,shotContext,{{140,4},false},680,{90,{}},pages,radar,*pixels)));
    assert(withShot->frame.radarActors.back().type==17&&shot.ownerAllocationIndex==0);
    shot.ownerAllocationIndex=2;
    rejects([&] { planLevel1EffectCombatFrame(assets,shotInput,shotContext,{{140,4},false},680,{90,{}},pages,radar,*pixels); });
    shot.stateBytes4dTo54[1]=0;
    context.arrivalMembership=static_cast<Level1CombatArrivalMembership>(99);rejects([&] { plan(); });
    context.arrivalMembership=Level1CombatArrivalMembership::approachCraft;
    craft.stateBytes4dTo52[5]=3;craft.flags=8;rejects([&] { plan(); });
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    leaves();membership();return 0;
}
