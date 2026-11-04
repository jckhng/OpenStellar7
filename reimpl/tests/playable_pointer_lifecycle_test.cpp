//Controlled lethal boundary on retained source owners; no natural combat/menu ancestry claim.
#include "native_level2_frame_fixture.hpp"
#include "playable_campaign_comparison.hpp"

#include <iostream>

namespace {
using namespace stellar7;
using test::requireCampaignComparison;
class MemoryStore final:public HighscoreStore {
public:
    HighscoreSaveResult saveAndReload(const HighscoreTable& table) override {
        ++calls;return {HighscoreSaveStatus::saved,decodeHighscoreTable(encodeHighscoreTable(table))};
    }
    unsigned calls=0;
};
GameplayInput pointer(const ProjectedPoint point,const bool down) {
    GameplayInput input;input.pointer=NativePointerInput{point,down,false};return input;
}
enum class PointerPackets { absent, present };
void keyboardContinue(const std::filesystem::path& root, const NativePointerCapability capability,
    const test::PlayableMouseFixture mouse, const PointerPackets packets) {
    auto session = test::makePlayableLethalSession(root,test::PlayableScoreFixture::rankMiss,{},capability,mouse);
    ProjectedPoint point{161,118};
    const auto step = [&](GameplayInput input = GameplayInput{}) {
        if (packets == PointerPackets::present) { input.pointer = NativePointerInput{point,false,false}; }
        const auto entry = session->elapsedSamples();
        const auto delivery = session->advancePlayableCampaign(input);
        requireCampaignComparison(delivery.firstSample == entry
            && entry + delivery.samples.size() == session->elapsedSamples(), "Continue keyboard lost PCM continuity");
        if (delivery.pointerPosition) { point = *delivery.pointerPosition; }
    };
    for (unsigned count = 0; session->model().phase() != GameplayPhase::continuePrompt && count < 50; ++count) { step(); }
    requireCampaignComparison(session->model().phase() == GameplayPhase::continuePrompt,"keyboard fixture missed Continue");
    step();
    GameplayInput up; up.forward = true;
    step(up); step();
    requireCampaignComparison(session->model().continueChoice() == ContinueChoice::yes,
        "released keyboard Up did not select Continue Yes");
    GameplayInput enter; enter.confirm = true;
    step(enter);
    requireCampaignComparison(session->model().recoveredContinueDecision() == ContinueDecision::pending,
        "keyboard Enter committed before release");
    step();
    requireCampaignComparison(session->model().phase() == GameplayPhase::playableLoaderReturn,
        "keyboard Continue Yes did not invoke actual retry");
}

void route(const std::filesystem::path& root) {
    auto leftStore=std::make_shared<MemoryStore>(),rightStore=std::make_shared<MemoryStore>();
    auto left=test::makePlayableLethalSession(root,test::PlayableScoreFixture::rankHit,leftStore,
        NativePointerCapability::portablePointer);
    auto right=test::makePlayableLethalSession(root,test::PlayableScoreFixture::rankHit,rightStore,
        NativePointerCapability::portablePointer);
    test::CampaignBudgetComparison twins;
    const auto step=[&](const GameplayInput input=GameplayInput{},const NameEntryInput text=NameEntryInput{}) {
        const auto a=left->advancePlayableCampaign(input,10000,text),b=right->advancePlayableCampaign(input,257,text);
        twins.compare(*left,*right,a,b,test::CampaignComparisonStage::boundary);
        requireCampaignComparison(a.pointerPosition.has_value()==b.pointerPosition.has_value(),"modal pointer feedback presence differs");
        if(a.pointerPosition) {requireCampaignComparison(a.pointerPosition->x==b.pointerPosition->x
            &&a.pointerPosition->y==b.pointerPosition->y,"modal pointer feedback differs");}
        return a;
    };
    const auto position=[](const NativeSessionDelivery& delivery,const ProjectedPoint expected) {
        requireCampaignComparison(delivery.pointerPosition&&delivery.pointerPosition->x==expected.x
            &&delivery.pointerPosition->y==expected.y,"modal entry retained previous dialog pointer instead of source focus");
    };
    NativeSessionDelivery entry;
    for(unsigned call=0;left->model().phase()!=GameplayPhase::continuePrompt&&call<50;++call) {entry=step();}
    requireCampaignComparison(left->model().phase()==GameplayPhase::continuePrompt,"controlled death missed Continue");
    position(entry,{161,118});
    GameplayInput cancel;
    cancel.left=cancel.right=cancel.forward=cancel.reverse=cancel.fire=cancel.confirm=cancel.exit=true;
    cancel.pointer=NativePointerInput{{0,0},true,true,NativePointerPhase::cancel};
    step(pointer({161,74},true));position(step(cancel,{{'X'}}),{161,74});step(pointer({161,74},false));
    requireCampaignComparison(left->model().phase()==GameplayPhase::continuePrompt
        &&left->model().recoveredContinueDecision()==ContinueDecision::pending,
        "cancelled Continue Yes activated on release");
    step(pointer({161,74},true));entry=step(pointer({161,74},false));
    requireCampaignComparison(left->model().phase()==GameplayPhase::highscoreNameEntry,"pointer Continue Yes missed ranked editor");
    position(entry,{161,139});
    step({},{{'T','E','S','T'}});const std::string name(left->highscoreNameEntry()->name());
    step(pointer({161,139},true));position(step(cancel,{{'X'}}),{161,139});step(pointer({161,139},false));
    requireCampaignComparison(left->model().phase()==GameplayPhase::highscoreNameEntry
        &&left->highscoreNameEntry()->name()==name&&leftStore->calls==0&&rightStore->calls==0,
        "cancelled name OK saved, changed name, or advanced");
    step(pointer({161,139},true));entry=step(pointer({161,139},false));
    requireCampaignComparison(left->model().phase()==GameplayPhase::highscoreHall
        &&leftStore->calls==1&&rightStore->calls==1,"pointer name OK did not save and enter Hall");
    position(entry,{164,173});
    const auto continues=left->model().remainingContinues();
    step(pointer({164,173},true));position(step(cancel),{164,173});step(pointer({164,173},false));
    requireCampaignComparison(left->model().phase()==GameplayPhase::highscoreHall
        &&left->model().remainingContinues()==continues,"cancelled Hall CONTINUE consumed retry");
    step(pointer({164,173},true));step(pointer({164,173},false));
    requireCampaignComparison(left->model().phase()==GameplayPhase::playableLoaderReturn
        &&left->model().remainingContinues()+1==continues,"pointer Hall CONTINUE did not return to its retry caller");
    std::cout<<"Controlled pointer Continue/name/Hall cancellation, focus entry and budget twins PASS\n";
}
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {
        unsigned failed = 0;
        const auto check = [&](const NativePointerCapability capability, const test::PlayableMouseFixture mouse, const PointerPackets packets) {
            try {
                keyboardContinue(argv[1],capability,mouse,packets);
                std::cout << "keyboard capability=" << static_cast<unsigned>(capability)
                    << " mouse=" << static_cast<unsigned>(mouse) << " pointer=" << static_cast<unsigned>(packets) << " PASS\n";
            } catch (const std::exception& error) {
                ++failed;
                std::cout << "keyboard capability=" << static_cast<unsigned>(capability)
                    << " mouse=" << static_cast<unsigned>(mouse) << " pointer=" << static_cast<unsigned>(packets)
                    << " FAIL " << error.what() << '\n';
            }
        };
        check(NativePointerCapability::unavailable,test::PlayableMouseFixture::disabled,PointerPackets::absent);
        for (const auto mouse : {test::PlayableMouseFixture::enabled,test::PlayableMouseFixture::disabled}) {
            for (const auto packets : {PointerPackets::absent,PointerPackets::present}) {
                check(NativePointerCapability::portablePointer,mouse,packets);
            }
        }
        route(argv[1]);
        return failed == 0 ? 0 : 1;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
