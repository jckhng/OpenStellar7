#include "openxr_test_runtime.hpp"
namespace {
using namespace stellar7;
using namespace stellar7::test::xr;
void mapping(bool pico) {
    Runtime runtime;rt=&runtime;runtime.pico=pico;runtime.controllersConnected=true;
    runtime.events={{XR_SESSION_STATE_READY},{XR_SESSION_STATE_FOCUSED}};
    {
        OpenXrSession xr(getProc,graphics(),{200},OpenXrInput::controllers);
        require(runtime.actionsAttached&&runtime.actionNames.size()==10,"controller semantic actions not attached");
        require((std::find(runtime.profiles.begin(),runtime.profiles.end(),"/interaction_profiles/bytedance/pico4_controller")!=runtime.profiles.end())==pico,
            "Pico profile used without advertised extension or omitted when available");
        const auto step=[&]() {xr.frame([](const auto&) {},OpenXrLayer::none);return xr.controls();};
        require(step().connected,"active controller source was not admitted");
        runtime.driveX=-0.7f;runtime.driveY=0.8f;runtime.fireValue=0.8f;runtime.activateValue=0.7f;
        runtime.inputButtons.fill(true);
        auto input=step();
        for(const auto button:{ControllerButton::left,ControllerButton::forward,ControllerButton::fire,ControllerButton::activate,
            ControllerButton::cycle,ControllerButton::jump,ControllerButton::shield,ControllerButton::menu,ControllerButton::options,
            ControllerButton::view,ControllerButton::recenter,ControllerButton::confirm,ControllerButton::back}) {
            require(input.down(button),"controller action binding/query did not reach normalized input");
        }
        require(!input.down(ControllerButton::right)&&!input.down(ControllerButton::reverse),"opposite drive directions asserted");
        runtime.fireValue=0.5f;require(step().down(ControllerButton::fire),"trigger hysteresis lost held fire");
        runtime.fireValue=0.4f;require(!step().down(ControllerButton::fire),"trigger hysteresis failed to release");
        runtime.inputNotFocused=true;require(!step().connected,"XR_SESSION_NOT_FOCUSED retained controller actions");
        runtime.inputNotFocused=false;runtime.events={{XR_SESSION_STATE_VISIBLE}};
        require(!step().connected,"runtime focus loss retained controls");
        runtime.events={{XR_SESSION_STATE_FOCUSED}};runtime.controllersConnected=false;
        require(!step().connected,"disconnected controller remained active");
    }
    require(runtime.destroyedActionSets==1&&runtime.destroyedSessions==1&&runtime.destroyedInstances==1,"input resources leaked");
}
}
int main() try {
    mapping(false);mapping(true);
    for(const char* failure:{"createActionSet","createAction3","suggestBindings","attachActions"}) {
        Runtime runtime;rt=&runtime;runtime.fail=failure;
        rejects([&] {OpenXrSession xr(getProc,graphics(),{200},OpenXrInput::controllers);});
        require(runtime.destroyedInstances==1&&runtime.destroyedSessions==1
            &&runtime.destroyedActionSets==(std::string(failure)=="createActionSet"?0U:1U),"partial input setup leaked ownership");
    }
    Runtime runtime;rt=&runtime;runtime.unsupportedProfile="/interaction_profiles/oculus/touch_controller";
    {OpenXrSession xr(getProc,graphics(),{200},OpenXrInput::controllers);require(runtime.actionsAttached,"unsupported profile blocked alternate bindings");}
    std::cout<<"OpenXR action lifecycle, Pico/SteamVR fallbacks, mappings, focus and cleanup pass\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
