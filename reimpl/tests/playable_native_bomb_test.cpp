//Actual public startup; no controlled factory or runtime state writes.
#include "playable_startup_resources.hpp"
#include "playable_campaign_comparison.hpp"
#include "playable_bomb.hpp"
#include <algorithm>
#include <iostream>
namespace stellar7 {
struct GameplayModelTestAccess {
    static auto records(const GameplayModel& model) {return model.level2FixedPool_;}
    static auto known(const GameplayModel& model) {return model.level2FixedPoolKnown_;}
    static void compare(const GameplayModel& a,const GameplayModel& b) {
        const auto pool=[](const auto& first,const auto& second) {
            for(std::size_t slot=0;slot<first.size();++slot) {
                test::requireCampaignComparison(test::campaignActorFields(first[slot])
                    ==test::campaignActorFields(second[slot]),"B changed physical actor pool by budget");
            }
        };
        pool(a.level2ActorPool_,b.level2ActorPool_);pool(a.level2FixedPool_,b.level2FixedPool_);
        pool(a.level2ProjectilePool_,b.level2ProjectilePool_);pool(a.level2DeathEffectPool_,b.level2DeathEffectPool_);
        test::requireCampaignComparison(test::campaignActorFields(a.campaignPlayerProxy_)
            ==test::campaignActorFields(b.campaignPlayerProxy_),"B changed canonical proxy by budget");
        test::requireCampaignComparison(a.level2ActorPoolActive_==b.level2ActorPoolActive_
            && a.level2FixedPoolKnown_==b.level2FixedPoolKnown_
            && a.level2ProjectilePoolActive_==b.level2ProjectilePoolActive_
            && a.level2EntityFixedPoolSlots_==b.level2EntityFixedPoolSlots_
            && a.level2AllocationListOrder_==b.level2AllocationListOrder_
            && a.playableSystemOldActorSlots_==b.playableSystemOldActorSlots_,"B changed physical alias/membership");
    }
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
        const auto a=left->advanceGameplay(input),b=right->advanceGameplay(input);
        comparison.compare(*owner,*right->session(),a,b,test::CampaignComparisonStage::boundary);
        GameplayModelTestAccess::compare(owner->model(),right->session()->model());
        const auto x=owner->synth().callbackWorkState(),y=right->session()->synth().callbackWorkState();
        requireCampaignComparison(x.program && y.program && x.program->bomb7CallerRow && y.program->bomb7CallerRow
            && x.program->bomb7CallerRow->values()==y.program->bomb7CallerRow->values(),
            "B row owner values differ between actual startup budgets");
        requireCampaignComparison(left->session()==owner,"Bomb replaced retained session owner");
        return a;
    };
    advance({});
    const auto leftRow=*owner->synth().callbackWorkState().program->bomb7CallerRow;
    const auto rightRow=*right->session()->synth().callbackWorkState().program->bomb7CallerRow;
    const auto initial=owner->model().level1State().systemStatusLevels()[3];
    requireCampaignComparison(initial==3,"actual startup B test requires FAME easy charges");
    const auto timer=owner->model().level1State().systemTimers()[3];
    for(unsigned use=0;use<static_cast<unsigned>(initial);++use) {
        const auto records=GameplayModelTestAccess::records(owner->model());
        const auto slot=playableBombSlot(records,GameplayModelTestAccess::known(owner->model()));
        requireCampaignComparison(slot.has_value(),"actual startup unexpectedly has full FE");
        const auto player=owner->model().level2Player();
        const auto expected=preparePlayableBombActor(records[*slot],player,300);
        GameplayInput input{};input.fire=use==0;
        input.systemAction=use==0 ? GameplaySystemAction::bomb : GameplaySystemAction::activateSelected;
        const auto delivery=advance(input);
        std::vector<std::uint16_t> events;
        for(const auto& event:delivery.events) {events.push_back(event.eventId);}
        const auto direct=std::find(events.begin(),events.end(),8),bomb=std::find(events.begin(),events.end(),7);
        requireCampaignComparison(direct!=events.end() && bomb!=events.end() && direct<bomb
            && std::count(events.begin(),events.end(),7)==1,"B8/7 order or event count changed");
        if(use==0) {
            requireCampaignComparison(std::find(events.begin(),direct,14)!=direct,"B overtook held Space fire");
        }
        const auto actual=GameplayModelTestAccess::records(owner->model())[*slot];
        auto anticipated=expected;anticipated.allocationIndex=actual.allocationIndex;
        requireCampaignComparison(test::campaignActorFields(actual)==test::campaignActorFields(anticipated),
            "actual B lost physical4506 partial record or one3B3A birth");
        requireCampaignComparison(owner->model().level1State().systemStatusLevels()[3]==initial-use-1
            && owner->model().level1State().systemTimers()[3]==timer,"B invented timer/negative status");
    }
    requireCampaignComparison(*owner->synth().callbackWorkState().program->bomb7CallerRow==leftRow
        && *right->session()->synth().callbackWorkState().program->bomb7CallerRow==rightRow,
        "repeated B replaced its trusted row generation");
    requireCampaignComparison(owner->model().level1State().selectedSystem()!=3,"last B did not normalize/cycle immediately");
    for(unsigned frame=0;frame<3;++frame) {advance({});}
    GameplayInput unknown{};unknown.systemAction=static_cast<GameplaySystemAction>(255);
    for(auto* frontend:{left.get(),right.get()}) {
        const auto samples=frontend->elapsedSamples(),ticks=frontend->session()->model().ticks();
        bool rejected=false;try {frontend->advanceGameplay(unknown);}catch(const std::invalid_argument&) {rejected=true;}
        requireCampaignComparison(rejected && frontend->elapsedSamples()==samples
            && frontend->session()->model().ticks()==ticks,"unknown action changed retained request");
    }
    std::cout<<"Actual startup B: physical FE,8/7,immediate normalization,repeat and budget twins PASS samples="
        <<comparison.leftPcm.size()<<" OPL="<<owner->synth().registerWrites().size()<<'\n';
}
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {run(argv[1]);return 0;}catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
