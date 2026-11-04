#pragma once

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <deque>

namespace stellar7 {

//Native attract-only FIFO of physical scan/character words. This does not
//emulate IRQ1/BDA state or full BIOS translation for arbitrary layouts.
class Win32SourceKeys {
public:
    void keyDown(const UINT message,const WPARAM key,const LPARAM data) {
        if((message!=WM_KEYDOWN&&message!=WM_SYSKEYDOWN)||modifier(key)) {return;}
        const auto scan=static_cast<std::uint8_t>((static_cast<std::uintptr_t>(data)>>scanShift)&scanMask);
        if(scan==0) {return;}
        const auto character=controlCharacter(key);
        const auto count=repeats(data);
        for(unsigned index=0;index<count&&pending_.size()<capacity;++index) {
            pending_.push_back({static_cast<std::uint16_t>((scan<<8)|character),character==0});
        }
    }

    void character(const WPARAM character,const LPARAM data) {
        const auto scan=static_cast<std::uint8_t>((static_cast<std::uintptr_t>(data)>>scanShift)&scanMask);
        const auto count=repeats(data);
        for(unsigned index=0;index<count;++index) {
            const auto found=std::find_if(pending_.begin(),pending_.end(),[scan](const auto& item) {
                return item.awaitingCharacter&&(item.word>>8)==scan;
            });
            if(found==pending_.end()) {return;}
            //Only actual ASCII WM_CHAR bytes enter AL here; no guessed Escape/Space
            //word is manufactured from a held boolean or an arbitrary key.
            if(character<=asciiMaximum) {found->word|=static_cast<std::uint16_t>(character);}
            found->awaitingCharacter=false;
        }
    }

    std::uint16_t front() const noexcept {return pending_.empty()?0:pending_.front().word;}
    void pop() noexcept {if(!pending_.empty()) {pending_.pop_front();}}
    void clear() noexcept {pending_.clear();}
    std::size_t size() const noexcept {return pending_.size();}

private:
    static constexpr unsigned scanShift=16;
    static constexpr unsigned scanMask=0xff;
    static constexpr unsigned asciiMaximum=127;
    static constexpr std::size_t capacity=15;
    static constexpr std::uint8_t escapeCharacter=27,enterCharacter=13,tabCharacter=9,
        backspaceCharacter=8,spaceCharacter=32;
    struct Key {std::uint16_t word;bool awaitingCharacter;};

    static unsigned repeats(const LPARAM data) noexcept {
        return std::max(1U,static_cast<unsigned>(static_cast<std::uintptr_t>(data)&0xffff));
    }

    static bool modifier(const WPARAM key) noexcept {
        switch(key) {
        case VK_SHIFT:case VK_LSHIFT:case VK_RSHIFT:
        case VK_CONTROL:case VK_LCONTROL:case VK_RCONTROL:
        case VK_MENU:case VK_LMENU:case VK_RMENU:
        case VK_CAPITAL:case VK_NUMLOCK:case VK_SCROLL:return true;
        default:return false;
        }
    }

    static std::uint8_t controlCharacter(const WPARAM key) noexcept {
        switch(key) {
        case VK_ESCAPE:return escapeCharacter;
        case VK_RETURN:return enterCharacter;
        case VK_TAB:return tabCharacter;
        case VK_BACK:return backspaceCharacter;
        case VK_SPACE:return spaceCharacter;
        default:return 0;
        }
    }

    std::deque<Key> pending_;
};

} //namespace stellar7
