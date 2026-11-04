//Controlled lethal component ancestry. This does not prove a natural campaign/title route.
#include "native_level2_frame_fixture.hpp"
#include "playable_campaign_comparison.hpp"

#include <algorithm>
#include <array>
#include <iostream>

namespace {
using namespace stellar7;
using test::requireCampaignComparison;
constexpr ProjectedPoint continueNo{161,118};
constexpr ProjectedPoint nameOk{161,139};
constexpr ProjectedPoint hallContinue{164,173};
constexpr ProjectedPoint mainBegin{163,69};
constexpr ProjectedPoint mainBriefing{163,101};
constexpr ProjectedPoint mainPreferences{163,126};
constexpr ProjectedPoint mainScores{163,151};
constexpr ProjectedPoint mainExit{163,176};
constexpr ProjectedPoint preferenceLevel{60,142};
constexpr ProjectedPoint preferenceExit{163,170};
constexpr ProjectedPoint briefingContinue{248,172};
constexpr ProjectedPoint briefingExit{67,172};
constexpr std::size_t difficultyOption=4;
constexpr std::array<std::int8_t,7> standardCharges{3,1,1,1,1,1,1};

class MemoryStore final:public HighscoreStore {
public:
    HighscoreSaveResult saveAndReload(const HighscoreTable& table) override {
        bytes=encodeHighscoreTable(table);
        ++calls;
        return {HighscoreSaveStatus::saved,decodeHighscoreTable(bytes)};
    }
    std::vector<std::uint8_t> bytes;
    unsigned calls=0;
};

GameplayInput pointer(const ProjectedPoint point,const bool down) {
    GameplayInput input;
    input.pointer=NativePointerInput{point,down,false};
    return input;
}

void expectPosition(const NativeSessionDelivery& delivery,const ProjectedPoint expected) {
    requireCampaignComparison(delivery.pointerPosition&&delivery.pointerPosition->x==expected.x
        &&delivery.pointerPosition->y==expected.y,"returned-title route lost source focus/host positioning feedback");
}

void route(const std::filesystem::path& root) {
    auto leftStore=std::make_shared<MemoryStore>();
    auto rightStore=std::make_shared<MemoryStore>();
    auto left=test::makePlayableLethalSession(root,test::PlayableScoreFixture::rankHit,leftStore,
        NativePointerCapability::portablePointer);
    auto right=test::makePlayableLethalSession(root,test::PlayableScoreFixture::rankHit,rightStore,
        NativePointerCapability::portablePointer);
    const auto* leftOwner=left.get();
    const auto* rightOwner=right.get();
    test::CampaignBudgetComparison twins;
    const auto step=[&](const GameplayInput input=GameplayInput{},const NameEntryInput text=NameEntryInput{}) {
        const auto firstSample=left->elapsedSamples();
        const auto a=left->advancePlayableCampaign(input,10000,text);
        const auto b=right->advancePlayableCampaign(input,257,text);
        twins.compare(*left,*right,a,b,test::CampaignComparisonStage::boundary);
        requireCampaignComparison(left.get()==leftOwner&&right.get()==rightOwner
            &&a.firstSample==firstSample&&firstSample+a.samples.size()==left->elapsedSamples()
            &&a.exitRequested==b.exitRequested&&!a.exitRequested,
            "returned-title route replaced retained owners, lost PCM prefix or exited");
        requireCampaignComparison(a.pointerPosition.has_value()==b.pointerPosition.has_value(),
            "returned-title budget changed pointer feedback presence");
        if(a.pointerPosition) {expectPosition(b,*a.pointerPosition);}
        requireCampaignComparison(leftStore->calls==rightStore->calls&&leftStore->bytes==rightStore->bytes,
            "returned-title budget changed exact save/reload bytes");
        requireCampaignComparison(left->highscoreTable()&&right->highscoreTable()
            &&encodeHighscoreTable(*left->highscoreTable())==encodeHighscoreTable(*right->highscoreTable()),
            "returned-title budgets changed retained configuration/score table");
        if(left->menu()||right->menu()) {
            requireCampaignComparison(left->menu()&&right->menu()
                &&left->menu()->selection()==right->menu()->selection(),"returned-title budgets changed keyboard focus");
        }
        return a;
    };
    const auto click=[&](const ProjectedPoint point) {
        step(pointer(point,true));
        return step(pointer(point,false));
    };
    for(unsigned call=0;left->model().phase()!=GameplayPhase::continuePrompt&&call<50;++call) {step();}
    requireCampaignComparison(left->model().phase()==GameplayPhase::continuePrompt,"controlled lethal route missed Continue");
    click(continueNo);
    requireCampaignComparison(left->model().phase()==GameplayPhase::campaignEnded
        &&left->model().recoveredContinueDecision()==ContinueDecision::no,"pointer NO did not return its game-over caller");
    expectPosition(step(),nameOk);
    requireCampaignComparison(left->model().phase()==GameplayPhase::highscoreNameEntry
        &&left->model().playableHighscoreCaller()==PlayableHighscoreCaller::gameOverTitle,
        "actual game-over title rank check missed the named editor caller");
    step({},{{'T','I','T','L','E'}});
    expectPosition(click(nameOk),hallContinue);
    requireCampaignComparison(left->model().phase()==GameplayPhase::highscoreHall&&leftStore->calls==1,
        "pointer name OK did not save and enter actual Hall");
    click(hallContinue);
    requireCampaignComparison(left->model().phase()==GameplayPhase::campaignEnded,
        "Hall returned to a different parent instead of game-over title");
    expectPosition(step(),mainBegin);
    requireCampaignComparison(left->model().phase()==GameplayPhase::nativeMenu&&left->menu()
        &&left->menu()->selection()==0,"actual title return did not reconstruct BEGIN");
    const auto score=left->model().score();
    const auto continues=left->model().remainingContinues();
    const auto worldTick=left->model().level2LoopTick();
    const auto rng=left->model().level2Rng();
    const auto retained=[&]() {
        requireCampaignComparison(left->model().score()==score&&left->model().remainingContinues()==continues
            &&left->model().level2LoopTick()==worldTick,"returned-title UI changed frozen campaign state");
        test::compareCampaignRng(left->model().level2Rng(),rng,test::CampaignComparisonStage::boundary);
    };
    GameplayInput up;
    up.forward=true;
    expectPosition(step(up),mainExit);
    step();
    requireCampaignComparison(left->menu()->selection()==4,"returned-title Up did not wrap to EXIT");
    GameplayInput down;
    down.reverse=true;
    expectPosition(step(down),mainBegin);
    step();
    requireCampaignComparison(left->menu()->selection()==0,"returned-title released poll did not rearm Down");
    retained();
    step(pointer(mainExit,false));
    const auto idle=left->frame();
    step(pointer(mainExit,true));
    requireCampaignComparison(left->frame().pixels!=idle.pixels,"returned-title cancellation test did not paint held EXIT");
    GameplayInput cancel;
    cancel.left=cancel.right=cancel.forward=cancel.reverse=cancel.fire=cancel.confirm=cancel.exit=true;
    cancel.pointer=NativePointerInput{{0,0},true,true,NativePointerPhase::cancel};
    expectPosition(step(cancel),mainExit);
    test::compareCampaignFrame(left->frame(),idle);
    step(pointer(mainExit,false));
    test::compareCampaignFrame(left->frame(),idle);
    requireCampaignComparison(left->model().phase()==GameplayPhase::nativeMenu&&left->menu()->selection()==0,
        "cancelled returned-title widget activated or changed keyboard focus");
    retained();
    click(mainPreferences);
    const auto beforePreferences=leftStore->bytes;
    click(preferenceLevel);
    click(preferenceExit);
    requireCampaignComparison(leftStore->calls==2&&leftStore->bytes.size()==beforePreferences.size()
        &&std::equal(beforePreferences.begin()+highscoreHeaderBytes,beforePreferences.end(),
            leftStore->bytes.begin()+highscoreHeaderBytes),"returned-title PREFS changed score records or missed save/reload");
    const auto configuration=decodeCockpitSettings(std::vector<std::uint8_t>(leftStore->bytes.begin(),
        leftStore->bytes.begin()+highscoreHeaderBytes));
    requireCampaignComparison(configuration.options[difficultyOption]==1,"returned-title LEVEL canonical save was lost");
    requireCampaignComparison(left->menu()&&left->menu()->selection()==0,"PREFS save did not reconstruct returned-title BEGIN");
    retained();
    click(mainScores);
    const auto scores=left->frame().pixels;
    click(hallContinue);
    requireCampaignComparison(left->menu()&&left->menu()->selection()==0&&left->frame().pixels!=scores,
        "returned-title SCORES did not close through its pointer widget");
    click(mainBriefing);
    const auto intro=left->frame().pixels;
    click(briefingContinue);
    requireCampaignComparison(left->frame().pixels!=intro,"returned-title Briefing did not enter actual model page");
    click(briefingExit);
    requireCampaignComparison(left->menu()&&left->menu()->selection()==0,"returned-title Briefing did not close");
    retained();
    click(mainBegin);
    requireCampaignComparison(left->model().phase()==GameplayPhase::playableLoaderReturn
        &&left->model().campaignLevel()==0&&left->model().score()==0&&left->model().playerArmor()==680
        &&left->model().runtimeDifficulty()==CampaignDifficulty::standard
        &&left->model().remainingContinues()==1&&left->model().level1State().systemStatusLevels()==standardCharges,
        "true returned-title pointer Begin lost saved difficulty or retained Level1 loader state");
    step();
    requireCampaignComparison(left->model().phase()==GameplayPhase::playablePreloopReturn,
        "returned-title Begin missed retained loader-to-preloop boundary");
    const auto firstTick=left->model().level2LoopTick();
    step();step();
    requireCampaignComparison(left->model().phase()==GameplayPhase::playable
        &&left->model().level2LoopTick()==static_cast<std::uint16_t>(firstTick+2),
        "returned-title pointer Begin did not complete two ordinary frames");
    std::cout<<"Controlled pointer NO/rank/name/Hall -> title/rearm/cancel/LEVEL/save/Begin+2 budget twins PASS\n";
}
}

int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {route(argv[1]);return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
