#include "stellar7/gameplay.hpp"
#include "playable_jump.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace {
void require(const bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
template<class Call> void rejects(Call call) {
    bool rejected=false;try {call();} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"unsupported Jump input/state was admitted");
}
void sameRng(stellar7::StellarRng left,stellar7::StellarRng right) {
    require(left.calls()==right.calls() && left.indexA()==right.indexA() && left.indexB()==right.indexB(),"Jump changed RNG cursors");
    for(std::size_t word=0;word<stellar7::StellarRng::wordCount;++word) {
        require(left.next()==right.next(),"Jump changed RNG words");
    }
}
std::vector<stellar7::Model3d> models(const std::filesystem::path& root,const char* name) {
    std::ifstream input(root/name,std::ios::binary);require(static_cast<bool>(input),"missing Jump test resource");
    return stellar7::decodeFirstModelTableResource({std::istreambuf_iterator<char>(input),{}});
}
struct NoopRenderer final:stellar7::Level2FrameRenderer {
    bool invoked=false;
    void render(stellar7::Level2RenderState) override {invoked=true;}
};
}

namespace stellar7 {
struct Level1ReloadTestAccess {
    static void retainedWord(Level1State& state) {state.systemTimers_[3]=17;}
};
struct GameplayModelTestAccess {
    static void initialize(GameplayModel& model,const std::vector<Model3d>& shared,const std::vector<Model3d>& local) {
        Level1ReloadTestAccess::retainedWord(model.level1_); //Declared before constructor entry.
        StellarRng rng;
        auto actors=constructCampaignLoaderActors(campaignLevelDefinition(3,0).table,rng,shared,local);
        model.playableCampaignFrames_=true;
        model.initializeRetainedCampaignLevel(3,std::move(actors),rng,shared,local);
        model.completePlayableCampaignPreloop();
    }
    static void reload(GameplayModel& model,const std::vector<Model3d>& shared,const std::vector<Model3d>& local) {
        //Controlled actual48D4/common-loader boundary, not a natural success claim.
        auto rng=model.level2Rng_;
        auto actors=constructCampaignLoaderActors(campaignLevelDefinition(3,0).table,rng,shared,local);
        model.initializeRetainedCampaignLevel(3,std::move(actors),rng,shared,local);
    }
};
}

namespace {
using namespace stellar7;
void strictAndLegacy() {
    auto model=std::make_unique<GameplayModel>();model->startLevel2();model->tick({});
    GameplayInput jump{};jump.systemAction=GameplaySystemAction::jump;
    const auto phase=model->phase();const auto ticks=model->ticks();const auto rng=model->level2Rng();
    const auto position=model->level2Player().position;
    rejects([&]{model->tick(jump);});
    NoopRenderer renderer;rejects([&]{model->tickWithRenderer(jump,renderer);});
    require(!renderer.invoked && model->phase()==phase && model->ticks()==ticks
        && model->level2Player().position==position,"source Jump rejection mutated its frame");
    sameRng(model->level2Rng(),rng);

}
void actualConstructorFlight(const std::vector<Model3d>& shared,const std::vector<Model3d>& local) {
    auto model=std::make_unique<GameplayModel>();auto neutral=std::make_unique<GameplayModel>();
    for(auto* current:{model.get(),neutral.get()}) {
        GameplayModelTestAccess::initialize(*current,shared,local);
        for(unsigned frame=0;frame<3;++frame) {current->advancePlayableCampaign({});current->finishPlayableCampaignFrame();}
    }
    GameplayInput input{};input.fire=true;input.forward=true;input.systemAction=GameplaySystemAction::jump;
    model->advancePlayableCampaign(input);
    
    const auto effects=model->finishPlayableCampaignFrame();
    input.systemAction=GameplaySystemAction::none;
    neutral->advancePlayableCampaign(input);neutral->finishPlayableCampaignFrame();
    sameRng(model->level2Rng(),neutral->level2Rng());
    std::vector<std::uint16_t> sounds;
    for(const auto& effect:effects) {if(const auto* sound=std::get_if<SystemAudio>(&effect)) {sounds.push_back(sound->event);}}
    require(sounds==std::vector<std::uint16_t>{14,8},"Space/J source event order changed");
    require(model->level2Player().position.z==0 && model->level2Player().forwardVelocity==0,
        "new Jump incorrectly moved the player on its activation tail");
    require(std::any_of(model->level2Entities().begin(),model->level2Entities().end(),[](const auto& actor) {
        return actor.active() && actor.ownerIsPlayer && actor.type==23 && actor.position.z==0;
    }),"simultaneous Space/J did not create its pre-J ground shot");
    for(int step=1;step<=45;++step) {
        input.fire=step<=9;
        model->advancePlayableCampaign(input);model->finishPlayableCampaignFrame();
        const auto height=step==45 ? 0 : 220*step-5*step*(step-1);
        require(model->level2Player().position.z==height && model->level2Player().zVelocity==0,
            "Jump altitude/12D2 became ordinary actor zVelocity");
        require(model->level1State().systemTimers()[3]==17,"Jump modified unrelated retained12D4");
        if(step==9) {
            require(std::any_of(model->level2Entities().begin(),model->level2Entities().end(),[](const auto& actor) {
                return actor.active() && actor.ownerIsPlayer && actor.type==23 && actor.position.z==1620;
            }),"ordinary primary constructor lost actual Jump altitude");
        }
        if(step==21) {require(model->level2Player().forwardVelocity==0,"airborne input bypassed12D2 gate");}
        if(step==22) {require(model->level2Player().forwardVelocity==10,"apexzero failed source horizontal-input branch");}
    }
    require(model->level1State().systemStatusLevels()[2]==2,"landing refilled or lost Jump charges");
    input={};input.systemAction=GameplaySystemAction::jump;
    model->advancePlayableCampaign(input);model->finishPlayableCampaignFrame();
    require(model->level1State().systemStatusLevels()[2]==-1,"second real Jump did not consume one remaining charge");
    GameplayModelTestAccess::reload(*model,shared,local);
    require(model->level2Player().position.z==0 && model->level1State().systemTimers()[2]==0
        && model->level1State().systemTimers()[3]==17,"48D4 failed Jump-only reset/retained-word ownership");
    model->completePlayableCampaignPreloop();
    require(model->level1State().systemStatusLevels()[2]==1 && model->level1State().selectedSystem()==2,
        "reset8F1C failed remaining-charge normalization/selection retention");
}
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {strictAndLegacy();actualConstructorFlight(models(argv[1],"COCKPIT.RES"),models(argv[1],"LEVEL4.RES"));}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
    std::cout<<"Declared model Jump: source rejection, input order, real constructor/airborne FD,45tails and reset\n";
}
