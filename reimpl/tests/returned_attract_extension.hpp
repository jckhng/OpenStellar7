#pragma once
#include "stellar7/recovered_frontend.hpp"
#include "stellar7/native_attract.hpp"
#include <set>
#include <stdexcept>

namespace stellar7::test {
//Append to an existing actual-startup campaign/title route with feature11/15.
//This helper does not synthesize a nativeMenu model or import a later snapshot.
inline void appendReturnedAttract(RecoveredFrontend& frontend) {
    const auto require=[](bool value,const char* message) {if(!value) {throw std::runtime_error(message);}};
    const auto* owner=frontend.session();
    require(owner && owner->model().phase()==GameplayPhase::nativeMenu,"returned attract requires actual retained title");
    const auto* synth=&owner->synth();const auto score=owner->model().score();
    const auto tick=owner->model().level2LoopTick();std::set<std::size_t> pages;std::set<NativeAttractStage> stages;
    const auto advance=[&](const GameplayInput& input=GameplayInput{}) {
        const auto first=frontend.elapsedSamples();const auto delivery=frontend.advanceGameplay(input);
        require(delivery.firstSample==first && first+delivery.samples.size()==frontend.elapsedSamples()
            && !delivery.exitRequested,"returned attract lost retained PCM epoch");
        require(frontend.session()==owner && &owner->synth()==synth,"returned attract replaced session/synth owners");
        require(owner->model().score()==score && owner->model().level2LoopTick()==tick,
            "returned attract advanced world/main step or changed earned score");
        if(const auto p=frontend.attractProgress()) {
            stages.insert(p->stage);
            if(p->stage==NativeAttractStage::briefing && p->tours==0) {pages.insert(p->page);}
        }
    };
    bool second=false;
    for(unsigned call=0;call<2000000;++call) {
        advance();
        const auto p=frontend.attractProgress();
        if(p && p->stage==NativeAttractStage::briefing && p->tours==1) {second=true;break;}
    }
    require(second && pages.size()==19,"returned actual expiry failed complete19page first tour");
    for(const auto stage:{NativeAttractStage::intro,NativeAttractStage::credits,NativeAttractStage::postCredits}) {
        require(stages.count(stage)==1,"returned attract omitted actual retained presentation stage");
    }
    while(!frontend.attractAcceptsSourceKey()) {advance();}
    GameplayInput escape{};escape.sourceKey=0x011b;escape.exit=true;advance(escape);advance();
    require(!frontend.attractProgress() && owner->menu() && owner->menu()->selection()==0,
        "returned actual Escape failed retained main menu");
    GameplayInput confirm{};confirm.confirm=true;advance(confirm);
    const auto first=frontend.elapsedSamples();const auto begin=frontend.advanceGameplay({});
    require(begin.firstSample==first && first+begin.samples.size()==frontend.elapsedSamples(),"returned Begin lost epoch");
    require(frontend.session()==owner && &owner->synth()==synth && owner->model().campaignLevel()==0,
        "returned Begin replaced owner or missed actual Level1");
    unsigned adjacent=0;
    for(unsigned call=0;adjacent<2 && call<16;++call) {
        const auto before=owner->model().phase();const auto previousTick=owner->model().level2LoopTick();
        frontend.advanceGameplay({});
        if(before==GameplayPhase::playable && owner->model().phase()==GameplayPhase::playable) {
            require(owner->model().level2LoopTick()==static_cast<std::uint16_t>(previousTick+1),"returned Begin frames not adjacent");++adjacent;
        }
    }
    require(adjacent==2,"returned attract Begin missed two ordinary frames");
}
}
