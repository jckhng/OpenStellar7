#include "native_level2_frame_fixture.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
using namespace stellar7;

class MemoryScoreStore final : public HighscoreStore {
public:
    HighscoreSaveResult saveAndReload(const HighscoreTable& table) override {
        ++calls;
        saved=table;
        return {HighscoreSaveStatus::saved,table};
    }
    unsigned calls=0;
    std::optional<HighscoreTable> saved;
};

void sameRng(StellarRng left,StellarRng right) {
    assert(left.calls()==right.calls() && left.indexA()==right.indexA() && left.indexB()==right.indexB());
    for (std::size_t index=0;index<StellarRng::wordCount;++index) {assert(left.next()==right.next());}
}

void same(const NativeGameplaySession& a,const NativeGameplaySession& b,
    const NativeSessionDelivery& left,const NativeSessionDelivery& right) {
    assert(left.firstSample==right.firstSample && left.samples==right.samples);
    assert(left.events.size()==right.events.size() && left.presentations.size()==right.presentations.size());
    const auto frame=[](const IndexedScreen& x,const IndexedScreen& y) {
        assert(x.pixels==y.pixels && x.palette.size()==y.palette.size());
        for (std::size_t index=0;index<x.palette.size();++index) {
            assert(x.palette[index].red==y.palette[index].red && x.palette[index].green==y.palette[index].green
                && x.palette[index].blue==y.palette[index].blue);
        }
    };
    frame(a.frame(),b.frame());
    for (std::size_t index=0;index<left.presentations.size();++index) {
        assert(left.presentations[index].sampleEnd==right.presentations[index].sampleEnd);
        frame(left.presentations[index].indexed,right.presentations[index].indexed);
    }
    for (const auto role:{NativePageRole::cache,NativePageRole::back,NativePageRole::front}) {
        assert(a.retainedPagePixels(role)==b.retainedPagePixels(role));
    }
    assert(a.elapsedSamples()==b.elapsedSamples() && a.model().phase()==b.model().phase());
    assert(a.model().score()==b.model().score() && a.model().playerArmor()==b.model().playerArmor());
    assert(a.model().ticks()==b.model().ticks() && a.model().presentationFrame()==b.model().presentationFrame());
    assert(a.model().remainingContinues()==b.model().remainingContinues());
    assert(a.model().playableHighscoreCaller()==b.model().playableHighscoreCaller());
    sameRng(a.model().level2Rng(),b.model().level2Rng());
    const auto& x=a.synth().registerWrites();const auto& y=b.synth().registerWrites();
    assert(x.size()==y.size());
    for (std::size_t index=0;index<x.size();++index) {
        assert(x[index].registerNumber==y[index].registerNumber && x[index].value==y[index].value);
    }
}

enum class TitleCaller { ending,gameOver,exhaustedGameOver };
enum class NameCase { empty,populated };

void route(const std::filesystem::path& root,const TitleCaller caller,const NameCase nameCase) {
    const bool empty=nameCase==NameCase::empty;
    auto leftStore=std::make_shared<MemoryScoreStore>(),rightStore=std::make_shared<MemoryScoreStore>();
    const auto session=[&](const std::shared_ptr<MemoryScoreStore>& store) {
        if (caller==TitleCaller::ending) {
            return test::makePlayableEndingSession(root,test::PlayableScoreFixture::rankHit,store);
        }
        if (caller==TitleCaller::exhaustedGameOver) {
            return test::makePlayableExhaustedSession(root,test::PlayableScoreFixture::rankHit,store);
        }
        return test::makePlayableLethalSession(root,test::PlayableScoreFixture::rankHit,store);
    };
    auto a=session(leftStore),b=session(rightStore);
    const auto step=[&](const GameplayInput& input=GameplayInput{},const NameEntryInput& text=NameEntryInput{}) {
        const auto x=a->advancePlayableCampaign(input,10000,text),y=b->advancePlayableCampaign(input,257,text);
        same(*a,*b,x,y);
    };
    GameplayInput confirm{};confirm.confirm=true;
    if (caller==TitleCaller::ending) {
        for (unsigned call=0;a->model().phase()!=GameplayPhase::endingReturn && call<50;++call) {step();}
        assert(a->model().phase()==GameplayPhase::endingReturn);
    } else if (caller==TitleCaller::exhaustedGameOver) {
        for (unsigned call=0;a->model().phase()!=GameplayPhase::campaignEnded && call<50;++call) {step();}
        assert(a->model().phase()==GameplayPhase::campaignEnded && a->model().remainingContinues()==0
            && a->model().recoveredContinueDecision()==ContinueDecision::pending);
    } else {
        for (unsigned call=0;a->model().phase()!=GameplayPhase::continuePrompt && call<50;++call) {step();}
        assert(a->model().phase()==GameplayPhase::continuePrompt);
        step();assert(a->model().continueChoice()==ContinueChoice::no);
        step(confirm);step();assert(a->model().phase()==GameplayPhase::campaignEnded);
    }
    const auto returnPhase=a->model().phase();
    const auto returnFrame=a->model().presentationFrame();
    const auto score=a->model().score(),continues=a->model().remainingContinues();
    const auto worldTick=a->model().level2LoopTick();
    const auto rng=a->model().level2Rng();
    const auto sourceSamples=a->elapsedSamples();
    assert(score>0);
    const auto writes=a->synth().registerWrites().size(),ticks=a->model().ticks();
    bool heldRejected=false;
    try {a->advancePlayableCampaign(confirm,10000);}
    catch (const std::invalid_argument&) {heldRejected=true;}
    assert(heldRejected && a->elapsedSamples()==sourceSamples && a->model().ticks()==ticks
        && a->synth().registerWrites().size()==writes && a->model().phase()==returnPhase);
    step();assert(a->model().phase()==GameplayPhase::highscoreNameEntry);
    const auto parent=caller==TitleCaller::ending ? PlayableHighscoreCaller::endingTitle : PlayableHighscoreCaller::gameOverTitle;
    assert(a->model().playableHighscoreCaller()==parent && a->model().score()==score);
    const auto frozen=[&]() {
        assert(a->model().score()==score && a->model().remainingContinues()==continues);
        assert(a->model().level2LoopTick()==worldTick);
        sameRng(a->model().level2Rng(),rng);
    };
    bool rejected=false;
    try {const_cast<GameplayModel&>(a->model()).beginPlayableHighscoreNameEntry();}
    catch (const std::invalid_argument&) {rejected=true;}
    assert(rejected);frozen();
    if (!empty) {step({},{{'P','I','L','O','T'}});frozen();}
    step(confirm);frozen();step();frozen();
    if (!empty) {
        assert(a->model().phase()==GameplayPhase::highscoreHall && leftStore->calls==1 && rightStore->calls==1);
        assert(leftStore->saved && rightStore->saved);
        const auto& entry=leftStore->saved->entries().front();
        assert(entry.score==score && !entry.name.empty());
        const bool flagged=(static_cast<std::uint8_t>(entry.name.front())&0x80U)!=0;
        assert(flagged==(caller==TitleCaller::ending));
        step(confirm);frozen();step();frozen();
    } else {
        assert(leftStore->calls==0 && rightStore->calls==0);
    }
    assert(a->model().phase()==returnPhase && a->model().presentationFrame()==returnFrame);
    assert(a->model().playableHighscoreCaller()==parent);
    step();assert(a->model().phase()==GameplayPhase::nativeMenu && !a->model().playableHighscoreCaller());
    assert(a->model().score()==score && a->model().remainingContinues()==continues);
    step();step();
    assert(a->model().phase()==GameplayPhase::nativeMenu && leftStore->calls==(empty ? 0U : 1U));
    assert(a->elapsedSamples()>sourceSamples);
    std::cout<<(caller==TitleCaller::ending ? "Ending"
        : caller==TitleCaller::exhaustedGameOver ? "Exhausted-stock game over" : "Game-over No")
        <<" rank hit/"<<(empty ? "empty name" : "save+hall")<<" -> same title parent/menu, no retry PASS\n";
}
} //namespace

int main(int argc,char** argv) {
    if (argc!=2) {return 2;}
    try {
        for (const auto caller:{TitleCaller::ending,TitleCaller::gameOver,TitleCaller::exhaustedGameOver}) {
            route(argv[1],caller,NameCase::populated);
            route(argv[1],caller,NameCase::empty);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
