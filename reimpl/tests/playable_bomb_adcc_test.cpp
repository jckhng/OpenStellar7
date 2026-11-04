//Declared pre-MAIN component entry with real Level1 model/pool constructors.
//No natural-wave/victory claim; all fixture writes precede the first frame.
#include "stellar7/gameplay.hpp"
#include "playable_bomb.hpp"
#include "playable_campaign_comparison.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>

namespace {
using namespace stellar7;
enum class ContactTimer { ready, waiting };
struct Owners { std::size_t guardian; std::size_t bomb; };
void require(const bool value,const char* message) {
    if(!value) {throw std::runtime_error(message);}
}
std::vector<Model3d> models(const std::filesystem::path& directory,const char* name) {
    std::ifstream input(directory/name,std::ios::binary);
    require(bool(input),"missing actual ADCC model bank");
    return decodeFirstModelTableResource({std::istreambuf_iterator<char>(input),{}});
}
std::vector<std::uint16_t> events(const GameplayModel& model,const std::size_t first) {
    std::vector<std::uint16_t> result;
    for(auto i=first;i<model.level2AudioEvents().size();++i) {result.push_back(model.level2AudioEvents()[i].eventId);}
    return result;
}
}
namespace stellar7 {
struct GameplayModelTestAccess {
    static Owners entry(GameplayModel& model,const std::vector<Model3d>& shared,
        const std::vector<Model3d>& local,const ContactTimer timer) {
        const auto definition=campaignLevelDefinition(0,0);
        StellarRng rng;
        auto actors=constructCampaignLoaderActors(definition.table,rng,shared,local);
        actors.erase(std::remove_if(actors.begin(),actors.end(),[](const auto& actor) {
            return actor.stateBytes4dTo52[0]==0xff;
        }),actors.end());
        model.playableCampaignFrames_=true;
        model.initializeRetainedCampaignLevel(0,std::move(actors),rng,shared,local);
        model.completePlayableCampaignPreloop();
        const auto slot=playableBombSlot(model.level2FixedPool_,model.level2FixedPoolKnown_);
        require(slot.has_value(),"declared ADCC entry has no source FE slot");
        auto bomb=preparePlayableBombActor(model.level2FixedPool_[*slot],model.level2Player_,
            campaignModelRadius(shared,local,43));
        bomb.position={10000,10000,0};
        const auto bombIndex=model.allocateLevel2FixedObject(bomb);
        require(bombIndex<model.level2Entities_.size(),"declared ADCC Bomb allocation failed");
        bomb.allocationIndex=bombIndex;model.level2Entities_[bombIndex]=bomb;model.level2FixedPool_[*slot]=bomb;
        Level1Entity guardian{};
        guardian.type=44;guardian.position=bomb.position;
        guardian.stateBytes4dTo52={0xff,15,2,0,5,0}; //Actual477A Level1 HP15.
        guardian.behaviorTimer=timer==ContactTimer::waiting ? 2 : 0;
        const auto index=model.allocateLevel2Actor(guardian);
        require(index<model.level2Entities_.size(),"declared ADCC guardian allocation failed");
        const auto owner=model.level2EntityActorPoolSlots_[index];
        model.level2PostWaveObjectIndex_=index;
        model.level2ActiveEnemyCount_=1;
        model.level2SpawnedEnemyCount_=static_cast<std::uint8_t>(definition.table.enemyLimit+2);
        model.level2ActiveListOrder_=model.level2AllocationListOrder_;
        return {owner,*slot};
    }
    static const Level1Entity& guardian(const GameplayModel& model,const Owners owners) {return model.level2ActorPool_[owners.guardian];}
    static const Level1Entity& bomb(const GameplayModel& model,const Owners owners) {return model.level2FixedPool_[owners.bomb];}
    static void aliases(const GameplayModel& model) {
        for(std::size_t i=0;i<model.level2Entities_.size();++i) {
            const auto actor=model.level2EntityActorPoolSlots_[i],fixed=model.level2EntityFixedPoolSlots_[i];
            const auto shot=model.level2EntityProjectilePoolSlots_[i],effect=model.level2EntityDeathEffectPoolSlots_[i];
            const auto& physical=actor<model.level2ActorPoolSize_ ? model.level2ActorPool_[actor]
                : fixed<model.level2FixedObjectPoolSize_ ? model.level2FixedPool_[fixed]
                : shot<model.level2ProjectilePoolSize_ ? model.level2ProjectilePool_[shot] : model.level2DeathEffectPool_.at(effect);
            require(test::campaignActorFields(physical)==test::campaignActorFields(model.level2Entities_[i]),
                "ADCC compact retirement lost a live physical alias");
        }
    }
};
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {
        const auto shared=models(argv[1],"COCKPIT.RES"),local=models(argv[1],"LEVEL1.RES");
        for(const auto timer:{ContactTimer::waiting,ContactTimer::ready}) {
            auto model=std::make_unique<GameplayModel>();
            const auto owners=GameplayModelTestAccess::entry(*model,shared,local,timer);
            const auto first=model->level2AudioEvents().size();
            model->advancePlayableCampaign({});
            require(GameplayModelTestAccess::bomb(*model,owners).type==43
                && GameplayModelTestAccess::bomb(*model,owners).stateBytes4dTo52[1]==0,
                "0784 ADCC must retire Bomb before delayed A6BE, even with timer2");
            require(GameplayModelTestAccess::guardian(*model,owners).stateBytes4dTo52[1]==5,
                "source1176(guardian,10) must follow ADCC(target,20)");
            require(events(*model,first)==(timer==ContactTimer::waiting ? std::vector<std::uint16_t>{18,5}
                : std::vector<std::uint16_t>{18,5,18}),"immediate/delayed source hit order differs");
            GameplayModelTestAccess::aliases(*model);
            model->finishPlayableCampaignFrame();
            if(timer==ContactTimer::waiting) {
                for(unsigned frame=0;frame<2;++frame) {
                    const auto start=model->level2AudioEvents().size();model->advancePlayableCampaign({});
                    require(events(*model,start)==(frame==0 ? std::vector<std::uint16_t>{}
                        : std::vector<std::uint16_t>{18}),"retired FE contact pointer was lost during A6BE countdown");
                    GameplayModelTestAccess::aliases(*model);model->finishPlayableCampaignFrame();
                }
            }
        }
        std::cout<<"Level0 ADCC20 before Bomb10; delayed A6BE and retired FE identity PASS\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
