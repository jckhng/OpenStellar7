#include "sdl_input.hpp"
#include <cassert>

int main() {
    using namespace stellar7::sdlHost;
    Keys keys;
    keys.transition(SDL_SCANCODE_RETURN,true);keys.transition(SDL_SCANCODE_RETURN,false);
    keys.poll();assert(keys.held(SDL_SCANCODE_RETURN)&&keys.pressed(SDL_SCANCODE_RETURN));
    keys.poll();assert(!keys.held(SDL_SCANCODE_RETURN)&&!keys.pressed(SDL_SCANCODE_RETURN));
    keys.transition(SDL_SCANCODE_UP,true);keys.transition(SDL_SCANCODE_LEFT,true);
    keys.transition(SDL_SCANCODE_UP,true);keys.poll();
    assert(keys.held(SDL_SCANCODE_UP)&&keys.held(SDL_SCANCODE_LEFT));
    keys.poll();assert(!keys.pressed(SDL_SCANCODE_UP));
    keys.transition(SDL_SCANCODE_RETURN,true);keys.clear();keys.poll();
    assert(!keys.held(SDL_SCANCODE_RETURN)&&!keys.held(SDL_SCANCODE_UP));

    Pulse button;button.transition(true);button.transition(false);button.poll();assert(button.held());
    button.poll();assert(!button.held());button.transition(true);button.clear();button.poll();assert(!button.held());

    SourceKeys source;
    source.keyDown(SDL_SCANCODE_UP);source.keyDown(SDL_SCANCODE_A);source.text("A");
    assert(source.front()==0x4800);source.pop();assert(source.front()==0x1e41);source.pop();assert(source.front()==0);
    source.keyDown(SDL_SCANCODE_ESCAPE);assert(source.front()==0x011b);source.clear();
    source.keyDown(SDL_SCANCODE_LSHIFT);assert(source.front()==0);
    source.keyDown(SDL_SCANCODE_RETURN);assert(source.front()==0x1c0d);source.clear();
    for(int index=0;index<20;++index) {source.keyDown(SDL_SCANCODE_SPACE);}
    for(int index=0;index<15;++index) {assert(source.front()==0x3920);source.pop();}
    assert(source.front()==0);
    source.keyDown(SDL_SCANCODE_A);source.text("\xc3\xa9");assert(source.front()==0x1e00);
}
