//Controlled component pose: an inherited actor contact precedes a player hit.
#include "stellar7/gameplay.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace {
void require(const bool condition,const char* message) {
    if(!condition) {throw std::runtime_error(message);}
}
std::vector<stellar7::Model3d> models(const std::filesystem::path& root,const char* name) {
    std::ifstream file(root/name,std::ios::binary);
    require(bool(file),"missing actual model resource");
    return stellar7::decodeFirstModelTableResource({std::istreambuf_iterator<char>(file),{}});
}
enum class PriorContact { absent,actor };
}

namespace stellar7 {
struct GameplayModelTestAccess {
    static std::unique_ptr<GameplayModel> entry(const std::vector<Model3d>& shared,
        const std::vector<Model3d>& local,const PriorContact prior) {
        auto table=campaignLevelDefinition(4,0).table;
        table.actors={{13,0,0,0},{13,1,1,0}};
        StellarRng rng;
        auto actors=constructCampaignLoaderActors(table,rng,shared,local);
        auto& actor=actors.front();
        require(actor.type==13&&actor.collisionRadius==559,"unexpected actual Type13 constructor");
        actor.position={0,800,0};
        actors.back().position={30000,30000,0};
        actor.forwardVelocity=actor.xVelocity=actor.yawVelocity=0;
        actor.yVelocity=-224;
        actor.collisionContactAllocationIndex=prior==PriorContact::actor ? 1 : static_cast<std::size_t>(-1);
        actor.collisionContactIsPlayer=false;
        auto model=std::make_unique<GameplayModel>();
        model->playableCampaignFrames_=true;
        model->initializeRetainedCampaignLevel(4,std::move(actors),rng,shared,local);
        model->completePlayableCampaignPreloop();
        model->level2ActiveListOrder_=model->level2AllocationListOrder_;
        require(model->level2Entities_.front().position.y==800
            &&model->level2Entities_.front().collisionContactAllocationIndex==(prior==PriorContact::actor ? 1 : static_cast<std::size_t>(-1)),
            "initializer changed the controlled contact");
        require(model->level2Player_.position.x==0&&model->level2Player_.position.y==0,
            "controlled contact lost its player origin");
        return model;
    }
    static void retired(const GameplayModel& model) {
        const auto& actor=model.level2ActorPool_.front();
        require(!model.level2ActorPoolActive_.front()&&actor.type==13&&actor.stateBytes4dTo52[1]==0,
            "player hit did not retire the physical Type13");
        require(actor.collisionContactIsPlayer&&actor.collisionContactAllocationIndex==static_cast<std::size_t>(-1),
            "player pointer retained an obsolete compact actor index");
        require(actor.position.y==576&&actor.position.z==0,"terminal callback changed contact pose");
        require(model.level2ActiveEnemyCount_==1,"Type13 player contact changed enemy count twice");
    }
};
}

int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {
        const auto shared=models(argv[1],"COCKPIT.RES"),local=models(argv[1],"LEVEL5.RES");
        for(const auto prior:{PriorContact::absent,PriorContact::actor}) {
            auto model=stellar7::GameplayModelTestAccess::entry(shared,local,prior);
            const auto armor=model->playerArmor();
            model->advancePlayableCampaign({});
            stellar7::GameplayModelTestAccess::retired(*model);
            require(model->playerArmor()==armor-128,"Type13 player contact lost its source damage");
            std::vector<std::uint16_t> events;
            for(const auto& event:model->level2AudioEvents()) {events.push_back(event.eventId);}
            require(events==std::vector<std::uint16_t>({6,6}),"Type13 contact lost ordered impact events");
            model->finishPlayableCampaignFrame();
            model->advancePlayableCampaign({});
            require(model->playerArmor()==armor-128,"retired Type13 hit player on its adjacent frame");
            model->finishPlayableCampaignFrame();
        }
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
