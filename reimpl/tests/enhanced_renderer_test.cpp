#define SDL_MAIN_HANDLED
#include "sdl_host.hpp"
#include <cstdlib>
#include <iostream>

namespace {
int created=0,destroyed=0,selected=-1;
Uint32 requestedFlags=0;
bool fail=false,wrongRenderer=false;
const char* names[]={"software","direct3d","direct3d11"};
void require(bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
int fakeCount() {return 3;}
int fakeInfo(int index,SDL_RendererInfo* info) {
    *info={};info->name=names[index];info->flags=index==0?SDL_RENDERER_SOFTWARE:SDL_RENDERER_ACCELERATED;return 0;
}
SDL_Renderer* fakeCreate(SDL_Window*,int index,Uint32 flags) {
    ++created;selected=index;requestedFlags=flags;
    return fail?nullptr:reinterpret_cast<SDL_Renderer*>(1);
}
int fakeRendererInfo(SDL_Renderer*,SDL_RendererInfo* info) {return fakeInfo(wrongRenderer?1:selected,info);}
void fakeDestroy(SDL_Renderer*) {++destroyed;}
void environment(const char* name,const char* value) {
#ifdef _WIN32
    require(_putenv_s(name,value)==0,"cannot set test environment");
#else
    require(setenv(name,value,1)==0,"cannot set test environment");
#endif
}
template<class Action> void rejects(Action action,const char* message) {
    bool rejected=false;try {action();} catch(const std::runtime_error&) {rejected=true;}
    require(rejected,message);
}
}
#define SDL_GetNumRenderDrivers fakeCount
#define SDL_GetRenderDriverInfo fakeInfo
#define SDL_CreateRenderer fakeCreate
#define SDL_GetRendererInfo fakeRendererInfo
#define SDL_DestroyRenderer fakeDestroy
#include "enhanced_host.hpp"

int main() try {
    using namespace stellar7;
    environment("STELLAR7_SAFE_MODE","0");environment("STELLAR7_HOST_CONTROLLERS","0");
    environment("STELLAR7_HOST_RENDERER","direct3d11");
    require(enhancedHostInitFlags()==(SDL_INIT_VIDEO|SDL_INIT_AUDIO),"input disable initialized device discovery");
    createEnhancedRenderer(nullptr);
    require(selected==2&&requestedFlags==SDL_RENDERER_ACCELERATED,"exact backend was not selected");
    fail=true;
    rejects([] {createEnhancedRenderer(nullptr);},"failed backend silently accepted");
    require(created==2,"failed backend attempted another renderer");
    environment("STELLAR7_HOST_RENDERER","missing");
    rejects([] {createEnhancedRenderer(nullptr);},"unavailable backend silently accepted");
    require(created==2,"unavailable backend reached renderer initialization");
    fail=false;wrongRenderer=true;
    environment("STELLAR7_SAFE_MODE","1");
    rejects([] {createEnhancedRenderer(nullptr);},"safe policy accepted an accelerated renderer");
    require(destroyed==1&&selected==0&&requestedFlags==SDL_RENDERER_SOFTWARE,
        "safe policy did not destroy the mismatched renderer");
} catch(const std::exception& error) {
    std::cerr<<error.what()<<'\n';return 1;
}
