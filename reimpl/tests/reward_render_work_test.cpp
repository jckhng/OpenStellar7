#include "stellar7/level1_deployment_render_work.hpp"
#include "stellar7/level1_frame.hpp"
#include <algorithm>
#include <cassert>
#include <memory>
#include <iterator>
#include <stdexcept>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace {
using namespace stellar7;
template<class Function> void rejects(Function function) {
    bool rejected=false;try { function(); } catch(const std::exception&) { rejected=true; }assert(rejected);
}
StellarRng rng() {
    std::array<std::uint16_t,StellarRng::wordCount> table{};
    for (std::size_t index=0;index<table.size();++index) { table[index]=static_cast<std::uint16_t>(index); }
    return {table,48,110,0,0};
}
Model3d guardianModel() {
    Model3d model{};model.flags=3;model.boundingRadius=16226;model.collisionRadius=2792;
    model.animationLimits={8,8,8};ModelLod lod{};
    constexpr std::array<unsigned,13> sizes{226,226,226,220,220,137,223,223,190,190,190,251,251};
    constexpr std::array<std::int16_t,13> offsets{2,2,2,0,0,0,0,0,0,1,1,1,1};
    for (std::size_t index=0;index<sizes.size();++index) {
        ModelGroup group{};group.vertexBlock=100+index;group.vertices.resize(sizes[index]);
        group.animationOffset=offsets[index];
        for (unsigned variant=0;variant<(index==9 || index==10 ? 4U : 8U);++variant) {
            ModelPrimitive primitive{};primitive.shapes={{0x80,{20,20,1,1},255,{1,2,3}}};
            group.primitives.push_back(primitive);
        }
        lod.groups.push_back(group);
    }
    model.lods={lod};return model;
}
Model3d rewardModel() {
    Model3d model{};model.flags=1;model.boundingRadius=2265;model.collisionRadius=897;
    model.animationLimits={4};ModelLod lod{};lod.flags=128;
    for (std::size_t index=0;index<6;++index) {
        ModelGroup group{};group.vertexBlock=100;group.vertices.resize(142);group.animationOffset=0;
        for (std::size_t variant=0;variant<4;++variant) {
            ModelPrimitive primitive{};primitive.shapes={{0x80,{20,20,1,1},255,{1,2,3}}};
            group.primitives.push_back(primitive);
        }
        lod.groups.push_back(group);
    }
    model.lods={lod};return model;
}
void guardianEffect() {
    std::vector<IndexedBitmap> frames(31,{4,4,std::vector<std::uint8_t>(16,7)});
    FailureEffectInput input{{0,6000,0},0,{0x88,{},0,31,32,0x0ac8},{24,294,24,145},{159,84},0xa000,1,0};
    rejects([&] { level1CombatEffectOperations(input,frames); });
    const auto terminal=level1GuardianEffectOperations(input,frames);
    assert(terminal.projected && terminal.retired && terminal.pixels.empty() && terminal.removal);
    assert(terminal.state.age==0 && terminal.state.endAge==0 && terminal.state.callback==0);
    const auto sentinel=uiEffectSentinelOperations({159,84},{24,294,24,145},0xa000);
    std::uint64_t ordinary=0;
    for (const auto operation:sentinel.operations) {
        if (operation.kind==NormalWorkKind::instructions) { ordinary+=operation.count; }
        else { assert(operation.kind==NormalWorkKind::repeatString && operation.count==5); }
    }
    assert(ordinary==103 && sentinel.effects.empty());
    input.state.age=13;input.state.endAge=17;
    const auto first=level1GuardianEffectOperations(input,frames);
    assert(first.state.age==14 && !first.retired && !first.pixels.empty());
    input.state.age=12;rejects([&] { level1GuardianEffectOperations(input,frames); });
    input.state.age=31;input.state.endAge=33;rejects([&] { level1GuardianEffectOperations(input,frames); });
    rejects([&] { uiEffectSentinelOperations({23,84},{24,294,24,145},0xa000); });
}
GameplayAssets assets() {
    GameplayAssets data{};data.models.resize(45);data.models[44]=guardianModel();
    data.models[1].collisionRadius=1342;data.models[11].collisionRadius=748;
    data.models[22].collisionRadius=134;data.models[25]=rewardModel();
    data.models[11].flags=1;data.models[11].animationLimits={8};data.models[29].collisionRadius=862;
    data.sharedModelRegistry={256,256};data.modelColorTables.resize(2);
    for (std::size_t index=0;index<256;++index) { data.modelColorTables[0][index]=static_cast<std::uint8_t>(index); }
    data.horizonFrames.assign(4,{256,16,std::vector<std::uint8_t>(4096,7)});data.artGroups.resize(5);
    data.artGroups[3]={{120,56,std::vector<std::uint8_t>(6720,7)},{4,1,{0,0,0,0}},{120,18,std::vector<std::uint8_t>(2160,3)}};
    data.artGroups[4].assign(15,{4,1,{0,0,0,0}});
    for (std::size_t index=7;index<15;++index) { data.artGroups[4][index]={8,1,std::vector<std::uint8_t>(8,13)}; }
    return data;
}
void animation() {
    const auto model=guardianModel();
    const auto expiry=level1GuardianAnimationOperations({2,0,0},model);
    assert(expiry.state.countdown==2 && expiry.state.variant==1 && expiry.operations.size()==1);
    assert(expiry.operations[0].count==346 && expiry.selectorWrites.size()==3);
    for (std::size_t index=0;index<3;++index) {
        const auto& write=expiry.selectorWrites[index];
        assert(write.index==index && write.value==1 && write.sourceInstruction==0x1358);
        assert(write.instructionOffset==157+88*index);
    }
    const auto wrap=level1GuardianAnimationOperations({1,0,7},model);
    assert(wrap.state.countdown==1 && wrap.state.variant==0 && wrap.operations[0].count==349);
    assert(wrap.selectorWrites.size()==6);
    for (std::size_t index=0;index<3;++index) {
        assert(wrap.selectorWrites[2*index].value==8 && wrap.selectorWrites[2*index+1].value==0);
        assert(wrap.selectorWrites[2*index].instructionOffset==157+89*index);
        assert(wrap.selectorWrites[2*index+1].instructionOffset==163+89*index);
    }
    const auto highDepth=level1DeploymentModelCallerOperations({0,80000,0},7);
    assert(highDepth.operations.front().count==27); //BA88 high depth bypasses the low-word clamp branch.
    const auto retained=level1GuardianAnimationOperations({1,2,7},model);
    assert(retained.state.countdown==1 && retained.state.variant==7 && retained.operations[0].count==15);
    assert(retained.selectorWrites.empty());
    rejects([&] { level1AnimationOperations({2,0,0},model); });
    auto wrong=model;wrong.animationLimits={8,7,8};rejects([&] { level1GuardianAnimationOperations({2,0,0},wrong); });
    rejects([&] { level1GuardianAnimationOperations({2,3,0},model); });
}
void beam() {
    const auto projection=level1BeamProjectionOperations({0,15528,5000},8,{17,18},19);
    assert(projection.projection.projected && projection.projection.flags==12);
    assert(projection.projection.projectedX==159 && projection.projection.projectedY==43);
    assert(projection.projection.projectedRadius==256 && projection.modelRadius==256);
    const auto work=level1CompletionBeamOperations(projection.projection,70,rng(),CompletionBeamView::main,PolygonClipMode::Enabled);
    assert(work.rng.calls()==10 && work.rng.indexA()==28 && work.rng.indexB()==90 && work.pixels.size()==55);
    const auto radar=level1CompletionBeamOperations(projection.projection,70,work.rng,CompletionBeamView::radar);
    assert(radar.rng.calls()==10 && radar.pixels.empty() && radar.operations.size()==1 && radar.operations[0].count==12);
    const auto skipped=level1BeamProjectionOperations({0,64,5000},12,{17,18},19);
    assert(!skipped.projection.projected && skipped.projection.flags==8);
    assert(skipped.projection.projectedX==17 && skipped.projection.projectedY==18 && skipped.projection.projectedRadius==19);
    const auto noBeam=level1CompletionBeamOperations(skipped.projection,70,rng(),CompletionBeamView::main);
    assert(noBeam.rng.calls()==0 && noBeam.pixels.empty() && noBeam.operations[0].count==15);
    auto edge=projection.projection;edge.projectedX=25;edge.projectedY=52;
    const auto unclipped=level1CompletionBeamOperations(edge,30,rng(),CompletionBeamView::main,PolygonClipMode::Disabled);
    assert(unclipped.pixels.size()==455);
    assert(std::any_of(unclipped.pixels.begin(),unclipped.pixels.end(),[](const auto& pixel) { return pixel.point.x==20; }));
    const auto clipped=level1CompletionBeamOperations(edge,30,rng(),CompletionBeamView::main,PolygonClipMode::Enabled);
    assert(clipped.pixels.size()==277);
    assert(std::all_of(clipped.pixels.begin(),clipped.pixels.end(),[](const auto& pixel) { return pixel.point.x>=24; }));
    rejects([&] { level1CompletionBeamOperations(edge,29,rng(),CompletionBeamView::main); });
    rejects([&] { level1BeamProjectionOperations({0,15528,5000},0,{17,18},19); });
}
void disabledGuardianRadar() {
    const auto data=assets();
    const IndexedBitmap panel{120,56,std::vector<std::uint8_t>(6720,7)};
    const RadarRetainedState state{680,42,{0xa400,0xa800,0xa000,0xa800,0xa400}};
    const WorldViewSetupInput view{{0,0,200000},{0xc000,0,0},{},0x88,7,{112,205,146,180},{256,-256,362}};
    std::vector<RadarActorInput> actors{{{0,{0,8000,0}},2792,44,-1,0,
        WorldActorRenderProfile::heapSelectorNoCallback,17,18,19}};
    const auto plan=[&] { return level1DeploymentCombatRadarOperations(state,view,{},0,actors,{},
        panel,{228,138},data.models[11],data.models[44],{Level1AnimationState{1,2,7}}); };
    const auto result=plan();
    assert(result.animations.size()==1 && result.animations[0]);
    assert(result.animations[0]->reload==1 && result.animations[0]->countdown==2 && result.animations[0]->variant==7);
    assert(result.radar.actors.size()==1 && result.radar.actors[0].flags==0);
    actors[0].renderProfile=WorldActorRenderProfile::heapSelectorDeferredAnimation;
    rejects([&] { plan(); }); //A disabled physical callback needs its matching render profile.
    actors[0].renderProfile=WorldActorRenderProfile::heapSelectorNoCallback;
    rejects([&] { level1ArrivalCombatRadarOperations(state,view,{},0,actors,{},panel,{228,138},
        data.models[11],{Level1AnimationState{1,2,7}}); });
}
void departureSetup() {
    Model3d model{};model.flags=1;model.boundingRadius=2522;model.collisionRadius=13052;
    ModelLod lod{};lod.flags=128;
    for (std::size_t index=0;index<9;++index) {
        ModelGroup group{};group.vertices={{{100,200,300}}};lod.groups.push_back(group);
    }
    model.lods={lod};
    const FixedPosition center{0,40000,10000};
    const WorldModelViewInput view{makeFixedMatrixQ14(0,0,0),center};
    const auto radius=level1DeploymentModelCullOperations(center,7).projectedRadius;
    const auto pitched=level1DeploymentModelSetupOperations(center,{0x900,0x100,0},radius,7,model,256,256,view);
    std::vector<NormalWorkOperation> canonical;
    for (const auto operation:pitched.operations) {
        if (operation.kind==NormalWorkKind::instructions && !canonical.empty()
            && canonical.back().kind==NormalWorkKind::instructions) {
            canonical.back().count+=operation.count;
        } else {
            canonical.push_back(operation);
        }
    }
    const auto copied=std::find_if(canonical.begin(),canonical.end(),[](const auto operation) {
        return operation.kind==NormalWorkKind::repeatString && operation.count==9;
    });
    assert(copied!=canonical.end() && std::distance(copied,canonical.end())>5);
    assert(copied[1].kind==NormalWorkKind::instructions && copied[1].count==170);
    assert(copied[5].kind==NormalWorkKind::instructions && copied[5].count==215);
    const auto direct=level1DeploymentModelSetupOperations(center,{0,0,0},radius,7,model,256,256,view);
    assert(direct.vertexPrecision==1 && direct.translation==FixedPosition({0,20000,5000}));
    assert(direct.groupDepths==std::vector<std::int32_t>(9,2750000));
}
void membership() {
    const auto data=assets();Level1State initial;initial.reloadAfterFailure(StellarRng{});
    auto actors=initial.entities();for (auto& actor:actors) { actor.position.y=-16384; }
    //Both physical ordinary slots are reused; no shadow dead Type11 record survives.
    auto& craft=actors[0];craft.type=7;craft.collisionRadius=13052;craft.stateBytes4dTo52[1]=0;
    craft.stateBytes4dTo52[5]=4;craft.renderProfile=WorldActorRenderProfile::inlineSelectorDeparture;
    auto& guardian=actors[1];guardian.type=44;guardian.collisionRadius=2792;guardian.position={0,-18000,0};
    guardian.pitch=guardian.roll=guardian.yaw=0;guardian.flags=0;guardian.primaryType11AnimationSelector=false;
    guardian.stateBytes4dTo52={255,15,2,0,5,0};guardian.modelAnimationVariant=0;
    guardian.animatedModelPresenterEnabled=true;guardian.renderProfile=WorldActorRenderProfile::heapSelectorDeferredAnimation;
    std::vector<Level1ObjectRef> order{{Level1PresentationKind::entity,2},{Level1PresentationKind::entity,3},
        {Level1PresentationKind::entity,4},{Level1PresentationKind::entity,5}};
    auto ground=initial.ground();auto random=initial.rng();auto player=initial.player();player.yaw=0;
    std::array<Level1Projectile,8> shots{};
    const Level1ProjectileRenderState input{ground,random,player,actors,shots,order,2};
    Level1CombatRenderContext context{order,{},Level1CombatLiftMembership::allowRetiredInitial,
        Level1CombatBonusMembership::none,Level1CombatArrivalMembership::deployment};
    auto pixels=std::make_unique<IndexedPages>();const auto pages=CockpitPageState::fromRecoveredMenu();
    RadarState radar(data.artGroups[3]);for (std::size_t index=0;index<actors.size();++index) { radar.initializeAllocation(index); }
    pixels->apply(radar.initializeLevel1(680,{},0,{pages.cachePage(),pages.backPage(),pages.frontPage(),pages.backPage(),pages.cachePage()}));
    const auto plan=[&] { return planLevel1EffectCombatFrame(data,input,context,{{140,4},false},680,{90,{}},pages,radar,*pixels); };
    //Constructor tail birth is absent from old45D2 until a real subsequent3768 rebuild.
    rejects([&] { plan(); });order.push_back({Level1PresentationKind::entity,1});
    auto result=std::unique_ptr<Level1CombatFramePlan>(new Level1CombatFramePlan(plan()));
    assert(result->frame.scene.animationStates.back()->variant==1 && result->frame.scene.animationStates.back()->countdown==2);
    assert(result->frame.animationsAfterRadar.back()->variant==1 && result->frame.animationsAfterRadar.back()->countdown==1);
    assert(result->frame.radarActors.back().type==44 && result->frame.radarActors.back().allocation==1);
    assert(result->frame.rngAfterScene && result->frame.beams.empty() && guardian.modelAnimationVariant==0);
    context.arrivalMembership=Level1CombatArrivalMembership::approachCraft;rejects([&] { plan(); });
    context.arrivalMembership=Level1CombatArrivalMembership::deployment;
    auto& shot=shots[0];shot.type=22;shot.collisionRadius=134;shot.position={0,-18000,2000};
    shot.pitch=0x1000;shot.rollVelocity=7;shot.xVelocity=3;shot.forwardVelocity=500;
    shot.stateBytes4dTo54={253,64,1,0,0,0,0,0};shot.ownerAllocationIndex=1;
    order.push_back({Level1PresentationKind::projectile,0});radar.initializeAllocation(16);
    result.reset(new Level1CombatFramePlan(plan()));
    assert(result->frame.radarActors.back().type==22 && result->frame.radarActors.back().allocation==16);
    assert(shot.rollVelocity==7 && shot.xVelocity==3 && shots[0].stateBytes4dTo54[1]==64);
    shot.stateBytes4dTo54[2]=0;rejects([&] { plan(); });shot.stateBytes4dTo54[2]=1;
    shot.ownerAllocationIndex=0;rejects([&] { plan(); });shot.ownerAllocationIndex=1;
    auto& reward=actors[5];reward.type=25;reward.collisionRadius=897;reward.flags=0;
    reward.position={0,-18000,0};reward.pitch=reward.roll=0;reward.yaw=0x2345;
    reward.pitchVelocity=reward.rollVelocity=0;reward.yawVelocity=2560;
    reward.stateBytes4dTo52={254,1,2,0,0,1};
    reward.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;
    result.reset(new Level1CombatFramePlan(plan()));
    assert(std::any_of(result->frame.radarActors.begin(),result->frame.radarActors.end(),[](const auto& actor) { return actor.type==25; }));
    assert(reward.stateBytes4dTo52[2]==2 && reward.yaw==0x2345);
    reward.stateBytes4dTo52[1]=0;rejects([&] { plan(); });reward.stateBytes4dTo52[1]=100;
    context.arrivalMembership=Level1CombatArrivalMembership::approachCraft;rejects([&] { plan(); });
    context.arrivalMembership=Level1CombatArrivalMembership::deployment;
    const auto departed=craft;
    while (actors.size()<17) { auto dead=departed;dead.allocationIndex=actors.size();actors.push_back(dead); }
    rejects([&] { plan(); }); //Entity16 has no qualified cache mapping; it must not alias shot0.
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    animation();beam();disabledGuardianRadar();departureSetup();guardianEffect();membership();return 0;
}
