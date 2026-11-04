#define SDL_MAIN_HANDLED
#include "gl_scene_renderer.hpp"
#include "world_presentation.hpp"
#include "spatial_view.hpp"
#include "playable_startup_resources.hpp"
#include "playable_campaign_comparison.hpp"
#include "failure_page.hpp"
#ifdef STELLAR7_OPENXR_TESTS
#include "openxr_test_runtime.hpp"
#include "openxr_pump.hpp"
#endif
#include <SDL_opengl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace stellar7 {
struct GameplayModelTestAccess {
    static std::unique_ptr<GameplayModel> sled(const GameplayAssets& assets) {
        auto model=std::make_unique<GameplayModel>();model->phase_=GameplayPhase::playable;
        model->playableCampaignFrames_=true;model->level2Player_.stateBytes4dTo52[1]=1;model->level2PlayerArmor_=680;
        Level1Entity actor{};actor.type=1;actor.position={0,3000,0};actor.collisionRadius=assets.models.at(1).collisionRadius;
        actor.stateBytes4dTo52={0xff,100,0,0,1,0};actor.renderProfile=WorldActorRenderProfile::noHandles;
        model->level2PresentationEntities_={actor};model->level2PresentationActiveListOrder_={0};return model;
    }
    static std::unique_ptr<GameplayModel> scenery(const GameplayAssets& assets,std::uint16_t yaw) {
        auto model=sled(assets);model->level2Player_.yaw=yaw;
        auto& actor=model->level2PresentationEntities_.front();actor.type=29;actor.position={0,6000,0};
        actor.collisionRadius=assets.models.at(29).collisionRadius;return model;
    }
    static std::unique_ptr<GameplayModel> energy(const GameplayAssets& assets,std::int16_t type) {
        auto model=sled(assets);auto& actor=model->level2PresentationEntities_.front();
        actor.type=type;actor.position={0,6000,0};actor.allocationIndex=0;
        actor.collisionRadius=assets.models.at(type).collisionRadius;
        actor.stateBytes4dTo52={0xfd,54,0,0,0,0};
        if(type==7) {
            actor.flags=8;actor.stateBytes4dTo52[5]=3;actor.behaviorTimer=50;
            CompletionBeamPresentation beam{};beam.projected=true;beam.anchor={159,84};
            beam.center={0,6000,0};beam.timer=50;
            for(std::size_t i=0;i<beam.colors.size();++i) {beam.colors[i]=static_cast<std::uint8_t>(128+i);}
            model->level2BeamPresentations_={beam};
        }
        return model;
    }
};
}

namespace {
using namespace stellar7;
void require(bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
void* load(const char* name) {return reinterpret_cast<void*>(eglGetProcAddress(name));}
template<class Procedure> Procedure proc(const char* name) {
    auto value=reinterpret_cast<Procedure>(load(name));require(value!=nullptr,name);return value;
}
struct Context {
    EGLDisplay display=EGL_NO_DISPLAY;EGLSurface surface=EGL_NO_SURFACE;EGLContext context=EGL_NO_CONTEXT;
    explicit Context(GlProfile profile) {
        const auto platform=proc<PFNEGLGETPLATFORMDISPLAYEXTPROC>("eglGetPlatformDisplayEXT");
        display=platform(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,nullptr);
        require(display!=EGL_NO_DISPLAY&&eglInitialize(display,nullptr,nullptr)==EGL_TRUE,"surfaceless EGL unavailable");
        const bool desktop=profile==GlProfile::desktop33;
        require(eglBindAPI(desktop?EGL_OPENGL_API:EGL_OPENGL_ES_API)==EGL_TRUE,"EGL API bind failed");
        const EGLint attributes[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,desktop?EGL_OPENGL_BIT:EGL_OPENGL_ES3_BIT_KHR,
            EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
        EGLConfig config{};EGLint count=0;
        require(eglChooseConfig(display,attributes,&config,1,&count)==EGL_TRUE&&count==1,"EGL config unavailable");
        const EGLint surfaceAttributes[]={EGL_WIDTH,320,EGL_HEIGHT,200,EGL_NONE};
        surface=eglCreatePbufferSurface(display,config,surfaceAttributes);
        const EGLint desktopAttributes[]={EGL_CONTEXT_MAJOR_VERSION_KHR,3,EGL_CONTEXT_MINOR_VERSION_KHR,3,
            EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,EGL_NONE};
        const EGLint esAttributes[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
        context=eglCreateContext(display,config,EGL_NO_CONTEXT,desktop?desktopAttributes:esAttributes);
        require(surface!=EGL_NO_SURFACE&&context!=EGL_NO_CONTEXT&&eglMakeCurrent(display,surface,surface,context)==EGL_TRUE,"EGL context unavailable");
    }
    ~Context() {
        if(display!=EGL_NO_DISPLAY) {
            eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
            if(context!=EGL_NO_CONTEXT) {eglDestroyContext(display,context);}
            if(surface!=EGL_NO_SURFACE) {eglDestroySurface(display,surface);}
            eglTerminate(display);
        }
    }
};
struct Target {
    GLuint fbo=0,color=0,depth=0;
    int width,height;
    explicit Target(int w=320,int h=200):width(w),height(h) {
        proc<decltype(&glGenTextures)>("glGenTextures")(1,&color);
        proc<decltype(&glBindTexture)>("glBindTexture")(GL_TEXTURE_2D,color);
        proc<decltype(&glTexImage2D)>("glTexImage2D")(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        proc<PFNGLGENFRAMEBUFFERSPROC>("glGenFramebuffers")(1,&fbo);
        proc<PFNGLBINDFRAMEBUFFERPROC>("glBindFramebuffer")(GL_FRAMEBUFFER,fbo);
        proc<PFNGLFRAMEBUFFERTEXTURE2DPROC>("glFramebufferTexture2D")(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
        proc<PFNGLGENRENDERBUFFERSPROC>("glGenRenderbuffers")(1,&depth);
        proc<PFNGLBINDRENDERBUFFERPROC>("glBindRenderbuffer")(GL_RENDERBUFFER,depth);
        proc<PFNGLRENDERBUFFERSTORAGEPROC>("glRenderbufferStorage")(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);
        proc<PFNGLFRAMEBUFFERRENDERBUFFERPROC>("glFramebufferRenderbuffer")(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
        require(proc<PFNGLCHECKFRAMEBUFFERSTATUSPROC>("glCheckFramebufferStatus")(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"test framebuffer incomplete");
    }
    ~Target() {
        proc<PFNGLDELETEFRAMEBUFFERSPROC>("glDeleteFramebuffers")(1,&fbo);
        proc<PFNGLDELETERENDERBUFFERSPROC>("glDeleteRenderbuffers")(1,&depth);
        proc<decltype(&glDeleteTextures)>("glDeleteTextures")(1,&color);
    }
    GlRenderTarget target() const {return {fbo,0,0,width,height};}
    std::vector<std::uint8_t> read() const {
        proc<PFNGLBINDFRAMEBUFFERPROC>("glBindFramebuffer")(GL_FRAMEBUFFER,fbo);
        std::vector<std::uint8_t> pixels(width*height*4);
        proc<decltype(&glReadPixels)>("glReadPixels")(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());return pixels;
    }
};
void ppm(const std::filesystem::path& path,const std::vector<std::uint8_t>& rgba) {
    std::ofstream out(path,std::ios::binary);out<<"P6\n320 200\n255\n";
    for(int y=199;y>=0;--y) {for(int x=0;x<320;++x) {out.write(reinterpret_cast<const char*>(rgba.data()+(y*320+x)*4),3);}}
    require(bool(out),"cannot write GL preview");
}
SpatialSurface quad(float depth,float half,std::uint8_t color) {
    return {{{-half,depth,-half},{half,depth,-half},{half,depth,half},{-half,depth,half}},color};
}
void primitives(GlSceneRenderer& renderer) {
    SpatialScene scene;scene.camera=initialLevel1MainView();scene.camera.viewport={0,319,0,199};
    scene.background.assign(320*200,0);scene.overlay.assign(320*200,-1);
    std::vector<Rgb> palette(256);palette[1]={255,0,0};palette[2]={0,255,0};palette[3]={0,0,255};
    scene.surfaces={quad(800,200,2),quad(1000,300,1)};
    Target left,right;const auto matrix=spatialProjection(scene.camera,4.0/3);
    renderer.prepare(scene,palette);renderer.render(matrix,left.target(),SpatialOverlay::omit);
    auto pixels=left.read();const auto center=(100*320+160)*4;
    require(pixels[center]==0&&pixels[center+1]==255,"depth test depended on submission order");
    auto blue=quad(700,100,3);blue.opacity=0.5;scene.surfaces.push_back(blue);
    scene.overlay[0]=1;renderer.prepare(scene,palette);renderer.render(matrix,left.target());pixels=left.read();
    require(std::abs(int(pixels[center+1])-128)<=1&&std::abs(int(pixels[center+2])-128)<=1,"GL alpha composition failed");
    require(pixels[199*320*4]==255,"indexed HUD overlay orientation/opacity failed");
    const auto held=pixels;renderer.render(matrix,right.target(),SpatialOverlay::omit);
    require(left.read()==held&&right.read()[199*320*4]==0,"independent framebuffer/overlay contract failed");
    renderer.renderTexture(matrix,right.color,320,200);
    require(right.read()[199*320*4]==0,"borrowed runtime texture inherited a head-locked overlay");
    require(proc<decltype(&glIsTexture)>("glIsTexture")(right.color)==GL_TRUE,"borrowed runtime texture ownership lost");
    const auto borrowed=right.read();
    require(std::abs(int(borrowed[center+1])-128)<=1&&std::abs(int(borrowed[center+2])-128)<=1,"borrowed runtime target lost depth/alpha");
    renderer.renderScaled(matrix,left.target(),160,100,SpatialOverlay::omit);pixels=left.read();
    require(std::abs(int(pixels[center+1])-128)<=1&&pixels[199*320*4]==0,"scaled world target lost geometry/overlay policy");
    renderer.renderScaled(matrix,right.target(),640,400);
    require(right.read()[199*320*4]==255,"resized world target lost HUD orientation");
    scene.surfaces={quad(1000,64,1)};scene.overlay.assign(320*200,-1);renderer.prepare(scene,palette);
    renderer.render(spatialProjection(scene.camera,4.0/3,{-32,0,0}),left.target(),SpatialOverlay::omit);
    renderer.render(spatialProjection(scene.camera,4.0/3,{32,0,0}),right.target(),SpatialOverlay::omit);
    const auto centroid=[](const std::vector<std::uint8_t>& data) {
        double sum=0,count=0;for(std::size_t i=0;i<data.size();i+=4) {if(data[i]>200) {sum+=(i/4)%320;++count;}}
        require(count>0,"stereo control mesh absent");return sum/count;
    };
    require(centroid(left.read())>centroid(right.read())+5,"stereo views did not show physical parallax");
    scene.surfaces[0].points[0].y=64;renderer.prepare(scene,palette);renderer.render(matrix,left.target());
    auto invalid=matrix;invalid[0]=std::numeric_limits<float>::quiet_NaN();bool rejected=false;
    try {renderer.render(invalid,left.target());} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"non-finite view matrix reached driver");
    palette[4]={80,80,80};scene.surfaces={quad(1000,200,4)};renderer.prepare(scene,palette);
    renderer.render(matrix,left.target(),SpatialOverlay::omit);const auto dark=left.read();
    scene.lights={{{0,700,100},{255,180,80},1500,0.8f}};renderer.prepare(scene,palette);
    renderer.render(matrix,right.target(),SpatialOverlay::omit);const auto lit=right.read();
    require(lit[center]>dark[center]+5&&lit[center]>lit[center+2],"local light failed to shade the actual 3D surface");
    std::cout<<"GL depth/alpha/overlay/FBO isolation/stereo/near-clip controls pass\n";
}
void energyReadability(GlSceneRenderer& renderer,const std::filesystem::path& root) {
    const auto assets=naturalRoute::assets(root);
    for(const std::int16_t type:{7,17,20}) {
        auto model=GameplayModelTestAccess::energy(assets,type);IndexedScreen indexed;
        {WorldPresentationCapture capture({},PresentationGeometry::spatial);indexed=renderPlayableLevel2ViewportIndexed(assets,*model);}
        require(indexed.presentation&&indexed.presentation->chase&&indexed.presentation->chase->spatial,"energy fixture lacks captured geometry");
        auto raster=std::make_shared<WorldPresentation>(*indexed.presentation->chase);
        raster->base.assign(64000,0);raster->source.assign(64000,0);
        raster->draws.erase(std::remove_if(raster->draws.begin(),raster->draws.end(),[](const auto& draw) {
            return !draw.face||draw.face->rasterStrokeWidth==0;
        }),raster->draws.end());
        require(!raster->draws.empty(),"recovered energy omitted its native raster pen width");
        for(auto& draw:raster->draws) {
            draw.patches.clear();draw.face->color=1;draw.face->tint.reset();draw.face->lighting=1;draw.face->opacity=1;
        }
        IndexedScreen isolated{std::vector<std::uint8_t>(64000),std::vector<Rgb>(256),raster};isolated.palette[1]={255,255,255};
        const auto countRaster=[&](unsigned scale) {
            const auto pixels=renderWorldPresentation(isolated,scale);
            return std::count(pixels.begin(),pixels.end(),0xffffffffU);
        };
        const auto raster2=countRaster(2),raster4=countRaster(4);
        require(raster2>0&&raster4>raster2*3,"CPU recovered energy loses angular width at higher resolution");
        std::cout<<"CPU energy type="<<type<<" coverage2x="<<raster2<<" coverage4x="<<raster4<<'\n';
        const auto& captured=*indexed.presentation->chase->spatial;
        auto scene=xrWorld(captured,indexed,EnhancedView::cockpit,{},xrDefaultUnitsPerMetre).scene;
        scene.billboards.clear();scene.lights.clear();scene.background.assign(64000,0);scene.overlay.assign(64000,-1);
        scene.surfaces.erase(std::remove_if(scene.surfaces.begin(),scene.surfaces.end(),[&](const auto& s) {
            return type==7 ? !(s.type==0&&!s.filled&&s.color>=128&&s.color<144)
                : s.type!=type;
        }),scene.surfaces.end());
        require(!scene.surfaces.empty(),"recovered energy model has no line/point primitives to qualify");
        for(auto& surface:scene.surfaces) {surface.tint=Rgb{255,0,255};surface.lighting=1;}
        std::vector<Rgb> palette(256);renderer.prepare(scene,palette);
        const auto coverage=[&](int width,int height,float eyeX) {
            Target target(width,height);stellar7::XrView eye{{},{-0.8f,0.8f,0.6f,-0.6f}};eye.pose.position.x=eyeX;
            renderer.render(xrClipFromNative(eye,{},xrWorldViewScale(xrDefaultUnitsPerMetre)),target.target(),SpatialOverlay::omit);
            const auto pixels=target.read();unsigned count=0;
            for(std::size_t i=0;i<pixels.size();i+=4) {count+=pixels[i]==255&&pixels[i+1]==0&&pixels[i+2]==255;}
            return count;
        };
        for(const float eyeX:{-0.032f,0.032f}) {
            const auto small=coverage(320,200,eyeX),large=coverage(1280,800,eyeX);
            std::cout<<"Energy type="<<type<<" eye="<<eyeX<<" coverage320="<<small<<" coverage1280="<<large<<'\n';
            require(small>0&&large>small*8,"energy strokes retain headset-pixel width instead of source angular coverage");
        }
    }
}
#ifdef STELLAR7_OPENXR_TESTS
void cockpitSurvey(GlSceneRenderer& renderer,const XrPresentation& cockpit,
    const std::filesystem::path& output,const std::string& label) {
    struct Direction {const char* name;float yaw;float pitch=0,lean=0;};
    constexpr float quarterTurn=1.57079632679f;
    Target target;renderer.prepare(cockpit.scene,cockpit.palette);
    std::vector<std::uint8_t> previous;
    for(const auto direction:std::array<Direction,10>{{{"front",0},{"left",quarterTurn},{"rear",2*quarterTurn},{"right",-quarterTurn},
        {"front-left",quarterTurn/2,-0.25f},{"front-right",-quarterTurn/2,-0.25f},
        {"side-left",quarterTurn*2/3,-0.25f},{"side-right",-quarterTurn*2/3,-0.25f},
        {"lean-left",quarterTurn/2,-0.25f,-0.18f},{"lean-right",-quarterTurn/2,-0.25f,0.18f}}}) {
        const auto sy=std::sin(direction.yaw/2),cy=std::cos(direction.yaw/2),sp=std::sin(direction.pitch/2),cp=std::cos(direction.pitch/2);
        stellar7::XrView eye{{{cy*sp,sy*cp,-sy*sp,cy*cp},{direction.lean,0,0}},{-0.8f,0.8f,0.6f,-0.6f}};
        renderer.render(xrClipFromNative(eye,{},xrWorldViewScale(xrDefaultUnitsPerMetre)),target.target(),SpatialOverlay::omit);
        const auto pixels=target.read();
        require(pixels!=previous,"cockpit survey ignored the head direction");
        ppm(output/(label+"-cabin-"+direction.name+".ppm"),pixels);previous=pixels;
    }
}
void xrGroundedMeshStability(const std::filesystem::path& root) {
    const auto assets=naturalRoute::assets(root);float error=0;
    for(unsigned yaw=0;yaw<=8192;yaw+=1024) {
        const auto model=GameplayModelTestAccess::scenery(assets,static_cast<std::uint16_t>(yaw));
        IndexedScreen indexed;
        {WorldPresentationCapture capture({},PresentationGeometry::spatial);indexed=renderPlayableLevel2ViewportIndexed(assets,*model);}
        require(indexed.presentation&&indexed.presentation->chase&&indexed.presentation->chase->spatial,"ground fixture lacks spatial observation");
        const auto& scene=*indexed.presentation->chase->spatial;float lowest=std::numeric_limits<float>::max();
        for(const auto& face:scene.surfaces) {if(face.type==29) {for(const auto p:face.points) {lowest=std::min(lowest,spatialToWorld(p,scene.camera).z);}}}
        error=std::max(error,std::abs(lowest-spatialGroundHeight));
        std::cout<<"Grounded Type29 yaw="<<yaw<<" minimum world Z="<<lowest<<'\n';
    }
    require(error<0.05f,"spatial ground contact moves vertically when the source camera turns");
}
void xrGroundStability(GlSceneRenderer& renderer) {
    SpatialScene scene;scene.background.assign(64000,0);scene.overlay.assign(64000,-1);
    std::vector<Rgb> palette(256);palette[1]={255,0,255};palette[2]={20,100,30};Target target(1024,1024);
    SpatialSurface floor;floor.color=2;floor.material=SpatialMaterial::groundDetail;
    floor.points={{-750000,-750000,-300},{750000,-750000,-300},{750000,750000,-300},{-750000,750000,-300}};
    for(const auto p:floor.points) {floor.uv.push_back({p.x/2048,p.y/2048});}
    unsigned checked=0;double least=1;
    for(const float distance:{1000,10000,40000,100000}) {
        const auto half=distance*0.2f;SpatialSurface patch;patch.color=1;
        patch.points={{-half,distance-half,-300},{half,distance-half,-300},{half,distance+half,-300},{-half,distance+half,-300}};
        for(const float shift:{-0.002f,0.0f,0.002f}) {
            const stellar7::XrView eye{{{std::sin(-0.1f),0,0,std::cos(-0.1f)},{shift,shift,0}},{-0.785398f,0.785398f,0.785398f,-0.785398f}};
            const auto matrix=xrClipFromNative(eye,{},xrWorldViewScale(xrDefaultUnitsPerMetre));
            const auto draw=[&](std::vector<SpatialSurface> faces) {
                scene.surfaces=std::move(faces);renderer.prepare(scene,palette);renderer.renderTexture(matrix,target.color,1024,1024,SpatialOverlay::omit);
                const auto pixels=target.read();unsigned count=0;
                for(std::size_t i=0;i<pixels.size();i+=4) {count+=pixels[i]==255&&pixels[i+1]==0&&pixels[i+2]==255;}
                return count;
            };
            const auto alone=draw({patch}),grounded=draw({patch,floor});
            if(alone<32) {continue;}
            ++checked;least=std::min(least,double(grounded)/alone);
            std::cout<<"Ground contact distance="<<distance<<" shift="<<shift<<" pixels="<<grounded<<'/'<<alone<<'\n';
        }
    }
    require(checked>=8&&least>=0.95,"XR ground depth loses coplanar polygons during small head motion");
}
void xrSledVisibility(GlSceneRenderer& renderer,const std::filesystem::path& root,const std::filesystem::path& output) {
    const auto assets=naturalRoute::assets(root);const auto model=GameplayModelTestAccess::sled(assets);
    IndexedScreen indexed;
    {WorldPresentationCapture capture({},PresentationGeometry::spatial);indexed=renderPlayableLevel2ViewportIndexed(assets,*model);}
    require(indexed.presentation&&indexed.presentation->chase&&indexed.presentation->chase->spatial,"sled fixture lacks recovered spatial geometry");
    for(auto& color:indexed.palette) {color={30,30,30};}
    const auto& source=*indexed.presentation->chase->spatial;
    const auto scene=xrWorld(source,indexed,EnhancedView::cockpit,{},xrDefaultUnitsPerMetre).scene;
    const stellar7::XrView eye{{},{-0.785398f,0.785398f,0.785398f,-0.785398f}};
    const auto matrix=xrClipFromNative(eye,{},xrWorldViewScale(xrDefaultUnitsPerMetre));Target target;
    const auto draw=[&](SpatialScene image,const std::string& name) {
        for(auto& surface:image.surfaces) {if(surface.type==1) {surface.tint=Rgb{255,0,255};surface.lighting=1;}}
        image.lights.clear();renderer.prepare(image,indexed.palette);renderer.renderTexture(matrix,target.color,320,200,SpatialOverlay::omit);
        const auto pixels=target.read();unsigned visible=0;
        for(std::size_t i=0;i<pixels.size();i+=4) {visible+=pixels[i]==255&&pixels[i+1]==0&&pixels[i+2]==255;}
        ppm(output/(name+".ppm"),pixels);return visible;
    };
    auto hull=scene;hull.billboards.clear();hull.surfaces.erase(std::remove_if(hull.surfaces.begin(),hull.surfaces.end(),[](const auto& s) {return s.type!=1;}),hull.surfaces.end());
    const auto alone=draw(hull,"xr-sled-isolated");
    auto floor=scene;floor.billboards.clear();floor.surfaces.erase(std::remove_if(floor.surfaces.begin(),floor.surfaces.end(),[](const auto& s) {return s.type!=1&&s.material!=SpatialMaterial::groundDetail;}),floor.surfaces.end());
    const auto grounded=draw(floor,"xr-sled-ground");const auto cockpit=draw(scene,"xr-sled-cockpit");
    std::cout<<"Sled pixels isolated="<<alone<<" floor="<<grounded<<" cockpit="<<cockpit<<'\n';
    require(alone>0&&grounded*10>=alone*9,"spatial floor hides the recovered sandsled's ground-contact geometry");
    require(cockpit*4>alone*3,"XR dashboard hides most of the recovered sandsled");
    SpatialOptions plain;plain.groundDetail=false;
    auto flat=xrWorld(source,indexed,EnhancedView::cockpit,plain,xrDefaultUnitsPerMetre).scene;
    flat.billboards.clear();flat.surfaces.erase(std::remove_if(flat.surfaces.begin(),flat.surfaces.end(),[](const auto& s) {return s.type!=1&&s.material!=SpatialMaterial::ground;}),flat.surfaces.end());
    require(draw(flat,"xr-sled-plain-ground")*10>=alone*9,"plain spatial ground hides the recovered sandsled");
}
void xrSeatedVisibility(GlSceneRenderer& renderer,GlProfile profile) {
    namespace xrTest=stellar7::test::xr;
    xrTest::Runtime runtime;xrTest::rt=&runtime;runtime.profile=profile;runtime.headPose.position.y=0.1f;
    Target a,b,c,d;runtime.textures={{{{a.color,b.color}},{{c.color,d.color}}}};
    OpenXrSession session(xrTest::getProc,xrTest::graphics(profile),xrWorldViewScale(xrDefaultUnitsPerMetre));
    runtime.events={{XR_SESSION_STATE_READY},{XR_SESSION_STATE_VISIBLE}};
    IndexedScreen indexed{std::vector<std::uint8_t>(64000,3),std::vector<Rgb>(256)};
    indexed.palette[1]={40,80,160};indexed.palette[2]={100,30,10};indexed.palette[3]={200,200,200};
    XrPresentationMailbox mailbox;OpenXrPump pump;
    mailbox.publish(std::make_shared<XrPresentation>(xrPanel(indexed,xrDefaultUnitsPerMetre)));
    pump.step(session,renderer,mailbox);const auto preview=b.read();
    runtime.headPose.position.y=1.6f;runtime.events={{XR_SESSION_STATE_FOCUSED}};
    pump.step(session,renderer,mailbox);
    require(b.read()==preview,"wearing headset moved startup panel onto the floor");
    SpatialScene source;source.camera=initialLevel1MainView();source.background.assign(64000,0);source.overlay.assign(64000,-1);
    SpatialWorld world;world.pilotCamera=source.camera;world.pilotCamera.origin.z=spatialPilotEyeHeight;
    world.skyColor=1;world.groundColor=2;source.world=world;
    auto cockpit=std::make_shared<XrPresentation>(xrWorld(source,indexed,EnhancedView::cockpit,{},xrDefaultUnitsPerMetre));
    mailbox.publish(cockpit);pump.step(session,renderer,mailbox);const auto pixels=b.read();
    unsigned instruments=0;
    for(unsigned y=0;y<100;++y) {for(unsigned x=80;x<240;++x) {
        const auto offset=(y*320+x)*4;
        if(pixels[offset]==200&&pixels[offset+1]==200&&pixels[offset+2]==200) {++instruments;}
    }}
    require(instruments>500,"seated XR view lacks visible cockpit instruments below eye level");
    unsigned centralDisplay=0;
    for(unsigned y=25;y<45;++y) {for(unsigned x=145;x<175;++x) {
        const auto offset=(y*320+x)*4;
        centralDisplay+=pixels[offset]==200&&pixels[offset+1]==200&&pixels[offset+2]==200;
    }}
    require(centralDisplay>500,"cockpit housing occludes the central instrument display");
    std::cout<<"XR low/unfocused to seated/focused panel pixels stable; cockpit instrument pixels="<<instruments<<'\n';
}
void xrPublication(GlSceneRenderer& renderer,GlProfile profile) {
    namespace xrTest=stellar7::test::xr;
    xrTest::Runtime runtime;xrTest::rt=&runtime;runtime.profile=profile;
    Target a,b,c,d;runtime.textures={{{{a.color,b.color}},{{c.color,d.color}}}};
    OpenXrSession session(xrTest::getProc,xrTest::graphics(profile),{1000});
    runtime.events={{XR_SESSION_STATE_READY},{XR_SESSION_STATE_FOCUSED}};
    OpenXrPump pump;XrPresentationMailbox mailbox;
    require(pump.step(session,renderer,mailbox)==OpenXrFrame::empty,"no-content XR startup submitted uninitialized images");
    auto original=std::make_shared<XrPresentation>();original->palette.resize(256);original->palette[1]={255,0,0};
    original->scene.background.assign(64000,0);original->scene.overlay.assign(64000,-1);original->scene.surfaces={quad(2000,600,1)};
    auto replacement=std::make_shared<XrPresentation>(*original);replacement->palette[1]={0,255,0};mailbox.publish(original);
    runtime.onAcquire=[&](unsigned eye) {if(eye==1) {mailbox.publish(replacement);}};
    require(pump.step(session,renderer,mailbox)==OpenXrFrame::submitted,"publication control frame failed");
    constexpr auto center=(100*320+160)*4;
    require(b.read()[center]==255&&c.read()[center]==255,"publication between eyes tore one immutable stereo frame");
    runtime.onAcquire={};require(pump.step(session,renderer,mailbox)==OpenXrFrame::submitted,"latest publication was not admitted on the next XR frame");
    require(b.read()[center+1]==255&&c.read()[center+1]==255&&original->palette[1].red==255,"next frame lost/latest publication mutated its retained predecessor");
    std::cout<<"XR publication between eyes retains one scene/palette; successor admits latest; empty startup owns no images\n";
}
#endif
void cockpitVisibility(GlSceneRenderer& renderer) {
    SpatialScene source;source.camera=initialLevel1MainView();source.camera.viewport={0,319,0,199};
    source.background.assign(64000,0);source.overlay.assign(64000,-1);
    SpatialWorld world;world.pilotCamera=source.camera;world.pilotCamera.origin.z=spatialPilotEyeHeight;
    world.skyColor=1;world.groundColor=2;source.world=world;
    IndexedScreen indexed{std::vector<std::uint8_t>(64000,3),std::vector<Rgb>(256)};
    indexed.palette[1]={40,80,160};indexed.palette[2]={100,30,10};indexed.palette[3]={200,200,200};
    SpatialOptions options;options.groundDetail=false;options.localLights=false;
    const auto scene=makeSpatialView(source,indexed,EnhancedView::cockpit,options);
    Target target;renderer.prepare(scene,indexed.palette);renderer.render(spatialProjection(scene.camera,4.0/3),target.target());
    const auto pixels=target.read();unsigned visible=0,total=0;
    for(int y=40;y<128;++y) {for(int x=40;x<280;++x) {
        const auto offset=((199-y)*320+x)*4;const auto sky=indexed.palette[1],ground=indexed.palette[2];
        visible+=(pixels[offset]==sky.red&&pixels[offset+1]==sky.green&&pixels[offset+2]==sky.blue)
            ||(pixels[offset]==ground.red&&pixels[offset+1]==ground.green&&pixels[offset+2]==ground.blue);++total;
    }}
    require(double(visible)/total>0.85,"3D cockpit blocks the central driving visibility envelope");
    std::cout<<"Cockpit central driving visibility="<<double(visible)/total<<'\n';
}
std::unique_ptr<RecoveredFrontend> begin(const GameplayAssets& assets,const NativeStartupAssets& startup,
    FrameCopyProfile copy,std::size_t budget,std::vector<std::int16_t>& pcm) {
    auto result=std::make_unique<RecoveredFrontend>(assets,startup,copy,budget,std::shared_ptr<HighscoreStore>{},CampaignExecution::playable);
    unsigned poll=0;
    for(unsigned call=0;!result->session()&&call<10000;++call) {
        NativeMenuInput input;
        if(result->startup().phase()==NativeStartupPhase::awaitingMenuDecision) {input.confirm=poll++==1;}
        const auto delivery=result->advanceStartup(input);pcm.insert(pcm.end(),delivery.samples.begin(),delivery.samples.end());
    }
    require(result->session()&&poll==3,"GL route did not execute real Begin");return result;
}
void gameplay(GlSceneRenderer& renderer,const std::filesystem::path& root,const std::filesystem::path& output,const std::string& label) {
    const auto assets=naturalRoute::assets(root);
    const auto startup=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto drivers=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    const auto copy=decodeFrameCopyProfile(drivers.at(0));
    test::CampaignBudgetComparison comparison;
    auto source=begin(assets,startup,copy,10000,comparison.leftPcm),observer=begin(assets,startup,copy,257,comparison.rightPcm);
    require(comparison.leftPcm==comparison.rightPcm,"GL startup PCM differs");
    const auto* owner=observer->session();const auto* synth=&owner->synth();
    Target left,right;unsigned snapshots=0,views=0,primarySnapshots=0;
#ifdef STELLAR7_OPENXR_TESTS
    namespace xrTest=stellar7::test::xr;
    Target xr0,xr1,xr2,xr3;xrTest::Runtime runtime;xrTest::rt=&runtime;
    const auto profile=label=="desktop-gl"?GlProfile::desktop33:GlProfile::es30;runtime.profile=profile;
    runtime.textures={{{{xr0.color,xr1.color}},{{xr2.color,xr3.color}}}};
    OpenXrSession xr(xrTest::getProc,xrTest::graphics(profile),xrWorldViewScale(xrDefaultUnitsPerMetre));
    XrPresentationMailbox mailbox;OpenXrPump xrPump;
    runtime.events={{XR_SESSION_STATE_READY},{XR_SESSION_STATE_FOCUSED}};unsigned xrViews=0;
#endif
    for(unsigned call=0;call<80;++call) {
        GameplayInput input;input.forward=call>0&&call<12;input.left=call>=12&&call<20;input.fire=call>=20;
        const auto a=source->advanceGameplay(input);NativeSessionDelivery b;
        {WorldPresentationCapture capture({},PresentationGeometry::spatial);b=observer->advanceGameplay(input);}
        comparison.compare(*source->session(),*owner,a,b,test::CampaignComparisonStage::boundary);
        const auto& frame=owner->frame();
        if(!frame.presentation||!frame.presentation->chase||!frame.presentation->chase->spatial) {continue;}
        ++snapshots;const auto scene=frame.presentation->chase->spatial;
        require(scene->world&&scene->world->tick==owner->model().ticks(),"cockpit animation lost the source simulation tick");
        // The frame snapshot precedes the native timer tail; post-step timers are a later boundary.
        require(std::all_of(scene->world->playerShotAges.begin(),scene->world->playerShotAges.end(),[](unsigned age) {return age<=32;}),
            "deck-cannon snapshot escaped the native weapon timer domain");
        if(std::any_of(scene->surfaces.begin(),scene->surfaces.end(),[](const auto& s) {return s.playerShotCenter.has_value();})) {++primarySnapshots;}
        const auto geometry=spatialGeometry(*scene,frame.palette);
        require(!geometry.triangles.empty(),"real gameplay has no unprojected mesh");
        const auto samples=observer->elapsedSamples(),ticks=owner->model().ticks();
        if(call>=77) {
            renderer.prepare(*scene,frame.palette);
            renderer.render(spatialProjection(scene->camera,4.0/3,{-32,0,0}),left.target());
            renderer.render(spatialProjection(scene->camera,4.0/3,{32,0,0}),right.target());views+=2;
            const auto first=left.read(),second=right.read();require(first!=second,"gameplay stereo collapsed to one image");
            if(call==79) {ppm(output/(label+"-left.ppm"),first);ppm(output/(label+"-right.ppm"),second);}
            const auto pilot=makeSpatialView(*scene,frame,EnhancedView::cockpit,{});
            renderer.prepare(pilot,frame.palette);
            renderer.render(spatialProjection(pilot.camera,4.0/3,{-32,0,0}),left.target());
            renderer.render(spatialProjection(pilot.camera,4.0/3,{32,0,0}),right.target());views+=2;
            const auto pilotLeft=left.read(),pilotRight=right.read();
            require(pilotLeft!=pilotRight&&pilotLeft!=first,"cockpit stereo remained a chase image or flat frame");
#ifdef STELLAR7_OPENXR_TESTS
            runtime.time=100+call;
            mailbox.publish(std::make_shared<const XrPresentation>(xrWorld(*scene,frame,EnhancedView::cockpit,{},xrDefaultUnitsPerMetre)));
            require(xrPump.step(xr,renderer,mailbox)==OpenXrFrame::submitted,"retained gameplay failed OpenXR dispatch submission");xrViews+=2;
            const auto xrLeft=xr1.read(),xrRight=xr2.read();require(xrLeft!=xrRight,"runtime stereo collapsed to one texture");
            if(call==78) {
                runtime.headPose.position.x+=0.15f;runtime.time+=1;
                require(xrPump.step(xr,renderer,mailbox)==OpenXrFrame::submitted,"held-scene head pose redraw failed");xrViews+=2;
                require(xr1.read()!=xrLeft,"held scene ignored a new runtime head pose");
            }
            if(call==79) {ppm(output/(label+"-xr-left.ppm"),xrLeft);ppm(output/(label+"-xr-right.ppm"),xrRight);}
            if(call==79) {cockpitSurvey(renderer,*mailbox.latest(),output,label);}
#endif
            if(call==79) {
                ppm(output/(label+"-cockpit-left.ppm"),pilotLeft);ppm(output/(label+"-cockpit-right.ppm"),pilotRight);
                SpatialOptions plain;plain.groundDetail=false;
                const auto flat=makeSpatialView(*scene,frame,EnhancedView::cockpit,plain);renderer.prepare(flat,frame.palette);
                renderer.render(spatialProjection(flat.camera,4.0/3,{-32,0,0}),left.target());
                const auto without=left.read();require(without!=pilotLeft,"ground detail toggle made no visible difference");
                ppm(output/(label+"-ground-off.ppm"),without);
                auto illuminated=pilot;illuminated.lights={{{-600,450,-50},{255,190,90},5000,0.8f}};
                renderer.prepare(illuminated,frame.palette);renderer.render(spatialProjection(pilot.camera,4.0/3,{-32,0,0}),left.target());
                const auto glow=left.read();require(glow!=pilotLeft,"cockpit local-light control made no visible difference");
                ppm(output/(label+"-cockpit-lit.ppm"),glow);
            }
        }
        require(observer->session()==owner&&&owner->synth()==synth&&observer->elapsedSamples()==samples&&owner->model().ticks()==ticks,
            "rendering two eyes advanced or replaced gameplay/device owners");
    }
    require(snapshots>=70&&views==12,"retained cockpit/chase stereo boundary was not reached");
    require(primarySnapshots>0,"retained firing never tagged an owned primary projectile");
    require(owner->model().presentationTranslation()[1]>0,"retained drive did not retain the world's translation around its recentered player");
#ifdef STELLAR7_OPENXR_TESTS
    require(xrViews==8&&runtime.layers==1&&runtime.frames==4,"retained OpenXR stereo submission boundary missing");
    std::cout<<"OpenXR deterministic SDK dispatch + actual software GL: eye_views="<<xrViews<<" held-scene late head motion changes pixels; source owners retained\n";
#endif
    std::cout<<"Actual Begin+80: snapshots="<<snapshots<<" stereo_views="<<views
        <<" source state/indexed/palette/events/PCM/device clocks match across retained budgets\n";
}
}
int main(int argc,char** argv) try {
    require(argc==3,"requires original data and private output directory");
    for(const auto profile:{GlProfile::desktop33,GlProfile::es30}) {
        Context context(profile);GlSceneRenderer renderer(load,profile);
        require(renderer.identity().find("llvmpipe")!=std::string::npos,"headless qualification requires llvmpipe software rendering");
        std::cout<<renderer.identity()<<'\n';primitives(renderer);cockpitVisibility(renderer);
        energyReadability(renderer,argv[1]);
#ifdef STELLAR7_OPENXR_TESTS
        xrGroundedMeshStability(argv[1]);
        xrGroundStability(renderer);
        xrSledVisibility(renderer,argv[1],argv[2]);
        xrSeatedVisibility(renderer,profile);xrPublication(renderer,profile);
#endif
        gameplay(renderer,argv[1],argv[2],profile==GlProfile::desktop33?"desktop-gl":"gles3");
    }
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
