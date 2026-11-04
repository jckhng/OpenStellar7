#include "spatial_view.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr float groundRadius=750000,panoramaRadius=500000,groundTileSize=2048;
constexpr std::int64_t groundPatternRepeat=17*std::int64_t(groundTileSize);
constexpr std::size_t panoramaSegments=32,maximumLocalLights=4;
constexpr std::uint64_t explosionLightTicks=6;
constexpr float shadowLift=1;
constexpr SpatialPoint cockpitMuzzle{0,2400,-580};
constexpr float cockpitShotJoinDistance=6000;
using Rotation=std::array<double,9>;
Rotation rotation(const ViewTransform& camera) {
    if(camera.matrix.mode==0) {return {1,0,0,0,1,0,0,0,1};}
    Rotation result{};for(std::size_t i=0;i<result.size();++i) {result[i]=camera.matrix.values[i]/16384.0;}return result;
}
SpatialPoint difference(SpatialPoint a,SpatialPoint b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
SpatialPoint normal(const std::vector<SpatialPoint>& points) {
    if(points.size()<3) {return {};}
    const auto a=difference(points[1],points[0]),b=difference(points[2],points[0]);
    SpatialPoint n{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
    const auto length=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
    if(length==0) {return {};}
    const auto sign=n.x*points[0].x+n.y*points[0].y+n.z*points[0].z>0?-1.0f:1.0f;
    return {n.x*sign/length,n.y*sign/length,n.z*sign/length};
}
void cabinFace(SpatialScene& scene,std::vector<SpatialPoint> points,Rgb color) {
    SpatialSurface face;face.points=std::move(points);face.tint=color;
    auto direction=scene.camera;direction.origin={};const auto sun=spatialFromWorld({-256,-256,362},direction);
    const auto n=normal(face.points);const auto length=std::sqrt(sun.x*sun.x+sun.y*sun.y+sun.z*sun.z);
    face.lighting=0.6+0.4*std::max(0.0f,(n.x*sun.x+n.y*sun.y+n.z*sun.z)/length);
    scene.surfaces.push_back(std::move(face));
}
void box(SpatialScene& scene,SpatialPoint a,SpatialPoint b,Rgb color) {
    cabinFace(scene,{{a.x,a.y,a.z},{b.x,a.y,a.z},{b.x,b.y,a.z},{a.x,b.y,a.z}},color);
    cabinFace(scene,{{a.x,a.y,b.z},{a.x,b.y,b.z},{b.x,b.y,b.z},{b.x,a.y,b.z}},color);
    cabinFace(scene,{{a.x,a.y,a.z},{a.x,a.y,b.z},{b.x,a.y,b.z},{b.x,a.y,a.z}},color);
    cabinFace(scene,{{a.x,b.y,a.z},{b.x,b.y,a.z},{b.x,b.y,b.z},{a.x,b.y,b.z}},color);
    cabinFace(scene,{{a.x,a.y,a.z},{a.x,b.y,a.z},{a.x,b.y,b.z},{a.x,a.y,b.z}},color);
    cabinFace(scene,{{b.x,a.y,a.z},{b.x,a.y,b.z},{b.x,b.y,b.z},{b.x,b.y,a.z}},color);
}
IndexedBitmap crop(const IndexedScreen& screen,int x,int y,int width,int height) {
    IndexedBitmap image{static_cast<std::uint16_t>(width),static_cast<std::uint16_t>(height),{}};
    for(int row=0;row<height;++row) {
        const auto first=screen.pixels.begin()+(y+row)*320+x;image.pixels.insert(image.pixels.end(),first,first+width);
    }
    return image;
}
void panel(SpatialScene& scene,IndexedBitmap image,std::array<SpatialPoint,4> corners) {
    SpatialBillboard value;value.image=std::move(image);value.corners=corners;
    value.alpha=BillboardAlpha::opaque;value.depth=BillboardDepth::write;scene.billboards.push_back(std::move(value));
}
void cabinHousing(SpatialScene& scene,const std::array<SpatialPoint,4>& front,float depth,Rgb rim,Rgb body) {
    auto back=front;for(auto& point:back) {point.y+=depth;}
    cabinFace(scene,{front.begin(),front.end()},rim);cabinFace(scene,{back.begin(),back.end()},body);
    for(std::size_t i=0;i<front.size();++i) {
        const auto next=(i+1)%front.size();cabinFace(scene,{front[i],front[next],back[next],back[i]},body);
    }
}
void cabinRail(SpatialScene& scene,SpatialPoint a,SpatialPoint b,float radius,Rgb color) {
    const auto direction=difference(b,a);
    const auto length=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);
    if(length==0) {return;}
    const SpatialPoint axis{direction.x/length,direction.y/length,direction.z/length};
    SpatialPoint across{axis.y,-axis.x,0};
    const auto width=std::sqrt(across.x*across.x+across.y*across.y);
    if(width==0) {across={1,0,0};}
    else {across.x/=width;across.y/=width;}
    const SpatialPoint up{axis.y*across.z-axis.z*across.y,axis.z*across.x-axis.x*across.z,axis.x*across.y-axis.y*across.x};
    constexpr std::size_t sides=6;
    constexpr float circle=6.28318530718f;
    std::vector<SpatialPoint> first,last;
    for(std::size_t i=0;i<sides;++i) {
        const auto angle=i*circle/sides,c=radius*std::cos(angle),s=radius*std::sin(angle);
        const SpatialPoint offset{across.x*c+up.x*s,across.y*c+up.y*s,across.z*c+up.z*s};
        first.push_back({a.x+offset.x,a.y+offset.y,a.z+offset.z});
        last.push_back({b.x+offset.x,b.y+offset.y,b.z+offset.z});
    }
    for(std::size_t i=0;i<sides;++i) {
        const auto next=(i+1)%sides;cabinFace(scene,{first[i],first[next],last[next],last[i]},color);
    }
    cabinFace(scene,std::move(first),color);cabinFace(scene,std::move(last),color);
}
void cabinShell(SpatialScene& scene,std::uint64_t tick) {
    constexpr float floor=-850;
    constexpr Rgb blue{36,42,84},dark{15,20,40},metal{104,112,128};
    constexpr Rgb amber{200,132,48},teal{55,146,156},red{102,35,42},rim{65,80,110};
    // A bevelled lower hull and clear wraparound canopy retain the Raven palette.
    // All upright support is behind the pilot's panoramic driving sightlines.
    const std::array<SpatialPoint,8> sill{{{-1450,1500,-550},{1450,1500,-550},
        {1700,300,-510},{1600,-750,-430},{900,-1500,-380},
        {-900,-1500,-380},{-1600,-750,-430},{-1700,300,-510}}};
    std::vector<SpatialPoint> footwell;
    for(const auto p:sill) {footwell.push_back({p.x*0.72f,p.y*0.85f,floor});}
    cabinFace(scene,footwell,dark);
    for(std::size_t i=0;i<sill.size();++i) {
        const auto next=(i+1)%sill.size();const auto a=sill[i],b=sill[next];
        const SpatialPoint innerA{a.x*0.94f,a.y*0.96f,-650},innerB{b.x*0.94f,b.y*0.96f,-650};
        cabinFace(scene,{footwell[i],footwell[next],innerB,innerA},dark);
        cabinFace(scene,{innerA,innerB,b,a},blue);
        cabinRail(scene,{a.x*0.98f,a.y*0.98f,a.z},{b.x*0.98f,b.y*0.98f,b.z},22,rim);
    }
    const std::array<SpatialPoint,6> arch{{{-1450,-650,1050},{-1100,-500,1260},
        {-500,-350,1380},{500,-350,1380},{1100,-500,1260},{1450,-650,1050}}};
    const auto rear=[](SpatialPoint p) {return SpatialPoint{p.x*0.65f,-1500,p.z-250};};
    for(std::size_t i=1;i<arch.size();++i) {
        const auto a=arch[i-1],b=arch[i],ra=rear(a),rb=rear(b);
        cabinRail(scene,a,b,32,rim);
        cabinFace(scene,{a,b,rb,ra},blue);
        cabinFace(scene,{{ra.x,ra.y,floor},{rb.x,rb.y,floor},rb,ra},dark);
    }
    for(const float side:{-1.0f,1.0f}) {
        const SpatialPoint shoulder{side*1450,-650,1050},back=rear(shoulder);
        cabinFace(scene,{{side*1600,-750,-430},{side*900,-1500,-380},back,shoulder},blue);
        cabinRail(scene,{side*1560,-760,-430},shoulder,38,rim);
        cabinRail(scene,shoulder,{side*1300,1450,1000},25,rim);
        cabinRail(scene,{side*1300,1450,1000},{side*650,1600,1260},25,rim);
    }
    cabinRail(scene,{-650,1600,1260},{650,1600,1260},25,rim);
    for(const float side:{-1.0f,1.0f}) {
        struct Station {float y,inner,outer,innerZ;};
        constexpr float consoleBankSlope=0.35f;
        constexpr std::array<Station,5> contour{{{-1050,850,1180,-720},
            {-650,1030,1450,-730},{0,1140,1560,-730},
            {650,1090,1500,-760},{980,930,1370,-820}}};
        const auto consolePoint=[&](float u,float y,float lift) {
            std::size_t next=1;
            while(next+1<contour.size()&&y>contour[next].y) {++next;}
            const auto a=contour[next-1],b=contour[next];
            const auto t=std::clamp((y-a.y)/(b.y-a.y),0.0f,1.0f);
            const auto inner=a.inner+(b.inner-a.inner)*t,outer=a.outer+(b.outer-a.outer)*t;
            const auto innerZ=a.innerZ+(b.innerZ-a.innerZ)*t;
            return SpatialPoint{side*(inner+(outer-inner)*u),y,innerZ+consoleBankSlope*(outer-inner)*u+lift};
        };
        const auto consolePatch=[&](float u1,float u2,float y1,float y2,Rgb color,float lift) {
            const auto segment=[&](float first,float last) {
                cabinFace(scene,{consolePoint(u1,first,lift),consolePoint(u2,first,lift),
                    consolePoint(u2,last,lift),consolePoint(u1,last,lift)},color);
            };
            // Every band is planar; overlays share its folds instead of cutting through them.
            auto first=y1;
            for(const auto station:contour) {
                if(station.y>first&&station.y<y2) {segment(first,station.y);first=station.y;}
            }
            segment(first,y2);
        };
        const auto base=[&](float u,float y) {
            auto p=consolePoint(u,y,0);p.z=std::max(floor+12,p.z-140);return p;
        };
        for(std::size_t i=1;i<contour.size();++i) {
            const auto first=contour[i-1].y,last=contour[i].y;
            consolePatch(0,1,first,last,blue,0);
            for(const float u:{0.0f,1.0f}) {
                cabinFace(scene,{consolePoint(u,first,0),consolePoint(u,last,0),base(u,last),base(u,first)},dark);
            }
            cabinRail(scene,consolePoint(0.96f,first,5),consolePoint(0.96f,last,5),7,amber);
        }
        for(const float y:{contour.front().y,contour.back().y}) {
            cabinFace(scene,{consolePoint(0,y,0),consolePoint(1,y,0),base(1,y),base(0,y)},dark);
        }
        cabinFace(scene,{consolePoint(0,contour.back().y,0),consolePoint(1,contour.back().y,0),
            {side*1400,980,-650},{side*930,980,-650}},blue);
        // Decorative auxiliary hardware has no duplicated gauges or false HUD readings.
        for(const float y:{-840.0f,-680.0f}) {
            consolePatch(0.1f,0.9f,y-65,y+65,rim,3);
            for(const float u:{0.28f,0.52f,0.76f}) {
                const auto n=normal({consolePoint(u-0.01f,y,0),consolePoint(u+0.01f,y,0),consolePoint(u,y+1,0)});
                const auto a=consolePoint(u,y,8);
                const SpatialPoint b{a.x+n.x*32,a.y+n.y*32,a.z+n.z*32};
                cabinRail(scene,a,b,28,metal);
                auto tangent=difference(consolePoint(u,y+1,0),consolePoint(u,y,0));
                const auto length=std::sqrt(tangent.x*tangent.x+tangent.y*tangent.y+tangent.z*tangent.z);
                tangent={tangent.x/length*12,tangent.y/length*12,tangent.z/length*12};
                const SpatialPoint cap{b.x+n.x*4,b.y+n.y*4,b.z+n.z*4};
                cabinRail(scene,{cap.x-tangent.x,cap.y-tangent.y,cap.z-tangent.z},
                    {cap.x+tangent.x,cap.y+tangent.y,cap.z+tangent.z},3,amber);
            }
        }
        consolePatch(0.08f,0.92f,-410,180,rim,3);
        for(unsigned slider=0;slider<3;++slider) {
            const auto u=0.24f+slider*0.26f;
            consolePatch(u-0.035f,u+0.035f,-345,110,dark,6);
            const auto y=-260.0f+slider*115;
            cabinRail(scene,consolePoint(u-0.085f,y,16),consolePoint(u+0.085f,y,16),9,metal);
            cabinRail(scene,consolePoint(u,y-10,27),consolePoint(u,y+10,27),2,teal);
        }
        consolePatch(0.08f,0.92f,300,580,rim,3);
        constexpr std::uint64_t lampPhaseTicks=8;
        for(unsigned lamp=0;lamp<6;++lamp) {
            const auto u=0.23f+(lamp%3)*0.27f,y=360.0f+(lamp/3)*120;
            const auto on=(tick/lampPhaseTicks+lamp+(side<0?0:1))%4<2;
            const auto color=on?(lamp%3==0?amber:lamp%3==1?teal:red):dark;
            consolePatch(u-0.08f,u+0.08f,y-26,y+26,dark,7);
            consolePatch(u-0.045f,u+0.045f,y-13,y+13,color,10);
            scene.surfaces.back().lighting=1;
        }
    }
    cabinFace(scene,{{-440,-1480,-700},{440,-1480,-700},{520,-1480,230},
        {310,-1480,650},{-310,-1480,650},{-520,-1480,230}},rim);
    cabinFace(scene,{{-390,-1470,-650},{390,-1470,-650},{465,-1470,210},
        {275,-1470,590},{-275,-1470,590},{-465,-1470,210}},blue);
    cabinRail(scene,{0,-1450,-620},{0,-1450,570},7,metal);
    cabinRail(scene,{-200,-1440,430},{200,-1440,430},10,amber);
    for(const float side:{-1.0f,1.0f}) {
        cabinFace(scene,{{side*590,-1450,-350},{side*800,-1450,-350},
            {side*780,-1450,300},{side*620,-1450,450}},teal);
        cabinFace(scene,{{side*400,-980,-650},{side*270,-1160,-170},
            {side*340,-1230,-190},{side*460,-1060,-680}},dark);
    }
    box(scene,{-350,-950,-830},{350,-500,-680},dark);
    cabinFace(scene,{{-340,-970,-650},{340,-970,-650},{250,-1150,-210},{-250,-1150,-210}},red);
    cabinFace(scene,{{-330,-930,-670},{330,-930,-670},{360,-520,-650},{-360,-520,-650}},red);
}
void ravenNose(SpatialScene& scene,const std::array<unsigned,2>& shotAges) {
    constexpr Rgb blue{36,42,84},dark{15,20,40},metal{104,112,128},teal{55,146,156};
    for(const float side:{-1.0f,1.0f}) {
        const SpatialPoint a{side*360,1500,-700},b{side*1420,1500,-680};
        const SpatialPoint c{side*920,2650,-670},d{side*300,2500,-620};
        cabinFace(scene,{a,b,c,d},blue);
        cabinFace(scene,{b,{b.x,b.y,-900},{c.x,c.y,-900},c},dark);
        cabinFace(scene,{d,c,{c.x,c.y,-900},{d.x,d.y,-900}},metal);
        cabinFace(scene,{a,d,{d.x,d.y,-900},{a.x,a.y,-900}},dark);
        cabinRail(scene,{side*1280,1640,-674},{side*900,2490,-655},10,teal);
    }
    cabinRail(scene,{0,1500,-640},cockpitMuzzle,60,metal);
    cabinRail(scene,cockpitMuzzle,{0,cockpitMuzzle.y+12,cockpitMuzzle.z},44,dark);
    constexpr unsigned flashTicks=2;
    if(std::any_of(shotAges.begin(),shotAges.end(),[flashTicks](unsigned age) {return age>0&&age<=flashTicks;})) {
        constexpr float flashRadius=105;
        const auto p=cockpitMuzzle;
        cabinFace(scene,{{p.x-flashRadius,p.y+18,p.z},{p.x,p.y+18,p.z+flashRadius},
            {p.x+flashRadius,p.y+18,p.z},{p.x,p.y+18,p.z-flashRadius}},Rgb{255,190,80});
        scene.surfaces.back().lighting=1;
    }
}
void cockpit(SpatialScene& scene,const IndexedScreen& indexed,const SpatialCockpitLayout& layout) {
    const auto firstSurface=scene.surfaces.size(),firstPanel=scene.billboards.size();
    constexpr Rgb blue{36,42,84},metal{104,112,128},dark{15,20,40};
    cabinHousing(scene,{{{-1400,1370,-440},{-1400,980,-650},{-470,980,-650},{-470,1370,-440}}},60,blue,dark);
    cabinHousing(scene,{{{470,1370,-440},{470,980,-650},{1400,980,-650},{1400,1370,-440}}},60,blue,dark);
    cabinHousing(scene,{{{-470,1480,-410},{-470,960,-790},{470,960,-790},{470,1480,-410}}},60,metal,dark);
    cabinFace(scene,{{-860,1650,1300},{-860,1650,1030},{860,1650,1030},{860,1650,1300}},dark);
    panel(scene,crop(indexed,0,146,96,54),{{{-1350,1300,-465},{-1350,1000,-630},{-500,1000,-630},{-500,1300,-465}}});
    panel(scene,crop(indexed,96,128,116,72),{{{-420,1405,-450},{-420,995,-750},{420,995,-750},{420,1405,-450}}});
    panel(scene,crop(indexed,212,146,108,54),{{{500,1300,-465},{500,1000,-630},{1350,1000,-630},{1350,1300,-465}}});
    panel(scene,crop(indexed,64,0,192,24),{{{-800,1640,1260},{-800,1640,1060},{800,1640,1060},{800,1640,1260}}});
    cabinShell(scene,scene.world->tick);
    ravenNose(scene,scene.world->playerShotAges);
    const auto resize=[&](SpatialPoint& p) {p.x*=layout.cabinScale;p.y*=layout.cabinScale;p.z*=layout.cabinScale;};
    for(auto i=firstSurface;i<scene.surfaces.size();++i) {for(auto& point:scene.surfaces[i].points) {resize(point);}}
    for(auto i=firstPanel;i<scene.billboards.size();++i) {for(auto& point:*scene.billboards[i].corners) {resize(point);}}
    // Fixed weapon axis at finite range; head motion does not steer the gun.
    constexpr float aimDepth=12000,aimHalf=150,aimGap=65;
    for(const auto& points:std::array<std::vector<SpatialPoint>,4>{{
        {{-aimHalf,aimDepth,-float(layout.eyeHeight)},{-aimGap,aimDepth,-float(layout.eyeHeight)}},
        {{aimGap,aimDepth,-float(layout.eyeHeight)},{aimHalf,aimDepth,-float(layout.eyeHeight)}},
        {{0,aimDepth,-float(layout.eyeHeight)-aimHalf},{0,aimDepth,-float(layout.eyeHeight)-aimGap}},
        {{0,aimDepth,-float(layout.eyeHeight)+aimGap},{0,aimDepth,-float(layout.eyeHeight)+aimHalf}}}}) {
        SpatialSurface marker;marker.points=points;marker.filled=false;marker.tint=Rgb{255,230,100};scene.surfaces.push_back(std::move(marker));
    }
}
void environment(SpatialScene& scene,const SpatialWorld& world,bool detail) {
    scene.background.assign(64000,world.skyColor);
    const auto x=float(world.anchor.x),y=float(world.anchor.y);
    SpatialSurface ground;ground.color=world.groundColor;ground.material=detail?SpatialMaterial::groundDetail:SpatialMaterial::ground;
    for(const auto point:std::array<SpatialPoint,4>{{{x-groundRadius,y-groundRadius,spatialGroundHeight},
        {x+groundRadius,y-groundRadius,spatialGroundHeight},{x+groundRadius,y+groundRadius,spatialGroundHeight},
        {x-groundRadius,y+groundRadius,spatialGroundHeight}}}) {
        ground.points.push_back(spatialFromWorld(point,scene.camera));
        // Simulation rebases actors around the player. Retain that translation
        // so the authored terrain pattern belongs to the world, not the seat.
        ground.uv.push_back({(point.x+float(world.translation[0]%groundPatternRepeat))/groundTileSize,
            (point.y+float(world.translation[1]%groundPatternRepeat))/groundTileSize});
    }
    scene.surfaces.push_back(std::move(ground));
    if(world.panorama.empty()) {return;}
    const auto height=world.panorama.front().height;
    const auto width=world.panorama.size()*256;
    for(const auto& strip:world.panorama) {
        if(strip.width!=256||strip.height!=height||strip.pixels.size()!=256*std::size_t(height)) {throw std::invalid_argument("Invalid spatial panorama");}
    }
    constexpr double circle=6.283185307179586;
    for(std::size_t segment=0;segment<panoramaSegments;++segment) {
        const auto first=segment*width/panoramaSegments,last=(segment+1)*width/panoramaSegments;
        IndexedBitmap image{static_cast<std::uint16_t>(last-first),height,{}};
        for(std::size_t row=0;row<height;++row) {for(auto column=first;column<last;++column) {
            image.pixels.push_back(world.panorama[column/256].pixels[row*256+column%256]);
        }}
        const auto edge=[&](std::size_t column,float z) {
            const auto angle=circle*(double(column)-160)/width;
            return spatialFromWorld({x+panoramaRadius*float(std::sin(angle)),y+panoramaRadius*float(std::cos(angle)),z},scene.camera);
        };
        const auto top=spatialGroundHeight+panoramaRadius*height/128;
        panel(scene,std::move(image),{{edge(first,top),edge(first,spatialGroundHeight),edge(last,spatialGroundHeight),edge(last,top)}});
    }
}
}
SpatialPoint spatialFromWorld(const SpatialPoint point,const ViewTransform& camera) {
    const auto r=rotation(camera);const double x=double(point.x)-camera.origin.x,y=double(point.y)-camera.origin.y,z=double(point.z)-camera.origin.z;
    return {float(r[0]*x+r[3]*y+r[6]*z),float(r[1]*x+r[4]*y+r[7]*z),float(r[2]*x+r[5]*y+r[8]*z)};
}
SpatialPoint spatialToWorld(const SpatialPoint point,const ViewTransform& camera) {
    const auto r=rotation(camera);const auto a=r[0],b=r[3],c=r[6],d=r[1],e=r[4],f=r[7],g=r[2],h=r[5],i=r[8];
    const auto determinant=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
    if(std::abs(determinant)<1e-8) {throw std::invalid_argument("Singular spatial camera");}
    return {float(camera.origin.x+((e*i-f*h)*point.x+(c*h-b*i)*point.y+(b*f-c*e)*point.z)/determinant),
        float(camera.origin.y+((f*g-d*i)*point.x+(a*i-c*g)*point.y+(c*d-a*f)*point.z)/determinant),
        float(camera.origin.z+((d*h-e*g)*point.x+(b*g-a*h)*point.y+(a*e-b*d)*point.z)/determinant)};
}
ViewTransform spatialPilotCamera(const Level1Entity& player) {
    auto camera=initialLevel1MainView();camera.origin=player.position;
    if(camera.origin.z>std::numeric_limits<std::int32_t>::max()-spatialPilotEyeHeight) {throw std::invalid_argument("Spatial pilot height overflow");}
    camera.origin.z+=spatialPilotEyeHeight;camera.viewport={0,319,0,199};
    camera.matrix=makeFixedMatrixQ14(player.pitch,player.roll,player.yaw);transpose(camera.matrix);return camera;
}
std::vector<SpatialLight> spatialExplosionLights(const std::vector<DestructionCue>& cues,std::uint64_t tick,
    std::size_t level,const ViewTransform& camera) {
    std::vector<SpatialLight> result;
    for(auto cue=cues.rbegin();cue!=cues.rend()&&result.size()<maximumLocalLights;++cue) {
        if(cue->level!=level||tick<cue->born||tick-cue->born>=explosionLightTicks) {continue;}
        const auto age=tick-cue->born;
        result.push_back({spatialFromWorld({float(cue->position.x),float(cue->position.y),float(cue->position.z+100)},camera),
            age<2?Rgb{255,218,142}:Rgb{255,100,24},std::clamp(float(cue->radius)*4,2400.0f,12000.0f),
            float(1-double(age)/explosionLightTicks)*0.35f});
    }
    return result;
}
SpatialScene makeSpatialView(const SpatialScene& source,const IndexedScreen& indexed,const EnhancedView view,const SpatialOptions& options,const SpatialCockpitLayout& layout) {
    if(indexed.pixels.size()!=64000||indexed.palette.size()!=256||!source.world) {throw std::invalid_argument("Spatial view requires source world context and indexed instruments");}
    SpatialScene scene=source;
    if(!std::isfinite(layout.cabinScale)||layout.cabinScale<=0) {throw std::invalid_argument("Invalid spatial cabin scale");}
    if(view==EnhancedView::cockpit) {
        scene.camera=source.world->pilotCamera;scene.overlay.assign(64000,-1);
        const auto z=std::int64_t(scene.camera.origin.z)+layout.eyeHeight-spatialPilotEyeHeight;
        if(z<std::numeric_limits<std::int32_t>::min()||z>std::numeric_limits<std::int32_t>::max()) {throw std::invalid_argument("Spatial seat height overflow");}
        scene.camera.origin.z=static_cast<std::int32_t>(z);
    }
    const auto rebase=[&](SpatialPoint point,float lift=0) {
        if(view==EnhancedView::chase&&lift==0) {return point;}
        auto world=spatialToWorld(point,source.camera);world.z+=lift;return spatialFromWorld(world,scene.camera);
    };
    scene.surfaces.clear();
    for(auto surface:source.surfaces) {
        if(view==EnhancedView::cockpit&&surface.type==0&&surface.allocation==std::numeric_limits<std::size_t>::max()) {continue;}
        for(auto& point:surface.points) {point=rebase(point,surface.material==SpatialMaterial::shadow?shadowLift:surface.floorContactLift);}
        if(surface.playerShotCenter) {
            surface.playerShotCenter=rebase(*surface.playerShotCenter);
            const auto center=*surface.playerShotCenter;
            const auto axialZ=center.z+float(layout.eyeHeight);
            const auto range=std::sqrt(center.x*center.x+center.y*center.y+axialZ*axialZ);
            const auto join=cockpitShotJoinDistance*layout.cabinScale;
            if(view==EnhancedView::cockpit&&center.y>=0&&range<join) {
                // Cosmetic near-field convergence from the deck cannon; native hits stay on their recovered path.
                const auto muzzle=cockpitMuzzle.y*layout.cabinScale;
                const auto amount=std::clamp((join-range)/(join-muzzle),0.0f,1.0f);
                const auto forward=std::max(0.0f,muzzle-center.y);
                const auto drop=(cockpitMuzzle.z*layout.cabinScale+float(layout.eyeHeight))*amount;
                for(auto& point:surface.points) {point.y+=forward;point.z+=drop;}
                surface.playerShotCenter->y+=forward;
                surface.playerShotCenter->z+=drop;
            }
        }
        scene.surfaces.push_back(std::move(surface));
    }
    for(auto& billboard:scene.billboards) {
        billboard.center=rebase(billboard.center);
        if(billboard.corners) {for(auto& point:*billboard.corners) {point=rebase(point);}}
    }
    if(!options.localLights) {scene.lights.clear();}
    else {for(auto& light:scene.lights) {light.position=rebase(light.position);}}
    environment(scene,*source.world,options.groundDetail);
    if(view==EnhancedView::cockpit) {cockpit(scene,indexed,layout);}
    return scene;
}
}
