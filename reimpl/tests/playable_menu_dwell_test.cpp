#include "playable_startup_resources.hpp"
#include "playable_campaign_comparison.hpp"
#include <iostream>

namespace {
using namespace stellar7;
using test::requireCampaignComparison;
std::unique_ptr<RecoveredFrontend> menu(const GameplayAssets& assets,const NativeStartupAssets& art,
    FrameCopyProfile copy,std::size_t budget,std::vector<std::int16_t>& pcm) {
    auto frontend=std::make_unique<RecoveredFrontend>(assets,art,copy,budget,
        std::shared_ptr<HighscoreStore>{},CampaignExecution::playable);
    for(unsigned calls=0;frontend->startup().phase()!=NativeStartupPhase::awaitingMenuDecision&&calls<10000;++calls) {
        const auto delivery=frontend->advanceStartup({});pcm.insert(pcm.end(),delivery.samples.begin(),delivery.samples.end());
    }
    requireCampaignComparison(frontend->startup().phase()==NativeStartupPhase::awaitingMenuDecision,"dwell missed actual menu");
    return frontend;
}
void run(const std::filesystem::path& root) {
    const auto assets=naturalRoute::assets(root);
    const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto vga=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    const auto copy=decodeFrameCopyProfile(vga.at(0));
    test::CampaignBudgetComparison twins;
    auto left=menu(assets,art,copy,10000,twins.leftPcm),right=menu(assets,art,copy,257,twins.rightPcm);
    requireCampaignComparison(twins.leftPcm==twins.rightPcm,"dwell startup PCM differs across packets");
    const auto first=left->elapsedSamples();unsigned calls=0;
    constexpr unsigned maximumDwellPolls=100000;
    constexpr auto dwellSamples=std::uint64_t{SoundBlasterSynth::sampleRate}*90;
    const auto advance=[&](NativeMenuInput input) {
        const auto origin=left->elapsedSamples();
        const auto a=left->advanceStartup(input),b=right->advanceStartup(input);
        requireCampaignComparison(a.firstSample==origin&&a.firstSample==b.firstSample&&a.samples==b.samples
            &&left->elapsedSamples()==right->elapsedSamples(),"menu dwell lost retained PCM composition");
        twins.leftPcm.insert(twins.leftPcm.end(),a.samples.begin(),a.samples.end());
        twins.rightPcm.insert(twins.rightPcm.end(),b.samples.begin(),b.samples.end());
        test::compareCampaignFrame(left->frame(),right->frame());
    };
    try {
        while(left->elapsedSamples()-first<dwellSamples&&calls<maximumDwellPolls) {advance({});++calls;}
    } catch(const std::exception& error) {
        throw std::runtime_error("neutral menu polls="+std::to_string(calls)+" samples="
            +std::to_string(left->elapsedSamples()-first)+": "+error.what());
    }
    requireCampaignComparison(left->elapsedSamples()-first>=dwellSamples,"menu dwell poll bound exhausted");
    advance({false,false,true});advance({});
    for(unsigned call=0;!left->session()&&call<10000;++call) {advance({});}
    requireCampaignComparison(left->session()&&right->session(),"dwell Begin missed actual loader");
    for(unsigned frame=0;frame<2;++frame) {
        const auto a=left->advanceGameplay({}),b=right->advanceGameplay({});
        twins.compare(*left->session(),*right->session(),a,b,test::CampaignComparisonStage::boundary);
    }
    std::cout<<"Actual menu90s dwell/Begin/adjacent frames PASS polls="<<calls<<'\n';
}
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {run(argv[1]);return 0;}catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
