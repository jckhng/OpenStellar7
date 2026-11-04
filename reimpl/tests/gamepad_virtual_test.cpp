#define SDL_MAIN_HANDLED
#include "enhanced_sdl.hpp"
#include <SDL.h>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
void environment(const char* name,const char* value) {
#ifdef _WIN32
    require(_putenv_s(name,value)==0,"test input policy");
#else
    require(setenv(name,value,1)==0,"test input policy");
#endif
}
}
int main() try {
#if SDL_VERSION_ATLEAST(2,24,0)
    environment("STELLAR7_SAFE_MODE","0");environment("STELLAR7_HOST_CONTROLLERS","1");environment("STELLAR7_GAMEPAD","0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI,"0");SDL_SetHint(SDL_HINT_XINPUT_ENABLED,"0");
    SDL_SetHint(SDL_HINT_DIRECTINPUT_ENABLED,"0");SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT,"0");
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT,"0xf00d/0x0007");
    SDL_SetMainReady();require(SDL_Init(SDL_INIT_EVENTS|SDL_INIT_GAMECONTROLLER)==0,"SDL game-controller initialization failed");
    struct Quit {~Quit() {SDL_Quit();}} quit;
    SDL_VirtualJoystickDesc description{};description.version=SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    description.type=SDL_JOYSTICK_TYPE_GAMECONTROLLER;description.naxes=SDL_CONTROLLER_AXIS_MAX;description.nbuttons=SDL_CONTROLLER_BUTTON_MAX;
    description.vendor_id=0xf00d;description.product_id=0x0007;description.name="OpenStellar7 synthetic pad";
    description.axis_mask=(1U<<SDL_CONTROLLER_AXIS_MAX)-1;description.button_mask=(1U<<SDL_CONTROLLER_BUTTON_MAX)-1;
    const auto device=SDL_JoystickAttachVirtualEx(&description);
    require(device>=0&&SDL_IsGameController(device),"virtual Xbox-style controller was not recognized");
    auto* joystick=SDL_JoystickOpen(device);require(joystick!=nullptr,"virtual joystick open failed");
    struct Close {SDL_Joystick* value;~Close() {SDL_JoystickClose(value);}} close{joystick};
    const stellar7::BitmapFont font{};stellar7::EnhancedSdl host({},font,25);
    const auto poll=[&]() {SDL_JoystickUpdate();SDL_GameControllerUpdate();host.poll(true);};
    poll();
    require(SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_A,1)==0,"virtual A input failed");poll();
    require(host.held(stellar7::EnhancedAction::fire)&&host.confirm(),"real SDL A mapping did not reach fire/menu confirm");
    SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_A,0);poll();
    SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_LEFTX,-24000);poll();
    require(host.held(stellar7::EnhancedAction::left),"real SDL left-stick mapping failed");
    host.poll(false);poll();require(!host.held(stellar7::EnhancedAction::left),"real SDL focus return skipped neutral gate");
    SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_LEFTX,0);poll();
    SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_START,1);poll();
    require(host.pressed(stellar7::EnhancedAction::menu),"real SDL Start mapping did not reach preferences");
    require(SDL_JoystickDetachVirtual(device)==0,"virtual controller detach failed");poll();
    require(!host.held(stellar7::EnhancedAction::menu),"real SDL disconnect retained controls");
    std::cout<<"Real SDL virtual gamepad: standard mapping, fire/menu, axes, focus and disconnect pass\n";
#else
    std::cout<<"SDL virtual controller test requires SDL2 >=2.24\n";return 77;
#endif
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
