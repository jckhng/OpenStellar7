#include "playable_startup_resources.hpp"
#include "stellar7/story_scene.hpp"
#include <array>
#include <iostream>
#include <memory>
#include <optional>
#ifndef STELLAR7_LEVEL1_SPAWN_ORACLE
#error "requires the private original Level1 spawn oracle"
#endif
#include STELLAR7_LEVEL1_SPAWN_ORACLE

namespace stellar7 {
struct GameplayModelTestAccess {
    static auto key(const GameplayModel& model,const std::size_t index) {
        constexpr std::size_t fixedPoolKeyBase=100,projectilePoolKeyBase=200,effectPoolKeyBase=300;
        const auto ff=model.level2EntityActorPoolSlots_[index],fe=model.level2EntityFixedPoolSlots_[index];
        const auto fd=model.level2EntityProjectilePoolSlots_[index],fc=model.level2EntityDeathEffectPoolSlots_[index];
        return ff<model.level2ActorPoolSize_ ? ff : fe<model.level2FixedObjectPoolSize_ ? fixedPoolKeyBase+fe
            : fd<model.level2ProjectilePoolSize_ ? projectilePoolKeyBase+fd : effectPoolKeyBase+fc;
    }
    static void check(const GameplayModel& model,const SpawnExpected& expected) {
        const auto require=[](const bool value,const char* text) {if(!value){throw std::runtime_error(text);}};
        require(model.level2LoopTick_==expected.tick+1,"spawn regression has wrong source tick boundary");
        require(model.playerArmor()==expected.armor,"source Level1 projectile damage diverged");
        require(model.level2ActiveEnemyCount_==expected.active && model.level2SpawnedEnemyCount_==expected.spawned,
            "source Level1 spawn counts diverged");
        const auto& rng=model.level2Rng_;
        require(rng.indexA()==expected.a && rng.indexB()==expected.b && rng.last()==expected.last,"source RNG cursor diverged");
        StellarRng oracle(expected.words,expected.a,expected.b,expected.last,rng.calls());
        auto actual=rng;
        for(unsigned word=0;word<StellarRng::wordCount;++word) {
            require(actual.next()==oracle.next(),"source RNG storage diverged");
        }
        std::vector<std::size_t> allocations,active;
        for(const auto index:model.level2AllocationListOrder_) {allocations.push_back(key(model,index));}
        for(const auto index:model.level2ActiveListOrder_) {active.push_back(key(model,index));}
        require(allocations==expected.allocations,"source physical spawn allocation order diverged");
        require(active==expected.activeOrder,"source physical spawn traversal order diverged");
        require(!model.level2ProjectilePoolActive_[0],"hostile projectile survived source player contact");
        require(model.level2ActorPool_[1].position.z==expected.type11Altitude
            && model.level2ActorPool_[1].stateBytes4dTo52[5]==expected.type11Behavior,
            "flying Type11 consumed an unsupported source contact");
        if(expected.spawnedPosition) {
            const auto& actor=model.level2ActorPool_[2];
            require(model.level2ActorPoolActive_[2] && actor.type==1 && actor.collisionRadius==1342,
                "source ordinary spawn identity diverged");
            require(std::array<std::int32_t,3>{actor.position.x,actor.position.y,actor.position.z}==*expected.spawnedPosition,
                "source ordinary spawn position or retained movement diverged");
        }
    }
};
}
namespace {
using namespace stellar7;
class MemoryStore final:public HighscoreStore {
public:
    HighscoreSaveResult saveAndReload(const HighscoreTable& table) override {
        return {HighscoreSaveStatus::saved,decodeHighscoreTable(encodeHighscoreTable(table))};
    }
};
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) {throw std::runtime_error("requires actual resources");}
        const std::filesystem::path root=argv[1];
        auto game=naturalRoute::assets(root);
        game.storyScenes=std::make_shared<const StorySceneAssets>(decodeStorySceneAssets(
            naturalRoute::read(root/"SCENEB1.RES"),naturalRoute::read(root/"SCENEA.RES"),
            {naturalRoute::read(root/"SCENEB2.RES"),naturalRoute::read(root/"SCENEB3.RES"),naturalRoute::read(root/"SCENEB5.RES")},
            naturalRoute::read(root/"SCENEC.RES"),naturalRoute::read(root/"VOICE.RES")));
        game.attractVoiceAvailable=true;
        const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
        const auto drivers=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
        auto frontend=std::make_unique<RecoveredFrontend>(game,art,decodeFrameCopyProfile(drivers.at(0)),10000,std::make_shared<MemoryStore>(),
            CampaignExecution::playable,NativePointerCapability::portablePointer,NativeMenuFeatures::releaseMenus);
        unsigned polls=0;
        for(unsigned call=0;!frontend->session()&&call<100000;++call) {
            NativeMenuInput input{};
            if(frontend->startup().phase()==NativeStartupPhase::awaitingMenuDecision) {input.confirm=polls++==1;}
            frontend->advanceStartup(input);
        }
        if(!frontend->session()) {throw std::runtime_error("actual menu Begin failed");}
        std::size_t checked=0;
        for(unsigned frame=0;frame<=expectedSpawns.back().tick;++frame) {
            frontend->advanceGameplay({});
            if(frame==expectedSpawns[checked].tick) {
                GameplayModelTestAccess::check(frontend->session()->model(),expectedSpawns[checked++]);
            }
        }
        if(checked!=expectedSpawns.size()) {throw std::runtime_error("spawn regression missed source boundaries");}
        std::cout<<"actual cold Begin Level1 spawn boundaries33/43/49/50/65 pass\n";
    }catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';return 1;
    }
}
