#include "stellar7/gameplay.hpp"
#include "playable_bomb.hpp"
#include "playable_campaign_comparison.hpp"
#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <fstream>
#include <iterator>
#include <memory>
#include <iostream>

namespace {
using namespace stellar7;
void require(bool condition,const char* message) {if(!condition) {throw std::runtime_error(message);}}
std::vector<Model3d> models(const std::filesystem::path& root,const char* name) {
    std::ifstream file(root/name,std::ios::binary);require(bool(file),"missing Bomb models");
    return decodeFirstModelTableResource({std::istreambuf_iterator<char>(file),{}});
}
enum class Entry { birth, full, unknown, contact, class3, playerTouch, projectile, guardian };
}
namespace stellar7 {
struct Level1ReloadTestAccess {
    static void charges(Level1State& state) {state.systemStatusLevels_.fill(3);state.systemTimers_[3]=17;}
};
struct GameplayModelTestAccess {
    static void enter(GameplayModel& model,const std::vector<Model3d>& shared,
        const std::vector<Model3d>& local,const Entry entry) {
        Level1ReloadTestAccess::charges(model.level1_);
        auto table=campaignLevelDefinition(3,0).table;table.actors={{9,0,8,0},{9,8,8,0}};
        StellarRng rng;auto actors=constructCampaignLoaderActors(table,rng,shared,local);
        if(entry==Entry::contact || entry==Entry::class3 || entry==Entry::guardian) {
            for(auto& actor:actors) {actor.position={10000,10000,0};actor.forwardVelocity=0;}
            if(entry==Entry::class3) {actors.front().stateBytes4dTo52[4]=3;}
            if(entry==Entry::guardian) {
                auto& guardian=actors.front();guardian.type=44;
                guardian.collisionRadius=campaignModelRadius(shared,local,44);
                guardian.stateBytes4dTo52[1]=10;guardian.stateBytes4dTo52[4]=5;
                guardian.renderProfile=WorldActorRenderProfile::heapSelectorNoCallback;
                guardian.animatedModelPresenterEnabled=false;
            }
        }
        model.playableCampaignFrames_=true;
        model.initializeRetainedCampaignLevel(3,std::move(actors),rng,shared,local);
        model.completePlayableCampaignPreloop();
        if(entry==Entry::full) {
            model.level2FixedPoolKnown_.fill(true);
            for(auto& actor:model.level2FixedPool_) {actor.stateBytes4dTo52[1]=1;}
        } else if(entry==Entry::unknown) {
            model.level2FixedPoolKnown_[0]=false;
        } else if(entry==Entry::contact || entry==Entry::class3 || entry==Entry::guardian || entry==Entry::playerTouch || entry==Entry::projectile) {
            auto bomb=preparePlayableBombActor(model.level2FixedPool_[0],model.level2Player_,
                campaignModelRadius(shared,local,43));
            bomb.position=entry==Entry::playerTouch ? FixedPosition{} : FixedPosition{10000,10000,0};
            const auto index=model.allocateLevel2FixedObject(bomb);
            bomb.allocationIndex=index;model.level2Entities_[index]=bomb;model.level2FixedPool_[0]=bomb;
            if(entry==Entry::projectile) {
                //One real-owned FD23 in the declared pre-MAIN pool, not a replay state import.
                Level1Entity shot{};shot.type=23;shot.stateBytes4dTo52={0xfd,45,0,0,0,0};
                shot.position=bomb.position;shot.ownerIsPlayer=true;
                shot.collisionRadius=campaignModelRadius(shared,local,23);
                shot.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;
                const auto slot=model.level2Entities_.size();shot.allocationIndex=slot;
                model.level2Entities_.push_back(shot);model.level2EntityActorPoolSlots_.push_back(16);
                model.level2EntityFixedPoolSlots_.push_back(16);model.level2EntityProjectilePoolSlots_.push_back(0);
                model.level2EntityDeathEffectPoolSlots_.push_back(model.level2DeathEffectPoolSize_);model.level2ProjectilePool_[0]=shot;
                model.level2ProjectilePoolActive_[0]=true;model.level2AllocationListOrder_.push_back(slot);
            }
        }
        model.level2ActiveListOrder_=model.level2AllocationListOrder_;
    }
    static auto fixed(const GameplayModel& model) {return model.level2FixedPool_;}
    static auto known(const GameplayModel& model) {return model.level2FixedPoolKnown_;}
    static auto active(const GameplayModel& model) {return model.level2ActiveEnemyCount_;}
    static bool shotActive(const GameplayModel& model) {return model.level2ProjectilePoolActive_[0];}
};
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {
        const auto shared=models(argv[1],"COCKPIT.RES"),local=models(argv[1],"LEVEL4.RES");
        for(const auto entry:{Entry::birth,Entry::full,Entry::unknown}) {
            auto model=std::make_unique<GameplayModel>();GameplayModelTestAccess::enter(*model,shared,local,entry);
            const auto before=std::make_unique<GameplayModel>(*model);
            GameplayInput input{};input.systemAction=GameplaySystemAction::bomb;
            if(entry==Entry::unknown) {
                bool rejected=false;try {model->advancePlayableCampaign(input);} catch(const std::invalid_argument&) {rejected=true;}
                require(rejected && model->ticks()==before->ticks() && model->level2AudioEvents().empty()
                    && model->level1State().systemStatusLevels()==before->level1State().systemStatusLevels(),
                    "unknown physicalFE admission was not atomic");
                test::compareCampaignRng(model->level2Rng(),before->level2Rng(),test::CampaignComparisonStage::boundary);
                continue;
            }
            const auto records=GameplayModelTestAccess::fixed(*model);
            model->advancePlayableCampaign(input);const auto effects=model->finishPlayableCampaignFrame();
            require(model->level1State().systemStatusLevels()[3]==2
                && model->level1State().systemTimers()[3]==17,"B full/birth charge normalization changed");
            unsigned events=0;for(const auto& audio:model->level2AudioEvents()) {events+=audio.eventId==7;}
            require(events==1,"B must emit7 even when FE full");
            const auto fixed=GameplayModelTestAccess::fixed(*model);
            if(entry==Entry::full) {
                for(std::size_t index=0;index<records.size();++index) {
                    require(test::campaignActorFields(fixed[index])==test::campaignActorFields(records[index]),
                        "fullFE changed a retained physical record");
                }
            }
            else {require(fixed[0].type==43 && fixed[0].stateBytes4dTo52[1]==1
                && fixed[0].collisionRadius==300 && fixed[0].position==FixedPosition{0,-1000,0},"B birth pose/radius differs");}
        }
        for(const auto entry:{Entry::contact,Entry::class3,Entry::playerTouch,Entry::projectile,Entry::guardian}) {
            auto model=std::make_unique<GameplayModel>();GameplayModelTestAccess::enter(*model,shared,local,entry);
            const auto armor=model->playerArmor();model->advancePlayableCampaign({});
            if(entry==Entry::contact) {
                require(GameplayModelTestAccess::active(*model)==0 && model->score()==60,
                    "Bomb10 did not visit both inherited ordinary owners after compact retirement");
                require(GameplayModelTestAccess::fixed(*model)[0].stateBytes4dTo52[1]==1,"ordinary contact consumed Bomb");
            } else if(entry==Entry::class3) {
                require(GameplayModelTestAccess::active(*model)==1 && model->score()==30,"Bomb ignored source class3 exclusion");
            } else if(entry==Entry::guardian) {
                require(model->score()==1033,"Bomb guardian death omitted1000 plus raw3 plus ordinary30");
                unsigned deaths=0;for(const auto& event:model->level2AudioEvents()) {deaths+=event.eventId==12;}
                require(deaths==1 && std::any_of(model->level2Entities().begin(),model->level2Entities().end(),
                    [](const auto& actor) {return actor.type==25 && actor.active();}),
                    "Bomb guardian10 lost its single actual reward transaction");
            } else if(entry==Entry::playerTouch) {
                require(model->playerArmor()==armor && model->level2Player().forwardVelocity==0,
                    "player Bomb contact applied armor/backout");
            } else {
                require(GameplayModelTestAccess::fixed(*model)[0].stateBytes4dTo52[1]==0
                    && !GameplayModelTestAccess::shotActive(*model),"FD did not retire physical FE43 and itself");
            }
            model->finishPlayableCampaignFrame();
        }
        std::cout<<"Bomb declared components PASS\n";return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
