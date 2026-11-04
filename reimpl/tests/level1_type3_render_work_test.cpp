#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif
#include "stellar7/combat_scene_work.hpp"
#include "stellar7/level1_owned_view_work.hpp"
#include "stellar7/radar_state.hpp"
#include "stellar7/world_traversal.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace {
using namespace stellar7;
template<class Action> void rejects(Action action) {
    bool rejected=false;try {action();} catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
}
GameplayAssets assets() {
    GameplayAssets a{};a.models.resize(44);
    auto& m=a.models[3];m.flags=1;m.collisionRadius=770;m.boundingRadius=3029;
    ModelGroup group{};group.vertexBlock=3;group.animationOffset=0;
    group.vertices={{{-40,0,-20}},{{40,0,-20}},{{0,0,40}}};
    ModelPrimitive first{},second{};
    first.shapes={{0x80,{72,72,0,0},255,{0,1,2}}};
    second.shapes={{0x80,{73,73,0,0},255,{0,1,2}}};
    group.primitives={first,second};
    //A null source selector owns variant0 even for an immutable offset0 group.
    m.lods={{10,128,{group}},{30,128,{group}},{255,128,{group}}};
    a.modelColorTables.resize(2);
    for(unsigned i=0;i<256;++i) {a.modelColorTables[0][i]=static_cast<std::uint8_t>(i);}
    return a;
}
WorldViewSetupInput radarView() {
    return {{0,0,200000},{0xc000,0,0},{0,0,0},0x88,7,{112,205,146,180},{256,-256,362}};
}
}
int main(int argc,char** argv) {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace stellar7;
    const auto data=assets();const auto view=initialLevel1MainView();
    const std::vector<Level1ObjectRef> allocations{{Level1PresentationKind::entity,6}};
    for(const auto yaw:{std::uint16_t{0},std::uint16_t{0x8000},std::uint16_t{0x9123}}) {
        const WorldSceneActor actor{{0,{0,10000,0}},{0,0,yaw},3,770,0,0};
        const auto work=level1Type3CombatSceneOperations(data,view,{actor},{std::nullopt},
            {256,256,0,0},{256,-256,362},0xa800,allocations,Level1OrdinaryRenderFamily::combatEffects);
        assert(work.scene.centers.size()==1 && !work.scene.culled[0]);
        assert(!work.scene.animationStates[0] && work.effects.empty());
        for(const auto pixel:work.scene.pixels) {assert(pixel.color==72);}
        rejects([&]{level1CombatSceneOperations(data,view,{actor},{std::nullopt},
            {256,256,0,0},{256,-256,362},0xa800,allocations);});
    }
    const WorldSceneActor behind{{0,{0,-10000,0}},{},3,770,0,0};
    assert(level1Type3CombatSceneOperations(data,view,{behind},{std::nullopt},
        {256,256,0,0},{256,-256,362},0xa800,allocations,Level1OrdinaryRenderFamily::combatEffects).scene.culled[0]);
    auto bad=behind;bad.heapAnimation=Level1AnimationState{5,3,1};
    rejects([&]{level1Type3CombatSceneOperations(data,view,{bad},{std::nullopt},
        {256,256,0,0},{256,-256,362},0xa800,allocations,Level1OrdinaryRenderFamily::combatEffects);});
    bad=behind;bad.collisionRadius=1342;
    rejects([&]{level1Type3CombatSceneOperations(data,view,{bad},{std::nullopt},
        {256,256,0,0},{256,-256,362},0xa800,allocations,Level1OrdinaryRenderFamily::combatEffects);});
    //Precision1 is an explicit owned family; the precision0 helper stays strict.
    const WorldSceneActor wideActor{{0,{0,32000,0}},{},3,770,0,0};
    const auto wideScene=level1Type3CombatSceneOperations(data,view,{wideActor},{std::nullopt},
        {256,256,0,0},{256,-256,362},0xa800,allocations,Level1OrdinaryRenderFamily::combatEffects);
    assert(wideScene.scene.centers.size()==1 && !wideScene.scene.culled[0]);
    assert(wideScene.scene.polygonCounts[0]==1 && !wideScene.scene.animationStates[0]);
    assert(wideScene.effects.empty());
    for(const auto pixel:wideScene.scene.pixels){assert(pixel.color==72);}
    const WorldModelViewInput wideView{view.matrix,wideActor.actor.position};
    const auto& wideModel=data.models[3];
    const auto wideCull=level1OwnedWideModelCullOperations(wideActor.actor.position,wideModel);
    assert(!wideCull.culled && wideCull.projectedRadius==3);
    rejects([&]{level1OwnedModelSetupOperations(wideActor.actor.position,{},3,wideModel,256,256,wideView);});
    rejects([&]{sharedModelSphereCullOperations(wideActor.actor.position,wideModel);});
    const auto wideSetup=level1OwnedWideModelSetupOperations(wideActor.actor.position,{},3,wideModel,256,256,wideView);
    assert(wideSetup.vertexPrecision==1 && wideSetup.selectedLodIndex==0);
    assert((wideSetup.translation==FixedPosition{0,16000,0}));
    const auto wideInput=worldModelGeometryInput(data,wideSetup,wideActor.actor.position,PolygonClipMode::Disabled);
    assert(wideInput.animationSelectors.empty());
    const auto wideGeometry=level1OwnedWideModelGeometryOperations(wideModel.lods[0],wideInput,{},wideView);
    assert(wideGeometry.polygons.size()==1 && wideGeometry.polygons[0].primaryColor==72);
    rejects([&]{level1OwnedModelGeometryOperations(wideModel.lods[0],wideInput,{},wideView);});
    const RadarRetainedState state{600,37,{0xa400,0xa800,0xa000,0xa800,0xa400}};
    const IndexedBitmap panel{120,56,std::vector<std::uint8_t>(120*56,7)};
    Model3d model11{};model11.flags=1;model11.animationLimits={8};
    const std::vector<RadarActorInput> actors{{{0,{0,10000,0}},770,3,-1,0,WorldActorRenderProfile::noHandles,0,0,0}};
    const auto radar=level1Type3RadarOperations(state,radarView(),{},0,actors,{},panel,{228,138},
        model11,nullptr,{std::nullopt},Level1OrdinaryRenderFamily::combatEffects);
    assert(radar.radar.cpuWorkQualified && radar.radar.actors.size()==1 && !radar.animations[0]);
    rejects([&]{level1CombatEffectRadarOperations(state,radarView(),{},0,actors,{},panel,{228,138},model11,{std::nullopt});});
    RadarState owned({panel});owned.initializeAllocation(6);owned.initializeLevel1(600,{},0,state.pages);
    const RadarStateActor ownedActor{6,actors[0].actor,770,3,-1,0,WorldActorRenderProfile::noHandles};
    const auto retained=owned.drawLevel1Type3Combat(600,{},0,{ownedActor},state.pages,{},model11,
        nullptr,{std::nullopt},Level1OrdinaryRenderFamily::combatEffects);
    assert(retained.radar.cpuWorkQualified);
    assert(owned.projection(6).cachedCenter==retained.radar.actors[0].cachedCenter);
    assert(owned.projection(6).cachedCenter.x==0);

    //Optional root resource pass exercises the actual immutable Type3 descriptor.
    if(argc==2) {
        std::ifstream file(argv[1],std::ios::binary);assert(file);
        const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        auto actual=data;actual.models=decodeFirstModelTableResource(bytes);
        assert(actual.models.size()==44 && actual.models[3].collisionRadius==770 && actual.models[3].flags==1);
        assert(actual.models[3].lods.size()==3 && actual.models[3].lods[0].groups.size()==6
            && actual.models[3].lods[1].groups.size()==9 && actual.models[3].lods[2].groups.size()==12);
        for(const auto y:{1000,6000,20000,32000}) {
            const WorldSceneActor actor{{0,{0,y,0}},{0,0,0x8000},3,770,0,0};
            const auto frame=level1Type3CombatSceneOperations(actual,view,{actor},{std::nullopt},
                {256,256,0,0},{256,-256,362},0xa800,allocations,Level1OrdinaryRenderFamily::combatEffects);
            assert(frame.scene.centers.size()==1 && !frame.scene.culled[0]);
        }
    }
}
