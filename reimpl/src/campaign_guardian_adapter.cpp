#include "campaign_contact.hpp"
#include "campaign_guardian.hpp"
#include "level5_guardian.hpp"
#include "playable_decoy.hpp"
#include "stellar7/guardian_deployment_work.hpp"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace stellar7 {

GameplayModel::CampaignArrivalAction GameplayModel::advanceCampaignArrivalCraft(Level1Entity& entity) {
    constexpr std::size_t craftBehaviorByte = 5;
    constexpr std::uint8_t approachState = 0;
    constexpr std::uint8_t brakingState = 1;
    constexpr std::uint8_t beamOpeningState = 2;
    constexpr std::uint8_t deploymentState = 3;
    constexpr std::uint8_t departureState = 4;

    // FUN_1000:af8c writes each transition to the retained craft record.
    auto& behavior = entity.stateBytes4dTo52[craftBehaviorByte];
    if (behavior == approachState) {
        const auto difference = static_cast<std::int16_t>(
            entity.targetHeading - entity.yaw
        );
        const auto nextYawVelocity = static_cast<std::int16_t>(
            std::clamp<std::int32_t>(difference, -0x500, 0x500)
        );
        auto bank = static_cast<std::int32_t>(
            static_cast<std::int16_t>(entity.roll)
        );
        bank += arithmeticShiftRightFour(
            -(
                static_cast<std::int32_t>(nextYawVelocity) * 0x10
                + bank
            )
        );
        entity.roll = static_cast<std::uint16_t>(bank);
        entity.yawVelocity = nextYawVelocity;
        if (entity.presenterDistance < 1000U) {
            behavior = brakingState;
            entity.behaviorTimer = 0x50;
        }
    } else if (behavior == brakingState) {
        entity.forwardVelocity = static_cast<std::int16_t>(
            entity.forwardVelocity - 5
        );
        --entity.behaviorTimer;
        if (entity.forwardVelocity == 0) {
            behavior = beamOpeningState;
            entity.behaviorTimer = 0x1D;
        }
    } else if (behavior == beamOpeningState) {
        --entity.behaviorTimer;
        entity.stateBytes4dTo52[2] = static_cast<std::uint8_t>(
            static_cast<std::int8_t>(entity.behaviorTimer) / -10
            + 3
        );
        if (entity.behaviorTimer == 0) {
            behavior = deploymentState;
            entity.behaviorTimer = 0x46;
            entity.flags = static_cast<std::uint8_t>(
                entity.flags | 0x08U
            );
            entity.renderProfile = entity.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback
                ? WorldActorRenderProfile::inlineSelectorBeamCallback : WorldActorRenderProfile::unknown;
        }
    } else if (behavior == deploymentState) {
        --entity.behaviorTimer;
        if (static_cast<std::int8_t>(entity.behaviorTimer) < 0x1E) {
            if (entity.behaviorTimer == 0x1D) {
                entity.renderProfile = entity.renderProfile == WorldActorRenderProfile::inlineSelectorBeamCallback
                    ? WorldActorRenderProfile::inlineSelectorNoCallback : WorldActorRenderProfile::unknown;
                entity.flags = static_cast<std::uint8_t>(
                    entity.flags & static_cast<std::uint8_t>(~0x08U)
                );
                return CampaignArrivalAction::deployGuardian;
            } else {
                entity.stateBytes4dTo52[2] =
                    static_cast<std::uint8_t>(
                        static_cast<std::int8_t>(
                            entity.behaviorTimer
                        ) / 10
                    );
                if (entity.behaviorTimer == 0) {
                    behavior = departureState;
                    entity.renderProfile = entity.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback
                        ? WorldActorRenderProfile::inlineSelectorDeparture : WorldActorRenderProfile::unknown;
                    // Native b452 dispatches operation 0x14 (SBL: RET),
                    // not the event-start operation used by b3e6.
                }
            }
        }
    } else if (behavior == departureState) {
        auto bank = static_cast<std::int32_t>(
            static_cast<std::int16_t>(entity.roll)
        );
        bank += arithmeticShiftRightFour(
            -(
                static_cast<std::int32_t>(0x100) * 0x10
                + bank
            )
        );
        entity.roll = static_cast<std::uint16_t>(bank);
        entity.pitch = static_cast<std::uint16_t>(
            entity.pitch + 0x90
        );
        entity.forwardVelocity = static_cast<std::int16_t>(
            entity.forwardVelocity + 0x32
        );
        entity.yawVelocity = 0x100;
        if (entity.presenterDistance > 60000U) {
            entity.stateBytes4dTo52[1] = 0;
            return CampaignArrivalAction::retire;
        }
    }
    return CampaignArrivalAction::none;
}

void GameplayModel::advancePhysicalLevel2Guardian(const std::size_t slot, const Level1Entity& behaviorTarget,
    const CampaignProjectileContactResult& projectileContact, bool& guardianAllocatedThisBoundary) {
    const auto live = std::find(level2EntityActorPoolSlots_.begin(), level2EntityActorPoolSlots_.end(), slot);
    const auto index = static_cast<std::size_t>(live - level2EntityActorPoolSlots_.begin());
    auto actor = live == level2EntityActorPoolSlots_.end() ? level2ActorPool_[slot] : level2Entities_[index];
    refreshPlayableTarget(actor, behaviorTarget);
    Level2GuardianBehaviorInput input{actor, level2Rng_, level2CompletionObjectIndex_ != static_cast<std::size_t>(-1)};
    input.retainedContactPresent = projectileContact.guardianRetiredContact[slot];
    const auto work = level2GuardianBehaviorOperations(input);
    level2Rng_ = work.rng;
    level2ActorPool_[slot] = work.actor;
    if (live != level2EntityActorPoolSlots_.end()) {
        level2Entities_[index] = work.actor;
    }
    if (!work.emission) {
        return;
    }
    if (*work.emission == Level2GuardianEmission::pulse46) {
        constexpr std::size_t pulseType = 46;
        constexpr std::uint32_t pulseRadius = 2260;
        if (!campaignModelsBound_ || campaignModels_[pulseType].radius != pulseRadius
            || campaignModels_[pulseType].selectors != 0) {
            throw std::invalid_argument("Level2 pulse requires its decoded selectorless model46");
        }
        for (std::size_t fixed = 0; fixed < level2FixedObjectPoolSize_; ++fixed) {
            if (!level2FixedPoolKnown_[fixed]) {
                throw std::invalid_argument("Level2 pulse reached unknown fixed-record storage");
            }
            if (level2FixedPool_[fixed].active()) {
                continue;
            }
            const auto profile = level2FixedPool_[fixed].renderProfile;
            if (profile != WorldActorRenderProfile::noHandles && profile != WorldActorRenderProfile::inlineSelectorNoCallback) {
                throw std::invalid_argument("Level2 pulse requires a known callback-free fixed record");
            }
            break;
        }
        Level1Entity request{};
        request.type = pulseType;
        request.collisionRadius = pulseRadius;
        const auto pulse = allocateLevel2FixedObject(request);
        if (pulse < level2Entities_.size()) {
            guardianAllocatedThisBoundary = true;
            level2Entities_[pulse].position = work.actor.position;
            level2FixedPool_[level2EntityFixedPoolSlots_[pulse]] = level2Entities_[pulse];
        }
        return;
    }
    const auto free = std::find(level2ActorPoolActive_.begin(), level2ActorPoolActive_.end(), false);
    if (free == level2ActorPoolActive_.end()) {
        return;
    }
    const auto birthSlot = static_cast<std::size_t>(free - level2ActorPoolActive_.begin());
    if (!playableOrdinaryConstructor_ || (level2ActorIncarnations_[birthSlot] != 0
        && level2ActorPool_[birthSlot].renderProfile == WorldActorRenderProfile::unknown)) {
        throw std::invalid_argument("Level2 AE10 requires its shared constructor and known prior selector");
    }
    //4370 resets the canonical slot before AE10 reads its source, including self-aliases.
    const auto birth = allocateLevel2Actor(playableOrdinaryConstructor_->construct(13, level2Rng_),
        ActorInitialization::validatedOrdinary);
    guardianAllocatedThisBoundary = true;
    const auto origin = level2ActorPool_[slot];
    auto& emitted = level2Entities_[birth];
    emitted.position = origin.position;
    emitted.position.z = static_cast<std::int32_t>(static_cast<std::uint32_t>(emitted.position.z) + 500U);
    emitted.pitch = origin.pitch;
    emitted.roll = 0;
    emitted.yaw = origin.yaw;
    emitted.type = 48;
    emitted.forwardVelocity = emitted.yawVelocity = 0;
    emitted.behaviorTimer = nextLevel2VisualRandomByte();
    emitted.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
    emitted.animatedModelPresenterEnabled = false;
    level2ActorPool_[birthSlot] = emitted;
    ++level2ActiveEnemyCount_;
    ++level2SpawnedEnemyCount_;
}

void GameplayModel::advanceLevel5Guardian(Level1Entity& actor, const std::size_t ownerSlot) {
    const auto sourceTick = static_cast<std::uint16_t>(level2LoopTick_ - 1);
    advanceLevel5GuardianBehavior(actor, sourceTick, [this, ownerSlot](auto& owner) {
        if (std::find(level2ProjectilePoolActive_.begin(), level2ProjectilePoolActive_.end(), false)
            == level2ProjectilePoolActive_.end()) {
            throw std::invalid_argument("Level5 ACEC requires a free projectile record");
        }
        fireLevel2EnemyProjectile(owner, ownerSlot, 0);
    });
}

void GameplayModel::advanceCampaignGuardian(Level1Entity& actor, const std::size_t ownerSlot,
    const std::vector<Level2EntityIdentity>& identities, const CampaignCombatCallbacks& combat) {
    constexpr auto fixedContactBase = retiredFixedContactBase_;
    CampaignGuardianContext context{};
    context.playerPosition = level2Player_.position;
    context.playerProxyPosition = &campaignPlayerProxy_.position;
    context.level6Turn = &level6GuardianTurn_;
    context.craftPresent = level2CompletionObjectIndex_ != static_cast<std::size_t>(-1);
    for (const auto identity : identities) {
        if (identity.pool != 1) { continue; }
        const auto index = campaignEntityIndex(level2ActorPoolSize_ + identity.slot);
        context.inheritedProjectiles.push_back({identity.slot,
            index < level2Entities_.size() ? level2Entities_[index] : level2ProjectilePool_[identity.slot]});
    }
    if (actor.collisionContactIsPlayer) {
        context.contact = CampaignGuardianContact{static_cast<std::size_t>(-1), 0, true};
    } else if (actor.collisionContactAllocationIndex>=fixedContactBase
        && actor.collisionContactAllocationIndex-fixedContactBase<level2FixedObjectPoolSize_) {
        const auto slot=actor.collisionContactAllocationIndex-fixedContactBase;
        context.contact=CampaignGuardianContact{level2ActorPoolSize_*3+slot,level2FixedPool_[slot].type,false};
    } else if (actor.collisionContactAllocationIndex < level2Entities_.size()) {
        const auto contact = actor.collisionContactAllocationIndex;
        context.contact = CampaignGuardianContact{campaignEntityIdentity(contact), level2Entities_[contact].type, false};
    }
    CampaignGuardianCallbacks callbacks{};
    callbacks.damageGuardian = [&](Level1Entity& shadow, const std::uint8_t amount) {
        applyCampaignHit(shadow, amount, ownerSlot, combat);
    };
    callbacks.retireProjectile = [&](const std::size_t slot) {
        const auto index = campaignEntityIndex(level2ActorPoolSize_ + slot);
        if (index < level2Entities_.size()) {
            combat.retire(index);
        } else {
            level2ProjectilePool_[slot].stateBytes4dTo52[1] = 0;
            level2ProjectilePoolActive_[slot] = false;
        }
    };
    callbacks.damageContact = [&](const CampaignGuardianContact& contact, const std::uint8_t amount) {
        if (contact.player) {
            auto proxy = level2Player_;
            applyCampaignHit(proxy, amount, static_cast<std::size_t>(-1), combat);
            level2Player_.stateBytes4dTo52[1] = proxy.stateBytes4dTo52[1];
            return;
        }
        const auto index = campaignEntityIndex(contact.identity);
        if (index >= level2Entities_.size()) {
            const auto kind=contact.identity/level2ActorPoolSize_;
            const auto slot=contact.identity%level2ActorPoolSize_;
            if(kind!=3 || slot>=level2FixedObjectPoolSize_ || !level2FixedPoolKnown_[slot]
                || level2FixedPool_[slot].type!=43) {
                throw std::invalid_argument("guardian hit lost its retained physical contact");
            }
            auto shadow=level2FixedPool_[slot];
            applyCampaignHit(shadow,amount,contact.identity, combat);
            level2FixedPool_[slot].stateBytes4dTo52[1]=shadow.stateBytes4dTo52[1];
            return;
        }
        auto shadow = level2Entities_[index];
        applyCampaignHit(shadow, amount, contact.identity, combat);
    };
    callbacks.changePlayerArmor = [&](const std::int16_t delta) {
        level2AudioEvents_.push_back({level2LoopTick_, 6});
        level2PlayerArmor_ = static_cast<std::int16_t>(level2PlayerArmor_ + delta);
    };
    callbacks.attack = [&](const Level1Entity& owner, const CampaignGuardianAttack attack) {
        if (attack == CampaignGuardianAttack::aimedProjectile || attack == CampaignGuardianAttack::volleyProjectile80) {
            auto shadow = owner;
            return fireLevel2EnemyProjectile(shadow, ownerSlot, -1);
        }
        if (attack == CampaignGuardianAttack::pulse46) {
            Level1Entity request{};
            request.type = 46;
            request.collisionRadius = campaignModels_[46].radius;
            const auto index = allocateLevel2FixedObject(request);
            if (index >= level2Entities_.size()) { return false; }
            auto& pulse = level2Entities_[index];
            pulse.position = owner.position;
            level2FixedPool_[level2EntityFixedPoolSlots_[index]] = pulse;
            return true;
        }
        if (std::find(level2ActorPoolActive_.begin(), level2ActorPoolActive_.end(), false)
            == level2ActorPoolActive_.end()) { return false; }
        if (!playableOrdinaryConstructor_) {
            throw std::invalid_argument("AE10 requires the retained shared Type13 constructor");
        }
        auto emitted = playableOrdinaryConstructor_->construct(13, level2Rng_);
        emitted.position = owner.position;
        emitted.position.z += 500;
        emitted.pitch = owner.pitch;
        emitted.roll = 0;
        emitted.yaw = owner.yaw;
        emitted.type = 48;
        emitted.forwardVelocity = emitted.yawVelocity = 0;
        emitted.behaviorTimer = nextLevel2VisualRandomByte();
        emitted.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
        emitted.animatedModelPresenterEnabled = false;
        const auto index = allocateLevel2Actor(emitted, ActorInitialization::validatedOrdinary);
        if (index >= level2Entities_.size()) { return false; }
        ++level2ActiveEnemyCount_;
        ++level2SpawnedEnemyCount_;
        return true;
    };
    const auto family = activeCampaignLevel_ == 0 ? CampaignGuardianFamily::level1
        : activeCampaignLevel_ == 3 ? CampaignGuardianFamily::level4
        : activeCampaignLevel_ == 5 ? CampaignGuardianFamily::level6 : CampaignGuardianFamily::level7;
    advanceCampaignGuardianBehavior(actor, family, level2Rng_, context, callbacks);
    const auto live = campaignEntityIndex(ownerSlot);
    if (live < level2Entities_.size()) {
        actor.allocationIndex = live;
        level2Entities_[live] = actor;
    }
    level2ActorPool_[ownerSlot] = actor;
}

std::optional<Level1Entity> GameplayModel::advanceLevel3Guardian(Level1Entity& entity) {
    std::optional<Level1Entity> burstSource;
    // Level 3 selects FUN_1000:a50e rather than the Level 2
    // a92e guardian callback.  The native routine keeps the
    // Type-44 record in place while its four-state descent/
    // retreat machine runs; state 1 emits two Type-47 records
    // when its timer crosses ten with no completion object.
    const auto choice = level2Rng_.next();
    auto& behavior = entity.stateBytes4dTo52[5];
    const auto headingDifference = static_cast<std::int16_t>(
        entity.targetHeading - entity.yaw
    );
    const auto headingMagnitude = static_cast<std::int16_t>(
        headingDifference
            == std::numeric_limits<std::int16_t>::min()
        ? std::numeric_limits<std::int16_t>::min()
        : std::abs(static_cast<int>(headingDifference))
    );
    const auto steer = [&entity](
        const std::int16_t difference,
        const std::int16_t limit
    ) noexcept {
        // FUN_1000:ac3a canonicalizes the otherwise ambiguous
        // signed half-turn 0x8000 to +0x7fff before applying the
        // steering limit.
        const auto canonicalDifference = difference
                == std::numeric_limits<std::int16_t>::min()
            ? static_cast<std::int32_t>(
                std::numeric_limits<std::int16_t>::max()
            )
            : static_cast<std::int32_t>(difference);
        const auto bounded = std::clamp<std::int32_t>(
            canonicalDifference,
            -limit,
            limit
        );
        entity.yawVelocity = static_cast<std::int16_t>(bounded);
        // FUN_1000:ac3a damps the roll accumulator toward the
        // selected steering speed while the object is above
        // the ground threshold.
        if (entity.position.z > 499) {
            const auto roll = static_cast<std::int16_t>(
                entity.roll
            );
            entity.roll = static_cast<std::uint16_t>(
                roll + (-(bounded * 16 + roll) >> 4)
            );
        }
    };
    if (behavior == 0) {
        entity.animatedModelPresenterEnabled = false; //A542 clears source+1A.
        entity.renderProfile = WorldActorRenderProfile::heapSelectorNoCallback;
        const auto launchGateOpen =
            entity.position.z > 0x833
            && choice > 199U
            && entity.collisionContactAllocationIndex
                == static_cast<std::size_t>(-1)
            && !entity.collisionContactIsPlayer;
        if (launchGateOpen) {
            entity.position.z = 0x834;
            steer(headingDifference, 0x200);
            entity.forwardVelocity = 300;
            if (
                headingMagnitude < 0x200
                && entity.presenterDistance > 4000U
            ) {
                behavior = 1;
            } else if (headingMagnitude < 0x200) {
                behavior = 2;
            }
        } else {
            // FUN_1000:a50e falls through from state 0 to state 2
            // when its launch gate is closed (including a live
            // contact record). A large heading error is the one
            // native path that retains state 0 after the launch
            // record has been initialized.
            behavior = 2;
        }
    } else if (behavior == 1) {
        steer(headingDifference, 0x200);
        if (entity.position.z > 500) {
            entity.position.z = std::max(
                0,
                entity.position.z - 100
            );
            if (
                entity.position.z < 501
                && headingMagnitude < 200
            ) {
                entity.behaviorTimer = 20;
            } else if (entity.position.z < 501) {
                behavior = 2;
                entity.behaviorTimer = 0;
            }
        } else {
            --entity.behaviorTimer;
            if (
                entity.behaviorTimer == 10
                && level2CompletionObjectIndex_
                    == static_cast<std::size_t>(-1)
            ) {
                burstSource = entity;
            } else if (
                entity.behaviorTimer == 0
                || entity.collisionContactAllocationIndex
                    != static_cast<std::size_t>(-1)
                || entity.collisionContactIsPlayer
            ) {
                // FUN_1000:a50e retains the timer-10 emission
                // return, but every other live contact falls
                // through from the state-1 hold into state 2.
                behavior = 2;
            }
        }
    } else if (behavior == 2) {
        if (entity.behaviorTimer != 0) {
            --entity.behaviorTimer;
        } else if (entity.position.z < 0x834) {
            // FUN_1000:a50e clears the contact word before the
            // zero-timer return climb.
            entity.collisionContactAllocationIndex =
                static_cast<std::size_t>(-1);
            entity.collisionContactIsPlayer = false;
            entity.position.z = std::min(
                0x834,
                entity.position.z + 100
            );
        } else {
            entity.collisionContactAllocationIndex =
                static_cast<std::size_t>(-1);
            entity.collisionContactIsPlayer = false;
            behavior = 3;
        }
    } else if (behavior == 3) {
        steer(
            static_cast<std::int16_t>(
                headingDifference + 0x8000
            ),
            200
        );
        entity.forwardVelocity = 300;
        if (entity.presenterDistance > 16000U) {
            behavior = 0;
        }
    }
    // FUN_1000:a50e stores a triangular model selector through
    // the +1c presentation pointer: 5 - abs((timer >> 1) - 5).
    // The earlier absolute-value-only form inverted this cycle
    // at the native timer-10 emission boundary.
    const auto animationDistance = std::abs(
        static_cast<int>(entity.behaviorTimer) / 2 - 5
    );
    const auto animation = std::max(0, 5 - animationDistance);
    entity.modelAnimationVariant = static_cast<std::uint8_t>(animation);
    entity.presentationModelAnimationVariant = entity.modelAnimationVariant;
    return burstSource;
}

void GameplayModel::deployLevel3GuardianBurst(const Level1Entity& source) {
    constexpr std::uint8_t emissionsPerBurst = 2;
    // FUN_1000:a50e calls FUN_1000:ae10 twice, with opposite nonzero
    // launch parameters.  Both records finish with zero forward speed,
    // zero visual timer, and a 500-unit altitude offset; the parameter
    // affects their transient steering setup only.
    for (std::uint8_t emission = 0;
         emission < emissionsPerBurst;
         ++emission) {
        if (std::none_of(
            level2ActorPoolActive_.begin(),
            level2ActorPoolActive_.end(),
            [](const bool active) noexcept { return !active; }
        )) {
            // FUN_1000:4370 searches its sixteen-record pool before
            // calling FUN_1000:4862 or either Type-13 RNG initializer.
            // A saturated pool therefore makes FUN_1000:ae10 a
            // zero-draw no-op for this and every remaining emission.
            break;
        }
        // FUN_1000:ae10 requests provisional Type 13 from the shared
        // FUN_1000:4370 allocator. Preserve the initializer draw at
        // FUN_1000:4869 plus the Type-13 steering and yaw draws at
        // FUN_1000:4434/4466 before ae10 overwrites the record as 47.
        level2Rng_.next();
        level2Rng_.next();
        level2Rng_.next();
        Level1Entity emitted{};
        emitted.type = 47;
        emitted.collisionRadius = playableCampaignFrames_ && campaignModelsBound_ ? campaignModels_[47].radius : 0;
        emitted.position = source.position;
        emitted.position.z += 500;
        emitted.pitch = source.pitch;
        emitted.roll = 0;
        // FUN_1000:ae10 raises the copied yaw high byte by 0x40 for the
        // transient FUN_1000:3b3a launch, then subtracts 0x4000 before
        // storing the final record yaw.
        emitted.yaw = static_cast<std::uint16_t>(
            source.yaw + 0x4000U
        );
        emitted.forwardVelocity = emission == 0 ? 1000 : -1000;
        advanceEntityKinematics(emitted);
        emitted.yaw = static_cast<std::uint16_t>(
            emitted.yaw - 0x4000U
        );
        emitted.forwardVelocity = 0;
        emitted.stateBytes4dTo52 = {255, 1, 0, 0, 4, 0};
        emitted.behaviorTimer = 0;
        const auto emittedIndex = allocateLevel2Actor(emitted);
        if (emittedIndex != static_cast<std::size_t>(-1)) {
            ++level2ActiveEnemyCount_;
            ++level2SpawnedEnemyCount_;
        }
    }
}

} // namespace stellar7
