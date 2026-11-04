#include "spatial_scene.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
template<class Action> void rejects(Action action) {
    bool rejected=false;try {action();} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"invalid spatial input was accepted");
}
}
int main() try {
    using namespace stellar7;
    std::vector<Rgb> palette(256);palette[1]={255,0,0};
    SpatialScene scene;
    SpatialSurface concave;concave.points={{0,100,0},{4,100,0},{4,100,4},{2,100,2},{0,100,4}};concave.color=1;
    scene.surfaces.push_back(concave);
    const auto geometry=spatialGeometry(scene,palette);
    require(geometry.triangles.size()==9,"concave pentagon did not form three triangles");
    double area=0;
    for(std::size_t i=0;i<geometry.triangles.size();i+=3) {
        const auto a=geometry.triangles[i],b=geometry.triangles[i+1],c=geometry.triangles[i+2];
        area+=std::abs((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))/2;
        require(a.z==-100&&a.r==1&&a.g==0,"native axes/palette changed");
    }
    require(area==12,"concave triangulation filled the missing notch");
    std::reverse(scene.surfaces[0].points.begin(),scene.surfaces[0].points.end());
    const auto reversed=spatialGeometry(scene,palette);double reversedArea=0;
    for(std::size_t i=0;i<reversed.triangles.size();i+=3) {
        const auto a=reversed.triangles[i],b=reversed.triangles[i+1],c=reversed.triangles[i+2];
        reversedArea+=std::abs((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x))/2;
    }
    require(reversedArea==12,"clockwise polygon changed filled area");
    scene.surfaces[0].opacity=0.5;scene.surfaces[0].lighting=0.5;
    const auto translucent=spatialGeometry(scene,palette);
    require(translucent.triangles.empty()&&!translucent.translucentTriangles.empty()
        &&translucent.translucentTriangles[0].r==0.5f&&translucent.translucentTriangles[0].a==0.5f,"transparent/lighting data lost");
    scene.surfaces[0].points={{0,100,0},{0,100,0},{1,100,0},{2,100,0},{0,100,0}};
    require(!spatialGeometry(scene,palette).translucentLines.empty(),"degenerate distant face lost line coverage");
    scene.surfaces[0].points={{0,100,0}};
    require(spatialGeometry(scene,palette).translucentPoints.size()==1,"point primitive lost");
    scene.surfaces[0].points={{0,100,0},{0,100,30}};scene.surfaces[0].filled=false;scene.surfaces[0].strokeRadius=4;
    const auto stroke=spatialGeometry(scene,palette);
    require(stroke.translucentLines.empty()&&stroke.translucentTriangles.size()==96,"energy segment has no capped physical width");
    const auto extent=std::minmax_element(stroke.translucentTriangles.begin(),stroke.translucentTriangles.end(),[](auto a,auto b) {return a.x<b.x;});
    require(extent.first->x==-4&&extent.second->x==4,"energy radius changed during native-axis conversion");
    require(stroke.translucentTriangles.front().r==0.5f&&stroke.translucentTriangles.front().a==0.5f,"energy color/opacity changed");
    scene.surfaces[0].points={{0,100,0}};
    require(spatialGeometry(scene,palette).translucentPoints.empty()&&spatialGeometry(scene,palette).translucentTriangles.size()==24,
        "point energy primitive still has zero physical size");
    scene.surfaces[0].points={{0,100,0},{20,120,0},{-20,120,0}};scene.surfaces[0].filled=true;
    require(spatialGeometry(scene,palette).translucentTriangles.size()>3,"edge-on energy face lacks minimum stroke coverage");
    scene.surfaces[0].strokeRadius=-1;rejects([&] {spatialGeometry(scene,palette);});
    scene.surfaces[0].strokeRadius=std::numeric_limits<float>::quiet_NaN();rejects([&] {spatialGeometry(scene,palette);});
    scene.surfaces[0].strokeRadius=0;
    scene.surfaces[0].points[0].x=std::numeric_limits<float>::infinity();
    rejects([&] {spatialGeometry(scene,palette);});
    auto camera=initialLevel1MainView();camera.viewport={0,319,0,199};
    const auto mono=spatialProjection(camera,4.0/3),wide=spatialProjection(camera,16.0/9);
    require(wide[0]<mono[0]&&wide[5]==mono[5],"wider frustum changed vertical FOV");
    const auto left=spatialProjection(camera,4.0/3,{-32,0,0}),right=spatialProjection(camera,4.0/3,{32,0,0});
    require(left[12]>right[12]&&left[5]==right[5],"stereo eyes lack independent physical translation");
    rejects([&] {spatialProjection(camera,0);});
    require(spatialIdentity()[15]==1,"identity matrix invalid");
    std::cout<<"Spatial scene: triangulation, axes/materials, finite energy strokes, degenerate coverage, frustum and eyes pass\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
