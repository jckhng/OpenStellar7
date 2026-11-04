#include "playable_startup_resources.hpp"
#include "playable_campaign_comparison.hpp"
#include "stellar7/audio_direct_work.hpp"

#include <algorithm>
#include <iostream>

namespace stellar7 {
//Controlled selected-empty-charge branch; no world/RNG/resource state is imported.
struct Level1ReloadTestAccess {
    static void emptySelected(Level1State& state) {
        state.systemStatusLevels_[0]=0;
        state.selectedSystem_=0;
    }
};
struct GameplayModelTestAccess {
    static void emptySelected(GameplayModel& model) {Level1ReloadTestAccess::emptySelected(model.level1_);}
};
struct NativeSessionTestAccess {
    static void emptySelected(const NativeGameplaySession& session) {
        GameplayModelTestAccess::emptySelected(const_cast<NativeGameplaySession&>(session).model_);
    }
    static std::size_t eventCursor(const NativeGameplaySession& session) {return session.level2Cursor_;}
};
}

namespace {
using namespace stellar7;
using test::requireCampaignComparison;
constexpr std::int16_t lowDetail=0,highDetail=81;
constexpr std::int16_t indicatorStep=4;
constexpr std::uint16_t selectionEvent=16;
constexpr std::size_t maximumCalls=10000;
constexpr unsigned endToEndSliderSteps=41; //8152 moves2 and clamps0..81.

class MemoryStore final:public HighscoreStore {
public:
    HighscoreSaveResult saveAndReload(const HighscoreTable& table) override {
        ++calls;bytes=encodeHighscoreTable(table);
        return {HighscoreSaveStatus::saved,decodeHighscoreTable(bytes)};
    }
    unsigned calls=0;
    std::vector<std::uint8_t> bytes;
};

void run(const std::filesystem::path& root) {
    auto assets=naturalRoute::assets(root);
    //A legal cold header isolates the two live detail transitions and SFX publication.
    assets.cockpitSettings->viewSize=highDetail;assets.cockpitSettings->options[3]=1;
    assets.highscoreTable->setConfiguration(*assets.cockpitSettings);
    const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto vga=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    const auto store=std::make_shared<MemoryStore>();
    RecoveredFrontend frontend(assets,art,decodeFrameCopyProfile(vga.at(0)),257,store,CampaignExecution::playable,
        NativePointerCapability::unavailable,NativeMenuFeatures::fullGraphicDetail);
    unsigned menuPolls=0;
    for(unsigned call=0;!frontend.session()&&call<maximumCalls;++call) {
        NativeMenuInput input;
        if(frontend.startup().phase()==NativeStartupPhase::awaitingMenuDecision) {input.confirm=menuPolls++==1;}
        frontend.advanceStartup(input);
    }
    requireCampaignComparison(frontend.session()!=nullptr,"live range test missed actual Begin");
    for(unsigned index=0;index<3;++index) {frontend.advanceGameplay({});}
    const auto* owner=frontend.session();const auto* synth=&owner->synth();
    const auto advance=[&](const GameplayInput input) {
        const auto before=frontend.elapsedSamples();const auto delivery=frontend.advanceGameplay(input);
        requireCampaignComparison(frontend.session()==owner&&&owner->synth()==synth,
            "live range replaced retained session or synth");
        requireCampaignComparison(delivery.firstSample==before&&before+delivery.samples.size()==frontend.elapsedSamples(),
            "live range lost contiguous device/audio time");
        return delivery;
    };
    GameplayInput open;open.systemAction=GameplaySystemAction::preferences;
    GameplayInput down;down.reverse=true;
    GameplayInput up;up.forward=true;
    GameplayInput left;left.left=true;
    GameplayInput right;right.right=true;
    GameplayInput enter;enter.confirm=true;
    GameplayInput escape;escape.exit=true;

    const auto round=[&](const std::int16_t detail,const GameplayInput direction,const unsigned saveCount) {
        const auto priorLoop=owner->model().level2LoopTick();
        advance(open);
        requireCampaignComparison(frontend.menuBackAvailable()
            &&owner->model().level2LoopTick()==static_cast<std::uint16_t>(priorLoop+1),
            "F10 did not consume exactly one ordinary frame");
        const auto world=owner->model().level2LoopTick();const auto ticks=owner->model().ticks();
        const auto timers=owner->model().level1State().systemTimers();
        const auto x=owner->model().level2CockpitIndicatorX();
        const auto velocity=owner->model().level2CockpitIndicatorVelocity();
        const auto rng=owner->model().level2Rng();
        const auto poll=[&](const GameplayInput input) {
            const auto delivery=advance(input);
            requireCampaignComparison(owner->model().level2LoopTick()==world&&owner->model().ticks()==ticks
                &&owner->model().level1State().systemTimers()==timers,
                "live slider/PLAY advanced world or system timers");
            if(frontend.menuBackAvailable()) {
                requireCampaignComparison(owner->model().level2CockpitIndicatorX()==x
                    &&owner->model().level2CockpitIndicatorVelocity()==velocity,
                    "slider polling advanced retained indicator");
                test::compareCampaignRng(rng,owner->model().level2Rng(),test::CampaignComparisonStage::boundary);
            }
            return delivery;
        };
        poll(down);poll({}); //PLAY->detail.
        for(unsigned index=0;index<endToEndSliderSteps;++index) {poll(direction);}
        poll({});poll(up);poll({});poll(enter);poll({}); //Source released PLAY.
        requireCampaignComparison(!frontend.menuBackAvailable()&&store->calls==saveCount
            &&owner->highscoreTable()->header()[0]==detail,
            "live range did not save/reload source slider endpoint");
        const auto descriptor=owner->model().playableModelDetail(0);
        requireCampaignComparison(descriptor.scale==(detail==lowDetail?64:252)
            &&descriptor.cap==(detail==lowDetail?9:90),"PLAY failed actual8AFC descriptor0 rebind");
        if(detail==lowDetail) {
            requireCampaignComparison(owner->model().level2CockpitIndicatorX()==x
                &&owner->model().level2CockpitIndicatorVelocity()==velocity,
                "detail0 restoration advanced source-skipped indicator");
        } else {
            requireCampaignComparison(velocity==indicatorStep&&x>=132&&x<=168,
                "high range fixture crossed a source indicator reflection boundary");
            requireCampaignComparison(owner->model().level2CockpitIndicatorX()==x+2*indicatorStep,
                "detail81 restoration did not perform exactly two cockpit advances");
        }
        const auto resumedX=owner->model().level2CockpitIndicatorX();
        for(unsigned index=1;index<=2;++index) {
            advance({});
            requireCampaignComparison(owner->model().level2LoopTick()==static_cast<std::uint16_t>(world+index)
                &&owner->model().ticks()==ticks+index&&owner->model().level1State().systemTimers()==timers,
                "live range lost an adjacent ordinary frame or changed idle system timers");
            requireCampaignComparison(owner->model().level2CockpitIndicatorX()
                ==resumedX+(detail==lowDetail?0:index*indicatorStep),
                "resumed ordinary detail indicator predicate is ineffective");
        }
    };
    round(lowDetail,left,1);
    round(highDetail,right,2);
    requireCampaignComparison(assets.cockpitSettings->viewSize==highDetail,
        "live range mutated immutable loaded configuration");

    //Controlled source8ED2 branch joined to the same actual retained owner.
    advance(open);NativeSessionTestAccess::emptySelected(*owner);
    const auto events=owner->model().level2AudioEvents().size();
    const auto world=owner->model().level2LoopTick();const auto timers=owner->model().level1State().systemTimers();
    const auto writeCursor=owner->synth().registerWrites().size();
    const auto expectedDirect=soundBlasterPlayableDirectAdvance(*owner->recoveredClockPhase()->callback,
        gameplaySystemCycleDirectRecord,SoundBlasterDirectCaller::gameplaySystemCycle);
    const auto delivered=advance(escape);
    requireCampaignComparison(!frontend.menuBackAvailable()&&delivered.events.size()==1
        &&delivered.events[0].eventId==selectionEvent&&delivered.events[0].loopTick==world
        &&owner->model().level2AudioEvents().size()==events+1,
        "8F1C selection event16 was delayed beyond live modal return");
    requireCampaignComparison(NativeSessionTestAccess::eventCursor(*owner)==owner->model().level2AudioEvents().size()
        &&owner->model().level2LoopTick()==world&&owner->model().level1State().systemTimers()==timers,
        "8F1C did not settle event cursor without a world/timer step");
    auto write=writeCursor;
    for(const auto& block:expectedDirect.blocks) {
        if(block.kind!=SoundBlasterWorkKind::registerWrite) {continue;}
        requireCampaignComparison(write<owner->synth().registerWrites().size()
            &&owner->synth().registerWrites()[write].registerNumber==block.registerNumber
            &&owner->synth().registerWrites()[write].value==block.value,
            "8F1C did not trigger event16 OPL before the modal return device work");
        ++write;
    }
    requireCampaignComparison(write>writeCursor,"event16 direct fixture has no OPL writes");
    const auto adjacent=advance({});
    requireCampaignComparison(std::none_of(adjacent.events.begin(),adjacent.events.end(),
        [](const auto event) {return event.eventId==selectionEvent;}),"ordinary frame replayed modal event16");
    std::cout<<"Actual same-owner F10/0/PLAY/adjacent/F10/81/PLAY/indicator rebind and controlled8F1C event16 PASS\n";
}
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {run(argv[1]);return 0;}catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
