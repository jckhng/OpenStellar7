#include "playable_startup_resources.hpp"
#include "playable_campaign_comparison.hpp"
#include <algorithm>
#include <iostream>

namespace stellar7 {
struct GameplayModelTestAccess {
    static std::int16_t spawned(const GameplayModel& model) {return model.level2SpawnedEnemyCount_;}
};
}

namespace {
using namespace stellar7;
using test::requireCampaignComparison;
constexpr std::size_t difficultyOption=4;
constexpr std::array<std::array<std::int8_t,7>,3> initialCharges{{
    {{3,3,3,3,3,3,3}},{{3,1,1,1,1,1,1}},{{3,0,0,0,0,0,0}}}};

class MemoryStore final:public HighscoreStore {
public:
    HighscoreSaveResult saveAndReload(const HighscoreTable& table) override {
        bytes=encodeHighscoreTable(table);++calls;
        return {HighscoreSaveStatus::saved,decodeHighscoreTable(bytes)};
    }
    std::vector<std::uint8_t> bytes;
    unsigned calls=0;
};

std::unique_ptr<RecoveredFrontend> menu(const GameplayAssets& data,const NativeStartupAssets& art,
    FrameCopyProfile copy,std::size_t budget,std::shared_ptr<HighscoreStore> store={}) {
    auto result=std::make_unique<RecoveredFrontend>(data,art,copy,budget,std::move(store),CampaignExecution::playable);
    for(unsigned call=0;result->startup().phase()!=NativeStartupPhase::awaitingMenuDecision&&call<10000;++call) {
        result->advanceStartup({});
    }
    requireCampaignComparison(result->startup().phase()==NativeStartupPhase::awaitingMenuDecision,
        "difficulty startup missed actual menu");
    return result;
}

void begin(RecoveredFrontend& frontend,unsigned difficulty) {
    frontend.advanceStartup({false,false,true});frontend.advanceStartup({});
    const auto* receipt=frontend.startup().recoveredBegin();
    requireCampaignComparison(receipt&&receipt->difficulty==initialCharges.at(difficulty),
        "Begin did not use its selected seven-charge row");
    for(unsigned call=0;!frontend.session()&&call<10000;++call) {frontend.advanceStartup({});}
    requireCampaignComparison(frontend.session()!=nullptr,"difficulty Begin missed retained loader");
    frontend.advanceGameplay({});
    requireCampaignComparison(frontend.session()->model().remainingContinues()==2-difficulty
        &&frontend.session()->model().level1State().systemStatusLevels()==initialCharges.at(difficulty),
        "difficulty loader/adoption lost source charges or Continue budget");
}

void run(const std::filesystem::path& root) {
    const auto data=naturalRoute::assets(root);
    const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto vga=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    const auto copy=decodeFrameCopyProfile(vga.at(0));
    for(unsigned difficulty=1;difficulty<=2;++difficulty) {
        auto store=std::make_shared<MemoryStore>();auto ui=menu(data,art,copy,10000,store);
        const auto pulse=[&](NativeMenuInput input) {ui->advanceStartup(input);ui->advanceStartup({});};
        pulse({false,true,false});pulse({false,true,false});pulse({false,false,true});
        pulse({true,false,false});pulse({false,false,false,true}); //EXIT -> STORY -> LEVEL.
        for(unsigned change=0;change<difficulty;++change) {pulse({false,false,true});}
        requireCampaignComparison(ui->startup().recoveredMenuResources()->configuration.options[difficultyOption]
            ==static_cast<std::int16_t>(difficulty),"LEVEL has no effective difficulty consumer");
        requireCampaignComparison(ui->startup().recoveredSettings()->settings.configuration.options[difficultyOption]==0,
            "difficulty edit changed immutable startup settings");
        if(difficulty==2) {
            pulse({true,false,false});pulse({false,false,true});pulse({false,true,false});
        }
        pulse({false,true,false});pulse({false,false,true}); //LEVEL -> EXIT/save.
        requireCampaignComparison(store->calls==1,"difficulty Exit did not save/reload");
        const auto original=encodeHighscoreTable(*data.highscoreTable);
        requireCampaignComparison(std::equal(original.begin()+highscoreHeaderBytes,original.end(),
            store->bytes.begin()+highscoreHeaderBytes),"difficulty save changed score/name bytes");
        begin(*ui,difficulty);
        auto saved=data;saved.highscoreTable=decodeHighscoreTable(store->bytes);
        saved.cockpitSettings=decodeCockpitSettings(std::vector<std::uint8_t>(store->bytes.begin(),
            store->bytes.begin()+highscoreHeaderBytes));
        auto left=menu(saved,art,copy,10000),right=menu(saved,art,copy,257);
        begin(*left,difficulty);begin(*right,difficulty);
        test::CampaignBudgetComparison twins;
        const auto initialSpawned=GameplayModelTestAccess::spawned(left->session()->model());
        bool allocated=false;
        constexpr unsigned maximumSpawnPolls=512;
        unsigned frames=0;
        for(;frames<maximumSpawnPolls;++frames) {
            GameplayInput control{};control.fire=true;
            const auto a=left->advanceGameplay(control),b=right->advanceGameplay(control);
            twins.compare(*left->session(),*right->session(),a,b,test::CampaignComparisonStage::boundary);
            const auto& model=left->session()->model();
            requireCampaignComparison(model.runtimeDifficulty()==static_cast<CampaignDifficulty>(difficulty),
                "periodic gameplay lost selected difficulty");
            allocated=allocated||GameplayModelTestAccess::spawned(model)>initialSpawned;
            if(allocated&&frames>=15) {break;}
        }
        requireCampaignComparison(allocated,"selected difficulty never reached a natural periodic allocation");
        std::cout<<"Difficulty"<<difficulty<<" natural periodic allocation within "<<frames+1
            <<" own calls; selected quota/charges/Continue and retained twins preserved\n";
    }
    std::cout<<"LEVEL UI/save/coldBegin/charges/Continue/budget twins PASS\n";
}
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {run(argv[1]);return 0;}catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
