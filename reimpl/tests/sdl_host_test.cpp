#include "sdl_host.hpp"
#include <cassert>
#include <cstdint>
#include <memory>
#include <iostream>
#include <vector>

namespace {
std::vector<std::uint32_t> readPixels(SDL_Renderer* renderer,int width,int height) {
    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(width)*height);
    stellar7::sdlHost::check(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_ARGB8888,pixels.data(),width*4),"SDL_RenderReadPixels");
    return pixels;
}
}

int main() {
    using namespace stellar7::sdlHost;
    SDL_SetMainReady();check(SDL_Init(SDL_INIT_VIDEO),"SDL_Init");
    {
        const std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window(createWindow(640,500),SDL_DestroyWindow);
        assert(window);
        const std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)> renderer(createRenderer(window.get()),SDL_DestroyRenderer);
        assert(renderer);configureRenderer(renderer.get());
        const std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> texture(
            SDL_CreateTexture(renderer.get(),SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,sourceWidth,sourceHeight),SDL_DestroyTexture);
        assert(texture);nearestTexture(texture.get());
        constexpr std::uint32_t left=0xff123456,right=0xffabcdef,black=0xff000000;
        std::vector<std::uint32_t> source(sourceWidth*sourceHeight);
        for(int y=0;y<sourceHeight;++y) {
            for(int x=0;x<sourceWidth;++x) {source[y*sourceWidth+x]=x<sourceWidth/2?left:right;}
        }
        check(SDL_UpdateTexture(texture.get(),nullptr,source.data(),sourceWidth*4),"SDL_UpdateTexture");
        check(SDL_SetRenderDrawColor(renderer.get(),0,0,0,255),"SDL_SetRenderDrawColor");
        check(SDL_RenderClear(renderer.get()),"SDL_RenderClear");renderTexture(renderer.get(),texture.get());
        int readHeight=500,contentTop=50;
        const auto output=readPixels(renderer.get(),640,readHeight);
        for(int y=0;y<readHeight;++y) {
            for(int x=0;x<640;++x) {
                const auto expected=y<contentTop||y>=contentTop+400?black:x<320?left:right;
                if(output[y*640+x]!=expected) {
                    std::cerr<<"first pixel mismatch x="<<x<<" y="<<y<<" actual="<<std::hex
                        <<output[y*640+x]<<" expected="<<expected<<'\n';
                }
                assert(output[y*640+x]==expected);
            }
        }
        // Fractional nearest sampling differs between SDL software releases.
        // Assert our viewport/colour contract without prescribing SDL's tie rounding.
        SDL_SetWindowSize(window.get(),800,600);SDL_PumpEvents();
        check(SDL_RenderClear(renderer.get()),"SDL_RenderClear");renderTexture(renderer.get(),texture.get());
        readHeight=600;contentTop=50;
        const auto fractional=readPixels(renderer.get(),800,readHeight);
        for(int row=0;row<readHeight;++row) {
            for(int column=0;column<800;++column) {
                const auto pixel=fractional[row*800+column];
                assert(row<contentTop||row>=contentTop+500?pixel==black:pixel==left||pixel==right);
            }
        }
        assert(fractional[300*800+200]==left&&fractional[300*800+600]==right);
        SDL_Event event{};float x=0,y=0;
        event.type=SDL_MOUSEMOTION;
        event.motion.windowID=SDL_GetWindowID(window.get());event.motion.x=400;event.motion.y=300;
        pointerPosition(window.get(),renderer.get(),event,x,y);assert(x==160&&y==100);
        event.motion.y=20;pointerPosition(window.get(),renderer.get(),event,x,y);assert(y<0);
        warpPointer(window.get(),renderer.get(),160,100);
        event={};
        event.type=SDL_WINDOWEVENT;event.window.event=SDL_WINDOWEVENT_CLOSE;
        assert(kind(event)==EventKind::quit);
        event.window.event=SDL_WINDOWEVENT_MINIMIZED;
        assert(kind(event)==EventKind::focusLost);
        event={};event.type=SDL_KEYDOWN;event.key.keysym.scancode=SDL_SCANCODE_C;event.key.keysym.mod=KMOD_CTRL|KMOD_ALT;
        assert(kind(event)==EventKind::keyDown&&scan(event.key)==SDL_SCANCODE_C);
        assert((modifiers(event.key)&controlModifier)!=0&&(modifiers(event.key)&altModifier)!=0);
        startText(window.get());stopText(window.get());setTitle(window.get(),"Stellar7 host contract");presentRenderer(renderer.get());
    }
    SDL_Quit();
}
