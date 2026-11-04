#include "stellar7/level1_deployment_render_work.hpp"
#include "stellar7/level1_frame.hpp"
#include "stellar7/combat_scene_work.hpp"
#include "../src/level1_departure_model.hpp"
#include <algorithm>
#include <cassert>
#include <memory>
#include <iterator>
#include <iostream>
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
GameplayAssets assets() {
    GameplayAssets data{};data.models.resize(45);data.models[44]=guardianModel();
    data.models[1].collisionRadius=1342;data.models[11].collisionRadius=748;
    data.models[22].collisionRadius=134;
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
Model3d departureFixtureModel() {
    Model3d model{};model.flags=1;model.boundingRadius=2522;model.collisionRadius=13052;
    ModelLod lod{};lod.flags=128;
    for (std::size_t index=0;index<9;++index) {
        ModelGroup group{};group.vertexBlock=100+index;group.vertices.resize(179);
        group.vertices[1]={-1000,0,-1000};group.vertices[2]={1000,0,-1000};group.vertices[3]={0,0,1000};
        group.animationOffset=index==6 ? 0 : -1;
        for (std::size_t variant=0;variant<(index==6 ? 4U : 1U);++variant) {
            ModelPrimitive primitive{};primitive.shapes={{0x80,{20,20,1,1},255,{1,2,3}}};
            group.primitives.push_back(primitive);
        }
        lod.groups.push_back(group);
    }
    model.lods={lod};return model;
}
void departureExtentScene() {
    auto data=assets();data.models[7]=departureFixtureModel();
    const auto view=worldViewSetupOperations({{}, {}, {0,0,1008},0x88,7,{24,294,24,145},{256,-256,362}});
    const FixedPosition raw{-11271,50798,19791};
    const auto center=transformToView(raw,view.view);
    assert(center==FixedPosition({-6320,51646,19791}));
    const auto cull=level1DeploymentModelCullOperations(center,7);
    assert(!cull.culled && cull.clipped && cull.projectedRadius==43);
    const WorldModelViewInput modelView{view.view.matrix,raw};
    const std::array<std::uint16_t,3> angles{5904,61725,6272};
    rejects([&] {level1DeploymentModelSetupOperations(center,angles,cull.projectedRadius,7,
        data.models[7],256,256,modelView);}); //The historical leaf keeps its declared64k boundary.
    const std::vector<WorldSceneActor> actors{{{4,raw},angles,7,13052,0,0,
        std::nullopt,std::nullopt,std::uint8_t{0},std::nullopt}};
    const auto scene=level1DeploymentCombatSceneOperations(data,view.view,actors,{std::nullopt},
        {256,256,0,0},{static_cast<std::int16_t>(view.transformedLight.x),
            static_cast<std::int16_t>(view.transformedLight.y),static_cast<std::int16_t>(view.transformedLight.z)},
        0xa000,{{Level1PresentationKind::entity,0}},rng());
    assert(scene.scene.centers==std::vector<FixedPosition>{center});
    assert(scene.scene.polygonCounts==std::vector<std::size_t>{9});
    assert(!scene.scene.pixels.empty() && scene.rngAfterMain && scene.rngAfterMain->calls()==0);
    const FixedPosition adjacentRaw{-12285,52200,20890};
    const auto adjacentCenter=transformToView(adjacentRaw,view.view);
    assert(adjacentCenter==FixedPosition({-7194,53139,20890}));
    const std::vector<WorldSceneActor> adjacentActors{{{4,adjacentRaw},{6048,61707,6528},7,13052,0,0,
        std::nullopt,std::nullopt,std::uint8_t{0},std::nullopt}};
    const auto adjacent=level1DeploymentCombatSceneOperations(data,view.view,adjacentActors,{std::nullopt},
        {256,256,0,0},{static_cast<std::int16_t>(view.transformedLight.x),
            static_cast<std::int16_t>(view.transformedLight.y),static_cast<std::int16_t>(view.transformedLight.z)},
        0xa000,{{Level1PresentationKind::entity,0}},*scene.rngAfterMain);
    assert(adjacent.scene.centers==std::vector<FixedPosition>{adjacentCenter});
    assert(adjacent.scene.polygonCounts==std::vector<std::size_t>{9});
    assert(adjacent.rngAfterMain && adjacent.rngAfterMain->calls()==0 && !adjacent.scene.pixels.empty());
}
void departureNormalization() {
    auto data=assets();data.models[7]=departureFixtureModel();const auto& model=data.models[7];
    const auto view=worldViewSetupOperations({{}, {}, {0,0,1008},0x88,7,{24,294,24,145},{256,-256,362}});
    const WorldModelViewInput modelView{view.view.matrix,{-11271,50798,19791}};
    const FixedPosition center{-6320,51646,19791};
    const std::array<std::uint16_t,3> angles{5904,61725,6272};
    const auto cull=level1DeploymentModelCullOperations(center,7);
    const auto setup=level1DepartureModelSetupOperations(center,angles,cull.projectedRadius,model,256,256,modelView);
    assert(setup.vertexPrecision==1 && setup.translation==FixedPosition({-1580,12911,4947}));
    auto input=worldModelGeometryInput(data,setup,center,PolygonClipMode::Enabled);
    input.matrixProfile=WorldModelMatrixProfile::level1Deployment;input.animationSelectors={0};
    const auto geometry=level1DepartureModelGeometryOperations(model.lods[0],input,angles,modelView);
    assert(geometry.polygons.size()==9 && geometry.polygons[0].points.size()==3);
    const auto point=transformToView({-1000,0,-1000},{{},setup.matrix,{},0});
    const auto expected=ProjectedPoint{static_cast<std::int16_t>(159+(std::int64_t{point.x}+center.x)*128/(point.y+center.y)),
        static_cast<std::int16_t>(84-(std::int64_t{point.z}+center.z)*128/(point.y+center.y))};
    assert(geometry.polygons[0].points[0]==expected); //55EB uses full center, independently of the sort receipt.
    auto wrong=input;wrong.translation={-3160,25823,9895};
    rejects([&]{level1DepartureModelGeometryOperations(model.lods[0],wrong,angles,modelView);});
    wrong=input;wrong.fullCenter.y+=1;
    rejects([&]{level1DepartureModelGeometryOperations(model.lods[0],wrong,angles,modelView);});
    auto bad=model;bad.flags=2;
    rejects([&]{level1DepartureModelSetupOperations(center,angles,cull.projectedRadius,bad,256,256,modelView);});
    rejects([&]{level1DepartureModelSetupOperations(center,angles,cull.projectedRadius,model,255,256,modelView);});
    const auto count=[](const auto& work) {std::uint64_t n=0;for(const auto& op:work.operations){n+=op.count;}return n;};
    for (const auto axis:{-32768,-32767,32767,32768}) {
        const FixedPosition at{axis,40000,0};const WorldModelViewInput flat{makeFixedMatrixQ14(0,0,0),at};
        const auto radius=level1DeploymentModelCullOperations(at,7).projectedRadius;
        const auto fresh=level1DepartureModelSetupOperations(at,{0,0,0},radius,model,256,256,flat);
        const auto legacy=level1DeploymentModelSetupOperations(at,{0,0,0},radius,7,model,256,256,flat);
        assert(count(fresh)+(axis==-32768 ? 2U : 0U)==count(legacy));
        assert(fresh.groupDepths==legacy.groupDepths && fresh.groupOrder==legacy.groupOrder);
        assert(fresh.translation==FixedPosition({axis>=0 ? axis/4 : -((-axis+3)/4),10000,0}));
    }
    for (const auto axis:{-98304,-65536,65536}) {
        const FixedPosition at{axis,50000,0};const WorldModelViewInput flat{makeFixedMatrixQ14(0,0,0),at};
        rejects([&]{level1DepartureModelSetupOperations(at,{0,0,0},43,model,256,256,flat);});
    }
    //Identity actors sort in raw actor coordinates; precision0 vertices still translate in view coordinates.
    const FixedPosition nearRaw{5000,10000,5000};
    const auto nearCenter=transformToView(nearRaw,view.view);
    const WorldModelViewInput nearView{view.view.matrix,nearRaw};
    const auto nearCull=level1DeploymentModelCullOperations(nearCenter,7);
    const auto nearSetup=level1DepartureModelSetupOperations(nearCenter,{0,0,0},nearCull.projectedRadius,model,256,256,nearView);
    assert(nearSetup.vertexPrecision==0 && nearSetup.translation==nearRaw);
    auto nearInput=worldModelGeometryInput(data,nearSetup,nearCenter,PolygonClipMode::Enabled);
    nearInput.matrixProfile=WorldModelMatrixProfile::level1Deployment;nearInput.animationSelectors={0};
    const auto nearGeometry=level1DepartureModelGeometryOperations(model.lods[0],nearInput,{0,0,0},nearView);
    const auto oldSetup=level1DeploymentModelSetupOperations(nearCenter,{0,0,0},nearCull.projectedRadius,7,model,256,256,nearView);
    auto oldInput=nearInput;oldInput.translation=oldSetup.translation;
    const auto oldGeometry=level1DeploymentModelGeometryOperations(model.lods[0],oldInput,{0,0,0},nearView,7);
    assert(nearGeometry.polygons.size()==oldGeometry.polygons.size());
    for (std::size_t index=0;index<nearGeometry.polygons.size();++index) {
        assert(nearGeometry.polygons[index].points==oldGeometry.polygons[index].points);
        const auto& fresh=nearGeometry.polygons[index].prefix;const auto& old=oldGeometry.polygons[index].prefix;
        assert(fresh.size()==old.size());
        for (std::size_t operation=0;operation<fresh.size();++operation) {
            assert(fresh[operation].kind==old[operation].kind && fresh[operation].count==old[operation].count
                && fresh[operation].value==old[operation].value);
        }
    }
    //Setup normalization is qualified; projection retains its explicit unsigned-word divisor boundary.
    const FixedPosition far{0,65535,0};const WorldModelViewInput flat{makeFixedMatrixQ14(0,0,0),far};
    const auto farCull=level1DeploymentModelCullOperations(far,7);
    const auto farSetup=level1DepartureModelSetupOperations(far,{0,0,0},farCull.projectedRadius,model,256,256,flat);
    auto farInput=worldModelGeometryInput(data,farSetup,far,PolygonClipMode::Disabled);
    farInput.matrixProfile=WorldModelMatrixProfile::level1Deployment;farInput.animationSelectors={0};
    auto farLod=model.lods[0];for(auto& group:farLod.groups){group.vertices[1][1]=1;}
    rejects([&]{level1DepartureModelGeometryOperations(farLod,farInput,{0,0,0},flat);});
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
void signedDeploymentEffect() {
    auto data=assets();
    data.models[7].flags=1;data.models[7].collisionRadius=13052;data.models[7].boundingRadius=2522;
    data.models[7].lods={{0,128,std::vector<ModelGroup>(9)}};
    data.artGroups.resize(6);data.artGroups[5].assign(40,{8,5,std::vector<std::uint8_t>(40,7)});
    Level1State initial;initial.reloadAfterFailure(StellarRng{});
    auto actors=initial.entities();for(auto& actor:actors) {actor.position.y=-16384;}
    auto retired=actors[1];retired.allocationIndex=6;retired.stateBytes4dTo52[1]=0;
    retired.primaryType11AnimationSelector=false;
    retired.renderProfile=WorldActorRenderProfile::heapSelectorDeferredAnimation;
    actors.push_back(retired);
    //The real recycled identities remain craft0/guardian1; no shadow primary lift.
    auto& craft=actors[0];craft.type=7;craft.collisionRadius=13052;craft.flags=0;
    craft.position={-723,-30000,5343};craft.pitch=1728;craft.roll=63338;craft.yaw=64384;
    craft.stateBytes4dTo52={255,100,0,0,5,4};
    craft.animatedModelPresenterEnabled=false;
    craft.renderProfile=WorldActorRenderProfile::inlineSelectorDeparture;
    auto& guardian=actors[1];guardian.type=44;guardian.collisionRadius=2792;guardian.flags=0;
    guardian.position={0,-18000,0};guardian.pitch=guardian.roll=0;guardian.yaw=33648;
    guardian.stateBytes4dTo52={255,13,2,1,5,0};guardian.modelAnimationVariant=1;
    guardian.primaryType11AnimationSelector=false;guardian.animatedModelPresenterEnabled=false;
    guardian.renderProfile=WorldActorRenderProfile::heapSelectorNoCallback;
    std::vector<Level1ObjectRef> order{{Level1PresentationKind::entity,0},{Level1PresentationKind::entity,5},
        {Level1PresentationKind::entity,2},{Level1PresentationKind::entity,3},
        {Level1PresentationKind::entity,4},{Level1PresentationKind::entity,1},
        {Level1PresentationKind::deathEffect,0}};
    Level1CombatRenderContext context{order,{},Level1CombatLiftMembership::allowRetiredInitial,
        Level1CombatBonusMembership::none,Level1CombatArrivalMembership::deployment};
    const FixedPosition impact{-215,2713,-356};
    context.effects.push_back({0,{0x88,impact},0,{0x88,{},0,1,12,0x0ac8},0});
    auto ground=initial.ground();auto random=initial.rng();auto player=initial.player();player.yaw=0;
    std::array<Level1Projectile,8> shots{};
    Level1ProjectileRenderState input{ground,random,player,actors,shots,order,2};
    auto pixels=std::make_unique<IndexedPages>();auto pages=CockpitPageState::fromRecoveredMenu();
    for(const std::uint16_t page:{0xa000,0xa400,0xa800}) {
        for(std::int16_t y=0;y<200;++y) {
            for(std::int16_t x=0;x<320;++x) {pixels->write(page,{x,y},93);}
        }
    }
    RadarState radar(data.artGroups[3]);
    for(std::size_t index=0;index<actors.size();++index) {radar.initializeAllocation(index);}
    radar.initializeAllocation(32);
    pixels->apply(radar.initializeLevel1(680,{},0,
        {pages.cachePage(),pages.backPage(),pages.frontPage(),pages.backPage(),pages.cachePage()}));
    Level1PresentationState presentation{{140,4},false};
    bool retiredEffect=false;
    for(unsigned frame=0;frame<11;++frame) {
        const auto incoming=context.effects.front().state.age;
        auto result=std::unique_ptr<Level1CombatFramePlan>(new Level1CombatFramePlan(
            planLevel1EffectCombatFrame(data,input,context,presentation,680,{90,{}},pages,radar,*pixels)));
        assert(result->effects.size()==1&&result->effects[0].poolSlot==0);
        const auto& effect=result->effects[0].work;
        const auto terminal=incoming+1==12;
        assert(effect.retired==terminal&&effect.cachedCenter==impact);
        assert(effect.state.age==(terminal?0:incoming+1));
        assert(effect.state.callback==(terminal?0:0x0ac8));
        assert(result->frame.radarActors.front().allocation==0&&result->frame.radarActors.front().type==7);
        assert(result->frame.radarActors.back().allocation==32&&result->frame.radarActors.back().type==-1);
        assert(result->frame.radarActors.back().actor.position==impact);
        assert(result->frame.radarActors.back().callback==effect.state.callback);
        assert(result->frame.radar.actors.size()==order.size());
        assert(result->frame.radar.actors.back().center==FixedPosition({impact.x,200000-impact.z,impact.y}));
        assert(result->frame.animationsAfterRadar[5]->countdown==1
            &&result->frame.animationsAfterRadar[5]->variant==1);
        assert(result->frame.rngAfterScene&&result->frame.rngAfterScene->calls()==result->frame.ground.rng.calls());
        assert(result->allocationOrderAfterMain.size()==order.size()-(terminal?1:0));
        assert(result->frame.radarState.projection(32).cachedCenter==FixedPosition({impact.x,200000-impact.z,impact.y}));
        ground=result->frame.ground.ground;random=*result->frame.rngAfterScene;
        presentation=result->frame.presentationAfter;radar=result->frame.radarState;
        *pixels=result->frame.pixels;pages=result->frame.pages;pages.flip();pages.beginFrame();
        ++input.loopTick;
        if(terminal) {
            assert(effect.removal&&effect.removal->removedIndex==6);
            order=result->allocationOrderAfterMain;
            assert(context.allocationOrder==order&&order.size()==6); //Context retains a reference to order.
            context.effects.clear();retiredEffect=true;break;
        }
        context.effects[0].state=effect.state;context.effects[0].actor.flags=effect.state.flags;
        const auto& retained=radar.projection(32);
        assert(retained.flags==effect.state.flags);
        context.effects[0].state.projected={retained.projectedX,retained.projectedY};
        context.effects[0].state.projectedRadius=retained.projectedRadius;
    }
    assert(retiredEffect);
    const auto after=std::unique_ptr<Level1CombatFramePlan>(new Level1CombatFramePlan(
        planLevel1EffectCombatFrame(data,input,context,presentation,680,{90,{}},pages,radar,*pixels)));
    assert(after->effects.empty()&&after->frame.radarActors.size()==6);
    assert(after->frame.radarState.projection(32).cachedCenter==FixedPosition({impact.x,200000-impact.z,impact.y}));

    const IndexedBitmap panel{120,56,std::vector<std::uint8_t>(6720,7)};
    const RadarRetainedState state{680,42,{0xa400,0xa800,0xa000,0xa800,0xa400}};
    const WorldViewSetupInput view{{0,0,200000},{0xc000,0,0},{},0x88,7,{112,205,146,180},{256,-256,362}};
    std::vector<RadarActorInput> effect{{{0x8c,impact},0,-1,-4,0x0ac8,
        WorldActorRenderProfile::bitmapEffectCallback,17,18,0}};
    const auto draw=[&] {return level1DeploymentCombatRadarOperations(state,view,{},0,effect,{},panel,
        {228,138},data.models[11],data.models[44],{std::nullopt});};
    for(const auto z:{-65535,-356,-301,-300,-1,0,1000,1001,65535}) {
        effect[0].actor.position.z=z;const auto result=draw();
        assert(result.radar.cpuWorkQualified&&result.radar.actors[0].flags==0x8c);
        assert(result.radar.actors[0].center==FixedPosition({impact.x,200000-z,impact.y}));
        assert(!result.animations[0]);
    }
    for(const auto z:{-65536,65536}) {effect[0].actor.position.z=z;rejects([&]{draw();});}
    effect[0].actor.position=impact;effect[0].callback=0;
    assert(draw().radar.cpuWorkQualified); //Terminal OLD-list entry does not run0AC8 again.
    effect[0].callback=0x130c;rejects([&]{draw();});effect[0].callback=0x0ac8;
    effect[0].radius=1;rejects([&]{draw();});effect[0].radius=0;
    effect[0].renderProfile=WorldActorRenderProfile::noHandles;rejects([&]{draw();});
    effect[0].renderProfile=WorldActorRenderProfile::bitmapEffectCallback;
    effect[0].category=-3;rejects([&]{draw();});effect[0].category=-4;
    rejects([&]{level1CombatEffectRadarOperations(state,view,{},0,effect,{},panel,{228,138},data.models[11],{std::nullopt});});
    rejects([&]{level1ArrivalCombatRadarOperations(state,view,{},0,effect,{},panel,{228,138},data.models[11],{std::nullopt});});
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
    animation();beam();disabledGuardianRadar();departureSetup();membership();
    try {departureExtentScene();departureNormalization();} catch(const std::exception& error) {
        std::cerr << "departure extent main scene: " << error.what() << '\n';return 1;
    }
    try {signedDeploymentEffect();} catch(const std::exception& error) {
        std::cerr << "signed deployment FC: " << error.what() << '\n';return 1;
    }
    return 0;
}
