//Public startup and ordinary calls only; no world-state writes.
#include "playable_startup_resources.hpp"
#include "playable_campaign_comparison.hpp"

#include <algorithm>
#include <iostream>

namespace {
using namespace stellar7;
using test::requireCampaignComparison;
constexpr std::uint16_t activationEvent=8;
constexpr std::uint16_t cycleEvent=16;
constexpr std::uint16_t superFireEvent=0;
constexpr std::uint16_t turboEvent=20;
constexpr std::uint16_t scannerEvent=22;

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
    const auto advance=[&](const GameplayInput& input,const test::CampaignComparisonStage stage) {
        const auto before=left->elapsedSamples();
        const auto a=left->advanceGameplay(input);
        const auto b=right->advanceGameplay(input);
        comparison.compare(*owner,*right->session(),a,b,stage);
        requireCampaignComparison(left->session()==owner && a.firstSample==before
            && before+a.samples.size()==left->elapsedSamples(),"timed-system frame replaced owned session/PCM");
        return a;
    };
    advance({},test::CampaignComparisonStage::boundary);
    const auto startingCharges=owner->model().level1State().systemStatusLevels();
    std::array<unsigned,26> counts{};
    const auto collect=[&](const NativeSessionDelivery& delivery) {
        for(const auto& event:delivery.events) {if(event.eventId<counts.size()) {++counts[event.eventId];}}
    };
    constexpr std::array<GameplaySystemAction,4> actions{GameplaySystemAction::shield,
        GameplaySystemAction::scanner,GameplaySystemAction::superPrimary,GameplaySystemAction::turbo};
    constexpr std::array<std::size_t,4> systems{1,4,5,6};
    for(std::size_t entry=0;entry<actions.size();++entry) {
        requireCampaignComparison(startingCharges[systems[entry]]>0,"actual settings give no requested timed-system charge");
        GameplayInput input{};input.fire=true;input.systemAction=actions[entry];
        const auto delivery=advance(input,test::CampaignComparisonStage::boundary);collect(delivery);
        const auto& state=owner->model().level1State();
        requireCampaignComparison(state.systemStatusLevels()[systems[entry]]==-(startingCharges[systems[entry]]-1)
            && state.systemTimers()[systems[entry]]==(entry==3 ? 200 : 500),"native activation spent/refilled wrong state");
        if(entry==0) {
            const auto fire=std::find_if(delivery.events.begin(),delivery.events.end(),[](const auto& event) {return event.eventId==14;});
            const auto activate=std::find_if(delivery.events.begin(),delivery.events.end(),[](const auto& event) {return event.eventId==activationEvent;});
            requireCampaignComparison(fire!=delivery.events.end() && activate!=delivery.events.end() && fire<activate,
                "heldSpace/activation lost source order");
        }
    }
    GameplayInput tab{};tab.systemAction=GameplaySystemAction::cycleSelected;
    collect(advance(tab,test::CampaignComparisonStage::boundary));
    requireCampaignComparison(owner->model().level1State().selectedSystem()==0,"Tab skipped still-charged I selection");
    //Keep the existing legal Tab path; B is supported, unknown actions remain atomic negatives.
    for(unsigned count=0;owner->model().level1State().selectedSystem()!=3 && count<7;++count) {
        collect(advance(tab,test::CampaignComparisonStage::boundary));
    }
    requireCampaignComparison(owner->model().level1State().selectedSystem()==3,"Tab did not reach B");
    GameplayInput unsupported{};unsupported.systemAction=static_cast<GameplaySystemAction>(255);
    for(auto* current:{left.get(),right.get()}) {
        const auto samples=current->elapsedSamples(),ticks=current->session()->model().ticks();
        const auto charges=current->session()->model().level1State().systemStatusLevels();
        bool rejected=false;
        try {current->advanceGameplay(unsupported);} catch(const std::invalid_argument&) {rejected=true;}
        requireCampaignComparison(rejected && current->elapsedSamples()==samples
            && current->session()->model().ticks()==ticks
            && current->session()->model().level1State().systemStatusLevels()==charges,
            "unknown action mutated its retained request");
    }
    //Restore supported selection with legal Tab only; no selected-index writes.
    for(unsigned count=0;count<7;++count) {
        collect(advance(tab,test::CampaignComparisonStage::boundary));
        const auto next=owner->model().level1State().selectedSystem();
        if(next>=0 && next!=0 && next!=3) {break;}
    }
    const auto selected=owner->model().level1State().selectedSystem();
    requireCampaignComparison(selected>=0 && selected!=0 && selected!=3,"Tab did not reach a supported remaining charge");
    GameplayInput enter{};enter.systemAction=GameplaySystemAction::activateSelected;
    collect(advance(enter,test::CampaignComparisonStage::boundary));
    for(unsigned tick=0;tick<505;++tick) {
        GameplayInput input{};input.fire=tick<70;
        collect(advance(input,tick==504 ? test::CampaignComparisonStage::boundary : test::CampaignComparisonStage::frame));
    }
    const auto& final=owner->model().level1State();
    for(const auto system:systems) {requireCampaignComparison(final.systemTimers()[system]==0,"timed ability survived native expiry");}
    requireCampaignComparison(counts[activationEvent]>=5 && counts[cycleEvent]>=2
        && counts[superFireEvent]>0 && counts[turboEvent]==1 && counts[scannerEvent]==62,
        "native direct event counts or scanner cadence changed");
    requireCampaignComparison(comparison.leftPcm==comparison.rightPcm,"whole retained timed-system PCM differs");
    std::cout<<"Actual startup E/C/S/T,Tab/Enter,Jump overlap, expiry and native budget twins PASS\n";
}
} //namespace

int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {run(argv[1]);return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
