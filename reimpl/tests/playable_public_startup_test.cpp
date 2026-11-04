#include "playable_startup_resources.hpp"
#include "playable_startup_pilot.hpp"
#include "playable_campaign_comparison.hpp"

#include <iostream>

namespace {
using namespace stellar7;
using namespace naturalRoute;

std::unique_ptr<RecoveredFrontend> begin(const GameplayAssets& data,const NativeStartupAssets& startup,
    const FrameCopyProfile copy,const std::size_t budget,std::vector<std::int16_t>& pcm) {
    auto frontend=std::make_unique<RecoveredFrontend>(data,startup,copy,budget,
        std::shared_ptr<HighscoreStore>{},CampaignExecution::playable);
    unsigned menuPoll=0;
    for(unsigned call=0;!frontend->session() && call<10000;++call) {
        NativeMenuInput input{};
        if(frontend->startup().phase()==NativeStartupPhase::awaitingMenuDecision) {
            input.confirm=menuPoll++==1;
        }
        const auto before=frontend->elapsedSamples();
        const auto delivery=frontend->advanceStartup(input);
        test::requireCampaignComparison(delivery.firstSample==before
            && before+delivery.samples.size()==frontend->elapsedSamples(),"startup lost PCM prefix");
        pcm.insert(pcm.end(),delivery.samples.begin(),delivery.samples.end());
    }
    test::requireCampaignComparison(frontend->session() && menuPoll==3,"actual Begin did not reach source Level1");
    return frontend;
}

void route(const std::filesystem::path& root) {
    const auto data=assets(root);
    const auto startup=decodeNativeStartupAssets(read(root/"STELART.RES"));
    const auto drivers=decodeTaggedPayloads(read(root/"STELLAR.RES"),{'V','G','A',':'});
    test::requireCampaignComparison(drivers.size()==1,"missing actual VGA driver");
    const auto copy=decodeFrameCopyProfile(drivers.front());
    std::vector<std::int16_t> leftStartup,rightStartup;
    auto left=begin(data,startup,copy,10000,leftStartup);
    auto right=begin(data,startup,copy,257,rightStartup);
    test::requireCampaignComparison(leftStartup==rightStartup,"startup budgets changed whole PCM");
    const auto* leftOwner=left->session();const auto* rightOwner=right->session();
    const auto initialContinues=leftOwner->model().remainingContinues();
    test::CampaignBudgetComparison comparison;
    comparison.leftPcm=leftStartup;comparison.rightPcm=rightStartup;
    Level1Policy level1;
    bool level1Contact=false,level2Contact=false;
    constexpr unsigned maximumCalls=12000;
    for(unsigned call=0;call<maximumCalls;++call) {
        const auto before=leftOwner->model().phase();
        const auto bank=leftOwner->model().campaignLevel();
        GameplayInput input{};
        // Preserve the recorded neutral first adoption call before using common-pool controls.
        if(before==GameplayPhase::playable || before==GameplayPhase::level2 || before==GameplayPhase::level3Native) {
            input=bank==0 ? level1(leftOwner->model()) : combatControls(leftOwner->model());
        }
        const auto samples=left->elapsedSamples();
        const auto a=left->advanceGameplay(input);
        const auto b=right->advanceGameplay(input);
        const auto& model=leftOwner->model();
        const bool reached=model.campaignLevel()==2 && model.phase()==GameplayPhase::level3Native
            && model.level2LoopTick()==3;
        const auto stage=before!=model.phase() || bank!=model.campaignLevel() || reached
            ? test::CampaignComparisonStage::boundary : test::CampaignComparisonStage::frame;
        comparison.compare(*leftOwner,*rightOwner,a,b,stage);
        test::requireCampaignComparison(left->session()==leftOwner && right->session()==rightOwner
            && a.firstSample==samples && samples+a.samples.size()==left->elapsedSamples(),
            "public route replaced session or broke PCM prefix");
        test::requireCampaignComparison(model.remainingContinues()==initialContinues,
            "natural Level3-entry regression unexpectedly needed a retry");
        if(model.phase()==GameplayPhase::level2SuccessPresentation) {
            level1Contact=level1Contact || bank==0;
            level2Contact=level2Contact || bank==1;
        }
        if(reached) {
            test::requireCampaignComparison(level1Contact && level2Contact,
                "public route bypassed natural green contacts");
            std::cout<<"Actual public startup/Begin/Level1 adoption -> natural Level3+3 twins; calls="
                <<call+1<<" samples="<<left->elapsedSamples()<<'\n';
            return;
        }
    }
    throw std::runtime_error("natural public-entry route exceeded its bounded Level3-entry calls");
}
} //namespace

int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {route(argv[1]);return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
