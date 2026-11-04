#include "openxr_sdl.hpp"
#include "openxr_loader.hpp"
#include "openxr_native.hpp"
#include "openxr_pump.hpp"
#include "enhanced_runtime.hpp"
#include <SDL.h>
#include <algorithm>
#include <atomic>
#include <thread>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr XrVersion rendererMinimumGl=XR_MAKE_VERSION(3,3,0);
}
struct OpenXrSdlHost::Impl {
    float scale;
    std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window{nullptr,SDL_DestroyWindow};
    std::unique_ptr<void,decltype(&SDL_GL_DeleteContext)> context{nullptr,SDL_GL_DeleteContext};
    XrPresentationMailbox mailbox;
    ControllerMailbox controls;
    std::atomic<bool> stop{false},inputFocus{false},exitRequested{false},recenterRequested{false};
    mutable std::mutex errorMutex;
    std::string error;
    std::thread worker;
    std::shared_ptr<const SpatialScene> previousSource;
    std::vector<Rgb> previousPalette;
    std::vector<std::uint8_t> previousPixels;
    EnhancedView previousView=EnhancedView::cockpit;
    SpatialOptions previousOptions;
    enum class Content {none,world,panel,options};
    Content previousContent=Content::none;
    explicit Impl(float value):scale(value) {}
    ~Impl() {
        stop=true;
        if(worker.joinable()) {worker.join();}
    }
    void initialize() {
        // Keep the normal desktop renderer independent. Native XR creation is
        // reached only after explicit opt-in and never from safe startup.
#if defined(__linux__)
        if(SDL_SetHintWithPriority(SDL_HINT_VIDEO_X11_FORCE_EGL,"1",SDL_HINT_OVERRIDE)!=SDL_TRUE) {throw std::runtime_error("Cannot select SDL EGL for the XR device");}
#endif
        // SDL_GL_CONTEXT_EGL is a legacy profile selector, not a backend switch;
        // it overwrites CORE with ES (and makes a 3.3 request invalid on EGL).
        const std::array<std::pair<SDL_GLattr,int>,6> attributes{{
            {SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE},{SDL_GL_CONTEXT_MAJOR_VERSION,int(XR_VERSION_MAJOR(rendererMinimumGl))},
            {SDL_GL_CONTEXT_MINOR_VERSION,int(XR_VERSION_MINOR(rendererMinimumGl))},{SDL_GL_DOUBLEBUFFER,0},{SDL_GL_DEPTH_SIZE,24},{SDL_GL_STENCIL_SIZE,0}}};
        for(const auto& attribute:attributes) {
            if(SDL_GL_SetAttribute(attribute.first,attribute.second)!=0) {throw std::runtime_error(std::string("Cannot select XR GL context attributes: ")+SDL_GetError());}
        }
        window.reset(SDL_CreateWindow("Stellar 7 XR device",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN));
        if(!window) {throw std::runtime_error(std::string("Cannot create XR device window: ")+SDL_GetError());}
        worker=std::thread([this] {run();});
    }
    OpenXrGraphics createGraphics(XrVersion minimum,XrVersion maximum,std::unique_ptr<OpenXrNativeBinding>& native) {
        const auto requested=std::max(minimum,rendererMinimumGl);
        const auto version=[](XrVersion value) {return std::to_string(XR_VERSION_MAJOR(value))+"."+std::to_string(XR_VERSION_MINOR(value));};
        enhancedHostTrace(("OpenXR GL runtime="+version(minimum)+".."+version(maximum)+" requested="+version(requested)).c_str());
        if(requested>maximum) {throw std::runtime_error("OpenXR runtime GL maximum is below renderer minimum 3.3");}
        if(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,int(XR_VERSION_MAJOR(requested)))!=0
            ||SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,int(XR_VERSION_MINOR(requested)))!=0) {
            throw std::runtime_error(std::string("Cannot select runtime-required XR GL version: ")+SDL_GetError());
        }
        context.reset(SDL_GL_CreateContext(window.get()));
        if(!context) {throw std::runtime_error("Cannot create XR OpenGL "+version(requested)+" context: "+SDL_GetError());}
        if(SDL_GL_MakeCurrent(window.get(),context.get())!=0) {throw std::runtime_error(std::string("Cannot make XR worker context current: ")+SDL_GetError());}
        native=std::make_unique<OpenXrNativeBinding>(window.get(),GlProfile::desktop33);
        const auto graphics=native->graphics();
        enhancedHostTrace(("OpenXR GL actual="+version(graphics.contextVersion)).c_str());
        return graphics;
    }
    void run() noexcept {
        try {
            {
                OpenXrLoader loader;
                std::unique_ptr<OpenXrNativeBinding> native;
#if defined(__linux__)
                constexpr auto api=OpenXrBinding::egl;
#else
                constexpr auto api=OpenXrBinding::platform;
#endif
                OpenXrSession session(loader.entry(),GlProfile::desktop33,api,xrWorldViewScale(scale),
                    [&](XrVersion minimum,XrVersion maximum) {return createGraphics(minimum,maximum,native);},
                    enhancedXrControllersEnabled()?OpenXrInput::controllers:OpenXrInput::none);
                GlSceneRenderer renderer(SDL_GL_GetProcAddress,GlProfile::desktop33);
                OpenXrPump pump;
                enhancedHostTrace("OpenXR render worker ready");
                while(!stop.load()) {
                    if(recenterRequested.exchange(false)) {session.recenter();}
                    const auto result=pump.step(session,renderer,mailbox);inputFocus=session.focused();
                    controls.publish(session.controls(),SDL_GetTicks64());
                    if(result==OpenXrFrame::exiting) {exitRequested=true;break;}
                    if(result==OpenXrFrame::idle) {SDL_Delay(5);}
                }
                constexpr Uint64 shutdownTimeoutMs=2000;
                const auto shutdownStart=SDL_GetTicks64();
                enhancedHostTrace("OpenXR requesting session shutdown");
                while(!session.pollShutdown()) {
                    if(SDL_GetTicks64()-shutdownStart>=shutdownTimeoutMs) {throw std::runtime_error("OpenXR runtime did not stop its session within the shutdown deadline");}
                    SDL_Delay(1);
                }
                enhancedHostTrace("OpenXR session stopped");
            }
        } catch(const std::exception& failure) {
            enhancedHostTrace((std::string("OpenXR worker failed: ")+failure.what()).c_str());
            const std::lock_guard<std::mutex> lock(errorMutex);error=failure.what();
        } catch(...) {
            const std::lock_guard<std::mutex> lock(errorMutex);error="Unknown XR render worker failure";
        }
        inputFocus=false;
        controls.publish({},SDL_GetTicks64());
        if(context&&SDL_GL_GetCurrentContext()==context.get()) {SDL_GL_MakeCurrent(window.get(),nullptr);}
    }
    void publishPanel(const IndexedScreen& indexed,Content content) {
        if(previousContent==content&&previousPalette==indexed.palette&&previousPixels==indexed.pixels) {return;}
        mailbox.publish(std::make_shared<const XrPresentation>(xrPanel(indexed,scale)));
        previousContent=content;previousPalette=indexed.palette;previousPixels=indexed.pixels;previousSource.reset();
    }
};
OpenXrSdlHost::OpenXrSdlHost(float scale):impl_(std::make_unique<Impl>(scale)) {
    if(!std::isfinite(scale)||scale<=0) {throw std::invalid_argument("OpenXR host requires a positive finite physical scale");}
    enhancedHostTrace(("OpenXR native units per metre="+std::to_string(scale)).c_str());
    impl_->initialize();
}
OpenXrSdlHost::~OpenXrSdlHost()=default;
void OpenXrSdlHost::world(std::shared_ptr<const SpatialScene> source,const IndexedScreen& indexed,EnhancedView view,const SpatialOptions& options) {
    if(!source) {throw std::invalid_argument("XR world publication requires source geometry");}
    auto& host=*impl_;auto spatial=options;spatial.cockpit=CockpitStyle::spatial;
    if(host.previousContent==Impl::Content::world&&host.previousSource==source&&host.previousPalette==indexed.palette
        &&host.previousPixels==indexed.pixels&&host.previousView==view&&host.previousOptions==spatial) {return;}
    auto presentation=std::make_shared<XrPresentation>(xrWorld(*source,indexed,view,spatial,host.scale));
    host.mailbox.publish(presentation);host.previousContent=Impl::Content::world;host.previousSource=std::move(source);
    host.previousPalette=indexed.palette;host.previousPixels=indexed.pixels;host.previousView=view;host.previousOptions=spatial;
}
void OpenXrSdlHost::panel(const IndexedScreen& indexed) {impl_->publishPanel(indexed,Impl::Content::panel);}
void OpenXrSdlHost::optionsPanel(const std::vector<std::uint32_t>& argb) {impl_->publishPanel(xrOptionsScreen(argb),Impl::Content::options);}
void OpenXrSdlHost::recenter() {impl_->recenterRequested=true;}
bool OpenXrSdlHost::focused() const {return impl_->inputFocus.load();}
bool OpenXrSdlHost::exiting() const {return impl_->exitRequested.load();}
ControllerInput OpenXrSdlHost::controls() {return impl_->controls.take(SDL_GetTicks64());}
void OpenXrSdlHost::check() const {
    const std::lock_guard<std::mutex> lock(impl_->errorMutex);
    if(!impl_->error.empty()) {throw std::runtime_error("OpenXR: "+impl_->error);}
}
}
