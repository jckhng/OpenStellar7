#include "retained_failure_scene.hpp"
#include "stellar7/failure_gameplay_work.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr Viewport fullHardwareViewport{0, 319, 0, 199};
constexpr Viewport failureViewport{0, 318, 0, 199};
constexpr std::uint16_t identityStride = 256;
constexpr std::uint16_t firstDisplayPage = 0xa000;
constexpr std::uint16_t secondDisplayPage = 0xa800;
constexpr std::uint16_t effectCallback = 0x0ac8;
enum class FailureGameplayDomain { level1, level2 };

std::uint16_t identity(const FailureObjectRef object) {
    if (object.index >= identityStride) {
        throw std::invalid_argument("failure object exceeds typed identity range");
    }
    return static_cast<std::uint16_t>(static_cast<unsigned>(object.kind) * identityStride + object.index);
}

Level1Entity& entity(FailureControlSnapshot& state, const FailureObjectRef object) {
    switch (object.kind) {
    case FailureObjectKind::mission: return state.mission.at(object.index);
    case FailureObjectKind::player:
        if (object.index == 0) { return state.player; }
        break;
    case FailureObjectKind::debris: return state.debris.at(object.index);
    case FailureObjectKind::effect: return state.effects.at(object.index).entity;
    case FailureObjectKind::projectile: break;
    }
    throw std::invalid_argument("failure object is not an entity");
}

FailureSceneItem sceneItem(FailureControlSnapshot& state, const FailureObjectRef object, const FailureGameplayDomain domain) {
    const auto level2 = domain == FailureGameplayDomain::level2;
    if (object.kind == FailureObjectKind::effect) {
        const auto& effect = state.effects.at(object.index);
        const auto& actor = effect.entity;
        if (!effect.listed || !actor.active() || actor.type != -1 || actor.stateBytes4dTo52[0] != 0xfc) {
            throw std::invalid_argument("failure scene requires its live FC pool record");
        }
        return FailureBitmapSceneItem{identity(object), {actor.flags, actor.position}, actor.collisionRadius,
            {actor.flags, effect.projected, 0, actor.stateBytes4dTo52[1], actor.stateBytes4dTo52[4], effectCallback}};
    }
    if (object.kind == FailureObjectKind::projectile) {
        const auto& shot = state.projectiles.at(object.index);
        if (!shot.active() || (shot.type != 23 && (!level2 || shot.type != 19)) || shot.stateBytes4dTo54[0] != 0xfd) {
            throw std::invalid_argument("failure scene supports the retained tail-launched primary shot");
        }
        WorldSceneActor actor{{shot.flags, shot.position}, {shot.pitch, shot.roll, shot.yaw},
            shot.type, shot.collisionRadius, 0, 0};
        if (shot.type == 23 || level2) {
            actor.inlineAnimation = std::array<std::uint8_t, 6>{shot.stateBytes4dTo54[2], shot.stateBytes4dTo54[3],
                shot.stateBytes4dTo54[4], shot.stateBytes4dTo54[5], shot.stateBytes4dTo54[6], shot.stateBytes4dTo54[7]};
        }
        return FailureModelSceneItem{identity(object), std::move(actor)};
    }
    const auto& source = entity(state, object);
    WorldSceneActor actor{{source.flags, source.position}, {source.pitch, source.roll, source.yaw},
        source.type, source.collisionRadius, 0, 0};
    if (source.type==44) {
        const auto enabled=source.animatedModelPresenterEnabled;
        if(source.renderProfile!=(enabled ? WorldActorRenderProfile::heapSelectorDeferredAnimation
            : WorldActorRenderProfile::heapSelectorNoCallback)) {
            throw std::invalid_argument("failure guardian callback differs from its canonical selector profile");
        }
        actor.drawCallback=enabled ? 0x130c : 0;
        actor.heapAnimation=Level1AnimationState{source.stateBytes4dTo52[2],source.stateBytes4dTo52[3],source.modelAnimationVariant};
    }
    if (source.type == 11) {
        if (!source.animatedModelPresenterEnabled) {
            throw std::invalid_argument("failure Type11 requires its owned callback");
        }
        actor.drawCallback = 0x130c;
        actor.heapAnimation = Level1AnimationState{source.stateBytes4dTo52[2], source.stateBytes4dTo52[3],
            source.modelAnimationVariant};
    }
    return FailureModelSceneItem{identity(object), std::move(actor)};
}

std::vector<std::uint16_t> identities(const std::vector<FailureObjectRef>& order) {
    std::vector<std::uint16_t> result;
    result.reserve(order.size());
    for (const auto object : order) { result.push_back(identity(object)); }
    return result;
}
}

FailureGameplayStepWork GameplayModel::advanceRecoveredFailureStep(const GameplayAssets& assets,
    const std::uint16_t drawPage, const Viewport incomingHardwareClip) {
    const auto entering = phase_ == GameplayPhase::failurePreExplosionBlackout;
    const auto level2 = failureFromLevel2_;
    const auto sourceTail = level2
        ? activeCampaignLevel_ == 1 && level1_.levelIndex_ == 1 && level2FailureTailFinished_
            && level2HasAdvanced_ && level2TailConsumed_ && level2PlayerArmor_ < 0
        : level1FailureTailFinished_ && level1_.levelIndex_ == 0 && level1_.failureTailConsumed_
            && level1_.failureState_ && !level1_.missionActive_;
    if (!sourceTail || !failureSourceLoopTick_
        || (drawPage != firstDisplayPage && drawPage != secondDisplayPage)
        || (entering ? (recoveredFailureControl_ && !recoveredFailureControl_->finished) || !failureDebris_.empty()
            : phase_ != GameplayPhase::failureExplosion || !recoveredFailureControl_
                || !recoveredFailureControl_->entered || recoveredFailureControl_->finished)
        || assets.models.size() <= 40) {
        throw std::invalid_argument("recovered failure requires its consumed campaign lethal tail");
    }
    FailureGameplayStepWork work{};
    FailureControlInput input{};
    input.snapshot.player = level2 ? level2Player_ : level1_.player_;
    input.snapshot.camera = recoveredFailureCamera_;
    std::vector<std::size_t> missionKeys;
    std::vector<FailureObjectRef> level2Refs;
    constexpr std::size_t fixedKeyBase = 16;
    constexpr std::size_t projectileKeyBase = 32;
    constexpr std::size_t effectKeyBase = 40;
    if (level2) {
        const auto keys = recoveredLevel2RenderKeys();
        for (std::size_t index = 0; index < keys.size(); ++index) {
            const auto key = keys[index];
            const auto& actor = key < fixedKeyBase ? level2ActorPool_[key]
                : key < projectileKeyBase ? level2FixedPool_[key-fixedKeyBase] : level2Entities_[index];
            if (key < projectileKeyBase) {
                const auto ordinary = key < fixedKeyBase;
                if ((ordinary ? actor.type != 4 && actor.type != 8 && actor.type != 13 : actor.type != 30)
                    || actor.stateBytes4dTo52[0] != (ordinary ? 0xff : 0xfe)) {
                    throw std::invalid_argument("Level2 failure has an unsupported physical mission record");
                }
                level2Refs.push_back({FailureObjectKind::mission,input.snapshot.mission.size()});
                missionKeys.push_back(key);
                input.snapshot.mission.push_back(actor);
            } else if (key < effectKeyBase) {
                const auto& shot=level2ProjectilePool_[key-projectileKeyBase];
                if ((shot.type!=19 && shot.type!=23) || shot.stateBytes4dTo52[0]!=0xfd) {
                    throw std::invalid_argument("Level2 failure has an unsupported FD view");
                }
                level2Refs.push_back({FailureObjectKind::projectile,key-projectileKeyBase});
            } else {
                const auto& effect=level2DeathEffectPool_[key-effectKeyBase];
                if (effect.type!=-1 || effect.stateBytes4dTo52[0]!=0xfc) {
                    throw std::invalid_argument("Level2 failure has an unsupported FC view");
                }
                level2Refs.push_back({FailureObjectKind::effect,key-effectKeyBase});
            }
        }
        if (missionKeys.size() != 7 || level2Visibility_.tracking() != VisibilityTracking::sourceInitialized
            || !level2Visibility_.lastCamera()) {
            throw std::invalid_argument("Level2 failure requires its actual constructor/list ancestry");
        }
        for (std::size_t slot = 0; slot < level2ProjectilePoolSize_; ++slot) {
            const auto& actor = level2ProjectilePool_[slot];
            auto& shot = input.snapshot.projectiles[slot];
            shot.poolSlot=slot;shot.flags=actor.flags;shot.type=actor.type;shot.collisionRadius=actor.collisionRadius;
            shot.position=actor.position;shot.pitch=actor.pitch;shot.roll=actor.roll;shot.yaw=actor.yaw;
            shot.pitchVelocity=actor.pitchVelocity;shot.rollVelocity=actor.rollVelocity;shot.yawVelocity=actor.yawVelocity;
            shot.xVelocity=actor.xVelocity;shot.yVelocity=actor.yVelocity;shot.zVelocity=actor.zVelocity;
            shot.forwardVelocity=actor.forwardVelocity;
            std::copy(actor.stateBytes4dTo52.begin(),actor.stateBytes4dTo52.end(),shot.stateBytes4dTo54.begin());
            shot.ownerIsPlayer=actor.ownerIsPlayer;shot.ownerAllocationIndex=actor.ownerAllocationIndex;
            shot.presenterDistance=actor.presenterDistance;shot.targetHeading=actor.targetHeading;
            input.snapshot.effects[slot].entity=level2DeathEffectPool_[slot];
            input.snapshot.effects[slot].listed=level2DeathEffectPoolActive_[slot];
        }
    } else {
        input.snapshot.mission = level1_.entities_;
        input.snapshot.projectiles = level1_.projectiles_;
        input.snapshot.effects = level1_.deathEffects_;
    }
    input.snapshot.debris = failureDebris_;
    input.snapshot.rng = entering ? (level2 ? level2Rng_ : level1_.rng_) : failureRng_;
    input.snapshot.sourceLoopTick = *failureSourceLoopTick_;
    for (std::size_t index = 0; index < input.debrisRadii.size(); ++index) {
        input.debrisRadii[index] = assets.models.at(36 + index).collisionRadius;
    }
    const auto sourceRef = [](const Level1State::ObjectReference object) -> FailureObjectRef {
        switch (object.kind) {
        case Level1State::ObjectKind::entity: return {FailureObjectKind::mission, object.index};
        case Level1State::ObjectKind::projectile: return {FailureObjectKind::projectile, object.index};
        case Level1State::ObjectKind::deathEffect: return {FailureObjectKind::effect, object.index};
        }
        throw std::invalid_argument("unsupported incoming failure object reference");
    };
    if (entering) {
        if (level2) {
            input.control.lastListCamera = *level2Visibility_.lastCamera();
            input.control.refreshRequested = level2Visibility_.refreshRequested();
            for (const auto index : level2AllocationListOrder_) {input.control.allocationOrder.push_back(level2Refs.at(index));}
            for (const auto index : level2ActiveListOrder_) {input.control.presenterOrder.push_back(level2Refs.at(index));}
        } else {
            input.control.lastListCamera = level1_.projectionListOrigin_;
            input.control.refreshRequested = level1_.nativeInitialListPending_;
            for (const auto object : level1_.allocationList_) { input.control.allocationOrder.push_back(sourceRef(object)); }
            for (const auto object : level1_.objectListOrder_) { input.control.presenterOrder.push_back(sourceRef(object)); }
        }
        work.entry = failureEnterOperations(input);
        input.control = work.entry->control;
        input.snapshot = work.entry->expected;
    } else {
        input.control = *recoveredFailureControl_;
    }
    auto histories = entering ? decltype(failureBurstHistory_){} : failureBurstHistory_;
    auto bursts = entering ? std::vector<FailureBurst>{} : failureBursts_;
    auto audio = entering ? std::vector<Level1AudioEvent>{} : failureAudioEvents_;
    const auto nextTick = ticks_ + 1;
    auto postControl = input.control;
    auto postScene = input.snapshot;
    std::vector<FailureVisibleItem> visible;
    if (input.control.display == failureExplosionFrames) {
        work.kind = FailureGameplayStepKind::leave;
        work.control = failureLeaveOperations(input);
        postControl = work.control.control;
        postScene = work.control.expected;
        visible = failureVisibleItems_;
    } else {
        work.kind = FailureGameplayStepKind::display;
        const auto display = input.control.display;
        work.control = failureDisplayOperations(input);
        postControl = work.control.control;
        postScene = work.control.expected;
        for (std::size_t slot = 0; slot < postScene.effects.size(); ++slot) {
            if (!input.snapshot.effects[slot].entity.active() && postScene.effects[slot].entity.active()) {
                histories[slot] = bursts.size();
                bursts.push_back({postScene.effects[slot].entity.position, display});
            }
        }
        for (auto& step : work.control.steps) {
            for (auto& operation : step.operations) {
                if (operation.kind != NormalWorkKind::failureExplosionEvent) { continue; }
                if (operation.count != 1 || operation.value != 6) {
                    throw std::logic_error("unexpected failure event observation");
                }
                audio.push_back({static_cast<std::uint16_t>(nextTick), operation.value});
                if (level2) {operation.kind = NormalWorkKind::failureLevel2ExplosionEvent;}
            }
        }
        const auto& panorama = assets.horizonFramesForLevel(level2 ? 1 : 0);
        if (panorama.empty()) { throw std::invalid_argument("missing retained failure panorama"); }
        const auto horizonState = HorizonWorkState{0,199,static_cast<std::int16_t>(postScene.camera.yaw),100,
            256,panorama.front().height,level2 ? HorizonBackground::solid : HorizonBackground::gradient,
            HorizonOverlay::enabled,static_cast<std::uint8_t>(level2 ? 64 : 233),static_cast<std::uint8_t>(level2 ? 78 : 246)};
        work.horizon = level2
            ? level2FailureHorizonExecution(horizonState,panorama,entering ? fullHardwareViewport : incomingHardwareClip)
            : failureHorizonExecution(horizonState,panorama,entering ? fullHardwareViewport : incomingHardwareClip);
        work.view = failureViewSetupOperations({postScene.camera.position, {},
            {postScene.camera.pitch, postScene.camera.roll, postScene.camera.yaw}, 0x88, 7,
            failureViewport, {256, -256, 362}});
        std::vector<FailureSceneItem> items;
        items.reserve(work.control.drawOrder.size());
        for (const auto object : work.control.drawOrder) { items.push_back(sceneItem(postScene, object, level2 ? FailureGameplayDomain::level2 : FailureGameplayDomain::level1)); }
        const auto scene = level2 ? retainedLevel2FailureSceneOperations : retainedFailureSceneOperations;
        work.scene = scene(assets, work.view->view, items,
            {assets.sharedModelRegistry[0], assets.sharedModelRegistry[1], 0, 0},
            {static_cast<std::int16_t>(work.view->transformedLight.x),
                static_cast<std::int16_t>(work.view->transformedLight.y),
                static_cast<std::int16_t>(work.view->transformedLight.z)}, drawPage, identities(postControl.allocationOrder));
        work.sceneOperations.push_back({NormalWorkKind::instructions, 8}); //B996..B9A2.
        work.sceneOperations.insert(work.sceneOperations.end(), work.view->operations.begin(), work.view->operations.end());
        work.sceneOperations.insert(work.sceneOperations.end(), work.scene->scene.operations.begin(), work.scene->scene.operations.end());
        if (work.scene->observations.size() != work.control.drawOrder.size()) {
            throw std::logic_error("failure scene lost a presenter observation");
        }
        std::vector<FailureObjectRef> retired;
        for (std::size_t ordinal = 0; ordinal < work.control.drawOrder.size(); ++ordinal) {
            const auto object = work.control.drawOrder[ordinal];
            const auto& observation = work.scene->observations[ordinal];
            if (observation.allocationIdentity != identity(object)) {
                throw std::logic_error("failure scene changed presenter identity");
            }
            if (observation.animation) {
                auto& actor = entity(postScene, object);
                actor.presentationModelAnimationVariant = actor.modelAnimationVariant;
                actor.stateBytes4dTo52[2] = observation.animation->reload;
                actor.stateBytes4dTo52[3] = observation.animation->countdown;
                actor.modelAnimationVariant = observation.animation->variant;
            }
            if (observation.effect) {
                auto& effect = postScene.effects.at(object.index);
                const auto& state = observation.effect->state;
                effect.entity.flags = state.flags;
                effect.entity.stateBytes4dTo52[1] = state.age;
                effect.entity.stateBytes4dTo52[4] = state.endAge;
                effect.projected = state.projected;
                if (observation.effect->retired) { effect.listed = false; retired.push_back(object); }
            }
            switch (object.kind) {
            case FailureObjectKind::mission: visible.push_back({FailureVisibleKind::mission, object.index}); break;
            case FailureObjectKind::player: visible.push_back({FailureVisibleKind::player, object.index}); break;
            case FailureObjectKind::debris: visible.push_back({FailureVisibleKind::debris, object.index}); break;
            case FailureObjectKind::effect:
                if (histories[object.index]) { visible.push_back({FailureVisibleKind::burst, *histories[object.index]}); }
                break;
            case FailureObjectKind::projectile: break; //Actual scene retains FD; legacy diagnostic enum has no shot kind.
            }
        }
        postControl = applyFailureSceneRetirements(std::move(postControl), retired);
        if (identities(postControl.allocationOrder) != work.scene->allocationIdentities) {
            throw std::logic_error("failure control and callback retirement disagree");
        }
    }
    std::vector<Level1State::ObjectReference> allocations;
    std::vector<Level1State::ObjectReference> objects;
    const auto retainPool = [](const std::vector<FailureObjectRef>& order,
        std::vector<Level1State::ObjectReference>& output) {
        for (const auto object : order) {
            switch (object.kind) {
            case FailureObjectKind::mission: output.push_back({Level1State::ObjectKind::entity, object.index}); break;
            case FailureObjectKind::projectile: output.push_back({Level1State::ObjectKind::projectile, object.index}); break;
            case FailureObjectKind::effect: output.push_back({Level1State::ObjectKind::deathEffect, object.index}); break;
            case FailureObjectKind::player:
            case FailureObjectKind::debris: break;
            }
        }
    };
    retainPool(postControl.allocationOrder, allocations);
    retainPool(postControl.presenterOrder, objects);
    std::vector<std::size_t> active;
    std::vector<std::size_t> semantic;
    for (const auto object : objects) {
        if (object.kind == Level1State::ObjectKind::entity) {
            active.push_back(object.index);
            semantic.push_back(postScene.mission.at(object.index).allocationIndex);
        }
    }
    //Commit once after every renderer/callback has validated its typed inputs.
    if (level2) {
        std::vector<Level1Entity> actors = postScene.mission;
        auto actorSlots=std::vector<std::size_t>(actors.size(),level2ActorPoolSize_);
        auto fixedSlots=std::vector<std::size_t>(actors.size(),level2FixedObjectPoolSize_);
        auto projectileSlots=std::vector<std::size_t>(actors.size(),level2ProjectilePoolSize_);
        auto effectSlots=std::vector<std::size_t>(actors.size(),level2DeathEffectPoolSize_);
        std::vector<FailureObjectRef> refs;
        for (std::size_t index=0;index<missionKeys.size();++index) {
            const auto key=missionKeys[index];
            refs.push_back({FailureObjectKind::mission,index});
            if (key<fixedKeyBase) {actorSlots[index]=key;}
            else {fixedSlots[index]=key-fixedKeyBase;}
        }
        const auto retained=[&](const FailureObjectRef ref) {
            return std::find(postControl.allocationOrder.begin(),postControl.allocationOrder.end(),ref)!=postControl.allocationOrder.end()
                || std::find(postControl.presenterOrder.begin(),postControl.presenterOrder.end(),ref)!=postControl.presenterOrder.end();
        };
        auto shots=level2ProjectilePool_;
        for (std::size_t slot=0;slot<level2ProjectilePoolSize_;++slot) {
            const auto& shot=postScene.projectiles[slot];
            auto& actor=shots[slot];
            actor.flags=shot.flags;actor.type=shot.type;actor.collisionRadius=shot.collisionRadius;
            actor.position=shot.position;actor.pitch=shot.pitch;actor.roll=shot.roll;actor.yaw=shot.yaw;
            actor.pitchVelocity=shot.pitchVelocity;actor.rollVelocity=shot.rollVelocity;actor.yawVelocity=shot.yawVelocity;
            actor.xVelocity=shot.xVelocity;actor.yVelocity=shot.yVelocity;actor.zVelocity=shot.zVelocity;
            actor.forwardVelocity=shot.forwardVelocity;
            std::copy_n(shot.stateBytes4dTo54.begin(),actor.stateBytes4dTo52.size(),actor.stateBytes4dTo52.begin());
            actor.ownerIsPlayer=shot.ownerIsPlayer;actor.ownerAllocationIndex=shot.ownerAllocationIndex;
            actor.presenterDistance=shot.presenterDistance;actor.targetHeading=shot.targetHeading;
            const auto add=[&](const FailureObjectRef ref,const Level1Entity& value) {
                refs.push_back(ref);actors.push_back(value);
                actorSlots.push_back(level2ActorPoolSize_);fixedSlots.push_back(level2FixedObjectPoolSize_);
                projectileSlots.push_back(ref.kind==FailureObjectKind::projectile ? slot : level2ProjectilePoolSize_);
                effectSlots.push_back(ref.kind==FailureObjectKind::effect ? slot : level2DeathEffectPoolSize_);
            };
            const auto projectile=FailureObjectRef{FailureObjectKind::projectile,slot};
            if (retained(projectile)) {add(projectile,actor);}
            const auto effect=FailureObjectRef{FailureObjectKind::effect,slot};
            if (retained(effect)) {add(effect,postScene.effects[slot].entity);}
        }
        const auto order=[&](const std::vector<FailureObjectRef>& source) {
            std::vector<std::size_t> result;
            for (const auto ref:source) {
                if (ref.kind==FailureObjectKind::player || ref.kind==FailureObjectKind::debris) {continue;}
                const auto found=std::find(refs.begin(),refs.end(),ref);
                if (found==refs.end()) {throw std::logic_error("Level2 failure lost a retained physical identity");}
                result.push_back(static_cast<std::size_t>(found-refs.begin()));
            }
            return result;
        };
        auto allocations=order(postControl.allocationOrder);
        auto old=order(postControl.presenterOrder);
        //All allocations and renderer receipts validated before any canonical pool write.
        for (std::size_t index=0;index<missionKeys.size();++index) {
            const auto key=missionKeys[index];
            if (key<fixedKeyBase) {level2ActorPool_[key]=actors[index];level2ActorPoolActive_[key]=actors[index].active();}
            else {level2FixedPool_[key-fixedKeyBase]=actors[index];}
        }
        level2ProjectilePool_=shots;
        for (std::size_t slot=0;slot<level2ProjectilePoolSize_;++slot) {
            level2ProjectilePoolActive_[slot]=postScene.projectiles[slot].active();
            level2DeathEffectPool_[slot]=postScene.effects[slot].entity;
            level2DeathEffectPoolActive_[slot]=postScene.effects[slot].listed;
        }
        level2Player_=postScene.player;level2Entities_=std::move(actors);
        level2EntityActorPoolSlots_=std::move(actorSlots);level2EntityFixedPoolSlots_=std::move(fixedSlots);
        level2EntityProjectilePoolSlots_=std::move(projectileSlots);level2EntityDeathEffectPoolSlots_=std::move(effectSlots);
        level2AllocationListOrder_=std::move(allocations);level2ObjectListOrder_=level2AllocationListOrder_;
        level2ActiveListOrder_=std::move(old);level2Rng_=postScene.rng;
    } else {
        level1_.player_ = postScene.player;
        level1_.entities_ = std::move(postScene.mission);
        level1_.projectiles_ = postScene.projectiles;
        level1_.deathEffects_ = postScene.effects;
        level1_.allocationList_ = std::move(allocations);
        level1_.objectListOrder_ = std::move(objects);
        level1_.activeListOrder_ = std::move(active);
        level1_.semanticActiveListOrder_ = std::move(semantic);
        level1_.projectionListOrigin_ = postControl.lastListCamera;
        level1_.nativeInitialListPending_ = postControl.refreshRequested;
        level1_.rng_ = postScene.rng;
        for (const auto& actor : level1_.entities_) {
            if (actor.type == 11 && actor.primaryType11AnimationSelector) {
                level1_.type11AnimationVariant_ = actor.modelAnimationVariant;
            }
        }
    }
    failurePlayer_ = postScene.player;
    recoveredFailureCamera_ = postScene.camera;
    failureDebris_ = std::move(postScene.debris);
    failureRng_ = postScene.rng;
    failureSourceLoopTick_ = postScene.sourceLoopTick;
    failureVisibleItems_ = std::move(visible);
    failureBursts_ = std::move(bursts);
    failureBurstHistory_ = histories;
    failureAudioEvents_ = std::move(audio);
    recoveredFailureControl_ = std::move(postControl);
    if (work.view) { failureView_ = work.view->view; }
    if (level2) {
        failureMissionEntities_ = std::move(postScene.mission); //Existing diagnostic projection, never an input owner.
    } else if (entering) {
        failureMissionEntities_.clear();
    }
    ticks_ = nextTick;
    if (work.kind == FailureGameplayStepKind::leave) {
        phase_ = GameplayPhase::failurePostExplosionBlackout;
        presentationFrame_ = 0;
    } else {
        phase_ = GameplayPhase::failureExplosion;
        presentationFrame_ = static_cast<unsigned>(work.control.control.display - 1);
    }
    return work;
}
}
