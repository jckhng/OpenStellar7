#include "chase_view.hpp"
#include "world_presentation.hpp"
#include "playable_jump.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>

namespace stellar7 {
namespace {
constexpr int width=320,height=200;
constexpr double q14=16384;
constexpr double lookAhead=1600,targetHeight=300;
constexpr double collisionAnchorHeight=600,cameraClearance=150,minimumBoom=0.15;
constexpr int hudTop=169;
constexpr std::int16_t fullArmor=680;
constexpr double ambientLight=0.55,directLight=0.45;
constexpr std::size_t shadowSides=12;
constexpr double shadowRadiusRatio=0.35,minimumShadowRadius=160,maximumShadowRadius=1100;
constexpr double minimumShadowOpacity=0.12,maximumShadowOpacity=0.34,shadowFadeHeight=22000;
// The Raven and grounded scenery meshes share this base below actor Z=0.
constexpr std::int32_t groundPlaneHeight=spatialGroundHeight;
constexpr double minimumShadowWidth=3,minimumShadowHeight=1;
constexpr double nearPlane=128;
constexpr std::int16_t primarySpeed=500;
constexpr std::size_t primaryProjectileType=23;
constexpr std::uint8_t hiddenMeshFlag=0x80;
using Vector=std::array<double,3>;
Vector subtract(const Vector& a,const Vector& b) {return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
double dot(const Vector& a,const Vector& b) {return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Vector unit(const Vector& value) {
    const auto length=std::sqrt(dot(value,value));
    return {value[0]/length,value[1]/length,value[2]/length};
}
Vector cross(const Vector& a,const Vector& b) {
    return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};
}
std::uint8_t ink(const std::vector<Rgb>& palette,const Rgb target) {
    std::size_t best=0;auto distance=std::numeric_limits<int>::max();
    for(std::size_t index=0;index<palette.size();++index) {
        const auto color=palette[index];
        const int r=int(color.red)-target.red,g=int(color.green)-target.green,b=int(color.blue)-target.blue;
        const auto squared=r*r+g*g+b*b;
        if(squared<distance) {best=index;distance=squared;}
    }
    return static_cast<std::uint8_t>(best);
}
FixedPosition primaryAim(const GameplayAssets& assets,const GameplayModel& model) {
    const auto& player=model.level2Player();
    Level1Entity shot{};shot.position=player.position;
    shot.pitch=player.pitch;shot.roll=player.roll;shot.yaw=player.yaw;shot.forwardVelocity=primarySpeed;
    const auto radius=assets.models.at(primaryProjectileType).collisionRadius;
    const auto profile=playablePrimaryProfile(model.level1State().systemTimers()[5]);
    // Freeze target poses: this is a range guide, not a prediction of future hits.
    for(unsigned tick=1;tick<profile.lifetime;++tick) {
        advanceEntityKinematics(shot);
        for(const auto& actor:model.level2PresentationEntities()) {
            const auto category=actor.stateBytes4dTo52[0];
            if(!actor.active()||(category!=0xff&&category!=0xfe)||actor.type==7||actor.type==46
                ||(actor.flags&hiddenMeshFlag)!=0) {continue;}
            const auto threshold=std::int64_t{(radius+actor.collisionRadius)/2};
            if(std::abs(std::int64_t{shot.position.x}-actor.position.x)<=threshold
                &&std::abs(std::int64_t{shot.position.y}-actor.position.y)<=threshold
                &&std::abs(std::int64_t{shot.position.z}-actor.position.z)<=threshold) {return shot.position;}
        }
    }
    return shot.position;
}
}
double enhancedSurfaceLighting(const std::vector<FixedPosition>& polygon,const FixedPosition& light) {
    if(polygon.size()<3) {return 1;}
    const auto vector=[](FixedPosition value) {return Vector{double(value.x),double(value.y),double(value.z)};};
    const auto origin=vector(polygon.front());
    for(std::size_t index=2;index<polygon.size();++index) {
        auto normal=cross(subtract(vector(polygon[index-1]),origin),subtract(vector(polygon[index]),origin));
        if(dot(normal,normal)==0) {continue;}
        if(dot(normal,origin)>0) {for(auto& component:normal) {component=-component;}}
        const auto direction=vector(light);
        if(dot(direction,direction)==0) {return 1;}
        return ambientLight+directLight*std::max(0.0,dot(unit(normal),unit(direction)));
    }
    return 1;
}
std::optional<PresentationFace> vehicleShadow(const Level1Entity& actor,const ViewTransform& view,
    const std::vector<Rgb>& palette) {
    const bool vehicle=actor.type==0||(actor.type>=1&&actor.type<=16)||actor.type==44||actor.type==45;
    if(!vehicle||!actor.active()||(actor.flags&hiddenMeshFlag)!=0) {return std::nullopt;}
    const auto radius=std::clamp(actor.collisionRadius*shadowRadiusRatio,minimumShadowRadius,maximumShadowRadius);
    PresentationFace shadow{{},view.viewport,ink(palette,{0,0,0}),true,{}};
    shadow.opacity=std::clamp(maximumShadowOpacity-std::max(0,actor.position.z)/shadowFadeHeight,
        minimumShadowOpacity,maximumShadowOpacity);
    const auto centerX=(view.viewport.left+view.viewport.right)/2,centerY=(view.viewport.top+view.viewport.bottom)/2;
    constexpr double circle=6.283185307179586;
    std::vector<FixedPosition> points;
    for(std::size_t index=0;index<shadowSides;++index) {
        const auto angle=circle*index/shadowSides;
        points.push_back(transformToView({actor.position.x+static_cast<std::int32_t>(std::lround(std::cos(angle)*radius)),
            actor.position.y+static_cast<std::int32_t>(std::lround(std::sin(angle)*radius)),groundPlaneHeight},view));
    }
    if(WorldPresentationCapture::spatialEnabled()) {
        for(const auto point:points) {shadow.spatialPoints.push_back({float(point.x),float(point.y),float(point.z)});}
    }
    // Ground projection stays below the same world XY, independently of the actor's altitude.
    auto previous=points.back();bool previousInside=previous.y>=nearPlane;
    std::vector<Vector> clipped;
    for(const auto point:points) {
        const bool inside=point.y>=nearPlane;
        if(inside!=previousInside) {
            const auto fraction=(nearPlane-previous.y)/(double(point.y)-previous.y);
            clipped.push_back({previous.x+(double(point.x)-previous.x)*fraction,nearPlane,
                previous.z+(double(point.z)-previous.z)*fraction});
        }
        if(inside) {clipped.push_back({double(point.x),double(point.y),double(point.z)});}
        previous=point;previousInside=inside;
    }
    if(clipped.size()<3) {return WorldPresentationCapture::spatialEnabled()?std::optional<PresentationFace>{shadow}:std::nullopt;}
    for(const auto point:clipped) {
        shadow.points.push_back({centerX+point[0]*view.perspectiveScale/point[1],
            centerY-point[2]*view.perspectiveScale/point[1]});
    }
    const auto ground=transformToView({actor.position.x,actor.position.y,groundPlaneHeight},view);
    if(ground.y<nearPlane) {return shadow;}
    const auto anchorX=centerX+double(ground.x)*view.perspectiveScale/ground.y;
    const auto anchorY=centerY-double(ground.z)*view.perspectiveScale/ground.y;
    if(anchorX<view.viewport.left||anchorX>view.viewport.right||anchorY<view.viewport.top||anchorY>view.viewport.bottom) {return shadow;}
    const auto horizontal=std::minmax_element(shadow.points.begin(),shadow.points.end(),[](auto a,auto b) {return a.x<b.x;});
    const auto vertical=std::minmax_element(shadow.points.begin(),shadow.points.end(),[](auto a,auto b) {return a.y<b.y;});
    const auto spanX=horizontal.second->x-horizontal.first->x,spanY=vertical.second->y-vertical.first->y;
    const auto scaleX=spanX>0?std::max(1.0,minimumShadowWidth/spanX):1;
    const auto scaleY=spanY>0?std::max(1.0,minimumShadowHeight/spanY):1;
    // Keep distant altitude cues readable after presentation scaling, at their real ground anchor.
    for(auto& point:shadow.points) {
        point.x=anchorX+(point.x-anchorX)*scaleX;point.y=anchorY+(point.y-anchorY)*scaleY;
    }
    return shadow;
}
ViewTransform chaseCamera(const Level1Entity& player,const std::vector<Level1Entity>& actors,const ChaseOptions options) {
    const auto yaw=makeFixedMatrixQ14(0,0,player.yaw);
    const Vector forward{yaw.values[3]/q14,yaw.values[4]/q14,0};
    const Vector right{yaw.values[0]/q14,yaw.values[1]/q14,0};
    const Vector anchor{double(player.position.x),double(player.position.y),player.position.z+collisionAnchorHeight};
    const auto distance=std::clamp(options.distance,ChaseOptions::minimumDistance,ChaseOptions::maximumDistance);
    const auto cameraHeight=std::clamp(options.height,ChaseOptions::minimumHeight,ChaseOptions::maximumHeight);
    Vector position{player.position.x-forward[0]*distance,player.position.y-forward[1]*distance,
        double(player.position.z+cameraHeight)};
    const auto boom=subtract(position,anchor);const auto squared=dot(boom,boom);
    double fraction=1;
    for(const auto& actor:actors) {
        // Solid FE scenery can shorten the visual boom. Pickups/projectiles cannot.
        if(!actor.active()||actor.stateBytes4dTo52[0]!=0xfe||actor.type<27||actor.type>41) {continue;}
        const double radius=actor.collisionRadius/2.0+cameraClearance;
        const Vector center{double(actor.position.x),double(actor.position.y),actor.position.z+radius/2};
        const auto relative=subtract(center,anchor);
        if(dot(relative,relative)<=radius*radius) {continue;} // Already inside a bay: do not invent an exit.
        const auto along=dot(relative,boom)/squared;
        if(along<=0||along>fraction) {continue;}
        const auto perpendicular=dot(relative,relative)-along*along*squared;
        if(perpendicular>=radius*radius) {continue;}
        const auto entry=along-std::sqrt((radius*radius-perpendicular)/squared);
        fraction=std::min(fraction,std::max(minimumBoom,entry-cameraClearance/std::sqrt(squared)));
    }
    for(std::size_t axis=0;axis<3;++axis) {position[axis]=anchor[axis]+boom[axis]*fraction;}
    const Vector target{player.position.x+forward[0]*lookAhead,player.position.y+forward[1]*lookAhead,
        player.position.z+targetHeight};
    const auto direction=unit(subtract(target,position));const auto up=unit(cross(right,direction));
    ViewTransform view{{static_cast<std::int32_t>(std::lround(position[0])),
        static_cast<std::int32_t>(std::lround(position[1])),static_cast<std::int32_t>(std::lround(position[2]))},
        {{},3},{0,width-1,0,height-1},160};
    const std::array<Vector,3> basis{right,direction,up};
    for(std::size_t axis=0;axis<3;++axis) {
        for(std::size_t input=0;input<3;++input) {
            view.matrix.values[axis+input*3]=static_cast<std::int16_t>(std::lround(basis[axis][input]*q14));
        }
    }
    return view;
}
void drawChaseHud(std::vector<std::uint8_t>& pixels,const GameplayAssets& assets,
    const GameplayModel& model,const ViewTransform& view,const std::vector<Rgb>& palette) {
    const auto dark=ink(palette,{8,12,18}),white=ink(palette,{235,240,245});
    const auto green=ink(palette,{80,255,100}),amber=ink(palette,{255,210,70}),red=ink(palette,{255,65,65});
    const auto pixel=[&](int x,int y,std::uint8_t color) {
        if(x>=0&&x<width&&y>=0&&y<height) {pixels[static_cast<std::size_t>(y)*width+x]=color;}
    };
    const auto text=[&](int x,int y,std::string_view value,std::uint8_t color) {
        const auto& font=assets.scoreFont;
        for(const unsigned char character:value) {
            if(character<font.firstCharacter||character-font.firstCharacter>=font.glyphs.size()) {continue;}
            const auto& glyph=font.glyphs[character-font.firstCharacter];
            for(int row=0;row<glyph.height;++row) {
                for(int column=0;column<glyph.width;++column) {
                    if(glyph.pixels[static_cast<std::size_t>(row)*glyph.width+column]) {pixel(x+column,y+row,color);}
                }
            }
            x+=glyph.width;
        }
    };
    text(5,6,"CHASE  SCORE "+std::to_string(model.score()),white);
    if(model.level2Player().position.z>0) {text(220,6,"AIRBORNE",white);}
    std::fill(pixels.begin()+hudTop*width,pixels.end(),dark);
    text(5,172,"ARMOR "+std::to_string(std::max<std::int16_t>(0,model.playerArmor())),white);
    text(107,172,model.weaponReady()?"GUN READY":"GUN RELOAD",model.weaponReady()?green:amber);
    text(220,172,"LEVEL "+std::to_string(model.campaignLevel()+1),white);
    const auto armor=std::clamp<int>(model.playerArmor(),0,fullArmor);
    for(int x=5;x<5+armor*90/fullArmor;++x) {pixel(x,182,armor<fullArmor/4?red:green);}
    constexpr std::array<const char*,7> labels{"DEC","SHD","JMP","BMB","SCN","SUP","TRB"};
    const auto& systems=model.level1State();
    for(std::size_t index=0;index<labels.size();++index) {
        const auto status=systems.systemStatusLevels()[index];
        const auto active=systems.systemTimers()[index]!=0&&index!=3;
        const auto selected=systems.selectedSystem()==static_cast<std::int16_t>(index);
        text(5+int(index)*44,188,std::string(selected?">":" ")+labels[index]+":"+std::to_string(std::abs(int(status))),
            active?amber:status==0?red:green);
    }
    // Read-only radar: bearings stay relative to the hull, independent of chase-camera pitch.
    constexpr int radarX=291,radarY=141,radarRadius=21,radarRange=30000;
    for(int y=radarY-radarRadius;y<=radarY+radarRadius;++y) {
        for(int x=radarX-radarRadius;x<=radarX+radarRadius;++x) {pixel(x,y,dark);}
    }
    auto radar=initialLevel1MainView();radar.origin=model.level2Player().position;
    radar.matrix=makeFixedMatrixQ14(0,0,model.level2Player().yaw);transpose(radar.matrix);
    for(const auto index:model.level2PresentationActiveListOrder()) {
        if(index>=model.level2PresentationEntities().size()) {continue;}
        const auto& actor=model.level2PresentationEntities()[index];
        if(!actor.active()||actor.type<0||actor.stateBytes4dTo52[0]==0xfd) {continue;}
        const auto point=transformToView(actor.position,radar);
        if(std::abs(std::int64_t{point.x})>radarRange||std::abs(std::int64_t{point.y})>radarRange) {continue;}
        pixel(radarX+point.x*radarRadius/radarRange,radarY-point.y*radarRadius/radarRange,
            actor.stateBytes4dTo52[0]==0xff?red:amber);
    }
    pixel(radarX,radarY,green);pixel(radarX,radarY-1,green);pixel(radarX-1,radarY+1,green);pixel(radarX+1,radarY+1,green);
    const auto& feedback=model.combatFeedback();
    const auto active=[&](const auto& cue) {return cue&&cue->active(model.campaignLevel(),model.ticks());};
    if(active(feedback.hit)) {text(5,20,"HIT",amber);}
    if(active(feedback.damage)) {
        text(5,31,"DAMAGE -"+std::to_string(feedback.damage->amount),red);
        for(int x=0;x<width;++x) {pixel(x,0,red);pixel(x,hudTop-1,red);}
        for(int y=0;y<hudTop;++y) {pixel(0,y,red);pixel(width-1,y,red);}
    }
    const auto aim=transformToView(primaryAim(assets,model),view);
    if(aim.y>128) {
        const auto point=projectToViewport(aim,view);
        if(point.y>=hudTop-6) {return;}
        for(int offset=3;offset<=6;++offset) {
            pixel(point.x-offset,point.y,white);pixel(point.x+offset,point.y,white);
            pixel(point.x,point.y-offset,white);pixel(point.x,point.y+offset,white);
        }
        if(active(feedback.hit)) {
            for(int offset=2;offset<=4;++offset) {
                pixel(point.x-offset,point.y-offset,amber);pixel(point.x+offset,point.y-offset,amber);
                pixel(point.x-offset,point.y+offset,amber);pixel(point.x+offset,point.y+offset,amber);
            }
        }
    }
}
void refreshChaseHud(IndexedScreen& frame,const GameplayAssets& assets,const GameplayModel& model) {
    if(!frame.presentation||!frame.presentation->chase) {return;}
    const auto& old=*frame.presentation->chase;
    if(old.level!=model.campaignLevel()||old.phase!=static_cast<unsigned>(model.phase())) {return;}
    auto pixels=old.source;
    drawChaseHud(pixels,assets,model,chaseCamera(model.level2Player(),model.level2PresentationEntities(),old.camera),frame.palette);
    // Late source writes own the status panel. Keep world annotations at their captured pose.
    std::copy_n(old.source.begin(),hudTop*width,pixels.begin());
    if(pixels==old.source) {return;}
    PresentationDraw overlay;
    for(std::size_t index=0;index<pixels.size();) {
        if(pixels[index]==old.source[index]) {++index;continue;}
        PresentationPatch patch{index,{}};
        do {patch.pixels.push_back(pixels[index]);++index;} while(index<pixels.size()&&pixels[index]!=old.source[index]);
        overlay.patches.push_back(std::move(patch));
    }
    auto chase=std::make_shared<WorldPresentation>(old);chase->source=std::move(pixels);
    if(old.spatial) {
        auto spatial=std::make_shared<SpatialScene>(*old.spatial);
        for(std::size_t i=hudTop*width;i<chase->source.size();++i) {spatial->overlay[i]=chase->source[i];}
        chase->spatial=std::move(spatial);
    }
    chase->draws.push_back(std::move(overlay));
    auto presentation=std::make_shared<WorldPresentation>(*frame.presentation);
    presentation->chase=std::move(chase);frame.presentation=std::move(presentation);
}
}
