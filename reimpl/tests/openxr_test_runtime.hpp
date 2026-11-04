#pragma once
#define XR_USE_GRAPHICS_API_OPENGL
#define XR_USE_GRAPHICS_API_OPENGL_ES
#include <SDL_opengl.h>
#include <SDL_egl.h>
#include <openxr/openxr_platform.h>
#include "openxr_session.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace stellar7;
namespace stellar7::test::xr {
void require(bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
template<class T> T handle(std::uintptr_t value) {return reinterpret_cast<T>(value);}
const auto instance=handle<XrInstance>(1);
const auto session=handle<XrSession>(2);
const auto local=handle<XrSpace>(3);
const auto head=handle<XrSpace>(4);
struct Event {XrSessionState state;XrTime change=0;bool poseValid=false;XrPosef previousFromNew{{0,0,0,1},{}};};
struct Runtime {
    std::vector<std::string> calls;
    std::deque<Event> events;
    GlProfile profile=GlProfile::desktop33;
    XrVersion minimumGl=XR_MAKE_VERSION(3,3,0),maximumGl=XR_MAKE_VERSION(4,6,0);
    bool extension=true,egl=false,render=true,valid=true,headValid=true,opaque=true,rgba=true,srgb=false;
    std::int64_t chainFormat=0;
    std::function<void(unsigned)> onAcquire;
    bool timeout=false,badIndex=false,missingProc=false;
    XrTime time=100,located=0,ended=0;
    unsigned configCount=2,viewCount=2,createdChains=0,createdSpaces=0;
    unsigned destroyedInstances=0,destroyedSessions=0,destroyedSpaces=0,destroyedChains=0;
    unsigned begins=0,ends=0,layers=0,frames=0;
    unsigned exitRequests=0;
    bool pico=false,controllersConnected=false,inputNotFocused=false,actionsAttached=false;
    float driveX=0,driveY=0,fireValue=0,activateValue=0;
    std::array<bool,7> inputButtons{};
    std::vector<std::string> actionNames,paths,profiles;
    std::vector<XrActionType> actionTypes;
    std::string unsupportedProfile;
    unsigned destroyedActionSets=0;
    std::array<bool,2> acquired{},waited{};
    std::array<std::array<std::uint32_t,2>,2> textures{{{{101,102}},{{201,202}}}};
    XrPosef headPose{{0,0,0,1},{2,1.6f,3}};
    std::string fail;
    void call(const std::string& name) {calls.push_back(name);}
    XrResult result(const std::string& name) {call(name);return fail==name?XR_ERROR_RUNTIME_FAILURE:XR_SUCCESS;}
    unsigned eye(XrSwapchain chain) {return chain==handle<XrSwapchain>(10)?0:1;}
};
Runtime* rt=nullptr;
XrResult XRAPI_CALL enumerateExtensions(const char*,std::uint32_t cap,std::uint32_t* count,XrExtensionProperties* out) {
    *count=rt->extension?(rt->egl?2:1)+(rt->pico?1:0):0;
    if(cap&&rt->extension) {
        require(out[0].type==XR_TYPE_EXTENSION_PROPERTIES,"extension type header");
        std::strcpy(out[0].extensionName,rt->profile==GlProfile::desktop33?XR_KHR_OPENGL_ENABLE_EXTENSION_NAME:XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME);
        if(rt->egl) {out[1].type=XR_TYPE_EXTENSION_PROPERTIES;std::strcpy(out[1].extensionName,"XR_MNDX_egl_enable");}
        if(rt->pico) {std::strcpy(out[rt->egl?2:1].extensionName,"XR_BD_controller_interaction");}
    }
    return XR_SUCCESS;
}
XrResult XRAPI_CALL createInstance(const XrInstanceCreateInfo* info,XrInstance* out) {
    require(info->enabledExtensionCount==(rt->egl?2u:1u)+(rt->pico?1u:0u)&&info->applicationInfo.apiVersion==XR_MAKE_VERSION(1,0,0),"instance version/extensions");
    if(rt->egl) {require(std::strcmp(info->enabledExtensionNames[1],"XR_MNDX_egl_enable")==0,"EGL extension not enabled");}
    *out=instance;return rt->result("createInstance");
}
XrResult XRAPI_CALL destroyInstance(XrInstance value) {require(value==instance,"instance owner");++rt->destroyedInstances;return rt->result("destroyInstance");}
XrResult XRAPI_CALL getSystem(XrInstance,const XrSystemGetInfo* info,XrSystemId* out) {
    require(info->formFactor==XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY,"headset form factor");*out=5;return rt->result("getSystem");
}
XrResult XRAPI_CALL requirementsGl(XrInstance,XrSystemId,XrGraphicsRequirementsOpenGLKHR* out) {
    require(out->type==XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR,"GL requirements type");
    out->minApiVersionSupported=rt->minimumGl;out->maxApiVersionSupported=rt->maximumGl;return rt->result("requirements");
}
XrResult XRAPI_CALL requirementsEs(XrInstance,XrSystemId,XrGraphicsRequirementsOpenGLESKHR* out) {
    require(out->type==XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR,"GLES requirements type");
    out->minApiVersionSupported=XR_MAKE_VERSION(3,0,0);out->maxApiVersionSupported=XR_MAKE_VERSION(3,2,0);return rt->result("requirements");
}
XrResult XRAPI_CALL blendModes(XrInstance,XrSystemId,XrViewConfigurationType type,std::uint32_t cap,std::uint32_t* count,XrEnvironmentBlendMode* out) {
    require(type==XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,"stereo blend configuration");
    *count=1;if(cap) {out[0]=rt->opaque?XR_ENVIRONMENT_BLEND_MODE_OPAQUE:XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND;}return XR_SUCCESS;
}
XrResult XRAPI_CALL createSession(XrInstance,const XrSessionCreateInfo* info,XrSession* out) {
    require(info->next&&info->systemId==5,"current graphics binding");
    require(std::find(rt->calls.begin(),rt->calls.end(),"requirements")!=rt->calls.end(),"graphics requirements skipped");
    *out=session;return rt->result("createSession");
}
XrResult XRAPI_CALL destroySession(XrSession value) {require(value==session,"session owner");++rt->destroyedSessions;return rt->result("destroySession");}
XrResult XRAPI_CALL createSpace(XrSession,const XrReferenceSpaceCreateInfo* info,XrSpace* out) {
    ++rt->createdSpaces;require(info->poseInReferenceSpace.orientation.w==1,"reference identity pose");
    if(info->referenceSpaceType==XR_REFERENCE_SPACE_TYPE_LOCAL) {*out=local;}
    else {require(info->referenceSpaceType==XR_REFERENCE_SPACE_TYPE_VIEW,"head VIEW reference");*out=head;}
    return rt->result("createSpace"+std::to_string(rt->createdSpaces));
}
XrResult XRAPI_CALL destroySpace(XrSpace) {++rt->destroyedSpaces;return rt->result("destroySpace");}
XrResult XRAPI_CALL formats(XrSession,std::uint32_t cap,std::uint32_t* count,std::int64_t* out) {
    std::vector<std::int64_t> formats;
    if(rt->rgba) {formats.push_back(GL_RGBA8);}
    if(rt->srgb) {formats.push_back(GL_SRGB8_ALPHA8);}
    if(formats.empty()) {formats.push_back(GL_RGBA16F);}
    *count=static_cast<std::uint32_t>(formats.size());
    for(unsigned i=0;i<std::min(cap,*count);++i) {out[i]=formats[i];}
    return XR_SUCCESS;
}
XrResult XRAPI_CALL configurations(XrInstance,XrSystemId,XrViewConfigurationType,std::uint32_t cap,std::uint32_t* count,XrViewConfigurationView* out) {
    *count=rt->configCount;
    for(unsigned i=0;i<std::min(cap,*count);++i) {
        require(out[i].type==XR_TYPE_VIEW_CONFIGURATION_VIEW,"view configuration type");
        out[i].recommendedImageRectWidth=320;out[i].recommendedImageRectHeight=200;
        out[i].maxImageRectWidth=1024;out[i].maxImageRectHeight=1024;out[i].maxSwapchainSampleCount=4;
    }
    return XR_SUCCESS;
}
XrResult XRAPI_CALL createChain(XrSession,const XrSwapchainCreateInfo* info,XrSwapchain* out) {
    require((info->format==GL_RGBA8||(rt->profile==GlProfile::desktop33&&info->format==GL_SRGB8_ALPHA8))
        &&info->sampleCount==1&&info->arraySize==1&&info->faceCount==1&&info->mipCount==1,"swapchain domain");
    rt->chainFormat=info->format;
    require(info->width==320&&info->height==200,"runtime eye size ignored");
    ++rt->createdChains;*out=handle<XrSwapchain>(9+rt->createdChains);return rt->result("createChain"+std::to_string(rt->createdChains));
}
XrResult XRAPI_CALL destroyChain(XrSwapchain) {++rt->destroyedChains;return rt->result("destroyChain");}
XrResult XRAPI_CALL images(XrSwapchain chain,std::uint32_t cap,std::uint32_t* count,XrSwapchainImageBaseHeader* out) {
    *count=2;
    for(unsigned i=0;i<cap;++i) {
        const auto texture=rt->textures[rt->eye(chain)][i];
        if(rt->profile==GlProfile::desktop33) {
            auto* gl=reinterpret_cast<XrSwapchainImageOpenGLKHR*>(out);require(gl[i].type==XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR,"GL image type");gl[i].image=texture;
        } else {
            auto* es=reinterpret_cast<XrSwapchainImageOpenGLESKHR*>(out);require(es[i].type==XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR,"GLES image type");es[i].image=texture;
        }
    }
    return XR_SUCCESS;
}
XrResult XRAPI_CALL poll(XrInstance,XrEventDataBuffer* out) {
    if(rt->events.empty()) {return XR_EVENT_UNAVAILABLE;}
    const auto e=rt->events.front();rt->events.pop_front();
    if(e.change) {
        XrEventDataReferenceSpaceChangePending change{XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING};
        change.session=session;change.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;change.changeTime=e.change;
        change.poseValid=e.poseValid?XR_TRUE:XR_FALSE;change.poseInPreviousSpace=e.previousFromNew;
        std::memcpy(out,&change,sizeof(change));return XR_SUCCESS;
    }
    XrEventDataSessionStateChanged state{XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED};state.session=session;state.state=e.state;
    std::memcpy(out,&state,sizeof(state));return rt->result("event");
}
XrResult XRAPI_CALL beginSession(XrSession,const XrSessionBeginInfo* info) {require(info->primaryViewConfigurationType==XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,"session stereo");++rt->begins;return rt->result("beginSession");}
XrResult XRAPI_CALL endSession(XrSession) {++rt->ends;return rt->result("endSession");}
XrResult XRAPI_CALL requestExit(XrSession) {
    ++rt->exitRequests;rt->events.push_back({XR_SESSION_STATE_STOPPING});rt->events.push_back({XR_SESSION_STATE_EXITING});
    return rt->result("requestExit");
}
XrResult XRAPI_CALL waitFrame(XrSession,const XrFrameWaitInfo*,XrFrameState* state) {
    state->predictedDisplayTime=rt->time;state->predictedDisplayPeriod=11;state->shouldRender=rt->render?XR_TRUE:XR_FALSE;return rt->result("waitFrame");
}
XrResult XRAPI_CALL beginFrame(XrSession,const XrFrameBeginInfo*) {return rt->result("beginFrame");}
XrResult XRAPI_CALL locateViews(XrSession,const XrViewLocateInfo* info,XrViewState* state,std::uint32_t cap,std::uint32_t* count,::XrView* views) {
    require(info->space==local&&info->displayTime==rt->time&&cap==2,"predicted view location");rt->located=info->displayTime;
    state->viewStateFlags=rt->valid?(XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT):0;*count=rt->viewCount;
    for(unsigned i=0;i<2;++i) {views[i].pose=rt->headPose;views[i].pose.position.x+=(i==0?-0.032f:0.032f);views[i].fov={-0.7f,0.8f,0.75f,-0.65f};}
    return rt->result("locateViews");
}
XrResult XRAPI_CALL locateSpace(XrSpace space,XrSpace base,XrTime time,XrSpaceLocation* out) {
    require(space==head&&base==local&&time==rt->time,"predicted head location");
    out->locationFlags=rt->headValid?(XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT):0;out->pose=rt->headPose;return rt->result("locateSpace");
}
XrResult XRAPI_CALL acquire(XrSwapchain chain,const XrSwapchainImageAcquireInfo*,std::uint32_t* index) {
    const auto e=rt->eye(chain);require(!rt->acquired[e],"double acquire");
    const auto result=rt->result("acquire"+std::to_string(e));
    if(XR_SUCCEEDED(result)) {rt->acquired[e]=true;*index=rt->badIndex?9:(1-e);if(rt->onAcquire) {rt->onAcquire(e);}}
    return result;
}
XrResult XRAPI_CALL waitImage(XrSwapchain chain,const XrSwapchainImageWaitInfo* info) {
    const auto e=rt->eye(chain);require(rt->acquired[e]&&!rt->waited[e]&&info->timeout==XR_INFINITE_DURATION,"image wait order");
    rt->call("wait"+std::to_string(e));if(rt->timeout) {return XR_TIMEOUT_EXPIRED;}
    rt->waited[e]=true;return XR_SUCCESS;
}
XrResult XRAPI_CALL release(XrSwapchain chain,const XrSwapchainImageReleaseInfo*) {
    const auto e=rt->eye(chain);require(rt->acquired[e]&&rt->waited[e],"image released before successful wait");
    const auto result=rt->result("release"+std::to_string(e));
    if(XR_SUCCEEDED(result)) {rt->acquired[e]=rt->waited[e]=false;}return result;
}
XrResult XRAPI_CALL endFrame(XrSession,const XrFrameEndInfo* info) {
    require(info->displayTime==rt->time&&info->environmentBlendMode==XR_ENVIRONMENT_BLEND_MODE_OPAQUE,"submission time/blend");
    rt->ended=info->displayTime;rt->layers=info->layerCount;++rt->frames;
    if(info->layerCount) {
        require(info->layerCount==1&&!rt->acquired[0]&&!rt->acquired[1],"submitted before releasing both eyes");
        const auto* layer=reinterpret_cast<const XrCompositionLayerProjection*>(info->layers[0]);
        require(layer->space==local&&layer->viewCount==2,"projection ownership");
        for(unsigned i=0;i<2;++i) {
            const auto& v=layer->views[i];require(v.subImage.swapchain==handle<XrSwapchain>(10+i)&&v.subImage.imageArrayIndex==0,"projection image ownership");
            require(v.fov.angleLeft==-0.7f&&v.fov.angleRight==0.8f&&v.pose.position.y==rt->headPose.position.y,"runtime views rewritten");
            require(v.subImage.imageRect.extent.width==320&&v.subImage.imageRect.extent.height==200,"projection image extent");
        }
    }
    return rt->result("endFrame");
}
XrResult XRAPI_CALL createActionSet(XrInstance,const XrActionSetCreateInfo* info,XrActionSet* out) {
    require(info->type==XR_TYPE_ACTION_SET_CREATE_INFO&&std::strcmp(info->actionSetName,"gameplay")==0,"action set identity");
    *out=handle<XrActionSet>(300);return rt->result("createActionSet");
}
XrResult XRAPI_CALL destroyActionSet(XrActionSet) {++rt->destroyedActionSets;return rt->result("destroyActionSet");}
XrResult XRAPI_CALL createAction(XrActionSet,const XrActionCreateInfo* info,XrAction* out) {
    require(info->type==XR_TYPE_ACTION_CREATE_INFO&&info->countSubactionPaths==0,"semantic action definition");
    rt->actionNames.push_back(info->actionName);rt->actionTypes.push_back(info->actionType);
    *out=handle<XrAction>(1000+rt->actionNames.size()-1);return rt->result("createAction"+std::to_string(rt->actionNames.size()));
}
XrResult XRAPI_CALL stringToPath(XrInstance,const char* name,XrPath* out) {
    const auto found=std::find(rt->paths.begin(),rt->paths.end(),name);
    if(found==rt->paths.end()) {rt->paths.push_back(name);*out=rt->paths.size();}
    else {*out=static_cast<XrPath>(found-rt->paths.begin()+1);}
    return XR_SUCCESS;
}
XrResult XRAPI_CALL suggestBindings(XrInstance,const XrInteractionProfileSuggestedBinding* info) {
    const auto name=rt->paths.at(static_cast<std::size_t>(info->interactionProfile)-1);rt->profiles.push_back(name);
    require(info->type==XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING&&info->countSuggestedBindings>0,"controller profile bindings missing");
    for(unsigned i=0;i<info->countSuggestedBindings;++i) {
        require(info->suggestedBindings[i].action!=XR_NULL_HANDLE&&info->suggestedBindings[i].binding!=XR_NULL_PATH,"null semantic action binding");
    }
    return name==rt->unsupportedProfile?XR_ERROR_PATH_UNSUPPORTED:rt->result("suggestBindings");
}
XrResult XRAPI_CALL attachActions(XrSession,const XrSessionActionSetsAttachInfo* info) {
    require(info->type==XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO&&info->countActionSets==1,"action set attachment");
    rt->actionsAttached=true;return rt->result("attachActions");
}
XrResult XRAPI_CALL syncActions(XrSession,const XrActionsSyncInfo* info) {
    require(rt->actionsAttached&&info->type==XR_TYPE_ACTIONS_SYNC_INFO&&info->countActiveActionSets==1,"unattached action synchronization");
    if(rt->inputNotFocused) {return XR_SESSION_NOT_FOCUSED;}
    return rt->result("syncActions");
}
std::size_t actionIndex(const XrActionStateGetInfo* info,XrActionType type) {
    const auto index=reinterpret_cast<std::uintptr_t>(info->action)-1000;
    require(info->type==XR_TYPE_ACTION_STATE_GET_INFO&&info->subactionPath==XR_NULL_PATH&&rt->actionTypes.at(index)==type,"wrong action query type");return index;
}
XrResult XRAPI_CALL vectorAction(XrSession,const XrActionStateGetInfo* info,XrActionStateVector2f* out) {
    require(actionIndex(info,XR_ACTION_TYPE_VECTOR2F_INPUT)==0&&out->type==XR_TYPE_ACTION_STATE_VECTOR2F,"drive action query");
    out->isActive=rt->controllersConnected;out->currentState={rt->driveX,rt->driveY};return rt->result("vectorAction");
}
XrResult XRAPI_CALL floatAction(XrSession,const XrActionStateGetInfo* info,XrActionStateFloat* out) {
    const auto index=actionIndex(info,XR_ACTION_TYPE_FLOAT_INPUT);require(out->type==XR_TYPE_ACTION_STATE_FLOAT,"trigger action query");
    out->isActive=rt->controllersConnected;out->currentState=index==1?rt->fireValue:rt->activateValue;return rt->result("floatAction");
}
XrResult XRAPI_CALL boolAction(XrSession,const XrActionStateGetInfo* info,XrActionStateBoolean* out) {
    const auto index=actionIndex(info,XR_ACTION_TYPE_BOOLEAN_INPUT);require(out->type==XR_TYPE_ACTION_STATE_BOOLEAN,"button action query");
    out->isActive=rt->controllersConnected;out->currentState=rt->inputButtons.at(index-3);return rt->result("boolAction");
}
XrResult XRAPI_CALL getProc(XrInstance,const char* name,PFN_xrVoidFunction* out) {
    *out=nullptr;
    if(rt->missingProc&&std::strcmp(name,"xrLocateViews")==0) {return XR_ERROR_FUNCTION_UNSUPPORTED;}
#define MAP(api,function) if(std::strcmp(name,"xr" #api)==0) {*out=reinterpret_cast<PFN_xrVoidFunction>(function);return XR_SUCCESS;}
    MAP(EnumerateInstanceExtensionProperties,enumerateExtensions) MAP(CreateInstance,createInstance) MAP(DestroyInstance,destroyInstance)
    MAP(GetSystem,getSystem) MAP(GetOpenGLGraphicsRequirementsKHR,requirementsGl) MAP(GetOpenGLESGraphicsRequirementsKHR,requirementsEs)
    MAP(EnumerateEnvironmentBlendModes,blendModes) MAP(CreateSession,createSession) MAP(DestroySession,destroySession)
    MAP(CreateReferenceSpace,createSpace) MAP(DestroySpace,destroySpace) MAP(EnumerateSwapchainFormats,formats)
    MAP(EnumerateViewConfigurationViews,configurations) MAP(CreateSwapchain,createChain) MAP(DestroySwapchain,destroyChain)
    MAP(EnumerateSwapchainImages,images) MAP(PollEvent,poll) MAP(BeginSession,beginSession) MAP(EndSession,endSession)
    MAP(RequestExitSession,requestExit)
    MAP(WaitFrame,waitFrame) MAP(BeginFrame,beginFrame) MAP(LocateViews,locateViews) MAP(LocateSpace,locateSpace)
    MAP(CreateActionSet,createActionSet) MAP(DestroyActionSet,destroyActionSet) MAP(CreateAction,createAction) MAP(StringToPath,stringToPath)
    MAP(SuggestInteractionProfileBindings,suggestBindings) MAP(AttachSessionActionSets,attachActions) MAP(SyncActions,syncActions)
    MAP(GetActionStateVector2f,vectorAction) MAP(GetActionStateFloat,floatAction) MAP(GetActionStateBoolean,boolAction)
    MAP(AcquireSwapchainImage,acquire) MAP(WaitSwapchainImage,waitImage) MAP(ReleaseSwapchainImage,release) MAP(EndFrame,endFrame)
#undef MAP
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}
OpenXrGraphics graphics(GlProfile profile=GlProfile::desktop33) {
    static const XrBaseInStructure binding{XR_TYPE_UNKNOWN};return {profile,&binding,XR_MAKE_VERSION(3,profile==GlProfile::desktop33?3:0,0)};
}
template<class F> void rejects(F action) {bool failed=false;try {action();} catch(const std::exception&) {failed=true;}require(failed,"unsupported OpenXR domain admitted");}
} // namespace stellar7::test::xr
