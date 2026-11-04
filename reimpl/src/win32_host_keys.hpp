#pragma once

#include <windows.h>
#include <array>
#include <deque>

namespace stellar7 {

// Retain short make/break pairs across audio backpressure. Repeats do not add edges.
class Win32HostKeys {
public:
    void transition(const UINT message,const WPARAM key) {
        if(key>=keyCount) {return;}
        const bool down=message==WM_KEYDOWN||message==WM_SYSKEYDOWN;
        if(!down&&message!=WM_KEYUP&&message!=WM_SYSKEYUP) {return;}
        if(received_[key]==down) {return;}
        received_[key]=down;pending_.push_back({static_cast<unsigned>(key),down});
    }
    void poll() {
        pressed_.fill(false);
        std::array<bool,keyCount> touched{};
        while(!pending_.empty()) {
            const auto next=pending_.front();
            if(touched[next.key]) {break;}
            touched[next.key]=true;held_[next.key]=next.down;pressed_[next.key]=next.down;
            pending_.pop_front();
        }
    }
    bool held(const unsigned key) const {return held_.at(key);}
    bool pressed(const unsigned key) const {return pressed_.at(key);}
    void clear() {
        received_.fill(false);held_.fill(false);pressed_.fill(false);pending_.clear();
    }
private:
    static constexpr unsigned keyCount=256;
    struct Edge {unsigned key;bool down;};
    std::array<bool,keyCount> received_{},held_{},pressed_{};
    std::deque<Edge> pending_;
};
} // namespace stellar7
