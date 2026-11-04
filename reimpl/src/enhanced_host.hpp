#pragma once
#include "sdl_host.hpp"
#include "enhanced_runtime.hpp"
#include "enhanced_diagnostics.hpp"

namespace stellar7 {
inline Uint32 enhancedHostInitFlags() {
    (void)enhancedHostRenderer();
    (void)enhancedGraphics();
    if(enhancedXrScale()&&enhancedGraphics()!=EnhancedGraphics::cpu) {throw std::invalid_argument("OpenXR currently requires STELLAR7_GRAPHICS=cpu for its independent desktop mirror");}
    return SDL_INIT_VIDEO|SDL_INIT_AUDIO
        |(enhancedControllersEnabled()?SDL_INIT_JOYSTICK|SDL_INIT_GAMECONTROLLER:0);
}
inline void configureEnhancedHost() {
    // A software screen surface must not silently create a GPU framebuffer.
    if(SDL_SetHintWithPriority(SDL_HINT_FRAMEBUFFER_ACCELERATION,"0",SDL_HINT_OVERRIDE)!=SDL_TRUE) {
        throw std::runtime_error("Cannot disable SDL framebuffer acceleration");
    }
    const auto* name=enhancedHostRenderer();
    if(name&&std::strcmp(name,"software")==0
        &&SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,"software",SDL_HINT_OVERRIDE)!=SDL_TRUE) {
        throw std::runtime_error("Cannot enforce software-only SDL startup");
    }
}
inline SDL_Renderer* createEnhancedRenderer(SDL_Window* window) {
    const auto* name=enhancedHostRenderer();
    int selected=-1;
    if(name) {
        const auto count=SDL_GetNumRenderDrivers();
        for(int index=0;index<count;++index) {
            SDL_RendererInfo info{};
            sdlHost::check(SDL_GetRenderDriverInfo(index,&info),"SDL_GetRenderDriverInfo");
            if(std::strcmp(name,info.name)==0) {selected=index;break;}
        }
        if(selected<0) {throw std::runtime_error(std::string("Unavailable SDL renderer: ")+name);}
    }
    const bool software=name&&std::strcmp(name,"software")==0;
    const Uint32 flags=name?(software?SDL_RENDERER_SOFTWARE:SDL_RENDERER_ACCELERATED):0;
    enhancedHostTrace((std::string("creating renderer=")+(name?name:"automatic")+" index="+std::to_string(selected)).c_str());
    // A driver hint with index=-1 can fall back to another backend after failure.
    auto* renderer=enhancedHostCall("SDL_CreateRenderer",[&] {return SDL_CreateRenderer(window,selected,flags);});
    if(!renderer) {throw std::runtime_error(std::string("SDL_CreateRenderer: ")+SDL_GetError());}
    SDL_RendererInfo info{};
    if(SDL_GetRendererInfo(renderer,&info)!=0||(name&&std::strcmp(name,info.name)!=0)
        ||(name&&!software&&(info.flags&SDL_RENDERER_ACCELERATED)==0)
        ||(software&&((info.flags&SDL_RENDERER_SOFTWARE)==0||(info.flags&SDL_RENDERER_ACCELERATED)!=0))) {
        SDL_DestroyRenderer(renderer);
        throw std::runtime_error("SDL renderer does not match the requested host policy");
    }
    enhancedHostTrace((std::string("renderer=")+info.name+" flags="+std::to_string(info.flags)).c_str());
    return renderer;
}
}
