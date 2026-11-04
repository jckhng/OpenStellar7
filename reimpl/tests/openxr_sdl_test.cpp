#define SDL_MAIN_HANDLED
#define XR_USE_PLATFORM_EGL
#ifdef _WIN32
#include <SDL_syswm.h>
#include <unknwn.h>
#define XR_USE_PLATFORM_WIN32
#endif
#include "openxr_test_runtime.hpp"
#include "openxr_loader.hpp"
#include "openxr_native.hpp"

namespace {
using namespace stellar7;
using stellar7::test::xr::require;
using stellar7::test::xr::rejects;
using stellar7::test::xr::handle;
SDL_Window* const window=handle<SDL_Window*>(50);
void* const context=handle<void*>(60);
bool loadFailure=false,symbolFailure=false,currentMismatch=false,windowMismatch=false,configFailure=false;
int majorVersion=4,minorVersion=5;
unsigned loads=0,unloads=0;
std::vector<std::string> libraries;
void APIENTRY getInteger(GLenum key,GLint* out) {*out=key==GL_MAJOR_VERSION?majorVersion:minorVersion;}
#ifdef _WIN32
HDC APIENTRY currentDC() {return handle<HDC>(70);}
HGLRC APIENTRY currentContext() {return handle<HGLRC>(currentMismatch?61:60);}
#endif
EGLDisplay EGLAPIENTRY currentDisplay() {return handle<EGLDisplay>(80);}
EGLContext EGLAPIENTRY currentEglContext() {return handle<EGLContext>(currentMismatch?61:60);}
EGLBoolean EGLAPIENTRY queryContext(EGLDisplay display,EGLContext value,EGLint key,EGLint* out) {
    require(display==handle<EGLDisplay>(80)&&value==context&&key==EGL_CONFIG_ID,"EGL context/config identity");*out=23;return configFailure?EGL_FALSE:EGL_TRUE;
}
EGLBoolean EGLAPIENTRY chooseConfig(EGLDisplay display,const EGLint* attributes,EGLConfig* out,EGLint cap,EGLint* count) {
    require(display==handle<EGLDisplay>(80)&&attributes[0]==EGL_CONFIG_ID&&attributes[1]==23&&cap==1,"EGL exact-config admission");
    *out=handle<EGLConfig>(90);*count=1;return EGL_TRUE;
}
__eglMustCastToProperFunctionPointerType EGLAPIENTRY eglAddress(const char*) {return nullptr;}
}
extern "C" {
void* SDLCALL SDL_LoadObject(const char* name) {++loads;libraries.push_back(name);return loadFailure?nullptr:handle<void*>(10);}
void SDLCALL SDL_UnloadObject(void*) {++unloads;}
const char* SDLCALL SDL_GetError() {return "injected SDL driver failure";}
void* SDLCALL SDL_LoadFunction(void*,const char* name) {
    if(symbolFailure) {return nullptr;}
    if(std::strcmp(name,"xrGetInstanceProcAddr")==0) {return reinterpret_cast<void*>(stellar7::test::xr::getProc);}
    if(std::strcmp(name,"eglGetCurrentDisplay")==0) {return reinterpret_cast<void*>(currentDisplay);}
    if(std::strcmp(name,"eglGetCurrentContext")==0) {return reinterpret_cast<void*>(currentEglContext);}
    if(std::strcmp(name,"eglQueryContext")==0) {return reinterpret_cast<void*>(queryContext);}
    if(std::strcmp(name,"eglChooseConfig")==0) {return reinterpret_cast<void*>(chooseConfig);}
    if(std::strcmp(name,"eglGetProcAddress")==0) {return reinterpret_cast<void*>(eglAddress);}
    return nullptr;
}
SDL_Window* SDLCALL SDL_GL_GetCurrentWindow() {return windowMismatch?nullptr:window;}
SDL_GLContext SDLCALL SDL_GL_GetCurrentContext() {return context;}
void* SDLCALL SDL_GL_GetProcAddress(const char* name) {
    if(symbolFailure) {return nullptr;}
    if(std::strcmp(name,"glGetIntegerv")==0) {return reinterpret_cast<void*>(getInteger);}
#ifdef _WIN32
    if(std::strcmp(name,"wglGetCurrentDC")==0) {return reinterpret_cast<void*>(currentDC);}
    if(std::strcmp(name,"wglGetCurrentContext")==0) {return reinterpret_cast<void*>(currentContext);}
#endif
    return nullptr;
}
#ifdef _WIN32
SDL_bool SDLCALL SDL_GetWindowWMInfo(SDL_Window* value,SDL_SysWMinfo* info) {
    require(value==window&&info->version.major==SDL_MAJOR_VERSION,"SDL native-window version query");
    info->subsystem=SDL_SYSWM_WINDOWS;info->info.win.hdc=handle<HDC>(70);return SDL_TRUE;
}
#endif
}
namespace {
void checkDriver() {
    stellar7::test::xr::Runtime runtime;stellar7::test::xr::rt=&runtime;
    require(loads==0,"OpenXR loader was opened before explicit construction");
    {
        OpenXrLoader loader;require(loader.entry()==stellar7::test::xr::getProc,"SDL OpenXR entry dispatch");
        require(libraries.back()==openXrLoaderName(),"platform runtime-loader name");
    }
    require(unloads==1,"runtime-loader ownership leaked");
    loadFailure=true;rejects([] {OpenXrLoader loader;});loadFailure=false;
    require(unloads==1,"failed library acquisition destroyed an unowned object");
    symbolFailure=true;rejects([] {OpenXrLoader loader;});symbolFailure=false;
    require(unloads==2,"missing entry point leaked its library");
    {
        OpenXrNativeBinding native(window,GlProfile::desktop33);const auto graphics=native.graphics();
        require(graphics.contextVersion==XR_MAKE_VERSION(4,5,0),"native binding reused requested version instead of querying actual GL");
#ifdef _WIN32
        const auto& binding=*static_cast<const XrGraphicsBindingOpenGLWin32KHR*>(graphics.binding);
        require(graphics.api==OpenXrBinding::platform&&binding.type==XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR
            &&binding.hDC==handle<HDC>(70)&&binding.hGLRC==handle<HGLRC>(60),"WGL current device/context binding");
#else
        const auto& binding=*static_cast<const XrGraphicsBindingEGLMNDX*>(graphics.binding);
        require(graphics.api==OpenXrBinding::egl&&binding.type==XR_TYPE_GRAPHICS_BINDING_EGL_MNDX
            &&binding.display==handle<EGLDisplay>(80)&&binding.context==context&&binding.config==handle<EGLConfig>(90)
            &&binding.getProcAddress==eglAddress,"EGL current display/context/config binding");
        rejects([&] {OpenXrSession missing(stellar7::test::xr::getProc,graphics,{1000});});
        runtime.egl=true;
#endif
        OpenXrLoader loader;OpenXrSession xr(loader.entry(),graphics,{1000});
        runtime.events={{XR_SESSION_STATE_READY},{XR_SESSION_STATE_FOCUSED}};
        require(xr.frame([](const auto&) {},OpenXrLayer::none)==OpenXrFrame::empty&&runtime.layers==0,
            "startup without source content acquired/submitted undefined images");
        require(std::none_of(runtime.calls.begin(),runtime.calls.end(),[](const auto& name) {return name=="acquire0"||name=="acquire1";}),"empty frame acquired images");
    }
    currentMismatch=true;rejects([] {OpenXrNativeBinding native(window,GlProfile::desktop33);});currentMismatch=false;
    windowMismatch=true;rejects([] {OpenXrNativeBinding native(window,GlProfile::desktop33);});windowMismatch=false;
    symbolFailure=true;rejects([] {OpenXrNativeBinding native(window,GlProfile::desktop33);});symbolFailure=false;
    majorVersion=2;rejects([] {OpenXrNativeBinding native(window,GlProfile::desktop33);});majorVersion=4;
#ifdef _WIN32
    rejects([] {OpenXrNativeBinding native(window,GlProfile::es30);});
#else
    configFailure=true;rejects([] {OpenXrNativeBinding native(window,GlProfile::desktop33);});configFailure=false;
#endif
    require(loads-1==unloads,"native/loader partial failure ownership leaked");
    std::cout<<"SDL loader/native current-context/version/requirements/empty-frame/failure controls pass; no physical driver called\n";
}
}
int main() {try {checkDriver();return 0;} catch(const std::exception& failure) {std::cerr<<failure.what()<<'\n';return 1;}}
