#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>

#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
struct Device {JOYCAPSW caps{};JOYINFOEX position{};bool available=false;MMRESULT capsResult=JOYERR_NOERROR;};
std::array<Device,3> devices;
UINT deviceCount=0;
std::vector<UINT> positionQueries;
bool validRequests=true;
UINT WINAPI fakeCount() {return deviceCount;}
MMRESULT WINAPI fakeCaps(const UINT_PTR id,LPJOYCAPSW output,const UINT size) {
    validRequests=validRequests&&size==sizeof(JOYCAPSW);
    if(id>=devices.size()) {return MMSYSERR_BADDEVICEID;}
    *output=devices[id].caps;return devices[id].capsResult;
}
MMRESULT WINAPI fakePosition(const UINT id,LPJOYINFOEX output) {
    positionQueries.push_back(id);
    validRequests=validRequests&&output->dwSize==sizeof(JOYINFOEX)
        &&output->dwFlags==(JOY_RETURNX|JOY_RETURNY|JOY_RETURNBUTTONS);
    if(id>=devices.size()||!devices[id].available) {return JOYERR_UNPLUGGED;}
    *output=devices[id].position;return JOYERR_NOERROR;
}
}
#define joyGetNumDevs fakeCount
#define joyGetDevCapsW fakeCaps
#define joyGetPosEx fakePosition
#include "win32_joystick.hpp"
#undef joyGetNumDevs
#undef joyGetDevCapsW
#undef joyGetPosEx
#include "win32_source_keys.hpp"

namespace {
using namespace stellar7;
void require(const bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
void reset() {devices={};deviceCount=0;positionQueries.clear();validRequests=true;}
void device(const UINT id,const UINT minimum=0,const UINT maximum=65535) {
    auto& value=devices[id];value.available=true;
    value.caps.wNumAxes=2;value.caps.wNumButtons=8;
    value.caps.wXmin=value.caps.wYmin=minimum;value.caps.wXmax=value.caps.wYmax=maximum;
    value.position.dwXpos=minimum;value.position.dwYpos=maximum;
}
void adapter() {
    reset();Win32Joystick absent;
    require(!absent.poll().connected&&absent.metrics().positionQueries==0,"absent driver performed a device query");
    reset();deviceCount=3;device(0);device(1);device(2);
    devices[0].available=false;devices[1].caps.wNumAxes=1;
    devices[2].position.dwButtons=JOY_BUTTON1|JOY_BUTTON2|JOY_BUTTON3;
    Win32Joystick joystick;
    auto sample=joystick.poll();
    require(sample.connected&&sample.x==0&&sample.y==65535&&sample.buttons==3,
        "adapter did not normalize declared ranges or mask unsupported buttons");
    require(joystick.capabilities()&&joystick.capabilities()->device==2
        &&joystick.capabilities()->axes==2&&joystick.capabilities()->buttons==8,
        "adapter did not retain selected device capabilities");
    require((positionQueries==std::vector<UINT>{0,2})&&validRequests,"adapter query flags/size/filter order differ");
    devices[0].available=true;devices[2].available=false;
    require(!joystick.poll().connected&&joystick.capabilities()->device==2
        &&joystick.metrics().connectionLosses==1,"disconnect rebound the primary joystick");
    require(positionQueries.back()==2,"disconnect silently sampled a different device");
    devices[2].available=true;devices[2].position.dwButtons=3;
    require(joystick.poll().connected&&joystick.poll().buttons==0,"reconnect synthesized a held-button press");
    devices[2].position.dwButtons=0;require(joystick.poll().buttons==0,"reconnect release did not rearm");
    devices[2].position.dwButtons=3;require(joystick.poll().buttons==3,"fresh post-reconnect press was lost");
    joystick.suspend();require(!joystick.connected(),"focus suspension remained connected");
    require(joystick.poll().buttons==0,"focus regain synthesized held-button action");
    devices[2].position.dwButtons=0;joystick.poll();
    devices[2].position.dwButtons=2;require(joystick.poll().buttons==2,"focus release failed to rearm");

    reset();deviceCount=1;device(0,100,300);
    devices[0].position.dwXpos=200;devices[0].position.dwYpos=400;
    Win32Joystick scaled;sample=scaled.poll();
    require(sample.x==32767&&sample.y==65535,"adapter range rounding/clamp differs");
    scaled.suspend();devices[0].caps.wXmin=devices[0].caps.wXmax=100;
    require(!scaled.poll().connected,"adapter admitted a zero-width device range");
    require(validRequests,"adapter supplied uninitialized or extra WinMM flags");
}

LPARAM keyData(const unsigned scan,const unsigned repeats=1) {
    return static_cast<LPARAM>((scan<<16)|repeats);
}
void words() {
    Win32SourceKeys queue;
    queue.keyDown(WM_KEYDOWN,VK_SHIFT,keyData(0x2a));
    queue.keyDown(WM_KEYUP,'A',keyData(0x1e));
    require(queue.size()==0,"modifiers/releases created BIOS words");
    queue.keyDown(WM_KEYDOWN,'A',keyData(0x1e));queue.keyDown(WM_KEYDOWN,'B',keyData(0x30));
    queue.character('a',keyData(0x1e));queue.character('B',keyData(0x30));
    require(queue.front()==0x1e61&&queue.front()==0x1e61&&queue.size()==2,"actual scan/character was replaced or dequeued early");
    queue.pop();require(queue.front()==0x3042,"queued character/scan ordering differs");queue.pop();
    queue.keyDown(WM_KEYDOWN,VK_ESCAPE,keyData(1));queue.character(27,keyData(1));
    require(queue.front()==0x011b&&queue.size()==1,"Escape was lost or duplicated by WM_CHAR");queue.pop();
    queue.keyDown(WM_KEYDOWN,VK_RETURN,keyData(0x1c));queue.character(13,keyData(0x1c));
    require(queue.front()==0x1c0d,"Enter did not preserve its actual BIOS word");queue.pop();
    queue.keyDown(WM_KEYDOWN,VK_UP,keyData(0x48));
    require(queue.front()==0x4800,"extended navigation did not preserve actual scan-only input");queue.pop();
    queue.keyDown(WM_KEYDOWN,'A',keyData(0x1e,3));queue.character('A',keyData(0x1e,3));
    for(unsigned index=0;index<3;++index) {require(queue.front()==0x1e41,"keyboard repeat words differ");queue.pop();}
    queue.keyDown(WM_SYSKEYDOWN,VK_F1,keyData(0x3b));
    require(queue.front()==0x3b00,"function key was synthesized as Escape or Space");queue.clear();
    queue.keyDown(WM_KEYDOWN,'A',keyData(0x1e));queue.character(0x03a9,keyData(0x1e));
    require(queue.front()==0x1e00,"unsupported Unicode was truncated into a fabricated DOS byte");queue.clear();
    queue.keyDown(WM_KEYDOWN,'A',keyData(0x1e,20));queue.character('a',keyData(0x1e,20));
    require(queue.size()==15&&queue.front()==0x1e61,"bounded queue overwrote older source words");
    queue.clear();require(queue.front()==0,"native focus cancellation retained stale source keys");
}
}

int main() {
    try {adapter();words();std::cout<<"Mock WinMM capabilities/ranges/disconnect/focus and native source-word queue PASS\n";return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
