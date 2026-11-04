#include "playable_joystick.hpp"
#include "playable_menu_dialog.hpp"

#include <iostream>
#include <stdexcept>

namespace {
using namespace stellar7;
void require(const bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
class Store final:public HighscoreStore {
public:
    HighscoreSaveResult saveAndReload(const HighscoreTable& table) override {
        ++calls;return {HighscoreSaveStatus::saved,table};
    }
    unsigned calls=0;
};
void run() {
    const NativeJoystickSample cancelled{true,32768,32768,3,NativeJoystickPhase::cancel};
    const auto absent=joystickCounterSample(cancelled);
    require(!absent.connected&&absent.buttons==0,"focus cancel became a sampled release/press");
    bool rejected=false;
    try {auto bad=cancelled;bad.phase=static_cast<NativeJoystickPhase>(99);joystickCounterSample(bad);}
    catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"unknown joystick phase was admitted");

    GameplayAssets assets{};CockpitSettings settings{81,{0,0,1,1,0,0}};
    HighscoreTable scores({},{});scores.setConfiguration(settings);Store store;
    PlayableMenuDialog dialog(PlayableMenuPage::preferences,assets);
    NativeMenuInput press;press.confirm=true;
    require(dialog.advance(press,settings,scores,&store)==MenuDialogResult::pending,"press closed a widget");
    dialog.cancelInteraction();
    require(dialog.advance({},settings,scores,&store)==MenuDialogResult::pending&&store.calls==0,
        "cancelled joystick press activated EXIT on apparent release");
    dialog.advance(press,settings,scores,&store);
    require(dialog.advance({},settings,scores,&store)==MenuDialogResult::preferencesSaved&&store.calls==1,
        "fresh press/release did not work after cancellation");
    std::cout<<"Native joystick cancel phase and pending dialog activation cancellation PASS\n";
}
}
int main() {
    try {run();return 0;}catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
