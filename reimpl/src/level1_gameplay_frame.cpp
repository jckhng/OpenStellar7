#include "stellar7/level1_frame.hpp"
#include "level1_reward_model_bank.hpp"
#include "level1_retained_roster_frame.hpp"
#include "stellar7/deployment_prefix_work.hpp"
#include "stellar7/level1_primary_fire_work.hpp"
#include "stellar7/level1_combat_effect_membership.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int16_t initialPlayerArmor = 680;
constexpr std::int16_t rewardType = 25;
constexpr std::size_t fixedResetSlotCount = 8;
constexpr auto playerCombatDomain = Level1PlayerCombatDomain::expiryAndLoss;
}

Level1GameplayFrameWork GameplayModel::advanceRecoveredLevel1Frame(const GameplayAssets& assets,
    const CockpitSettings& settings, const CockpitPageState& pages, const RadarState& radar,
    const IndexedPages& pixels, FrameTailGuards guards) {
    auto& state = level1_;
    const auto firstFrame = !state.hasAdvanced_;
    if (phase_ != GameplayPhase::level1 || ticks_ < recoveredLevel1TickBase_
        || static_cast<std::uint16_t>(ticks_ - recoveredLevel1TickBase_) != state.loopTick_
        || !state.missionActive_ || state.failurePending_ || state.successContactPending_
        || state.ordinaryTailPending_ || recoveredSuccessPause_
        || state.selectedSystem_ != -1
        || std::any_of(state.systemTimers_.begin(),state.systemTimers_.end(),[](auto timer) { return timer != 0; })) {
        throw std::invalid_argument("recovered Level1 frame requires its retained ordinary state");
    }
    const auto noEntity = static_cast<std::size_t>(-1);
    auto guardianIndex = state.postWaveObjectAllocationIndex_ == noEntity
        ? std::optional<std::size_t>{} : std::optional<std::size_t>{state.postWaveObjectAllocationIndex_};
    if (guardianIndex && (*guardianIndex >= state.entities_.size()
        || state.entities_[*guardianIndex].type != 44
        || state.entities_[*guardianIndex].allocationIndex != *guardianIndex)) {
        throw std::invalid_argument("recovered Level1 guardian lost its canonical identity");
    }
    auto rewardIndex = level2PickupIndex_ == noEntity
        ? std::optional<std::size_t>{} : std::optional<std::size_t>{level2PickupIndex_};
    if (rewardIndex && (!guardianIndex || state.levelIndex_ != 0
        || *rewardIndex >= state.entities_.size() || state.entities_[*rewardIndex].type != rewardType
        || state.entities_[*rewardIndex].allocationIndex != *rewardIndex)) {
        throw std::invalid_argument("recovered Level1 reward lost its canonical128E identity");
    }
    auto craftIndex = state.completionObjectAllocationIndex_ == noEntity
        ? std::optional<std::size_t>{} : std::optional<std::size_t>{state.completionObjectAllocationIndex_};
    if (craftIndex && (*craftIndex >= state.entities_.size()
        || !state.entities_[*craftIndex].active()
        || !isLevel1Type7DeploymentState(state.entities_[*craftIndex])
        || state.entities_[*craftIndex].allocationIndex != *craftIndex)) {
        throw std::invalid_argument("recovered Level1 craft lost its canonical identity");
    }
    const auto isCraft = [craftIndex](const auto& actor) {
        return craftIndex && actor.type == 7 && actor.allocationIndex == *craftIndex;
    };
    const auto isGuardian = [guardianIndex](const auto& actor) {
        return guardianIndex && actor.type == 44 && actor.allocationIndex == *guardianIndex;
    };
    const auto isReward = [rewardIndex](const auto& actor) {
        return rewardIndex && actor.type == rewardType && actor.allocationIndex == *rewardIndex;
    };
    //Reject the next unsupported membership before any prefix state is committed.
    constexpr std::int16_t systemBonusType = 42;
    constexpr std::size_t maximumApproachEntities = 15;
    constexpr std::size_t maximumDeploymentEntities = 16;
    const auto maximumRetainedEntities = guardianIndex || (craftIndex
        && state.entities_[*craftIndex].stateBytes4dTo52[5] >= 3)
        ? maximumDeploymentEntities : maximumApproachEntities;
    const auto isRetiredCraft = [guardianIndex](const auto& actor) {
        return guardianIndex && actor.type == 7 && !actor.active()
            && actor.stateBytes4dTo52[5] == 4 && isLevel1Type7DeploymentState(actor);
    };
    const auto isBonus = [systemBonusType](const auto& actor) {
        return actor.type == systemBonusType && actor.stateBytes4dTo52[0] == 0xfe;
    };
    if (state.entities_.size() > maximumRetainedEntities
        || std::count_if(state.entities_.begin(),state.entities_.end(),isBonus) > 1
        || (state.entities_.size() > Level1State::entityCount
            && std::any_of(state.entities_.begin()+Level1State::entityCount,state.entities_.end(),
                [&](const auto& actor) { return actor.type != 1 && actor.type != 3 && actor.type != 11 && !isBonus(actor) && !isCraft(actor) && !isGuardian(actor) && !isRetiredCraft(actor) && !isReward(actor); }))) {
        throw std::invalid_argument("recovered Level1 next frame requires qualified Type1/Type3/Type11/bonus storage");
    }
    const auto dynamicMembership = state.entities_.size() > Level1State::entityCount;
    const auto type3Family = std::any_of(state.entities_.begin(),state.entities_.end(),
        [](const auto& actor) { return actor.type == 3; });
    const auto ordinaryFamily = type3Family ? Level1OrdinaryFamily::type1And3 : Level1OrdinaryFamily::type1;
    const auto recycledOrdinary = state.entities_.size() >= 2
        && (state.entities_[0].type != 1 || state.entities_[1].type != 11);
    const auto objectRef = [](const auto& object) -> Level1ObjectRef {
        switch (object.kind) {
        case Level1State::ObjectKind::entity: return {Level1PresentationKind::entity,object.index};
        case Level1State::ObjectKind::projectile: return {Level1PresentationKind::projectile,object.index};
        case Level1State::ObjectKind::deathEffect: return {Level1PresentationKind::deathEffect,object.index};
        default: throw std::invalid_argument("recovered Level1 frame has an unsupported object");
        }
    };
    const auto privateRef = [](const Level1ObjectRef object) -> Level1State::ObjectReference {
        switch (object.kind) {
        case Level1PresentationKind::entity: return {Level1State::ObjectKind::entity,object.index};
        case Level1PresentationKind::projectile: return {Level1State::ObjectKind::projectile,object.index};
        case Level1PresentationKind::deathEffect: return {Level1State::ObjectKind::deathEffect,object.index};
        default: throw std::invalid_argument("recovered Level1 frame returned an unsupported object");
        }
    };
    const auto ordinaryPool = [&]() {
        Level1PeriodicSpawnInput periodicInput{state.activeEnemyCount_,state.spawnedEnemyCount_,
            static_cast<std::uint8_t>(state.maximumEnemyCount_),
            static_cast<std::uint16_t>(state.allocationList_.size()),state.entities_.size(),{}};
        std::size_t ordinarySlot = 0;
        for (std::size_t index = 0; index < state.entities_.size(); ++index) {
            const auto& actor = state.entities_[index];
            if (actor.stateBytes4dTo52[0] != 0xff) { continue; }
            if (ordinarySlot == periodicInput.ordinaryPool.size()) {
                throw std::invalid_argument("recovered Level1 ordinary pool exceeds its source capacity");
            }
            const auto hasAnimation = actor.primaryType11AnimationSelector
                || actor.renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation
                || actor.renderProfile == WorldActorRenderProfile::heapSelectorNoCallback;
            periodicInput.ordinaryPool[ordinarySlot++] = {actor.stateBytes4dTo52[1],hasAnimation,index};
        }
        periodicInput.selectorReuse=Level1SelectorReuseDomain::retainedType11;
        periodicInput.retainedEntities=&state.entities_;
        return periodicInput;
    };
    const auto hadProjectile = std::any_of(state.projectiles_.begin(),state.projectiles_.end(),
        [](const auto& shot) { return shot.active(); });
    const auto incomingPool = state.projectiles_;
    Level1PrefixWork prefix{};
    std::optional<Level1ProjectilePrefixWork> projectilePrefix;
    std::vector<Level1PrefixPaletteStep> prefixPalettes;
    std::vector<Level1PrefixScoreStep> prefixScores;
    std::vector<Level1PrefixDrawingStep> prefixDrawings;
    std::vector<Level1PrefixBonusBirth> bonusBirths;
    std::vector<Level1Type42BonusReceipt> rewardBirths;
    std::optional<Level1RewardSuccessPause> successPause;
    std::vector<Level1PrefixDeploymentChild> deploymentChildren;
    std::vector<Level1PrefixDeploymentObservation> deploymentObservations;
    std::optional<Level1BonusPickupPresentation> pickupPresentation;
    std::vector<Level1ObjectRef> activeRefs;
    if (hadProjectile || !firstFrame) {
        std::vector<Level1ObjectRef> allocations;
        for (const auto& object : state.allocationList_) { allocations.push_back(objectRef(object)); }
        for (const auto& object : state.objectListOrder_) { activeRefs.push_back(objectRef(object)); }
        constexpr std::size_t tankShotType = 17;
        constexpr std::size_t aircraftShotType = 20;
        if (assets.models.size() <= aircraftShotType) {
            throw std::invalid_argument("enemy fire requires its decoded models");
        }
        const auto& tankShot = assets.models[tankShotType];
        const auto& aircraftShot = assets.models[aircraftShotType];
        constexpr unsigned damageFadeLength = 8;
        if (damageFlashActive_ && damageFlashAge_ >= damageFadeLength) {
            throw std::invalid_argument("recovered Level1 damage fade has invalid retained age");
        }
        const auto fadeTicks = static_cast<std::uint16_t>(damageFlashActive_ ? damageFadeLength - damageFlashAge_ : 0);
        const Level1ProjectilePrefixInput targetInput{state.player_,state.entities_,state.projectiles_,
            allocations,activeRefs,state.projectionListOrigin_,state.nativeInitialListPending_,state.rng_,
            fadeTicks,false,state.playerCollisionMomentum_,state.missionActive_,{rewardIndex.has_value(),false}};
        const auto modelCapacity = guardianIndex ? level1RewardModelCapacity(assets) : assets.sharedModelRegistry[0];
        const Level1DamagePrefixProfile damageProfile{
            {state.loopTick_,modelCapacity,{tankShot.scaleShift,tankShot.collisionRadius},
                {aircraftShot.scaleShift,aircraftShot.collisionRadius},
                Level1ProjectileAllocationDomain::retainedRoster,Level1ActorContactDomain::ordinaryPlayer,
                Level1OrdinaryMembership::periodicSpawned,ordinaryFamily},state.playerArmor_,0,0,0,
                Level1DamagePaletteMode::residentVga,playerCombatDomain};
        constexpr std::size_t systemPanelGroup = 4;
        auto target = [&]() -> Level1TargetPrefixWork {
            const Level1TargetPrefixInput input{targetInput,state.deathEffects_,
                {state.lastDestroyedType_,state.consecutiveDestroyedTypeCount_},state.activeEnemyCount_,state.score_};
            const Level1TargetPrefixProfile profile{damageProfile,
                static_cast<std::uint16_t>(settings.options[4]),type3Family
                    ? Level1TargetHitDomain::periodicClass1 : Level1TargetHitDomain::initialOrdinary};
            if (assets.models.size() <= systemBonusType || assets.artGroups.size() <= systemPanelGroup) {
                //Retain the earlier qualified domain when no bonus resources are supplied.
                return level1TargetPrefixOperations(input,profile,assets.scoreFont,pages);
            }
            const Level1BonusPickupContext pickupContext{state.systemStatusLevels_,state.systemTimers_,
                state.selectedSystem_,assets.artGroups[systemPanelGroup]};
            auto pickup = [&]() -> Level1BonusPickupPrefixWork {
                if (!craftIndex && !guardianIndex) {
                    return level1BonusPickupPrefixOperations(input,profile,assets.scoreFont,pages,
                        assets.sharedModelRegistry[0],assets.models[systemBonusType],pickupContext);
                }
                const Level1DeploymentPrefixContext context{craftIndex,guardianIndex,ordinaryPool(),assets.models};
                auto deployment = guardianIndex
                    ? level1GuardianRewardPrefixOperations(input,profile,assets.scoreFont,pages,
                        modelCapacity,assets.models[systemBonusType],pickupContext,context,{rewardIndex,0})
                    : level1DeploymentPrefixOperations(input,profile,assets.scoreFont,pages,
                        assets.sharedModelRegistry[0],assets.models[systemBonusType],pickupContext,context);
                rewardBirths = std::move(deployment.rewardBirths);
                rewardIndex = deployment.rewardEntityIndex;
                successPause = deployment.successPause;
                deploymentChildren = std::move(deployment.children);
                deploymentObservations = std::move(deployment.observations);
                return std::move(deployment.prefix);
            }();
            state.systemStatusLevels_ = pickup.systems;
            state.systemTimers_ = pickup.timers;
            state.selectedSystem_ = pickup.selectedSystem;
            prefixDrawings = std::move(pickup.drawingSteps);
            pickupPresentation = std::move(pickup.presentation);
            bonusBirths = std::move(pickup.bonus.bonusBirths);
            return std::move(pickup.bonus.prefix);
        }();
        state.deathEffects_ = std::move(target.effects);
        state.lastDestroyedType_ = target.bonus.lastType;
        state.consecutiveDestroyedTypeCount_ = target.bonus.consecutive;
        state.activeEnemyCount_ = target.activeEnemies;
        state.score_ = target.score;
        prefixScores = std::move(target.scoreSteps);
        auto damage = std::move(target.damage);
        auto& combat = damage.combat;
        state.playerArmor_ = damage.armor;
        if (damageFlashActive_ || damage.fadeTicks != 0) {
            damageFlashActive_ = damage.fadeTicks != 0;
            damageFlashAge_ = damageFadeLength - damage.fadeTicks;
        }
        prefixPalettes = std::move(damage.paletteSteps);
        state.audioEvents_.insert(state.audioEvents_.end(),combat.audioEvents.begin(),combat.audioEvents.end());
        projectilePrefix = std::move(combat.prefix);
        state.player_ = projectilePrefix->player;
        state.playerCollisionMomentum_ = projectilePrefix->impulse;
        state.entities_ = projectilePrefix->entities;
        state.projectiles_ = projectilePrefix->projectiles;
        state.rng_ = projectilePrefix->rng;
        state.projectionListOrigin_ = projectilePrefix->lastListCamera;
        state.nativeInitialListPending_ = projectilePrefix->refreshRequested;
        activeRefs = projectilePrefix->activeOrder;
        state.allocationList_.clear();
        for (const auto object : projectilePrefix->allocationOrder) { state.allocationList_.push_back(privateRef(object)); }
    } else {
        std::vector<std::size_t> allocations;
        for (const auto& object : state.allocationList_) {
            if (object.kind != Level1State::ObjectKind::entity) {
                throw std::invalid_argument("recovered Level1 prefix has a non-actor allocation");
            }
            allocations.push_back(object.index);
        }
        prefix = level1PrefixOperations({state.player_,state.entities_,allocations,state.activeListOrder_,
            state.projectionListOrigin_,state.nativeInitialListPending_,state.rng_,0,false,
            state.playerCollisionMomentum_,state.missionActive_,{false,false}});
        state.player_ = prefix.player;
        state.entities_ = prefix.actors;
        state.rng_ = prefix.rng;
        state.projectionListOrigin_ = prefix.lastListCamera;
        state.nativeInitialListPending_ = prefix.refreshRequested;
        for (const auto index : prefix.activeOrder) { activeRefs.push_back({Level1PresentationKind::entity,index}); }
    }
    level2PickupIndex_ = rewardIndex.value_or(noEntity);
    for (const auto& observation : deploymentObservations) {
        if (observation.craftRetired) {
            if (!craftIndex || *craftIndex != observation.entityIndex
                || state.entities_.at(observation.entityIndex).active()) {
                throw std::logic_error("craft retirement lost its canonical identity");
            }
            //B0A9 clears1294;12AA is deliberately retained by this retirement.
            state.completionObjectAllocationIndex_ = noEntity;
            craftIndex.reset();
        }
        if (!observation.guardianBirth) { continue; }
        const auto& birth = *observation.guardianBirth;
        const auto index = birth.actor.allocationIndex;
        if (guardianIndex || index >= state.entities_.size()
            || state.entities_[index].type != 44
            || state.activeEnemyCount_ != birth.activeEnemies
            || birth.appendedAllocationIndex >= state.allocationList_.size()
            || state.allocationList_[birth.appendedAllocationIndex].kind != Level1State::ObjectKind::entity
            || state.allocationList_[birth.appendedAllocationIndex].index != index) {
            throw std::logic_error("guardian receipt disagrees with canonical prefix storage");
        }
        guardianIndex = index;
        state.postWaveObjectAllocationIndex_ = index;
        state.spawnedEnemyCount_ = birth.spawnedEnemies;
    }
    //45D2 remains the complete mixed list; the older entity views are projections.
    state.activeListOrder_.clear();
    state.semanticActiveListOrder_.clear();
    state.objectListOrder_.clear();
    for (const auto object : activeRefs) {
        state.objectListOrder_.push_back(privateRef(object));
        if (object.kind == Level1PresentationKind::entity) {
            state.activeListOrder_.push_back(object.index);
            state.semanticActiveListOrder_.push_back(state.entities_.at(object.index).allocationIndex);
        }
    }
    auto drawingPages = pages;
    auto renderPixels = pixels;
    std::size_t scoreIndex = 0, drawingIndex = 0;
    while (scoreIndex < prefixScores.size() || drawingIndex < prefixDrawings.size()) {
        //A drawing return precedes the next excluded score child at the same offset.
        const auto drawingFirst = drawingIndex < prefixDrawings.size()
            && (scoreIndex == prefixScores.size()
                || prefixDrawings[drawingIndex].operationOffset <= prefixScores[scoreIndex].operationOffset);
        if (drawingFirst) {
            renderPixels.apply(prefixDrawings[drawingIndex++].effects);
            continue;
        }
        renderPixels.apply(prefixScores[scoreIndex++].work.effects);
        drawingPages.selectScoreCopy();
    }
    if (pickupPresentation) {
        const auto& presentation = *pickupPresentation;
        if (presentation.pages.cachePage() != pages.cachePage()
            || presentation.pages.backPage() != pages.backPage()
            || presentation.pages.frontPage() != pages.frontPage()
            || presentation.pages.drawPage() != pages.backPage()
            || presentation.pages.copyPage() != pages.frontPage()
            || presentation.fillStyle != 1 || presentation.fillColor != 137 || presentation.selectedFont != 0) {
            throw std::logic_error("bonus pickup lost source drawing roles/font");
        }
        drawingPages = presentation.pages;
    }
    auto renderRadar = radar;
    for (const auto& birth : bonusBirths) {
        if (!birth.resetProjection) { continue; }
        const auto& reset = *birth.resetProjection;
        if (reset.sourceResetEntry != 0x49f4 || reset.firstResetSlotCount != 8
            || reset.fixedPoolSlot >= reset.firstResetSlotCount
            || reset.entityIndex != birth.receipt.entityIndex || !birth.receipt.materializeEntity
            || reset.entityIndex >= state.entities_.size() || !isBonus(state.entities_[reset.entityIndex])) {
            throw std::logic_error("bonus projection reset lacks earlier loader ancestry");
        }
        //Publish the earlier49F4 reset only for fresh materialization, never reuse.
        renderRadar.initializeAllocation(reset.entityIndex);
    }
    for (const auto& observation : deploymentObservations) {
        if (observation.guardianBirth) {
            //4370 resets this allocation once, including reused selector storage.
            renderRadar.initializeAllocation(observation.guardianBirth->actor.allocationIndex);
        }
    }
    for (const auto& birth : rewardBirths) {
        if (!birth.materializeEntity) { continue; }
        if (birth.poolSlot >= fixedResetSlotCount || birth.entityIndex >= state.entities_.size()
            || birth.drawCallback != 0 || state.entities_[birth.entityIndex].type != rewardType
            || rewardIndex != std::optional<std::size_t>{birth.entityIndex}) {
            throw std::logic_error("reward projection reset lacks earlier49F4 ancestry");
        }
        //4506 retains cache; publish48D4's earlier49F4 only on first materialization.
        renderRadar.initializeAllocation(birth.entityIndex);
    }
    const auto deploymentDomain = guardianIndex || (craftIndex
        && state.entities_[*craftIndex].stateBytes4dTo52[5] >= 3);
    const auto hasBonusStorage = std::any_of(state.entities_.begin(),state.entities_.end(),isBonus);
    constexpr std::size_t sourceOrdinaryPoolCapacity = 16;
    for (std::size_t slot = 0; slot < state.projectiles_.size(); ++slot) {
        if (incomingPool[slot].type == 0 && state.projectiles_[slot].active()) {
            renderRadar.initializeAllocation(sourceOrdinaryPoolCapacity + slot);
        }
    }
    if (successPause) {
        recoveredSuccessPause_ = successPause;
        recoveredSuccessReturned_ = false;
        Level1FramePlan retained{{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
            drawingPages,renderRadar,renderPixels,{}, {}};
        return {std::move(prefix),std::move(retained),{},std::move(projectilePrefix),{},
            std::move(prefixPalettes),{}, {},std::move(prefixScores),std::move(prefixDrawings),{},
            std::move(deploymentChildren),Level1GameplayFrameBoundary::successPaused,successPause};
    }
    //0256/0259 follows0784/score return; prefix already owns its dispatches.
    if (!prefixScores.empty() || !prefixDrawings.empty()) { drawingPages.selectBackCopy(); }
    drawingPages.beginFrame();
    const auto hasProjectile = std::any_of(state.projectiles_.begin(),state.projectiles_.end(),
        [](const auto& shot) { return shot.active(); });
    const Level1PresentationState presentation{{cockpitIndicatorX_,cockpitIndicatorVelocity_},alternateSightPhase_};
    const auto combatFrame = hadProjectile || hasProjectile || state.playerArmor_ != initialPlayerArmor;
    const auto retainedHostileRecords=std::any_of(state.projectiles_.begin(),state.projectiles_.end(),
        [](const auto& shot) {return shot.type==17 || shot.type==20;});
    const auto effectDomain = type3Family || recycledOrdinary || retainedHostileRecords || craftIndex || hasBonusStorage || std::any_of(state.entities_.begin(),state.entities_.end(),
        [](const auto& actor) { return !actor.active(); })
        || std::any_of(activeRefs.begin(),activeRefs.end(),[](const auto ref) {
            return ref.kind == Level1PresentationKind::deathEffect;
        });
    auto render = [&]() -> Level1FramePlan {
        if (effectDomain) {
            std::vector<Level1ObjectRef> liveAllocations;
            for (const auto& object : state.allocationList_) { liveAllocations.push_back(objectRef(object)); }
            Level1CombatRenderContext context{liveAllocations,{},Level1CombatLiftMembership::allowRetiredInitial};
            context.ordinaryFamily = ordinaryFamily;
            context.projectileAllocationDomain=Level1ProjectileAllocationDomain::retainedRoster;
            context.bonusMembership = hasBonusStorage
                ? Level1CombatBonusMembership::oneType42 : Level1CombatBonusMembership::none;
            context.arrivalMembership = deploymentDomain ? Level1CombatArrivalMembership::deployment
                : craftIndex ? Level1CombatArrivalMembership::approachCraft : Level1CombatArrivalMembership::none;
            constexpr std::size_t effectAllocationBase = 32;
            for (const auto ref : activeRefs) {
                if (ref.kind != Level1PresentationKind::deathEffect) { continue; }
                const auto& effect = state.deathEffects_.at(ref.index);
                const auto& actor = effect.entity;
                const auto& cache = renderRadar.projection(effectAllocationBase+ref.index);
                const auto age = actor.stateBytes4dTo52[1];
                const auto end = actor.stateBytes4dTo52[4];
                context.effects.push_back({ref.index,{actor.flags,actor.position},actor.collisionRadius,
                    {actor.flags,effect.projected,cache.projectedRadius,age,end,
                        static_cast<std::uint16_t>(end != 0 ? 0x0ac8 : 0)},actor.targetHeading});
            }
            auto composed = planLevel1RetainedRosterFrame(assets,{state.ground_,state.rng_,state.player_,state.entities_,
                state.projectiles_,activeRefs,state.loopTick_},context,presentation,state.playerArmor_,settings,
                drawingPages,renderRadar,renderPixels);
            for (const auto& observation : composed.effects) {
                auto& effect = state.deathEffects_.at(observation.poolSlot);
                effect.entity.flags = observation.work.state.flags;
                effect.entity.stateBytes4dTo52[1] = observation.work.state.age;
                effect.entity.stateBytes4dTo52[4] = observation.work.state.endAge;
                effect.projected = observation.work.state.projected;
                if (observation.work.retired) {
                    effect.listed = false;
                    state.nativeInitialListPending_ = true;
                }
            }
            //The scene already applies481A sequentially to its ephemeral registry.
            //Adopt the final order once; retain the same old45D2 through radar/tail.
            state.allocationList_.clear();
            for (const auto ref : composed.allocationOrderAfterMain) { state.allocationList_.push_back(privateRef(ref)); }
            return std::move(composed.frame);
        }
        return dynamicMembership
            ? planLevel1PeriodicCombatFrame(assets,{state.ground_,state.rng_,state.player_,state.entities_,
                state.projectiles_,activeRefs,state.loopTick_},presentation,state.playerArmor_,settings,drawingPages,renderRadar,renderPixels)
            : combatFrame
            ? planLevel1CombatFrame(assets,{state.ground_,state.rng_,state.player_,state.entities_,
                state.projectiles_,activeRefs,state.loopTick_},presentation,state.playerArmor_,settings,drawingPages,renderRadar,renderPixels)
            : planLevel1Frame(assets,{state.ground_,state.rng_,state.player_,state.entities_,
                state.activeListOrder_,state.loopTick_},presentation,state.playerArmor_,settings,drawingPages,radar,renderPixels);
    }();
    state.ground_ = render.ground.ground;
    state.rng_ = render.rngAfterScene.value_or(render.ground.rng);
    for (std::size_t ordinal = 0; ordinal < activeRefs.size(); ++ordinal) {
        const auto object = activeRefs[ordinal];
        if (object.kind == Level1PresentationKind::projectile) {
            state.projectiles_.at(object.index).flags = render.radar.actors.at(ordinal).flags;
            continue;
        }
        if (object.kind == Level1PresentationKind::deathEffect) {
            auto& effect = state.deathEffects_.at(object.index);
            const auto& projection = render.radar.actors.at(ordinal);
            effect.entity.flags = projection.flags;
            effect.projected = {projection.projectedX,projection.projectedY};
            continue;
        }
        auto& actor = state.entities_.at(object.index);
        actor.flags = render.radar.actors.at(ordinal).flags;
        if (actor.type == 44) {
            if (render.animationsAfterRadar.size() != activeRefs.size()
                || !render.animationsAfterRadar[ordinal]) {
                throw std::logic_error("guardian rendering lost its ordered selector receipt");
            }
            const auto& animation = *render.animationsAfterRadar[ordinal];
            actor.presentationModelAnimationVariant = actor.modelAnimationVariant;
            actor.stateBytes4dTo52[2] = animation.reload;
            actor.stateBytes4dTo52[3] = animation.countdown;
            actor.modelAnimationVariant = animation.variant;
            continue;
        }
        if (actor.type != 11) { continue; }
        // Main geometry used the incoming selector; the two130C calls each ran once.
        if (render.animationsAfterRadar.size() != activeRefs.size()
            || !render.animationsAfterRadar[ordinal]) {
            throw std::logic_error("Type11 rendering lost its ordered selector receipt");
        }
        actor.presentationModelAnimationVariant = actor.modelAnimationVariant;
        const auto& animation = *render.animationsAfterRadar[ordinal];
        actor.stateBytes4dTo52[2] = animation.reload;
        actor.stateBytes4dTo52[3] = animation.countdown;
        actor.modelAnimationVariant = animation.variant;
        if (actor.primaryType11AnimationSelector) { state.type11AnimationVariant_ = actor.modelAnimationVariant; }
    }
    cockpitIndicatorX_ = render.presentationAfter.indicator.x;
    cockpitIndicatorVelocity_ = render.presentationAfter.indicator.velocity;
    alternateSightPhase_ = render.presentationAfter.alternateSightPhase;
    if (firstFrame) {
        lastCockpitInitialization_.displaySteps = 3;
        lastCockpitInitialization_.states.back() = render.presentationAfter.indicator;
    }
    FrameTailState input{state.loopTick_,state.player_.position,state.player_.yawVelocity,
        state.player_.forwardVelocity,{state.playerWeaponCooldown_,state.playerProjectileTimers_[0],
            state.playerProjectileTimers_[1]}, {}};
    for (const auto object : activeRefs) {
        if (object.kind == Level1PresentationKind::projectile) {
            const auto& shot = state.projectiles_.at(object.index);
            input.actors.push_back({shot.type,shot.position});
        } else if (object.kind == Level1PresentationKind::deathEffect) {
            const auto& actor = state.deathEffects_.at(object.index).entity;
            input.actors.push_back({actor.type,actor.position});
        } else {
            const auto& actor = state.entities_.at(object.index);
            input.actors.push_back({actor.type,actor.position});
        }
    }
    guards.armor = state.playerArmor_;
    guards.firstFrame = firstFrame;
    guards.missionActive = state.missionActive_;
    constexpr std::uint16_t periodicMask = 15;
    if ((state.loopTick_ & periodicMask) == 0) {
        guards.periodic = FrameTailPeriodicInput{0,state.activeEnemyCount_,state.spawnedEnemyCount_,
            static_cast<std::uint8_t>(state.maximumEnemyCount_),
            campaignSpawnThreshold(0,static_cast<std::uint8_t>(state.activeEnemyCount_)),
            state.rng_.indexA(),state.rng_.indexB(),render.radar.enemyFlag};
    } else {
        guards.periodic.reset();
    }
    auto periodicInput = ordinaryPool();
    //Live registry counts are independent from the old display/recenter list.
    Level1CombatEffectMembership effectMembership{state.allocationList_.size(),0,0};
    effectMembership.projectileAllocationDomain=Level1ProjectileAllocationDomain::retainedRoster;
    if (hasBonusStorage) { effectMembership.profile = Level1CombatEffectProfile::systemBonus; }
    for (const auto& object : state.allocationList_) {
        if (object.kind == Level1State::ObjectKind::entity) {
            ++effectMembership.ordinaryAllocations;
            if (isBonus(state.entities_.at(object.index))) { ++effectMembership.bonusAllocations; }
        } else if (object.kind == Level1State::ObjectKind::deathEffect) {
            ++effectMembership.effectAllocations;
        }
    }
    constexpr std::size_t fireKey = 8;
    std::optional<Level1TailFireAction> fire;
    std::optional<WeaponLampWork> lamp;
    auto afterDisplay = render.pages;
    afterDisplay.flip();
    const auto requestLamp = [&](const WeaponLampState requested) {
        constexpr std::size_t panelGroup = 4;
        constexpr std::size_t unavailableLamp = 39;
        constexpr std::size_t readyLamp = 40;
        const auto dirty = weaponIndicatorVisible_
            ? static_cast<std::uint16_t>(weaponReadyIndicator_) : std::uint16_t{0xffff};
        auto next = weaponLampOperations(dirty,requested,
            assets.artGroups.at(panelGroup).at(requested == WeaponLampState::ready ? readyLamp : unavailableLamp),
            lamp ? lamp->pages : afterDisplay);
        const auto operations = next.operations;
        if (lamp) {
            lamp->effects.insert(lamp->effects.end(),next.effects.begin(),next.effects.end());
            lamp->operations.insert(lamp->operations.end(),next.operations.begin(),next.operations.end());
            lamp->pages = next.pages;
            lamp->dirty = next.dirty;
        } else {
            lamp = std::move(next);
        }
        weaponIndicatorVisible_ = true;
        weaponReadyIndicator_ = requested == WeaponLampState::ready;
        return operations;
    };
    if (guards.heldKeys[fireKey]) {
        constexpr std::size_t primaryType = 23;
        if (assets.models.size() <= primaryType) {
            throw std::invalid_argument("primary fire requires its decoded model");
        }
        std::vector<std::size_t> allocatedPoolSlots;
        for (const auto& object : state.allocationList_) {
            if (object.kind == Level1State::ObjectKind::projectile) { allocatedPoolSlots.push_back(object.index); }
        }
        const auto& model = assets.models[primaryType];
        auto firing = level1PrimaryFireOperations({state.player_,state.projectiles_,state.playerWeaponCooldown_,
            state.playerProjectileTimers_,0,state.allocationList_.size(),allocatedPoolSlots,
            state.nativeInitialListPending_,level1ShotOrigin_,
            {assets.sharedModelRegistry[0],model.scaleShift,model.collisionRadius},
            Level1ProjectileAllocationDomain::retainedRoster, deploymentDomain ? Level1PrimaryFireMembership::deployment
                : effectDomain ? Level1PrimaryFireMembership::combatEffects
                : state.entities_.size() > Level1State::entityCount
                    ? Level1PrimaryFireMembership::periodicSpawned : Level1PrimaryFireMembership::initialSix,
            effectDomain ? std::optional<Level1CombatEffectMembership>{effectMembership} : std::nullopt,&state.entities_});
        if (firing.launch) {
            requestLamp(WeaponLampState::unavailable);
            const auto slot = firing.launch->poolSlot;
            const auto fresh = state.projectiles_.at(slot).type == 0;
            //05D4's existing semantic owner appends49F8, never the old45D2 list.
            if (!state.firePrimary()) { throw std::logic_error("qualified primary fire failed its semantic launch"); }
            const auto& appended = state.allocationList_.back();
            if (state.allocationList_.size() != firing.allocationCount
                || firing.launch->allocationIndex + 1 != firing.allocationCount
                || appended.kind != Level1State::ObjectKind::projectile || appended.index != slot) {
                throw std::logic_error("qualified primary fire disagrees with its allocation receipt");
            }
            //The qualified constructor preserves bytes untouched by44BA/05D4.
            state.projectiles_.at(slot) = firing.launch->projectile;
            state.nativeInitialListPending_ = firing.refreshRequested;
            level1ShotOrigin_ = firing.shotOrigin;
            if (fresh) { render.radarState.initializeAllocation(sourceOrdinaryPoolCapacity + slot); }
        }
        fire = Level1TailFireAction{std::move(firing),lamp ? lamp->operations : std::vector<NormalWorkOperation>{}};
    }
    const auto effectiveTimers = fire
        ? std::array<std::uint8_t,3>{fire->work.mainCooldown,fire->work.weaponTimers[0],fire->work.weaponTimers[1]}
        : input.weaponTimers;
    std::vector<std::vector<NormalWorkOperation>> readyLamps;
    for (std::size_t call = 0; call < level1TailReadyLampCalls(effectiveTimers); ++call) {
        readyLamps.push_back(requestLamp(WeaponLampState::ready));
    }
    //032F fire precedes0332 OLD-list recenter and033C periodic birth.
    periodicInput.allocationCount = static_cast<std::uint16_t>(state.allocationList_.size());
    std::optional<Level1ArrivalWork> arrivalWork;
    auto periodicTail = [&]() -> Level1PeriodicTailWork {
        if (craftIndex || state.spawnedEnemyCount_ >= state.maximumEnemyCount_ || render.radar.enemyFlag != 0) {
            const Level1ArrivalInput arrivalInput{render.radar.enemyFlag,craftIndex,guardianIndex,state.player_,periodicInput};
            auto composed = rewardIndex
                ? level1RewardFrameTailOperations(input,guards,state.rng_,arrivalInput,assets.models,
                    effectMembership,fire,readyLamps,playerCombatDomain)
                : deploymentDomain
                ? level1DeploymentFrameTailOperations(input,guards,state.rng_,arrivalInput,assets.models,
                    effectMembership,fire,readyLamps,playerCombatDomain)
                : level1ArrivalFrameTailOperations(input,guards,state.rng_,arrivalInput,assets.models,
                    effectMembership,fire,readyLamps,playerCombatDomain);
            arrivalWork = std::move(composed.arrival);
            return std::move(composed.periodic);
        }
        return effectDomain
            ? level1CombatEffectFrameTailOperations(input,guards,state.rng_,periodicInput,assets.models,
                effectMembership,fire,readyLamps,playerCombatDomain)
            : level1PeriodicCombatFrameTailOperations(input,guards,state.rng_,
                periodicInput,assets.models,fire,readyLamps,playerCombatDomain);
    }();
    auto tail = std::move(periodicTail.tail);
    state.audioEvents_.insert(state.audioEvents_.end(),tail.audioEvents.begin(),tail.audioEvents.end());
    state.player_.position = tail.state.playerPosition;
    state.player_.yawVelocity = tail.state.yawVelocity;
    state.player_.forwardVelocity = tail.state.forwardVelocity;
    state.playerWeaponCooldown_ = tail.state.weaponTimers[0];
    state.playerProjectileTimers_ = {tail.state.weaponTimers[1],tail.state.weaponTimers[2]};
    for (std::size_t ordinal = 0; ordinal < activeRefs.size(); ++ordinal) {
        const auto object = activeRefs[ordinal];
        if (object.kind == Level1PresentationKind::projectile) {
            state.projectiles_.at(object.index).position = tail.state.actors[ordinal].position;
        } else if (object.kind == Level1PresentationKind::deathEffect) {
            state.deathEffects_.at(object.index).entity.position = tail.state.actors[ordinal].position;
        } else {
            state.entities_.at(object.index).position = tail.state.actors[ordinal].position;
        }
    }
    if (periodicTail.periodic && periodicTail.periodic->spawn) {
        const auto& receipt = *periodicTail.periodic->spawn;
        const auto index = receipt.actor.allocationIndex;
        if (receipt.appendedAllocationIndex != state.allocationList_.size()
            || index > state.entities_.size()) {
            throw std::logic_error("periodic constructor disagrees with its allocation receipt");
        }
        if (index == state.entities_.size()) {
            state.entities_.push_back(receipt.actor);
        } else {
            state.entities_[index] = receipt.actor;
        }
        state.allocationList_.push_back({Level1State::ObjectKind::entity,index});
        state.activeEnemyCount_ = receipt.activeEnemies;
        state.spawnedEnemyCount_ = receipt.spawnedEnemies;
        state.nativeInitialListPending_ = receipt.refreshRequested;
        state.lastSpawnCheckpoint_ = receipt.actor;
        state.lastSpawnTick_ = state.loopTick_;
        state.lastSpawnRng_ = periodicTail.periodic->rng;
        state.hasLastSpawnCheckpoint_ = true;
        //49F4 clears this allocation's cache once; it is not in OLD45D2 yet.
        render.radarState.initializeAllocation(index);
    }
    if (arrivalWork && arrivalWork->birth) {
        const auto& receipt = *arrivalWork->birth;
        const auto index = receipt.actor.allocationIndex;
        if (craftIndex || receipt.actor.type != 7 || receipt.actor.stateBytes4dTo52[1] != 100
            || receipt.appendedAllocationIndex != state.allocationList_.size()
            || index > state.entities_.size()
            || (index == state.entities_.size() && index >= maximumRetainedEntities)
            || (index < state.entities_.size() && state.entities_[index].active())) {
            throw std::logic_error("arrival constructor disagrees with canonical allocation storage");
        }
        if (index == state.entities_.size()) {
            state.entities_.push_back(receipt.actor);
        } else {
            state.entities_[index] = receipt.actor;
        }
        state.allocationList_.push_back({Level1State::ObjectKind::entity,index});
        state.activeEnemyCount_ = receipt.activeEnemies;
        state.spawnedEnemyCount_ = receipt.spawnedEnemies;
        state.nativeInitialListPending_ = receipt.refreshRequested;
        state.completionObjectAllocationIndex_ = index;
        //4370 owns49F4 on both fresh allocation and ordinary-slot reuse.
        render.radarState.initializeAllocation(index);
    }
    state.rng_ = tail.rng;
    state.loopTick_ = tail.state.loopTick;
    state.hasAdvanced_ = true;
    state.ordinaryTailPending_ = false;
    state.ordinaryTailConsumed_ = true;
    state.pendingPostBoundaryInput_ = {};
    ++ticks_;
    if (tail.failureRequested) {
        state.missionActive_ = tail.missionActive;
        state.failureState_ = true;
        state.failurePending_ = false;
        state.failureTailConsumed_ = true;
        failureSourceLoopTick_ = state.loopTick_;
        level1FailureTailFinished_ = true;
        failureFromLevel2_ = false;
        phase_ = GameplayPhase::failurePreExplosionBlackout;
        presentationFrame_ = 0;
    }
    return {std::move(prefix),std::move(render),std::move(tail),std::move(projectilePrefix),std::move(lamp),
        std::move(prefixPalettes),std::move(periodicTail.periodic),
        std::move(periodicTail.beforePeriodicProvider),std::move(prefixScores),std::move(prefixDrawings),std::move(arrivalWork),std::move(deploymentChildren)};
}

} // namespace stellar7
