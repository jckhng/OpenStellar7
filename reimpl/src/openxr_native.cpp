#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_egl.h>
#define XR_USE_GRAPHICS_API_OPENGL
#define XR_USE_PLATFORM_EGL
#ifdef _WIN32
#include <SDL_syswm.h>
#include <unknwn.h>
#define XR_USE_PLATFORM_WIN32
#endif
#include <openxr/openxr_platform.h>
#include "openxr_native.hpp"
#include <stdexcept>
#include <string>

namespace stellar7 {
namespace {
template<class T> T procedure(void* value,const char* name) {
    if(!value) {throw std::runtime_error(std::string("Missing native GL procedure ")+name);}
    return reinterpret_cast<T>(value);
}
struct Unload {void operator()(void* value) const {SDL_UnloadObject(value);}};
}
struct OpenXrNativeBinding::Impl {
    GlProfile profile;
    XrVersion version=0;
#ifdef _WIN32
    XrGraphicsBindingOpenGLWin32KHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
#elif defined(__linux__)
    std::unique_ptr<void,Unload> egl;
    XrGraphicsBindingEGLMNDX binding{XR_TYPE_GRAPHICS_BINDING_EGL_MNDX};
#endif
    explicit Impl(GlProfile selected):profile(selected) {}
    void initialize(SDL_Window* window) {
        if(!window||SDL_GL_GetCurrentWindow()!=window||!SDL_GL_GetCurrentContext()) {throw std::runtime_error("OpenXR requires the expected current SDL GL context");}
        using GetInteger=void(APIENTRY*)(GLenum,GLint*);
        const auto get=procedure<GetInteger>(SDL_GL_GetProcAddress("glGetIntegerv"),"glGetIntegerv");
        GLint major=0,minor=0;get(GL_MAJOR_VERSION,&major);get(GL_MINOR_VERSION,&minor);
        if(major<3||minor<0||major>99||minor>99) {throw std::runtime_error("Unsupported actual OpenXR GL context version");}
        version=XR_MAKE_VERSION(major,minor,0);
#ifdef _WIN32
        if(profile!=GlProfile::desktop33) {throw std::runtime_error("OpenXR WGL binding requires desktop OpenGL");}
        SDL_SysWMinfo info{};SDL_VERSION(&info.version);
        if(SDL_GetWindowWMInfo(window,&info)!=SDL_TRUE||info.subsystem!=SDL_SYSWM_WINDOWS) {throw std::runtime_error("Cannot query OpenXR SDL Win32 window");}
        using CurrentDC=HDC(APIENTRY*)();using CurrentContext=HGLRC(APIENTRY*)();
        const auto dc=procedure<CurrentDC>(SDL_GL_GetProcAddress("wglGetCurrentDC"),"wglGetCurrentDC")();
        const auto context=procedure<CurrentContext>(SDL_GL_GetProcAddress("wglGetCurrentContext"),"wglGetCurrentContext")();
        if(!dc||!context||dc!=info.info.win.hdc||reinterpret_cast<void*>(context)!=SDL_GL_GetCurrentContext()) {throw std::runtime_error("OpenXR WGL context does not own this SDL window");}
        binding.hDC=dc;binding.hGLRC=context;
#elif defined(__linux__)
        egl.reset(SDL_LoadObject("libEGL.so.1"));
        if(!egl) {throw std::runtime_error(std::string("Cannot query OpenXR EGL context: ")+SDL_GetError());}
        const auto symbol=[&](const char* name) {return SDL_LoadFunction(egl.get(),name);};
        const auto display=procedure<PFNEGLGETCURRENTDISPLAYPROC>(symbol("eglGetCurrentDisplay"),"eglGetCurrentDisplay")();
        const auto context=procedure<PFNEGLGETCURRENTCONTEXTPROC>(symbol("eglGetCurrentContext"),"eglGetCurrentContext")();
        if(display==EGL_NO_DISPLAY||context==EGL_NO_CONTEXT||context!=SDL_GL_GetCurrentContext()) {throw std::runtime_error("OpenXR requires an SDL EGL context on Linux (GLX is not yet supported)");}
        const auto query=procedure<PFNEGLQUERYCONTEXTPROC>(symbol("eglQueryContext"),"eglQueryContext");
        EGLint configId=0;if(query(display,context,EGL_CONFIG_ID,&configId)!=EGL_TRUE||configId==0) {throw std::runtime_error("Cannot query OpenXR EGL config identity");}
        const EGLint attributes[]={EGL_CONFIG_ID,configId,EGL_NONE};EGLint count=0;EGLConfig config=nullptr;
        const auto choose=procedure<PFNEGLCHOOSECONFIGPROC>(symbol("eglChooseConfig"),"eglChooseConfig");
        if(choose(display,attributes,&config,1,&count)!=EGL_TRUE||count!=1||!config) {throw std::runtime_error("Cannot recover the current OpenXR EGL config");}
        binding.getProcAddress=procedure<PFNEGLGETPROCADDRESSPROC>(symbol("eglGetProcAddress"),"eglGetProcAddress");
        binding.display=display;binding.context=context;binding.config=config;
#else
        throw std::runtime_error("No native OpenXR GL binding for this host");
#endif
    }
    OpenXrGraphics graphics() const {
#if defined(_WIN32)
        return {profile,&binding,version,OpenXrBinding::platform};
#elif defined(__linux__)
        return {profile,&binding,version,OpenXrBinding::egl};
#else
        throw std::runtime_error("No native OpenXR GL binding for this host");
#endif
    }
};
OpenXrNativeBinding::OpenXrNativeBinding(SDL_Window* window,GlProfile profile):impl_(std::make_unique<Impl>(profile)) {impl_->initialize(window);}
OpenXrNativeBinding::~OpenXrNativeBinding()=default;
OpenXrGraphics OpenXrNativeBinding::graphics() const {return impl_->graphics();}
}
