#pragma once

#include <windows.h>
#include <mmsystem.h>

#include "stellar7/native_joystick.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace stellar7 {

struct Win32JoystickCapabilities {
    UINT device=0;
    UINT xMinimum=0,xMaximum=0,yMinimum=0,yMaximum=0;
    UINT axes=0,buttons=0;
};

struct Win32JoystickMetrics {
    std::uint64_t polls=0,capabilityQueries=0,positionQueries=0,connectionLosses=0;
};

//No capture, thread, timer, source calibration or game-owner mutation.
class Win32Joystick {
public:
    NativeJoystickSample poll() {
        ++metrics_.polls;
        if(capabilities_) {
            if(!connected_) {
                const auto refreshed=capabilities(capabilities_->device);
                if(!refreshed) {return {};}
                capabilities_=refreshed;
            }
            const auto sample=position(*capabilities_);
            if(!sample.connected) {
                if(connected_) {++metrics_.connectionLosses;}
                connected_=false;buttonsArmed_=false;
                return {};
            }
            connected_=true;
            return gateButtons(sample);
        }
        const auto count=std::min(joyGetNumDevs(),maximumDevices);
        for(UINT device=0;device<count;++device) {
            const auto candidate=capabilities(device);
            if(!candidate) {continue;}
            const auto sample=position(*candidate);
            if(!sample.connected) {continue;}
            capabilities_=candidate;connected_=true;
            return gateButtons(sample);
        }
        return {};
    }

    void suspend() noexcept {connected_=false;buttonsArmed_=false;}
    bool connected() const noexcept {return connected_;}
    const std::optional<Win32JoystickCapabilities>& capabilities() const noexcept {return capabilities_;}
    const Win32JoystickMetrics& metrics() const noexcept {return metrics_;}

private:
    static constexpr UINT maximumDevices=16;
    static constexpr std::uint64_t hostMaximum=65535;
    static constexpr DWORD requestedPosition=JOY_RETURNX|JOY_RETURNY|JOY_RETURNBUTTONS;
    static constexpr DWORD supportedButtons=JOY_BUTTON1|JOY_BUTTON2;

    std::optional<Win32JoystickCapabilities> capabilities(const UINT device) {
        JOYCAPSW caps{};++metrics_.capabilityQueries;
        if(joyGetDevCapsW(device,&caps,sizeof(caps))!=JOYERR_NOERROR
            ||caps.wNumAxes<2||caps.wXmin>=caps.wXmax||caps.wYmin>=caps.wYmax) {return {};}
        return Win32JoystickCapabilities{device,caps.wXmin,caps.wXmax,caps.wYmin,caps.wYmax,caps.wNumAxes,caps.wNumButtons};
    }

    static std::uint16_t axis(const DWORD value,const UINT minimum,const UINT maximum) {
        const auto bounded=std::clamp(value,static_cast<DWORD>(minimum),static_cast<DWORD>(maximum));
        return static_cast<std::uint16_t>(static_cast<std::uint64_t>(bounded-minimum)*hostMaximum/(maximum-minimum));
    }

    NativeJoystickSample position(const Win32JoystickCapabilities& caps) {
        JOYINFOEX info{};info.dwSize=sizeof(info);info.dwFlags=requestedPosition;
        ++metrics_.positionQueries;
        if(joyGetPosEx(caps.device,&info)!=JOYERR_NOERROR) {return {};}
        return {true,axis(info.dwXpos,caps.xMinimum,caps.xMaximum),axis(info.dwYpos,caps.yMinimum,caps.yMaximum),
            static_cast<std::uint8_t>(info.dwButtons&supportedButtons)};
    }

    NativeJoystickSample gateButtons(NativeJoystickSample sample) {
        if(!buttonsArmed_) {
            if(sample.buttons==0) {buttonsArmed_=true;}
            sample.buttons=0;
        }
        return sample;
    }

    std::optional<Win32JoystickCapabilities> capabilities_;
    Win32JoystickMetrics metrics_{};
    bool connected_=false;
    bool buttonsArmed_=true;
};

} //namespace stellar7
