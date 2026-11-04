#pragma once
#include <SDL.h>
#include <algorithm>
#include <stdexcept>
#include <string>

namespace stellar7::sdlHost {
inline constexpr int sourceWidth=320,sourceHeight=200;
inline constexpr auto scanCount=SDL_NUM_SCANCODES;
inline constexpr auto controlModifier=KMOD_CTRL,altModifier=KMOD_ALT,guiModifier=KMOD_GUI;
inline void check(int result,const char* operation) {
    if(result<0) {throw std::runtime_error(std::string(operation)+": "+SDL_GetError());}
}
inline SDL_Scancode scan(const SDL_KeyboardEvent& event) {return event.keysym.scancode;}
inline auto modifiers(const SDL_KeyboardEvent& event) {return event.keysym.mod;}

enum class EventKind {other,quit,focusLost,focusGained,exposed,mouseLeave,keyDown,keyUp,text,
    mouseMotion,mouseDown,mouseUp,joystickDown,joystickUp};
inline EventKind kind(const SDL_Event& event) {
    switch(event.type) {
    case SDL_QUIT:return EventKind::quit;
    case SDL_KEYDOWN:return EventKind::keyDown;
    case SDL_KEYUP:return EventKind::keyUp;
    case SDL_TEXTINPUT:return EventKind::text;
    case SDL_MOUSEMOTION:return EventKind::mouseMotion;
    case SDL_MOUSEBUTTONDOWN:return EventKind::mouseDown;
    case SDL_MOUSEBUTTONUP:return EventKind::mouseUp;
    case SDL_JOYBUTTONDOWN:return EventKind::joystickDown;
    case SDL_JOYBUTTONUP:return EventKind::joystickUp;
    case SDL_WINDOWEVENT:
        switch(event.window.event) {
        case SDL_WINDOWEVENT_CLOSE:return EventKind::quit;
        case SDL_WINDOWEVENT_FOCUS_LOST:case SDL_WINDOWEVENT_MINIMIZED:return EventKind::focusLost;
        case SDL_WINDOWEVENT_FOCUS_GAINED:return EventKind::focusGained;
        case SDL_WINDOWEVENT_EXPOSED:return EventKind::exposed;
        case SDL_WINDOWEVENT_LEAVE:return EventKind::mouseLeave;
        default:return EventKind::other;
        }
    default:return EventKind::other;
    }
}
inline SDL_Window* createWindow(int width,int height) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
    return SDL_CreateWindow("Stellar 7",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,width,height,
        SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
}
inline SDL_Renderer* createRenderer(SDL_Window* window) {
    return SDL_CreateRenderer(window,-1,0);
}
inline void configureRenderer(SDL_Renderer* renderer) {
    // Keep raw window mouse coordinates. SDL2's logical-size filter would scale
    // them implicitly; the explicit viewport below owns both directions instead.
    (void)renderer;
}
inline SDL_Rect viewport(SDL_Renderer* renderer) {
    int width=0,height=0;check(SDL_GetRendererOutputSize(renderer,&width,&height),"SDL_GetRendererOutputSize");
    const auto scale=std::min(static_cast<double>(width)/sourceWidth,static_cast<double>(height)/sourceHeight);
    const auto w=static_cast<int>(sourceWidth*scale),h=static_cast<int>(sourceHeight*scale);
    return {(width-w)/2,(height-h)/2,w,h};
}
inline void renderTexture(SDL_Renderer* renderer,SDL_Texture* texture) {
    const auto bounds=viewport(renderer);
    if(bounds.w>0&&bounds.h>0) {check(SDL_RenderCopy(renderer,texture,nullptr,&bounds),"SDL_RenderCopy");}
}
inline void presentRenderer(SDL_Renderer* renderer) {
    SDL_RenderPresent(renderer);
}
inline void nearestTexture(SDL_Texture* texture) {
    (void)texture; // SDL2.0.10 uses the pre-creation nearest-scale hint.
}
inline void startText(SDL_Window* window) {
    (void)window;SDL_StartTextInput();check(SDL_ShowCursor(SDL_DISABLE),"SDL_ShowCursor");
}
inline void stopText(SDL_Window* window) {
    (void)window;SDL_StopTextInput();SDL_ShowCursor(SDL_ENABLE);
}
inline void setTitle(SDL_Window* window,const char* title) {
    SDL_SetWindowTitle(window,title);
}
inline void pointerPosition(SDL_Window* window,SDL_Renderer* renderer,const SDL_Event& event,float& x,float& y) {
    const auto motion=kind(event)==EventKind::mouseMotion;
    x=motion?event.motion.x:event.button.x;y=motion?event.motion.y:event.button.y;
    int width=0,height=0,outputWidth=0,outputHeight=0;SDL_GetWindowSize(window,&width,&height);
    check(SDL_GetRendererOutputSize(renderer,&outputWidth,&outputHeight),"SDL_GetRendererOutputSize");
    const auto bounds=viewport(renderer);
    if(width<=0||height<=0||bounds.w<=0||bounds.h<=0) {x=-1;y=-1;return;}
    x=(x*outputWidth/width-bounds.x)*sourceWidth/bounds.w;
    y=(y*outputHeight/height-bounds.y)*sourceHeight/bounds.h;
}
inline void warpPointer(SDL_Window* window,SDL_Renderer* renderer,float x,float y) {
    int width=0,height=0,outputWidth=0,outputHeight=0;SDL_GetWindowSize(window,&width,&height);
    check(SDL_GetRendererOutputSize(renderer,&outputWidth,&outputHeight),"SDL_GetRendererOutputSize");
    if(outputWidth<=0||outputHeight<=0) {return;}
    const auto bounds=viewport(renderer);
    SDL_WarpMouseInWindow(window,static_cast<int>((bounds.x+x*bounds.w/sourceWidth)*width/outputWidth),
        static_cast<int>((bounds.y+y*bounds.h/sourceHeight)*height/outputHeight));
}
inline auto joystickId(SDL_Joystick* device) {
    return SDL_JoystickInstanceID(device);
}
inline bool joystickConnected(SDL_Joystick* device) {
    return SDL_JoystickGetAttached(device)==SDL_TRUE;
}
inline void closeJoystick(SDL_Joystick* device) {
    SDL_JoystickClose(device);
}
inline SDL_Joystick* openGameplayJoystick() {
    const auto count=SDL_NumJoysticks();
    SDL_Joystick* result=nullptr;
    for(int index=0;index<count;++index) {
        auto* next=SDL_JoystickOpen(index);
        if(next&&SDL_JoystickNumAxes(next)>=2) {result=next;break;}
        if(next) {closeJoystick(next);}
    }
    return result;
}
inline auto joystickAxis(SDL_Joystick* device,int axis) {
    return SDL_JoystickGetAxis(device,axis);
}
inline auto joystickButton(SDL_Joystick* device,int button) {
    return SDL_JoystickGetButton(device,button);
}
inline int joystickButtons(SDL_Joystick* device) {
    return SDL_JoystickNumButtons(device);
}
} //namespace stellar7::sdlHost
