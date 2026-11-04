#include "native_level2_frame_fixture.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
using namespace stellar7;

void compare(const NativeGameplaySession& a,const NativeGameplaySession& b,
    const NativeSessionDelivery& left,const NativeSessionDelivery& right) {
    assert(left.firstSample==right.firstSample && left.samples==right.samples);
    assert(left.events.size()==right.events.size() && left.presentations.size()==right.presentations.size());
    for (std::size_t index=0;index<left.events.size();++index) {
        assert(left.events[index].loopTick==right.events[index].loopTick
            && left.events[index].eventId==right.events[index].eventId);
    }
    const auto pixels=[](const IndexedScreen& x,const IndexedScreen& y) {
        assert(x.pixels==y.pixels && x.palette.size()==y.palette.size());
        for (std::size_t index=0;index<x.palette.size();++index) {
            assert(x.palette[index].red==y.palette[index].red && x.palette[index].green==y.palette[index].green
                && x.palette[index].blue==y.palette[index].blue);
        }
    };
    pixels(a.frame(),b.frame());
    for (std::size_t index=0;index<left.presentations.size();++index) {
        assert(left.presentations[index].sampleEnd==right.presentations[index].sampleEnd);
        pixels(left.presentations[index].indexed,right.presentations[index].indexed);
    }
    for (const auto role:{NativePageRole::cache,NativePageRole::back,NativePageRole::front}) {
        assert(a.retainedPagePixels(role)==b.retainedPagePixels(role));
    }
    assert(a.elapsedSamples()==b.elapsedSamples() && a.model().phase()==b.model().phase());
    assert(a.model().campaignLevel()==b.model().campaignLevel() && a.model().ticks()==b.model().ticks());
    assert(a.model().score()==b.model().score() && a.model().playerArmor()==b.model().playerArmor());
    assert(a.model().remainingContinues()==b.model().remainingContinues());
    assert(a.synth().driverState().normalClockCycles==b.synth().driverState().normalClockCycles);
    const auto& l=a.synth().registerWrites();const auto& r=b.synth().registerWrites();
    assert(l.size()==r.size());
    for (std::size_t index=0;index<l.size();++index) {
        assert(l[index].registerNumber==r[index].registerNumber && l[index].value==r[index].value);
    }
}

void route(const std::filesystem::path& root,const test::PlayableScoreFixture scores) {
    auto a=test::makePlayableLethalSession(root,scores);
    auto b=test::makePlayableLethalSession(root,scores);
    const auto continues=a->model().remainingContinues();
    const auto firstSample=a->elapsedSamples();
    const auto step=[&](const GameplayInput& input=GameplayInput{},const NameEntryInput& text=NameEntryInput{}) {
        const auto left=a->advancePlayableCampaign(input,10000,text);
        const auto right=b->advancePlayableCampaign(input,257,text);
        compare(*a,*b,left,right);
    };
    unsigned destructionCalls=0;
    while (a->model().phase()!=GameplayPhase::continuePrompt && destructionCalls<50) {
        step();++destructionCalls;
    }
    assert(destructionCalls==47 && a->model().phase()==GameplayPhase::continuePrompt);
    step(); //Released navigation gate.
    GameplayInput choose{};choose.forward=true;step(choose);step();
    assert(a->model().continueChoice()==ContinueChoice::yes);
    GameplayInput confirm{};confirm.confirm=true;step(confirm);step();
    if (scores==test::PlayableScoreFixture::rankHit) {
        assert(a->model().phase()==GameplayPhase::highscoreNameEntry);
        step({},{{'T','E','S','T'}});step(confirm);step();
        assert(a->model().phase()==GameplayPhase::highscoreHall);
        step(confirm);step();
    }
    assert(a->model().phase()==GameplayPhase::playableLoaderReturn);
    assert(a->model().campaignLevel()==1 && a->model().remainingContinues()+1==continues);
    assert(a->model().score()==0 && a->model().playerArmor()==680);
    step(); //Actual preloop, then three main/radar/tail frames.
    step();
    GameplayInput moving{};moving.forward=true;moving.left=true;moving.fire=true;
    step(moving);step(moving);
    assert(a->model().phase()==GameplayPhase::playable && a->model().level2LoopTick()==3);
    assert(a->elapsedSamples()>firstSample);
    std::cout<<(scores==test::PlayableScoreFixture::rankHit ? "Rank-hit editor/hall" : "Rank-miss")
        <<" retained death/Continue/retry +3 PASS\n";
}

void declineAndBegin(const std::filesystem::path& root) {
    auto a=test::makePlayableLethalSession(root,test::PlayableScoreFixture::rankMiss);
    auto b=test::makePlayableLethalSession(root,test::PlayableScoreFixture::rankMiss);
    const auto step=[&](const GameplayInput& input=GameplayInput{}) {
        const auto left=a->advancePlayableCampaign(input,10000);
        const auto right=b->advancePlayableCampaign(input,257);
        compare(*a,*b,left,right);
    };
    for (unsigned count=0;a->model().phase()!=GameplayPhase::continuePrompt && count<50;++count) {step();}
    assert(a->model().phase()==GameplayPhase::continuePrompt);
    step();
    assert(a->model().continueChoice()==ContinueChoice::no);
    GameplayInput confirm{};confirm.confirm=true;
    step(confirm);step();
    assert(a->model().phase()==GameplayPhase::campaignEnded);
    const auto returnSamples=a->elapsedSamples();
    step();
    assert(a->model().phase()==GameplayPhase::nativeMenu);
    step();step(confirm);step();
    assert(a->model().phase()==GameplayPhase::playableLoaderReturn);
    assert(a->model().campaignLevel()==0 && a->model().remainingContinues()==2);
    assert(a->model().score()==0 && a->model().playerArmor()==680);
    assert(a->elapsedSamples()>returnSamples);
    step();step();
    GameplayInput moving{};moving.forward=true;moving.left=true;moving.fire=true;
    step(moving);step(moving);
    assert(a->model().phase()==GameplayPhase::playable && a->model().level2LoopTick()==3);
    std::cout<<"Retained Continue No -> title/Begin -> actual Level1 loader +3 PASS\n";
}
}

int main(int argc,char** argv) {
    if (argc!=2) {return 2;}
    try {
        route(argv[1],test::PlayableScoreFixture::rankMiss);
        route(argv[1],test::PlayableScoreFixture::rankHit);
        declineAndBegin(argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
