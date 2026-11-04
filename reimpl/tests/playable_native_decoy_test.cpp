//Actual startup only; no state writes or synthetic late-state entry.
#include "playable_startup_resources.hpp"
#include "playable_campaign_comparison.hpp"
#include "playable_decoy.hpp"
#include <algorithm>
#include <iostream>

namespace stellar7 {
struct NativeSessionTestAccess {
    static const PaletteWorkState& palette(const NativeGameplaySession& session) {
        return *session.recoveredStartupPalette_;
    }
};
struct GameplayModelTestAccess {
    static const Level1Entity& proxy(const GameplayModel& model) {return model.campaignPlayerProxy_;}
};
}
namespace {
using namespace stellar7;
using test::requireCampaignComparison;
std::unique_ptr<RecoveredFrontend> begin(const GameplayAssets& assets,const NativeStartupAssets& startup,
    const FrameCopyProfile copy,const std::size_t budget,std::vector<std::int16_t>& pcm) {
    auto frontend=std::make_unique<RecoveredFrontend>(assets,startup,copy,budget,
        std::shared_ptr<HighscoreStore>{},CampaignExecution::playable);
    unsigned polls=0;
    for(unsigned call=0;!frontend->session() && call<10000;++call) {
        NativeMenuInput input{};
        if(frontend->startup().phase()==NativeStartupPhase::awaitingMenuDecision) {input.confirm=polls++==1;}
        const auto before=frontend->elapsedSamples();
        const auto delivery=frontend->advanceStartup(input);
        requireCampaignComparison(delivery.firstSample==before
            && before+delivery.samples.size()==frontend->elapsedSamples(),"timed-system startup replaced PCM prefix");
        pcm.insert(pcm.end(),delivery.samples.begin(),delivery.samples.end());
    }
    requireCampaignComparison(frontend->session()!=nullptr,"timed-system startup did not reach actual Begin");
    return frontend;
}

void run(const std::filesystem::path& root) {
    const auto assets=naturalRoute::assets(root);
    const auto startup=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto drivers=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    requireCampaignComparison(drivers.size()==1,"missing actual VGA driver");
    const auto copy=decodeFrameCopyProfile(drivers.front());
    test::CampaignBudgetComparison comparison;
    auto left=begin(assets,startup,copy,10000,comparison.leftPcm);
    auto right=begin(assets,startup,copy,257,comparison.rightPcm);
    requireCampaignComparison(comparison.leftPcm==comparison.rightPcm,"startup PCM differs by budget");
    const auto* owner=left->session();
    const auto advance=[&](const GameplayInput& input) {
        const auto a=left->advanceGameplay(input);
        const auto b=right->advanceGameplay(input);
        comparison.compare(*owner,*right->session(),a,b,test::CampaignComparisonStage::boundary);
        requireCampaignComparison(test::campaignActorFields(GameplayModelTestAccess::proxy(owner->model()))
            ==test::campaignActorFields(GameplayModelTestAccess::proxy(right->session()->model())),
            "I proxy differs by packet budget");
        return a;
    };
    advance({}); //Neutral source adoption; the queued key belongs to the following ordinary tail.
    const auto initialStatus=owner->model().level1State().systemStatusLevels()[0];
    requireCampaignComparison(initialStatus>0,"actual settings have no I charge");
    const auto palette=NativeSessionTestAccess::palette(*owner);
    const auto beforePlayer=owner->model().level2Player();
    requireCampaignComparison(beforePlayer.weaponCooldown==0
        && beforePlayer.weaponSlotTimers==std::array<std::uint8_t,3>{0,0,0},
        "first ordinary I/fire request did not retain initial ready cannons");
    GameplayInput activate{};activate.fire=true;activate.systemAction=GameplaySystemAction::decoy;
    const auto delivery=advance(activate);
    std::vector<std::uint16_t> events;
    for(const auto& event:delivery.events) {events.push_back(event.eventId);}
    const auto fire=std::find(events.begin(),events.end(),14);
    const auto direct=std::find(events.begin(),events.end(),8);
    const auto decoy=std::find(events.begin(),events.end(),25);
    requireCampaignComparison(fire!=events.end() && direct!=events.end() && decoy!=events.end()
        && fire<direct && direct<decoy && std::count(events.begin(),events.end(),25)==1,
        "held fire /8E94 /0FF6 event order changed");
    const auto& model=owner->model();
    requireCampaignComparison(model.level1State().systemTimers()[0]==900
        && model.level1State().systemStatusLevels()[0]==-(initialStatus-1),"native I spent wrong charge/timer");
    constexpr std::uint8_t primaryCooldown=9;
    constexpr std::uint8_t primaryCannonLifetime=32;
    requireCampaignComparison(model.level2Player().weaponCooldown==primaryCooldown-1
        && model.level2Player().weaponSlotTimers==std::array<std::uint8_t,3>{primaryCannonLifetime-1,0,0},
        "completed tail did not decrement the newly allocated9/32 fire timers once");
    auto expected=model.level2Player();expected.allocationIndex=GameplayModelTestAccess::proxy(model).allocationIndex;
    //0FF6 copies after05D4 but before0372..03B3's cooldown/cannon decrements.
    expected.weaponCooldown=primaryCooldown;
    expected.weaponSlotTimers=beforePlayer.weaponSlotTimers;
    expected.weaponSlotTimers[0]=primaryCannonLifetime;
    requireCampaignComparison(test::campaignActorFields(expected)
        ==test::campaignActorFields(GameplayModelTestAccess::proxy(model)),"native activation omitted represented99-byte fields");
    unsigned repeats=0;
    for(unsigned step=1;step<=40;++step) {
        const auto frame=advance({});
        for(const auto& event:frame.events) {repeats+=event.eventId==25;}
        requireCampaignComparison(owner->model().level1State().systemTimers()[0]==900-step,
            "native timer did not decrease once per completed tail");
    }
    const auto& revealed=NativeSessionTestAccess::palette(*owner);
    requireCampaignComparison(repeats==0 && revealed.source==palette.source && revealed.output==palette.output,
        "1026 mutated a persistent palette bank or repeated25");
    requireCampaignComparison(std::equal(revealed.dac.colors.begin()+128*3,revealed.dac.colors.end(),
        palette.dac.colors.begin()+128*3),"1026 overwrote upper128 colors");
    const auto red=[&](const std::size_t color) {return revealed.dac.colors[color*3];};
    requireCampaignComparison(red(15)==60 && red(16)==0 && red(63)==60 && red(64)==0
        && red(85)==63 && red(86)==2 && red(87)==5 && red(88)==0 && red(95)==56
        && red(96)==0 && red(127)==60,"source raw red ramp / six-bit DAC alias differs");
    requireCampaignComparison(comparison.leftPcm==comparison.rightPcm,"I whole retained PCM differs by budget");
    std::cout<<"Actual startup I: full proxy,14/8/25 order,40-tail reveal and palette/page/audio twins PASS\n";
}
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {run(argv[1]);return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
