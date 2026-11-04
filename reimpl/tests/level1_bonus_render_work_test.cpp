#include "stellar7/level1_bonus_render_work.hpp"
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
template<class Function> void rejects(Function function) {
    bool rejected=false;
    try { function(); } catch(const std::exception&) { rejected=true; }
    assert(rejected);
}
Model3d model() {
    Model3d result{};
    result.flags=1;result.collisionRadius=330;result.boundingRadius=1895;
    ModelLod lod{};lod.flags=128;
    for(unsigned index=0;index<5;++index) {
        ModelGroup group{};
        group.animationOffset=index==0?0:-1;
        group.originVertex=static_cast<std::uint8_t>(index==0?0:112+5*index);
        group.vertexBlock=74819;group.vertices.resize(137);
        group.vertices[1]={-256,0,-256};group.vertices[2]={256,0,-256};group.vertices[3]={0,0,256};
        for(unsigned variant=0;variant<(index==0?5U:1U);++variant) {
            ModelPrimitive primitive{};
            const auto color=static_cast<std::uint8_t>(index==0?20+variant:40+index);
            primitive.shapes={{0x80,{color,color,1,1},255,{1,2,3}}};
            group.primitives.push_back(primitive);
        }
        lod.groups.push_back(group);
    }
    result.lods={lod};
    return result;
}
std::uint64_t instructions(const WorldModelGeometryWork& work) {
    std::uint64_t total=0;
    const auto add=[&](const auto& ops) { for(const auto& op:ops) {
        assert(op.count<1000000);
        if(op.kind==NormalWorkKind::instructions) { total+=op.count; }
    }};
    for(const auto& p:work.polygons) { add(p.prefix); } add(work.suffix);return total;
}
void leaves() {
    const auto immutable=model();
    const FixedPosition position{2000,8000,100};
    const WorldModelViewInput view{makeFixedMatrixQ14(0,0,0),position};
    const auto cull=level1BonusModelCullOperations(position);
    assert(!cull.culled&&!cull.clipped&&cull.projectedRadius==5);
    rejects([&] { worldModelCullOperations(position,330); });
    const auto setup=level1BonusModelSetupOperations(position,{},cull.projectedRadius,immutable,256,256,view);
    assert(setup.selectedLodIndex==0&&setup.groupOrder.size()==5);
    rejects([&] { worldModelSetupOperations(position,{},cull.projectedRadius,42,immutable,256,256,view); });
    WorldModelGeometryInput input{};
    input.matrix=setup.matrix;input.translation=setup.translation;input.groupOrder=setup.groupOrder;
    input.fullCenter=position;input.matrixProfile=WorldModelMatrixProfile::level1InlineSpin;
    input.animationSelectors={7};
    for(std::size_t i=0;i<256;++i) { input.colorRemap[i]=static_cast<std::uint8_t>(i); }
    const auto fallback=level1BonusModelGeometryOperations(immutable.lods[0],input,{},view);
    assert(fallback.polygons.size()==5&&fallback.polygons[0].primaryColor==20);
    auto first=input;first.animationSelectors={0};
    const auto direct=worldModelGeometryOperations(immutable.lods[0],first);
    assert(instructions(fallback)==instructions(direct)+1); //CF30 fallback, not a fitted total.
    unsigned clearWords=0,clearByte=0;
    for(const auto& polygon:fallback.polygons) { for(const auto& op:polygon.prefix) {
        if(op.kind==NormalWorkKind::repeatString&&op.count==68) { ++clearWords; }
        if(op.kind==NormalWorkKind::repeatString&&op.count==1) { ++clearByte; }
    }}
    assert(clearWords==1&&clearByte==1);
    auto variant=input;variant.animationSelectors={3};
    const auto selected=level1BonusModelGeometryOperations(immutable.lods[0],variant,{},view);
    assert(selected.polygons.front().primaryColor==23);
    rejects([&] { level1BonusModelGeometryOperations(immutable.lods[0],first,{},view); });
    auto badModel=immutable;badModel.lods[0].groups[1].vertexBlock=123;
    rejects([&] { level1BonusModelGeometryOperations(badModel.lods[0],input,{},view); });
    const std::array<std::uint16_t,3> spin{0,0x800,0x800};
    const auto spun=level1BonusModelSetupOperations(position,spin,cull.projectedRadius,immutable,256,256,view);
    assert(spun.matrix.mode==2);
    auto invalidAngles=spin;invalidAngles[0]=1;
    rejects([&] { level1BonusModelSetupOperations(position,invalidAngles,cull.projectedRadius,immutable,256,256,view); });
    auto badAltitude=position;badAltitude.z=101;
    rejects([&] { level1BonusModelCullOperations(badAltitude); });
    const auto camera=makeFixedMatrixQ14(0,0,65216);
    const WorldModelViewInput turned{camera,position};
    const auto center=transformToView(position,{{},camera,{},0});
    const auto turnedCull=level1BonusModelCullOperations(center);
    const auto combined=level1BonusModelSetupOperations(center,spin,turnedCull.projectedRadius,immutable,256,256,turned);
    assert(combined.matrix.mode==3&&combined.matrix.values==composeFixedMatricesQ14(makeFixedMatrixQ14(0,0x800,0x800),camera,0x5240).values);
    auto wrong=input;wrong.matrix=combined.matrix;wrong.translation=center;
    const auto geometry=level1BonusModelGeometryOperations(immutable.lods[0],wrong,spin,turned);
    assert(!geometry.polygons.empty());
    wrong.matrix.values[0]^=1;
    rejects([&] { level1BonusModelGeometryOperations(immutable.lods[0],wrong,spin,turned); });
}
GameplayAssets assets() {
    GameplayAssets result;result.horizonFrames.assign(4,{256,16,std::vector<std::uint8_t>(4096,7)});
    result.artGroups.resize(5);
    result.artGroups[3]={{120,56,std::vector<std::uint8_t>(6720,7)},{4,1,{0,0,0,0}},{120,18,std::vector<std::uint8_t>(2160,3)}};
    result.artGroups[4].assign(15,{4,1,{0,0,0,0}});
    for(std::size_t i=7;i<15;++i) { result.artGroups[4][i]={8,1,std::vector<std::uint8_t>(8,13)}; }
    result.models.resize(43);result.models[1].collisionRadius=1342;result.models[29].collisionRadius=862;
    result.models[11].collisionRadius=748;result.models[11].flags=1;result.models[11].animationLimits={8};
    result.models[42]=model();result.sharedModelRegistry={256,256};result.modelColorTables.resize(2);
    for(std::size_t i=0;i<256;++i) { result.modelColorTables[0][i]=static_cast<std::uint8_t>(i); }
    return result;
}
void frame() {
    auto data=assets();Level1State state;state.reloadAfterFailure(StellarRng{});
    auto actors=state.entities();for(auto& actor:actors) { actor.position.y=-16384; }
    Level1Entity bonus{};bonus.allocationIndex=6;bonus.type=42;bonus.collisionRadius=330;
    bonus.position={2000,8000,100};bonus.rollVelocity=bonus.yawVelocity=0x800;
    bonus.stateBytes4dTo52={254,1,7,0,0,7};bonus.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;
    actors.push_back(bonus);
    std::vector<Level1ObjectRef> refs;for(std::size_t i=0;i<actors.size();++i) { refs.push_back({Level1PresentationKind::entity,i}); }
    auto ground=state.ground();auto rng=state.rng();std::array<Level1Projectile,8> shots{};
    const Level1ProjectileRenderState input{ground,rng,state.player(),actors,shots,refs,2};
    Level1CombatRenderContext context{refs,{}};
    const auto pages=CockpitPageState::fromRecoveredMenu();auto pixels=std::make_unique<IndexedPages>();
    RadarState radar(data.artGroups[3]);for(std::size_t i=0;i<actors.size();++i) { radar.initializeAllocation(i); }
    pixels->apply(radar.initializeLevel1(680,{},0,{pages.cachePage(),pages.backPage(),pages.frontPage(),pages.backPage(),pages.cachePage()}));
    const CockpitSettings settings{90,{}};const Level1PresentationState presentation{{140,4},false};
    rejects([&] { planLevel1EffectCombatFrame(data,input,context,presentation,680,settings,pages,radar,*pixels); });
    context.bonusMembership=Level1CombatBonusMembership::oneType42;
    auto planned=std::unique_ptr<Level1CombatFramePlan>(new Level1CombatFramePlan(planLevel1EffectCombatFrame(data,input,context,presentation,680,settings,pages,radar,*pixels)));
    assert(planned->frame.radarActors.size()==7&&!planned->frame.scene.animationStates.back());
    assert(planned->frame.radarActors.back().type==42&&planned->frame.radarActors.back().callback==0);
    assert(planned->frame.animationAfterRadar.countdown==4&&rng.calls()==16&&actors.back().roll==0);
    const RadarRetainedState retained{680,42,{pages.cachePage(),pages.backPage(),pages.frontPage(),pages.backPage(),pages.cachePage()}};
    const RadarActorInput bonusInput{{0,bonus.position},330,42,-2,0,WorldActorRenderProfile::inlineSelectorNoCallback,17,18,19};
    const auto single=level1BonusCombatRadarOperations(retained,radar.view(),{},0,{bonusInput},{},
        data.artGroups[3][0],{228,138},data.models[11],{std::nullopt});
    assert(single.animations.size()==1&&!single.animations[0]&&single.radar.enemyFlag==1&&single.radar.cpuWorkQualified);
    rejects([&] { level1CombatEffectRadarOperations(retained,radar.view(),{},0,{bonusInput},{},data.artGroups[3][0],{228,138},data.models[11],{std::nullopt}); });
    auto callback=bonusInput;callback.callback=0x130c;
    rejects([&] { level1BonusCombatRadarOperations(retained,radar.view(),{},0,{callback},{},data.artGroups[3][0],{228,138},data.models[11],{std::nullopt}); });
    rejects([&] { level1BonusCombatRadarOperations(retained,radar.view(),{},0,{bonusInput,bonusInput},{},data.artGroups[3][0],{228,138},data.models[11],{std::nullopt,std::nullopt}); });
    rejects([&] { level1BonusCombatRadarOperations(retained,radar.view(),{},0,{bonusInput},{},data.artGroups[3][0],{228,138},data.models[11],{Level1AnimationState{5,3,1}}); });
    auto noOption=context;noOption.bonusMembership=static_cast<Level1CombatBonusMembership>(99);
    rejects([&] { planLevel1EffectCombatFrame(data,input,noOption,presentation,680,settings,pages,radar,*pixels); });
    actors.back().stateBytes4dTo52[2]=2;
    rejects([&] { planLevel1EffectCombatFrame(data,input,context,presentation,680,settings,pages,radar,*pixels); });
    actors.back().stateBytes4dTo52[2]=7;actors.back().pitch=1;
    rejects([&] { planLevel1EffectCombatFrame(data,input,context,presentation,680,settings,pages,radar,*pixels); });
}
} // namespace
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    leaves();frame();return 0;
}
