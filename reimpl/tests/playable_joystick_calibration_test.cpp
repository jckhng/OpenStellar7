#include "playable_joystick_calibration.hpp"
#include "playable_menu_dialog.hpp"
#include "playable_startup_resources.hpp"
#include "stellar7/ui_text_work.hpp"

#include <iostream>
#include <tuple>

namespace {
using namespace stellar7;
void require(bool condition,const char* expression,int line) {
    if(!condition) {throw std::runtime_error("joystick UI line="+std::to_string(line)+" expression="+expression);}
}
#define REQUIRE(...) require(bool((__VA_ARGS__)),#__VA_ARGS__,__LINE__)
template<class F> void rejects(F action) {
    bool rejected=false;try {action();} catch(const std::exception&) {rejected=true;}REQUIRE(rejected);
}
const JoystickCounterSample center{true,127,127,0};
const JoystickCounterSample upperLeft{true,24,35,0};
const JoystickCounterSample lowerRight{true,231,218,0};
using Activation=JoystickCalibrationActivation;
using Wait=JoystickCalibrationWait;
using Phase=JoystickCalibrationPhase;
auto axes(const JoystickAxes value) {return std::pair{value.x,value.y};}
void same(PlayableJoystick left,PlayableJoystick right) {
    REQUIRE(left.enabled()==right.enabled()&&left.calibrationPoint()==right.calibrationPoint());
    const auto& a=left.calibration();const auto& b=right.calibration();
    REQUIRE(axes(a.center)==axes(b.center)&&axes(a.upperLeft)==axes(b.upperLeft)&&axes(a.lowerRight)==axes(b.lowerRight));
    const auto x=left.poll(lowerRight),y=right.poll(lowerRight);
    REQUIRE(x.connected==y.connected&&axes(x.normalized)==axes(y.normalized)&&x.directions==y.directions&&x.buttons==y.buttons);
    REQUIRE(left.advanceButtons(0)==right.advanceButtons(0)); //Retained41A2 release history.
}
void stableSample(PlayableJoystickCalibration& ui,const JoystickCounterSample sample) {
    auto step=ui.advance(sample,Activation::released);REQUIRE(step.phase==Phase::waitingPress&&step.waitRawIrqs==0);
    step=ui.advance(center,Activation::held);REQUIRE(step.phase==Phase::waitingRelease&&step.waitRawIrqs==0);
    step=ui.advance(sample,Activation::held);REQUIRE(step.phase==Phase::waitingRelease);
    step=ui.advance(sample,Activation::released);REQUIRE(step.phase==Phase::waitingDebounce&&step.waitRawIrqs==10);
    step=ui.advance(center,Activation::released);REQUIRE(step.phase==Phase::waitingDebounce&&step.waitRawIrqs==0);
    step=ui.advance(sample,Activation::released,Wait::returned);REQUIRE(step.promptChanged&&step.waitRawIrqs==0);
}
void stateMachine() {
    PlayableJoystick disabled;rejects([&] {PlayableJoystickCalibration ui(disabled);});
    PlayableJoystick owner;REQUIRE(owner.enable(center));auto expected=owner;expected.beginCalibration();
    PlayableJoystickCalibration ui(owner);REQUIRE(ui.point()==JoystickCalibrationPoint::upperLeft);
    rejects([&] {ui.advance(center,Activation::released,Wait::returned);});
    ui.advance(upperLeft,Activation::held);
    auto step=ui.advance(upperLeft,Activation::released);REQUIRE(step.waitRawIrqs==10);
    step=ui.advance(lowerRight,Activation::held,Wait::returned); //88E9 bounce returns to88D4.
    REQUIRE(step.phase==Phase::waitingRelease&&ui.point()==JoystickCalibrationPoint::upperLeft);
    step=ui.advance(upperLeft,Activation::released);REQUIRE(step.waitRawIrqs==10);
    step=ui.advance(upperLeft,Activation::released,Wait::returned);
    REQUIRE(step.point==JoystickCalibrationPoint::lowerRight&&step.promptChanged);
    expected.captureCalibration(upperLeft);same(owner,expected);
    stableSample(ui,lowerRight);expected.captureCalibration(lowerRight);same(owner,expected);
    REQUIRE(ui.point()==JoystickCalibrationPoint::center);
    stableSample(ui,center);expected.captureCalibration(center);same(owner,expected);
    REQUIRE(ui.phase()==Phase::complete&&ui.point()==JoystickCalibrationPoint::complete);
    rejects([&] {ui.advance(center,Activation::released);});
    owner.advanceButtons(2);const auto before=owner;
    PlayableJoystickCalibration retry(owner);stableSample(retry,upperLeft);
    step=retry.advance({},Activation::released);REQUIRE(step.phase==Phase::unavailable);same(owner,before);
    rejects([&] {retry.advance(center,Activation::released);});
    PlayableJoystick narrow;REQUIRE(narrow.enable(center));const auto beforeNarrow=narrow;
    PlayableJoystickCalibration rejected(narrow);
    stableSample(rejected,{true,118,118,0});stableSample(rejected,{true,136,136,0});stableSample(rejected,center);
    REQUIRE(rejected.phase()==Phase::rejected);same(narrow,beforeNarrow);
    rejects([&] {rejected.advance(center,Activation::released);});
}
class MemoryStore final:public HighscoreStore {
public:
    HighscoreSaveResult saveAndReload(const HighscoreTable& table) override {
        ++calls;if(fail) {return {HighscoreSaveStatus::writeFailed,std::nullopt};}
        bytes=encodeHighscoreTable(table);return {HighscoreSaveStatus::saved,decodeHighscoreTable(bytes)};
    }
    bool fail=false;unsigned calls=0;std::vector<std::uint8_t> bytes;
};
NativeMenuFeatures features() {
    return static_cast<NativeMenuFeatures>(static_cast<std::uint8_t>(NativeMenuFeatures::fullGraphicDetail)
        |static_cast<std::uint8_t>(NativeMenuFeatures::joystickControls));
}
void menu(const GameplayAssets& assets,const PreferenceLayout layout) {
    auto settings=*assets.cockpitSettings;settings.options[1]=0;
    auto scores=*assets.highscoreTable;scores.setConfiguration(settings);MemoryStore store;
    PlayableJoystick owner;PlayableMenuDialog dialog(PlayableMenuPage::preferences,assets,
        NativePointerCapability::unavailable,NativeDetailCapability::fullRange,features(),layout,{},&owner);
    const auto pulse=[&](NativeMenuInput input) {
        dialog.advance(input,settings,scores,&store,&center);return dialog.advance({},settings,scores,&store,&center);
    };
    for(unsigned i=0;i<3;++i) {pulse({true,false,false});} //EXIT/PLAY->STORY/SOUNDS->SOUNDS/MUSIC->JOYSTICK.
    pulse({false,false,true});REQUIRE(owner.enabled()&&settings.options[1]==1);
    PlayableJoystickCalibration calibrate(owner);stableSample(calibrate,upperLeft);stableSample(calibrate,lowerRight);stableSample(calibrate,center);
    const auto calibrated=owner;
    for(unsigned i=0;i<3;++i) {pulse({false,true,false});}
    REQUIRE(pulse({false,false,true})==MenuDialogResult::preferencesSaved&&store.calls==1);
    REQUIRE(decodeCockpitSettings(std::vector<std::uint8_t>(store.bytes.begin(),store.bytes.begin()+highscoreHeaderBytes)).options[1]==1);
    PlayableMenuDialog undo(PlayableMenuPage::preferences,assets,NativePointerCapability::unavailable,
        NativeDetailCapability::fullRange,features(),layout,{},&owner);store.fail=true;
    const auto poll=[&](NativeMenuInput input) {return undo.advance(input,settings,scores,&store,&center);};
    const auto undoPulse=[&](NativeMenuInput input) {poll(input);return poll({});};
    owner.advanceButtons(2);const auto beforeUndo=owner;
    for(unsigned i=0;i<3;++i) {undoPulse({true,false,false});}
    undoPulse({false,false,true});REQUIRE(!owner.enabled()&&settings.options[1]==0);
    for(unsigned i=0;i<3;++i) {undoPulse({false,true,false});}
    REQUIRE(undoPulse({false,false,true})==MenuDialogResult::pending&&store.calls==2&&undo.backAvailable());
    NativeMenuInput escape;escape.cancel=true;
    REQUIRE(poll(escape)==MenuDialogResult::closed&&settings.options[1]==1);same(owner,beforeUndo);
    REQUIRE(axes(owner.calibration().lowerRight)==axes(calibrated.calibration().lowerRight));
    PlayableMenuDialog off(PlayableMenuPage::preferences,assets,NativePointerCapability::unavailable,
        NativeDetailCapability::fullRange,features(),layout,{},&owner);store.fail=false;
    const auto offPulse=[&](NativeMenuInput input) {
        off.advance(input,settings,scores,&store,&center);return off.advance({},settings,scores,&store,&center);
    };
    for(unsigned i=0;i<3;++i) {offPulse({true,false,false});}offPulse({false,false,true});
    for(unsigned i=0;i<3;++i) {offPulse({false,true,false});}
    REQUIRE(offPulse({false,false,true})==MenuDialogResult::preferencesSaved&&store.calls==3&&!owner.enabled());
    REQUIRE(decodeCockpitSettings(std::vector<std::uint8_t>(store.bytes.begin(),store.bytes.begin()+highscoreHeaderBytes)).options[1]==0);
}
void absenceAndLegacy(const GameplayAssets& assets) {
    for(const auto admitted:{false,true}) {
        auto settings=*assets.cockpitSettings;settings.options[1]=0;auto scores=*assets.highscoreTable;PlayableJoystick owner;
        PlayableMenuDialog dialog(PlayableMenuPage::preferences,assets,NativePointerCapability::unavailable,
            NativeDetailCapability::fullRange,admitted?features():NativeMenuFeatures::legacy,PreferenceLayout::title,{},&owner);
        const JoystickCounterSample absent{};
        const auto pulse=[&](NativeMenuInput input) {dialog.advance(input,settings,scores,nullptr,&absent);dialog.advance({},settings,scores,nullptr,&absent);};
        for(unsigned i=0;i<3;++i) {pulse({true,false,false});}pulse({false,false,true});
        REQUIRE(settings.options[1]==0&&!owner.enabled());
    }
}
void graphics(const GameplayAssets& assets) {
    constexpr std::uint16_t page=0xa800;
    constexpr auto discriminator=static_cast<unsigned char>('x');
    REQUIRE(discriminator<assets.titleFont.firstCharacter
        ||static_cast<std::size_t>(discriminator-assets.titleFont.firstCharacter)>=assets.titleFont.glyphs.size());
    rejects([&] {uiTextWidthOperations(assets.titleFont,UiFontLayout::fixed,"x");});
    IndexedPages pixels;
    for(int y=0;y<200;++y) {for(int x=0;x<320;++x) {pixels.write(page,{static_cast<std::int16_t>(x),static_cast<std::int16_t>(y)},0);}}
    std::vector<std::uint8_t> previous;
    for(const auto point:{JoystickCalibrationPoint::upperLeft,JoystickCalibrationPoint::lowerRight,JoystickCalibrationPoint::center}) {
        pixels.apply(joystickCalibrationPageEffects(assets.artGroups.at(0),assets.titleFont,assets.scoreFont,point,page));
        const auto drawn=pixels.snapshot(page);REQUIRE(drawn!=previous&&drawn[0]==0&&drawn[199*320+319]==0);
        previous=drawn;
    }
    pixels.apply(joystickDisabledPageEffects(assets.artGroups.at(0),assets.titleFont,page));REQUIRE(pixels.snapshot(page)!=previous);
    previous=pixels.snapshot(page);
    pixels.apply(joystickCalibrationRejectedPageEffects(assets.artGroups.at(0),assets.titleFont,assets.scoreFont,page));
    REQUIRE(pixels.snapshot(page)!=previous);
    rejects([&] {joystickCalibrationPageEffects({},assets.titleFont,assets.scoreFont,JoystickCalibrationPoint::upperLeft,page);});
    rejects([&] {joystickCalibrationPageEffects(assets.artGroups.at(0),assets.titleFont,assets.scoreFont,JoystickCalibrationPoint::complete,page);});
    rejects([&] {joystickCalibrationPageEffects(assets.artGroups.at(0),{},assets.scoreFont,JoystickCalibrationPoint::upperLeft,page);});
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) {throw std::invalid_argument("requires actual resource directory");}
        stateMachine();const auto assets=naturalRoute::assets(argv[1]);
        menu(assets,PreferenceLayout::title);menu(assets,PreferenceLayout::inGame);absenceAndLegacy(assets);graphics(assets);
        std::cout<<"Joystick calibration88C4/88F8 prompts, stable10IRQ samples, recalibration rollback, title/live On-Off save/undo and absent/legacy gates PASS\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
