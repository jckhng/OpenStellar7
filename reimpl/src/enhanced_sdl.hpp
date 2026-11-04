#pragma once
#include "enhanced_options.hpp"
#include "enhanced_view.hpp"
#include "sdl_input.hpp"
#include "controller_input.hpp"
#include "stellar7/resource_codec.hpp"
#include <utility>
#include <optional>

namespace stellar7 {
class EnhancedSdl {
public:
    EnhancedSdl(std::filesystem::path path,const BitmapFont& font,unsigned gameRate);
    ~EnhancedSdl();
    const EnhancedOptions& options() const {return options_;}
    EnhancedView view() const {return view_;}
    void attach(SDL_Window* window);
    SDL_Rect viewport(SDL_Renderer* renderer,SDL_Window* window=nullptr) const;
    void pointer(SDL_Window* window,SDL_Renderer* renderer,float& x,float& y) const;
    void warp(SDL_Window* window,SDL_Renderer* renderer,float x,float y) const;
    bool key(const SDL_KeyboardEvent& event,SDL_Window* window);
    bool event(const SDL_Event& event,bool focused,SDL_Window* window,SDL_Renderer* renderer=nullptr);
    void servicePanel(SDL_Window* window,bool focused);
    void poll(bool focused);
    void externalInput(const ControllerInput& input,bool focused,SDL_Window* window);
    bool recenterPressed() const {return external_.pressed(ControllerButton::recenter);}
    void cancel();
    bool panelOpen() const {return panel_;}
    bool held(EnhancedAction action) const;
    bool pressed(EnhancedAction action) const;
    bool confirm() const;
    bool back() const;
    bool uiDirection(EnhancedAction action) const;
    SDL_Scancode binding(EnhancedAction action) const;
    bool takeTextureChange();
    void draw(std::array<std::uint32_t,320*200>& pixels) const;
    void rate(unsigned value,SDL_Window* window);
    void save(SDL_Window* window);
    unsigned playbackVolume() const {return options_.muted?0:static_cast<unsigned>(options_.volume);}
private:
    bool safeMode_=false;
    ControllerState external_;
    enum class PanelAction { up,down,left,right,activate,back,clear };
    void panelAction(PanelAction action,SDL_Window* window);
    bool panelMouse(const SDL_Event& event,bool focused,SDL_Window* window,SDL_Renderer* renderer);
    void togglePanel(SDL_Window* window);
    void clearGameplay();
    bool backAssigned() const;
    bool viewHotkeyAvailable() const;
    void repeat(PanelAction action,SDL_Window* window);
    void validateBindings();
    void openController();
    void closeController();
    void fullscreen(SDL_Window* window,bool enabled);
    void adjust(int direction,SDL_Window* window);
    void captureKey(SDL_Scancode key);
    void captureButton(SDL_GameControllerButton button);
    void filterHint() const;
    EnhancedOptionsStore store_;
    EnhancedOptions options_;
    EnhancedView view_=EnhancedView::cockpit;
    const BitmapFont& font_;
    std::array<SDL_Scancode,enhancedActionCount> keys_{};
    std::array<SDL_GameControllerButton,enhancedActionCount> bindings_{};
    std::array<sdlHost::Pulse,SDL_CONTROLLER_BUTTON_MAX> buttons_;
    std::array<bool,enhancedActionCount> held_{},pressed_{};
    SDL_GameController* controller_=nullptr;
    bool controllerScanPending_=true;
    int axisX_=0,axisY_=0;
    std::size_t selected_=0;
    std::optional<PanelAction> repeating_;
    std::optional<PanelAction> externalRepeating_;
    std::optional<SDL_GameControllerButton> capturePendingButton_;
    Uint64 repeatAt_=0;
    Uint64 externalRepeatAt_=0;
    bool panelAwaitRelease_=true,backReceived_=false,startReceived_=false,chordHeld_=false;
    bool panelMouseArmed_=false;
    bool panel_=false,capturing_=false,armed_=false,textureChanged_=false,loaded_=true,dirty_=false;
    std::string message_;
};
} //namespace stellar7
