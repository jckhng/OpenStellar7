#include "native_level2_frame_fixture.hpp"
#include "playable_campaign_comparison.hpp"
#include <algorithm>
#include <iostream>

namespace {
using namespace stellar7;
void run(const std::filesystem::path& root,std::uint8_t bank,std::uint8_t sourceType) {
    auto a=test::makePlayableBonusPickupSession(root,bank,sourceType);
    auto b=test::makePlayableBonusPickupSession(root,bank,sourceType);
    const auto& actors=a->model().level2Entities();
    const auto pickup=std::find_if(actors.begin(),actors.end(),[](const auto& actor) {return actor.type==42;});
    test::requireCampaignComparison(pickup!=actors.end(),"native FE42 is absent");
    const auto payload=pickup->stateBytes4dTo52[5];
    auto status=a->model().level1State().systemStatusLevels();status.at(payload-1)=3;
    const auto score=a->model().score();
    const auto* synth=&a->synth();
    const auto count=[&]() {
        const auto& events=a->model().level2AudioEvents();
        return std::count_if(events.begin(),events.end(),[](const auto& event) {return event.eventId==24;});
    };
    const auto events=count();
    test::CampaignBudgetComparison comparison;
    for(unsigned frame=0;frame<3;++frame) {
        const auto first=a->elapsedSamples();
        auto x=a->advancePlayableCampaign({},10000),y=b->advancePlayableCampaign({},257);
        comparison.compare(*a,*b,x,y,test::CampaignComparisonStage::boundary);
        test::requireCampaignComparison(&a->synth()==synth&&x.firstSample==first
            &&first+x.samples.size()==a->elapsedSamples(),"bonus discarded retained synth/PCM");
        test::requireCampaignComparison(a->model().level1State().systemStatusLevels()==status
            &&a->model().score()==score+100&&count()==events+1,"native bonus repair/award was missing or repeated");
    }
    std::cout<<"native bank="<<unsigned(bank+1)<<" payload="<<unsigned(payload)<<" own collection+2 successors PASS\n";
}
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {
        for(std::uint8_t bank=0;bank<7;++bank) {run(argv[1],bank,1);}
        for(const std::uint8_t source:{4,5,6,11,13}) {run(argv[1],2,source);}
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
