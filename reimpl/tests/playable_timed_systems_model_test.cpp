//Actual decoded common-loader ancestry; no victory claim.
#include "stellar7/gameplay.hpp"
#include "playable_jump.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace {
void require(const bool condition,const char* message) {
    if(!condition) {throw std::runtime_error(message);}
}
std::vector<stellar7::Model3d> models(const std::filesystem::path& root,const char* name) {
    std::ifstream file(root/name,std::ios::binary);
    require(static_cast<bool>(file),"missing actual timed-system model resource");
    return stellar7::decodeFirstModelTableResource({std::istreambuf_iterator<char>(file),{}});
}
}

namespace stellar7 {
struct Level1ReloadTestAccess {
    static void beforeEntry(Level1State& state) {
        state.systemStatusLevels_.fill(3);
        state.systemTimers_[3]=17;
    }
    static void contactAbilities(Level1State& state,const bool turbo) {
        state.systemTimers_[1]=500;state.systemStatusLevels_[1]=-2;
        if(turbo) {state.systemTimers_[6]=200;state.systemStatusLevels_[6]=-2;}
    }
};
struct GameplayModelTestAccess {
    static void initialize(GameplayModel& model,const std::vector<Model3d>& shared,
        const std::vector<Model3d>& local) {
        Level1ReloadTestAccess::beforeEntry(model.level1_);
        StellarRng rng;
        auto actors=constructCampaignLoaderActors(campaignLevelDefinition(3,0).table,rng,shared,local);
        model.playableCampaignFrames_=true;
        model.initializeRetainedCampaignLevel(3,std::move(actors),rng,shared,local);
        model.completePlayableCampaignPreloop();
    }
    static void reload(GameplayModel& model,const std::vector<Model3d>& shared,
        const std::vector<Model3d>& local) {
        auto rng=model.level2Rng_;
        auto actors=constructCampaignLoaderActors(campaignLevelDefinition(3,0).table,rng,shared,local);
        model.initializeRetainedCampaignLevel(3,std::move(actors),rng,shared,local);
    }
    static std::size_t declaredShieldContact(GameplayModel& model,const std::vector<Model3d>& shared,
        const std::vector<Model3d>& local,const bool turbo) {
        initialize(model,shared,local);
        //Declared source first-contact entry, not a naturally reached fight.
        //The actual constructor supplies model/profile/physical-slot ancestry.
        auto& target=model.level2Entities_.front();
        require(target.type==9,"unexpected actual Level4 first constructor");
        target.position={};target.forwardVelocity=0;target.yawVelocity=0;
        target.stateBytes4dTo52[1]=6;
        const auto slot=model.level2EntityActorPoolSlots_.front();
        model.level2ActorPool_[slot]=target;
        model.level2ActiveListOrder_=model.level2AllocationListOrder_;
        Level1ReloadTestAccess::contactAbilities(model.level1_,turbo);
        return slot;
    }
    static const Level1Entity& actor(const GameplayModel& model,const std::size_t slot) {
        return model.level2ActorPool_[slot];
    }
};
}

namespace {
using namespace stellar7;

std::vector<std::uint16_t> tail(GameplayModel& model,const GameplayInput& input) {
    model.advancePlayableCampaign(input);
    std::vector<std::uint16_t> events;
    for(const auto& effect:model.finishPlayableCampaignFrame()) {
        if(const auto* audio=std::get_if<SystemAudio>(&effect)) {events.push_back(audio->event);}
    }
    return events;
}

void actualPrimary(const std::vector<Model3d>& shared,const std::vector<Model3d>& local) {
    auto model=std::make_unique<GameplayModel>();
    GameplayModelTestAccess::initialize(*model,shared,local);
    GameplayInput activate{};activate.fire=true;activate.systemAction=GameplaySystemAction::superPrimary;
    const auto events=tail(*model,activate);
    require(events==std::vector<std::uint16_t>({14,8}),"S occurred before heldSpace05D4");
    require(model->level1State().systemTimers()[5]==500
        && model->level1State().systemStatusLevels()[5]==-2,"S activation canonical state mismatch");
    require(std::any_of(model->level2Entities().begin(),model->level2Entities().end(),[](const auto& actor) {
        return actor.active() && actor.ownerIsPlayer && actor.type==23 && actor.stateBytes4dTo52[2]==0;
    }),"activation tail replaced its earlier normal FD");
    for(int tick=0;tick<8;++tick) {tail(*model,{});}
    require(model->level2Player().weaponCooldown==0,"normal primary cooldown reset before super branch");
    const auto priorSlots=model->level2Player().weaponSlotTimers;
    GameplayInput fire{};fire.fire=true;
    const auto fired=tail(*model,fire);
    require(fired==std::vector<std::uint16_t>{0},"super primary omitted exact event0");
    require(model->level2Player().weaponCooldown==4,"super cooldown5 omitted same-tail decrement");
    const auto super=std::find_if(model->level2Entities().begin(),model->level2Entities().end(),[](const auto& actor) {
        return actor.active() && actor.ownerIsPlayer && actor.type==23 && actor.stateBytes4dTo52[2]==1;
    });
    require(super!=model->level2Entities().end() && super->stateBytes4dTo52[1]==80
        && super->forwardVelocity==500 && super->collisionRadius==campaignModelRadius(shared,local,23),
        "super primary lost actual FD lifetime/model/speed");
    for(std::size_t slot=0;slot<priorSlots.size();++slot) {
        require(model->level2Player().weaponSlotTimers[slot]==(priorSlots[slot]==0 ? 0 : priorSlots[slot]-1),
            "super primary rewrote a normal cannon slot");
    }
}

void actualTimerControls(const std::vector<Model3d>& shared,const std::vector<Model3d>& local) {
    auto model=std::make_unique<GameplayModel>();
    GameplayModelTestAccess::initialize(*model,shared,local);
    GameplayInput turbo{};turbo.reverse=true;turbo.systemAction=GameplaySystemAction::turbo;
    require(tail(*model,turbo)==std::vector<std::uint16_t>({8,20}),"T callback audio source order changed");
    require(model->level2Player().forwardVelocity==40 && model->level1State().systemTimers()[6]==200,
        "T activation failed immediate same-tail forward branch");
    GameplayInput jump{};jump.systemAction=GameplaySystemAction::jump;
    tail(*model,jump);
    require(model->level2Player().forwardVelocity==40 && model->level1State().systemTimers()[6]==199
        && model->level1State().systemTimers()[2]==220,"T bypassed source12D2 horizontal gate");
    GameplayModelTestAccess::reload(*model,shared,local);
    require(model->level1State().systemTimers()[2]==0 && model->level1State().systemTimers()[6]==0
        && model->level1State().systemTimers()[3]==17 && model->level2Player().position.z==0,
        "actual48D4 reset mixed independent timers or retained12D4");
    model->completePlayableCampaignPreloop();
    require(model->level1State().systemStatusLevels()[2]==2 && model->level1State().systemStatusLevels()[6]==2,
        "8F1C reset refilled remaining ability charges");
}

void actualShieldContact(const std::vector<Model3d>& shared,const std::vector<Model3d>& local) {
    for(const bool turbo:{false,true}) {
        auto model=std::make_unique<GameplayModel>();
        const auto slot=GameplayModelTestAccess::declaredShieldContact(*model,shared,local,turbo);
        const auto score=model->score();
        model->advancePlayableCampaign({});
        const auto& actor=GameplayModelTestAccess::actor(*model,slot);
        require(actor.stateBytes4dTo52[1]==(turbo ? 0 : 1),"first-contact1176 lost shield5/turbo15");
        require(model->score()==score+(turbo ? 30 : 0),"shield first-contact ordinary points mismatch");
        const auto& events=model->level2AudioEvents();
        if(turbo) {
            const auto death=std::find_if(events.begin(),events.end(),[](const auto& event) {return event.eventId==18;});
            const auto shield=std::find_if(events.begin(),events.end(),[](const auto& event) {return event.eventId==21;});
            require(death!=events.end() && shield!=events.end() && death<shield,"shield lethal21 precedes1176 death18");
        } else {
            require(std::none_of(events.begin(),events.end(),[](const auto& event) {return event.eventId==21;}),
                "nonlethal shield contact emitted21");
        }
        model->finishPlayableCampaignFrame();
    }
}
} //namespace

int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    const auto shared=models(argv[1],"COCKPIT.RES");
    const auto local=models(argv[1],"LEVEL4.RES");
    actualPrimary(shared,local);actualTimerControls(shared,local);actualShieldContact(shared,local);
}
