// Controlled model/shared-table contracts. These fixtures do not qualify cold
// startup, menu persistence, or a whole Standard/Hard campaign.
#include "stellar7/gameplay.hpp"
#include "playable_bomb.hpp"
#include "campaign_guardian_children.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <tuple>

namespace stellar7 {
struct Level1ReloadTestAccess {
    static void repair(Level1State& state,const std::uint8_t payload) {state.repairBonusSystem(payload);}
    static void reset(Level1State& state) {state.resetCampaignTransientState();}
};
struct GameplayModelTestAccess {
    static void native(GameplayModel& model) {model.playableCampaignFrames_=true;}
    static void title(GameplayModel& model) {
        model.playableCampaignFrames_=true;model.phase_=GameplayPhase::nativeMenu;
    }
    static std::uint16_t award(const GameplayModel& model,const std::uint16_t value) {
        return model.scaledCampaignAward(value);
    }
    static void repair(GameplayModel& model,const std::uint8_t payload) {Level1ReloadTestAccess::repair(model.level1_,payload);}
    static void reset(GameplayModel& model) {Level1ReloadTestAccess::reset(model.level1_);}
    static void quota(GameplayModel& model,const std::uint8_t level,const std::int16_t spawned) {
        model.activeCampaignLevel_=level;model.level2SpawnedEnemyCount_=spawned;model.level2ActiveEnemyCount_=0;
        //Controlled quota checkpoint stops before a constructor: all source slots
        //are occupied. No actor allocation or resource eligibility is claimed.
        model.level2ActorPoolActive_.fill(true);
    }
    static std::uint64_t randomCalls(const GameplayModel& model) {return model.level2Rng_.calls();}
    static void spawn(GameplayModel& model) {model.runLevel2PeriodicSpawnCheck();}
};
}
namespace {
using namespace stellar7;
void require(const bool valid,const char* message) {
    if (!valid) {throw std::runtime_error(message);}
}
template<class Function> void rejects(Function function) {
    bool rejected=false;
    try {function();} catch (const std::invalid_argument&) {rejected=true;}
    require(rejected,"invalid difficulty/phase was admitted");
}
constexpr std::array<CampaignDifficulty,3> difficulties{
    CampaignDifficulty::easy,CampaignDifficulty::standard,CampaignDifficulty::hard};
constexpr std::array<std::array<std::int8_t,7>,3> charges{{
    {{3,3,3,3,3,3,3}},{{3,1,1,1,1,1,1}},{{3,0,0,0,0,0,0}}}};
constexpr std::array<std::array<std::uint8_t,7>,3> limits{{
    {{10,12,12,14,16,18,20}},{{13,15,15,18,20,23,25}},{{15,18,18,21,24,27,30}}}};

void tablesAndScore() {
    constexpr std::array<std::uint16_t,18> awards{0,5,10,20,25,30,35,40,50,60,75,90,100,770,999,1000,1027,6674};
    constexpr std::array<std::uint16_t,18> standard{0,7,15,30,37,45,52,60,75,90,112,135,150,1155,1498,1000,1027,6674};
    constexpr std::array<std::uint16_t,18> hard{0,10,20,40,50,60,70,80,100,120,150,180,200,1540,1998,1000,1027,6674};
    for (std::size_t index=0;index<difficulties.size();++index) {
        const auto difficulty=difficulties[index];
        require(campaignInitialCharges(difficulty)==charges[index],"8CEE source charge row differs");
        require(campaignContinueBudget(difficulty)==2-index,"016B Continue budget differs");
        for (std::uint8_t level=0;level<7;++level) {
            const auto definition=campaignLevelDefinition(level,campaignDifficultyIndex(difficulty));
            const auto easy=campaignLevelDefinition(level,0);
            require(definition.table.enemyLimit==limits[index][level],"4EAA source enemy quota differs");
            require(definition.scheduler==easy.scheduler && definition.weights==easy.weights
                && definition.table.actors.size()==easy.table.actors.size(),"difficulty changed unrelated constructor tables");
            for (std::size_t actor=0;actor<definition.table.actors.size();++actor) {
                const auto& a=definition.table.actors[actor];const auto& b=easy.table.actors[actor];
                require(std::tie(a.type,a.x,a.y,a.yaw)==std::tie(b.type,b.x,b.y,b.yaw),
                    "difficulty changed initial actor metadata/placement");
            }
            require(!campaignSpawnType(definition,0,definition.table.enemyLimit,0x0100),
                "periodic selector exceeded its difficulty quota");
            constexpr std::array<std::uint8_t,7> baseHealth{15,20,20,40,50,50,20};
            const auto expected=level==6 && index!=0 ? 40*index : baseHealth[level];
            require(campaignGuardianInitialHealth(level,difficulty)==expected,"477A/47F3 guardian health differs");
        }
        for (std::size_t award=0;award<awards.size();++award) {
            const auto expected=index==0 ? awards[award] : index==1 ? standard[award] : hard[award];
            require(campaignScoreAward(awards[award],difficulty)==expected,"7970 score signed-positive truncation differs");
        }
        //Bomb source compatibility table has both sides of7977's threshold.
        require(campaignScoreAward(playableBombContactScore(23),difficulty)==1027
            && campaignScoreAward(playableBombContactScore(26),difficulty)==4106,
            "Bomb compatibility awards>=1000 were scaled");
    }
    const auto invalid=static_cast<CampaignDifficulty>(3);
    rejects([&] {campaignInitialCharges(invalid);});
    rejects([&] {campaignContinueBudget(invalid);});
    rejects([&] {campaignScoreAward(5,invalid);});
    rejects([&] {campaignScoreAward(0x8000,CampaignDifficulty::easy);});
    rejects([&] {campaignGuardianInitialHealth(7,CampaignDifficulty::easy);});
}

void finalGuardianHits() {
    for (const auto difficulty:difficulties) {
        Level1Entity guardian{};
        guardian.type=44;guardian.stateBytes4dTo52[0]=0xff;guardian.stateBytes4dTo52[4]=5;
        const auto health=campaignGuardianInitialHealth(6,difficulty);
        guardian.stateBytes4dTo52[1]=health;
        for (unsigned hit=1;hit<=health;++hit) {
            const auto result=campaignActorHit(guardian,1,6);
            require(result.lethal==(hit==health),"final guardian health exceeded existing hit-domain eligibility");
            if (result.lethal) {
                require(std::find(result.actions.begin(),result.actions.end(),CampaignActorHitAction::victory)!=result.actions.end()
                    && std::find(result.actions.begin(),result.actions.end(),CampaignActorHitAction::guardianScore)!=result.actions.end(),
                    "final guardian difficulty lost the existing victory/score child");
            }
            guardian.stateBytes4dTo52[1]=result.health;
        }
    }
}

void modelContracts() {
    for (std::size_t index=0;index<difficulties.size();++index) {
        auto model=std::unique_ptr<GameplayModel>(new GameplayModel(GameplayModel::fromNativeStartup()));
        const auto randomCalls=model->level1State().rng().calls();
        require(model->runtimeDifficulty()==CampaignDifficulty::easy,"component/source model default changed");
        const auto oldCharges=model->level1State().systemStatusLevels();
        rejects([&] {model->initializePlayableDifficulty(static_cast<CampaignDifficulty>(3));});
        require(model->runtimeDifficulty()==CampaignDifficulty::easy
            && model->level1State().systemStatusLevels()==oldCharges,"rejected difficulty mutated fresh state");
        model->initializePlayableDifficulty(difficulties[index]);
        require(model->runtimeDifficulty()==difficulties[index] && model->remainingContinues()==2-index
            && model->level1State().systemStatusLevels()==charges[index]
            && model->level1State().rng().calls()==randomCalls && model->ticks()==0,
            "fresh difficulty changed constructor RNG/phase or missed charges/budget");
        require(GameplayModelTestAccess::award(*model,5)==5,"strict source model score branch changed");
        GameplayModelTestAccess::native(*model);
        require(GameplayModelTestAccess::award(*model,5)==std::array<std::uint16_t,3>{5,7,10}[index],
            "native model did not consume selected score difficulty");
        GameplayModelTestAccess::repair(*model,3);
        auto repaired=charges[index];repaired[2]=3;
        require(model->level1State().systemStatusLevels()==repaired && model->runtimeDifficulty()==difficulties[index],
            "single-system pickup incorrectly applied difficulty row or changed selected difficulty");
        GameplayModelTestAccess::reset(*model);
        require(model->level1State().systemStatusLevels()==repaired,"level/retry transient reset refilled charges");
        for (std::uint8_t level=0;level<7;++level) {
            GameplayModelTestAccess::quota(*model,level,limits[index][level]);
            const auto calls=GameplayModelTestAccess::randomCalls(*model);
            GameplayModelTestAccess::spawn(*model);
            require(GameplayModelTestAccess::randomCalls(*model)==calls,"native quota failed to stop before RNG");
            GameplayModelTestAccess::quota(*model,level,limits[index][level]-1);
            GameplayModelTestAccess::spawn(*model);
            require(GameplayModelTestAccess::randomCalls(*model)==calls+1,"native spawn did not use selected all-level quota");
        }
        GameplayModelTestAccess::title(*model);
        const auto next=difficulties[(index+1)%difficulties.size()];
        model->preparePlayableMenuBegin(next);
        require(model->runtimeDifficulty()==next && model->remainingContinues()==campaignContinueBudget(next)
            && model->level1State().systemStatusLevels()==campaignInitialCharges(next),
            "retained title Begin kept the previous mission's difficulty");
        rejects([&] {model->initializePlayableDifficulty(CampaignDifficulty::easy);});
    }
}
}
int main() {
    try {tablesAndScore();finalGuardianHits();modelContracts();std::cout<<"difficulty shared tables/native model contracts PASS\n";return 0;}
    catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
