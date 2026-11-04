#define SDL_MAIN_HANDLED
#define XR_USE_PLATFORM_EGL
#include <SDL.h>
#include "openxr_test_runtime.hpp"
#include <atomic>
#include <thread>
#include <chrono>

namespace {
namespace xr=stellar7::test::xr;
xr::Runtime runtime;
std::atomic<unsigned> frames{0},projections{0},emptyFrames{0};
std::atomic<bool> hold{false},waiting{false},glFailure{false};
std::atomic<int> command{0};
std::atomic<bool> latch{false};
std::atomic<std::uint32_t> latchedCenter{0};
std::array<std::atomic<std::uint64_t>,2> eyeHashes{};
std::array<bool,2> allocated{};
GLuint readFramebuffer=0;
std::string rendererName;
template<class T> T gl(const char* name) {
    auto value=reinterpret_cast<T>(SDL_GL_GetProcAddress(name));xr::require(value!=nullptr,name);return value;
}
XrResult XRAPI_CALL requirements(XrInstance instance,XrSystemId system,XrGraphicsRequirementsOpenGLKHR* out) {
    xr::require(!SDL_GL_GetCurrentContext(),"XR context was created before querying runtime requirements");
    return xr::requirementsGl(instance,system,out);
}
XrResult XRAPI_CALL sessionCreate(XrInstance instance,const XrSessionCreateInfo* info,XrSession* out) {
    const auto& binding=*static_cast<const XrGraphicsBindingEGLMNDX*>(info->next);
    xr::require(binding.type==XR_TYPE_GRAPHICS_BINDING_EGL_MNDX&&binding.context==SDL_GL_GetCurrentContext()
        &&binding.display!=EGL_NO_DISPLAY&&binding.config&&binding.getProcAddress,"actual SDL EGL binding did not reach OpenXR session");
    rendererName=reinterpret_cast<const char*>(gl<decltype(&glGetString)>("glGetString")(GL_RENDERER));
    return xr::createSession(instance,info,out);
}
XrResult XRAPI_CALL enumerateImages(XrSwapchain chain,std::uint32_t cap,std::uint32_t* count,XrSwapchainImageBaseHeader* out) {
    const auto eye=runtime.eye(chain);
    if(!allocated[eye]) {
        gl<decltype(&glGenTextures)>("glGenTextures")(2,runtime.textures[eye].data());
        for(const auto texture:runtime.textures[eye]) {
            gl<decltype(&glBindTexture)>("glBindTexture")(GL_TEXTURE_2D,texture);
            gl<decltype(&glTexImage2D)>("glTexImage2D")(GL_TEXTURE_2D,0,GLint(runtime.chainFormat),320,200,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        }
        allocated[eye]=true;
        gl<decltype(&glEnable)>("glEnable")(GL_FRAMEBUFFER_SRGB);
    }
    return xr::images(chain,cap,count,out);
}
XrResult XRAPI_CALL waitFrame(XrSession session,const XrFrameWaitInfo* info,XrFrameState* out) {
    waiting=true;
    while(hold.load()) {std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    waiting=false;std::this_thread::sleep_for(std::chrono::milliseconds(2));
    runtime.time+=11111111;return xr::waitFrame(session,info,out);
}
XrResult XRAPI_CALL poll(XrInstance instance,XrEventDataBuffer* event) {
    switch(command.exchange(0)) {
    case 1:runtime.events.push_back({XR_SESSION_STATE_VISIBLE});break;
    case 2:runtime.events.push_back({XR_SESSION_STATE_FOCUSED});break;
    case 3:runtime.events.push_back({XR_SESSION_STATE_EXITING});break;
    case 4:runtime.valid=false;break;
    case 5:runtime.valid=true;break;
    case 6:runtime.controllersConnected=true;runtime.fireValue=0;runtime.driveX=0;break;
    case 7:runtime.fireValue=0.8f;runtime.driveX=-0.8f;break;
    case 8:runtime.fireValue=0;runtime.driveX=0;break;
    case 9:runtime.controllersConnected=false;break;
    default:break;
    }
    return xr::poll(instance,event);
}
XrResult XRAPI_CALL endFrame(XrSession session,const XrFrameEndInfo* info) {
    std::uint32_t center=0;
    if(info->layerCount) {
        if(!readFramebuffer) {gl<PFNGLGENFRAMEBUFFERSPROC>("glGenFramebuffers")(1,&readFramebuffer);}
        gl<PFNGLBINDFRAMEBUFFERPROC>("glBindFramebuffer")(GL_FRAMEBUFFER,readFramebuffer);
        for(unsigned eye=0;eye<2;++eye) {
            gl<PFNGLFRAMEBUFFERTEXTURE2DPROC>("glFramebufferTexture2D")(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,runtime.textures[eye][1-eye],0);
            if(gl<PFNGLCHECKFRAMEBUFFERSTATUSPROC>("glCheckFramebufferStatus")(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) {glFailure=true;return XR_ERROR_RUNTIME_FAILURE;}
            std::vector<std::uint8_t> pixels(320*200*4);
            gl<decltype(&glReadPixels)>("glReadPixels")(0,0,320,200,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            std::uint64_t hash=14695981039346656037ULL;
            for(const auto value:pixels) {hash^=value;hash*=1099511628211ULL;}
            eyeHashes[eye]=hash;
            if(eye==0) {constexpr auto middle=(100*320+160)*4;center=(std::uint32_t(pixels[middle])<<16)|(std::uint32_t(pixels[middle+1])<<8)|pixels[middle+2];}
        }
        gl<PFNGLFRAMEBUFFERTEXTURE2DPROC>("glFramebufferTexture2D")(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,0,0);
        gl<PFNGLBINDFRAMEBUFFERPROC>("glBindFramebuffer")(GL_FRAMEBUFFER,0);++projections;
    } else {++emptyFrames;}
    if(latch.exchange(false)) {latchedCenter=center;}
    ++frames;return xr::endFrame(session,info);
}
XrResult XRAPI_CALL destroyChain(XrSwapchain chain) {
    if(!SDL_GL_GetCurrentContext()) {glFailure=true;return XR_ERROR_RUNTIME_FAILURE;}
    const auto eye=runtime.eye(chain);
    if(allocated[eye]) {gl<decltype(&glDeleteTextures)>("glDeleteTextures")(2,runtime.textures[eye].data());allocated[eye]=false;}
    return xr::destroyChain(chain);
}
XrResult XRAPI_CALL destroySession(XrSession session) {
    if(readFramebuffer) {gl<PFNGLDELETEFRAMEBUFFERSPROC>("glDeleteFramebuffers")(1,&readFramebuffer);readFramebuffer=0;}
    return xr::destroySession(session);
}
}
extern "C" {
XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance instance,const char* name,PFN_xrVoidFunction* out) {
    xr::rt=&runtime;
#define MAP(api,function) if(std::strcmp(name,"xr" #api)==0) {*out=reinterpret_cast<PFN_xrVoidFunction>(function);return XR_SUCCESS;}
    MAP(CreateSession,sessionCreate) MAP(EnumerateSwapchainImages,enumerateImages) MAP(WaitFrame,waitFrame)
    MAP(PollEvent,poll) MAP(EndFrame,endFrame) MAP(DestroySwapchain,destroyChain) MAP(DestroySession,destroySession)
    MAP(GetOpenGLGraphicsRequirementsKHR,requirements)
#undef MAP
    return xr::getProc(instance,name,out);
}
void stellar7TestReset() {
    runtime=xr::Runtime{};runtime.egl=true;runtime.minimumGl=XR_MAKE_VERSION(4,3,0);runtime.rgba=false;runtime.srgb=true;
    runtime.time=1000000000;runtime.events={{XR_SESSION_STATE_READY},{XR_SESSION_STATE_FOCUSED}};
    frames=0;projections=0;emptyFrames=0;hold=false;waiting=false;glFailure=false;command=0;allocated={};latch=false;latchedCenter=0;
    eyeHashes[0]=eyeHashes[1]=0;rendererName.clear();xr::rt=&runtime;
}
void stellar7TestHold(int value) {hold=value!=0;}
void stellar7TestCommand(int value) {command=value;}
unsigned stellar7TestFrames() {return frames.load();}
unsigned stellar7TestProjections() {return projections.load();}
unsigned stellar7TestEmpty() {return emptyFrames.load();}
int stellar7TestWaiting() {return waiting.load()?1:0;}
void stellar7TestLatch() {latchedCenter=0;latch=true;}
std::uint32_t stellar7TestLatchedCenter() {return latchedCenter.load();}
int stellar7TestGlFailure() {return glFailure.load()?1:0;}
unsigned stellar7TestExitRequests() {return runtime.exitRequests;}
unsigned stellar7TestEnds() {return runtime.ends;}
std::uint64_t stellar7TestEyeHash(unsigned eye) {return eyeHashes.at(eye).load();}
const char* stellar7TestRenderer() {return rendererName.c_str();}
int stellar7TestOwned() {
    return int(runtime.createdChains-runtime.destroyedChains+runtime.createdSpaces-runtime.destroyedSpaces)
        +1-int(runtime.destroyedSessions)+1-int(runtime.destroyedInstances);
}
}
