#pragma once
#include <bitset>
#include <cstdint>
#include <deque>
#include <mutex>

namespace stellar7 {
enum class ControllerButton {left,right,forward,reverse,fire,activate,cycle,jump,shield,menu,confirm,back,options,view,recenter,count};
inline constexpr std::size_t controllerButtonCount=static_cast<std::size_t>(ControllerButton::count);
struct ControllerInput {
    bool connected=false;
    std::bitset<controllerButtonCount> buttons;
    bool operator==(const ControllerInput& other) const {return connected==other.connected&&buttons==other.buttons;}
    bool down(ControllerButton button) const {return buttons.test(static_cast<std::size_t>(button));}
    void set(ControllerButton button,bool down) {buttons.set(static_cast<std::size_t>(button),down);}
};
class ControllerState {
public:
    void cancel() {armed_=false;held_.reset();pressed_.reset();}
    void poll(const ControllerInput& input,bool focused) {
        pressed_.reset();
        if(!focused||!input.connected) {cancel();return;}
        if(!armed_) {
            if(input.buttons.none()) {armed_=true;}
            held_.reset();return;
        }
        pressed_=input.buttons&~held_;held_=input.buttons;
    }
    bool held(ControllerButton button) const {return held_.test(static_cast<std::size_t>(button));}
    bool pressed(ControllerButton button) const {return pressed_.test(static_cast<std::size_t>(button));}
private:
    bool armed_=false;
    std::bitset<controllerButtonCount> held_,pressed_;
};
// XR producer and main-thread consumer never share session/API ownership.
// Retain short taps, bound backlog, and fail closed on focus loss or stale input.
class ControllerMailbox {
public:
    void publish(const ControllerInput& input,std::uint64_t now) {
        const std::lock_guard<std::mutex> lock(mutex_);updated_=now;
        if(!input.connected) {pending_.clear();latest_=consumed_={};cancelPending_=true;return;}
        if(input==latest_) {return;}
        if(pending_.size()>=maximumPending) {pending_.clear();cancelPending_=true;}
        pending_.push_back(input);latest_=input;
    }
    ControllerInput take(std::uint64_t now) {
        const std::lock_guard<std::mutex> lock(mutex_);
        if(now-updated_>staleMilliseconds) {pending_.clear();latest_=consumed_={};cancelPending_=false;return {};}
        if(cancelPending_) {cancelPending_=false;return {};}
        if(!pending_.empty()) {consumed_=pending_.front();pending_.pop_front();}
        return consumed_;
    }
private:
    static constexpr std::size_t maximumPending=16;
    static constexpr std::uint64_t staleMilliseconds=250;
    std::mutex mutex_;
    std::deque<ControllerInput> pending_;
    ControllerInput latest_,consumed_;
    std::uint64_t updated_=0;
    bool cancelPending_=false;
};
}
