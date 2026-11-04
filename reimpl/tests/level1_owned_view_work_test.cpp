#include "stellar7/level1_owned_view_work.hpp"
#include "stellar7/failure_model_work.hpp"
#include "stellar7/level1_projectile_render_work.hpp"
#include "stellar7/raster_line_work.hpp"
#include <cassert>
#include <stdexcept>
namespace {
using namespace stellar7;
template<class Action> void rejects(Action action) {
    bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}assert(rejected);
}
void same(const std::vector<NormalWorkOperation>& a,const std::vector<NormalWorkOperation>& b) {
    assert(a.size()==b.size());for(std::size_t i=0;i<a.size();++i){assert(a[i].kind==b[i].kind && a[i].count==b[i].count && a[i].value==b[i].value);}
}
ModelGroup group() {
    ModelGroup g{};g.vertexBlock=1;g.animationOffset=0;
    g.vertices={{{-40,64,-20}},{{40,256,-20}},{{0,256,40}},{{0,0,16384}}};
    ModelPrimitive first{},second{};
    first.shapes={{0x80,{72,72,0,0},255,{0,1,2}}};
    second.shapes={{0x80,{73,73,0,0},255,{0,1,2}}};
    g.primitives={first,second};return g;
}
WorldModelGeometryInput input(const std::array<std::uint16_t,3> angles,const WorldModelViewInput& view) {
    WorldModelGeometryInput i{};i.matrix=composeFixedMatricesQ14(makeFixedMatrixQ14(angles[0],angles[1],angles[2]),view.matrix,0x5240);
    i.fullCenter=transformToView(view.relativePosition,{{},view.matrix,{},0});i.translation=i.fullCenter;
    i.groupOrder={0};i.animationSelectors={1};
    if(angles[1]!=0){i.matrixProfile=WorldModelMatrixProfile::level1Banked;}
    for(unsigned n=0;n<256;++n){i.colorRemap[n]=static_cast<std::uint8_t>(n);}
    return i;
}
Model3d model(std::uint32_t radius) {
    Model3d m{};m.flags=1;m.collisionRadius=radius;auto g=group();g.animationOffset=-1;
    m.lods={{10,128,{g,g}},{30,128,{g,g}},{255,128,{g,g}}};return m;
}
}
int main() {
    using namespace stellar7;const auto identity=makeFixedMatrixQ14(0,0,0);
    for(const auto radius:{134U,441U,748U,862U,1342U}) {
        auto m=model(radius);
        for(const auto center:{FixedPosition{0,128,0},FixedPosition{-100,800,150},FixedPosition{100,800,1000}}) {
            const auto cull=sharedModelSphereCullOperations(center,m);
            if(cull.culled){continue;}
            const auto setup=level1OwnedModelSetupOperations(center,{0,0,35584},cull.projectedRadius,m,256,256,{identity,center});
            assert(setup.translation==center && setup.vertexPrecision==0);
            assert(setup.selectedLodIndex==(cull.projectedRadius<=10 ? 0U : cull.projectedRadius<=30 ? 1U : 2U));
        }
    }
    auto m=model(748);const FixedPosition near{0,128,150};
    const auto cull=sharedModelSphereCullOperations(near,m);
    assert(cull.projectedRadius==256);
    const auto terminal=level1OwnedModelSetupOperations(near,{0,0,0},256,m,256,256,{identity,near});
    assert(terminal.selectedLodIndex==2);
    rejects([&]{worldModelSetupOperations(near,{0,0,0},256,11,m,256,256,WorldModelViewInput{identity,near});});
    const WorldModelViewInput view{identity,{}};const std::array<std::uint16_t,3> angles{};
    ModelLod lod{255,128,{group()}};auto i=input(angles,view);
    const auto clipped=level1OwnedModelGeometryOperations(lod,i,angles,view);
    assert(clipped.polygons.size()==1 && clipped.polygons[0].primaryColor==73);
    assert((clipped.polygons[0].points==std::vector<ProjectedPoint>{{132,84},{145,104},{179,94},{159,64}}));
    rejects([&]{worldModelGeometryOperations(lod,i);});
    auto reference=i;reference.screenCenter.y=99;
    const auto pinned=failureModelGeometryOperations(lod,reference);
    same(clipped.polygons[0].prefix,pinned.polygons[0].prefix);same(clipped.suffix,pinned.suffix);
    for(std::size_t n=0;n<clipped.polygons[0].points.size();++n){assert(clipped.polygons[0].points[n].y==pinned.polygons[0].points[n].y-15);}
    auto selected=i;selected.animationSelectors={0};
    assert(level1OwnedModelGeometryOperations(lod,selected,angles,view).polygons[0].primaryColor==72);
    auto allBehind=lod;for(auto& v:allBehind.groups[0].vertices){v[1]=64;}
    assert(level1OwnedModelGeometryOperations(allBehind,i,angles,view).polygons[0].points.empty());
    auto repeated=lod;repeated.groups.push_back(repeated.groups[0]);i.groupOrder={0,1};
    const auto cached=level1OwnedModelGeometryOperations(repeated,i,angles,view);
    auto old=i;old.screenCenter.y=99;const auto cachedReference=failureModelGeometryOperations(repeated,old);
    assert(cached.polygons.size()==2);same(cached.suffix,cachedReference.suffix);
    for(std::size_t n=0;n<2;++n){same(cached.polygons[n].prefix,cachedReference.polygons[n].prefix);}
    //Cached normal/shading path is shared with the qualified failure body.
    repeated.groups[0].vertices[3]={0,0,1000};repeated.groups[1].vertices[3]={0,0,1000};
    for(auto& g:repeated.groups){for(auto& v:g.vertices){v[1]+=500;}for(auto& primitive:g.primitives){primitive.shapes={{0xa0,{8,8,0,0},3,{0,1,2}}};}}
    i.shadeLevels[8]=1;i.lightingAdjustments[0]=3;old=i;old.screenCenter.y=99;
    const auto shade=level1OwnedModelGeometryOperations(repeated,i,angles,view);const auto shadeReference=failureModelGeometryOperations(repeated,old);
    assert(shade.polygons.size()==shadeReference.polygons.size());
    for(std::size_t n=0;n<shade.polygons.size();++n){assert(shade.polygons[n].primaryColor==shadeReference.polygons[n].primaryColor);same(shade.polygons[n].prefix,shadeReference.polygons[n].prefix);}
    for(const auto actorAngles:{std::array<std::uint16_t,3>{0,0,0},{0,0,18432},{0,271,18432}}) {
        for(const auto cameraYaw:{std::uint16_t{0},std::uint16_t{65216}}) {
            const WorldModelViewInput rotatedView{makeFixedMatrixQ14(0,0,cameraYaw),{0,1000,0}};
            auto rotated=input(actorAngles,rotatedView);auto front=lod;
            for(auto& v:front.groups[0].vertices){v[1]+=500;}
            const auto work=level1OwnedModelGeometryOperations(front,rotated,actorAngles,rotatedView);
            auto ref=rotated;ref.screenCenter.y=99;
            const auto reference=failureModelGeometryOperations(front,ref,
                actorAngles[1]==0 && actorAngles[2]==0 ? std::optional<FixedPosition>{rotatedView.relativePosition} : std::nullopt);
            assert(work.polygons.size()==reference.polygons.size());same(work.suffix,reference.suffix);
            for(std::size_t n=0;n<work.polygons.size();++n){same(work.polygons[n].prefix,reference.polygons[n].prefix);assert(work.polygons[n].primaryColor==reference.polygons[n].primaryColor);}
        }
    }
    //Pinned P26 bank/shading output, now through the explicit owned family.
    {
        const std::array<std::uint16_t,3> bankAngles{0,65265,18432};
        const WorldModelViewInput bankView{identity,{0,1000,0}};auto bank=input(bankAngles,bankView);
        ModelGroup g{};g.vertexBlock=10;g.animationOffset=-1;
        g.vertices={{{-103,217,-359}},{{103,-217,359}},{{-173,47,-239}},{{71,191,133}}};
        ModelPrimitive primitive{};primitive.shapes={{0xa1,{8,8,1,1},0,{1,2,3}}};g.primitives={primitive};
        bank.animationSelectors={0};bank.shadeLevels[8]=1;bank.lightingAdjustments[13]=3;
        const auto work=level1OwnedModelGeometryOperations({255,128,{g}},bank,bankAngles,bankView);
        assert(work.polygons.size()==1 && work.polygons[0].primaryColor==11);
        assert((work.polygons[0].points==std::vector<ProjectedPoint>{{180,44},{157,121},{134,68}}));
    }
    const Viewport viewport{24,294,24,145};
    for(const auto points:{std::vector<ProjectedPoint>{},std::vector<ProjectedPoint>{{12,80},{40,84}},
        std::vector<ProjectedPoint>{{10,20},{310,30},{150,170}},std::vector<ProjectedPoint>{{24,80},{40,80},{40,80}}}) {
        const auto raster=ownedPolygonOperations(points,viewport,PolygonClipMode::Enabled,FailurePolygonStyle::filled,73,73);
        for(const auto& pixel:raster.pixels){assert(pixel.point.x>=24 && pixel.point.x<=294 && pixel.point.y>=24 && pixel.point.y<=145);}
    }
    auto invalid=i;invalid.vertexPrecision=1;rejects([&]{level1OwnedModelGeometryOperations(repeated,invalid,angles,view);});
    auto noSelector=i;noSelector.animationSelectors.clear();
    const auto direct=level1OwnedModelGeometryOperations(repeated,noSelector,angles,view);
    auto directInput=noSelector;directInput.screenCenter.y=99;
    const auto directReference=failureModelGeometryOperations(repeated,directInput);
    assert(direct.polygons.size()==directReference.polygons.size());same(direct.suffix,directReference.suffix);
    for(std::size_t n=0;n<direct.polygons.size();++n) {
        assert(direct.polygons[n].primaryColor==directReference.polygons[n].primaryColor);
        same(direct.polygons[n].prefix,directReference.polygons[n].prefix);
    }
    rejects([&]{level1OwnedModelGeometryOperations(repeated,i,{1,0,0},view);});
    rejects([&]{level1OwnedModelSetupOperations(near,{1,0,0},256,m,256,256,{identity,near});});
    rejects([&]{level1OwnedModelSetupOperations(near,{0,0,0},256,m,44,256,{identity,near});});
    const FixedPosition wide{0,32000,0};rejects([&]{level1OwnedModelSetupOperations(wide,{0,0,0},3,m,256,256,{identity,wide});});
    //Source49F6 switches vertex precision independently of actor/camera rotation.
    //The actual retained151 boundary combines a banked Type11 and a yaw camera.
    {
        const auto setupView=worldViewSetupOperations({{}, {}, {0,0,12640},0x88,7,{24,294,24,145},{256,-256,362}});
        const WorldModelViewInput wideView{setupView.view.matrix,{-26431,18788,1000}};
        const FixedPosition center{8305,31344,1000};
        const std::array<std::uint16_t,3> bankAngles{0,65448,16076};
        const auto wideCull=level1OwnedWideModelCullOperations(center,m);
        assert(!wideCull.culled && !wideCull.clipped && wideCull.projectedRadius==3);
        rejects([&]{sharedModelSphereCullOperations(center,m);});
        rejects([&]{level1OwnedModelSetupOperations(center,bankAngles,3,m,256,256,wideView);});
        const auto setup=level1OwnedWideModelSetupOperations(center,bankAngles,3,m,256,256,wideView);
        assert(setup.vertexPrecision==1 && setup.matrix.mode>=2);
        assert((setup.translation==FixedPosition{4152,15672,500}));
        auto wideInput=input(bankAngles,wideView);
        wideInput.matrix=setup.matrix;wideInput.translation=setup.translation;
        wideInput.vertexPrecision=setup.vertexPrecision;wideInput.groupOrder=setup.groupOrder;
        const auto geometry=level1OwnedWideModelGeometryOperations(m.lods[setup.selectedLodIndex],wideInput,bankAngles,wideView);
        assert(!geometry.polygons.empty());
        for(const auto& polygon:geometry.polygons) {
            assert(polygon.primaryColor==72);
            for(const auto& point:polygon.points){assert(point.x>=24 && point.x<=294 && point.y>=24 && point.y<=145);}
        }
        rejects([&]{level1OwnedModelGeometryOperations(m.lods[setup.selectedLodIndex],wideInput,bankAngles,wideView);});
        auto invalid=wideInput;invalid.translation.x++;
        rejects([&]{level1OwnedWideModelGeometryOperations(m.lods[setup.selectedLodIndex],invalid,bankAngles,wideView);});
        invalid=wideInput;invalid.vertexPrecision=0;
        rejects([&]{level1OwnedWideModelGeometryOperations(m.lods[setup.selectedLodIndex],invalid,bankAngles,wideView);});
        invalid=wideInput;invalid.matrix.values[0]++;
        rejects([&]{level1OwnedWideModelGeometryOperations(m.lods[setup.selectedLodIndex],invalid,bankAngles,wideView);});
        rejects([&]{level1OwnedWideModelSetupOperations(center,{1,0,0},3,m,256,256,wideView);});
        rejects([&]{level1OwnedWideModelCullOperations({0,64000,0},m);});
        rejects([&]{level1OwnedWideModelCullOperations(near,m);});
    }

    //Identity-actor shortcut, yaw and full matrix branches share the precision flag.
    for(const auto actorAngles:{std::array<std::uint16_t,3>{0,0,0},{0,0,18432},{0,271,18432}}) {
        for(const auto cameraYaw:{std::uint16_t{0},std::uint16_t{4096}}) {
            const auto wideViewSetup=worldViewSetupOperations({{}, {}, {0,0,cameraYaw},0x88,7,{24,294,24,145},{256,-256,362}});
            const WorldModelViewInput wideView{wideViewSetup.view.matrix,{0,40000,0}};
            const auto center=transformToView(wideView.relativePosition,{{},wideView.matrix,{},0});
            const auto wideCull=level1OwnedWideModelCullOperations(center,m);
            assert(!wideCull.culled);
            const auto setup=level1OwnedWideModelSetupOperations(center,actorAngles,wideCull.projectedRadius,m,256,256,wideView);
            auto wideInput=input(actorAngles,wideView);wideInput.vertexPrecision=setup.vertexPrecision;
            wideInput.matrix=setup.matrix;wideInput.translation=setup.translation;wideInput.groupOrder=setup.groupOrder;
            wideInput.animationSelectors.clear();
            const auto geometry=level1OwnedWideModelGeometryOperations(m.lods[setup.selectedLodIndex],wideInput,actorAngles,wideView);
            assert(geometry.polygons.size()==2);
        }
    }

    //Source5392 chooses its two-DIV branch for unsigned divisors >=8000.
    {
        auto descriptor=model(748);
        const FixedPosition center{7229,33520,1000};
        const auto caller=level1OwnedWideModelCallerOperations(center,descriptor,0,0);
        assert(caller.minimumDepth==32772 && caller.projectedRadius==2);
        same(caller.operations,{{NormalWorkKind::instructions,30},{NormalWorkKind::instructions,114},
            {NormalWorkKind::instructions,17}});
        const auto setupView=worldViewSetupOperations({{}, {}, {0,0,12640},0x88,7,{24,294,24,145},{256,-256,362}});
        const WorldModelViewInput modelView{setupView.view.matrix,{-28846,18545,1000}};
        const std::array<std::uint16_t,3> angles160{0,63573,18515};
        const auto setup=level1OwnedWideModelSetupOperations(center,angles160,2,descriptor,256,256,modelView);
        assert(setup.vertexPrecision==1);
        assert((setup.translation==FixedPosition{1807,8380,250}));
        auto geometryInput=input(angles160,modelView);
        geometryInput.translation=setup.translation;geometryInput.vertexPrecision=setup.vertexPrecision;
        geometryInput.matrix=setup.matrix;geometryInput.groupOrder=setup.groupOrder;
        const auto geometry=level1OwnedWideModelGeometryOperations(descriptor.lods[0],geometryInput,angles160,modelView);
        assert(!geometry.polygons.empty());
        geometryInput.translation={3614,16760,500};
        rejects([&]{level1OwnedWideModelGeometryOperations(descriptor.lods[0],geometryInput,angles160,modelView);});
        rejects([&]{worldModelCallerOperations(center,748,0,0);});
        rejects([&]{level1OwnedWideModelCallerOperations(center,descriptor,4,0);});
        rejects([&]{level1OwnedWideModelCallerOperations(center,descriptor,0,0x130c);});
        rejects([&]{level1OwnedWideModelCallerOperations({0,32000,0},descriptor,0,0);});
        rejects([&]{level1OwnedWideModelCallerOperations({0,64000,0},descriptor,0,0);});
        descriptor.scaleShift=1;
        rejects([&]{level1OwnedWideModelCallerOperations(center,descriptor,0,0);});
        descriptor=model(748);descriptor.origin[0]=1;
        rejects([&]{level1OwnedWideModelCallerOperations(center,descriptor,0,0);});
        for(const auto radius:{134U,441U,748U,770U,862U,1342U}) {
            const auto sourceDescriptor=model(radius);
            const auto work=level1OwnedWideModelCallerOperations({0,40000,0},sourceDescriptor,0,0);
            assert(work.minimumDepth==40000-static_cast<std::int32_t>(radius));
            assert(work.projectedRadius==radius*128/(40000-radius));
            same(work.operations,caller.operations);
        }
    }

}
