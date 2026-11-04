#define SDL_MAIN_HANDLED
#include "enhanced_sdl.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
int windowWidth=1280,windowHeight=720,outputWidth=2560,outputHeight=1440;
int mouseX=0,mouseY=0;
Uint32 mouseButtons=0;
void require(bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
void fakeWindowSize(SDL_Window*,int* width,int* height) {*width=windowWidth;*height=windowHeight;}
int fakeOutputSize(SDL_Renderer*,int* width,int* height) {*width=outputWidth;*height=outputHeight;return 0;}
void fakeDrawableSize(SDL_Window*,int* width,int* height) {*width=outputWidth;*height=outputHeight;}
Uint32 fakeMouseState(int* x,int* y) {
    if(x) {*x=mouseX;}
    if(y) {*y=mouseY;}
    return mouseButtons;
}
void environment(const char* name,const char* value) {
#ifdef _WIN32
    require(_putenv_s(name,value)==0,"cannot set environment");
#else
    require((*value?setenv(name,value,1):unsetenv(name))==0,"cannot set environment");
#endif
}
}
#define SDL_GetWindowSize fakeWindowSize
#define SDL_GetRendererOutputSize fakeOutputSize
#define SDL_GetMouseState fakeMouseState
#define SDL_GL_GetDrawableSize fakeDrawableSize
#include "enhanced_sdl.cpp"

namespace {
SDL_Window* const window=reinterpret_cast<SDL_Window*>(1);
SDL_Renderer* const renderer=reinterpret_cast<SDL_Renderer*>(1);
void key(stellar7::EnhancedSdl& host,SDL_Scancode scan) {
    SDL_KeyboardEvent event{};event.keysym.scancode=scan;host.key(event,window);
}
void position(stellar7::EnhancedSdl& host,int x,int y) {
    if(outputWidth<=0||outputHeight<=0) {mouseX=0;mouseY=0;return;}
    const auto bounds=host.viewport(renderer);
    mouseX=static_cast<int>((bounds.x+static_cast<double>(x)*bounds.w/320)*windowWidth/outputWidth);
    mouseY=static_cast<int>((bounds.y+static_cast<double>(y)*bounds.h/200)*windowHeight/outputHeight);
}
bool click(stellar7::EnhancedSdl& host,int x,int y,Uint8 button=SDL_BUTTON_LEFT,bool focused=true) {
    position(host,x,y);
    SDL_Event event{};event.type=SDL_MOUSEBUTTONDOWN;event.button.button=button;
    event.button.x=mouseX;event.button.y=mouseY;
    return host.event(event,focused,window,renderer);
}
void wheel(stellar7::EnhancedSdl& host,int amount,Uint32 direction=SDL_MOUSEWHEEL_NORMAL) {
    SDL_Event event{};event.type=SDL_MOUSEWHEEL;event.wheel.y=amount;event.wheel.direction=direction;
    require(host.event(event,true,window,renderer),"panel wheel escaped to gameplay");
}
struct RemoveFile {
    std::filesystem::path path;
    ~RemoveFile() {std::error_code ignored;std::filesystem::remove(path,ignored);}
};
}
int main(int argc,char** argv) try {
    environment("STELLAR7_SAFE_MODE","0");environment("STELLAR7_HOST_CONTROLLERS","0");
    environment("STELLAR7_HOST_RENDERER","software");
    const stellar7::BitmapFont font{};
    stellar7::EnhancedSdl host({},font,25);
    require(!click(host,100,75),"closed panel stole gameplay mouse input");
    key(host,SDL_SCANCODE_F12);
    mouseButtons=SDL_BUTTON_LMASK;host.servicePanel(window,true);
    require(click(host,100,75),"open panel did not consume mouse input");
    require(host.options().filter==stellar7::EnhancedFilter::sharp,"held opening click changed options");
    mouseButtons=0;host.servicePanel(window,true);
    require(click(host,100,75),"panel click was not consumed");
    require(host.options().filter==stellar7::EnhancedFilter::smooth,"DPI/letterbox row click missed filter");
    // Window coordinates in the letterbox are not source-space controls.
    SDL_Event outside{};outside.type=SDL_MOUSEBUTTONDOWN;outside.button.button=SDL_BUTTON_LEFT;
    outside.button.x=5;outside.button.y=270;host.event(outside,true,window,renderer);
    require(host.options().filter==stellar7::EnhancedFilter::smooth,"letterbox click changed filter");
    require(click(host,100,75,SDL_BUTTON_RIGHT),"right click escaped panel");
    require(host.options().filter==stellar7::EnhancedFilter::sharp,"right click did not decrease/toggle");
    click(host,100,147);require(host.options().muted,"volume label click did not mute");
    click(host,282,147);require(host.options().volume==95&&!host.options().muted,"minus control did not adjust/unmute volume");
    click(host,306,147);require(host.options().volume==100,"plus control did not adjust volume");
    click(host,306,147);require(host.options().volume==100,"volume exceeded maximum");
    // Wheel selection changes rows, not values; flipped wheels reverse it.
    click(host,100,111);const auto originalRate=host.options().gameRate;
    position(host,100,111);wheel(host,-1);
    require(host.options().gameRate==originalRate,"wheel adjusted a value instead of selection");
    wheel(host,1);key(host,SDL_SCANCODE_RIGHT);
    require(host.options().gameRate!=originalRate,"wheel did not return to speed row");
    const auto selectedRate=host.options().gameRate;
    wheel(host,-1,SDL_MOUSEWHEEL_FLIPPED);key(host,SDL_SCANCODE_RIGHT);
    require(host.options().gameRate==selectedRate,"flipped wheel selected wrong row");
    // Page buttons reach camera settings and keyboard bindings without keyboard navigation.
    click(host,304,24);click(host,100,39);
    require(host.options().camera.distance==4900,"next page did not reach chase distance");
    click(host,100,111); // First key binding on page two: left.
    const auto beforeCapture=host.binding(stellar7::EnhancedAction::left);
    wheel(host,-11);click(host,100,87);
    key(host,SDL_SCANCODE_Q);
    require(host.binding(stellar7::EnhancedAction::left)==SDL_SCANCODE_Q,"mouse could not start key capture, or capture navigated");
    require(beforeCapture!=SDL_SCANCODE_Q,"capture control did not change binding");
    click(host,100,111,SDL_BUTTON_RIGHT);
    require(host.binding(stellar7::EnhancedAction::left)==SDL_SCANCODE_UNKNOWN,"right click did not clear binding");
    click(host,100,111);click(host,280,10);
    require(host.panelOpen(),"capture cancel unexpectedly closed panel");
    key(host,SDL_SCANCODE_R);
    require(host.binding(stellar7::EnhancedAction::left)==SDL_SCANCODE_UNKNOWN,"cancelled capture still rebound key");
    click(host,240,24);click(host,100,159);
    require(host.view()==stellar7::EnhancedView::chase,"previous page did not reach camera row");
    click(host,280,10);require(!host.panelOpen(),"close control did not close panel");
    // Focus/open neutral gates reject the click that opened or resumed the panel.
    key(host,SDL_SCANCODE_F12);mouseButtons=SDL_BUTTON_LMASK;host.servicePanel(window,true);
    click(host,100,75);require(host.options().filter==stellar7::EnhancedFilter::sharp,"reopening bypassed mouse neutral gate");
    mouseButtons=0;host.servicePanel(window,true);
    host.servicePanel(window,false);click(host,100,75,SDL_BUTTON_LEFT,false);
    click(host,100,75);require(host.options().filter==stellar7::EnhancedFilter::sharp,"focus return bypassed neutral gate");
    host.servicePanel(window,true);click(host,100,75);
    require(host.options().filter==stellar7::EnhancedFilter::smooth,"neutral mouse did not rearm after focus return");
    // Integer scaling and square pixels use the same actual viewport transform.
    click(host,100,51);click(host,100,63);
    windowWidth=1366;windowHeight=768;outputWidth=2732;outputHeight=1536;
    click(host,100,75);require(host.options().filter==stellar7::EnhancedFilter::sharp,"integer/square-pixel transform missed row");
    outputWidth=0;outputHeight=0;
    require(click(host,100,75),"minimized panel mouse was not consumed");
    require(host.options().filter==stellar7::EnhancedFilter::sharp,"zero viewport activated control");
    outputWidth=2560;outputHeight=1440;windowWidth=1280;windowHeight=720;
    const auto bounds=host.viewport(nullptr,window);
    float gx=mouseX,gy=mouseY;host.pointer(window,nullptr,gx,gy);
    require(bounds.w>0&&bounds.h>0,"GL output viewport missing");
    position(host,100,75);SDL_Event glClick{};glClick.type=SDL_MOUSEBUTTONDOWN;
    glClick.button.button=SDL_BUTTON_LEFT;glClick.button.x=mouseX;glClick.button.y=mouseY;
    require(host.event(glClick,true,window,nullptr)&&host.options().filter==stellar7::EnhancedFilter::smooth,"GL mouse viewport/click path failed");
    // Saving uses the same existing store; a temporary profile leaves user data untouched.
    require(argc==2,"requires private build directory");
    environment("STELLAR7_HOST_RENDERER","");
    environment("STELLAR7_HOST_CONTROLLERS","");
    RemoveFile saved{std::filesystem::u8path(argv[1])/("panel-mouse-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".cfg")};
    stellar7::EnhancedSdl persistent(saved.path,font,25);
    key(persistent,SDL_SCANCODE_F12);persistent.servicePanel(window,true);
    click(persistent,100,75);click(persistent,280,10);
    require(stellar7::EnhancedOptionsStore(saved.path).load().filter==stellar7::EnhancedFilter::smooth,"mouse change was not persisted");
    environment("STELLAR7_SAFE_MODE","1");
    stellar7::EnhancedSdl safe(saved.path,font,25);key(safe,SDL_SCANCODE_F12);safe.servicePanel(window,true);
    click(safe,100,75);click(safe,280,10);
    require(stellar7::EnhancedOptionsStore(saved.path).load().filter==stellar7::EnhancedFilter::smooth,"safe mouse options overwrote profile");
    std::cout<<"Mouse options: viewport, pages, values, capture, neutral/focus, persistence and safe-mode checks pass\n";
} catch(const std::exception& error) {
    std::cerr<<error.what()<<'\n';return 1;
}
