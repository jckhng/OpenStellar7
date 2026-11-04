#pragma once
#include "../src/playable_menu_pages.hpp"
#include "../src/briefing_pages.hpp"

namespace stellar7::test {
enum class ReturnedMenuDirection { up,down,left,right };

inline void appendReturnedTitleMenu(RecoveredFrontend& left,RecoveredFrontend& right,
    CampaignBudgetComparison& comparison,const GameplayAssets& data) {
    const auto* owner=left.session();const auto* twin=right.session();
    requireCampaignComparison(owner&&twin&&owner->model().phase()==GameplayPhase::nativeMenu,
        "returned-menu extension requires the completed actual startup route");
    requireCampaignComparison(owner->highscoreTable()&&twin->highscoreTable()&&data.cockpitSettings,
        "returned-menu extension requires its actual resident settings/table");
    const auto* synth=&owner->synth();const auto* twinSynth=&twin->synth();
    const auto frozen=owner->model();
    const auto scores=*owner->highscoreTable();
    unsigned polls=0;
    const auto advance=[&](const GameplayInput& input) {
        const auto first=left.elapsedSamples();
        const auto a=left.advanceGameplay(input),b=right.advanceGameplay(input);
        requireCampaignComparison(left.session()==owner&&right.session()==twin
            && &owner->synth()==synth&& &twin->synth()==twinSynth,
            "returned menu replaced the actual startup session/synth");
        requireCampaignComparison(a.firstSample==first&&first+a.samples.size()==left.elapsedSamples()
            && !a.exitRequested&&!b.exitRequested,"returned menu lost PCM or exited unexpectedly");
        comparison.compare(*owner,*twin,a,b,CampaignComparisonStage::frame);
        GameplayModelTestAccess::compare(owner->model(),twin->model());
        ++polls;
    };
    const auto frozenMenu=[&]() {
        const auto& model=owner->model();
        requireCampaignComparison(model.phase()==GameplayPhase::nativeMenu
            && model.score()==frozen.score()&&model.playerArmor()==frozen.playerArmor()
            && model.remainingContinues()==frozen.remainingContinues()
            && model.level2LoopTick()==frozen.level2LoopTick(),"submenu changed retained campaign state");
        compareCampaignRng(model.level2Rng(),frozen.level2Rng(),CampaignComparisonStage::boundary);
        GameplayModelTestAccess::compare(model,frozen);
    };
    const auto step=[&](const GameplayInput& input=GameplayInput{}) {advance(input);frozenMenu();};
    const auto boundary=[&]() {
        NativeSessionDelivery empty{};empty.firstSample=left.elapsedSamples();
        comparison.compare(*owner,*twin,empty,empty,CampaignComparisonStage::boundary);
    };
    const auto main=[&](const std::size_t selection) {
        requireCampaignComparison(owner->menu()&&owner->menu()->selection()==selection
            && !left.menuBackAvailable()&&!right.menuBackAvailable(),"returned main selection/back owner drifted");
        const auto roles=owner->pageObservation();IndexedPages expected;
        paintPlayableMainMenu(expected,data.artGroups.at(0),data.titleFont,*owner->menu(),std::nullopt,
            roles.back,roles.front);
        const auto wanted=expected.snapshot(roles.front);
        if(owner->frame().pixels!=wanted) {
            const auto mismatch=std::mismatch(owner->frame().pixels.begin(),owner->frame().pixels.end(),wanted.begin());
            const auto offset=static_cast<std::size_t>(mismatch.first-owner->frame().pixels.begin());
            throw std::runtime_error("returned main mismatch polls="+std::to_string(polls)+" selection="
                +std::to_string(selection)+" pixel="+std::to_string(offset%320)+","+std::to_string(offset/320)
                +" actual="+std::to_string(*mismatch.first)+" expected="+std::to_string(*mismatch.second));
        }
    };
    GameplayInput confirm{};confirm.confirm=true;
    GameplayInput down{};down.reverse=true;
    const auto pulse=[&](const ReturnedMenuDirection direction) {
        GameplayInput input{};
        switch(direction) {
        case ReturnedMenuDirection::up:input.forward=true;break;
        case ReturnedMenuDirection::down:input.reverse=true;break;
        case ReturnedMenuDirection::left:input.left=true;break;
        case ReturnedMenuDirection::right:input.right=true;break;
        }
        step(input);step();
    };
    const auto activate=[&]() {step(confirm);step();};
    const auto open=[&](const unsigned selection) {
        main(0);
        for(unsigned index=0;index<selection;++index) {pulse(ReturnedMenuDirection::down);}
        main(selection);activate();
    };

    // A direction cancels a pending press, with Enter held or released on that poll.
    main(0);step(confirm);
    auto heldDown=down;heldDown.confirm=true;step(heldDown);step();main(1);
    step(confirm);step(down);step();main(2);
    pulse(ReturnedMenuDirection::down);main(3);activate();
    requireCampaignComparison(left.menuBackAvailable()&&right.menuBackAvailable(),"Scores did not own Escape");
    {
        const auto roles=owner->pageObservation();IndexedPages expected;
        paintPlayableScores(expected,data.artGroups.at(0),data.titleFont,*owner->highscoreTable(),
            MenuWidgetState::released,roles.back,roles.front);
        requireCampaignComparison(owner->frame().pixels==expected.snapshot(roles.front),
            "returned Scores did not render the earned resident table");
    }
    GameplayInput escape{};escape.exit=true;step(escape);step();main(0);boundary();

    open(1);activate(); //Advance intro to the real first model before viewer controls.
    requireCampaignComparison(!left.menuBackAvailable(),"Briefing incorrectly assigned Scores Escape");
    pulse(ReturnedMenuDirection::left);pulse(ReturnedMenuDirection::down);activate(); //Stop.
    const auto stopped=owner->frame().pixels;step();
    for(std::size_t y=35;y<=109;++y) {
        requireCampaignComparison(std::equal(stopped.begin()+y*320+50,stopped.begin()+y*320+295,
            owner->frame().pixels.begin()+y*320+50),"Briefing STOP did not hold the actual model pane");
    }
    pulse(ReturnedMenuDirection::down);activate(); //Faster.
    pulse(ReturnedMenuDirection::down);activate(); //Zoom in.
    pulse(ReturnedMenuDirection::down);activate(); //Zoom out.
    for(unsigned index=0;index<4;++index) {pulse(ReturnedMenuDirection::up);}
    pulse(ReturnedMenuDirection::right); //NEXT.
    for(std::size_t page=1;page<briefingPageCount;++page) {
        activate();
        if(page+1<briefingPageCount) {
            requireCampaignComparison(owner->menu()&&owner->menu()->selection()==1
                && !left.menuBackAvailable(),"Briefing closed before its last recovered page");
        }
    }
    main(0);boundary();

    open(2);
    pulse(ReturnedMenuDirection::up);activate(); //Unavailable STORY must stay fixed.
    pulse(ReturnedMenuDirection::up);activate(); //SOUNDS1->0.
    pulse(ReturnedMenuDirection::down);pulse(ReturnedMenuDirection::down);activate(); //EXIT/save.
    main(0);
    auto expectedConfiguration=*data.cockpitSettings;
    expectedConfiguration.options[0]=0; //The actual startup's absent mouse normalization.
    expectedConfiguration.options[3]=0;
    auto expectedScores=scores;expectedScores.setConfiguration(expectedConfiguration);
    const auto persisted=encodeHighscoreTable(*owner->highscoreTable());
    requireCampaignComparison(persisted==encodeHighscoreTable(expectedScores)
        && persisted==encodeHighscoreTable(*twin->highscoreTable()),
        "Prefs did not save SOUNDS0 while preserving the earned scores/fixed settings");
    boundary();

    // Begin is an ordinary released widget action on the same returned-title parent.
    step(confirm);advance({});
    requireCampaignComparison(owner->model().campaignLevel()==0
        && owner->model().phase()==GameplayPhase::playableLoaderReturn,"returned Begin missed its real Level1 loader");
    unsigned ordinary=0;
    for(unsigned calls=0;ordinary<2&&calls<8;++calls) {
        const auto phase=owner->model().phase();const auto world=owner->model().level2LoopTick();
        advance({});
        requireCampaignComparison(encodeHighscoreTable(*owner->highscoreTable())==persisted
            && encodeHighscoreTable(*twin->highscoreTable())==persisted,
            "returned Begin lost the saved current configuration/table");
        if(phase==GameplayPhase::playable&&owner->model().phase()==GameplayPhase::playable) {
            requireCampaignComparison(owner->model().level2LoopTick()==static_cast<std::uint16_t>(world+1),
                "returned Begin ordinary frames were not adjacent");
            ++ordinary;
        }
    }
    requireCampaignComparison(ordinary==2&&owner->model().campaignLevel()==0,
        "returned Begin missed two adjacent ordinary frames");
    boundary();
    std::cout<<"Actual-startup returned title: cancelled presses/Scores Escape/Briefing19+viewer/Prefs SOUNDS0/Begin+2 PASS polls="
        <<polls<<'\n';
}
} //namespace stellar7::test
