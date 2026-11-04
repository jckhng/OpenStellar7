#include "spatial_view.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void require(bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
stellar7::SpatialPoint subtract(stellar7::SpatialPoint a,stellar7::SpatialPoint b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
stellar7::SpatialPoint cross(stellar7::SpatialPoint a,stellar7::SpatialPoint b) {
    return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
float dot(stellar7::SpatialPoint a,stellar7::SpatialPoint b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
bool cabinHit(const stellar7::SpatialScene& scene,stellar7::SpatialPoint origin,stellar7::SpatialPoint ray,
    float maximumFraction=std::numeric_limits<float>::infinity()) {
    for(const auto& face:scene.surfaces) {
        if(!face.tint||!face.filled||face.opacity!=1||face.points.size()<3) {continue;}
        for(std::size_t i=2;i<face.points.size();++i) {
            const auto a=face.points[0],edge1=subtract(face.points[i-1],a),edge2=subtract(face.points[i],a);
            const auto p=cross(ray,edge2);const auto determinant=dot(edge1,p);
            if(std::abs(determinant)<1e-5f) {continue;}
            const auto offset=subtract(origin,a);const auto u=dot(offset,p)/determinant;
            if(u<0||u>1) {continue;}
            const auto q=cross(offset,edge1);const auto v=dot(ray,q)/determinant;
            if(v<0||u+v>1) {continue;}
            const auto fraction=dot(edge2,q)/determinant;
            if(fraction>0&&fraction<maximumFraction) {return true;}
        }
    }
    return false;
}
void instrumentVisibility(const stellar7::SpatialScene& scene,float scale) {
    for(const auto origin:std::array<stellar7::SpatialPoint,3>{{{0,0,0},{-180*scale,120*scale,100*scale},{180*scale,-120*scale,-100*scale}}}) {
        for(std::size_t panel=0;panel<3;++panel) {
            const auto& p=*scene.billboards[panel].corners;
            for(const float u:{0.05f,0.25f,0.5f,0.75f,0.95f}) {for(const float v:{0.05f,0.25f,0.5f,0.75f,0.95f}) {
                stellar7::SpatialPoint point;
                point.x=(p[0].x*(1-u)+p[3].x*u)*(1-v)+(p[1].x*(1-u)+p[2].x*u)*v;
                point.y=(p[0].y*(1-u)+p[3].y*u)*(1-v)+(p[1].y*(1-u)+p[2].y*u)*v;
                point.z=(p[0].z*(1-u)+p[3].z*u)*(1-v)+(p[1].z*(1-u)+p[2].z*u)*v;
                require(!cabinHit(scene,origin,subtract(point,origin),0.9999f),"front instruments are clipped by the surrounding cabin");
            }}
        }
    }
}
void equipmentPlanes(const stellar7::SpatialScene& scene,float scale) {
    unsigned plates=0;
    for(const auto& face:scene.surfaces) {
        if(!face.tint||face.points.size()!=4||!face.filled) {continue;}
        const auto c=*face.tint;
        if(!((c.red==65&&c.green==80&&c.blue==110)||(c.red==104&&c.green==112&&c.blue==128))) {continue;}
        if(!std::all_of(face.points.begin(),face.points.end(),[&](auto p) {
            return std::abs(p.x)>700*scale&&p.y>-1000*scale&&p.y<600*scale&&p.z< -250*scale;
        })) {continue;}
        const auto n=cross(subtract(face.points[1],face.points[0]),subtract(face.points[2],face.points[0]));
        const auto length=std::sqrt(dot(n,n));
        if(length==0) {continue;}
        ++plates;
        require(std::abs(dot(n,subtract(face.points[3],face.points[0])))/length<0.25f*scale,
            "raised console plate crosses a warped mounting surface");
    }
    require(plates>=6,"equipment mounting planes were not exercised");
}
void enclosedCabin(const stellar7::SpatialScene& scene,float scale) {
    constexpr double circle=6.283185307179586;
    for(const auto origin:std::array<stellar7::SpatialPoint,3>{{{0,0,0},{-180*scale,120*scale,100*scale},{180*scale,-120*scale,-100*scale}}}) {
        for(unsigned angle=0;angle<360;angle+=15) {
            const auto heading=angle*circle/360;
            require(cabinHit(scene,origin,{float(std::sin(heading)),float(std::cos(heading)),-0.65f}),
                "cockpit lower hull is open when looking around the seat");
        }
        require(cabinHit(scene,origin,{0,-1,0}),"cockpit lacks a solid rear bulkhead");
        require(cabinHit(scene,origin,{0,-0.5f,1}),"cockpit roof is open behind the seat");
        require(cabinHit(scene,origin,{0,0,-1}),"cockpit floor is open beneath the pilot");
    }
    require(!cabinHit(scene,{}, {0,1,0}),"cabin blocked the forward weapon sightline");
    require(!cabinHit(scene,{}, {1,0,0})&&!cabinHit(scene,{}, {-1,0,0}),"side window openings were sealed");
    for(const auto origin:std::array<stellar7::SpatialPoint,3>{{{0,0,0},{-180*scale,120*scale,100*scale},{180*scale,-120*scale,-100*scale}}}) {
        for(int angle=-95;angle<=95;angle+=5) {
            const auto heading=angle*circle/360;
            for(const float slope:{-0.15f,0.0f,0.25f}) {
                if(cabinHit(scene,origin,{float(std::sin(heading)),float(std::cos(heading)),slope})) {
                    throw std::runtime_error("canopy framing obstructs the panoramic driving sightlines: heading="+std::to_string(angle)
                        +" slope="+std::to_string(slope)+" origin="+std::to_string(origin.x)+","+std::to_string(origin.y)+","+std::to_string(origin.z));
                }
            }
        }
    }
}
}
int main() try {
    using namespace stellar7;
    SpatialScene source;source.camera=initialLevel1MainView();source.camera.origin={0,-4000,1800};
    source.camera.viewport={0,319,0,199};source.background.assign(64000,0);source.overlay.assign(64000,-1);
    SpatialWorld world;world.pilotCamera=initialLevel1MainView();world.pilotCamera.origin={0,0,600};
    world.pilotCamera.viewport={0,319,0,199};world.skyColor=1;world.groundColor=2;
    source.world=world;
    source.surfaces.push_back({{{0,4000,-1800}},3,true,1,1,{},std::numeric_limits<std::size_t>::max(),0,1});
    IndexedScreen indexed{std::vector<std::uint8_t>(64000),std::vector<Rgb>(256)};
    for(std::size_t i=0;i<indexed.pixels.size();++i) {indexed.pixels[i]=static_cast<std::uint8_t>(i/320);}
    const auto pilot=makeSpatialView(source,indexed,EnhancedView::cockpit,{});
    require(pilot.camera.origin.z==600&&pilot.camera.origin.y==0,"cockpit retained the chase camera");
    require(pilot.billboards.size()==4,"auxiliary consoles duplicate the live dashboard instruments");
    require(cabinHit(pilot,{}, {0,1,-0.25f}),"cockpit lacks a visible Raven nose below the windshield");
    enclosedCabin(pilot,1);
    instrumentVisibility(pilot,1);equipmentPlanes(pilot,1);
    const auto xrPilot=makeSpatialView(source,indexed,EnhancedView::cockpit,{}, {0.2f,0});
    enclosedCabin(xrPilot,0.2f);instrumentVisibility(xrPilot,0.2f);equipmentPlanes(xrPilot,0.2f);
    require(std::none_of(pilot.surfaces.begin(),pilot.surfaces.end(),[](const auto& surface) {
        return surface.allocation==std::numeric_limits<std::size_t>::max()&&surface.type==0;
    }),"cockpit retained the exterior player hull");
    require(std::all_of(pilot.overlay.begin(),pilot.overlay.end(),[](auto value) {return value==-1;}),"cockpit retained a head-locked chase HUD");
    const auto ground=std::find_if(pilot.surfaces.begin(),pilot.surfaces.end(),[](const auto& surface) {return surface.material==SpatialMaterial::groundDetail;});
    require(ground!=pilot.surfaces.end()&&ground->uv.size()==4,"world ground coordinates missing");
    require(source.camera.origin.y==-4000&&source.billboards.empty()&&source.surfaces.size()==1,"view generation mutated the source scene");
    auto blinkingSource=source;blinkingSource.world->tick=8;
    const auto blinking=makeSpatialView(blinkingSource,indexed,EnhancedView::cockpit,{});
    require(blinking.surfaces.size()==pilot.surfaces.size(),"indicator animation changed cockpit topology");
    bool lampChanged=false;
    for(std::size_t i=0;i<pilot.surfaces.size();++i) {
        const auto a=pilot.surfaces[i].tint,b=blinking.surfaces[i].tint;
        lampChanged=lampChanged||(a&&b&&(a->red!=b->red||a->green!=b->green||a->blue!=b->blue));
    }
    require(lampChanged,"auxiliary indicators ignore the retained simulation tick");
    auto firingSource=source;firingSource.world->playerShotAges={1,0};
    require(makeSpatialView(firingSource,indexed,EnhancedView::cockpit,{}).surfaces.size()==pilot.surfaces.size()+1,
        "accepted shot lacks a deck-cannon flash");
    firingSource.world->playerShotAges={3,0};
    require(makeSpatialView(firingSource,indexed,EnhancedView::cockpit,{}).surfaces.size()==pilot.surfaces.size(),
        "deck-cannon flash outlives its source shot age");
    auto shotSource=source;shotSource.surfaces.clear();
    const auto addShot=[&](std::size_t slot,float depth,bool owned) {
        SpatialSurface shot;shot.type=23;shot.allocation=slot;
        for(const auto p:std::array<SpatialPoint,3>{{{-10,depth,0},{10,depth,0},{0,depth+20,10}}}) {
            shot.points.push_back(spatialFromWorld(p,source.camera));
        }
        if(owned) {shot.playerShotCenter=spatialFromWorld({0,depth,0},source.camera);}
        shotSource.surfaces.push_back(std::move(shot));
    };
    addShot(7,500,true);addShot(8,500,false);addShot(9,4000,true);addShot(10,0,true);
    auto lateral=shotSource.surfaces[0];lateral.allocation=11;
    for(auto& point:lateral.points) {point.x+=4000;}
    lateral.playerShotCenter->x+=4000;shotSource.surfaces.push_back(std::move(lateral));
    const auto shots=makeSpatialView(shotSource,indexed,EnhancedView::cockpit,{}, {0.2f,0});
    require(shots.surfaces[0].points[0].z<shots.surfaces[1].points[0].z,
        "near player shot still launches through the center dashboard");
    constexpr float muzzleDepth=480,muzzleHeight=-116,joinDepth=1200;
    const auto near=*shots.surfaces[0].playerShotCenter,born=*shots.surfaces[3].playerShotCenter;
    require(std::abs(near.z*(joinDepth-muzzleDepth)-muzzleHeight*(joinDepth-near.y))<0.1f,
        "near-field shot does not follow the deck muzzle convergence line");
    require(std::abs(born.y-muzzleDepth)<0.01f&&std::abs(born.z-muzzleHeight)<0.01f,
        "newborn shot is drawn behind the deck muzzle");
    require(std::abs(shots.surfaces[4].points[0].z)<0.01f&&std::abs(shots.surfaces[4].playerShotCenter->y-500)<0.01f,
        "distant lateral projectile was pulled toward the current cannon");
    require(std::abs(shots.surfaces[1].points[0].z)<0.01f&&std::abs(shots.surfaces[2].points[0].z)<0.01f,
        "cockpit changed hostile or far-field projectile geometry");
    require(shotSource.surfaces[0].points[0].z==-source.camera.origin.z,
        "cosmetic projectile alignment mutated source geometry");
    const auto chaseShots=makeSpatialView(shotSource,indexed,EnhancedView::chase,{});
    require(chaseShots.surfaces[0].points[0].z==shotSource.surfaces[0].points[0].z,
        "cockpit launch correction leaked into chase rendering");
    for(std::size_t i=0;i<ground->points.size();++i) {
        const auto p=spatialToWorld(ground->points[i],pilot.camera);
        require(std::abs(p.z-spatialGroundHeight)<0.25f&&std::abs(p.x/2048-ground->uv[i][0])<0.001f
            &&std::abs(p.y/2048-ground->uv[i][1])<0.001f,"ground pattern follows the camera rather than world coordinates");
    }
    source.world->translation={2048,-1024};
    const auto moved=makeSpatialView(source,indexed,EnhancedView::cockpit,{});
    const auto shifted=std::find_if(moved.surfaces.begin(),moved.surfaces.end(),[](const auto& surface) {return surface.material==SpatialMaterial::groundDetail;});
    for(std::size_t i=0;i<ground->uv.size();++i) {
        require(std::abs(shifted->uv[i][0]-ground->uv[i][0]-1)<0.001f
            &&std::abs(shifted->uv[i][1]-ground->uv[i][1]+0.5f)<0.001f,"terrain UVs ignored simulation world recentering");
    }
    Level1Entity player{};player.position={100,200,300};player.yaw=0x4000;
    const auto rotated=spatialPilotCamera(player);const SpatialPoint worldPoint{2400,7000,1800};
    const auto roundTrip=spatialToWorld(spatialFromWorld(worldPoint,rotated),rotated);
    require(std::abs(roundTrip.x-worldPoint.x)<0.01f&&std::abs(roundTrip.y-worldPoint.y)<0.01f
        &&std::abs(roundTrip.z-worldPoint.z)<0.01f,"pilot camera transform is not reversible");
    const std::vector<DestructionCue> cues{{1,10,0,{0,2000,0},1,500,0},{2,10,1,{0,2000,0},1,500,0}};
    const auto first=spatialExplosionLights(cues,10,0,source.camera),last=spatialExplosionLights(cues,15,0,source.camera);
    require(first.size()==1&&last.size()==1&&first[0].strength>last[0].strength,"light age/level ownership is incorrect");
    require(spatialExplosionLights(cues,9,0,source.camera).empty()&&spatialExplosionLights(cues,16,0,source.camera).empty(),"local light escaped its source tick lifetime");
    SpatialOptions plain;plain.groundDetail=false;plain.localLights=false;
    const auto classic=makeSpatialView(source,indexed,EnhancedView::chase,plain);
    require(std::none_of(classic.surfaces.begin(),classic.surfaces.end(),[](const auto& surface) {return surface.material==SpatialMaterial::groundDetail;}),"ground detail toggle ignored");
    std::cout<<"Spatial cockpit camera, physical panels, hull exclusion, world-ground and immutable view checks pass\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
