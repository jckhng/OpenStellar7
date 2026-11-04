#pragma once
#include <SDL.h>
#include "stellar7/audio.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef STELLAR7_ENHANCED
#include "enhanced_diagnostics.hpp"
#endif

namespace stellar7::sdlHost {
class Audio {
public:
    static constexpr std::uint64_t inputLead=2560;
    static constexpr std::uint64_t maximumQueued=4096;
    static constexpr std::size_t packet=512;
    Audio() {
#ifdef STELLAR7_ENHANCED
        const EnhancedHostOperation operation("SDL_OpenAudioDevice");
#endif
        SDL_AudioSpec desired{},obtained{};
        desired.freq=SoundBlasterSynth::sampleRate;desired.format=AUDIO_S16SYS;desired.channels=1;
        desired.samples=packet;desired.callback=callback;desired.userdata=this;
        // SDL converts to hardware format; all accounting remains in source samples.
        device_=SDL_OpenAudioDevice(nullptr,0,&desired,&obtained,0);
        if(device_==0) {throw std::runtime_error(SDL_GetError());}
        if(obtained.freq!=desired.freq||obtained.format!=desired.format||obtained.channels!=desired.channels) {
            SDL_CloseAudioDevice(device_);device_=0;
            throw std::runtime_error("SDL changed the fixed source audio format");
        }
    }
    ~Audio() {if(device_) {SDL_CloseAudioDevice(device_);}}
    Audio(const Audio&)=delete;
    Audio& operator=(const Audio&)=delete;
    void service(std::function<bool()> callback) {service_=std::move(callback);}
    std::uint64_t submitted() const {return submitted_.load(std::memory_order_acquire);}
    std::uint64_t consumed() const {checkCallback();return consumed_.load(std::memory_order_acquire);}
    std::uint64_t presentationEstimate() const {
        checkCallback();return std::min(submitted(),estimated_.load(std::memory_order_acquire));
    }
#ifdef STELLAR7_ENHANCED
    void setVolume(const unsigned percent) {
        if(percent>100) {throw std::invalid_argument("Playback volume must be0..100");}
        volume_.store(percent,std::memory_order_relaxed);
    }
    void suspend() {
        if(!suspended_) {SDL_PauseAudioDevice(device_,1);suspended_=true;}
    }
    void resume() {
        if(!suspended_) {return;}
        suspended_=false;++pauseGeneration_;
        if(started_) {SDL_PauseAudioDevice(device_,0);}
    }
#endif
    bool waitForInput() {return waitUntil([&] {return submitted()-consumed()<=inputLead;});}
    bool submit(const std::vector<std::int16_t>& samples) {
        for(std::size_t offset=0;offset<samples.size();) {
            const auto count=std::min(packet,samples.size()-offset);
            if(!waitUntil([&] {return submitted()-consumed()+count<=maximumQueued;})) {return false;}
            SDL_LockAudioDevice(device_);
            const auto tail=(head_+queued_)%queue_.size();
            const auto first=std::min(count,queue_.size()-tail);
            std::copy_n(samples.data()+offset,first,queue_.data()+tail);
            std::copy_n(samples.data()+offset+first,count-first,queue_.data());
            queued_+=count;submitted_.fetch_add(count,std::memory_order_release);
            SDL_UnlockAudioDevice(device_);
            offset+=count;
            if(!started_&&submitted()>=packet) {SDL_PauseAudioDevice(device_,0);started_=true;}
            if(service_&&!service_()) {return false;}
        }
        return true;
    }
    bool drain() {
        if(!started_&&submitted()!=0) {SDL_PauseAudioDevice(device_,0);started_=true;}
        return waitUntil([&] {return consumed()==submitted()&&presentationEstimate()==submitted();});
    }
private:
    void checkCallback() const {
        if(callbackFailed_.load(std::memory_order_acquire)) {throw std::runtime_error("invalid SDL source callback buffer");}
    }
    template<class Done> bool waitUntil(Done done) {
#ifdef STELLAR7_ENHANCED
        const EnhancedHostOperation operation("audio source wait");
#endif
        auto progress=SDL_GetPerformanceCounter();auto previous=consumed();
#ifdef STELLAR7_ENHANCED
        auto pauseGeneration=pauseGeneration_;
#endif
        const auto stallTicks=stallSeconds*SDL_GetPerformanceFrequency();
        while(!done()) {
            if(service_&&!service_()) {return false;}
#ifdef STELLAR7_ENHANCED
            //A deliberate options pause is not a failed device-progress deadline.
            if(pauseGeneration!=pauseGeneration_) {pauseGeneration=pauseGeneration_;progress=SDL_GetPerformanceCounter();}
#endif
            const auto now=consumed();
            if(now!=previous) {previous=now;progress=SDL_GetPerformanceCounter();}
            if(SDL_GetPerformanceCounter()-progress>stallTicks) {throw std::runtime_error("SDL audio source queue stalled");}
            SDL_Delay(1);
        }
        return !service_||service_();
    }
    static void SDLCALL callback(void* userdata,Uint8* output,int bytes) noexcept {
        auto& owner=*static_cast<Audio*>(userdata);
        if(bytes<0||bytes%sizeof(std::int16_t)!=0) {owner.callbackFailed_=true;return;}
        if(bytes==0) {return;}
        std::memset(output,0,static_cast<std::size_t>(bytes));
        // SDL holds the device lock. Retire the preceding feed, including silence;
        // this is a presentation estimate, not a hardware playback cursor.
        owner.estimated_.store(owner.consumed_.load(std::memory_order_acquire),std::memory_order_release);
        const auto count=std::min(owner.queued_,static_cast<std::size_t>(bytes)/sizeof(std::int16_t));
        const auto first=std::min(count,owner.queue_.size()-owner.head_);
        std::memcpy(output,owner.queue_.data()+owner.head_,first*sizeof(std::int16_t));
        std::memcpy(output+first*sizeof(std::int16_t),owner.queue_.data(),(count-first)*sizeof(std::int16_t));
#ifdef STELLAR7_ENHANCED
        const auto volume=owner.volume_.load(std::memory_order_relaxed);
        if(volume!=100) {
            for(std::size_t index=0;index<count;++index) {
                std::int16_t sample=0;std::memcpy(&sample,output+index*sizeof(sample),sizeof(sample));
                sample=static_cast<std::int16_t>(static_cast<std::int32_t>(sample)*static_cast<int>(volume)/100);
                std::memcpy(output+index*sizeof(sample),&sample,sizeof(sample));
            }
        }
#endif
        owner.head_=(owner.head_+count)%owner.queue_.size();owner.queued_-=count;
        owner.consumed_.fetch_add(count,std::memory_order_release);
    }
    static constexpr std::uint64_t stallSeconds=30;
    SDL_AudioDeviceID device_=0;
    std::array<std::int16_t,maximumQueued> queue_{};
    std::size_t head_=0,queued_=0;
    std::atomic<std::uint64_t> submitted_{0},consumed_{0},estimated_{0};
    std::atomic<bool> callbackFailed_{false};
    std::function<bool()> service_;
    bool started_=false;
#ifdef STELLAR7_ENHANCED
    bool suspended_=false;
    std::atomic<unsigned> volume_{100};
    std::uint64_t pauseGeneration_=0;
#endif
};
} //namespace stellar7::sdlHost
