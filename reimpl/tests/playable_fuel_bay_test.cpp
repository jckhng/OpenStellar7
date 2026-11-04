#include "native_level2_frame_fixture.hpp"
#include "playable_campaign_comparison.hpp"
#include <algorithm>
#include <iostream>

namespace {
using namespace stellar7;
const Level1Entity* station(const GameplayModel& model) {
    const auto& actors=model.level2Entities();
    const auto found=std::find_if(actors.begin(),actors.end(),[](const auto& actor) {return actor.type==31;});
    return found==actors.end()?nullptr:&*found;
}
void run(const std::filesystem::path& root,std::uint8_t bank) {
    {
        auto rim=test::makePlayableFuelBaySession(root,bank,700);
        rim->advancePlayableCampaign({});
        test::requireCampaignComparison(station(rim->model())&&station(rim->model())->stateBytes4dTo52[2]==0
            &&rim->model().playerArmor()==200&&rim->model().level2Player().forwardVelocity==0,
            "fuel bay opened at700 or applied generic rebound");
    }
    auto a=test::makePlayableFuelBaySession(root,bank),b=test::makePlayableFuelBaySession(root,bank);
    const auto* synth=&a->synth();const auto score=a->model().score();
    test::CampaignBudgetComparison comparison;
    const auto advance=[&]() {
        const auto first=a->elapsedSamples();auto x=a->advancePlayableCampaign({},10000),y=b->advancePlayableCampaign({},257);
        comparison.compare(*a,*b,x,y,test::CampaignComparisonStage::boundary);
        test::requireCampaignComparison(test::observeFuelBay(a->model())==test::observeFuelBay(b->model()),"fuel source words differ across budgets");
        test::requireCampaignComparison(&a->synth()==synth&&x.firstSample==first&&first+x.samples.size()==a->elapsedSamples(),"fuel bay lost owned synth/PCM");
    };
    advance();
    test::requireCampaignComparison(station(a->model())&&station(a->model())->stateBytes4dTo52[2]==1,"fuel station contact rebounded instead of opening");
    test::requireCampaignComparison(a->model().playerArmor()==200&&a->model().level2Player().forwardVelocity==0
        &&a->model().level2Player().position.x==station(a->model())->position.x
        &&a->model().level2Player().position.y==station(a->model())->position.y,
        "fuel contact changed armor before0EF8 or rebounded instead of recentering with station");
    std::int32_t altitude=0;std::uint16_t yaw=station(a->model())->yaw;
    for(unsigned step=0;step<29;++step) {
        advance();altitude+=10+5*step;yaw=static_cast<std::uint16_t>(yaw+256*(step+1));
        test::requireCampaignComparison(station(a->model())&&station(a->model())->position.z==altitude
            &&station(a->model())->yaw==yaw&&a->model().playerArmor()==680,"fuel recharge/lift/spin sequence changed");
    }
    advance();
    test::requireCampaignComparison(!station(a->model())&&a->model().score()==score+100
        &&a->model().level1State().systemStatusLevels()[0]==3,"fuel completion did not retire/award/repair decoy");
    test::requireCampaignComparison(test::observeFuelBay(a->model())==std::array<std::int64_t,5>{-1,0,20,10,0},
        "fuel completion lost source0086/0088/008A reset values");
    const auto repaired=a->model().score();advance();advance();
    test::requireCampaignComparison(a->model().score()==repaired,"fuel station completed twice");
    std::cout<<"Fuel bank"<<unsigned(bank+1)<<" own30-step recovery and adjacent frames PASS\n";
}
void shieldRecovery(const std::filesystem::path& root,std::uint8_t bank) {
    auto a=test::makePlayableFuelBaySession(root,bank),b=test::makePlayableFuelBaySession(root,bank);
    test::CampaignBudgetComparison comparison;
    const auto score=a->model().score();
    for(unsigned step=0;step<33;++step) {
        GameplayInput input;
        if(step==0) {input.systemAction=GameplaySystemAction::shield;}
        if(step==1) {input.systemAction=GameplaySystemAction::turbo;}
        auto x=a->advancePlayableCampaign(input,10000),y=b->advancePlayableCampaign(input,257);
        comparison.compare(*a,*b,x,y,test::CampaignComparisonStage::boundary);
        test::requireCampaignComparison(test::observeFuelBay(a->model())==test::observeFuelBay(b->model()),
            "shield fuel recovery source words differ across budgets");
        if(step==1) {
            test::requireCampaignComparison(a->model().level1State().systemTimers()[1]!=0
                &&a->model().level1State().systemTimers()[6]!=0,"shield/Turbo inputs were not activated");
        }
    }
    test::requireCampaignComparison(!station(a->model())&&a->model().score()==score+100
        &&test::observeFuelBay(a->model())[0]==-1,"shield fuel recovery did not complete once");
    std::cout<<"Fuel bank"<<unsigned(bank+1)<<" shield/Turbo retained recovery PASS\n";
}
}
int main(int argc,char** argv) {
    if(argc<2||argc>3) {return 2;}
    try {
        if(argc==3) {shieldRecovery(argv[1],2);shieldRecovery(argv[1],5);}
        else {run(argv[1],2);run(argv[1],5);shieldRecovery(argv[1],2);shieldRecovery(argv[1],5);}
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
