//Controlled lethal component ancestry; actual natural ending/title qualification is separate.
#include "native_level2_frame_fixture.hpp"
#include "playable_startup_resources.hpp"
#include "save_failure_route_extension.hpp"
#include <iostream>

namespace {
using namespace stellar7;
using test::requireCampaignComparison;
GameplayInput pointer(const ProjectedPoint point,const bool down) {
    GameplayInput input;input.pointer=NativePointerInput{point,down,false};return input;
}
void run(const std::filesystem::path& root,const HighscoreSaveStatus status) {
    auto leftStore=std::make_shared<test::SaveFailureTestStore>(),rightStore=std::make_shared<test::SaveFailureTestStore>();
    leftStore->status=rightStore->status=status;
    auto left=test::makePlayableLethalSession(root,test::PlayableScoreFixture::rankHit,leftStore,NativePointerCapability::portablePointer);
    auto right=test::makePlayableLethalSession(root,test::PlayableScoreFixture::rankHit,rightStore,NativePointerCapability::portablePointer);
    test::CampaignBudgetComparison comparison;
    const auto* owner=left.get();const auto* twin=right.get();
    const auto step=[&](const GameplayInput input=GameplayInput{},const NameEntryInput text=NameEntryInput{}) {
        const auto a=left->advancePlayableCampaign(input,10000,text),b=right->advancePlayableCampaign(input,257,text);
        comparison.compare(*left,*right,a,b,test::CampaignComparisonStage::boundary);
        requireCampaignComparison(left.get()==owner&&right.get()==twin&&!a.exitRequested&&!b.exitRequested,
            "failed rank save replaced retained session or exited");
    };
    const auto click=[&](const ProjectedPoint point) {step(pointer(point,true));step(pointer(point,false));};
    for(unsigned call=0;left->model().phase()!=GameplayPhase::continuePrompt&&call<50;++call) {step();}
    requireCampaignComparison(left->model().phase()==GameplayPhase::continuePrompt,"rank fixture missed actual Continue");
    click({161,118});step();
    requireCampaignComparison(left->model().phase()==GameplayPhase::highscoreNameEntry,"rank fixture missed name editor");
    step({},{{'S','A','V','E'}});click({161,139});
    const auto assets=naturalRoute::assets(root);
    test::requireFailedRankNotice(*left,assets);test::requireFailedRankNotice(*right,assets);
    requireCampaignComparison(leftStore->calls==1&&rightStore->calls==1&&leftStore->persisted.empty()
        &&rightStore->persisted.empty()&&left->highscoreTable()&&right->highscoreTable()
        &&encodeHighscoreTable(*left->highscoreTable())==leftStore->attempted
        &&encodeHighscoreTable(*right->highscoreTable())==rightStore->attempted,
        "failed rank save lost the earned name/score/header or pretended to persist");
    step();test::requireFailedRankNotice(*left,assets);
    click({164,173});step();
    requireCampaignComparison(left->model().phase()==GameplayPhase::nativeMenu&&left->menu()&&left->menu()->selection()==0
        &&encodeHighscoreTable(*left->highscoreTable())==leftStore->attempted,
        "failed rank save did not return retained earned score to title");
}
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {
        for(const auto status:{HighscoreSaveStatus::openFailed,HighscoreSaveStatus::writeFailed,HighscoreSaveStatus::reloadFailed}) {
            run(argv[1],status);
        }
        std::cout<<"Controlled failed rank save preserves earned score/notice/Hall/title at both budgets PASS\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
