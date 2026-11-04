#pragma once
#include "sdl_host.hpp"
#include "stellar7/native_joystick.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <optional>

namespace stellar7::sdlHost {
class Pulse {
public:
    void transition(bool down) {if(received_!=down) {received_=down;pending_.push_back(down);}}
    void poll() {if(!pending_.empty()) {held_=pending_.front();pending_.pop_front();}}
    bool held() const {return held_;}
    void clear() {received_=false;held_=false;pending_.clear();}
private:
    bool received_=false,held_=false;
    std::deque<bool> pending_;
};

class Keys {
public:
    void transition(SDL_Scancode key,bool down) {
        const auto index=static_cast<std::size_t>(key);
        if(index>=received_.size()||received_[index]==down) {return;}
        received_[index]=down;pending_.push_back({index,down});
    }
    void poll() {
        pressed_.fill(false);std::array<bool,scanCount> touched{};
        while(!pending_.empty()) {
            const auto edge=pending_.front();
            if(touched[edge.key]) {break;}
            touched[edge.key]=true;held_[edge.key]=edge.down;pressed_[edge.key]=edge.down;pending_.pop_front();
        }
    }
    bool held(SDL_Scancode key) const {return held_.at(static_cast<std::size_t>(key));}
    bool pressed(SDL_Scancode key) const {return pressed_.at(static_cast<std::size_t>(key));}
    bool received(SDL_Scancode key) const {return received_.at(static_cast<std::size_t>(key));}
    void clear() {received_.fill(false);held_.fill(false);pressed_.fill(false);pending_.clear();}
private:
    struct Edge {std::size_t key;bool down;};
    std::array<bool,scanCount> received_{},held_{},pressed_{};
    std::deque<Edge> pending_;
};

inline std::uint8_t biosScan(SDL_Scancode key) {
    switch(key) {
    case SDL_SCANCODE_ESCAPE:return 0x01;
    case SDL_SCANCODE_1:return 0x02;case SDL_SCANCODE_2:return 0x03;case SDL_SCANCODE_3:return 0x04;
    case SDL_SCANCODE_4:return 0x05;case SDL_SCANCODE_5:return 0x06;case SDL_SCANCODE_6:return 0x07;
    case SDL_SCANCODE_7:return 0x08;case SDL_SCANCODE_8:return 0x09;case SDL_SCANCODE_9:return 0x0a;
    case SDL_SCANCODE_0:return 0x0b;case SDL_SCANCODE_MINUS:return 0x0c;case SDL_SCANCODE_EQUALS:return 0x0d;
    case SDL_SCANCODE_BACKSPACE:return 0x0e;case SDL_SCANCODE_TAB:return 0x0f;
    case SDL_SCANCODE_Q:return 0x10;case SDL_SCANCODE_W:return 0x11;case SDL_SCANCODE_E:return 0x12;
    case SDL_SCANCODE_R:return 0x13;case SDL_SCANCODE_T:return 0x14;case SDL_SCANCODE_Y:return 0x15;
    case SDL_SCANCODE_U:return 0x16;case SDL_SCANCODE_I:return 0x17;case SDL_SCANCODE_O:return 0x18;
    case SDL_SCANCODE_P:return 0x19;case SDL_SCANCODE_LEFTBRACKET:return 0x1a;case SDL_SCANCODE_RIGHTBRACKET:return 0x1b;
    case SDL_SCANCODE_RETURN:case SDL_SCANCODE_KP_ENTER:return 0x1c;
    case SDL_SCANCODE_A:return 0x1e;case SDL_SCANCODE_S:return 0x1f;case SDL_SCANCODE_D:return 0x20;
    case SDL_SCANCODE_F:return 0x21;case SDL_SCANCODE_G:return 0x22;case SDL_SCANCODE_H:return 0x23;
    case SDL_SCANCODE_J:return 0x24;case SDL_SCANCODE_K:return 0x25;case SDL_SCANCODE_L:return 0x26;
    case SDL_SCANCODE_SEMICOLON:return 0x27;case SDL_SCANCODE_APOSTROPHE:return 0x28;
    case SDL_SCANCODE_GRAVE:return 0x29;case SDL_SCANCODE_BACKSLASH:return 0x2b;
    case SDL_SCANCODE_Z:return 0x2c;case SDL_SCANCODE_X:return 0x2d;case SDL_SCANCODE_C:return 0x2e;
    case SDL_SCANCODE_V:return 0x2f;case SDL_SCANCODE_B:return 0x30;case SDL_SCANCODE_N:return 0x31;
    case SDL_SCANCODE_M:return 0x32;case SDL_SCANCODE_COMMA:return 0x33;case SDL_SCANCODE_PERIOD:return 0x34;
    case SDL_SCANCODE_SLASH:return 0x35;case SDL_SCANCODE_SPACE:return 0x39;
    case SDL_SCANCODE_F1:return 0x3b;case SDL_SCANCODE_F2:return 0x3c;case SDL_SCANCODE_F3:return 0x3d;
    case SDL_SCANCODE_F4:return 0x3e;case SDL_SCANCODE_F5:return 0x3f;case SDL_SCANCODE_F6:return 0x40;
    case SDL_SCANCODE_F7:return 0x41;case SDL_SCANCODE_F8:return 0x42;case SDL_SCANCODE_F9:return 0x43;
    case SDL_SCANCODE_F10:return 0x44;
    case SDL_SCANCODE_HOME:case SDL_SCANCODE_KP_7:return 0x47;
    case SDL_SCANCODE_UP:case SDL_SCANCODE_KP_8:return 0x48;
    case SDL_SCANCODE_PAGEUP:case SDL_SCANCODE_KP_9:return 0x49;
    case SDL_SCANCODE_LEFT:case SDL_SCANCODE_KP_4:return 0x4b;
    case SDL_SCANCODE_RIGHT:case SDL_SCANCODE_KP_6:return 0x4d;
    case SDL_SCANCODE_END:case SDL_SCANCODE_KP_1:return 0x4f;
    case SDL_SCANCODE_DOWN:case SDL_SCANCODE_KP_2:return 0x50;
    case SDL_SCANCODE_PAGEDOWN:case SDL_SCANCODE_KP_3:return 0x51;
    case SDL_SCANCODE_INSERT:return 0x52;case SDL_SCANCODE_DELETE:return 0x53;
    default:return 0;
    }
}

class SourceKeys {
public:
    void keyDown(SDL_Scancode key) {
        const auto scan=biosScan(key);
        if(scan==0||pending_.size()>=capacity) {return;}
        std::uint8_t character=0;
        switch(key) {
        case SDL_SCANCODE_ESCAPE:character=27;break;
        case SDL_SCANCODE_RETURN:case SDL_SCANCODE_KP_ENTER:character=13;break;
        case SDL_SCANCODE_TAB:character=9;break;
        case SDL_SCANCODE_BACKSPACE:character=8;break;
        case SDL_SCANCODE_SPACE:character=32;break;
        default:break;
        }
        const bool printable=(key>=SDL_SCANCODE_A&&key<=SDL_SCANCODE_0)
            ||(key>=SDL_SCANCODE_MINUS&&key<=SDL_SCANCODE_EQUALS)
            ||(key>=SDL_SCANCODE_LEFTBRACKET&&key<=SDL_SCANCODE_BACKSLASH)
            ||(key>=SDL_SCANCODE_SEMICOLON&&key<=SDL_SCANCODE_SLASH);
        for(auto& previous:pending_) {previous.awaiting=false;}
        pending_.push_back({static_cast<std::uint16_t>((scan<<8)|character),character==0&&printable});
    }
    void text(const char* utf8) {
        for(const auto* character=reinterpret_cast<const unsigned char*>(utf8);*character;++character) {
            const auto next=std::find_if(pending_.begin(),pending_.end(),[](const Item& item) {return item.awaiting;});
            if(next==pending_.end()) {return;}
            if(*character<=127) {next->word|=*character;}
            next->awaiting=false;
        }
    }
    std::uint16_t front() const {return pending_.empty()?0:pending_.front().word;}
    void pop() {if(!pending_.empty()) {pending_.pop_front();}}
    void clear() {pending_.clear();}
private:
    static constexpr std::size_t capacity=15;
    struct Item {std::uint16_t word;bool awaiting;};
    std::deque<Item> pending_;
};

class Joystick {
public:
    ~Joystick() {if(device_) {closeJoystick(device_);}}
    void event(const SDL_Event& event) {
        if(!device_||event.jbutton.which!=joystickId(device_)||event.jbutton.button>=buttons_.size()) {return;}
        if(kind(event)==EventKind::joystickDown||kind(event)==EventKind::joystickUp) {
            buttons_[event.jbutton.button].transition(kind(event)==EventKind::joystickDown);
        }
    }
    NativeJoystickSample poll(bool focused) {
        if(!focused) {armed_=false;for(auto& button:buttons_) {button.clear();}return cancellation();}
        if(device_&&!joystickConnected(device_)) {
            closeJoystick(device_);device_=nullptr;armed_=false;
            for(auto& button:buttons_) {button.clear();}return cancellation();
        }
        if(!device_) {
            device_=openGameplayJoystick();
        }
        if(!device_) {return {};}
        const auto axis=[&](int index) {return static_cast<std::uint16_t>(static_cast<int>(joystickAxis(device_,index))+32768);};
        std::uint8_t buttons=0;
        const auto count=joystickButtons(device_);
        for(std::size_t index=0;index<buttons_.size();++index) {
            buttons_[index].transition(static_cast<int>(index)<count&&joystickButton(device_,static_cast<int>(index)));
            buttons_[index].poll();
            if(buttons_[index].held()) {buttons|=static_cast<std::uint8_t>(1U<<index);}
        }
        if(!armed_) {if(buttons==0) {armed_=true;}buttons=0;}
        return {true,axis(0),axis(1),buttons};
    }
private:
    static NativeJoystickSample cancellation() {NativeJoystickSample result;result.phase=NativeJoystickPhase::cancel;return result;}
    SDL_Joystick* device_=nullptr;
    std::array<Pulse,2> buttons_;
    bool armed_=false;
};
} //namespace stellar7::sdlHost
