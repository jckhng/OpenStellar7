// Controlled game-over boundary after real public startup, Begin, and ordinary
// frames. It does not claim natural player-death ancestry or original PCM.
#include "playable_startup_resources.hpp"
#include "stellar7/story_scene.hpp"

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>

namespace stellar7 {
enum class ControlledScoreRoute { rankMiss, rankHitEmptyName };
struct GameplayModelTestAccess {
    static void gameOver(GameplayModel& model,const ContinueDecision decision,
        const ControlledScoreRoute scoreRoute) {
        if(model.phase_!=GameplayPhase::playable || !model.playableCampaignFrames_) {
            throw std::invalid_argument("game-over control needs real playable ordinary frames");
        }
        model.phase_=GameplayPhase::campaignEnded;
        model.failureFromLevel2_=true;
        model.remainingContinues_=decision==ContinueDecision::pending ? 0 : 1;
        if(decision==ContinueDecision::pending) {model.recoveredContinueDecision_.reset();}
        else {model.recoveredContinueDecision_=decision;}
        model.failureRng_=model.level2Rng_;
        model.playableHighscoreContinuation_.reset();
        constexpr std::uint32_t rankHitScore=10001;
        model.level2Score_=scoreRoute==ControlledScoreRoute::rankHitEmptyName ? rankHitScore : 0;
    }
};
struct NativeSessionTestAccess {
    static void gameOver(NativeGameplaySession& session,const ContinueDecision decision,
        const ControlledScoreRoute scoreRoute) {
        GameplayModelTestAccess::gameOver(session.model_,decision,scoreRoute);
    }
};
}

namespace {
using namespace stellar7;

void require(const bool valid,const char* message) {
    if(!valid) {throw std::runtime_error(message);}
}

void sameFrame(const IndexedScreen& a,const IndexedScreen& b) {
    require(a.pixels==b.pixels&&a.palette==b.palette,"game-over indexed frame changed across budgets");
}

void sameDelivery(const NativeSessionDelivery& a,const NativeSessionDelivery& b) {
    require(a.firstSample==b.firstSample&&a.samples==b.samples
        &&a.events.size()==b.events.size()&&a.entry.size()==b.entry.size()
        &&a.presentations.size()==b.presentations.size(),
        "game-over PCM/entry/event count changed across budgets");
    for(std::size_t index=0;index<a.events.size();++index) {
        require(a.events[index].loopTick==b.events[index].loopTick
            &&a.events[index].eventId==b.events[index].eventId,"game-over event order changed");
    }
    for(std::size_t index=0;index<a.entry.size();++index) {
        require(a.entry[index].samples==b.entry[index].samples
            &&a.entry[index].elapsedNanoseconds==b.entry[index].elapsedNanoseconds,
            "game-over entry audio changed");
        sameFrame(a.entry[index].indexed,b.entry[index].indexed);
    }
    for(std::size_t index=0;index<a.presentations.size();++index) {
        require(a.presentations[index].sampleEnd==b.presentations[index].sampleEnd,
            "game-over presentation time changed");
        sameFrame(a.presentations[index].indexed,b.presentations[index].indexed);
    }
}

void sameSession(const RecoveredFrontend& a,const RecoveredFrontend& b) {
    require(a.session()&&b.session()&&a.elapsedSamples()==b.elapsedSamples(),
        "game-over retained clock changed across budgets");
    const auto& left=*a.session(),&right=*b.session();
    require(left.model().phase()==right.model().phase()
        &&left.model().score()==right.model().score()
        &&left.model().ticks()==right.model().ticks()
        &&left.storyActive()==right.storyActive()
        &&left.storyScene()==right.storyScene()
        &&left.storyPage()==right.storyPage(),
        "game-over session/controller diverged across budgets");
    sameFrame(a.frame(),b.frame());
    const auto x=left.synth().driverState(),y=right.synth().driverState();
    require(x.sequenceAllocated==y.sequenceAllocated&&x.sequenceSelected==y.sequenceSelected
        &&x.sequenceActive==y.sequenceActive&&x.sequencePosition==y.sequencePosition
        &&x.sequenceWait==y.sequenceWait&&x.sequenceAllocations==y.sequenceAllocations
        &&x.sequenceSelections==y.sequenceSelections&&x.sequenceActivity==y.sequenceActivity
        &&x.normalClockCycles==y.normalClockCycles&&x.clockPhase==y.clockPhase,
        "game-over retained synth/clock diverged across budgets");
    const auto voice=[](const SampledVoiceObservation& value) {
        return std::tie(value.phase,value.transferred,value.sampleCount,value.clockPhase,
            value.clockPeriod,value.dacValue,value.speakerEnabled);
    };
    require(voice(left.synth().sampledVoiceObservation())==voice(right.synth().sampledVoiceObservation()),
        "game-over sampled voice diverged across budgets");
    const auto& writes=left.synth().registerWrites();const auto& other=right.synth().registerWrites();
    require(writes.size()==other.size(),"game-over OPL write count diverged");
    for(std::size_t index=0;index<writes.size();++index) {
        require(writes[index].registerNumber==other[index].registerNumber
            &&writes[index].value==other[index].value,"game-over OPL writes diverged");
    }
}

struct Twins {
    std::unique_ptr<RecoveredFrontend> a,b;
    Twins(const GameplayAssets& data,const NativeStartupAssets& startup,const FrameCopyProfile copy)
        :a(std::make_unique<RecoveredFrontend>(data,startup,copy,10000,std::shared_ptr<HighscoreStore>{},CampaignExecution::playable,
            NativePointerCapability::unavailable,NativeMenuFeatures::releaseMenus)),
         b(std::make_unique<RecoveredFrontend>(data,startup,copy,257,std::shared_ptr<HighscoreStore>{},CampaignExecution::playable,
            NativePointerCapability::unavailable,NativeMenuFeatures::releaseMenus)) {
        for(std::size_t twin=0;twin<2;++twin) {
            auto* frontend=twin==0 ? a.get() : b.get();
            const auto failure=[&](const char* stage,const unsigned call) {
                throw std::runtime_error(std::string("public game-over ")+stage
                    +" music="+std::to_string(data.cockpitSettings->options[2])
                    +" twin="+std::to_string(twin)
                    +" phase="+std::to_string(static_cast<unsigned>(frontend->startup().phase()))
                    +" call="+std::to_string(call));
            };
            unsigned call=0;
            for(;frontend->startup().phase()!=NativeStartupPhase::awaitingMenuDecision&&call<20000;++call) {
                NativeMenuInput input{};
                if(frontend->startup().phase()==NativeStartupPhase::awaitingIntroInput
                    ||frontend->startup().storyActive()) {
                    input.cancel=true;
                }
                frontend->advanceStartup(input);
            }
            if(frontend->startup().phase()!=NativeStartupPhase::awaitingMenuDecision) {
                failure("pre-menu",call);
            }
            NativeMenuInput begin{};begin.confirm=true;
            frontend->advanceStartup(begin);frontend->advanceStartup({});
            for(call=0;!frontend->session()&&call<128;++call) {frontend->advanceStartup({});}
            if(!frontend->session()) {failure("Begin",call);}
            for(unsigned frame=0;frame<3;++frame) {frontend->advanceGameplay({});}
            require(frontend->session()->model().phase()==GameplayPhase::playable,
                "public game-over fixture missed ordinary gameplay");
        }
        sameSession(*a,*b);
    }

    NativeSessionDelivery advance(const GameplayInput input={}) {
        const auto left=a->advanceGameplay(input),right=b->advanceGameplay(input);
        sameDelivery(left,right);sameSession(*a,*b);
        return left;
    }
    void gameOver(const ContinueDecision decision,const ControlledScoreRoute scoreRoute) {
        NativeSessionTestAccess::gameOver(const_cast<NativeGameplaySession&>(*a->session()),decision,scoreRoute);
        NativeSessionTestAccess::gameOver(const_cast<NativeGameplaySession&>(*b->session()),decision,scoreRoute);
        sameSession(*a,*b);
    }
};

void menuAndBegin(Twins& twins) {
    for(unsigned call=0;twins.a->session()->model().phase()!=GameplayPhase::nativeMenu&&call<8;++call) {
        twins.advance();
    }
    require(twins.a->session()->model().phase()==GameplayPhase::nativeMenu,
        "game-over scene failed actual title/menu return");
    require(twins.a->session()->synth().sampledVoiceObservation().phase==SampledVoicePhase::unloaded,
        "game-over speech leaked into menu");
    twins.advance();
    GameplayInput confirm{};confirm.confirm=true;
    twins.advance(confirm);twins.advance();
    for(unsigned call=0;twins.a->session()->model().phase()!=GameplayPhase::playable&&call<8;++call) {
        twins.advance();
    }
    require(twins.a->session()->model().phase()==GameplayPhase::playable
        &&twins.a->session()->model().campaignLevel()==0,
        "game-over menu failed actual next Begin");
    twins.advance();twins.advance();
}

void gameOverScene(Twins& twins,const StorySceneAssets& stories,const bool skip,const bool music,
    const ContinueDecision decision,const ControlledScoreRoute scoreRoute=ControlledScoreRoute::rankMiss) {
    twins.gameOver(decision,scoreRoute);
    require(!twins.a->session()->storyActive(),"controlled game-over entered with stale scene");
    twins.advance();
    require(twins.a->session()->storyScene()==StoryScene::defeat,
        "Story Off suppressed source SCENEE game-over scene");
    std::vector<std::size_t> observed;
    const auto observe=[&]() {
        const auto value=twins.a->session()->synth().sampledVoiceObservation();
        if(value.transferred>0 && (observed.empty()||observed.back()!=value.sampleCount)) {
            observed.push_back(value.sampleCount);
        }
    };
    observe();
    if(skip) {
        GameplayInput exit{};exit.exit=true;
        twins.advance(exit);observe();
    }
    for(unsigned call=0;twins.a->session()->storyActive()&&call<20000;++call) {
        twins.advance();observe();
    }
    require(!twins.a->session()->storyActive(),"SCENEE failed complete/Escape return");
    require(twins.a->session()->synth().sampledVoiceObservation().phase==SampledVoicePhase::unloaded,
        "SCENEE retained sampled voice after scene exit");
    if(!skip&&music) {
        std::vector<std::size_t> expected;
        for(const auto& encoded:stories.voices.at(3)) {
            expected.push_back(decodeSampledVoice(encoded).samples.size());
        }
        require(expected.size()==2&&observed==expected,"SCENEE missed its two source SSM4 speech segments");
    } else if(!music) {
        require(observed.empty(),"Music Off played SCENEE sampled voice");
    }
    if(scoreRoute==ControlledScoreRoute::rankHitEmptyName) {
        twins.advance();
        require(twins.a->session()->model().phase()==GameplayPhase::highscoreNameEntry
            &&twins.a->session()->model().playableHighscoreCaller()==PlayableHighscoreCaller::gameOverTitle
            &&!twins.a->session()->storyActive(),
            "SCENEE rank hit missed the game-over name editor or replayed E");
        GameplayInput confirm{};confirm.confirm=true;
        twins.advance(confirm);twins.advance();
        require(twins.a->session()->model().phase()==GameplayPhase::campaignEnded
            &&!twins.a->session()->storyActive(),
            "empty game-over name failed to return to title without replaying E");
    }
    menuAndBegin(twins);
}

GameplayAssets assets(const std::filesystem::path& root,const bool music) {
    auto data=naturalRoute::assets(root);
    data.cockpitSettings->options[2]=music ? 1 : 0;
    data.cockpitSettings->options[5]=0; //Game-over SCENEE is independent of Story preference.
    HighscoreTable::Entries entries{};
    for(auto& entry:entries) {entry.score=10000;}
    HighscoreTable table({},entries);table.setConfiguration(*data.cockpitSettings);
    data.highscoreTable=table;
    data.storyScenes=std::make_shared<const StorySceneAssets>(decodeStorySceneAssets(
        naturalRoute::read(root/"SCENEB1.RES"),naturalRoute::read(root/"SCENEA.RES"),
        {naturalRoute::read(root/"SCENEB2.RES"),naturalRoute::read(root/"SCENEB3.RES"),
            naturalRoute::read(root/"SCENEB5.RES")},
        naturalRoute::read(root/"SCENEC.RES"),naturalRoute::read(root/"VOICE.RES"),
        naturalRoute::read(root/"DRAXON.RES"),naturalRoute::read(root/"SCENEE.RES")));
    return data;
}

void run(const std::filesystem::path& root) {
    const auto startup=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto vga=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    require(vga.size()==1,"public game-over fixture requires supplied VGA driver");
    const auto copy=decodeFrameCopyProfile(vga.front());
    auto enabled=assets(root,true);
    Twins normal(enabled,startup,copy);
    gameOverScene(normal,*enabled.storyScenes,false,true,ContinueDecision::pending);
    gameOverScene(normal,*enabled.storyScenes,true,true,ContinueDecision::cancelled);
    gameOverScene(normal,*enabled.storyScenes,true,true,ContinueDecision::no);
    gameOverScene(normal,*enabled.storyScenes,true,true,ContinueDecision::pending,
        ControlledScoreRoute::rankHitEmptyName);
    auto muted=assets(root,false);
    Twins musicOff(muted,startup,copy);
    gameOverScene(musicOff,*muted.storyScenes,false,false,ContinueDecision::pending);
    std::cout<<"Controlled public Begin/game-over SCENEE complete/Escape/Music Off/rank hit -> menu/Begin+2 twins PASS\n";
}
}

int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {run(argv[1]);return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
