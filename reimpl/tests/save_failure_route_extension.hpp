#pragma once
#include "playable_campaign_comparison.hpp"
#include "stellar7/recovered_frontend.hpp"
#include "stellar7/highscore_render.hpp"
#include "stellar7/ui_text_work.hpp"
#include <algorithm>

namespace stellar7::test {
class SaveFailureTestStore final:public HighscoreStore {
public:
    HighscoreSaveResult saveAndReload(const HighscoreTable& table) override {
        ++calls;attempted=encodeHighscoreTable(table);
        if(status!=HighscoreSaveStatus::saved) {return {status,std::nullopt};}
        persisted=attempted;return {status,decodeHighscoreTable(persisted)};
    }
    HighscoreSaveStatus status=HighscoreSaveStatus::saved;
    unsigned calls=0;
    std::vector<std::uint8_t> attempted,persisted;
};

inline void requireFailedRankNotice(const NativeGameplaySession& session,const GameplayAssets& assets) {
    requireCampaignComparison(session.model().phase()==GameplayPhase::highscoreHall&&session.highscoreTable(),
        "failed rank did not preserve its resident earned score and enter Hall");
    constexpr std::string_view message="SCORE NOT SAVED";
    const auto roles=session.pageObservation();IndexedPages expected;
    expected.apply(highscoreHallPageEffects(assets.artGroups.at(0),assets.titleFont,*session.highscoreTable(),roles.back,roles.front));
    const auto width=uiTextWidthOperations(assets.titleFont,UiFontLayout::fixed,message).width;
    expected.apply(uiTextOperations(assets.titleFont,UiFontLayout::fixed,message,
        {static_cast<std::int16_t>((320-width)/2),155},239,roles.front,{0,319,0,199}).effects);
    const auto pixels=expected.snapshot(roles.front);
    for(std::size_t row=155;row<161;++row) {
        requireCampaignComparison(std::equal(pixels.begin()+row*320,pixels.begin()+(row+1)*320,
            session.frame().pixels.begin()+row*320),"failed rank did not visibly disclose the unsaved score");
    }
}

//Append after an actual earned title; does not construct or seed a title session.
inline void appendFailedPrefsTitle(RecoveredFrontend& left,RecoveredFrontend& right,
    CampaignBudgetComparison& comparison,SaveFailureTestStore& leftStore,SaveFailureTestStore& rightStore) {
    const auto* owner=left.session();const auto* twin=right.session();
    requireCampaignComparison(owner&&twin&&owner->model().phase()==GameplayPhase::nativeMenu
        &&owner->menu()&&owner->menu()->selection()==0&&owner->highscoreTable(),
        "save recovery extension requires actual retained earned title/BEGIN");
    const auto table=encodeHighscoreTable(*owner->highscoreTable());
    const auto score=owner->model().score();const auto world=owner->model().level2LoopTick();
    const auto rng=owner->model().level2Rng();
    const auto* synth=&owner->synth();const auto* twinSynth=&twin->synth();
    requireCampaignComparison(owner->synth().driverState().sequenceSelected,
        "save title extension requires the supported MUSIC On profile");
    const auto step=[&](const GameplayInput input=GameplayInput{}) {
        const auto a=left.advanceGameplay(input),b=right.advanceGameplay(input);
        comparison.compare(*owner,*twin,a,b,CampaignComparisonStage::boundary);
        requireCampaignComparison(left.session()==owner&&right.session()==twin
            && &owner->synth()==synth&& &twin->synth()==twinSynth&&!a.exitRequested&&!b.exitRequested
            &&owner->model().score()==score&&owner->model().level2LoopTick()==world
            &&encodeHighscoreTable(*owner->highscoreTable())==table
            &&twin->highscoreTable()&&encodeHighscoreTable(*twin->highscoreTable())==table,
            "failed title save changed owners/world/earned resident score table or exited");
        compareCampaignRng(owner->model().level2Rng(),rng,CampaignComparisonStage::boundary);
    };
    const auto pulse=[&](const GameplayInput input) {step(input);step();};
    GameplayInput down;down.reverse=true;GameplayInput up;up.forward=true;
    GameplayInput confirm;confirm.confirm=true;
    pulse(down);pulse(down);pulse(confirm); //PREFS.
    pulse(up);GameplayInput leftKey;leftKey.left=true;pulse(leftKey);pulse(up); //STORY -> LEVEL -> MUSIC.
    pulse(confirm);
    requireCampaignComparison(!owner->synth().driverState().sequenceSelected,
        "title MUSIC edit did not suspend current publication before failure");
    pulse(down);pulse(down);
    const auto calls=leftStore.calls;
    leftStore.status=rightStore.status=HighscoreSaveStatus::writeFailed;
    pulse(confirm);
    requireCampaignComparison(leftStore.calls==calls+1&&rightStore.calls==leftStore.calls
        &&left.menuBackAvailable()&&right.menuBackAvailable(),"failed title save did not expose modal undo");
    GameplayInput undo;undo.exit=true;step(undo);step();
    requireCampaignComparison(!left.menuBackAvailable()&&!right.menuBackAvailable()
        &&owner->menu()&&owner->menu()->selection()==0&&owner->synth().driverState().sequenceSelected,
        "title failure undo did not restore MUSIC publication and retained BEGIN");
    leftStore.status=rightStore.status=HighscoreSaveStatus::saved;
}
}
