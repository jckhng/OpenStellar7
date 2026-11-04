#define SDL_MAIN_HANDLED
#include "enhanced_sdl.hpp"
#include <cstdlib>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
unsigned enumerated=0,opened=0;
unsigned closed=0;
bool failOpen=true,connected=true;
std::array<Uint8,SDL_CONTROLLER_BUTTON_MAX> buttons{};
Sint16 axis=0;
int fakeCount() {++enumerated;return 1;}
SDL_bool fakeRecognized(int) {return SDL_TRUE;}
SDL_GameController* fakeOpen(int) {++opened;return failOpen?nullptr:reinterpret_cast<SDL_GameController*>(1);}
void fakeClose(SDL_GameController*) {++closed;}
SDL_bool fakeAttached(SDL_GameController*) {return connected?SDL_TRUE:SDL_FALSE;}
Uint8 fakeButton(SDL_GameController*,SDL_GameControllerButton button) {return buttons[button];}
Sint16 fakeAxis(SDL_GameController*,SDL_GameControllerAxis value) {return value==SDL_CONTROLLER_AXIS_LEFTX?axis:0;}
SDL_Joystick* fakeJoystick(SDL_GameController*) {return reinterpret_cast<SDL_Joystick*>(1);}
SDL_JoystickID fakeId(SDL_Joystick*) {return 42;}
Uint32 fakeInitialized(Uint32 flags) {return flags&SDL_INIT_GAMECONTROLLER;}
void require(bool condition,const char* message) {if(!condition) {throw std::runtime_error(message);}}
void environment(const char* name,const char* value) {
#ifdef _WIN32
    require(_putenv_s(name,value)==0,"cannot set test environment");
#else
    require(setenv(name,value,1)==0,"cannot set test environment");
#endif
}
}
#define SDL_NumJoysticks fakeCount
#define SDL_IsGameController fakeRecognized
#define SDL_GameControllerOpen fakeOpen
#define SDL_GameControllerClose fakeClose
#define SDL_GameControllerGetAttached fakeAttached
#define SDL_GameControllerGetButton fakeButton
#define SDL_GameControllerGetAxis fakeAxis
#define SDL_GameControllerGetJoystick fakeJoystick
#define SDL_JoystickInstanceID fakeId
#define SDL_WasInit fakeInitialized
#include "enhanced_sdl.cpp"

int main(int argc,char** argv) try {
    environment("STELLAR7_SAFE_MODE","0");
    environment("STELLAR7_HOST_CONTROLLERS","1");environment("STELLAR7_HOST_RENDERER","software");
    const stellar7::BitmapFont font{};
    stellar7::EnhancedSdl host({},font,25);
    require(opened==1,"initial controller scan missing");
    for(unsigned poll=0;poll<500;++poll) {host.poll(true);}
    require(opened==1&&enumerated==1,"unavailable controller reopened on every source poll");
    SDL_Event added{};added.type=SDL_CONTROLLERDEVICEADDED;
    host.event(added,true,nullptr);
    require(opened==2,"hotplug did not retry unavailable controller");
    for(unsigned poll=0;poll<500;++poll) {host.poll(true);}
    require(opened==2&&enumerated==2,"hotplug retried beyond one scan");
    failOpen=false;host.event(added,true,nullptr);
    buttons[SDL_CONTROLLER_BUTTON_A]=1;host.poll(true);
    require(!host.held(stellar7::EnhancedAction::fire),"connected controller skipped neutral gate");
    buttons.fill(0);host.poll(true);buttons[SDL_CONTROLLER_BUTTON_A]=1;host.poll(true);
    require(host.held(stellar7::EnhancedAction::fire)&&host.pressed(stellar7::EnhancedAction::fire),"controller fire missing");
    host.poll(true);require(!host.pressed(stellar7::EnhancedAction::fire),"held fire repeated press");
    host.poll(false);host.poll(true);
    require(!host.held(stellar7::EnhancedAction::fire),"focus return bypassed neutral gate");
    buttons.fill(0);host.poll(true);axis=-32768;host.poll(true);
    require(host.held(stellar7::EnhancedAction::left),"controller axis input missing");
    axis=0;connected=false;failOpen=true;host.poll(true);
    const auto disconnectedOpens=opened;
    for(unsigned poll=0;poll<500;++poll) {host.poll(true);}
    require(closed==1&&opened==disconnectedOpens&&!host.held(stellar7::EnhancedAction::left),
        "disconnected controller retained input or repeated open");
    environment("STELLAR7_HOST_CONTROLLERS","0");
    stellar7::EnhancedSdl disabled({},font,25);disabled.event(added,true,nullptr);disabled.poll(true);
    require(opened==disconnectedOpens,"disabled input policy still opened a controller");
    stellar7::ControllerInput external;external.connected=true;disabled.externalInput(external,true,nullptr);
    external.set(stellar7::ControllerButton::fire,true);external.set(stellar7::ControllerButton::confirm,true);
    disabled.externalInput(external,true,nullptr);
    require(disabled.held(stellar7::EnhancedAction::fire)&&disabled.confirm(),"XR input was blocked by disabled SDL enumeration");
    disabled.externalInput(external,false,nullptr);disabled.externalInput(external,true,nullptr);
    require(!disabled.held(stellar7::EnhancedAction::fire),"external controller focus return skipped release");
    external.buttons.reset();disabled.externalInput(external,true,nullptr);
    external.set(stellar7::ControllerButton::menu,true);disabled.externalInput(external,true,nullptr);
    require(disabled.pressed(stellar7::EnhancedAction::menu),"external original-menu action missing");
    external.buttons.reset();disabled.externalInput(external,true,nullptr);
    external.set(stellar7::ControllerButton::options,true);disabled.externalInput(external,true,nullptr);
    require(disabled.panelOpen()&&!disabled.held(stellar7::EnhancedAction::fire),"external options failed to suspend gameplay");
    external.buttons.reset();disabled.externalInput(external,true,nullptr);
    external.set(stellar7::ControllerButton::back,true);disabled.externalInput(external,true,nullptr);
    require(!disabled.panelOpen(),"external back could not close native options");
    disabled.cancel();
    SDL_KeyboardEvent view{};view.keysym.scancode=SDL_SCANCODE_F9;
    require(disabled.view()==stellar7::EnhancedView::cockpit,"camera did not default to cockpit");
    require(disabled.key(view,nullptr)&&disabled.view()==stellar7::EnhancedView::chase,"F9 did not select chase view");
    view.repeat=1;require(disabled.key(view,nullptr)&&disabled.view()==stellar7::EnhancedView::chase,"held F9 repeated toggle");
    view.repeat=0;disabled.key(view,nullptr);require(disabled.view()==stellar7::EnhancedView::cockpit,"F9 did not return to cockpit");
    require(argc==2,"requires private build directory");
    const auto path=std::filesystem::u8path(argv[1])/("view-bindings-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".cfg");
    auto options=stellar7::EnhancedOptions{};options.bindings[static_cast<std::size_t>(stellar7::EnhancedAction::fire)].key="F9";
    stellar7::EnhancedOptionsStore(path).save(options);
    stellar7::EnhancedSdl custom(path,font,25);
    require(!custom.key(view,nullptr)&&custom.binding(stellar7::EnhancedAction::fire)==SDL_SCANCODE_F9
        &&custom.view()==stellar7::EnhancedView::cockpit,"camera stole an existing F9 binding");
    SDL_KeyboardEvent panel{};panel.keysym.scancode=SDL_SCANCODE_F12;custom.key(panel,nullptr);
    panel.keysym.scancode=SDL_SCANCODE_DOWN;
    for(unsigned row=0;row<10;++row) {custom.key(panel,nullptr);}
    panel.keysym.scancode=SDL_SCANCODE_RIGHT;custom.key(panel,nullptr);
    require(custom.view()==stellar7::EnhancedView::chase&&custom.binding(stellar7::EnhancedAction::fire)==SDL_SCANCODE_F9,
        "F12 camera setting failed or changed the custom binding");
    panel.keysym.scancode=SDL_SCANCODE_DOWN;custom.key(panel,nullptr);
    panel.keysym.scancode=SDL_SCANCODE_RIGHT;custom.key(panel,nullptr);
    require(custom.options().camera.distance==4900,"F12 chase distance did not adjust");
    for(unsigned repeat=0;repeat<20;++repeat) {custom.key(panel,nullptr);}
    require(custom.options().camera.distance==stellar7::ChaseOptions::maximumDistance,"chase distance exceeded its bound");
    panel.keysym.scancode=SDL_SCANCODE_DOWN;custom.key(panel,nullptr);
    panel.keysym.scancode=SDL_SCANCODE_LEFT;custom.key(panel,nullptr);
    require(custom.options().camera.height==1600,"F12 chase height did not adjust");
    for(unsigned repeat=0;repeat<20;++repeat) {custom.key(panel,nullptr);}
    require(custom.options().camera.height==stellar7::ChaseOptions::minimumHeight,"chase height exceeded its bound");
    require(custom.binding(stellar7::EnhancedAction::fire)==SDL_SCANCODE_F9,"camera options changed held control bindings");
    environment("STELLAR7_GRAPHICS","opengl");
    panel.keysym.scancode=SDL_SCANCODE_DOWN;custom.key(panel,nullptr);custom.key(panel,nullptr);
    panel.keysym.scancode=SDL_SCANCODE_RIGHT;custom.key(panel,nullptr);
    require(custom.options().scene.cockpit==stellar7::CockpitStyle::original,"F12 cockpit style did not toggle");
    panel.keysym.scancode=SDL_SCANCODE_DOWN;custom.key(panel,nullptr);
    panel.keysym.scancode=SDL_SCANCODE_RIGHT;custom.key(panel,nullptr);
    require(!custom.options().scene.groundDetail,"F12 ground detail did not toggle");
    panel.keysym.scancode=SDL_SCANCODE_DOWN;custom.key(panel,nullptr);
    panel.keysym.scancode=SDL_SCANCODE_RIGHT;custom.key(panel,nullptr);
    require(!custom.options().scene.localLights,"F12 local lights did not toggle");
    std::filesystem::remove(path);
} catch(const std::exception& error) {
    std::cerr<<error.what()<<'\n';return 1;
}
