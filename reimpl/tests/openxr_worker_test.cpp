#define SDL_MAIN_HANDLED
#include "openxr_sdl.hpp"
#include "xr_presentation.hpp"
#include "sdl2_audio.hpp"
#include <SDL.h>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
using namespace stellar7;
void require(bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
template<class T> T function(void* library,const char* name) {
    auto value=reinterpret_cast<T>(SDL_LoadFunction(library,name));require(value!=nullptr,name);return value;
}
template<class Predicate> void until(OpenXrSdlHost& host,Predicate done) {
    const auto start=SDL_GetTicks64();
    while(!done()) {host.check();require(SDL_GetTicks64()-start<10000,"software XR worker did not reach its expected boundary");SDL_Delay(1);}
}
}
int main(int argc,char** argv) try {
    require(argc==2,"requires private test runtime path");
    SDL_SetMainReady();require(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)==0,SDL_GetError());
    struct Quit {~Quit() {SDL_Quit();}} quit;
    const std::unique_ptr<void,decltype(&SDL_UnloadObject)> library(SDL_LoadObject(argv[1]),SDL_UnloadObject);
    require(bool(library),"test runtime did not load");
    const auto reset=function<void(*)()>(library.get(),"stellar7TestReset");
    const auto hold=function<void(*)(int)>(library.get(),"stellar7TestHold");
    const auto command=function<void(*)(int)>(library.get(),"stellar7TestCommand");
    const auto frames=function<unsigned(*)()>(library.get(),"stellar7TestFrames");
    const auto projections=function<unsigned(*)()>(library.get(),"stellar7TestProjections");
    const auto empty=function<unsigned(*)()>(library.get(),"stellar7TestEmpty");
    const auto waiting=function<int(*)()>(library.get(),"stellar7TestWaiting");
    const auto owned=function<int(*)()>(library.get(),"stellar7TestOwned");
    const auto glFailure=function<int(*)()>(library.get(),"stellar7TestGlFailure");
    const auto exitRequests=function<unsigned(*)()>(library.get(),"stellar7TestExitRequests");
    const auto ends=function<unsigned(*)()>(library.get(),"stellar7TestEnds");
    const auto hash=function<std::uint64_t(*)(unsigned)>(library.get(),"stellar7TestEyeHash");
    const auto renderer=function<const char*(*)()>(library.get(),"stellar7TestRenderer");
    const auto latch=function<void(*)()>(library.get(),"stellar7TestLatch");
    const auto latchedCenter=function<std::uint32_t(*)()>(library.get(),"stellar7TestLatchedCenter");
    reset();
    {
        OpenXrSdlHost host(xrDefaultUnitsPerMetre);
        struct ReleaseHold {void(*release)(int);~ReleaseHold() {release(0);}} release{hold};
        until(host,[&] {return host.focused()&&empty()>0;});
        command(6);until(host,[&] {return host.controls().connected;});
        command(7);until(host,[&] {const auto input=host.controls();return input.down(ControllerButton::fire)&&input.down(ControllerButton::left);});
        command(8);until(host,[&] {const auto input=host.controls();return input.connected&&!input.down(ControllerButton::fire);});
        command(9);until(host,[&] {return !host.controls().connected;});
        command(6);until(host,[&] {return host.controls().connected;});
        require(std::string(renderer()).find("llvmpipe")!=std::string::npos,"worker qualification requires actual software llvmpipe");
        std::cout<<"Actual SDL offscreen worker: "<<renderer()<<'\n';
        IndexedScreen panel{std::vector<std::uint8_t>(64000,17),std::vector<Rgb>(256)};panel.palette[17]={180,70,30};
        host.panel(panel);until(host,[&] {return projections()>=2;});
        require(hash(0)!=0&&hash(0)!=hash(1),"seated panel lacked stereo disparity");
        const auto panelHash=hash(0);auto source=std::make_shared<SpatialScene>();
        source->camera=initialLevel1MainView();source->background.assign(64000,0);source->overlay.assign(64000,-1);
        SpatialWorld world;world.pilotCamera=source->camera;world.pilotCamera.origin.z=spatialPilotEyeHeight;world.skyColor=19;world.groundColor=20;source->world=world;
        source->surfaces.push_back({{{-300,2500,-300},{300,2500,-300},{300,2500,300},{-300,2500,300}},25});
        panel.palette[19]={25,60,110};panel.palette[20]={100,40,20};panel.palette[25]={20,180,90};
        host.world(source,panel,EnhancedView::chase,{});until(host,[&] {return hash(0)!=panelHash;});
        const auto chaseHash=hash(0);host.world(source,panel,EnhancedView::cockpit,{});until(host,[&] {return hash(0)!=chaseHash;});
        require(source->surfaces.size()==1&&source->billboards.empty()&&source->camera.origin.z==0,"XR publication changed its source camera/geometry");
        hold(1);until(host,[&] {return waiting()!=0;});
        const auto before=frames();const auto publicationStart=std::chrono::steady_clock::now();
        for(unsigned i=0;i<20;++i) {panel.palette[17].green=std::uint8_t(i);host.panel(panel);}
        host.optionsPanel(std::vector<std::uint32_t>(64000,0xff2080c0));
        const auto publicationMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-publicationStart).count();
        {
            sdlHost::Audio audio;unsigned services=0;audio.service([&] {++services;return true;});
            require(audio.submit(std::vector<std::int16_t>(4096,0))&&audio.drain(),"audio could not drain while XR wait was held");
            require(audio.submitted()==4096&&audio.consumed()==4096&&services>0,"XR worker changed/blocked audio source counters");
        }
        require(waiting()!=0&&frames()==before,"test runtime was not held during independent publication/audio");
        SDL_Delay(270);require(!host.controls().connected,"stalled XR frame retained stale controller input");
        latch();hold(0);until(host,[&] {return frames()>before+1;});
        const auto options=xrOptionsScreen(std::vector<std::uint32_t>(64000,0xff2080c0));
        const auto color=options.palette[options.pixels[0]];
        const auto expected=(std::uint32_t(color.red)<<16)|(std::uint32_t(color.green)<<8)|color.blue;
        require(latchedCenter()==expected,"XR selected stale source content before xrWaitFrame completed");
        std::cout<<"XR wait held: 20 latest publications ms="<<publicationMs<<"; dummy audio4096 samples drained independently\n";
        command(1);until(host,[&] {return !host.focused();});const auto visible=projections();until(host,[&] {return projections()>visible;});
        command(2);until(host,[&] {return host.focused();});host.recenter();
        command(4);const auto invalid=empty();until(host,[&] {return empty()>invalid+1;});
        command(5);const auto resumed=projections();until(host,[&] {return projections()>resumed+1;});
        command(3);until(host,[&] {return host.exiting();});host.check();
    }
    require(owned()==0&&glFailure()==0,"XR worker destroyed resources outside their context or leaked owners");
    for(unsigned launch=0;launch<2;++launch) {
        reset();
        {OpenXrSdlHost host(xrDefaultUnitsPerMetre);until(host,[&] {return host.focused()&&empty()>0;});}
        require(exitRequests()==1&&ends()==1&&owned()==0&&glFailure()==0,"desktop close/relaunch skipped XR exit/STOPPING cleanup");
    }
    std::cout<<"Actual SDL context transfer/loader/panel stereo/late publication/focus/tracking/shutdown pass; deterministic XR compositor substitute\n";
    return 0;
} catch(const std::exception& failure) {std::cerr<<failure.what()<<'\n';return 1;}
