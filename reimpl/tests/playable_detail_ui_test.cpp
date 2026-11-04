#include "playable_menu_dialog.hpp"
#include "playable_menu_pages.hpp"
#include "playable_startup_resources.hpp"

#include <iostream>
#include <stdexcept>

namespace {
using namespace stellar7;
void require(const bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
NativeMenuInput mouse(const ProjectedPoint point,const bool held) {
    NativeMenuInput input;input.pointer=NativePointerInput{point,held,false};return input;
}
void expectBoundary(PlayableMenuDialog& dialog,const GameplayAssets& assets,const CockpitSettings& settings,
    const HighscoreTable& scores,const PreferenceControl selected) {
    constexpr std::uint16_t back=0xa000,front=0xa800;
    IndexedPages actual,expected;
    dialog.paint(actual,assets,settings,scores,back,front);
    expected.apply(preferencesPageEffects(assets.artGroups.at(0),assets.titleFont,assets.scoreFont,settings,
        selected,PreferenceControl::detail,"DETAIL 51-81 ONLY",back));
    expected.apply(std::vector<IndexedPageEffect>{RadarPageCopy{back,front,{0,0},320,200}});
    paintPlayableMenuPointer(expected,assets.artGroups.at(0).at(0),dialog.pointerPosition(),back,front);
    require(actual.snapshot(front)==expected.snapshot(front),"rejected detail domain did not visibly disclose its boundary");
}
void run(const std::filesystem::path& root) {
    const auto assets=naturalRoute::assets(root);
    CockpitSettings settings{82,{1,0,1,1,0,0}};
    HighscoreTable scores({},{});scores.setConfiguration(settings);
    PlayableMenuDialog locked(PlayableMenuPage::preferences,assets);
    locked.advance({false,true,false},settings,scores,nullptr);locked.advance({},settings,scores,nullptr);
    locked.advance({false,false,true},settings,scores,nullptr);locked.advance({},settings,scores,nullptr);
    require(settings.viewSize==82,"default unavailable detail profile admitted mutation");

    PlayableMenuDialog keyboard(PlayableMenuPage::preferences,assets,NativePointerCapability::unavailable,
        NativeDetailCapability::bounded51To81);
    const auto key=[&](const NativeMenuInput input) {return keyboard.advance(input,settings,scores,nullptr);};
    key({false,true,false});key({}); //EXIT -> detail uses8468, not generic5C14 center.
    require(keyboard.pointerPosition().x==201&&keyboard.pointerPosition().y==46&&settings.viewSize==82,
        "loaded82 focus changed value or used generic rectangle center");
    NativeMenuInput left;left.left=true;
    key(left);require(settings.viewSize==80,"source detail left did not move by two");
    key(left);require(settings.viewSize==78,"held detail key stopped repeating");
    NativeMenuInput both;both.left=both.right=true;
    key(both);require(settings.viewSize==78,"source independent left/right adjustments did not cancel");key({});
    NativeMenuInput right;right.right=true;
    key(right);require(settings.viewSize==80,"source detail right did not move by two");
    key(right);require(settings.viewSize==81,"interactive detail maximum did not clamp at81");key({});
    for(unsigned step=0;step<15;++step) {key(left);}
    require(settings.viewSize==51,"supported keyboard detail range could not reach51");
    key(left);require(settings.viewSize==51,"unsupported keyboard49 mutated canonical detail");
    expectBoundary(keyboard,assets,settings,scores,PreferenceControl::detail);key({});
    key({true,false,false});key({});
    require(keyboard.pointerPosition().x==163&&keyboard.pointerPosition().y==170,
        "detail release did not rearm source navigation back to EXIT");

    settings.viewSize=82;
    PlayableMenuDialog pointer(PlayableMenuPage::preferences,assets,NativePointerCapability::portablePointer,
        NativeDetailCapability::bounded51To81);
    const auto poll=[&](const NativeMenuInput input) {return pointer.advance(input,settings,scores,nullptr);};
    poll(mouse({189,46},false));require(settings.viewSize==82,"detail hover changed loaded82");
    poll(mouse({189,46},true));require(settings.viewSize==70,"detail press did not start source held drag immediately");
    poll(mouse({170,190},true));require(settings.viewSize==51&&pointer.pointerPosition().y==47,
        "captured slider drag re-hit widgets or lost source fixedY47");
    poll(mouse({169,46},true));require(settings.viewSize==51,"unsupported pointer50 mutated detail");
    expectBoundary(pointer,assets,settings,scores,PreferenceControl::exit);
    poll(mouse({319,46},true));require(settings.viewSize==81&&pointer.pointerPosition().x==200,
        "held slider drag lost source interactive maximum or pointer clamp");
    require(poll(mouse({163,170},false))==MenuDialogResult::pending&&settings.viewSize==81,
        "slider release outside activated EXIT as a checkbox");
    poll(mouse({189,46},true));
    NativeMenuInput cancel;cancel.confirm=cancel.left=cancel.up=true;
    cancel.pointer=NativePointerInput{{0,0},true,true,NativePointerPhase::cancel};
    poll(cancel);poll(mouse({163,170},false));
    require(settings.viewSize==70,"cancelled slider consumed coordinates/keys or mutated on later release");
    std::cout<<"Typed detail focus/held keys/drag/clamp/rejection/visible boundary/cancel/default lock PASS\n";
}
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {run(argv[1]);return 0;}catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
