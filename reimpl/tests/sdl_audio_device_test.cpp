#include "sdl_host.hpp"
#include "sdl_audio.hpp"
#include <cassert>

int main() {
    using namespace stellar7::sdlHost;
    SDL_SetMainReady();check(SDL_Init(SDL_INIT_AUDIO),"SDL_Init");
    {
        Audio audio;
        assert(audio.drain());
        unsigned serviced=0;audio.service([&] {++serviced;return true;});
        const std::vector<std::int16_t> source(8193,123);
        assert(audio.submit(source)&&audio.waitForInput()&&audio.drain());
        assert(serviced!=0&&audio.submitted()==source.size()&&audio.consumed()==source.size());
        assert(audio.presentationEstimate()==source.size());
    }
    {
        Audio audio;
        assert(audio.submit({1,2,3})&&audio.drain());
        assert(audio.submitted()==3&&audio.consumed()==3&&audio.presentationEstimate()==3);
    }
    SDL_Quit();
}
