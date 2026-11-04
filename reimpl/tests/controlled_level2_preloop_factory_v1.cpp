#include "native_level2_frame_fixture.hpp"
#include "playable_campaign_comparison.hpp"
#include "stellar7/level1_loader_work.hpp"
#include "stellar7/level2_music_work.hpp"
#include "stellar7/success_audio_work.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <sstream>

namespace stellar7 {
enum class CampaignFixtureActors { intact, clearedWave };
struct Level1ReloadTestAccess {
    static void bonusEntry(Level1State& state,std::uint8_t type) {
        state.lastDestroyedType_=type;state.consecutiveDestroyedTypeCount_=2;
        state.systemStatusLevels_.fill(1);state.systemTimers_.fill(0);state.selectedSystem_=2;
    }
    static void fuelCharge(Level1State& state) {state.systemStatusLevels_[0]=1;}
    static void successReturn(Level1State& state,std::uint16_t loop) {
        state.levelIndex_=0;state.loopTick_=loop;state.player_.position={100,-250,0};state.player_.yawVelocity=20;state.player_.forwardVelocity=30;
        state.playerArmor_=600;state.score_=3400;state.systemTimers_={0,0,0,17,0,0,0};state.selectedSystem_=-1;
        state.entities_[0].stateBytes4dTo52[1]=0;state.entities_[1].stateBytes4dTo52[1]=0;
        Level1Entity guardian{};guardian.allocationIndex=state.entities_.size();guardian.type=44;guardian.collisionRadius=2792;
        guardian.stateBytes4dTo52={255,0,2,0,5,0};guardian.renderProfile=WorldActorRenderProfile::heapSelectorNoCallback;guardian.animatedModelPresenterEnabled=false;
        state.postWaveObjectAllocationIndex_=guardian.allocationIndex;state.entities_.push_back(guardian);
        Level1Entity reward{};reward.allocationIndex=state.entities_.size();reward.type=25;reward.collisionRadius=897;reward.position={12000,4000,0};
        reward.stateBytes4dTo52={254,100,2,0,0,1};reward.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;reward.animatedModelPresenterEnabled=false;
        state.entities_.push_back(reward);state.completionObjectAllocationIndex_=static_cast<std::size_t>(-1);
        state.activeEnemyCount_=0;state.spawnedEnemyCount_=12;state.missionActive_=false;state.successState_=true;
        state.failureState_=false;state.failurePending_=false;state.exitState_=false;state.successContactPending_=false;
        state.ordinaryTailPending_=false;state.ordinaryTailConsumed_=true;state.hasAdvanced_=true;state.playerCollisionMomentum_=0;
        state.playerWeaponCooldown_=0;state.playerProjectileTimers_={};state.projectionListOrigin_={13,29,0};
        state.allocationList_.clear();state.objectListOrder_.clear();state.activeListOrder_.clear();state.semanticActiveListOrder_.clear();
        for(const std::size_t index:{2,3,4,5,7}) {
            state.allocationList_.push_back({Level1State::ObjectKind::entity,index});state.objectListOrder_.push_back({Level1State::ObjectKind::entity,index});
            state.activeListOrder_.push_back(index);state.semanticActiveListOrder_.push_back(index);
        }
        std::array<std::uint16_t,56> table{};for(unsigned i=0;i<table.size();++i) {table[i]=static_cast<std::uint16_t>(i*1193+83);}
        state.rng_=StellarRng(table,48,110,0,100);state.lastDestroyedType_=44;state.consecutiveDestroyedTypeCount_=1;
    }
};

struct GameplayModelTestAccess {
    static void bonusEntry(GameplayModel& model,std::uint8_t type) {
        Level1ReloadTestAccess::bonusEntry(model.level1_,type);
        Level1Entity source{};source.type=type;source.position=model.level2Player_.position;
        test::requireCampaignComparison(model.registerLevel5DestroyedTypeBonus(source).has_value(),
            "native controlled third kill did not allocate bonus");
        model.level2ActiveListOrder_=model.level2AllocationListOrder_;
        model.syncPlayableCampaignPools();
    }
    static std::array<std::int64_t,5> fuelState(const GameplayModel& model) {
        return {model.fuelBay_.fixedSlot?static_cast<std::int64_t>(*model.fuelBay_.fixedSlot):-1,
            model.fuelBay_.frame,model.fuelBay_.yawIncrement,model.fuelBay_.liftIncrement,model.level2ArmorWarningActive_};
    }
    static void fuelEntry(GameplayModel& model,std::uint32_t distance) {
        const auto station=std::find_if(model.level2Entities_.begin(),model.level2Entities_.end(),
            [](const auto& actor) {return actor.type==31;});
        test::requireCampaignComparison(station!=model.level2Entities_.end(),"missing actual fuel station");
        model.level2Player_.position=station->position;model.level2Player_.position.z=0;
        model.level2Player_.position.y+=distance;
        model.level2Player_.forwardVelocity=model.level2Player_.yawVelocity=0;
        model.level2PlayerArmor_=200;station->presenterDistance=distance;model.level2ArmorWarningActive_=true;
        // The controlled contact pose needs its matching inherited 45D2 membership.
        model.level2ActiveListOrder_=model.level2AllocationListOrder_;
        Level1ReloadTestAccess::fuelCharge(model.level1_);model.syncPlayableCampaignPools();
    }
    static void sameAdmissionState(const GameplayModel& before,const GameplayModel& after) {
        const auto require=test::requireCampaignComparison;
        require(before.phase_==after.phase_ && before.ticks_==after.ticks_
            && before.activeCampaignLevel_==after.activeCampaignLevel_
            && before.playableCampaignFrames_==after.playableCampaignFrames_
            && before.campaignModelsBound_==after.campaignModelsBound_
            && before.playableOrdinaryConstructor_.has_value()==after.playableOrdinaryConstructor_.has_value(),
            "rejected admission changed model phase or constructor readiness");
        for(std::size_t type=0;type<before.campaignModels_.size();++type) {
            const auto& a=before.campaignModels_[type];const auto& b=after.campaignModels_[type];
            require(std::tie(a.radius,a.selectors,a.animationLimit)==std::tie(b.radius,b.selectors,b.animationLimit),
                "rejected admission changed bound model metadata");
        }
        const auto actors=[&](const auto& a,const auto& b) {
            require(a.size()==b.size(),"rejected admission changed pool membership");
            for(std::size_t index=0;index<a.size();++index) {
                require(test::campaignActorFields(a[index])==test::campaignActorFields(b[index]),
                    "rejected admission changed an actor or physical pool record");
            }
        };
        actors(before.level2Entities_,after.level2Entities_);
        actors(before.level2ActorPool_,after.level2ActorPool_);
        actors(before.level2FixedPool_,after.level2FixedPool_);
        actors(before.level2ProjectilePool_,after.level2ProjectilePool_);
        actors(before.level2DeathEffectPool_,after.level2DeathEffectPool_);
        require(test::campaignActorFields(before.level2Player_)==test::campaignActorFields(after.level2Player_)
            && before.level2EntityActorPoolSlots_==after.level2EntityActorPoolSlots_
            && before.level2EntityFixedPoolSlots_==after.level2EntityFixedPoolSlots_
            && before.level2EntityProjectilePoolSlots_==after.level2EntityProjectilePoolSlots_
            && before.level2EntityDeathEffectPoolSlots_==after.level2EntityDeathEffectPoolSlots_
            && before.level2AllocationListOrder_==after.level2AllocationListOrder_
            && before.level2ObjectListOrder_==after.level2ObjectListOrder_
            && before.level2ActiveListOrder_==after.level2ActiveListOrder_
            && before.level2ActorPoolActive_==after.level2ActorPoolActive_
            && before.level2ActorIncarnations_==after.level2ActorIncarnations_
            && before.level2FixedPoolKnown_==after.level2FixedPoolKnown_
            && before.level2FixedPoolEverUsed_==after.level2FixedPoolEverUsed_
            && before.level2ProjectilePoolActive_==after.level2ProjectilePoolActive_
            && before.level2DeathEffectPoolActive_==after.level2DeathEffectPoolActive_
            && before.level2AllocationRevision_==after.level2AllocationRevision_
            && before.level2NonProjectileAllocationRevision_==after.level2NonProjectileAllocationRevision_
            && before.level2FixedAllocationRevision_==after.level2FixedAllocationRevision_
            && before.level2LoopTick_==after.level2LoopTick_
            && before.level2HasAdvanced_==after.level2HasAdvanced_
            && before.level2TailConsumed_==after.level2TailConsumed_,
            "rejected admission changed model aliases or frame ownership");
        test::compareCampaignRng(before.level2Rng_,after.level2Rng_,test::CampaignComparisonStage::boundary);
        test::compareCampaignRng(before.failureRng_,after.failureRng_,test::CampaignComparisonStage::boundary);
    }
    static void rejectMissingWeightedModel(GameplayModel& model,const std::vector<Model3d>& shared,
        const std::vector<Model3d>& local) {
        test::requireCampaignComparison(!model.playableOrdinaryConstructor_,
            "source metadata unexpectedly admitted the playable constructor");
        const auto before=std::make_unique<GameplayModel>(model);
        const auto reject=[&](const auto& action) {
            bool rejected=false;
            try {action();} catch(const std::invalid_argument&) {rejected=true;}
            test::requireCampaignComparison(rejected,"missing weighted model was admitted");
            sameAdmissionState(*before,model);
        };
        reject([&] {model.preparePlayableCampaignModels(shared,local);});
        reject([&] {model.advancePlayableCampaign({});});
    }
    static void campaignBank(GameplayModel& model,const std::vector<Model3d>& shared,const std::vector<Model3d>& local,const std::uint8_t level,const CampaignFixtureActors fixture) {
        auto rng=model.level2Rng_;
        const auto definition=campaignLevelDefinition(level,0);
        auto actors=constructCampaignLoaderActors(definition.table,rng,shared,local);
        if (fixture==CampaignFixtureActors::clearedWave) {
            actors.erase(std::remove_if(actors.begin(),actors.end(),[](const auto& actor) {
                return actor.stateBytes4dTo52[0]==0xff;
            }),actors.end());
        }
        model.playableCampaignFrames_=true;
        model.initializeRetainedCampaignLevel(level,std::move(actors),rng,shared,local);
        if (fixture==CampaignFixtureActors::clearedWave) {
            model.level2ActiveEnemyCount_=0;
            model.level2SpawnedEnemyCount_=definition.table.enemyLimit;
        }
    }

    static void incomingPlayerShot(GameplayModel& model,const FixedPosition position) {
        auto shot=model.level2ProjectilePool_[0];
        shot.type=23;shot.flags=0;shot.position=position;
        shot.pitch=shot.roll=shot.yaw=0;shot.forwardVelocity=500;
        shot.pitchVelocity=shot.rollVelocity=shot.yawVelocity=0;
        shot.xVelocity=shot.yVelocity=shot.zVelocity=0;
        shot.collisionRadius=model.campaignModels_[23].radius;
        shot.stateBytes4dTo52={0xfd,45,0,0,0,0};
        shot.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;
        shot.animatedModelPresenterEnabled=false;shot.ownerIsPlayer=true;
        shot.ownerAllocationIndex=model.level2Player_.allocationIndex;
        shot.allocationIndex=model.level2Entities_.size();
        model.level2Entities_.push_back(shot);
        model.level2EntityActorPoolSlots_.push_back(model.level2ActorPoolSize_);
        model.level2EntityFixedPoolSlots_.push_back(model.level2FixedObjectPoolSize_);
        model.level2EntityProjectilePoolSlots_.push_back(0);
        model.level2EntityDeathEffectPoolSlots_.push_back(model.level2DeathEffectPoolSize_);
        model.level2ProjectilePool_[0]=shot;model.level2ProjectilePoolActive_[0]=true;
        model.level2AllocationListOrder_.push_back(shot.allocationIndex);
        model.level2ObjectListOrder_=model.level2AllocationListOrder_;
        model.level2ActiveListOrder_=model.level2AllocationListOrder_;
        model.level2Player_.weaponSlotTimers[0]=13;
        model.phase_=GameplayPhase::playable;model.level2LoopTick_=20;
        model.level2HasAdvanced_=true;model.level2TailConsumed_=true;model.level3RadarFinished_=true;
        model.snapshotLevel2Presentation();
    }
    static void blueHitBoundary(GameplayModel& model) {
        const auto target = std::find_if(model.level2Entities_.begin(),model.level2Entities_.end(),[](const auto& actor) {
            return actor.stateBytes4dTo52[4]==0xf3;
        });
        if(target==model.level2Entities_.end()) {throw std::runtime_error("bank has no actual F3 warp producer");}
        if(target->stateBytes4dTo52[0]!=0xfe || target->stateBytes4dTo52[1]!=16
            || target->type!=29+model.activeCampaignLevel_) {
            throw std::runtime_error("actual F3 source row/profile changed");
        }
        bool sourceCorner=false;
        for(const auto& candidate:model.level2Entities_) {
            for(const auto delta:{static_cast<std::int64_t>(candidate.position.x)-target->position.x,
                static_cast<std::int64_t>(candidate.position.y)-target->position.y}) {
                if(delta!=-32768 && delta!=32768) {continue;}
                const auto threshold=(static_cast<std::uint64_t>(candidate.collisionRadius)+target->collisionRadius)/2;
                if(candidate.collisionRadius>=32768 || target->collisionRadius>=32768 || threshold>=32767) {
                    throw std::runtime_error("actual corner radii cannot prove the source saturation decision");
                }
                const auto wideMagnitude=static_cast<std::uint64_t>(delta<0 ? -delta : delta);
                const auto semanticCollision=wideMagnitude<=threshold;
                const auto sourceSaturatedCollision=std::uint64_t{32767}<=threshold;
                if(semanticCollision || sourceSaturatedCollision || semanticCollision!=sourceSaturatedCollision) {
                    throw std::runtime_error("actual32768 corner changed the far-no-overlap decision");
                }
                sourceCorner=true;
            }
        }
        if(model.activeCampaignLevel_==0 && !sourceCorner) {
            throw std::runtime_error("actual bank1 fixed corner precondition was lost");
        }
        //Declared legal late-hit component entry; preceding hits are not claimed.
        target->stateBytes4dTo52[1]=1;
        const auto targetIndex=static_cast<std::size_t>(target-model.level2Entities_.begin());
        const auto fixed=model.level2EntityFixedPoolSlots_.at(targetIndex);
        model.level2FixedPool_.at(fixed)=*target;
        model.level2Player_.position={target->position.x,target->position.y-2000,0};
        model.level2Player_.yaw=0;
        incomingPlayerShot(model,{target->position.x,target->position.y-500,0});
    }
    static void guardianLateFight(GameplayModel& model) {
        model.level2Rng_.next();model.level2Rng_.next();
        Level1Entity guardian{};guardian.type=44;guardian.position={0,10000,0};
        guardian.presenterDistance=10000;guardian.targetHeading=0x8000;
        guardian.stateBytes4dTo52={0xff,1,2,0,5,0};
        model.level2PostWaveObjectIndex_=model.allocateLevel2Actor(guardian);
        model.level2ActiveEnemyCount_=1;model.level2SpawnedEnemyCount_+=2;
        incomingPlayerShot(model,{0,9500,0});
    }

    static void destructionEntry(GameplayModel& model,test::DestructionTarget kind,int distance,int height,test::DestructionPool pool) {
        for(auto& actor:model.level2Entities_) {
            actor.position={100000,100000,0};actor.weaponSlotCount=0;
        }
        auto target=model.playableOrdinaryConstructor_->construct(model.activeCampaignLevel_==1?4:1,model.level2Rng_);
        if(kind==test::DestructionTarget::guardian) {
            target={};target.type=44;target.stateBytes4dTo52={0xff,1,2,0,5,0};
        }
        target.stateBytes4dTo52[1]=kind==test::DestructionTarget::survivor?20:1;target.position={2000,distance,height};
        target.forwardVelocity=target.yawVelocity=0;target.weaponSlotCount=0;
        model.allocateLevel2Actor(target,kind==test::DestructionTarget::ordinary
            ?GameplayModel::ActorInitialization::validatedOrdinary:GameplayModel::ActorInitialization::legacy);
        model.level2ActiveEnemyCount_=2;
        if(pool==test::DestructionPool::full) {
            for(std::size_t slot=0;slot<model.level2DeathEffectPoolSize_;++slot) {
                auto effect=model.level2DeathEffectPool_[slot];effect.type=-1;effect.position={100000,100000,0};
                effect.flags=0x8c;effect.renderProfile=WorldActorRenderProfile::bitmapEffectCallback;
                effect.stateBytes4dTo52={0xfc,15,0,0,17,0};effect.allocationIndex=model.level2Entities_.size();
                model.level2Entities_.push_back(effect);
                model.level2EntityActorPoolSlots_.push_back(model.level2ActorPoolSize_);
                model.level2EntityFixedPoolSlots_.push_back(model.level2FixedObjectPoolSize_);
                model.level2EntityProjectilePoolSlots_.push_back(model.level2ProjectilePoolSize_);
                model.level2EntityDeathEffectPoolSlots_.push_back(slot);
                model.level2DeathEffectPool_[slot]=effect;model.level2DeathEffectPoolActive_[slot]=true;
                model.level2AllocationListOrder_.push_back(effect.allocationIndex);
            }
        }
        const auto phase=model.phase_;const auto loop=model.level2LoopTick_;
        incomingPlayerShot(model,{2000,distance-500,height});
        model.phase_=phase;model.level2LoopTick_=loop;model.syncPlayableCampaignPools();
    }

    static void finalBank(GameplayModel& model,const std::vector<Model3d>& shared,const std::vector<Model3d>& local) {
        auto rng=model.level2Rng_;
        auto actors=constructCampaignLoaderActors(campaignLevelDefinition(6,0).table,rng,shared,local);
        model.playableCampaignFrames_=true;
        model.initializeRetainedCampaignLevel(6,std::move(actors),rng,shared,local);
    }
    static void finalVictory(GameplayModel& model) {model.enterPlayableGuardianVictory();}
    static void lethalBoundary(GameplayModel& model) {
        model.level2PlayerArmor_=-4;
        model.failureFromLevel2_=true;
        model.level2FailureTailFinished_=true;
        model.failureSourceLoopTick_=model.level2LoopTick_;
        model.phase_=GameplayPhase::failurePreExplosionBlackout;
        model.presentationFrame_=0;
    }
    static void exhaustedStock(GameplayModel& model) { model.remainingContinues_=0; }
    static std::string successDiagnostic(const GameplayModel& model) {
        std::ostringstream result;
        if (!model.level2SuccessContinuation_) {return "no success continuation";}
        const auto& pause=*model.level2SuccessContinuation_;
        result<<"revision="<<pause.allocationRevision<<'/'<<model.level2AllocationRevision_
            <<" order="<<(pause.order==model.level2ActiveListOrder_)
            <<" fixed="<<(pause.fixedSlots==model.level2EntityFixedPoolSlots_)
            <<" types="<<pause.types.size()<<'/'<<model.level2Entities_.size()
            <<" pickup="<<pause.pickupIndex<<'/'<<model.level2PickupIndex_
            <<" loop="<<pause.sourceTick<<'/'<<model.level2LoopTick_;
        return result.str();
    }
    static void clearedWave(GameplayModel& model) {
        const auto projectiles=model.level2ProjectilePool_;
        const auto effects=model.level2DeathEffectPool_;
        auto& entities=model.level2Entities_;
        entities.erase(std::remove_if(entities.begin(),entities.end(),[](const auto& actor) {
            return actor.stateBytes4dTo52[0]==255;
        }),entities.end());
        for(std::size_t index=0;index<entities.size();++index) {entities[index].allocationIndex=index;}
        model.initializeCampaignPools(1,0);
        model.level2ProjectilePool_=projectiles;
        model.level2DeathEffectPool_=effects;
        for(auto& actor:model.level2ActorPool_) {actor.stateBytes4dTo52[0]=255;}
        for(auto& projectile:model.level2ProjectilePool_) {projectile.stateBytes4dTo52[0]=253;}
        for(auto& effect:model.level2DeathEffectPool_) {effect.stateBytes4dTo52[0]=252;}
        model.level2ActiveEnemyCount_=0;model.level2SpawnedEnemyCount_=12;
        model.level2HasAdvanced_=false;model.level2TailConsumed_=false;
        model.snapshotLevel2Presentation();
    }
    static void returned(GameplayModel& model,std::uint16_t loop) {
        Level1ReloadTestAccess::successReturn(model.level1_,loop);
        model.phase_=GameplayPhase::successPresentation;model.level2PickupIndex_=7;
        model.recoveredSuccessPause_.reset();model.recoveredSuccessReturned_=false;model.ticks_=loop;model.recoveredLevel1TickBase_=0;
        model.remainingContinues_=1;
    }
};
struct NativeSessionTestAccess {
    static void destructionEntry(NativeGameplaySession& session,test::DestructionTarget kind,int distance,int height,test::DestructionPool pool) {
        GameplayModelTestAccess::destructionEntry(session.model_,kind,distance,height,pool);
    }
    static void bonusEntry(NativeGameplaySession& session,std::uint8_t type) {GameplayModelTestAccess::bonusEntry(session.model_,type);}
    static void fuelEntry(NativeGameplaySession& session,std::uint32_t distance) {GameplayModelTestAccess::fuelEntry(session.model_,distance);}
    static void rejectMissingWeightedModel(NativeGameplaySession& session) {
        const auto model=std::make_unique<GameplayModel>(session.model_);
        const auto samples=session.elapsedSamples();
        const auto cycles=session.synth_.driverState().normalClockCycles;
        const auto writes=session.synth_.registerWrites().size();
        const auto frame=session.frame();const auto pages=session.pageObservation();
        const auto cache=session.retainedPagePixels(NativePageRole::cache);
        const auto back=session.retainedPagePixels(NativePageRole::back);
        const auto front=session.retainedPagePixels(NativePageRole::front);
        const auto playable=session.playableCampaign_;const auto level1=session.level1Pages_;
        bool rejected=false;
        try {session.advancePlayableCampaign({},10000);} catch(const std::invalid_argument&) {rejected=true;}
        test::requireCampaignComparison(rejected,"native preflight admitted a missing weighted model");
        GameplayModelTestAccess::sameAdmissionState(*model,session.model_);
        test::compareCampaignFrame(frame,session.frame());
        const auto after=session.pageObservation();
        test::requireCampaignComparison(session.elapsedSamples()==samples
            && session.synth_.driverState().normalClockCycles==cycles
            && session.synth_.registerWrites().size()==writes
            && session.playableCampaign_==playable && session.level1Pages_==level1
            && cache==session.retainedPagePixels(NativePageRole::cache)
            && back==session.retainedPagePixels(NativePageRole::back)
            && front==session.retainedPagePixels(NativePageRole::front)
            && std::tie(pages.projection,pages.cache,pages.back,pages.front,pages.copy,pages.draw)
                ==std::tie(after.projection,after.cache,after.back,after.front,after.copy,after.draw),
            "native preflight rejection changed pages, palette, PCM, OPL or clock");
    }
    static void campaignBank(NativeGameplaySession& session,const std::uint8_t level,const CampaignFixtureActors fixture) {
        const auto& resource=session.assets_.campaignLevels.at(level);
        const auto sharedCount=session.assets_.models.size()-session.assets_.campaignLevels.front().models.size();
        const std::vector<Model3d> shared(session.assets_.models.begin(),session.assets_.models.begin()+sharedCount);
        GameplayModelTestAccess::campaignBank(session.model_,shared,resource.models,level,fixture);
        constexpr std::array<NativeSessionStart,7> starts{NativeSessionStart::level1,NativeSessionStart::level2,
            NativeSessionStart::level3,NativeSessionStart::level4,NativeSessionStart::level5,NativeSessionStart::level6,NativeSessionStart::level7};
        session.pages_.selectFront();session.initializePages(starts.at(level));
        auto& phase=*const_cast<SoundBlasterClockPhase*>(session.recoveredClockPhase());
        phase.callback=session.synth_.selectPlayableSequence(resource.musicSequence,PlayableSequenceMode::repeat);
        phase.sequenceSelection.reset();phase.successAudio.reset();phase.level2RetryMusic.reset();
        session.campaignResources_=NativeGameplaySession::CampaignResourceBorrow{level,&resource.bitmapGroups,&resource.models,&resource.musicSequence};
        session.level2Cursor_=0;
    }

    static void guardianLateFight(NativeGameplaySession& session) {
        GameplayModelTestAccess::guardianLateFight(session.model_);
        session.level2Cursor_=session.model_.level2AudioEvents().size();
    }
    static void blueHitBoundary(NativeGameplaySession& session) {
        GameplayModelTestAccess::blueHitBoundary(session.model_);
        session.level2Cursor_=session.model_.level2AudioEvents().size();
    }
    static bool bluePalette(const NativeGameplaySession& session) {
        return session.successPalette_ && session.successPalette_->channel==SuccessPaletteChannel::blue;
    }

    static void controlledEnding(NativeGameplaySession& session) {
        const auto& resource=session.assets_.campaignLevels.at(6);
        const auto sharedCount=session.assets_.models.size()-session.assets_.campaignLevels.front().models.size();
        const std::vector<Model3d> shared(session.assets_.models.begin(),session.assets_.models.begin()+sharedCount);
        GameplayModelTestAccess::finalBank(session.model_,shared,resource.models);
        session.pages_.selectFront();session.initializePages(NativeSessionStart::level7);
        auto& phase=*const_cast<SoundBlasterClockPhase*>(session.recoveredClockPhase());
        phase.callback=session.synth_.selectPlayableSequence(resource.musicSequence,PlayableSequenceMode::repeat);
        phase.sequenceSelection.reset();phase.successAudio.reset();phase.level2RetryMusic.reset();
        session.campaignResources_=NativeGameplaySession::CampaignResourceBorrow{6,&resource.bitmapGroups,&resource.models,&resource.musicSequence};
        session.model_.completePlayableCampaignPreloop();
        GameplayModelTestAccess::finalVictory(session.model_);
        session.level2Cursor_=session.model_.level2AudioEvents().size();
    }
    static void controlledReturn(NativeGameplaySession& session,std::uint16_t loop) {
        GameplayModelTestAccess::returned(session.model_,loop);
        while(session.recoveredController_->expectedLoopTick()!=loop) {
            session.recoveredController_->consumeEmptyTail(session.recoveredController_->expectedLoopTick());
        }
        session.level1Cursor_=session.model_.level1State().audioEvents().size();
        NativeSuccessState state{};state.control.stage=SuccessControlStage::finished;
        state.control.channel=SuccessPaletteChannel::green;
        session.recoveredSuccessState_=state;
        NativeSuccessRun run{};run.finished=true;run.state=state;session.recoveredSuccess_=std::move(run);
    }
    static SoundBlasterSynth& synth(NativeGameplaySession& session) {return session.synth_;}
    static PresentationClock& clock(NativeGameplaySession& session) {return session.clock_;}
    static const RadarState* radar(const NativeGameplaySession& session) {return &*session.radar_;}
    static const IndexedPages* pixels(const NativeGameplaySession& session) {return &*session.pixels_;}
    static std::uint16_t expected(const NativeGameplaySession& session) {return session.recoveredController_->expectedLoopTick();}
    static void publishPhase(NativeGameplaySession& session,const SoundBlasterClockPhase& phase) {
        //Test-only receipt adoption after the same live clock rendered each child.
        //No callback/model bytes are imported from an original-state snapshot.
        *const_cast<SoundBlasterClockPhase*>(session.recoveredClockPhase())=phase;
    }
};
}
using namespace stellar7;
namespace {
std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream stream(path,std::ios::binary);
    if(!stream) {throw std::runtime_error("missing immutable resource");}
    return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
GameplayAssets assets(const std::filesystem::path& root,const unsigned lastLevel=2) {
    const auto cockpit=read(root/"COCKPIT.RES"),main=read(root/"STELLAR.RES"),art=read(root/"STELART.RES");
    GameplayAssets result{};
    result.artGroups=decodeBitmapGroups(art);const auto palettes=decodePalettes(art);
    result.uiPalette=palettes.back();
    for(const auto color:palettes.at(2)) {
        for(const auto component:{color.red,color.green,color.blue}) {result.cockpitDacPalette.push_back(component>>2);}
    }
    result.cockpit=applyVgaDacPalette(decodeFirstIndexedScreenResource(cockpit),result.cockpitDacPalette);
    const auto fonts=decodeFontResources(main);result.titleFont=fonts.at(0);result.scoreFont=fonts.at(1);
    result.soundSequences=decodeTaggedPayloads(cockpit,{'S','N','G',':'});
    result.briefingText=decodeTaggedPayloads(cockpit,{'T','X','T',':'}).at(0);
    result.soundBlasterInstrumentResource=decodeTaggedPayloads(main,{'0','0','3',':'}).at(0);
    const auto songs=decodeTaggedPayloads(main,{'S','N','G',':'});
    result.logoMusicSequence=songs.at(0);result.presentationMusicSequence=songs.at(1);
    result.soundBlasterVolumeTable=decodeSoundBlasterVolumeTable(decodeTaggedPayloads(main,{'S','B','L',':'}).at(0));
    result.models=decodeFirstModelTableResource(cockpit);
    for(unsigned level=1;level<=lastLevel;++level) {
        const auto name="LEVEL"+std::to_string(level)+".RES";const auto bytes=read(root/name);
        result.campaignLevels.push_back({name,decodeBitmapGroups(bytes),decodeTaggedPayloads(bytes,{'S','N','G',':'}).at(0),decodeFirstModelTableResource(bytes)});
    }
    const auto& own=result.campaignLevels[0].models;result.models.insert(result.models.end(),own.begin(),own.end());
    result.sharedModelRegistry={256,256}; //Actual loaded44 must be derived from owned banks.
    result.modelColorTables=decodeByteArrays(main);
    const CockpitSettings settings{82,{1,0,1,1,0,0}};result.cockpitSettings=settings;
    HighscoreTable scores({},{});scores.setConfiguration(settings);result.highscoreTable=scores;
    return result;
}
std::unique_ptr<NativeGameplaySession> session(GameplayAssets& game,const std::filesystem::path& root,std::uint16_t loop,
    std::shared_ptr<HighscoreStore> store = {},
    const NativePointerCapability pointerCapability = NativePointerCapability::unavailable,
    const NativeMenuFeatures menuFeatures = NativeMenuFeatures::legacy) {
    const auto resources=decodeNativeStartupAssets(read(root/"STELART.RES"));
    auto startup=std::make_unique<NativeStartupSequence>(game,resources);
    startup->start(menuFeatures!=NativeMenuFeatures::legacy || pointerCapability==NativePointerCapability::portablePointer
        ? NativeStartupExecution::playableContext : NativeStartupExecution::recoveredContext,pointerCapability,menuFeatures);
    startup->initializeSettings(pointerCapability==NativePointerCapability::portablePointer
        ? StartupCapabilityProfile::portablePointer : StartupCapabilityProfile::noMouseOrJoystick);
    startup->initializeResources(NativeStartupDeviceProfile::portableOplNoDspProbe);
    startup->initializePresentation();startup->initializeFonts();
    while(startup->phase()==NativeStartupPhase::logoCountdownWork) {startup->pollLogoCountdown(10000);}
    startup->playLogoEvent();startup->animateLogo();startup->finishLogoSound();startup->releaseLogo();
    startup->skipIntro(NativeIntroKey::escape);startup->beginMenuSong();startup->initializeMenuResources();
    startup->skipCredits(NativeIntroKey::escape);startup->enterMenu(NativeMenuEntryProfile::keyboardOnly);
    startup->beginGame(NativeMenuBeginProfile::enterPressRelease);
    auto result=std::make_unique<NativeGameplaySession>(game,startup->releaseGameplay(),std::move(store));
    result->initializeRecoveredLevel1();
    auto phase=bindSuccessAudioResource(*result->recoveredClockPhase(),game.soundSequences.at(0));
    auto& clock=NativeSessionTestAccess::clock(*result);
    for(const auto child:{SuccessAudioChild::entryStop,SuccessAudioChild::activation,SuccessAudioChild::countdownStore,SuccessAudioChild::returnStop}) {
        phase=renderSuccessAudio(clock,phase,child).timing.phase;
    }
    NativeSessionTestAccess::publishPhase(*result,phase);
    NativeSessionTestAccess::controlledReturn(*result,loop);
    return result;
}
template<class Action>void rejects(NativeGameplaySession& session,Action action) {
    const auto sample=session.elapsedSamples(),ticks=session.model().ticks();
    const auto writes=session.synth().registerWrites().size();const auto page=session.pageObservation();
    bool rejected=false;try {action();}catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected&&session.elapsedSamples()==sample&&session.model().ticks()==ticks);
    assert(session.synth().registerWrites().size()==writes);
    assert(session.pageObservation().front==page.front&&session.pageObservation().back==page.back);
    assert(!session.recoveredLevel2Transition());
}
}

namespace stellar7::test {
std::string playableSuccessDiagnostic(const GameplayModel& model) {
    return GameplayModelTestAccess::successDiagnostic(model);
}
std::unique_ptr<NativeGameplaySession> makeRecoveredLevel2PreloopSession(const std::filesystem::path& root) {
    //Controlled completed-success fixture, not the actual317 ancestry route.
    static auto game=assets(root);
    auto current=session(game,root,17);
    current->advanceRecoveredLevel2Transition(10000);
    return current;
}

std::unique_ptr<NativeGameplaySession> makePlayableLevel2Session(const std::filesystem::path& root) {
    static auto game=assets(root,7);
    auto current=session(game,root,17);
    current->advanceRecoveredLevel2Transition(10000);
    current->advanceRecoveredLevel2Frame(FrameCopyProfile::modeXNoop,10000,{});
    return current;
}

std::unique_ptr<NativeGameplaySession> makePlayableLevel2ClearedWaveSession(const std::filesystem::path& root) {
    auto current=makePlayableLevel2Session(root);
    GameplayModelTestAccess::clearedWave(const_cast<GameplayModel&>(current->model()));
    return current;
}

std::unique_ptr<NativeGameplaySession> makePlayableEndingSession(const std::filesystem::path& root,
    const PlayableScoreFixture scores,std::shared_ptr<HighscoreStore> store) {
    static auto miss=assets(root,7);
    static auto hit=assets(root,7);
    if (scores!=PlayableScoreFixture::rankMiss && scores!=PlayableScoreFixture::rankHit) {
        throw std::invalid_argument("invalid playable score fixture");
    }
    auto& game=scores==PlayableScoreFixture::rankMiss ? miss : hit;
    HighscoreTable::Entries entries{};
    for (auto& entry:entries) {entry.score=scores==PlayableScoreFixture::rankMiss ? 10000 : 0;}
    HighscoreTable table({},entries);table.setConfiguration(*game.cockpitSettings);
    game.highscoreTable=table;
    if (!game.endingScene) {
        game.endingScene=std::make_shared<const EndingSceneAssets>(decodeEndingSceneAssets(read(root/"SCENED.RES"),read(root/"VOICE.RES")));
    }
    auto current=session(game,root,17,std::move(store));
    current->advanceRecoveredLevel2Transition(10000);
    current->advanceRecoveredLevel2Frame(FrameCopyProfile::modeXNoop,10000,{});
    current->advancePlayableCampaign({});
    NativeSessionTestAccess::controlledEnding(*current);
    return current;
}

std::unique_ptr<NativeGameplaySession> makePlayableLethalSession(const std::filesystem::path& root,
    const PlayableScoreFixture scores,std::shared_ptr<HighscoreStore> store,
    const NativePointerCapability pointerCapability, const PlayableMouseFixture mouse) {
    static auto miss=assets(root,7);
    static auto hit=assets(root,7);
    if (scores!=PlayableScoreFixture::rankMiss && scores!=PlayableScoreFixture::rankHit) {
        throw std::invalid_argument("invalid playable score fixture");
    }
    auto& game=scores==PlayableScoreFixture::rankMiss ? miss : hit;
    game.cockpitSettings->options[0] = mouse == PlayableMouseFixture::enabled ? 1 : 0;
    HighscoreTable::Entries entries{};
    for (auto& entry:entries) {entry.score=scores==PlayableScoreFixture::rankMiss ? 10000 : 0;}
    HighscoreTable table({},entries);table.setConfiguration(*game.cockpitSettings);
    game.highscoreTable=table;
    auto current=session(game,root,17,std::move(store),pointerCapability);
    current->advanceRecoveredLevel2Transition(10000);
    current->advanceRecoveredLevel2Frame(FrameCopyProfile::modeXNoop,10000,{});
    current->advancePlayableCampaign({});
    GameplayModelTestAccess::lethalBoundary(const_cast<GameplayModel&>(current->model()));
    return current;
}

std::unique_ptr<NativeGameplaySession> makePlayableExhaustedSession(const std::filesystem::path& root,
    const PlayableScoreFixture scores,std::shared_ptr<HighscoreStore> store) {
    auto current=makePlayableLethalSession(root,scores,std::move(store));
    GameplayModelTestAccess::exhaustedStock(const_cast<GameplayModel&>(current->model()));
    return current;
}
}

namespace {
std::unique_ptr<NativeGameplaySession> campaignBoundarySession(const std::filesystem::path& root,
    const std::uint8_t level,const CampaignFixtureActors fixture,
    const NativeMenuFeatures menuFeatures=NativeMenuFeatures::legacy) {
    static auto game=assets(root,7);
    if (level==6 && !game.endingScene) {
        game.endingScene=std::make_shared<const EndingSceneAssets>(
            decodeEndingSceneAssets(read(root/"SCENED.RES"),read(root/"VOICE.RES")));
    }
    auto current=session(game,root,17,{},NativePointerCapability::unavailable,menuFeatures);
    current->advanceRecoveredLevel2Transition(10000);
    current->advanceRecoveredLevel2Frame(FrameCopyProfile::modeXNoop,10000,{});
    current->advancePlayableCampaign({});
    NativeSessionTestAccess::campaignBank(*current,level,fixture);
    return current;
}
}
namespace stellar7::test {
std::unique_ptr<NativeGameplaySession> makePlayableGuardianSession(const std::filesystem::path& root,
    const std::uint8_t level,const PlayableGuardianEntry entry) {
    return makePlayableGuardianSession(root,level,entry,NativeMenuFeatures::legacy);
}
std::unique_ptr<NativeGameplaySession> makePlayableGuardianSession(const std::filesystem::path& root,
    const std::uint8_t level,const PlayableGuardianEntry entry,const NativeMenuFeatures menuFeatures) {
    if (entry!=PlayableGuardianEntry::clearedWave && entry!=PlayableGuardianEntry::lateFight) {
        throw std::invalid_argument("invalid playable guardian fixture");
    }
    auto current=campaignBoundarySession(root,level,CampaignFixtureActors::clearedWave,menuFeatures);
    if (entry==PlayableGuardianEntry::lateFight) {
        current->advancePlayableCampaign({});
        NativeSessionTestAccess::guardianLateFight(*current);
    }
    return current;
}
std::unique_ptr<NativeGameplaySession> makePlayableBlueWarpSession(const std::filesystem::path& root,
    const std::uint8_t level) {
    if (level!=0 && level!=3) {
        throw std::invalid_argument("bank has no actual initial F3 producer");
    }
    auto current=campaignBoundarySession(root,level,CampaignFixtureActors::intact);
    current->advancePlayableCampaign({});
    NativeSessionTestAccess::blueHitBoundary(*current);
    return current;
}
std::unique_ptr<NativeGameplaySession> makePlayableFuelBaySession(const std::filesystem::path& root,const std::uint8_t level,std::uint32_t distance) {
    if((level!=2&&level!=5)||distance>700) {throw std::invalid_argument("invalid controlled fuel bay entry");}
    auto current=campaignBoundarySession(root,level,CampaignFixtureActors::clearedWave);
    current->advancePlayableCampaign({});current->advancePlayableCampaign({});
    NativeSessionTestAccess::fuelEntry(*current,distance);return current;
}
std::array<std::int64_t,5> observeFuelBay(const GameplayModel& model) {return GameplayModelTestAccess::fuelState(model);}
std::unique_ptr<NativeGameplaySession> makePlayableBonusPickupSession(const std::filesystem::path& root,
    std::uint8_t level,std::uint8_t type) {
    if(level>=7 || (type!=1&&type!=4&&type!=5&&type!=6&&type!=11&&type!=13)) {
        throw std::invalid_argument("invalid controlled campaign bonus entry");
    }
    auto current=campaignBoundarySession(root,level,CampaignFixtureActors::clearedWave);
    current->advancePlayableCampaign({});current->advancePlayableCampaign({});
    NativeSessionTestAccess::bonusEntry(*current,type);return current;
}
std::unique_ptr<NativeGameplaySession> makeDestructionSession(const std::filesystem::path& root,
    std::uint8_t level,DestructionTarget kind,int distance,int height,DestructionPool pool) {
    auto current=campaignBoundarySession(root,level,CampaignFixtureActors::clearedWave);
    current->advancePlayableCampaign({});current->advancePlayableCampaign({});
    NativeSessionTestAccess::destructionEntry(*current,kind,distance,height,pool);return current;
}
void prepareNextDestruction(NativeGameplaySession& session,DestructionTarget kind,int distance,int height) {
    NativeSessionTestAccess::destructionEntry(session,kind,distance,height,DestructionPool::available);
}
bool hasNativeBluePalette(const NativeGameplaySession& session) {
    return NativeSessionTestAccess::bluePalette(session);
}
void verifyMissingWeightedPlayableAdmission(const std::filesystem::path& root,const std::uint8_t type) {
    if(type!=1 && type!=4) {
        throw std::invalid_argument("expected a Level2 weighted model absent from the initial roster");
    }
    auto game=assets(root,7);
    auto current=session(game,root,17);
    current->advanceRecoveredLevel2Transition(10000);
    const auto radius=game.models.at(type).collisionRadius;
    test::requireCampaignComparison(radius!=0,"actual weighted model has no radius");
    game.models[type].collisionRadius=0;
    NativeSessionTestAccess::rejectMissingWeightedModel(*current);
    game.models[type].collisionRadius=radius;
    current->advanceRecoveredLevel2Frame(FrameCopyProfile::modeXNoop,10000,{});
    game.models[type].collisionRadius=0;
    const auto sharedCount=game.models.size()-game.campaignLevels.front().models.size();
    const std::vector<Model3d> shared(game.models.begin(),game.models.begin()+sharedCount);
    GameplayModelTestAccess::rejectMissingWeightedModel(
        const_cast<GameplayModel&>(current->model()),shared,game.campaignLevels.at(1).models);
    NativeSessionTestAccess::rejectMissingWeightedModel(*current);
}
}
