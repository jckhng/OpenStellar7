#define main unusedJoystickUiUnitMain
#include "playable_joystick_calibration_test.cpp"
#undef main

namespace {
void checkpoint(const GameplayAssets& assets) {
    auto configuration=*assets.cockpitSettings;configuration.options[1]=1;
    auto scores=*assets.highscoreTable;scores.setConfiguration(configuration);MemoryStore store;
    PlayableJoystick owner;REQUIRE(owner.enable(center));
    PlayableJoystickCalibration first(owner);stableSample(first,upperLeft);stableSample(first,lowerRight);stableSample(first,center);
    const auto oldCalibration=owner.calibration();
    PlayableMenuDialog dialog(PlayableMenuPage::preferences,assets,NativePointerCapability::unavailable,
        NativeDetailCapability::fullRange,features(),PreferenceLayout::title,{},&owner);
    dialog.advance({},configuration,scores,&store,&center);REQUIRE(dialog.calibrationAvailable());
    const auto persistedBefore=encodeHighscoreTable(scores);
    PlayableJoystickCalibration child(owner);
    stableSample(child,{true,10,18,0});stableSample(child,{true,240,238,0});stableSample(child,{true,121,123,0});
    REQUIRE(child.phase()==Phase::complete&&axes(owner.calibration().lowerRight)!=axes(oldCalibration.lowerRight));
    auto candidate=scores;candidate.setConfiguration(configuration);
    const auto saved=store.saveAndReload(candidate);REQUIRE(saved.status==HighscoreSaveStatus::saved&&saved.reloaded);
    scores=*saved.reloaded;REQUIRE(store.bytes==persistedBefore); //FAME never serializes calibration words.
    REQUIRE(!dialog.menuCalibrationSaved(configuration));
    const auto savedOwner=owner;
    const auto pulse=[&](NativeMenuInput input) {dialog.advance(input,configuration,scores,&store,&center);return dialog.advance({},configuration,scores,&store,&center);};
    for(unsigned i=0;i<3;++i) {pulse({true,false,false});}pulse({false,false,true});
    REQUIRE(!owner.enabled()&&configuration.options[1]==0);
    for(unsigned i=0;i<3;++i) {pulse({false,true,false});}
    store.fail=true;REQUIRE(pulse({false,false,true})==MenuDialogResult::pending&&store.calls==2);
    REQUIRE(store.bytes==persistedBefore);
    NativeMenuInput escape;escape.cancel=true;
    REQUIRE(dialog.advance(escape,configuration,scores,&store,&center)==MenuDialogResult::closed&&configuration.options[1]==1);
    same(owner,savedOwner); //Must retain successful CHILD calibration, not old menu-entry calibration.

    MemoryStore noticeStore;noticeStore.fail=true;
    PlayableMenuDialog noticeDialog(PlayableMenuPage::preferences,assets,NativePointerCapability::unavailable,
        NativeDetailCapability::fullRange,features(),PreferenceLayout::title,{},&owner);
    NativeMenuInput confirm;confirm.confirm=true;
    noticeDialog.advance(confirm,configuration,scores,&noticeStore,&center);
    REQUIRE(noticeDialog.advance({},configuration,scores,&noticeStore,&center)==MenuDialogResult::pending&&noticeDialog.backAvailable());
    noticeStore.fail=false;auto noticeCandidate=scores;noticeCandidate.setConfiguration(configuration);
    REQUIRE(noticeStore.saveAndReload(noticeCandidate).status==HighscoreSaveStatus::saved);
    REQUIRE(!noticeDialog.menuCalibrationSaved(configuration)&&!noticeDialog.backAvailable());
    PlayableMenuDialog cleanDialog(PlayableMenuPage::preferences,assets,NativePointerCapability::unavailable,
        NativeDetailCapability::fullRange,features(),PreferenceLayout::title,{},&owner);
    IndexedPages cleared,expected;
    for(const auto page:{std::uint16_t{0xa800},std::uint16_t{0xa000}}) {
        for(int y=0;y<200;++y) {for(int x=0;x<320;++x) {
            const ProjectedPoint point{static_cast<std::int16_t>(x),static_cast<std::int16_t>(y)};
            cleared.write(page,point,0);expected.write(page,point,0);
        }}
    }
    noticeDialog.paint(cleared,assets,configuration,scores,0xa800,0xa000);
    cleanDialog.paint(expected,assets,configuration,scores,0xa800,0xa000);
    REQUIRE(cleared.snapshot(0xa000)==expected.snapshot(0xa000)); //Successful checkpoint removes stale SAVE FAILED footer.

    PlayableMenuDialog briefing(PlayableMenuPage::briefing,assets,NativePointerCapability::unavailable,
        NativeDetailCapability::fullRange,features(),PreferenceLayout::title,{},&owner);
    const auto selected=briefing.pointerPosition();
    REQUIRE(briefing.menuCalibrationSaved(configuration)==std::optional<std::uint16_t>{15000});
    REQUIRE(briefing.pointerPosition()==selected);
    PlayableMenuDialog live(PlayableMenuPage::preferences,assets,NativePointerCapability::unavailable,
        NativeDetailCapability::fullRange,features(),PreferenceLayout::inGame,{},&owner);
    REQUIRE(!live.calibrationAvailable());
    PlayableMenuDialog pending(PlayableMenuPage::preferences,assets,NativePointerCapability::unavailable,
        NativeDetailCapability::fullRange,features(),PreferenceLayout::title,{},&owner);
    NativeMenuInput press;press.confirm=true;pending.advance(press,configuration,scores,nullptr,&center);
    REQUIRE(!pending.calibrationAvailable()); //Released widget action precedes source5E9C callback.
}
}
int main(int argc,char** argv) {
    try {if(argc!=2) {throw std::invalid_argument("requires actual resource directory");}checkpoint(naturalRoute::assets(argv[1]));
        std::cout<<"Successful3010 child calibration rebases later failed-save undo; FAME unchanged; BriefingFF15000 and widget/F10 gates PASS\n";}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
