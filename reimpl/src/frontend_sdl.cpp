#define SDL_MAIN_HANDLED
#include "sdl_host.hpp"
#include "stellar7/frontend.hpp"
#include "stellar7/recovered_frontend.hpp"
#include "stellar7/native_game_rate.hpp"
#include "sdl_audio.hpp"
#include "sdl_input.hpp"
#include "enhanced_options.hpp"
#ifdef STELLAR7_ENHANCED
#include "enhanced_sdl.hpp"
#include "destruction_audio.hpp"
#include "world_presentation.hpp"
#include "enhanced_runtime.hpp"
#include "enhanced_host.hpp"
#include "gl_sdl_presenter.hpp"
#ifdef STELLAR7_OPENXR_HOST
#include "openxr_sdl.hpp"
#endif
#endif
#include <array>
#include <deque>
#include <memory>
#include <string>
#include <utility>

namespace stellar7 {
namespace {
using sdlHost::check;
using sdlHost::EventKind;
constexpr int sourceWidth=320,sourceHeight=200,initialScale=3;
bool ordinary(GameplayPhase phase) {
    return phase==GameplayPhase::level1||phase==GameplayPhase::level2||phase==GameplayPhase::level3Native
        ||phase==GameplayPhase::playable||phase==GameplayPhase::level5;
}
bool modifier(SDL_Scancode key) {
    return key==SDL_SCANCODE_LSHIFT||key==SDL_SCANCODE_RSHIFT||key==SDL_SCANCODE_LCTRL
        ||key==SDL_SCANCODE_RCTRL||key==SDL_SCANCODE_LALT||key==SDL_SCANCODE_RALT
        ||key==SDL_SCANCODE_LGUI||key==SDL_SCANCODE_RGUI;
}
class Runtime {
public:
    Runtime() {
#ifdef STELLAR7_ENHANCED
        flags_=enhancedHostInitFlags();configureEnhancedHost();
#ifndef STELLAR7_OPENXR_HOST
        if(enhancedXrScale()) {throw std::runtime_error("This enhanced build has no OpenXR host; configure STELLAR7_OPENXR=ON");}
#endif
        enhancedHostTrace(enhancedHostRenderer()?enhancedHostRenderer():"automatic renderer selection");
        enhancedHostTrace(enhancedControllersEnabled()?"controller subsystem requested":"controller subsystem disabled");
        enhancedHostTrace("initializing SDL");
        const EnhancedHostOperation operation("SDL_Init");
#endif
        SDL_SetMainReady();check(SDL_Init(flags_),"SDL_Init");
#ifdef STELLAR7_ENHANCED
        enhancedHostTrace("SDL initialized");
        SDL_version version{};SDL_GetVersion(&version);
        const auto details=std::string("SDL=")+std::to_string(version.major)+"."+std::to_string(version.minor)+"."
            +std::to_string(version.patch)+" video="+(SDL_GetCurrentVideoDriver()?SDL_GetCurrentVideoDriver():"none")
            +" audio="+(SDL_GetCurrentAudioDriver()?SDL_GetCurrentAudioDriver():"none");
        enhancedHostTrace(details.c_str());
#endif
    }
    ~Runtime() {
#ifdef STELLAR7_ENHANCED
        const EnhancedHostOperation operation("SDL_QuitSubSystem");
#endif
        SDL_QuitSubSystem(flags_);
    }
private:
#ifdef STELLAR7_ENHANCED
    EnhancedHostDiagnostics diagnostics_;
#endif
    Uint32 flags_=SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_JOYSTICK
#ifdef STELLAR7_ENHANCED
        |SDL_INIT_GAMECONTROLLER
#endif
        ;
};

class SdlGameplay {
public:
    SdlGameplay(const GameplayAssets& assets,const NativeStartupAssets& startup,FrameCopyProfile copy,
        const GameplayRunOptions& options):options_(options),pacer_(options.nativeGameRate) {
        if(!validNativeGameRate(options.nativeGameRate)) {throw std::invalid_argument("unsupported native game rate");}
        if(options.startAtLevel2||options.startAtLevel3) {throw std::invalid_argument("SDL playable entry requires actual cold startup");}
        if(!options.inputRecordingPath.empty()) {throw std::invalid_argument("SDL native input recording is not yet supported");}
#ifdef STELLAR7_ENHANCED
        auto path=options.nativeOptionsPath;
        if(path.empty()) {
            const std::unique_ptr<char,decltype(&SDL_free)> directory(SDL_GetPrefPath("dos-re-harness","Stellar7Enhanced"),SDL_free);
            if(!directory) {throw std::runtime_error(SDL_GetError());}
            path=std::filesystem::u8path(directory.get())/"enhanced.cfg";
        }
        enhanced_=std::make_unique<EnhancedSdl>(path,assets.scoreFont,options.nativeGameRate);
        enhancedHostTrace("creating window");
        window_.reset(enhancedHostCall("SDL_CreateWindow",[&] {
            const auto graphics=enhancedGraphics();
            if(graphics!=EnhancedGraphics::cpu) {
                return GlSdlPresenter::createWindow(enhanced_->options().width,enhanced_->options().height,
                    graphics==EnhancedGraphics::opengl?GlProfile::desktop33:GlProfile::es30);
            }
            return sdlHost::createWindow(enhanced_->options().width,enhanced_->options().height);
        }));
#else
        window_.reset(sdlHost::createWindow(sourceWidth*initialScale,sourceHeight*initialScale));
#endif
        if(!window_) {throw std::runtime_error(SDL_GetError());}
#ifdef STELLAR7_ENHANCED
        enhanced_->attach(window_.get());
        audio_.setVolume(enhanced_->playbackVolume());
#endif
#ifdef STELLAR7_ENHANCED
        const auto graphics=enhancedGraphics();
        if(graphics==EnhancedGraphics::cpu) {renderer_.reset(createEnhancedRenderer(window_.get()));}
        else {gl_=std::make_unique<GlSdlPresenter>(window_.get(),graphics==EnhancedGraphics::opengl?GlProfile::desktop33:GlProfile::es30);}
#else
        renderer_.reset(sdlHost::createRenderer(window_.get()));
#endif
#ifdef STELLAR7_ENHANCED
        if(!gl_) {
#endif
        if(!renderer_) {throw std::runtime_error(SDL_GetError());}
#ifdef STELLAR7_ENHANCED
        enhancedHostTrace("renderer created");
#endif
        sdlHost::configureRenderer(renderer_.get());
        texture_.reset(SDL_CreateTexture(renderer_.get(),SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,sourceWidth,sourceHeight));
        if(!texture_) {throw std::runtime_error(SDL_GetError());}
        sdlHost::nearestTexture(texture_.get());
#ifdef STELLAR7_ENHANCED
        }
#endif
        sdlHost::startText(window_.get());
        focused_=(SDL_GetWindowFlags(window_.get())&SDL_WINDOW_INPUT_FOCUS)!=0;
#ifdef STELLAR7_OPENXR_HOST
        desktopFocused_=focused_;
        if(const auto scale=enhancedXrScale()) {xr_=std::make_unique<OpenXrSdlHost>(*scale);}
#endif
        const auto joystick=joystickEnabled()?joystick_.poll(focused_):NativeJoystickSample{};
        frontend_=std::make_unique<RecoveredFrontend>(assets,startup,copy,workBudget,options.highscoreStore,
            CampaignExecution::playable,NativePointerCapability::portablePointer,options.menuFeatures,joystick);
#ifdef STELLAR7_ENHANCED
        enhancedHostTrace("frontend ready");
#endif
        audio_.service([this] {return service();});
#ifdef STELLAR7_ENHANCED
        const EnhancedHostOperation firstPresent("initial texture/clear/present");
#endif
#ifdef STELLAR7_ENHANCED
        if(gl_) {gl_->bitmap(std::vector<std::uint32_t>(sourceWidth*sourceHeight,0xff000000),sourceWidth,sourceHeight,{0,0,0,sourceWidth,sourceHeight});gl_->swap();}
        else {
#endif
        check(SDL_SetRenderDrawColor(renderer_.get(),0,0,0,255),"SDL_SetRenderDrawColor");
        check(SDL_RenderClear(renderer_.get()),"SDL_RenderClear");sdlHost::presentRenderer(renderer_.get());
#ifdef STELLAR7_ENHANCED
        }
#endif
    }
    ~SdlGameplay() {
#ifdef STELLAR7_ENHANCED
        if(enhanced_&&window_) {enhanced_->save(window_.get());}
#endif
        if(window_) {sdlHost::stopText(window_.get());}
    }
    int run() {
        while(service()&&audio_.waitForInput()) {
#ifdef STELLAR7_ENHANCED
            auto geometry=gl_?PresentationGeometry::spatial:PresentationGeometry::projected;
#ifdef STELLAR7_OPENXR_HOST
            if(xr_) {geometry=PresentationGeometry::spatial;}
#endif
            WorldPresentationCapture capture(enhanced_->options().camera,geometry);
            const EnhancedHostOperation operation("source step");
            if(enhanced_->panelOpen()) {SDL_Delay(8);continue;}
            enhanced_->poll(focused_);
#ifdef STELLAR7_OPENXR_HOST
            if(xr_) {
                enhanced_->externalInput(xr_->controls(),controllerFocused(),window_.get());
                if(enhanced_->recenterPressed()) {xr_->recenter();}
                if(enhanced_->panelOpen()) {
                    clearInput();redrawRequested_=true;
                    if(!lastFrame_) {lastFrame_=IndexedScreen{std::vector<std::uint8_t>(sourceWidth*sourceHeight),std::vector<Rgb>(256)};}
                    continue;
                }
            }
#endif
            if(!frontend_->session()&&!mainMenu()) {
                for(std::size_t i=0;i<enhancedActionCount;++i) {
                    if(enhanced_->pressed(static_cast<EnhancedAction>(i))) {introSkipRequested_=true;}
                }
            }
#endif
            if(!cancelPending_&&!pointerCancelPending_) {
                keys_.poll();primary_.poll();secondary_.poll();
            }
            cancelThisPoll_=std::exchange(cancelPending_,false);
            pointerCancelThisPoll_=std::exchange(pointerCancelPending_,false)||cancelThisPoll_;
            if(!keys_.held(SDL_SCANCODE_ESCAPE)) {pauseEscapeHeld_=false;}
            auto joystick=joystickEnabled()?std::optional<NativeJoystickSample>{joystick_.poll(focused_)}:std::nullopt;
            if(joystick&&cancelThisPoll_) {joystick->connected=false;joystick->buttons=0;joystick->phase=NativeJoystickPhase::cancel;}
#ifdef STELLAR7_ENHANCED
            if(frontend_->session()&&ordinary(frontend_->session()->model().phase())&&!modal()) {
                for(std::size_t index=static_cast<std::size_t>(EnhancedAction::activate);index<enhancedActionCount;++index) {
                    const auto action=static_cast<EnhancedAction>(index);
                    if(enhanced_->pressed(action)) {enqueueAction(action);}
                }
            }
#endif
            const bool menuPolled=mainMenu();
            const auto sourceKey=focused_&&frontend_->attractAcceptsSourceKey()?sourceKeys_.front():0;
            const auto entrySample=frontend_->elapsedSamples();
            const bool paced=frontend_->session()&&ordinary(frontend_->session()->model().phase())&&!modal();
            NativeSessionDelivery delivery;
            std::size_t streamed=0;
            std::vector<NativeTimedFrame> streamedFrames;
            bool running=true;
            const auto streamChunk=[&](const NativeSessionDelivery& chunk) {
                if(running) {
                    running=queue(chunk);streamed+=chunk.samples.size();
                    streamedFrames.insert(streamedFrames.end(),chunk.presentations.begin(),chunk.presentations.end());
                }
            };
            if(frontend_->session()) {delivery=advanceGameplay(joystick,sourceKey,streamChunk);}
            else {
                auto input=startupInput();input.joystick=joystick;input.sourceKey=sourceKey;
                input.calibrateJoystick=std::exchange(calibrationRequested_,false);
                delivery=frontend_->advanceStartup(input,streamChunk);
            }
            if(sourceKey!=0||(menuPolled&&!attract())) {sourceKeys_.pop();}
            if(!attract()&&!mainMenu()&&!frontend_->presentationInputActive()) {sourceKeys_.clear();}
            if(!running||!deliver(delivery,streamed,streamedFrames)||delivery.exitRequested) {return 0;}
            if(paced&&frontend_->session()&&ordinary(frontend_->session()->model().phase())&&!modal()) {
                if(!queue(frontend_->paceGameplay(pacer_.deadline(entrySample)))) {return 0;}
            } else {pacer_.reset();}
            title();
        }
        return 0;
    }
private:
    bool joystickEnabled() const {
#ifdef STELLAR7_ENHANCED
        if(!enhancedControllersEnabled()) {return false;}
        if(enhanced_&&enhanced_->options().controller!=EnhancedController::original) {return false;}
#endif
        return hasNativeMenuFeature(options_.menuFeatures,NativeMenuFeatures::joystickControls);
    }
    bool sourceKeysEnabled() const {return hasNativeMenuFeature(options_.menuFeatures,NativeMenuFeatures::automaticAttract);}
    bool mainMenu() const {
        return frontend_->session()?frontend_->session()->model().phase()==GameplayPhase::nativeMenu
            :frontend_->startup().phase()==NativeStartupPhase::awaitingMenuDecision;
    }
    bool attract() const {return frontend_->attractProgress().has_value();}
    bool modal() const {return frontend_->menuBackAvailable()||frontend_->presentationInputActive()||frontend_->joystickCalibrationActive();}
    bool held(SDL_Scancode key) const {return focused_&&keys_.held(key);}
    bool controllerFocused() const {
#ifdef STELLAR7_OPENXR_HOST
        if(xr_) {return xr_->focused();}
#endif
        return focused_;
    }
    bool driveHeld(EnhancedAction action,SDL_Scancode fallback) const {
#ifdef STELLAR7_ENHANCED
        const bool arrow=action==EnhancedAction::left||action==EnhancedAction::right
            ||action==EnhancedAction::forward||action==EnhancedAction::reverse;
        return held(enhanced_->binding(action))||(controllerFocused()&&enhanced_->held(action))||(arrow&&held(fallback));
#else
        (void)action;return held(fallback);
#endif
    }
    bool uiHeld(EnhancedAction action,SDL_Scancode fallback) const {
#ifdef STELLAR7_ENHANCED
        return held(fallback)||(controllerFocused()&&enhanced_->uiDirection(action));
#else
        (void)action;return held(fallback);
#endif
    }
    bool confirmHeld() const {
#ifdef STELLAR7_ENHANCED
        return held(SDL_SCANCODE_RETURN)||(controllerFocused()&&enhanced_->confirm());
#else
        return held(SDL_SCANCODE_RETURN);
#endif
    }
    bool backHeld() const {
#ifdef STELLAR7_ENHANCED
        return held(SDL_SCANCODE_ESCAPE)||(controllerFocused()&&enhanced_->back());
#else
        return held(SDL_SCANCODE_ESCAPE);
#endif
    }
    void enqueueAction(EnhancedAction action) {
        switch(action) {
        case EnhancedAction::activate:systemActions_.push_back(GameplaySystemAction::activateSelected);break;
        case EnhancedAction::cycle:systemActions_.push_back(GameplaySystemAction::cycleSelected);break;
        case EnhancedAction::decoy:systemActions_.push_back(GameplaySystemAction::decoy);break;
        case EnhancedAction::bomb:systemActions_.push_back(GameplaySystemAction::bomb);break;
        case EnhancedAction::jump:systemActions_.push_back(GameplaySystemAction::jump);break;
        case EnhancedAction::shield:systemActions_.push_back(GameplaySystemAction::shield);break;
        case EnhancedAction::scanner:systemActions_.push_back(GameplaySystemAction::scanner);break;
        case EnhancedAction::superPrimary:systemActions_.push_back(GameplaySystemAction::superPrimary);break;
        case EnhancedAction::turbo:systemActions_.push_back(GameplaySystemAction::turbo);break;
        case EnhancedAction::menu:systemActions_.push_back(GameplaySystemAction::preferences);break;
        default:break;
        }
    }
    void clearInput() {
#ifdef STELLAR7_ENHANCED
        if(enhanced_) {enhanced_->cancel();}
#endif
        keys_.clear();primary_.clear();secondary_.clear();sourceKeys_.clear();systemActions_.clear();continueScans_.clear();
        nameCharacters_.clear();presentationKey_=false;presentationEscape_=false;calibrationRequested_=false;
        introSkipRequested_=false;pointerCancelled_=true;mouseArmed_=false;pauseEscapeHeld_=false;
        cancelPending_=true;joystick_.poll(false);
    }
    void changeRate(int direction) {
        auto found=std::find(nativeGameRates.begin(),nativeGameRates.end(),pacer_.rate());
        const auto index=static_cast<int>(found-nativeGameRates.begin());
        const auto count=static_cast<int>(nativeGameRates.size());
        pacer_.setRate(nativeGameRates[static_cast<std::size_t>((index+direction+count)%count)]);
#ifdef STELLAR7_ENHANCED
        enhanced_->rate(pacer_.rate(),window_.get());
#endif
    }
    void keyDown(const SDL_KeyboardEvent& event) {
#ifdef STELLAR7_ENHANCED
        if(enhanced_->key(event,window_.get())) {
            if(sdlHost::scan(event)!=SDL_SCANCODE_F9) {clearInput();}
            redrawRequested_=true;
            if(!lastFrame_&&enhanced_->panelOpen()) {lastFrame_=IndexedScreen{std::vector<std::uint8_t>(sourceWidth*sourceHeight),std::vector<Rgb>(256)};}
            return;
        }
#endif
        const auto key=sdlHost::scan(event);
        if(event.repeat&&!keys_.received(key)) {return;}
        keys_.transition(key,true);
        if(sourceKeysEnabled()&&(mainMenu()||attract()||frontend_->presentationInputActive())) {sourceKeys_.keyDown(key);}
        if(frontend_->startup().titleActive()) {return;}
        if(attract()||frontend_->joystickCalibrationActive()) {return;}
        if(!frontend_->session()&&!mainMenu()&&!modifier(key)) {introSkipRequested_=true;return;}
        if(frontend_->presentationInputActive()) {
            if(key==SDL_SCANCODE_ESCAPE) {presentationEscape_=true;}
            else if(!modifier(key)) {presentationKey_=true;}
            return;
        }
        const auto mods=sdlHost::modifiers(event);
        const bool modified=(mods&(sdlHost::controlModifier|sdlHost::altModifier|sdlHost::guiModifier))!=0;
        if(!event.repeat&&!modified&&(key==SDL_SCANCODE_MINUS||key==SDL_SCANCODE_KP_MINUS)) {changeRate(-1);return;}
        if(!event.repeat&&!modified&&(key==SDL_SCANCODE_EQUALS||key==SDL_SCANCODE_KP_PLUS)) {changeRate(1);return;}
        const auto* session=frontend_->session();
        if(!event.repeat&&key==SDL_SCANCODE_C&&(mods&sdlHost::altModifier)!=0&&(mods&sdlHost::controlModifier)==0&&joystickEnabled()) {
            if(session) {systemActions_.push_back(GameplaySystemAction::calibrateJoystick);}
            else if(mainMenu()) {calibrationRequested_=true;}
            return;
        }
        if(session&&ordinary(session->model().phase())&&!modal()&&!modified) {
#ifdef STELLAR7_ENHANCED
            if(!event.repeat) {
                if(key==SDL_SCANCODE_F10) {enqueueAction(EnhancedAction::menu);}
                else {
                    for(std::size_t index=static_cast<std::size_t>(EnhancedAction::activate);index<enhancedActionCount;++index) {
                        const auto action=static_cast<EnhancedAction>(index);
                        if(key==enhanced_->binding(action)) {enqueueAction(action);break;}
                    }
                }
            }
#else
            switch(key) {
            case SDL_SCANCODE_F10:
                if(!event.repeat&&hasNativeMenuFeature(options_.menuFeatures,NativeMenuFeatures::fullGraphicDetail)) {
                    systemActions_.push_back(GameplaySystemAction::preferences);
                }break;
            case SDL_SCANCODE_I:systemActions_.push_back(GameplaySystemAction::decoy);break;
            case SDL_SCANCODE_B:systemActions_.push_back(GameplaySystemAction::bomb);break;
            case SDL_SCANCODE_J:systemActions_.push_back(GameplaySystemAction::jump);break;
            case SDL_SCANCODE_E:systemActions_.push_back(GameplaySystemAction::shield);break;
            case SDL_SCANCODE_C:systemActions_.push_back(GameplaySystemAction::scanner);break;
            case SDL_SCANCODE_S:systemActions_.push_back(GameplaySystemAction::superPrimary);break;
            case SDL_SCANCODE_T:systemActions_.push_back(GameplaySystemAction::turbo);break;
            case SDL_SCANCODE_TAB:systemActions_.push_back(GameplaySystemAction::cycleSelected);break;
            case SDL_SCANCODE_RETURN:systemActions_.push_back(GameplaySystemAction::activateSelected);break;
            default:break;
            }
#endif
        }
        if(session&&(session->model().phase()==GameplayPhase::failurePostExplosionBlackout
            ||session->model().phase()==GameplayPhase::continuePrompt)) {
            const auto scan=sdlHost::biosScan(key);if(scan!=0&&scan!=1) {continueScans_.push_back(scan);}
        }
        if(session&&session->model().phase()==GameplayPhase::highscoreNameEntry&&key==SDL_SCANCODE_BACKSPACE) {nameCharacters_.push_back(8);}
    }
    void event(SDL_Event value) {
#ifdef STELLAR7_OPENXR_HOST
        if(xr_&&value.type==SDL_WINDOWEVENT&&value.window.windowID!=SDL_GetWindowID(window_.get())) {return;}
        if(xr_&&sdlHost::kind(value)==EventKind::keyDown&&value.key.keysym.scancode==SDL_SCANCODE_F8&&value.key.repeat==0) {xr_->recenter();return;}
#endif
#ifdef STELLAR7_ENHANCED
        const auto panelWasOpen=enhanced_->panelOpen();
        const bool hostEvent=enhanced_->event(value,focused_,window_.get(),renderer_.get());
        if(panelWasOpen!=enhanced_->panelOpen()) {
            clearInput();redrawRequested_=true;
        }
        if(hostEvent) {return;}
#endif
        const auto kind=sdlHost::kind(value);
        if(kind==EventKind::quit) {running_=false;return;}
        if(kind==EventKind::focusLost) {
#ifdef STELLAR7_OPENXR_HOST
            desktopFocused_=false;
#endif
            focused_=false;clearInput();return;
        }
        if(kind==EventKind::focusGained) {
#ifdef STELLAR7_OPENXR_HOST
            desktopFocused_=true;focused_=!xr_||xr_->focused();
#else
            focused_=true;
#endif
            clearInput();redrawRequested_=true;return;
        }
#ifdef STELLAR7_ENHANCED
        if(enhanced_->panelOpen()&&kind!=EventKind::keyDown) {return;}
#endif
        if(kind==EventKind::joystickDown||kind==EventKind::joystickUp) {if(focused_&&joystickEnabled()) {joystick_.event(value);}return;}
        if(!focused_) {return;}
        if(kind==EventKind::keyDown) {keyDown(value.key);return;}
        if(kind==EventKind::keyUp) {keys_.transition(sdlHost::scan(value.key),false);return;}
        if(kind==EventKind::text) {
            if(sourceKeysEnabled()&&(mainMenu()||attract()||frontend_->presentationInputActive())) {sourceKeys_.text(value.text.text);}
            if(frontend_->session()&&frontend_->session()->model().phase()==GameplayPhase::highscoreNameEntry) {
                for(const auto* c=reinterpret_cast<const unsigned char*>(value.text.text);*c;++c) {if(*c<=127) {nameCharacters_.push_back(*c);}}
            }return;
        }
        if(kind==EventKind::mouseLeave) {
            pointerCancelled_=true;pointerCancelPending_=true;primary_.clear();secondary_.clear();mouseArmed_=false;return;
        }
        if(kind==EventKind::mouseMotion||kind==EventKind::mouseDown||kind==EventKind::mouseUp) {
            float x=0,y=0;
#ifdef STELLAR7_ENHANCED
            const bool motion=kind==EventKind::mouseMotion;
            x=motion?value.motion.x:value.button.x;y=motion?value.motion.y:value.button.y;
            enhanced_->pointer(window_.get(),renderer_.get(),x,y);
#else
            sdlHost::pointerPosition(window_.get(),renderer_.get(),value,x,y);
#endif
            const bool wasCancelled=pointerCancelled_;
            pointerCancelled_=x<0||x>=sourceWidth||y<0||y>=sourceHeight;
            pointerPosition_={static_cast<std::int16_t>(std::clamp(x,0.0f,319.0f)),static_cast<std::int16_t>(std::clamp(y,0.0f,199.0f))};
            if(pointerCancelled_) {pointerCancelPending_=pointerCancelPending_||!wasCancelled;primary_.clear();secondary_.clear();mouseArmed_=false;return;}
            if(kind==EventKind::mouseDown||kind==EventKind::mouseUp) {
                const bool down=kind==EventKind::mouseDown;
                if(value.button.button==SDL_BUTTON_LEFT) {primary_.transition(down);}
                if(value.button.button==SDL_BUTTON_RIGHT) {secondary_.transition(down);}
            }
        }
        if(kind==EventKind::exposed) {redrawRequested_=true;}
    }
    void pumpEvents() {
#ifdef STELLAR7_ENHANCED
        const EnhancedHostOperation operation("SDL_PollEvent/event dispatch");
#endif
        // Repaints are requests, not render calls. Bound each drain so a resize
        // or input producer cannot starve audio, close or the next source poll.
        constexpr std::size_t maximumEventsPerService=128;
        SDL_Event next;
        for(std::size_t count=0;running_&&count<maximumEventsPerService&&SDL_PollEvent(&next);++count) {event(next);}
    }
    bool service() {
        pumpEvents();
        if(!running_) {return false;}
#ifdef STELLAR7_OPENXR_HOST
        if(!serviceXr()) {return false;}
#endif
#ifdef STELLAR7_ENHANCED
        if(enhanced_->panelOpen()) {
            //Service the overlay inside any queued-audio wait as well as between
            //frames. Neither source advancement nor device consumption continues.
            audio_.suspend();
            try {
                while(running_&&enhanced_->panelOpen()) {
                    pumpEvents();
#ifdef STELLAR7_OPENXR_HOST
                    if(!serviceXr()) {break;}
                    if(xr_) {
                        enhanced_->externalInput(xr_->controls(),controllerFocused(),window_.get());
                        if(enhanced_->recenterPressed()) {xr_->recenter();}
                    }
#endif
                    if(!running_) {break;}
                    enhanced_->servicePanel(window_.get(),focused_);
                    audio_.setVolume(enhanced_->playbackVolume());
                    if(lastFrame_) {present(*lastFrame_);}
                    SDL_Delay(8);
                }
            } catch(...) {audio_.resume();throw;}
            audio_.setVolume(enhanced_->playbackVolume());audio_.resume();
            if(!running_) {return false;}
        }
        audio_.setVolume(enhanced_->playbackVolume());
        if(pacer_.rate()!=enhanced_->options().gameRate) {pacer_.setRate(enhanced_->options().gameRate);}
        if(enhanced_->takeTextureChange()) {
            texture_.reset();textureWidth_=0;textureHeight_=0;
            if(lastFrame_) {present(*lastFrame_);}
        }
#endif
        const auto cursor=audio_.presentationEstimate();
        while(!presentations_.empty()&&presentations_.front().sampleEnd<=cursor) {
#ifdef STELLAR7_ENHANCED
            lastSourceSample_=presentations_.front().sampleEnd;
            if(enhanced_->options().presentationRate!=0) {
                lastFrame_=presentations_.front().indexed;redrawRequested_=true;
            } else {present(presentations_.front().indexed);}
            presentations_.pop_front();
#else
            present(presentations_.front().indexed);presentations_.pop_front();
#endif
        }
#ifdef STELLAR7_ENHANCED
        const auto now=SDL_GetPerformanceCounter(),frequency=SDL_GetPerformanceFrequency();
        if(cursor!=displayCursor_) {displayCursor_=cursor;displayCursorTick_=now;}
        const auto rate=enhanced_->options().presentationRate;
        if(rate!=lastDisplayRate_) {lastDisplayRate_=rate;nextPresentTick_=now;}
        if(rate!=0&&lastFrame_&&(redrawRequested_||lastFrame_->presentation)&&!enhanced_->panelOpen()
            &&now>=nextPresentTick_) {
            const IndexedScreen* next=nullptr;double fraction=0;
            if(!presentations_.empty()&&presentations_.front().sampleEnd>lastSourceSample_) {
                const auto duration=presentations_.front().sampleEnd-lastSourceSample_;
                // Smooth the callback estimate only inside audio already fed to
                // the device. No lookahead wait, simulation step or PCM is added.
                constexpr std::uint64_t sourceSampleRate=44100,maximumBlendSamples=sourceSampleRate/5;
                const auto smooth=std::min(audio_.consumed(),cursor+(now-displayCursorTick_)*sourceSampleRate/frequency);
                if(duration<=maximumBlendSamples&&smooth>=lastSourceSample_) {
                    next=&presentations_.front().indexed;
                    fraction=std::min(1.0,static_cast<double>(smooth-lastSourceSample_)/duration);
                }
            }
            present(*lastFrame_,next,fraction);
            const auto period=frequency/rate;
            nextPresentTick_=now>nextPresentTick_+period?now+period:nextPresentTick_+period;
        }
#endif
#ifdef STELLAR7_ENHANCED
        if(rate==0&&redrawRequested_&&lastFrame_) {present(*lastFrame_);}
#else
        if(redrawRequested_&&lastFrame_) {present(*lastFrame_);}
#endif
        return running_;
    }
#ifdef STELLAR7_OPENXR_HOST
    bool serviceXr() {
        if(!xr_) {return running_;}
        xr_->check();
        if(xr_->exiting()) {running_=false;return false;}
        const auto runtimeFocus=xr_->focused();const auto focus=desktopFocused_&&runtimeFocus;
        if(focus!=focused_||xrInputFocused_!=runtimeFocus) {focused_=focus;xrInputFocused_=runtimeFocus;clearInput();redrawRequested_=true;}
        return running_;
    }
#endif
    std::optional<NativePointerInput> pointer() {
        NativePointerInput result;result.position=pointerPosition_;
        if(pointerCancelThisPoll_) {result.phase=NativePointerPhase::cancel;return result;}
        // Deliver cancellation once; an unavailable pointer must not keep
        // preempting keyboard navigation while the cursor stays outside.
        if(!focused_||pointerCancelled_||SDL_GetMouseFocus()!=window_.get()) {return std::nullopt;}
        if(!mouseArmed_) {if(!primary_.held()&&!secondary_.held()) {mouseArmed_=true;}return result;}
        result.primary=primary_.held();result.secondary=secondary_.held();return result;
    }
    NativeMenuInput startupInput() {
        NativeMenuInput input;
        const bool title=frontend_->startup().titleActive();
        const bool coldPresentation=frontend_->startup().storyActive()||title||frontend_->startup().creditsActive();
        if(mainMenu()||coldPresentation||frontend_->joystickCalibrationActive()||attract()) {
            input={uiHeld(EnhancedAction::forward,SDL_SCANCODE_UP),uiHeld(EnhancedAction::reverse,SDL_SCANCODE_DOWN),confirmHeld(),
                uiHeld(EnhancedAction::left,SDL_SCANCODE_LEFT),uiHeld(EnhancedAction::right,SDL_SCANCODE_RIGHT),
                (frontend_->menuBackAvailable()||coldPresentation)&&backHeld()};
            if(title) {presentationKey_=false;presentationEscape_=false;introSkipRequested_=false;}
            else if(coldPresentation) {
                input.confirm=std::exchange(presentationKey_,false)||input.confirm;
                input.cancel=std::exchange(presentationEscape_,false)||input.cancel;
                if(introSkipRequested_) {input.cancel=true;introSkipRequested_=false;}
            } else {presentationKey_=false;presentationEscape_=false;}
            if(frontend_->joystickCalibrationActive()) {input.cancel=false;input.confirm=input.confirm||held(SDL_SCANCODE_SPACE);}
            input.pointer=pointer();
        }
        return input;
    }
    ContinueKeyboardPacket continuePacket() const {
        ContinueKeyboardPacket result;result.peekScan=continueScans_.empty()?0:continueScans_.front();
        constexpr std::array<SDL_Scancode,8> keys{SDL_SCANCODE_END,SDL_SCANCODE_DOWN,SDL_SCANCODE_PAGEDOWN,
            SDL_SCANCODE_HOME,SDL_SCANCODE_UP,SDL_SCANCODE_PAGEUP,SDL_SCANCODE_LEFT,SDL_SCANCODE_RIGHT};
        for(std::size_t index=0;index<keys.size();++index) {result.directions[index]=held(keys[index]);}
#ifdef STELLAR7_ENHANCED
        result.directions[1]=result.directions[1]||enhanced_->uiDirection(EnhancedAction::reverse);
        result.directions[4]=result.directions[4]||enhanced_->uiDirection(EnhancedAction::forward);
        result.directions[6]=result.directions[6]||enhanced_->uiDirection(EnhancedAction::left);
        result.directions[7]=result.directions[7]||enhanced_->uiDirection(EnhancedAction::right);
#endif
        result.enter=confirmHeld();result.space=held(SDL_SCANCODE_SPACE);return result;
    }
    NativeSessionDelivery advanceGameplay(std::optional<NativeJoystickSample> joystick,std::uint16_t sourceKey,const NativeDeliverySink& sink) {
        const auto& model=frontend_->session()->model();const auto phase=model.phase();
        const bool presentation=frontend_->presentationInputActive(),calibration=frontend_->joystickCalibrationActive();
        const bool live=frontend_->menuBackAvailable()&&phase!=GameplayPhase::nativeMenu;
        const bool isModal=modal()||phase==GameplayPhase::continuePrompt||phase==GameplayPhase::highscoreNameEntry
            ||phase==GameplayPhase::highscoreHall||phase==GameplayPhase::nativeMenu||phase==GameplayPhase::nativeBeginWait;
        const auto movement=[&](EnhancedAction action,SDL_Scancode key) {return isModal?uiHeld(action,key):driveHeld(action,key);};
        GameplayInput input{movement(EnhancedAction::left,SDL_SCANCODE_LEFT),movement(EnhancedAction::right,SDL_SCANCODE_RIGHT),
            movement(EnhancedAction::forward,SDL_SCANCODE_UP),movement(EnhancedAction::reverse,SDL_SCANCODE_DOWN),
            isModal?held(SDL_SCANCODE_SPACE):driveHeld(EnhancedAction::fire,SDL_SCANCODE_SPACE),
            (frontend_->menuBackAvailable()||presentation||phase==GameplayPhase::continuePrompt)&&!pauseEscapeHeld_&&backHeld(),
            isModal&&confirmHeld()};
        if(presentation&&!attract()) {
            input.confirm=std::exchange(presentationKey_,false)||input.confirm||confirmHeld();input.exit=std::exchange(presentationEscape_,false)||input.exit;
            if(!frontend_->session()->storyScene()) {input={};}
        } else {presentationKey_=false;presentationEscape_=false;}
        if(calibration) {input.exit=false;}
        if(isModal) {input.pointer=pointer();}
        input.joystick=joystick;input.sourceKey=sourceKey;
        if(ordinary(phase)&&!live&&!presentation&&!calibration&&!attract()&&keys_.pressed(SDL_SCANCODE_ESCAPE)
            &&hasNativeMenuFeature(options_.menuFeatures,NativeMenuFeatures::fullGraphicDetail)) {
            input.systemAction=GameplaySystemAction::preferences;pauseEscapeHeld_=true;
        }
        if(input.systemAction==GameplaySystemAction::none&&(ordinary(phase)||phase==GameplayPhase::nativeMenu)
            &&!live&&!presentation&&!calibration&&!attract()&&!systemActions_.empty()) {input.systemAction=systemActions_.front();systemActions_.pop_front();}
        if((!ordinary(phase)&&phase!=GameplayPhase::nativeMenu)||live||presentation||calibration||attract()) {systemActions_.clear();}
        NameEntryInput text;
        if(phase==GameplayPhase::highscoreNameEntry) {text.characters=std::exchange(nameCharacters_,{});} else {nameCharacters_.clear();}
        const bool polling=phase==GameplayPhase::continuePrompt&&model.recoveredContinueDecision()==ContinueDecision::pending;
        if((phase==GameplayPhase::endingReturn||phase==GameplayPhase::campaignEnded)&&!presentation) {input={};input.joystick=joystick;}
        auto result=frontend_->advanceGameplay(input,polling?continuePacket():ContinueKeyboardPacket{},text,sink);
        if(polling&&!continueScans_.empty()) {continueScans_.pop_front();}
        if(model.phase()!=GameplayPhase::continuePrompt||model.recoveredContinueDecision()!=ContinueDecision::pending) {continueScans_.clear();}
        return result;
    }
    bool queue(const NativeSessionDelivery& delivery) {
        if(delivery.firstSample!=audio_.submitted()||!delivery.entry.empty()) {throw std::logic_error("noncontiguous SDL source delivery");}
        const auto end=delivery.firstSample+delivery.samples.size();
        for(const auto& frame:delivery.presentations) {
            if(frame.sampleEnd<delivery.firstSample||frame.sampleEnd>end
                ||(!presentations_.empty()&&frame.sampleEnd<presentations_.back().sampleEnd)) {throw std::logic_error("invalid SDL source mark");}
            presentations_.push_back(frame);
        }
        if(delivery.pointerPosition&&focused_&&!pointerCancelled_&&SDL_GetMouseFocus()==window_.get()
            &&(delivery.pointerPosition->x!=pointerPosition_.x||delivery.pointerPosition->y!=pointerPosition_.y)) {
            pointerPosition_=*delivery.pointerPosition;
            warpPointer(delivery.pointerPosition->x,delivery.pointerPosition->y);
        }
#ifdef STELLAR7_ENHANCED
        const auto* session=frontend_->session();
        const bool world=session&&(ordinary(session->model().phase())||frontend_->menuBackAvailable());
        if(!world) {destructionAudio_.reset();}
        const auto sound=world&&session->soundsEnabled()?DestructionSound::enabled:DestructionSound::muted;
        return audio_.submit(destructionAudio_.mix(delivery,sound))&&service();
#else
        return audio_.submit(delivery.samples)&&service();
#endif
    }
    static bool sameFrame(const NativeTimedFrame& a,const NativeTimedFrame& b) {
        if(a.sampleEnd!=b.sampleEnd||a.indexed.pixels!=b.indexed.pixels||a.indexed.palette.size()!=b.indexed.palette.size()) {return false;}
        for(std::size_t index=0;index<a.indexed.palette.size();++index) {
            const auto left=a.indexed.palette[index],right=b.indexed.palette[index];
            if(left.red!=right.red||left.green!=right.green||left.blue!=right.blue) {return false;}
        }
        return true;
    }
    bool deliver(const NativeSessionDelivery& delivery,std::size_t streamed,const std::vector<NativeTimedFrame>& streamedFrames) {
        if(streamed>delivery.samples.size()||delivery.firstSample+streamed!=audio_.submitted()) {throw std::logic_error("invalid SDL streamed startup prefix");}
        auto remaining=delivery;remaining.firstSample+=streamed;
        remaining.samples.erase(remaining.samples.begin(),remaining.samples.begin()+streamed);
        // Some streaming callers return their whole observation receipt. Remove
        // only its verified, already-published prefix; equal-time new marks stay.
        if(!streamedFrames.empty()&&remaining.presentations.size()>=streamedFrames.size()
            &&std::equal(streamedFrames.begin(),streamedFrames.end(),remaining.presentations.begin(),sameFrame)) {
            remaining.presentations.erase(remaining.presentations.begin(),remaining.presentations.begin()+streamedFrames.size());
        }
        if(!queue(remaining)) {return false;}
        if(audio_.submitted()!=frontend_->elapsedSamples()) {throw std::logic_error("SDL lost generated PCM");}
        return true;
    }
    void warpPointer(float x,float y) {
#ifdef STELLAR7_ENHANCED
        enhanced_->warp(window_.get(),renderer_.get(),x,y);
#else
        sdlHost::warpPointer(window_.get(),renderer_.get(),x,y);
#endif
    }
    void present(const IndexedScreen& frame,const IndexedScreen* next=nullptr,double fraction=0) {
        if(frame.pixels.size()!=pixels_.size()||frame.palette.size()!=256) {throw std::runtime_error("invalid SDL indexed frame");}
        redrawRequested_=false;
#ifdef STELLAR7_OPENXR_HOST
        if(xr_&&!enhanced_->panelOpen()) {
            const bool world=frontend_->session()&&ordinary(frontend_->session()->model().phase())&&!modal();
            if(world&&frame.presentation&&frame.presentation->chase&&frame.presentation->chase->spatial) {
                xr_->world(frame.presentation->chase->spatial,frame,enhanced_->view(),enhanced_->options().scene);
            } else {xr_->panel(frame);}
        }
#endif
        bool nativePanel=false;
#ifdef STELLAR7_ENHANCED
        nativePanel=enhanced_->panelOpen();
        if(nativePanel) {enhanced_->draw(pixels_);}
#ifdef STELLAR7_OPENXR_HOST
        if(xr_&&nativePanel) {xr_->optionsPanel({pixels_.begin(),pixels_.end()});}
#endif
#endif
        if(!focused_) {lastFrame_=frame;return;}
        if(!nativePanel) {
            for(std::size_t index=0;index<pixels_.size();++index) {
                const auto color=frame.palette[frame.pixels[index]];
                pixels_[index]=0xff000000U|(static_cast<std::uint32_t>(color.red)<<16)|(static_cast<std::uint32_t>(color.green)<<8)|color.blue;
            }
        }
#ifdef STELLAR7_ENHANCED
        const EnhancedHostOperation rendering("render frame");
        const unsigned scale=enhanced_->panelOpen()?1:enhanced_->options().renderScale;
        const int width=sourceWidth*scale,height=sourceHeight*scale;
        if(gl_) {
            int outputWidth=0,outputHeight=0;SDL_GL_GetDrawableSize(window_.get(),&outputWidth,&outputHeight);
            const auto bounds=enhanced_->viewport(nullptr,window_.get());
            if(bounds.w<=0||bounds.h<=0) {lastFrame_=frame;return;}
            const GlRenderTarget target{0,bounds.x,outputHeight-bounds.y-bounds.h,bounds.w,bounds.h};
            gl_->sampling(enhanced_->options().filter==EnhancedFilter::sharp?GlSampling::nearest:GlSampling::linear);
            const bool liveWorld=frontend_->session()&&ordinary(frontend_->session()->model().phase())&&!modal();
            const bool chase=liveWorld&&enhanced_->view()==EnhancedView::chase;
            const bool spatial=liveWorld&&(chase||enhanced_->options().scene.cockpit==CockpitStyle::spatial);
            if(!enhanced_->panelOpen()&&spatial&&frame.presentation&&frame.presentation->chase&&frame.presentation->chase->spatial) {
                gl_->scene(frame.presentation->chase->spatial,frame,chase?EnhancedView::chase:EnhancedView::cockpit,
                    enhanced_->options().scene,target,scale);
            } else {
                const auto pixels=enhanced_->panelOpen()?std::vector<std::uint32_t>(pixels_.begin(),pixels_.end())
                    :renderWorldPresentation(frame,scale,next,fraction,chase?EnhancedView::chase:EnhancedView::cockpit);
                gl_->bitmap(pixels,width,height,target);
            }
            gl_->swap();lastFrame_=frame;return;
        }
        if(!texture_||textureWidth_!=width||textureHeight_!=height) {
            texture_.reset(enhancedHostCall("SDL_CreateTexture",[&] {
                return SDL_CreateTexture(renderer_.get(),SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,width,height);
            }));
            if(!texture_) {throw std::runtime_error(SDL_GetError());}
            textureWidth_=width;textureHeight_=height;
            enhancedHostTrace(("texture="+std::to_string(width)+"x"+std::to_string(height)).c_str());
        }
        if(enhanced_->panelOpen()) {
            check(enhancedHostCall("SDL_UpdateTexture",[&] {
                return SDL_UpdateTexture(texture_.get(),nullptr,pixels_.data(),width*sizeof(std::uint32_t));
            }),"SDL_UpdateTexture");
        }
        else {
            const auto pixels=enhancedHostCall("CPU world/bitmap raster",[&] {
                const auto view=frontend_->session()&&ordinary(frontend_->session()->model().phase())&&!modal()
                    ?enhanced_->view():EnhancedView::cockpit;
                return renderWorldPresentation(frame,scale,next,fraction,view);
            });
            check(enhancedHostCall("SDL_UpdateTexture",[&] {
                return SDL_UpdateTexture(texture_.get(),nullptr,pixels.data(),width*sizeof(std::uint32_t));
            }),"SDL_UpdateTexture");
        }
#else
        (void)next;(void)fraction;
        check(SDL_UpdateTexture(texture_.get(),nullptr,pixels_.data(),sourceWidth*sizeof(std::uint32_t)),"SDL_UpdateTexture");
#endif
#ifdef STELLAR7_ENHANCED
        const EnhancedHostOperation drawing("SDL_RenderClear/Copy");
#endif
        check(SDL_SetRenderDrawColor(renderer_.get(),0,0,0,255),"SDL_SetRenderDrawColor");check(SDL_RenderClear(renderer_.get()),"SDL_RenderClear");
#ifdef STELLAR7_ENHANCED
        const auto bounds=enhanced_->viewport(renderer_.get());
        if(bounds.w>0&&bounds.h>0) {check(SDL_RenderCopy(renderer_.get(),texture_.get(),nullptr,&bounds),"SDL_RenderCopy");}
#else
        sdlHost::renderTexture(renderer_.get(),texture_.get());
#endif
#ifdef STELLAR7_ENHANCED
        enhancedHostCall("SDL_RenderPresent",[&] {sdlHost::presentRenderer(renderer_.get());});
#else
        sdlHost::presentRenderer(renderer_.get());
#endif
        lastFrame_=frame;
    }
    void title() {
        const auto speed=pacer_.rate()==0?std::string("Native"):std::to_string(pacer_.rate())+" updates/s";
        const std::string text=std::string(
#ifdef STELLAR7_ENHANCED
            enhancedSafeMode()?"Stellar 7 Enhanced (Safe Mode) | F12: Options | ":"Stellar 7 Enhanced | F12: Options | "
#else
            "Stellar 7 | "
#endif
            )+(frontend_->joystickCalibrationActive()?"Joystick calibration"
            :attract()?"Attract | Escape: Return":frontend_->presentationInputActive()
                ?(frontend_->session()?"Story | Any key: Advance | Escape: Skip":"Intro | Any key: Skip")
            :frontend_->menuBackAvailable()?"Arrows: Select | Enter: Activate | Escape: Play"
            :mainMenu()?"Arrows: Select | Enter: Activate":"Arrows: Drive | Space: Fire | Escape/F10: Menu")+" | Speed: "+speed+" | +/-: Speed";
        sdlHost::setTitle(window_.get(),text.c_str());
    }
    static constexpr std::size_t workBudget=10000;
    const GameplayRunOptions& options_;
    std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window_{nullptr,SDL_DestroyWindow};
    std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)> renderer_{nullptr,SDL_DestroyRenderer};
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> texture_{nullptr,SDL_DestroyTexture};
#ifdef STELLAR7_ENHANCED
    std::unique_ptr<EnhancedSdl> enhanced_;
    std::unique_ptr<GlSdlPresenter> gl_;
#ifdef STELLAR7_OPENXR_HOST
    std::unique_ptr<OpenXrSdlHost> xr_;
    bool desktopFocused_=true;
#endif
    DestructionAudio destructionAudio_;
    int textureWidth_=sourceWidth,textureHeight_=sourceHeight;
    std::uint64_t lastSourceSample_=0,nextPresentTick_=0,displayCursor_=0,displayCursorTick_=0;
    unsigned lastDisplayRate_=0;
#endif
    sdlHost::Joystick joystick_;
    std::unique_ptr<RecoveredFrontend> frontend_;
    sdlHost::Audio audio_;
    sdlHost::Keys keys_;sdlHost::SourceKeys sourceKeys_;sdlHost::Pulse primary_,secondary_;
    std::deque<NativeTimedFrame> presentations_;
    std::deque<GameplaySystemAction> systemActions_;
    std::deque<std::uint8_t> continueScans_;
    std::vector<std::uint8_t> nameCharacters_;
    std::array<std::uint32_t,sourceWidth*sourceHeight> pixels_{};
    std::optional<IndexedScreen> lastFrame_;
    ProjectedPoint pointerPosition_{};
    NativeFramePacer pacer_;
    bool running_=true,focused_=true,pointerCancelled_=true,mouseArmed_=true;
#ifdef STELLAR7_OPENXR_HOST
    bool xrInputFocused_=false;
#endif
    bool redrawRequested_=false;
    bool cancelPending_=false,cancelThisPoll_=false;
    bool pointerCancelPending_=false,pointerCancelThisPoll_=false;
    bool presentationKey_=false,presentationEscape_=false,calibrationRequested_=false,introSkipRequested_=false,pauseEscapeHeld_=false;
};
} //namespace

int runSdlPlayableNativeEntry(const GameplayAssets& assets,const NativeStartupAssets& startup,
    FrameCopyProfile profile,const GameplayRunOptions& options) {
    Runtime runtime;SdlGameplay host(assets,startup,profile,options);return host.run();
}
} //namespace stellar7
