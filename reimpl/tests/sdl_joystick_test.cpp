#include <SDL.h>
#include <cassert>
#include <cstdint>

namespace {
constexpr unsigned usableId=42;
bool connected=true;
unsigned opened=0,closed=0;
SDL_Joystick* device(unsigned id) {return reinterpret_cast<SDL_Joystick*>(static_cast<std::uintptr_t>(id));}
auto fakeId(SDL_Joystick* value) {return static_cast<SDL_JoystickID>(reinterpret_cast<std::uintptr_t>(value));}
void fakeClose(SDL_Joystick*) {++closed;}
int fakeAxes(SDL_Joystick* value) {return fakeId(value)==usableId?2:1;}
int fakeButtons(SDL_Joystick*) {return 2;}
Sint16 fakeAxis(SDL_Joystick*,int axis) {return axis==0?-32768:32767;}
int fakeCount() {return 2;}
SDL_Joystick* fakeOpen(int index) {++opened;return device(index==1?usableId:17);}
SDL_bool fakeConnected(SDL_Joystick*) {return connected?SDL_TRUE:SDL_FALSE;}
Uint8 fakeButton(SDL_Joystick*,int) {return 0;}
}

#define SDL_NumJoysticks fakeCount
#define SDL_JoystickOpen fakeOpen
#define SDL_JoystickClose fakeClose
#define SDL_JoystickInstanceID fakeId
#define SDL_JoystickGetAttached fakeConnected
#define SDL_JoystickNumAxes fakeAxes
#define SDL_JoystickNumButtons fakeButtons
#define SDL_JoystickGetAxis fakeAxis
#define SDL_JoystickGetButton fakeButton
#include "sdl_input.hpp"

int main() {
    using namespace stellar7;
    {
        sdlHost::Joystick joystick;
        auto sample=joystick.poll(true);
        assert(opened==2&&closed==1&&sample.connected&&sample.x==0&&sample.y==65535&&sample.buttons==0);
        SDL_Event event{};
        event.type=SDL_JOYBUTTONDOWN;
        event.jbutton.which=17;event.jbutton.button=0;
        joystick.event(event);assert(joystick.poll(true).buttons==0);
        // A tap between polls survives once; device indices are not instance IDs.
        event.jbutton.which=usableId;joystick.event(event);
        assert(joystick.poll(true).buttons==1&&joystick.poll(true).buttons==0);
        joystick.event(event);sample=joystick.poll(false);
        assert(sample.phase==NativeJoystickPhase::cancel&&!sample.connected);
        assert(joystick.poll(true).buttons==0);
        connected=false;sample=joystick.poll(true);
        assert(sample.phase==NativeJoystickPhase::cancel&&!sample.connected&&closed==2);
        connected=true;sample=joystick.poll(true);
        assert(sample.connected&&sample.buttons==0&&opened==4&&closed==3);
    }
    assert(closed==4);
}
