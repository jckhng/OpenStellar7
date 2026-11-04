#include "enhanced_sdl.hpp"
#include "enhanced_runtime.hpp"
#include "enhanced_diagnostics.hpp"
#include "stellar7/native_game_rate.hpp"
#include <cctype>
#include <iostream>

namespace stellar7 {
namespace {
enum class NativeRow { fullscreen,aspect,scaling,filter,resolution,displayRate,speed,controller,deadzone,volume,view,chaseDistance,chaseHeight,resetControls,cockpitStyle,groundDetail,localLights,count };
constexpr std::size_t nativeRows=static_cast<std::size_t>(NativeRow::count);
constexpr std::size_t rowCount=nativeRows+2*enhancedActionCount,visibleRows=11;
constexpr Uint64 repeatDelayMilliseconds=300,repeatIntervalMilliseconds=100;
constexpr int displayWidth=320,displayHeight=240,sourceHeight=200;
constexpr int panelLeft=8,panelRight=316,rowTop=35,rowHeight=12,decreaseX=272,increaseX=296;
constexpr SDL_Rect closeBounds{256,4,60,14},previousPageBounds{232,18,24,14},nextPageBounds{292,18,24,14};
constexpr std::uint32_t background=0xff101824,foreground=0xffe4ebf4,accent=0xffffd060,dim=0xff95a9c4;
bool contains(const SDL_Rect& bounds,const float x,const float y) {
    return x>=bounds.x&&x<bounds.x+bounds.w&&y>=bounds.y&&y<bounds.y+bounds.h;
}
bool reserved(SDL_Scancode key) {
    return key==SDL_SCANCODE_F10||key==SDL_SCANCODE_F6||key==SDL_SCANCODE_F7||key==SDL_SCANCODE_F8
        ||key==SDL_SCANCODE_F11||key==SDL_SCANCODE_F12;
}
bool allowedKey(std::size_t action,SDL_Scancode key) {
    if(reserved(key)) {return false;}
    constexpr std::array<SDL_Scancode,4> arrows{SDL_SCANCODE_LEFT,SDL_SCANCODE_RIGHT,SDL_SCANCODE_UP,SDL_SCANCODE_DOWN};
    for(std::size_t index=0;index<arrows.size();++index) {
        if(key==arrows[index]&&action!=index) {return false;}
    }
    return key!=SDL_SCANCODE_ESCAPE||action==static_cast<std::size_t>(EnhancedAction::menu);
}
std::string upper(std::string text) {
    for(auto& value:text) {value=static_cast<char>(std::toupper(static_cast<unsigned char>(value)));}
    return text;
}
}
EnhancedSdl::EnhancedSdl(std::filesystem::path path,const BitmapFont& font,const unsigned gameRate)
    :store_(std::move(path)),font_(font) {
    try {options_=store_.load();validateBindings();}
    catch(const std::exception& error) {
        std::cerr<<"Enhanced options: "<<error.what()<<'\n';options_=EnhancedOptions{};
        validateBindings();loaded_=false;message_="OPTIONS NOT LOADED - USING DEFAULTS";
    }
    safeMode_=enhancedSafeMode();
    if(safeMode_) {
        options_.fullscreen=false;options_.width=960;options_.height=720;
        options_.controller=EnhancedController::off;options_.renderScale=2;options_.presentationRate=60;
    }
    if(enhancedGamepadMode()&&!safeMode_) {options_.fullscreen=false;options_.controller=EnhancedController::modern;}
    if(!enhancedControllersEnabled()) {options_.controller=EnhancedController::off;}
    options_.gameRate=gameRate;filterHint();
    if(!enhancedControllersEnabled()) {enhancedHostTrace("controller initialization disabled");}
    else {enhancedHostTrace("opening controller");openController();enhancedHostTrace("controller ready");}
}
EnhancedSdl::~EnhancedSdl() {closeController();}
void EnhancedSdl::validateBindings() {
    for(std::size_t index=0;index<enhancedActionCount;++index) {
        const auto& value=options_.bindings[index];
        keys_[index]=value.key=="none"?SDL_SCANCODE_UNKNOWN:SDL_GetScancodeFromName(value.key.c_str());
        bindings_[index]=value.button=="none"?SDL_CONTROLLER_BUTTON_INVALID:SDL_GameControllerGetButtonFromString(value.button.c_str());
        if((keys_[index]==SDL_SCANCODE_UNKNOWN&&value.key!="none")||!allowedKey(index,keys_[index])
            ||(bindings_[index]==SDL_CONTROLLER_BUTTON_INVALID&&value.button!="none")) {
            throw std::invalid_argument("Invalid enhanced binding: "+std::string(enhancedActionNames[index]));
        }
        for(std::size_t other=0;other<index;++other) {
            if((keys_[index]!=SDL_SCANCODE_UNKNOWN&&keys_[index]==keys_[other])
                ||(bindings_[index]!=SDL_CONTROLLER_BUTTON_INVALID&&bindings_[index]==bindings_[other])) {
                throw std::invalid_argument("Duplicate enhanced control binding");
            }
        }
    }
}
void EnhancedSdl::filterHint() const {SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,options_.filter==EnhancedFilter::sharp?"0":"1");}
void EnhancedSdl::attach(SDL_Window* window) {
    filterHint();
    SDL_SetWindowMinimumSize(window,displayWidth,sourceHeight);
    if(options_.fullscreen) {fullscreen(window,true);}
}
void EnhancedSdl::fullscreen(SDL_Window* window,const bool enabled) {
    if((safeMode_||enhancedGamepadMode())&&enabled) {message_="THIS MODE USES A WINDOW";return;}
    if(!options_.fullscreen&&enabled) {SDL_GetWindowSize(window,&options_.width,&options_.height);}
    const auto result=enhancedHostCall("SDL_SetWindowFullscreen",[&] {
        return SDL_SetWindowFullscreen(window,enabled?SDL_WINDOW_FULLSCREEN_DESKTOP:0);
    });
    if(result!=0) {
        options_.fullscreen=(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN_DESKTOP)!=0;
        message_="FULLSCREEN UNAVAILABLE";std::cerr<<SDL_GetError()<<'\n';return;
    }
    options_.fullscreen=enabled;
    if(!enabled) {SDL_SetWindowSize(window,options_.width,options_.height);}
}
SDL_Rect EnhancedSdl::viewport(SDL_Renderer* renderer,SDL_Window* window) const {
    int width=0,height=0;
    if(renderer) {sdlHost::check(SDL_GetRendererOutputSize(renderer,&width,&height),"SDL_GetRendererOutputSize");}
    else if(window) {SDL_GL_GetDrawableSize(window,&width,&height);}
    const int targetHeight=options_.aspect==EnhancedAspect::display4by3?displayHeight:sourceHeight;
    auto scale=std::min(static_cast<double>(width)/displayWidth,static_cast<double>(height)/targetHeight);
    if(options_.scaling==EnhancedScaling::integer&&scale>=1) {scale=static_cast<int>(scale);}
    const int w=static_cast<int>(displayWidth*scale),h=static_cast<int>(targetHeight*scale);
    return {(width-w)/2,(height-h)/2,w,h};
}
void EnhancedSdl::pointer(SDL_Window* window,SDL_Renderer* renderer,float& x,float& y) const {
    int width=0,height=0,outputWidth=0,outputHeight=0;SDL_GetWindowSize(window,&width,&height);
    if(renderer) {sdlHost::check(SDL_GetRendererOutputSize(renderer,&outputWidth,&outputHeight),"SDL_GetRendererOutputSize");}
    else {SDL_GL_GetDrawableSize(window,&outputWidth,&outputHeight);}
    const auto bounds=viewport(renderer,window);
    if(width<=0||height<=0||bounds.w<=0||bounds.h<=0) {x=-1;y=-1;return;}
    x=(x*outputWidth/width-bounds.x)*displayWidth/bounds.w;
    y=(y*outputHeight/height-bounds.y)*sourceHeight/bounds.h;
}
void EnhancedSdl::warp(SDL_Window* window,SDL_Renderer* renderer,float x,float y) const {
    int width=0,height=0,outputWidth=0,outputHeight=0;SDL_GetWindowSize(window,&width,&height);
    if(renderer) {sdlHost::check(SDL_GetRendererOutputSize(renderer,&outputWidth,&outputHeight),"SDL_GetRendererOutputSize");}
    else {SDL_GL_GetDrawableSize(window,&outputWidth,&outputHeight);}
    if(outputWidth<=0||outputHeight<=0) {return;}
    const auto bounds=viewport(renderer,window);
    SDL_WarpMouseInWindow(window,static_cast<int>((bounds.x+x*bounds.w/displayWidth)*width/outputWidth),
        static_cast<int>((bounds.y+y*bounds.h/sourceHeight)*height/outputHeight));
}
void EnhancedSdl::save(SDL_Window* window) {
    if(safeMode_||enhancedHostOverrides()) {return;}
    if(!loaded_&&!dirty_) {return;}
    if(!options_.fullscreen) {
        SDL_GetWindowSize(window,&options_.width,&options_.height);
        options_.width=std::clamp(options_.width,320,7680);options_.height=std::clamp(options_.height,200,4320);
    }
    try {store_.save(options_);loaded_=true;dirty_=false;}
    catch(const std::exception& error) {message_="OPTIONS NOT SAVED";std::cerr<<error.what()<<'\n';}
}
void EnhancedSdl::rate(const unsigned value,SDL_Window* window) {options_.gameRate=value;dirty_=true;save(window);}
void EnhancedSdl::adjust(const int direction,SDL_Window* window) {
    switch(static_cast<NativeRow>(selected_)) {
    case NativeRow::fullscreen:fullscreen(window,!options_.fullscreen);break;
    case NativeRow::aspect:options_.aspect=options_.aspect==EnhancedAspect::display4by3?EnhancedAspect::sourcePixels:EnhancedAspect::display4by3;break;
    case NativeRow::scaling:options_.scaling=options_.scaling==EnhancedScaling::fit?EnhancedScaling::integer:EnhancedScaling::fit;break;
    case NativeRow::filter:options_.filter=options_.filter==EnhancedFilter::sharp?EnhancedFilter::smooth:EnhancedFilter::sharp;filterHint();textureChanged_=true;break;
    case NativeRow::resolution: {
        constexpr std::array<unsigned,3> scales{1,2,4};
        const auto index=std::find(scales.begin(),scales.end(),options_.renderScale)-scales.begin();
        options_.renderScale=scales[(static_cast<int>(index)+direction+3)%3];textureChanged_=true;break;
    }
    case NativeRow::displayRate: {
        constexpr std::array<unsigned,3> rates{0,60,120};
        const auto index=std::find(rates.begin(),rates.end(),options_.presentationRate)-rates.begin();
        options_.presentationRate=rates[(static_cast<int>(index)+direction+3)%3];break;
    }
    case NativeRow::speed: {
        const auto found=std::find(nativeGameRates.begin(),nativeGameRates.end(),options_.gameRate);
        const auto next=(static_cast<int>(found-nativeGameRates.begin())+direction+static_cast<int>(nativeGameRates.size()))%static_cast<int>(nativeGameRates.size());
        options_.gameRate=nativeGameRates[static_cast<std::size_t>(next)];break;
    }
    case NativeRow::controller:
        if(!enhancedControllersEnabled()) {message_="CONTROLLER DISABLED BY HOST POLICY";return;}
        options_.controller=static_cast<EnhancedController>((static_cast<int>(options_.controller)+direction+3)%3);clearGameplay();break;
    case NativeRow::deadzone:options_.deadzone=std::clamp(options_.deadzone+direction*5,5,95);break;
    case NativeRow::volume:options_.volume=std::clamp(options_.volume+direction*5,0,100);options_.muted=false;break;
    case NativeRow::view:view_=view_==EnhancedView::cockpit?EnhancedView::chase:EnhancedView::cockpit;return;
    case NativeRow::chaseDistance:options_.camera.distance=std::clamp(options_.camera.distance+direction*ChaseOptions::distanceStep,
        ChaseOptions::minimumDistance,ChaseOptions::maximumDistance);break;
    case NativeRow::chaseHeight:options_.camera.height=std::clamp(options_.camera.height+direction*ChaseOptions::heightStep,
        ChaseOptions::minimumHeight,ChaseOptions::maximumHeight);break;
    case NativeRow::cockpitStyle:
    case NativeRow::groundDetail:
    case NativeRow::localLights:
        if(enhancedGraphics()==EnhancedGraphics::cpu&&!enhancedXrScale()) {message_="GL MODE REQUIRED";return;}
        if(selected_==static_cast<std::size_t>(NativeRow::cockpitStyle)&&enhancedXrScale()) {message_="XR USES 3D COCKPIT";return;}
        if(selected_==static_cast<std::size_t>(NativeRow::cockpitStyle)) {options_.scene.cockpit=options_.scene.cockpit==CockpitStyle::spatial?CockpitStyle::original:CockpitStyle::spatial;}
        else if(selected_==static_cast<std::size_t>(NativeRow::groundDetail)) {options_.scene.groundDetail=!options_.scene.groundDetail;}
        else {options_.scene.localLights=!options_.scene.localLights;}
        break;
    default:return;
    }
    dirty_=true;save(window);
}
void EnhancedSdl::togglePanel(SDL_Window* window) {
    panel_=!panel_;capturing_=false;capturePendingButton_.reset();cancel();
    if(!panel_) {save(window);}
}
void EnhancedSdl::panelAction(const PanelAction action,SDL_Window* window) {
    if(action==PanelAction::back) {
        if(capturing_) {capturing_=false;capturePendingButton_.reset();message_.clear();panelAwaitRelease_=true;}
        else {togglePanel(window);}
        return;
    }
    if(capturing_) {return;}
    if(action==PanelAction::up||action==PanelAction::down) {
        selected_=action==PanelAction::up?(selected_+rowCount-1)%rowCount:(selected_+1)%rowCount;message_.clear();return;
    }
    if(action==PanelAction::left||action==PanelAction::right) {adjust(action==PanelAction::left?-1:1,window);return;}
    if(action==PanelAction::clear&&selected_>=nativeRows) {
        if(selected_<nativeRows+enhancedActionCount) {
            const auto index=selected_-nativeRows;keys_[index]=SDL_SCANCODE_UNKNOWN;options_.bindings[index].key="none";
        } else {
            const auto index=selected_-nativeRows-enhancedActionCount;bindings_[index]=SDL_CONTROLLER_BUTTON_INVALID;options_.bindings[index].button="none";
        }
        dirty_=true;message_="BINDING CLEARED";save(window);return;
    }
    if(action!=PanelAction::activate) {return;}
    if(selected_==static_cast<std::size_t>(NativeRow::resetControls)) {
        options_.bindings=EnhancedOptions{}.bindings;validateBindings();clearGameplay();
        dirty_=true;message_="DEFAULT CONTROLS RESTORED";panelAwaitRelease_=true;save(window);return;
    }
    if(selected_==static_cast<std::size_t>(NativeRow::volume)) {
        options_.muted=!options_.muted;dirty_=true;save(window);return;
    }
    if(selected_>=nativeRows) {
        capturing_=true;capturePendingButton_.reset();panelAwaitRelease_=true;
        message_="ESC OR BACK+START CANCELS";return;
    }
    adjust(1,window);
}
bool EnhancedSdl::panelMouse(const SDL_Event& event,const bool focused,SDL_Window* window,SDL_Renderer* renderer) {
    if(event.type!=SDL_MOUSEMOTION&&event.type!=SDL_MOUSEBUTTONDOWN
        &&event.type!=SDL_MOUSEBUTTONUP&&event.type!=SDL_MOUSEWHEEL) {return false;}
    if(!focused) {panelMouseArmed_=false;return true;}
    if(!panelMouseArmed_||!window) {return true;}
    if(event.type==SDL_MOUSEMOTION||event.type==SDL_MOUSEBUTTONUP) {return true;}
    if(event.type==SDL_MOUSEBUTTONDOWN&&event.button.button!=SDL_BUTTON_LEFT&&event.button.button!=SDL_BUTTON_RIGHT) {return true;}
    int mouseX=0,mouseY=0;
    if(event.type==SDL_MOUSEWHEEL) {SDL_GetMouseState(&mouseX,&mouseY);}
    else {mouseX=event.button.x;mouseY=event.button.y;}
    float x=static_cast<float>(mouseX),y=static_cast<float>(mouseY);
    pointer(window,renderer,x,y);
    if(x<panelLeft||x>=panelRight||y<0||y>=sourceHeight) {return true;}
    if(event.type==SDL_MOUSEBUTTONDOWN&&event.button.button==SDL_BUTTON_LEFT&&contains(closeBounds,x,y)) {
        panelAction(PanelAction::back,window);return true;
    }
    if(capturing_) {return true;}
    repeating_.reset();
    if(event.type==SDL_MOUSEWHEEL) {
        const int steps=std::clamp(event.wheel.y,-static_cast<int>(visibleRows),static_cast<int>(visibleRows));
        const bool up=(steps>0)!=(event.wheel.direction==SDL_MOUSEWHEEL_FLIPPED);
        for(int step=0;step<std::abs(steps);++step) {panelAction(up?PanelAction::up:PanelAction::down,window);}
        return true;
    }
    if(event.button.button==SDL_BUTTON_LEFT&&(contains(previousPageBounds,x,y)||contains(nextPageBounds,x,y))) {
        const auto page=selected_/visibleRows;
        if(contains(previousPageBounds,x,y)&&page>0) {selected_=(page-1)*visibleRows;}
        if(contains(nextPageBounds,x,y)&&(page+1)*visibleRows<rowCount) {selected_=(page+1)*visibleRows;}
        message_.clear();return true;
    }
    if(y<rowTop||y>=rowTop+rowHeight*static_cast<int>(visibleRows)) {return true;}
    const auto row=selected_/visibleRows*visibleRows+static_cast<std::size_t>((y-rowTop)/rowHeight);
    if(row>=rowCount) {return true;}
    selected_=row;message_.clear();
    if(event.button.button==SDL_BUTTON_RIGHT) {
        panelAction(row>=nativeRows?PanelAction::clear:PanelAction::left,window);return true;
    }
    const bool adjustable=row<nativeRows&&row!=static_cast<std::size_t>(NativeRow::resetControls);
    panelAction(adjustable&&x>=decreaseX?(x<increaseX?PanelAction::left:PanelAction::right):PanelAction::activate,window);
    return true;
}
void EnhancedSdl::captureKey(const SDL_Scancode key) {
    const auto index=selected_-nativeRows;
    if(!allowedKey(index,key)) {message_="THAT KEY IS RESERVED";return;}
    const auto value=key==SDL_SCANCODE_DELETE?SDL_SCANCODE_UNKNOWN:key;
    for(std::size_t other=0;other<keys_.size();++other) {
        if(other!=index&&value!=SDL_SCANCODE_UNKNOWN&&keys_[other]==value) {message_="KEY ALREADY ASSIGNED";return;}
    }
    options_.bindings[index].key=value==SDL_SCANCODE_UNKNOWN?"none":SDL_GetScancodeName(value);
    keys_[index]=value;capturing_=false;dirty_=true;message_="BINDING SAVED ON CLOSE";
}
void EnhancedSdl::captureButton(const SDL_GameControllerButton button) {
    const auto index=selected_-nativeRows-enhancedActionCount;
    for(std::size_t other=0;other<bindings_.size();++other) {
        if(other!=index&&button!=SDL_CONTROLLER_BUTTON_INVALID&&bindings_[other]==button) {message_="BUTTON ALREADY ASSIGNED";return;}
    }
    options_.bindings[index].button=button==SDL_CONTROLLER_BUTTON_INVALID?"none":SDL_GameControllerGetStringForButton(button);
    bindings_[index]=button;capturing_=false;dirty_=true;message_="BINDING SAVED ON CLOSE";cancel();
}
bool EnhancedSdl::key(const SDL_KeyboardEvent& event,SDL_Window* window) {
    const auto code=event.keysym.scancode;
    if(!panel_&&code==SDL_SCANCODE_F9&&viewHotkeyAvailable()
        &&(event.keysym.mod&(KMOD_CTRL|KMOD_ALT|KMOD_GUI))==0) {
        if(!event.repeat) {view_=view_==EnhancedView::cockpit?EnhancedView::chase:EnhancedView::cockpit;}
        return true;
    }
    const bool navigation=code==SDL_SCANCODE_UP||code==SDL_SCANCODE_DOWN||code==SDL_SCANCODE_LEFT||code==SDL_SCANCODE_RIGHT;
    if(event.repeat&&(!panel_||capturing_||!navigation)) {return panel_;}
    if(panel_&&capturing_) {
        if(code==SDL_SCANCODE_ESCAPE) {panelAction(PanelAction::back,window);}
        else if(selected_<nativeRows+enhancedActionCount) {captureKey(code);}
        else if(code==SDL_SCANCODE_DELETE) {captureButton(SDL_CONTROLLER_BUTTON_INVALID);}
        return true;
    }
    if(code==SDL_SCANCODE_F12||(panel_&&code==SDL_SCANCODE_ESCAPE)) {togglePanel(window);return true;}
    if(code==SDL_SCANCODE_F11||(code==SDL_SCANCODE_RETURN&&(event.keysym.mod&KMOD_ALT)!=0)) {
        fullscreen(window,!options_.fullscreen);dirty_=true;save(window);return true;
    }
    if(!panel_&&(code==SDL_SCANCODE_F6||code==SDL_SCANCODE_F7||code==SDL_SCANCODE_F8)) {
        const auto previous=selected_;selected_=code==SDL_SCANCODE_F6?static_cast<std::size_t>(NativeRow::aspect)
            :code==SDL_SCANCODE_F7?static_cast<std::size_t>(NativeRow::scaling):static_cast<std::size_t>(NativeRow::filter);
        adjust(1,window);selected_=previous;return true;
    }
    if(!panel_) {return false;}
    switch(code) {
    case SDL_SCANCODE_UP:panelAction(PanelAction::up,window);break;
    case SDL_SCANCODE_DOWN:panelAction(PanelAction::down,window);break;
    case SDL_SCANCODE_LEFT:panelAction(PanelAction::left,window);break;
    case SDL_SCANCODE_RIGHT:panelAction(PanelAction::right,window);break;
    case SDL_SCANCODE_RETURN:panelAction(PanelAction::activate,window);break;
    case SDL_SCANCODE_DELETE:panelAction(PanelAction::clear,window);break;
    default:break;
    }
    return true;
}
bool EnhancedSdl::takeTextureChange() {return std::exchange(textureChanged_,false);}
void EnhancedSdl::openController() {
    //Keep a recognized pad available for native options even when gameplay input
    //is disabled or delegated to the original joystick owner.
    if(!enhancedControllersEnabled()||controller_||!controllerScanPending_
        ||(SDL_WasInit(SDL_INIT_GAMECONTROLLER)&SDL_INIT_GAMECONTROLLER)==0) {return;}
    // A failed open is retried on a device change, not on every source poll.
    controllerScanPending_=false;
    const EnhancedHostOperation operation("controller discovery/open");
    const auto count=SDL_NumJoysticks();
    for(int index=0;index<count;++index) {
        if(SDL_IsGameController(index)) {controller_=SDL_GameControllerOpen(index);if(controller_) {panelAwaitRelease_=true;break;}}
    }
}
void EnhancedSdl::closeController() {
    cancel();controllerScanPending_=true;
    if(controller_) {SDL_GameControllerClose(controller_);controller_=nullptr;}
}
void EnhancedSdl::clearGameplay() {
    armed_=false;axisX_=0;axisY_=0;held_.fill(false);pressed_.fill(false);
    for(auto& button:buttons_) {button.clear();}
}
void EnhancedSdl::cancel() {
    external_.cancel();externalRepeating_.reset();externalRepeatAt_=0;
    clearGameplay();panelAwaitRelease_=true;repeating_.reset();repeatAt_=0;
    panelMouseArmed_=false;
    backReceived_=false;startReceived_=false;chordHeld_=false;capturePendingButton_.reset();
}
bool EnhancedSdl::backAssigned() const {
    return std::find(bindings_.begin(),bindings_.end(),SDL_CONTROLLER_BUTTON_BACK)!=bindings_.end();
}
bool EnhancedSdl::viewHotkeyAvailable() const {
    return std::find(keys_.begin(),keys_.end(),SDL_SCANCODE_F9)==keys_.end();
}
void EnhancedSdl::repeat(const PanelAction action,SDL_Window* window) {
    panelAction(action,window);repeating_=action;
    repeatAt_=SDL_GetPerformanceCounter()+repeatDelayMilliseconds*SDL_GetPerformanceFrequency()/1000;
}
bool EnhancedSdl::event(const SDL_Event& event,const bool focused,SDL_Window* window,SDL_Renderer* renderer) {
    if(panel_&&panelMouse(event,focused,window,renderer)) {return true;}
    if(event.type==SDL_CONTROLLERDEVICEADDED||event.type==SDL_CONTROLLERDEVICEREMOVED) {
        controllerScanPending_=true;
        if(controller_&&!SDL_GameControllerGetAttached(controller_)) {closeController();}
        openController();return false;
    }
    if(!focused||!controller_||(event.type!=SDL_CONTROLLERBUTTONDOWN&&event.type!=SDL_CONTROLLERBUTTONUP)
        ||event.cbutton.which!=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller_))
        ||event.cbutton.button>=buttons_.size()) {return false;}
    const auto button=static_cast<SDL_GameControllerButton>(event.cbutton.button);
    const bool down=event.type==SDL_CONTROLLERBUTTONDOWN;
    if(button==SDL_CONTROLLER_BUTTON_BACK) {backReceived_=down;}
    if(button==SDL_CONTROLLER_BUTTON_START) {startReceived_=down;}
    const bool chord=backReceived_&&startReceived_;
    if(!chord) {chordHeld_=false;}
    if(chord&&!chordHeld_) {
        chordHeld_=true;
        if(panel_&&capturing_) {panelAction(PanelAction::back,window);}
        else {togglePanel(window);}
        return true;
    }
    if(!panel_) {
        if(down&&button==SDL_CONTROLLER_BUTTON_BACK&&!backAssigned()) {togglePanel(window);return true;}
        if(options_.controller==EnhancedController::modern) {buttons_[event.cbutton.button].transition(down);}
        return false;
    }
    if(panelAwaitRelease_) {return true;}
    if(capturing_) {
        if(selected_<nativeRows+enhancedActionCount) {
            if(down&&(button==SDL_CONTROLLER_BUTTON_B||button==SDL_CONTROLLER_BUTTON_BACK)) {panelAction(PanelAction::back,window);}
            else if(down) {message_="KEYBOARD KEY REQUIRED - B CANCELS";}
            return true;
        }
        //Back/Start complete on release so their chord can cancel capture first.
        if(button==SDL_CONTROLLER_BUTTON_BACK||button==SDL_CONTROLLER_BUTTON_START) {
            if(down) {capturePendingButton_=button;}
            else if(capturePendingButton_==button) {capturePendingButton_.reset();captureButton(button);}
        } else if(down) {captureButton(button);}
        return true;
    }
    if(!down) {return true;}
    switch(button) {
    case SDL_CONTROLLER_BUTTON_A:panelAction(PanelAction::activate,window);break;
    case SDL_CONTROLLER_BUTTON_B:panelAction(PanelAction::back,window);break;
    case SDL_CONTROLLER_BUTTON_Y:panelAction(PanelAction::clear,window);break;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:repeat(PanelAction::up,window);break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:repeat(PanelAction::down,window);break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:repeat(PanelAction::left,window);break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:repeat(PanelAction::right,window);break;
    default:break;
    }
    return true;
}
void EnhancedSdl::servicePanel(SDL_Window* window,const bool focused) {
    if(!panel_) {return;}
    if(!focused) {panelMouseArmed_=false;}
    else if(SDL_GetMouseState(nullptr,nullptr)==0) {panelMouseArmed_=true;}
    if(controller_&&!SDL_GameControllerGetAttached(controller_)) {closeController();message_="CONTROLLER DISCONNECTED";}
    openController();
    if(!focused||!controller_) {panelAwaitRelease_=true;repeating_.reset();return;}
    const auto button=[&](SDL_GameControllerButton value) {return SDL_GameControllerGetButton(controller_,value)!=0;};
    const int x=SDL_GameControllerGetAxis(controller_,SDL_CONTROLLER_AXIS_LEFTX);
    const int y=SDL_GameControllerGetAxis(controller_,SDL_CONTROLLER_AXIS_LEFTY);
    const int threshold=options_.deadzone*32767/100;
    if(panelAwaitRelease_) {
        bool any=false;for(int index=0;index<SDL_CONTROLLER_BUTTON_MAX;++index) {any=any||button(static_cast<SDL_GameControllerButton>(index));}
        if(!any&&std::abs(x)<=threshold&&std::abs(y)<=threshold) {panelAwaitRelease_=false;}
        return;
    }
    if(capturing_) {repeating_.reset();return;}
    std::optional<PanelAction> direction;
    if(button(SDL_CONTROLLER_BUTTON_DPAD_UP)) {direction=PanelAction::up;}
    else if(button(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) {direction=PanelAction::down;}
    else if(button(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) {direction=PanelAction::left;}
    else if(button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) {direction=PanelAction::right;}
    else if(std::abs(y)>=std::abs(x)&&std::abs(y)>threshold) {direction=y<0?PanelAction::up:PanelAction::down;}
    else if(std::abs(x)>threshold) {direction=x<0?PanelAction::left:PanelAction::right;}
    if(!direction) {repeating_.reset();return;}
    if(repeating_!=direction) {repeat(*direction,window);return;}
    if(SDL_GetPerformanceCounter()>=repeatAt_) {
        panelAction(*direction,window);
        repeatAt_=SDL_GetPerformanceCounter()+repeatIntervalMilliseconds*SDL_GetPerformanceFrequency()/1000;
    }
}
void EnhancedSdl::poll(const bool focused) {
    const EnhancedHostOperation operation("controller poll");
    if(controller_&&!SDL_GameControllerGetAttached(controller_)) {closeController();}
    openController();
    if(!focused||!controller_||panel_||options_.controller!=EnhancedController::modern) {clearGameplay();return;}
    const int threshold=options_.deadzone*32767/100;
    axisX_=SDL_GameControllerGetAxis(controller_,SDL_CONTROLLER_AXIS_LEFTX);
    axisY_=SDL_GameControllerGetAxis(controller_,SDL_CONTROLLER_AXIS_LEFTY);
    bool any=false;
    for(std::size_t index=0;index<buttons_.size();++index) {
        buttons_[index].transition(SDL_GameControllerGetButton(controller_,static_cast<SDL_GameControllerButton>(index))!=0);
        buttons_[index].poll();any=any||buttons_[index].held();
    }
    if(!armed_) {
        if(!any&&std::abs(axisX_)<=threshold&&std::abs(axisY_)<=threshold) {armed_=true;}
        held_.fill(false);pressed_.fill(false);return;
    }
    for(std::size_t index=0;index<enhancedActionCount;++index) {
        const auto button=bindings_[index];const bool down=button!=SDL_CONTROLLER_BUTTON_INVALID&&buttons_[static_cast<std::size_t>(button)].held();
        const bool stick=index==static_cast<std::size_t>(EnhancedAction::left)?axisX_<-threshold
            :index==static_cast<std::size_t>(EnhancedAction::right)?axisX_>threshold
            :index==static_cast<std::size_t>(EnhancedAction::forward)?axisY_<-threshold
            :index==static_cast<std::size_t>(EnhancedAction::reverse)&&axisY_>threshold;
        const bool current=down||stick;pressed_[index]=current&&!held_[index];held_[index]=current;
    }
}
namespace {
std::optional<ControllerButton> externalButton(EnhancedAction action) {
    switch(action) {
    case EnhancedAction::left:return ControllerButton::left;
    case EnhancedAction::right:return ControllerButton::right;
    case EnhancedAction::forward:return ControllerButton::forward;
    case EnhancedAction::reverse:return ControllerButton::reverse;
    case EnhancedAction::fire:return ControllerButton::fire;
    case EnhancedAction::activate:return ControllerButton::activate;
    case EnhancedAction::cycle:return ControllerButton::cycle;
    case EnhancedAction::jump:return ControllerButton::jump;
    case EnhancedAction::shield:return ControllerButton::shield;
    case EnhancedAction::menu:return ControllerButton::menu;
    default:return std::nullopt;
    }
}
}
void EnhancedSdl::externalInput(const ControllerInput& input,bool focused,SDL_Window* window) {
    external_.poll(input,focused&&!safeMode_);
    if(!focused||!input.connected||safeMode_) {externalRepeating_.reset();return;}
    if(external_.pressed(ControllerButton::options)) {togglePanel(window);return;}
    if(!panel_) {
        if(external_.pressed(ControllerButton::view)) {view_=view_==EnhancedView::cockpit?EnhancedView::chase:EnhancedView::cockpit;}
        return;
    }
    if(external_.pressed(ControllerButton::back)||external_.pressed(ControllerButton::menu)) {panelAction(PanelAction::back,window);return;}
    if(capturing_) {externalRepeating_.reset();return;} // XR bindings belong to the runtime, not SDL pad capture.
    if(external_.pressed(ControllerButton::confirm)) {panelAction(PanelAction::activate,window);return;}
    std::optional<PanelAction> direction;
    for(const auto& item:std::array<std::pair<ControllerButton,PanelAction>,4>{{
        {ControllerButton::forward,PanelAction::up},{ControllerButton::reverse,PanelAction::down},
        {ControllerButton::left,PanelAction::left},{ControllerButton::right,PanelAction::right}}}) {
        if(external_.held(item.first)) {direction=item.second;break;}
    }
    if(!direction) {externalRepeating_.reset();return;}
    const auto now=SDL_GetPerformanceCounter();
    if(externalRepeating_!=direction) {
        externalRepeating_=direction;panelAction(*direction,window);
        externalRepeatAt_=now+repeatDelayMilliseconds*SDL_GetPerformanceFrequency()/1000;return;
    }
    if(now>=externalRepeatAt_) {
        panelAction(*direction,window);externalRepeatAt_=now+repeatIntervalMilliseconds*SDL_GetPerformanceFrequency()/1000;
    }
}
bool EnhancedSdl::held(const EnhancedAction action) const {
    const auto button=externalButton(action);
    return held_[static_cast<std::size_t>(action)]||(!panel_&&button&&external_.held(*button));
}
bool EnhancedSdl::pressed(const EnhancedAction action) const {
    const auto button=externalButton(action);
    return pressed_[static_cast<std::size_t>(action)]||(!panel_&&button&&external_.pressed(*button));
}
SDL_Scancode EnhancedSdl::binding(const EnhancedAction action) const {return keys_[static_cast<std::size_t>(action)];}
bool EnhancedSdl::confirm() const {return (armed_&&buttons_[SDL_CONTROLLER_BUTTON_A].held())||(!panel_&&external_.held(ControllerButton::confirm));}
bool EnhancedSdl::back() const {return (armed_&&buttons_[SDL_CONTROLLER_BUTTON_B].held())||(!panel_&&external_.held(ControllerButton::back));}
bool EnhancedSdl::uiDirection(const EnhancedAction action) const {
    const auto external=externalButton(action);
    if(!panel_&&external&&external_.held(*external)) {return true;}
    if(!armed_) {return false;}
    const int threshold=options_.deadzone*32767/100;
    switch(action) {
    case EnhancedAction::left:return buttons_[SDL_CONTROLLER_BUTTON_DPAD_LEFT].held()||axisX_<-threshold;
    case EnhancedAction::right:return buttons_[SDL_CONTROLLER_BUTTON_DPAD_RIGHT].held()||axisX_>threshold;
    case EnhancedAction::forward:return buttons_[SDL_CONTROLLER_BUTTON_DPAD_UP].held()||axisY_<-threshold;
    case EnhancedAction::reverse:return buttons_[SDL_CONTROLLER_BUTTON_DPAD_DOWN].held()||axisY_>threshold;
    default:return false;
    }
}
void EnhancedSdl::draw(std::array<std::uint32_t,320*200>& pixels) const {
    pixels.fill(background);
    const auto text=[&](const int x,const int y,const std::string& label,const std::uint32_t color,const int right=panelRight) {
        int cursor=x;
        for(const auto character:upper(label)) {
            const auto value=static_cast<unsigned char>(character);
            if(value<font_.firstCharacter||value-font_.firstCharacter>=font_.glyphs.size()) {continue;}
            const auto& glyph=font_.glyphs[value-font_.firstCharacter];
            for(int row=0;row<glyph.height&&y+row<200;++row) {
                for(int column=0;column<glyph.width&&cursor+column<right;++column) {
                    if(glyph.pixels[static_cast<std::size_t>(row)*glyph.width+column]) {pixels[static_cast<std::size_t>(y+row)*320+cursor+column]=color;}
                }
            }
            cursor+=glyph.width;
        }
    };
    text(8,6,"STELLAR 7 ENHANCED",accent);text(8,19,"OPTIONS - GAME PAUSED",foreground);
    const auto first=selected_/visibleRows*visibleRows;
    text(closeBounds.x,6,capturing_?"[BACK]":"[CLOSE]",accent);
    text(previousPageBounds.x,19,"[<]",first>0?accent:dim);
    text(260,19,std::to_string(selected_/visibleRows+1)+"/"+std::to_string((rowCount+visibleRows-1)/visibleRows),foreground);
    text(nextPageBounds.x,19,"[>]",first+visibleRows<rowCount?accent:dim);
    for(std::size_t index=first;index<std::min(first+visibleRows,rowCount);++index) {
        std::string label;
        switch(static_cast<NativeRow>(index)) {
        case NativeRow::fullscreen:label=std::string("FULLSCREEN: ")+(options_.fullscreen?"ON":"OFF");break;
        case NativeRow::aspect:label=std::string("ASPECT: ")+(options_.aspect==EnhancedAspect::display4by3?"DOS DISPLAY 4:3":"SQUARE PIXELS 8:5");break;
        case NativeRow::scaling:label=std::string("SCALING: ")+(options_.scaling==EnhancedScaling::fit?"FIT WINDOW":"INTEGER DISPLAY SCALE");break;
        case NativeRow::filter:label=std::string("FILTER: ")+(options_.filter==EnhancedFilter::sharp?"SHARP":"SMOOTH");break;
        case NativeRow::resolution:label="WORLD: "+std::to_string(320*options_.renderScale)+" X "+std::to_string(200*options_.renderScale);break;
        case NativeRow::displayRate:label="DISPLAY: "+(options_.presentationRate==0?std::string("SOURCE FRAMES"):std::to_string(options_.presentationRate)+" HZ");break;
        case NativeRow::speed:label="SPEED: "+(options_.gameRate==0?std::string("NATIVE"):std::to_string(options_.gameRate)+" UPDATES/S");break;
        case NativeRow::controller:label=std::string("CONTROLLER: ")+(!enhancedControllersEnabled()?"DISABLED BY HOST":options_.controller==EnhancedController::modern?"MODERN":options_.controller==EnhancedController::original?"ORIGINAL JOYSTICK":"OFF");break;
        case NativeRow::deadzone:label="STICK DEAD ZONE: "+std::to_string(options_.deadzone)+"%";break;
        case NativeRow::volume:label="MASTER VOLUME: "+std::to_string(options_.volume)+"%"+(options_.muted?" MUTED":"");break;
        case NativeRow::view:label=std::string("CAMERA: ")+(view_==EnhancedView::cockpit?"COCKPIT":"CHASE")
            +(viewHotkeyAvailable()?" (F9)":"");break;
            case NativeRow::chaseDistance:label="CHASE DISTANCE: "+std::to_string(options_.camera.distance);break;
            case NativeRow::chaseHeight:label="CHASE HEIGHT: "+std::to_string(options_.camera.height);break;
            case NativeRow::resetControls:label="RESET CONTROL BINDINGS";break;
            case NativeRow::cockpitStyle:label=std::string("COCKPIT: ")+(enhancedXrScale()?"3D XR":enhancedGraphics()==EnhancedGraphics::cpu?"GL ONLY":options_.scene.cockpit==CockpitStyle::spatial?"3D":"ORIGINAL");break;
            case NativeRow::groundDetail:label=std::string("GROUND DETAIL: ")+(enhancedGraphics()==EnhancedGraphics::cpu&&!enhancedXrScale()?"GL ONLY":options_.scene.groundDetail?"ON":"OFF");break;
            case NativeRow::localLights:label=std::string("EXPLOSION LIGHTS: ")+(enhancedGraphics()==EnhancedGraphics::cpu&&!enhancedXrScale()?"GL ONLY":options_.scene.localLights?"ON":"OFF");break;
        default: {
            const bool keyboard=index<nativeRows+enhancedActionCount;
            const auto action=index-nativeRows-(keyboard?0:enhancedActionCount);
            label=std::string(keyboard?"KEY ":"PAD ")+enhancedActionNames[action]+": "+(keyboard?options_.bindings[action].key:options_.bindings[action].button);break;
        }
        }
        const auto y=rowTop+static_cast<int>(index-first)*rowHeight;
        const auto color=index==selected_?accent:foreground;
        const bool adjustable=index<nativeRows&&index!=static_cast<std::size_t>(NativeRow::resetControls);
        text(panelLeft,y,(index==selected_?"> ":"  ")+label,color,adjustable?decreaseX:panelRight);
        if(adjustable) {text(280,y,"-",color);text(304,y,"+",color);}
    }
    const bool waitingForPad=capturing_&&selected_>=nativeRows+enhancedActionCount&&!controller_;
    text(8,170,capturePendingButton_?"RELEASE BUTTON TO ASSIGN":waitingForPad?"CONNECT A CONTROLLER - ESC CANCELS"
        :panelAwaitRelease_&&controller_?"RELEASE BUTTONS AND CENTER STICK"
        :capturing_?"PRESS A KEY OR CONTROLLER BUTTON":message_.empty()?"ARROWS/WHEEL SELECT - CLICK CHANGE":message_,accent);
    text(8,181,capturing_?"ESC / BACK+START CANCEL CAPTURE":controller_?"A SELECT - B CLOSE - Y CLEAR BINDING":"ENTER REBIND - DELETE CLEAR - ESC CLOSE",dim);
    text(8,191,controller_?"F12 / BACK+START OPTIONS - F11 FULLSCREEN":"F12 OPTIONS - F11 FULLSCREEN - F6/F7/F8",dim);
}
} //namespace stellar7
