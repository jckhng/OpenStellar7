#include "playable_startup_resources.hpp"
#include "playable_campaign_comparison.hpp"

#include <algorithm>
#include <iostream>

namespace {
using namespace stellar7;
using test::requireCampaignComparison;
void append(const RecoveredFrontend& frontend,const NativeSessionDelivery& delivery,std::vector<std::int16_t>& pcm) {
    requireCampaignComparison(delivery.firstSample==pcm.size()
        &&delivery.firstSample+delivery.samples.size()==frontend.elapsedSamples(),"idle route lost contiguous PCM");
    pcm.insert(pcm.end(),delivery.samples.begin(),delivery.samples.end());
}
std::unique_ptr<RecoveredFrontend> menu(const GameplayAssets& assets,const NativeStartupAssets& art,
    const FrameCopyProfile copy,const std::size_t budget,std::vector<std::int16_t>& pcm) {
    auto frontend=std::make_unique<RecoveredFrontend>(assets,art,copy,budget,std::shared_ptr<HighscoreStore>{},
        CampaignExecution::playable,NativePointerCapability::unavailable);
    for(unsigned call=0;frontend->startup().phase()!=NativeStartupPhase::awaitingMenuDecision&&call<10000;++call) {
        const auto delivery=frontend->advanceStartup({});append(*frontend,delivery,pcm);
    }
    requireCampaignComparison(frontend->startup().phase()==NativeStartupPhase::awaitingMenuDecision,"idle route missed actual cold menu");
    return frontend;
}
void run(const std::filesystem::path& root) {
    const auto assets=naturalRoute::assets(root);
    const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto vga=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    const auto copy=decodeFrameCopyProfile(vga.at(0));
    test::CampaignBudgetComparison twins;
    auto left=menu(assets,art,copy,10000,twins.leftPcm),right=menu(assets,art,copy,257,twins.rightPcm);
    requireCampaignComparison(twins.leftPcm==twins.rightPcm,"idle cold startup budgets changed whole PCM");
    const auto initialFrame=left->frame();
    const auto firstSample=left->elapsedSamples();
    bool renewed=false;
    unsigned polls=0;
    constexpr unsigned maximumExpiryPolls=1000000;
    const auto advance=[&](const NativeMenuInput input) {
        const auto a=left->advanceStartup(input),b=right->advanceStartup(input);
        append(*left,a,twins.leftPcm);append(*right,b,twins.rightPcm);
        requireCampaignComparison(a.firstSample==b.firstSample&&a.samples==b.samples
            &&a.exitRequested==b.exitRequested&&!a.exitRequested
            &&left->elapsedSamples()==right->elapsedSamples(),"idle input budgets changed PCM or requested host exit");
    };
    try {
        while(!renewed&&polls<maximumExpiryPolls) {
            const auto before=left->startup().countdown();
            advance({});++polls;
            requireCampaignComparison(left->startup().countdown()==right->startup().countdown()
                &&left->startup().phase()==NativeStartupPhase::awaitingMenuDecision&&!left->session()
                &&left->startup().menu().selection()==0,"actual cold idling lost the retained BEGIN menu");
            renewed=left->startup().countdown()>before;
        }
    }catch(const std::exception& error) {
        throw std::runtime_error("actual unseeded idle expiry polls="+std::to_string(polls)+": "+error.what());
    }
    requireCampaignComparison(renewed,"actual cold timer did not expire and renew within the bounded native polls");
    const auto expirySamples=left->elapsedSamples()-firstSample;
    const auto* receipt=left->startup().recoveredMenuInput();
    requireCampaignComparison(receipt&&!receipt->activated&&std::any_of(receipt->work.begin(),receipt->work.end(),[](const auto& work) {
        return work.timing.menuDecisionCountdownRead==0;
    }),"idle renewal lacked an actual captured zero-count decision");
    const auto& phase=receipt->work.back().timing.phase;
    requireCampaignComparison(phase.sequenceSelection&&phase.sequenceSelection->songOrdinal==1
        &&phase.sequenceSelection->beginFadeRequests==0&&phase.callback
        &&phase.callback->sequence==assets.presentationMusicSequence,
        "idle renewal replaced retained menu audio or entered Begin/attract work");
    test::compareCampaignFrame(initialFrame,left->frame());test::compareCampaignFrame(left->frame(),right->frame());
    requireCampaignComparison(twins.leftPcm==twins.rightPcm,"whole actual expiry PCM differs across budgets");
    advance({false,false,true});advance({});
    for(unsigned call=0;!left->session()&&call<64;++call) {advance({});}
    requireCampaignComparison(left->session()&&right->session(),"post-expiry Begin missed the retained loader");
    for(unsigned frame=0;frame<2;++frame) {
        const auto a=left->advanceGameplay({}),b=right->advanceGameplay({});
        twins.compare(*left->session(),*right->session(),a,b,test::CampaignComparisonStage::boundary);
    }
    std::cout<<"Actual cold/no-pointer expiry/retained Begin+adjacent10000/257 PASS polls="<<polls
        <<" expirySamples="<<expirySamples<<'\n';
}
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {run(argv[1]);return 0;}catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
