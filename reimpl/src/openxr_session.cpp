#define XR_USE_GRAPHICS_API_OPENGL
#define XR_USE_GRAPHICS_API_OPENGL_ES
#define XR_USE_PLATFORM_EGL
#include <SDL_opengl.h>
#include <SDL_egl.h>
#include <openxr/openxr_platform.h>
#include "openxr_session.hpp"
#include "openxr_actions.hpp"
#include <algorithm>
#include <cstring>
#include <deque>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>

namespace stellar7 {
namespace {
constexpr unsigned eyeCount=2,maximumEventsPerFrame=64;
constexpr std::int64_t rgba8Format=GL_RGBA8,srgb8Format=GL_SRGB8_ALPHA8;
constexpr unsigned maximumImageDimension=16384,maximumSwapchainImages=64;
XrVersion apiVersion(XrVersion version) {return XR_MAKE_VERSION(XR_VERSION_MAJOR(version),XR_VERSION_MINOR(version),0);}
std::string versionText(XrVersion version) {
    return std::to_string(XR_VERSION_MAJOR(version))+"."+std::to_string(XR_VERSION_MINOR(version))+"."+std::to_string(XR_VERSION_PATCH(version));
}
void check(XrResult result,const char* operation) {
    if(XR_FAILED(result)) {throw std::runtime_error(std::string(operation)+" failed: "+std::to_string(result));}
}
XrPose pose(const XrPosef& p) {return {{p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w},{p.position.x,p.position.y,p.position.z}};}
XrView view(const ::XrView& v) {return {pose(v.pose),{v.fov.angleLeft,v.fov.angleRight,v.fov.angleUp,v.fov.angleDown}};}
#define XR_FUNCTIONS(X) \
    X(DestroyInstance) X(GetSystem) X(EnumerateEnvironmentBlendModes) X(EnumerateViewConfigurationViews) \
    X(CreateSession) X(DestroySession) X(CreateReferenceSpace) X(DestroySpace) X(PollEvent) \
    X(BeginSession) X(EndSession) X(RequestExitSession) X(WaitFrame) X(BeginFrame) X(EndFrame) X(LocateViews) X(LocateSpace) \
    X(EnumerateSwapchainFormats) X(CreateSwapchain) X(DestroySwapchain) X(EnumerateSwapchainImages) \
    X(AcquireSwapchainImage) X(WaitSwapchainImage) X(ReleaseSwapchainImage)
}
struct OpenXrSession::Impl {
    PFN_xrGetInstanceProcAddr loader=nullptr;
#define DECLARE_XR(name) PFN_xr##name name=nullptr;
    XR_FUNCTIONS(DECLARE_XR)
#undef DECLARE_XR
    XrInstance instance=XR_NULL_HANDLE;
    XrSystemId system=XR_NULL_SYSTEM_ID;
    XrSession session=XR_NULL_HANDLE;
    std::unique_ptr<OpenXrActions> actions;
    ControllerInput controls;
    XrSpace local=XR_NULL_HANDLE,head=XR_NULL_HANDLE;
    struct Swapchain {
        XrSwapchain handle=XR_NULL_HANDLE;
        int width=0,height=0;
        std::vector<std::uint32_t> textures;
        bool acquired=false,waited=false;
    };
    std::array<Swapchain,eyeCount> swapchains{};
    XrViewScale scale;
    bool running=false,hasFocus=false,terminal=false;
    bool shutdownRequested=false;
    std::optional<XrPose> seat;
    struct ReferenceChange {XrTime time;std::optional<XrPose> previousFromNew;};
    std::deque<ReferenceChange> spaceChanges;
    explicit Impl(PFN_xrGetInstanceProcAddr getProc,const XrViewScale& viewScale):loader(getProc),scale(viewScale) {}
    ~Impl() {
        for(auto& chain:swapchains) {if(chain.handle&&DestroySwapchain) {DestroySwapchain(chain.handle);}}
        if(head&&DestroySpace) {DestroySpace(head);}
        if(local&&DestroySpace) {DestroySpace(local);}
        if(session&&DestroySession) {DestroySession(session);}
        actions.reset();
        if(instance&&DestroyInstance) {DestroyInstance(instance);}
    }
    template<class T> T proc(const char* name,XrInstance owner) {
        PFN_xrVoidFunction function=nullptr;check(loader(owner,name,&function),name);
        if(!function) {throw std::runtime_error(std::string("Missing OpenXR procedure ")+name);}
        return reinterpret_cast<T>(function);
    }
    void initialize(GlProfile profile,OpenXrBinding api,const OpenXrGraphicsFactory& createGraphics,OpenXrInput input) {
        if(!loader||!createGraphics) {throw std::invalid_argument("OpenXR requires a loader and graphics factory");}
        // Validate authored scale before any runtime side effects.
        xrClipFromNative({{}, {-0.7f,0.7f,0.7f,-0.7f}}, {},scale);
        const char* extension=profile==GlProfile::desktop33?XR_KHR_OPENGL_ENABLE_EXTENSION_NAME:XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME;
        std::vector<const char*> required{extension};
        if(api==OpenXrBinding::egl) {required.push_back(XR_MNDX_EGL_ENABLE_EXTENSION_NAME);}
        const auto enumerate=proc<PFN_xrEnumerateInstanceExtensionProperties>("xrEnumerateInstanceExtensionProperties",XR_NULL_HANDLE);
        std::uint32_t count=0;check(enumerate(nullptr,0,&count,nullptr),"xrEnumerateInstanceExtensionProperties");
        if(count>4096) {throw std::runtime_error("OpenXR extension count exceeds supported domain");}
        std::vector<XrExtensionProperties> extensions(count,{XR_TYPE_EXTENSION_PROPERTIES});
        check(enumerate(nullptr,count,&count,extensions.data()),"xrEnumerateInstanceExtensionProperties");
        for(const auto name:required) {
            if(std::none_of(extensions.begin(),extensions.end(),[&](const auto& item) {return std::strcmp(item.extensionName,name)==0;})) {
                throw std::runtime_error(std::string("OpenXR runtime lacks ")+name);
            }
        }
        constexpr const char* picoExtension="XR_BD_controller_interaction";
        const bool picoProfile=input==OpenXrInput::controllers&&std::any_of(extensions.begin(),extensions.end(),
            [](const auto& item) {return std::strcmp(item.extensionName,"XR_BD_controller_interaction")==0;});
        if(picoProfile) {required.push_back(picoExtension);}
        XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
        std::strcpy(info.applicationInfo.applicationName,"Stellar 7 Enhanced");
        info.applicationInfo.applicationVersion=1;info.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,0);
        info.enabledExtensionCount=static_cast<std::uint32_t>(required.size());info.enabledExtensionNames=required.data();
        XrInstance createdInstance=XR_NULL_HANDLE;
        check(proc<PFN_xrCreateInstance>("xrCreateInstance",XR_NULL_HANDLE)(&info,&createdInstance),"xrCreateInstance");instance=createdInstance;
        // Resolve destruction first so subsequent admission failures release the instance.
        DestroyInstance=proc<PFN_xrDestroyInstance>("xrDestroyInstance",instance);
#define LOAD_XR(name) name=proc<PFN_xr##name>("xr" #name,instance);
        XR_FUNCTIONS(LOAD_XR)
#undef LOAD_XR
        XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};systemInfo.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        check(GetSystem(instance,&systemInfo,&system),"xrGetSystem");
        XrVersion minimum=0,maximum=0;
        if(profile==GlProfile::desktop33) {
            XrGraphicsRequirementsOpenGLKHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
            check(proc<PFN_xrGetOpenGLGraphicsRequirementsKHR>("xrGetOpenGLGraphicsRequirementsKHR",instance)(instance,system,&requirements),"xrGetOpenGLGraphicsRequirementsKHR");
            minimum=requirements.minApiVersionSupported;maximum=requirements.maxApiVersionSupported;
        } else {
            XrGraphicsRequirementsOpenGLESKHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
            check(proc<PFN_xrGetOpenGLESGraphicsRequirementsKHR>("xrGetOpenGLESGraphicsRequirementsKHR",instance)(instance,system,&requirements),"xrGetOpenGLESGraphicsRequirementsKHR");
            minimum=requirements.minApiVersionSupported;maximum=requirements.maxApiVersionSupported;
        }
        minimum=apiVersion(minimum);maximum=apiVersion(maximum);
        if(minimum>maximum) {throw std::runtime_error("Invalid OpenXR GL version range: "+versionText(minimum)+" to "+versionText(maximum));}
        const auto graphics=createGraphics(minimum,maximum);
        if(!graphics.binding||graphics.profile!=profile||graphics.api!=api) {throw std::invalid_argument("OpenXR graphics factory returned a different API or missing binding");}
        const auto actual=apiVersion(graphics.contextVersion);
        if(actual<minimum||actual>maximum) {
            throw std::runtime_error("Current GL context "+versionText(actual)+" outside OpenXR runtime requirements "+versionText(minimum)+" to "+versionText(maximum));
        }
        check(EnumerateEnvironmentBlendModes(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&count,nullptr),"xrEnumerateEnvironmentBlendModes");
        if(count>64) {throw std::runtime_error("OpenXR blend mode count exceeds supported domain");}
        std::vector<XrEnvironmentBlendMode> modes(count);
        check(EnumerateEnvironmentBlendModes(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,count,&count,modes.data()),"xrEnumerateEnvironmentBlendModes");
        if(std::find(modes.begin(),modes.end(),XR_ENVIRONMENT_BLEND_MODE_OPAQUE)==modes.end()) {throw std::runtime_error("OpenXR requires opaque stereo presentation");}
        XrSessionCreateInfo create{XR_TYPE_SESSION_CREATE_INFO};create.next=graphics.binding;create.systemId=system;
        XrSession createdSession=XR_NULL_HANDLE;check(CreateSession(instance,&create,&createdSession),"xrCreateSession");session=createdSession;
        if(input==OpenXrInput::controllers) {actions=std::make_unique<OpenXrActions>(loader,instance,session,picoProfile);}
        XrReferenceSpaceCreateInfo space{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};space.poseInReferenceSpace.orientation.w=1;
        XrSpace createdSpace=XR_NULL_HANDLE;
        space.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;check(CreateReferenceSpace(session,&space,&createdSpace),"xrCreateReferenceSpace LOCAL");local=createdSpace;
        space.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_VIEW;check(CreateReferenceSpace(session,&space,&createdSpace),"xrCreateReferenceSpace VIEW");head=createdSpace;
        initializeSwapchains(graphics.profile);
    }
    void initializeSwapchains(GlProfile profile) {
        std::uint32_t count=0;check(EnumerateSwapchainFormats(session,0,&count,nullptr),"xrEnumerateSwapchainFormats");
        if(count>4096) {throw std::runtime_error("OpenXR format count exceeds supported domain");}
        std::vector<std::int64_t> formats(count);check(EnumerateSwapchainFormats(session,count,&count,formats.data()),"xrEnumerateSwapchainFormats");
        // Desktop GL writes the already display-encoded palette directly with
        // FRAMEBUFFER_SRGB disabled. The compositor decodes an sRGB swapchain.
        const auto format=profile==GlProfile::desktop33&&std::find(formats.begin(),formats.end(),srgb8Format)!=formats.end()?srgb8Format:rgba8Format;
        if(std::find(formats.begin(),formats.end(),format)==formats.end()) {throw std::runtime_error("OpenXR requires GL_RGBA8 or desktop GL_SRGB8_ALPHA8 swapchains");}
        check(EnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&count,nullptr),"xrEnumerateViewConfigurationViews");
        if(count!=eyeCount) {throw std::runtime_error("OpenXR requires exactly two primary stereo views");}
        std::array<XrViewConfigurationView,eyeCount> views{{{XR_TYPE_VIEW_CONFIGURATION_VIEW},{XR_TYPE_VIEW_CONFIGURATION_VIEW}}};
        check(EnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,eyeCount,&count,views.data()),"xrEnumerateViewConfigurationViews");
        if(count!=eyeCount) {throw std::runtime_error("OpenXR stereo configuration changed");}
        for(unsigned eye=0;eye<eyeCount;++eye) {
            const auto& v=views[eye];auto& chain=swapchains[eye];
            if(v.recommendedImageRectWidth==0||v.recommendedImageRectHeight==0||v.recommendedImageRectWidth>maximumImageDimension
                ||v.recommendedImageRectHeight>maximumImageDimension||v.maxSwapchainSampleCount<1) {throw std::runtime_error("Unsupported OpenXR eye dimensions");}
            XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
            info.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_SAMPLED_BIT;info.format=format;
            info.sampleCount=1;info.width=v.recommendedImageRectWidth;info.height=v.recommendedImageRectHeight;info.faceCount=1;info.arraySize=1;info.mipCount=1;
            XrSwapchain createdChain=XR_NULL_HANDLE;
            check(CreateSwapchain(session,&info,&createdChain),"xrCreateSwapchain");chain.handle=createdChain;chain.width=int(info.width);chain.height=int(info.height);
            check(EnumerateSwapchainImages(chain.handle,0,&count,nullptr),"xrEnumerateSwapchainImages");
            if(count==0||count>maximumSwapchainImages) {throw std::runtime_error("Unsupported OpenXR swapchain image count");}
            const auto expected=count;
            if(profile==GlProfile::desktop33) {
                std::vector<XrSwapchainImageOpenGLKHR> images(count,{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
                check(EnumerateSwapchainImages(chain.handle,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())),"xrEnumerateSwapchainImages GL");
                if(count!=expected) {throw std::runtime_error("OpenXR image count changed");}
                for(const auto& image:images) {chain.textures.push_back(image.image);}
            } else {
                std::vector<XrSwapchainImageOpenGLESKHR> images(count,{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
                check(EnumerateSwapchainImages(chain.handle,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())),"xrEnumerateSwapchainImages GLES");
                if(count!=expected) {throw std::runtime_error("OpenXR image count changed");}
                for(const auto& image:images) {chain.textures.push_back(image.image);}
            }
            if(std::find(chain.textures.begin(),chain.textures.end(),0)!=chain.textures.end()) {throw std::runtime_error("OpenXR supplied a null texture");}
        }
    }
    bool events() {
        for(unsigned i=0;i<maximumEventsPerFrame;++i) {
            XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};const auto result=PollEvent(instance,&event);
            if(result==XR_EVENT_UNAVAILABLE) {return true;}
            check(result,"xrPollEvent");
            if(event.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {terminal=true;hasFocus=false;return true;}
            if(event.type==XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) {
                const auto& change=reinterpret_cast<const XrEventDataReferenceSpaceChangePending&>(event);
                if(change.session==session&&change.referenceSpaceType==XR_REFERENCE_SPACE_TYPE_LOCAL) {
                    if(spaceChanges.size()==maximumEventsPerFrame) {terminal=true;hasFocus=false;throw std::runtime_error("Too many pending OpenXR reference changes");}
                    ReferenceChange pending{change.changeTime,change.poseValid?std::optional<XrPose>(pose(change.poseInPreviousSpace)):std::nullopt};
                    const auto position=std::upper_bound(spaceChanges.begin(),spaceChanges.end(),pending.time,
                        [](XrTime time,const auto& item) {return time<item.time;});
                    spaceChanges.insert(position,pending);
                }
            }
            if(event.type!=XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {continue;}
            const auto& change=reinterpret_cast<const XrEventDataSessionStateChanged&>(event);
            if(change.session!=session) {continue;}
            hasFocus=change.state==XR_SESSION_STATE_FOCUSED;
            if(change.state==XR_SESSION_STATE_READY&&!running&&!shutdownRequested) {
                XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};begin.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                check(BeginSession(session,&begin),"xrBeginSession");running=true;
            }
            if(change.state==XR_SESSION_STATE_STOPPING&&running) {check(EndSession(session),"xrEndSession");running=false;seat.reset();spaceChanges.clear();}
            if(change.state==XR_SESSION_STATE_EXITING||change.state==XR_SESSION_STATE_LOSS_PENDING) {terminal=true;hasFocus=false;return true;}
        }
        return false; // Bound a noisy runtime's event drain without beginning a frame yet.
    }
    bool pollShutdown() {
        if(terminal) {return true;}
        if(!shutdownRequested) {
            shutdownRequested=true;hasFocus=false;
            if(running) {
                const auto result=RequestExitSession(session);
                if(result==XR_ERROR_SESSION_NOT_RUNNING) {running=false;}
                else {check(result,"xrRequestExitSession");}
            }
        }
        events();hasFocus=false;
        return !running||terminal;
    }
    OpenXrFrame frame(const std::function<OpenXrLayer()>& select,const std::function<void(const OpenXrEye&)>& draw) {
        controls={};
        if(terminal) {return OpenXrFrame::exiting;}
        if(!events()) {return OpenXrFrame::idle;}
        if(terminal) {return OpenXrFrame::exiting;}
        if(!running) {return OpenXrFrame::idle;}
        if(actions) {controls=actions->poll(hasFocus);}
        XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};XrFrameState state{XR_TYPE_FRAME_STATE};
        check(WaitFrame(session,&wait,&state),"xrWaitFrame");
        XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};check(BeginFrame(session,&begin),"xrBeginFrame");
        bool endAttempted=false;
        const auto end=[&](const XrCompositionLayerBaseHeader* layer) {
            XrFrameEndInfo info{XR_TYPE_FRAME_END_INFO};info.displayTime=state.predictedDisplayTime;info.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
            info.layerCount=layer?1:0;info.layers=layer?&layer:nullptr;endAttempted=true;
            const auto result=EndFrame(session,&info);
            if(XR_FAILED(result)) {terminal=true;hasFocus=false;}
            check(result,"xrEndFrame");
        };
        try {
            if(!state.shouldRender||select()==OpenXrLayer::none) {end(nullptr);return OpenXrFrame::empty;}
            XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            locate.displayTime=state.predictedDisplayTime;locate.space=local;
            XrViewState validity{XR_TYPE_VIEW_STATE};std::uint32_t count=0;
            std::array<::XrView,eyeCount> views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};
            check(LocateViews(session,&locate,&validity,eyeCount,&count,views.data()),"xrLocateViews");
            XrSpaceLocation headLocation{XR_TYPE_SPACE_LOCATION};check(LocateSpace(head,local,state.predictedDisplayTime,&headLocation),"xrLocateSpace VIEW");
            constexpr auto viewFlags=XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT;
            constexpr auto poseFlags=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
            if(count!=eyeCount||(validity.viewStateFlags&viewFlags)!=viewFlags||(headLocation.locationFlags&poseFlags)!=poseFlags) {
                end(nullptr);return OpenXrFrame::empty;
            }
            while(!spaceChanges.empty()&&state.predictedDisplayTime>=spaceChanges.front().time) {
                const auto change=spaceChanges.front();spaceChanges.pop_front();
                if(seat&&change.previousFromNew) {seat=xrRebasedPose(*seat,*change.previousFromNew);}
                else {seat.reset();}
            }
            // Preview can start while the headset is resting on a desk. Commit
            // the seat only with input focus, preserving it on later focus loss.
            const auto origin=seat.value_or(xrSeatedOrigin(pose(headLocation.pose)));
            if(!seat&&hasFocus) {seat=origin;}
            std::array<SpatialMatrix,eyeCount> matrices{};
            for(unsigned eye=0;eye<eyeCount;++eye) {matrices[eye]=xrClipFromNative(view(views[eye]),origin,scale);}
            std::array<XrCompositionLayerProjectionView,eyeCount> projection{{{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};
            for(unsigned eye=0;eye<eyeCount;++eye) {
                auto& chain=swapchains[eye];std::uint32_t index=0;
                XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                check(AcquireSwapchainImage(chain.handle,&acquire,&index),"xrAcquireSwapchainImage");chain.acquired=true;
                XrSwapchainImageWaitInfo ready{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};ready.timeout=XR_INFINITE_DURATION;
                const auto result=WaitSwapchainImage(chain.handle,&ready);
                if(result!=XR_SUCCESS) {check(result,"xrWaitSwapchainImage");throw std::runtime_error("OpenXR infinite image wait did not complete");}
                chain.waited=true;
                if(index>=chain.textures.size()) {throw std::runtime_error("OpenXR acquired image index outside owned storage");}
                draw({eye,chain.textures[index],chain.width,chain.height,matrices[eye]});
                XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};chain.waited=false;
                check(ReleaseSwapchainImage(chain.handle,&release),"xrReleaseSwapchainImage");chain.acquired=false;
                auto& p=projection[eye];p.pose=views[eye].pose;p.fov=views[eye].fov;p.subImage.swapchain=chain.handle;
                p.subImage.imageRect.extent={chain.width,chain.height};
            }
            XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};layer.space=local;layer.viewCount=eyeCount;layer.views=projection.data();
            end(reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer));return OpenXrFrame::submitted;
        } catch(...) {
            const auto failure=std::current_exception();
            for(auto& chain:swapchains) {
                if(chain.waited) {
                    XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};chain.waited=false;
                    const auto result=ReleaseSwapchainImage(chain.handle,&release);chain.acquired=XR_FAILED(result);
                }
                if(chain.acquired) {terminal=true;hasFocus=false;}
            }
            if(!endAttempted) {try {end(nullptr);} catch(...) {terminal=true;hasFocus=false;}}
            std::rethrow_exception(failure);
        }
    }
};
OpenXrSession::OpenXrSession(PFN_xrGetInstanceProcAddr loader,const OpenXrGraphics& graphics,const XrViewScale& scale,OpenXrInput input)
    :impl_(std::make_unique<Impl>(loader,scale)) {
    if(!graphics.binding) {throw std::invalid_argument("OpenXR requires a current graphics binding");}
    impl_->initialize(graphics.profile,graphics.api,[&](XrVersion,XrVersion) {return graphics;},input);
}
OpenXrSession::OpenXrSession(PFN_xrGetInstanceProcAddr loader,GlProfile profile,OpenXrBinding api,
    const XrViewScale& scale,const OpenXrGraphicsFactory& createGraphics,OpenXrInput input)
    :impl_(std::make_unique<Impl>(loader,scale)) {impl_->initialize(profile,api,createGraphics,input);}
OpenXrSession::~OpenXrSession()=default;
OpenXrFrame OpenXrSession::frame(const std::function<void(const OpenXrEye&)>& draw,OpenXrLayer layer) {return impl_->frame([layer] {return layer;},draw);}
OpenXrFrame OpenXrSession::frameFromLatest(const std::function<OpenXrLayer()>& select,const std::function<void(const OpenXrEye&)>& draw) {return impl_->frame(select,draw);}
void OpenXrSession::recenter() {impl_->seat.reset();}
bool OpenXrSession::pollShutdown() {return impl_->pollShutdown();}
bool OpenXrSession::focused() const {return impl_->hasFocus;}
ControllerInput OpenXrSession::controls() const {return impl_->hasFocus&&!impl_->terminal?impl_->controls:ControllerInput{};}
OpenXrFrame presentOpenXrScene(OpenXrSession& session,GlSceneRenderer& renderer,const SpatialScene& scene,const std::vector<Rgb>& palette) {
    bool prepared=false;
    return session.frame([&](const auto& eye) {
        if(!prepared) {renderer.prepare(scene,palette);prepared=true;}
        renderer.renderTexture(eye.clipFromNative,eye.texture,eye.width,eye.height,SpatialOverlay::omit);
    });
}
} // namespace stellar7
