#include "openxr_actions.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace stellar7 {
namespace {
enum class Action {drive,fire,activate,cycle,jump,shield,menu,options,view,recenter,count};
constexpr std::size_t actionCount=static_cast<std::size_t>(Action::count);
constexpr float stickDeadzone=0.25f,triggerPress=0.55f,triggerRelease=0.45f;
struct Definition {const char* name;const char* label;XrActionType type;};
constexpr std::array<Definition,actionCount> definitions{{
    {"drive","Drive / menu navigation",XR_ACTION_TYPE_VECTOR2F_INPUT},
    {"fire","Fire / menu confirm",XR_ACTION_TYPE_FLOAT_INPUT},
    {"activate","Activate selected ability",XR_ACTION_TYPE_FLOAT_INPUT},
    {"cycle","Cycle ability / menu confirm",XR_ACTION_TYPE_BOOLEAN_INPUT},
    {"jump","Jump / menu back",XR_ACTION_TYPE_BOOLEAN_INPUT},
    {"shield","Shield",XR_ACTION_TYPE_BOOLEAN_INPUT},
    {"menu","Original game menu",XR_ACTION_TYPE_BOOLEAN_INPUT},
    {"options","Native options",XR_ACTION_TYPE_BOOLEAN_INPUT},
    {"view","Cockpit / chase view",XR_ACTION_TYPE_BOOLEAN_INPUT},
    {"recenter","Recenter seat",XR_ACTION_TYPE_BOOLEAN_INPUT}}};
void check(XrResult result,const char* operation) {
    if(XR_FAILED(result)) {throw std::runtime_error(std::string(operation)+" failed: "+std::to_string(result));}
}
}
struct OpenXrActions::Impl {
    XrInstance instance;
    XrSession session;
    PFN_xrGetInstanceProcAddr loader;
    XrActionSet set=XR_NULL_HANDLE;
    std::array<XrAction,actionCount> actions{};
    PFN_xrDestroyActionSet destroy=nullptr;
    PFN_xrStringToPath stringToPath=nullptr;
    PFN_xrSuggestInteractionProfileBindings suggest=nullptr;
    PFN_xrSyncActions sync=nullptr;
    PFN_xrGetActionStateVector2f vectorState=nullptr;
    PFN_xrGetActionStateFloat floatState=nullptr;
    PFN_xrGetActionStateBoolean boolState=nullptr;
    ControllerInput previous;
    Impl(PFN_xrGetInstanceProcAddr getProc,XrInstance owner,XrSession target):instance(owner),session(target),loader(getProc) {}
    ~Impl() {if(set&&destroy) {destroy(set);}}
    template<class T> T proc(const char* name) {
        PFN_xrVoidFunction function=nullptr;check(loader(instance,name,&function),name);
        if(!function) {throw std::runtime_error(std::string("Missing OpenXR input procedure ")+name);}
        return reinterpret_cast<T>(function);
    }
    XrPath path(const std::string& value) {
        XrPath result=XR_NULL_PATH;check(stringToPath(instance,value.c_str(),&result),"xrStringToPath");return result;
    }
    void profile(const char* name,const std::vector<std::pair<Action,const char*>>& entries) {
        std::vector<XrActionSuggestedBinding> bindings;
        for(const auto& entry:entries) {bindings.push_back({actions[static_cast<std::size_t>(entry.first)],path(entry.second)});}
        XrInteractionProfileSuggestedBinding info{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        info.interactionProfile=path(name);info.countSuggestedBindings=static_cast<std::uint32_t>(bindings.size());info.suggestedBindings=bindings.data();
        const auto result=suggest(instance,&info);
        // SteamVR may expose Pico devices through a standard controller profile.
        // A missing profile must not prevent keyboard or another binding fallback.
        if(result!=XR_ERROR_PATH_UNSUPPORTED) {check(result,"xrSuggestInteractionProfileBindings");}
    }
    void initialize(bool picoProfile) {
        destroy=proc<PFN_xrDestroyActionSet>("xrDestroyActionSet");
        stringToPath=proc<PFN_xrStringToPath>("xrStringToPath");
        suggest=proc<PFN_xrSuggestInteractionProfileBindings>("xrSuggestInteractionProfileBindings");
        sync=proc<PFN_xrSyncActions>("xrSyncActions");
        vectorState=proc<PFN_xrGetActionStateVector2f>("xrGetActionStateVector2f");
        floatState=proc<PFN_xrGetActionStateFloat>("xrGetActionStateFloat");
        boolState=proc<PFN_xrGetActionStateBoolean>("xrGetActionStateBoolean");
        XrActionSetCreateInfo create{XR_TYPE_ACTION_SET_CREATE_INFO};
        std::strcpy(create.actionSetName,"gameplay");std::strcpy(create.localizedActionSetName,"Stellar 7 controls");
        XrActionSet created=XR_NULL_HANDLE;
        check(proc<PFN_xrCreateActionSet>("xrCreateActionSet")(instance,&create,&created),"xrCreateActionSet");set=created;
        const auto createAction=proc<PFN_xrCreateAction>("xrCreateAction");
        for(std::size_t i=0;i<actions.size();++i) {
            XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};info.actionType=definitions[i].type;
            std::strcpy(info.actionName,definitions[i].name);std::strcpy(info.localizedActionName,definitions[i].label);
            check(createAction(set,&info,&actions[i]),"xrCreateAction");
        }
        const std::vector<std::pair<Action,const char*>> touch{
            {Action::drive,"/user/hand/left/input/thumbstick"},
            {Action::fire,"/user/hand/right/input/trigger/value"},{Action::activate,"/user/hand/left/input/trigger/value"},
            {Action::cycle,"/user/hand/right/input/a/click"},{Action::jump,"/user/hand/right/input/b/click"},
            {Action::shield,"/user/hand/left/input/x/click"},{Action::options,"/user/hand/left/input/y/click"},
            {Action::menu,"/user/hand/left/input/menu/click"},
            {Action::view,"/user/hand/right/input/thumbstick/click"},{Action::recenter,"/user/hand/left/input/thumbstick/click"}};
        profile("/interaction_profiles/oculus/touch_controller",touch);
        if(picoProfile) {profile("/interaction_profiles/bytedance/pico4_controller",touch);}
        auto index=touch;
        index.erase(std::remove_if(index.begin(),index.end(),[](const auto& binding) {return binding.first==Action::shield;}),index.end());
        for(auto& binding:index) {
            if(binding.first==Action::options) {binding.second="/user/hand/left/input/a/click";}
            if(binding.first==Action::menu) {binding.second="/user/hand/left/input/b/click";}
        }
        profile("/interaction_profiles/valve/index_controller",index);
        profile("/interaction_profiles/khr/simple_controller",{
            {Action::fire,"/user/hand/right/input/select/click"},{Action::activate,"/user/hand/left/input/select/click"},
            {Action::menu,"/user/hand/left/input/menu/click"},{Action::options,"/user/hand/right/input/menu/click"}});
        XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};attach.countActionSets=1;attach.actionSets=&set;
        check(proc<PFN_xrAttachSessionActionSets>("xrAttachSessionActionSets")(session,&attach),"xrAttachSessionActionSets");
    }
    ControllerInput poll(bool focused) {
        if(!focused) {previous={};return {};}
        XrActiveActionSet active{set,XR_NULL_PATH};XrActionsSyncInfo info{XR_TYPE_ACTIONS_SYNC_INFO};info.countActiveActionSets=1;info.activeActionSets=&active;
        const auto result=sync(session,&info);check(result,"xrSyncActions");
        if(result!=XR_SUCCESS) {previous={};return {};}
        ControllerInput input;
        XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};
        get.action=actions[static_cast<std::size_t>(Action::drive)];XrActionStateVector2f drive{XR_TYPE_ACTION_STATE_VECTOR2F};
        check(vectorState(session,&get,&drive),"xrGetActionStateVector2f");
        if(drive.isActive&&std::isfinite(drive.currentState.x)&&std::isfinite(drive.currentState.y)) {
            input.connected=true;
            input.set(ControllerButton::left,drive.currentState.x<-stickDeadzone);input.set(ControllerButton::right,drive.currentState.x>stickDeadzone);
            input.set(ControllerButton::forward,drive.currentState.y>stickDeadzone);input.set(ControllerButton::reverse,drive.currentState.y<-stickDeadzone);
        }
        const auto trigger=[&](Action action,ControllerButton button) {
            get.action=actions[static_cast<std::size_t>(action)];XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
            check(floatState(session,&get,&state),"xrGetActionStateFloat");input.connected=input.connected||state.isActive;
            input.set(button,state.isActive&&std::isfinite(state.currentState)&&state.currentState>=(previous.down(button)?triggerRelease:triggerPress));
        };
        trigger(Action::fire,ControllerButton::fire);trigger(Action::activate,ControllerButton::activate);
        constexpr std::array<ControllerButton,7> buttons{{ControllerButton::cycle,ControllerButton::jump,ControllerButton::shield,
            ControllerButton::menu,ControllerButton::options,ControllerButton::view,ControllerButton::recenter}};
        for(std::size_t i=0;i<buttons.size();++i) {
            get.action=actions[static_cast<std::size_t>(Action::cycle)+i];XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
            check(boolState(session,&get,&state),"xrGetActionStateBoolean");input.connected=input.connected||state.isActive;
            input.set(buttons[i],state.isActive&&state.currentState);
        }
        input.set(ControllerButton::confirm,input.down(ControllerButton::fire)||input.down(ControllerButton::cycle));
        input.set(ControllerButton::back,input.down(ControllerButton::jump));previous=input;return input;
    }
};
OpenXrActions::OpenXrActions(PFN_xrGetInstanceProcAddr loader,XrInstance instance,XrSession session,bool picoProfile)
    :impl_(std::make_unique<Impl>(loader,instance,session)) {impl_->initialize(picoProfile);}
OpenXrActions::~OpenXrActions()=default;
ControllerInput OpenXrActions::poll(bool focused) {return impl_->poll(focused);}
}
