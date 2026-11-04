//Declared component entry with actual decoded models and typed constructor.
#include "stellar7/gameplay.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
void require(const bool condition,const char* message) {
    if(!condition) {throw std::runtime_error(message);}
}
std::vector<stellar7::Model3d> models(const std::filesystem::path& root,const char* name) {
    std::ifstream file(root/name,std::ios::binary);
    require(static_cast<bool>(file),"missing actual model resource");
    return stellar7::decodeFirstModelTableResource({std::istreambuf_iterator<char>(file),{}});
}
enum class ShieldContact { normal, turbo };
}

namespace stellar7 {
struct Level1ReloadTestAccess {
    static void charge(Level1State& state) {state.systemStatusLevels_.fill(3);}
    static void activate(Level1State& state,const ShieldContact contact) {
        state.systemTimers_[1]=500;
        state.systemStatusLevels_[1]=-2;
        if(contact==ShieldContact::turbo) {
            state.systemTimers_[6]=200;
            state.systemStatusLevels_[6]=-2;
        }
    }
};
struct GameplayModelTestAccess {
    static std::size_t enter(GameplayModel& model,const std::vector<Model3d>& shared,
        const std::vector<Model3d>& local,const ShieldContact contact,const std::uint8_t health) {
        Level1ReloadTestAccess::charge(model.level1_);
        auto table=campaignLevelDefinition(1,0).table;
        table.actors={{4,0,0,0}};
        StellarRng rng;
        auto actors=constructCampaignLoaderActors(table,rng,shared,local);
        require(actors.size()==1 && actors.front().type==4,"unexpected typed Type4 constructor");
        //Actual HP2 and an injured HP1 entry exercise the same retained contact.
        auto& actor=actors.front();
        require(actor.stateBytes4dTo52[1]==2 && (health==1 || health==2),"unexpected source Type4 health");
        actor.stateBytes4dTo52[1]=health;
        actor.forwardVelocity=0;
        actor.yawVelocity=0;
        actor.xVelocity=0;
        actor.yVelocity=0;
        model.playableCampaignFrames_=true;
        model.initializeRetainedCampaignLevel(1,std::move(actors),rng,shared,local);
        model.completePlayableCampaignPreloop();
        model.level2ActiveListOrder_=model.level2AllocationListOrder_;
        Level1ReloadTestAccess::activate(model.level1_,contact);
        return model.level2EntityActorPoolSlots_.front();
    }
    static void retired(const GameplayModel& model,const std::size_t slot) {
        require(model.level2ActorPool_[slot].type==4
            && model.level2ActorPool_[slot].stateBytes4dTo52[1]==0
            && !model.level2ActorPoolActive_[slot],"1176 lost retired physical Type4 identity");
        require(model.level2ActiveEnemyCount_==0,"Type4 enemy count changed twice");
    }
};
}

int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {
    const auto shared=models(argv[1],"COCKPIT.RES");
    const auto local=models(argv[1],"LEVEL2.RES");
    for(const auto health:{1,2}) {
    for(const auto contact:{ShieldContact::normal,ShieldContact::turbo}) {
        auto model=std::make_unique<stellar7::GameplayModel>();
        const auto slot=stellar7::GameplayModelTestAccess::enter(*model,shared,local,contact,static_cast<std::uint8_t>(health));
        const auto armor=model->playerArmor();
        const auto score=model->score();
        model->advancePlayableCampaign({});
        stellar7::GameplayModelTestAccess::retired(*model,slot);
        require(model->playerArmor()==armor-100,"E suppressed negative Type4 contact after lethal1176");
        require(model->score()==score+30,"Type4 first contact omitted ordinary points");
        std::vector<std::uint16_t> events;
        for(const auto& audio:model->level2AudioEvents()) {events.push_back(audio.eventId);}
        require(events==std::vector<std::uint16_t>({18,21,6,9}),
            "Type4 lethal contact must emit1176 death, shield, negative0E7C, backout in order");
        model->finishPlayableCampaignFrame();
    }
    }
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
