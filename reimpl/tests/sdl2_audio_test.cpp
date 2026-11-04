#define SDL_MAIN_HANDLED 1
#include <SDL.h>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>

namespace {
SDL_AudioSpec opened{};
std::vector<std::int16_t> played;
std::uint64_t ticks=0;
bool resumed=false,closed=false,consume=true,wrongFormat=false;
unsigned locks=0;
SDL_AudioDeviceID fakeOpen(const char* name,int capture,const SDL_AudioSpec* desired,
    SDL_AudioSpec* obtained,int changes) {
    assert(!name&&capture==0&&changes==0);
    assert(desired->freq==44100&&desired->format==AUDIO_S16SYS&&desired->channels==1);
    assert(desired->samples==512&&desired->callback&&desired->userdata);
    opened=*desired;*obtained=*desired;
    if(wrongFormat) {obtained->freq=48000;}
    return 2;
}
void fakeClose(SDL_AudioDeviceID device) {assert(device==2);closed=true;}
void fakePause(SDL_AudioDeviceID device,int paused) {assert(device==2);resumed=paused==0;}
void fakeLock(SDL_AudioDeviceID device) {assert(device==2);++locks;}
void fakeUnlock(SDL_AudioDeviceID device) {assert(device==2&&locks);--locks;}
Uint64 fakeCounter() {return ticks;}
Uint64 fakeFrequency() {return 1000;}
void feed(int frames) {
    std::vector<std::int16_t> output(static_cast<std::size_t>(frames),-123);
    opened.callback(opened.userdata,reinterpret_cast<Uint8*>(output.data()),frames*sizeof(std::int16_t));
    played.insert(played.end(),output.begin(),output.end());
}
void fakeDelay(Uint32 milliseconds) {
    ticks+=milliseconds;
    if(resumed&&consume) {feed(512);}
}
}

#define SDL_OpenAudioDevice fakeOpen
#define SDL_CloseAudioDevice fakeClose
#define SDL_PauseAudioDevice fakePause
#define SDL_LockAudioDevice fakeLock
#define SDL_UnlockAudioDevice fakeUnlock
#define SDL_GetPerformanceCounter fakeCounter
#define SDL_GetPerformanceFrequency fakeFrequency
#define SDL_Delay fakeDelay
#include "sdl2_audio.hpp"

int main() {
    using stellar7::sdlHost::Audio;
    {
        Audio audio;
        std::vector<std::int16_t> source(8193);
        for(std::size_t i=0;i<source.size();++i) {source[i]=static_cast<std::int16_t>(i+1);}
        unsigned serviced=0;
        audio.service([&] {
            ++serviced;
            assert(audio.submitted()-audio.consumed()<=Audio::maximumQueued);
            assert(audio.presentationEstimate()<=audio.consumed());
            return true;
        });
        assert(audio.submit(source));assert(audio.submitted()==source.size());
        assert(audio.waitForInput()&&serviced);
        assert(audio.submitted()-audio.consumed()<=Audio::inputLead);
        assert(audio.drain());
        assert(audio.consumed()==source.size()&&audio.presentationEstimate()==source.size());
        assert(played.size()>=source.size()&&std::equal(source.begin(),source.end(),played.begin()));
        assert(std::all_of(played.begin()+source.size(),played.end(),[](auto value) {return value==0;}));
        const auto end=audio.consumed();feed(512);feed(512);
        assert(audio.consumed()==end&&audio.presentationEstimate()==end);
    }
    assert(closed&&locks==0);
    played.clear();closed=false;resumed=false;
    {
        Audio audio;
        assert(audio.submit(std::vector<std::int16_t>(4096,7)));
        audio.service([] {return false;});assert(!audio.waitForInput());
        audio.service({});assert(audio.drain());
    }
    // Partial callbacks/ring wrap retain PCM order; silence cannot advance source time.
    played.clear();resumed=false;
    {
        Audio audio;
        assert(audio.submit({1,2,3}));feed(2);
        assert(audio.consumed()==2&&audio.presentationEstimate()==0);
        feed(4);assert(audio.consumed()==3&&audio.presentationEstimate()==2);
        feed(1);assert(audio.presentationEstimate()==3);
        assert((played==std::vector<std::int16_t>{1,2,3,0,0,0,0}));
    }
    consume=false;resumed=false;
    {
        Audio audio;
        assert(audio.submit(std::vector<std::int16_t>(4096,1)));
        bool rejected=false;
        try {audio.waitForInput();} catch(const std::runtime_error&) {rejected=true;}
        assert(rejected);
    }
    wrongFormat=true;closed=false;
    bool rejected=false;
    try {Audio audio;} catch(const std::runtime_error&) {rejected=true;}
    assert(rejected&&closed);
}
