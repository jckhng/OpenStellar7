#include "stellar7/gameplay.hpp"
#include "stellar7/combat_scene_work.hpp"
#include "stellar7/level1_owned_view_work.hpp"
#include "stellar7/world_model_work.hpp"
#include "stellar7/world_scene_work.hpp"
#include "stellar7/world_traversal.hpp"
#include "stellar7/raster_line_work.hpp"
#include "stellar7/failure_model_work.hpp"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif
#include <stdexcept>
using namespace stellar7;
namespace {
template<class Action> void rejects(Action action) {bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}assert(rejected);}
std::vector<NormalWorkOperation> canonical(const std::vector<NormalWorkOperation>& input) {
    std::vector<NormalWorkOperation> result;
    for(const auto& operation:input) {
        if(operation.kind==NormalWorkKind::instructions && !result.empty() && result.back().kind==operation.kind) {result.back().count+=operation.count;}
        else {result.push_back(operation);}
    }
    return result;
}
void same(const std::vector<NormalWorkOperation>& a,const std::vector<NormalWorkOperation>& b) {
    const auto left=canonical(a),right=canonical(b);assert(left.size()==right.size());
    for(std::size_t i=0;i<left.size();++i){assert(left[i].kind==right[i].kind && left[i].count==right[i].count && left[i].value==right[i].value);}
}
ModelLod lod(std::uint16_t threshold,std::size_t count) {
    ModelGroup group{};group.animationOffset=-1;group.vertexBlock=10;
    group.vertices={{{0,-256,0}},{{-10,100,0}},{{10,100,0}},{{0,100,10}}};
    ModelPrimitive primitive{};primitive.shapes={{0x80,{8,8,1,1},255,{1,2,3}}};group.primitives={primitive};
    return {threshold,128,std::vector<ModelGroup>(count,group)};
}
GameplayAssets assets() {
    GameplayAssets data{};data.modelColorTables.resize(2);
    for(std::size_t i=0;i<256;++i){data.modelColorTables[0][i]=static_cast<std::uint8_t>(i);}
    return data;
}
void constructorAxis(const GameplayAssets& data) {
    for(const std::int16_t type:{8,13,30}) {
        //Actual shared header metadata; synthetic groups isolate sphere/setup routing.
        Model3d model{};model.collisionRadius=type==8?860:type==13?559:868;
        model.flags=type==8?1:0;model.boundingRadius=type==8?3236:type==13?9:8;
        model.lods={lod(255,2)};
        for(const std::uint16_t cameraYaw:{0,80}) {
            auto matrix=makeFixedMatrixQ14(0,0,cameraYaw);transpose(matrix);
            for(const auto x:{-static_cast<std::int32_t>(model.collisionRadius)/2,0,
                    static_cast<std::int32_t>(model.collisionRadius)/2}) {
                const WorldModelViewInput view{matrix,{x,10000,0}};
                const auto center=transformToView(view.relativePosition,{{},matrix,{},0});
                assert(std::abs(std::int64_t{center.x})<=model.collisionRadius);
                const auto sphere=sharedModelSphereCullOperations(center,model);
                rejects([&]{worldModelCullOperations(center,model.collisionRadius);});
                const auto setup=level2ConstructorModelSetupOperations(center,{},sphere.projectedRadius,
                    type,model,256,256,view);
                assert(!sphere.culled && !sphere.clipped && setup.vertexPrecision==0 && setup.translation==center);
                auto sceneAssets=data;sceneAssets.models.resize(31);sceneAssets.models[type]=model;
                const ViewTransform sceneView{{},matrix,{24,294,24,145},128};
                const WorldSceneActor actor{{0,view.relativePosition},{},type,model.collisionRadius,0,0};
                const auto scene=worldSceneOperations(sceneAssets,sceneView,{actor},{256,256,0,0},
                    {256,-256,362},WorldSceneMatrixProfile::level2Constructor);
                assert(scene.centers.size()==1 && scene.centers[0]==center && !scene.culled[0]);
                assert(!scene.operations.empty() && !scene.animationStates[0]);
                rejects([&]{worldSceneOperations(sceneAssets,sceneView,{actor},{256,256,0,0});});
            }
        }
        const auto identity=makeFixedMatrixQ14(0,0,0);
        auto lineModel=model;
        for(auto& group:lineModel.lods.front().groups) {
            group.primitives.front().shapes.front().vertexIndices={1,2};
        }
        auto lineAssets=data;lineAssets.models.resize(31);lineAssets.models[type]=lineModel;
        const ViewTransform nearView{{},identity,{24,294,24,145},128};
        const FixedPosition nearCenter{0,1500,0};
        assert(sharedModelSphereCullOperations(nearCenter,lineModel).clipped);
        const WorldSceneActor lineActor{{0,nearCenter},{},type,model.collisionRadius,0,0};
        const auto line=worldSceneOperations(lineAssets,nearView,{lineActor},{256,256,0,0},
            {256,-256,362},WorldSceneMatrixProfile::level2Constructor);
        assert(!line.culled.front() && !line.pixels.empty());
        const auto lineSphere=sharedModelSphereCullOperations(nearCenter,lineModel);
        const WorldModelViewInput lineView{identity,nearCenter};
        const auto lineSetup=level2ConstructorModelSetupOperations(nearCenter,{},lineSphere.projectedRadius,
            type,lineModel,256,256,lineView);
        auto lineInput=worldModelGeometryInput(lineAssets,lineSetup,nearCenter,PolygonClipMode::Enabled);
        lineInput.matrixProfile=WorldModelMatrixProfile::level2Constructor;
        const auto lineGeometry=level2ConstructorModelGeometryOperations(lineModel.lods.front(),lineInput,{},lineView,type);
        auto cursor=line.operations.begin();std::size_t pixels=0;
        for(const auto& polygon:lineGeometry.polygons) {
            assert(polygon.points.size()==2 && polygon.filled && polygon.primaryColor==polygon.secondaryColor);
            rejects([&]{collapsedPolygonOperations(polygon.points,nearView.viewport,PolygonClipMode::Enabled);});
            const auto child=ownedPolygonOperations(polygon.points,nearView.viewport,PolygonClipMode::Enabled,
                FailurePolygonStyle::filled,polygon.primaryColor,polygon.secondaryColor);
            const auto found=std::search(cursor,line.operations.end(),child.operations.begin(),child.operations.end(),
                [](const auto& a,const auto& b){return a.kind==b.kind && a.count==b.count && a.value==b.value;});
            assert(found!=line.operations.end());cursor=found+child.operations.size();pixels+=child.pixels.size();
        }
        assert(line.pixels.size()==pixels);
        const FixedPosition axis{0,10000,0};const WorldModelViewInput view{identity,axis};
        const auto radius=sharedModelSphereCullOperations(axis,model).projectedRadius;
        rejects([&]{level2ConstructorModelSetupOperations(axis,{1,0,0},radius,type,model,256,256,view);});
        rejects([&]{level2ConstructorModelSetupOperations(axis,{},radius,11,model,256,256,view);});
        rejects([&]{level2ConstructorModelSetupOperations(axis,{},radius-1,type,model,256,256,view);});
        auto bad=model;bad.scaleShift=1;
        rejects([&]{level2ConstructorModelSetupOperations(axis,{},radius,type,bad,256,256,view);});
        bad=model;bad.flags=2;
        rejects([&]{level2ConstructorModelSetupOperations(axis,{},radius,type,bad,256,256,view);});
        bad=model;++bad.collisionRadius;
        rejects([&]{level2ConstructorModelSetupOperations(axis,{},radius,type,bad,256,256,view);});
        //The precision1 branch still uses its old sphere admission, including axis rejection.
        const FixedPosition wideAxis{0,33000,0};const WorldModelViewInput wideView{identity,wideAxis};
        const auto wideRadius=worldModelCallerOperations(wideAxis,model.collisionRadius,0,0).projectedRadius;
        rejects([&]{level2ConstructorModelSetupOperations(wideAxis,{},wideRadius,type,model,256,256,wideView);});
    }
}

void periodicNearType13(const GameplayAssets& data) {
    auto sceneAssets=data;sceneAssets.models.resize(31);
    Model3d model{};model.collisionRadius=559;model.boundingRadius=9;
    auto sourceLod=lod(255,1);auto& group=sourceLod.groups.front();
    group.vertices={{{0,0,0}},{{0,-1024,-128}},{{512,-1024,128}},{{0,0,16384}}};
    group.primitives.front().shapes.front().vertexIndices={0,1,2};model.lods={sourceLod};
    sceneAssets.models[13]=model;
    auto camera=makeFixedMatrixQ14(0,0,51584);transpose(camera);
    const ViewTransform view{{},camera,{24,294,24,145},128};
    const std::array<std::uint16_t,3> angles{0,0,9472};
    const FixedPosition position{-307,919,0},center{-966,-87,0};
    assert(transformToView(position,view)==center);
    const WorldSceneActor actor{{0,position},angles,13,model.collisionRadius,0,0};
    const WorldSceneProfile profile{256,256,0,0};
    const std::array<std::int16_t,3> light{256,-256,362};
    const std::vector<Level1ObjectRef> allocations{{Level1PresentationKind::entity,0}};
    rejects([&] {level2CombatSceneOperations(sceneAssets,view,{actor},{std::nullopt},profile,light,0xa000,allocations);});
    const auto actual=level2PeriodicSceneOperations(sceneAssets,view,{actor},{std::nullopt},profile,light,0xa000,allocations);
    assert(actual.scene.centers.front()==center && !actual.scene.culled.front());
    const auto first=worldFirstCenterOperations(view,actor.actor,WorldSourceFlagsDomain::flagsWithoutCallback);
    const auto caller=worldModelCallerOperations(center,model.collisionRadius,0,0,WorldSourceFlagsDomain::flagsWithoutCallback);
    const auto cull=sharedModelSphereCullOperations(center,model);
    assert(!cull.culled && cull.clipped && cull.projectedRadius==256);
    const WorldModelViewInput modelView{camera,position};
    const auto setup=level1OwnedModelSetupOperations(center,angles,cull.projectedRadius,model,256,256,modelView);
    auto input=worldModelGeometryInput(sceneAssets,setup,center,PolygonClipMode::Enabled);input.light=light;
    const auto geometry=level1OwnedModelGeometryOperations(sourceLod,input,angles,modelView);
    assert(geometry.polygons.size()==1 && geometry.polygons.front().points.size()==4);
    std::vector<NormalWorkOperation> expected;
    const auto append=[&](const auto& operations) {expected.insert(expected.end(),operations.begin(),operations.end());};
    append(first.operations);append(caller.operations);append(cull.operations);append(setup.operations);
    std::vector<WorldPixelWrite> pixels;
    for(const auto& polygon:geometry.polygons) {
        append(polygon.prefix);
        const auto raster=ownedPolygonOperations(polygon.points,view.viewport,PolygonClipMode::Enabled,
            polygon.filled ? FailurePolygonStyle::filled : FailurePolygonStyle::outline,
            polygon.primaryColor,polygon.secondaryColor);
        append(raster.operations);pixels.insert(pixels.end(),raster.pixels.begin(),raster.pixels.end());
    }
    append(geometry.suffix);append(worldModelReturnOperations(WorldActorContinuation::returnToCaller));
    same(actual.scene.operations,expected);
    assert(!pixels.empty() && pixels.size()==actual.scene.pixels.size());
    for(std::size_t index=0;index<pixels.size();++index) {
        assert(pixels[index].point==actual.scene.pixels[index].point && pixels[index].color==actual.scene.pixels[index].color);
    }
    auto strictInput=input;strictInput.matrixProfile=WorldModelMatrixProfile::level2Constructor;
    rejects([&] {level2ConstructorModelGeometryOperations(sourceLod,strictInput,angles,modelView,13);});
    const ViewTransform flat{{},makeFixedMatrixQ14(0,0,0),view.viewport,128};
    for(const auto depth:{-1000,10000}) {
        const WorldSceneActor control{{0,{0,depth,0}},{},13,model.collisionRadius,0,0};
        const auto old=level2CombatSceneOperations(sceneAssets,flat,{control},{std::nullopt},profile,light,0xa000,allocations);
        const auto admitted=level2PeriodicSceneOperations(sceneAssets,flat,{control},{std::nullopt},profile,light,0xa000,allocations);
        same(old.scene.operations,admitted.scene.operations);
        assert(old.scene.culled.front()==(depth<0) && old.scene.culled==admitted.scene.culled);
        assert(old.scene.pixels.size()==admitted.scene.pixels.size());
    }
}

}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    const auto data=assets();
    try {constructorAxis(data);periodicNearType13(data);} catch(const std::exception& error) {
        std::fprintf(stderr,"Level2 constructor axis: %s\n",error.what());return 1;
    }
    for(const std::int16_t type:{8,13,30}) {
        Model3d model{};model.collisionRadius=type==8?860:type==13?559:868;model.lods={lod(255,2)};
        for(const std::uint16_t cameraYaw:{0,80}) {
            auto matrix=makeFixedMatrixQ14(0,0,cameraYaw);transpose(matrix);
            for(const std::uint16_t actorYaw:{0,80}) {
                const WorldModelViewInput view{matrix,{1000,10000,0}};
                const auto center=transformToView(view.relativePosition,{{},matrix,{},0});
                const auto radius=worldModelCullOperations(center,model.collisionRadius).projectedRadius;
                const auto old=worldModelSetupOperations(center,{0,0,actorYaw},radius,type,model,256,256,view);
                const auto shared=level2ConstructorModelSetupOperations(center,{0,0,actorYaw},radius,type,model,256,256,view);
                same(old.operations,shared.operations);assert(old.matrix.values==shared.matrix.values && old.groupOrder==shared.groupOrder);
                assert(shared.vertexPrecision==0 && shared.translation==center);
            }
        }
        //Precision follows actualcenter+radius; camera and actor yaw remain independent.
        for(const std::uint16_t actorYaw:{0,0x3450}) {
            auto matrix=makeFixedMatrixQ14(0,0,80);transpose(matrix);
            const WorldModelViewInput view{matrix,{4000,32217,0}};
            const auto center=transformToView(view.relativePosition,{{},matrix,{},0});
            const auto radius=worldModelCullOperations(center,model.collisionRadius).projectedRadius;
            rejects([&]{worldModelSetupOperations(center,{0,0,actorYaw},radius,type,model,256,256,view);});
            const auto wide=level2ConstructorModelSetupOperations(center,{0,0,actorYaw},radius,type,model,256,256,view);
            assert(wide.vertexPrecision==1 && wide.matrix.mode==1);
            auto input=worldModelGeometryInput(data,wide,center,PolygonClipMode::Disabled);
            input.matrixProfile=WorldModelMatrixProfile::level2Constructor;
            const auto geometry=level2ConstructorModelGeometryOperations(model.lods[0],input,{0,0,actorYaw},view,type);
            assert(geometry.polygons.size()==2 && !geometry.suffix.empty());
            auto wrong=input;wrong.vertexPrecision=0;
            rejects([&]{level2ConstructorModelGeometryOperations(model.lods[0],wrong,{0,0,actorYaw},view,type);});
            wrong=input;++wrong.fullCenter.x;
            rejects([&]{level2ConstructorModelGeometryOperations(model.lods[0],wrong,{0,0,actorYaw},view,type);});
        }
    }
    //P22's previously supported identity-wide source body remains the same stream.
    auto wideLod=lod(8,6);wideLod.groups[0].vertices.resize(14);
    for(auto& group:wideLod.groups){group.vertices.resize(14);group.vertices[1]={-256,400,256};group.vertices[2]={256,-400,-256};group.primitives[0].shapes[0].vertexIndices={1,2};}
    WorldModelGeometryInput input{};input.matrix=makeFixedMatrixQ14(0,0,0);input.vertexPrecision=1;
    input.fullCenter={8192,32217,0};input.translation={4096,16108,0};input.groupOrder={0,1,2,3,4,5};
    for(std::size_t i=0;i<256;++i){input.colorRemap[i]=static_cast<std::uint8_t>(i);}
    const auto old=worldModelGeometryOperations(wideLod,input);
    input.matrixProfile=WorldModelMatrixProfile::level2Constructor;
    const auto shared=level2ConstructorModelGeometryOperations(wideLod,input,{}, {makeFixedMatrixQ14(0,0,0),input.fullCenter},30);
    assert(old.polygons.size()==shared.polygons.size());
    for(std::size_t i=0;i<old.polygons.size();++i){same(old.polygons[i].prefix,shared.polygons[i].prefix);assert(old.polygons[i].points==shared.polygons[i].points);}
    same(old.suffix,shared.suffix);
}
