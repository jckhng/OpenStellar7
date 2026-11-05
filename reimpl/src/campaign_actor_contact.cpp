#include "campaign_contact.hpp"
#include "playable_bomb.hpp"
#include "playable_jump.hpp"
#include <algorithm>
#include <stdexcept>

namespace stellar7 {

GameplayModel::CampaignContactFlow GameplayModel::resolveCampaignActorContacts(
    const CampaignContactStart& start, const CampaignContactMotion& motion,
    const CampaignCombatCallbacks& combat) {
    const bool playableLevel1 = playableCampaignFrames_ && activeCampaignLevel_ == 0;
    constexpr std::uint8_t projectilePoolKind = 0xfd;
    constexpr std::uint8_t skipSourceContactMode = 3;
    constexpr auto fixedContactBase = retiredFixedContactBase_;
    // FUN_1000:0784 walks the inherited pointer list. A Type-23 callback can
    // rebuild the allocation list later in that same walk, but it cannot
    // retroactively reorder actor collisions which preceded it. Portable
    // projectile handling is separated into an earlier pass, so retain the
    // boundary identities here instead of consuming the rebuilt list.
    const auto level2CollisionOrder = remapLevel2BoundaryOrder(start.identities);
    std::vector<std::size_t> collisionIdentities;
    if(playableCampaignFrames_) {
        for(const auto identity:start.identities) {
            const auto base=identity.pool==0 ? 0 : identity.pool==1 ? level2ActorPoolSize_
                : identity.pool==2 ? level2ActorPoolSize_*2 : level2ActorPoolSize_*3;
            collisionIdentities.push_back(base+identity.slot);
        }
    }
    const auto collisionCount=playableCampaignFrames_ ? collisionIdentities.size() : level2CollisionOrder.size();
    auto sourceRemoval = static_cast<std::size_t>(-1);
    std::int16_t sourceRemovalPlayerDamage = 0;
    std::vector<std::size_t> level2Type47MutualHitActorSlots;
    for (std::size_t position=0;position<collisionCount;++position) {
        const auto sourceIndex=playableCampaignFrames_ ? campaignEntityIndex(collisionIdentities[position])
            : level2CollisionOrder[position];
        if (sourceIndex>=level2Entities_.size() || !campaignActorExistedAtStart(sourceIndex, start)) {
            continue;
        }
        const auto sourceProjectileSlot =
            level2EntityProjectilePoolSlots_[sourceIndex];
        if (
            sourceProjectileSlot < level2ProjectilePoolSize_
            && !start.projectiles[sourceProjectileSlot]
        ) {
            continue;
        }
        auto& source = level2Entities_[sourceIndex];
        if (start.retainedLevel3 && source.stateBytes4dTo52[0] != 0xff
            && !(playableCampaignFrames_ && source.stateBytes4dTo52[0] == projectilePoolKind)) {
            continue;
        }
        if (playableCampaignFrames_ && source.stateBytes4dTo52[0]==0xfe && source.type==43) {continue;}
        if (source.stateBytes4dTo52[0] == 0xfe
            && (source.type == 42 || (activeCampaignLevel_ == 4 && source.type == 25))) {
            //0784 integrates only42; the player pass and0A9A own reward25.
            continue;
        }
        const auto sourceFlags = static_cast<std::int8_t>(
            source.stateBytes4dTo52[0]
        );
        const auto hostileProjectile =
            source.type == 17
            || source.type == 18
            || source.type == 19
            || source.type == 20
            || source.type == 22;
        if (sourceFlags == -4 || (sourceFlags == -3 && !hostileProjectile)) {
            // FUN_1000:0784 dispatches -3 records to FUN_1000:04d8. Hostile
            // shots still need its contact/retirement path: Level 2 loop
            // 382->383 removes a Type-19 shot on Type 30. Player shots are
            // handled above; presenter-only Type -1 records stay excluded
            // (Level 3 loop 1532 leaves the Type-44 contact word clear).
            continue;
        }
        const auto sourceActorSlot =
            level2EntityActorPoolSlots_[sourceIndex];
        auto sourceCollidedWithActor = false;
        auto sourceCollidedWithType47 = false;
        auto lastCollisionTargetType = std::int16_t{0};
        auto lastBombSlot=level2FixedObjectPoolSize_;
        if (campaignCollides(source, level2Player_)) {
            level2PrefixGuards_.reset();
            if (source.type == 25 || source.type == 26) {
                level2Complete_ = true;
                level3CompletionByPickup_ = activeCampaignLevel_ == 2;
                continue;
            }
            if (source.type == 13 || source.type == 47) {
                // FUN_1000:0784 handles ground-riding Type 47 with the same
                // post-integration player-impact retirement as Type 13.
                sourceRemoval = sourceIndex;
                sourceRemovalPlayerDamage = 128;
                break;
            }
            if (hostileProjectile) {
                sourceRemoval = sourceIndex;
                sourceRemovalPlayerDamage = source.type == 19
                    ? 80
                    : source.type == 18
                        ? 64
                    : source.type == 20
                        ? 56
                        : source.type == 22 ? 104 : 40;
                break;
            }
            if(source.type==4) {
                //09B4 applies signed Type4 damage only on a current overlap.
                constexpr std::int16_t contactDamageUnits=-25;
                level2AudioEvents_.push_back({level2LoopTick_,6});
                level2PlayerArmor_=playableArmorAfterDamage(level2PlayerArmor_,contactDamageUnits,
                    level1_.systemTimers_[1]!=0 ? PlayableShield::active : PlayableShield::inactive);
            }
            if (
                source.stateBytes4dTo52[4] != 3
            ) {
                source.collisionContactAllocationIndex =
                    static_cast<std::size_t>(-1);
                source.collisionContactIsPlayer = true;
                // The player is a Type-0 target. After FUN_1000:0e50,
                // FUN_1000:0784 therefore calls FUN_1000:3d10 for every
                // non-Type-13/47 source. This includes both the state-2
                // guardian contact at loop 1559 and the state-1 contact at
                // loop 1698; limiting the backout by behavior state leaves an
                // extra forward step in the portable actor.
                if (source.type != 13 && source.type != 47) {
                    reverseEntityTranslation(source);
                }
                if (
                    sourceActorSlot < level2ActorPoolSize_
                    && source.stateBytes4dTo52[4] == 1
                ) {
                    //0E50 changes class1 behavior; it does not damage armor.
                    source.stateBytes4dTo52[5] = 4;
                    source.behaviorTimer = 0;
                } else if (source.stateBytes4dTo52[4] == 5) {
                    // FUN_1000:0e50 dispatches FUN_1000:adcc for presentation
                    // state 5. A player contact then calls FUN_1000:0e7c(-25),
                    // which removes 100 armor and emits the shared impact
                    // sound. Native Level 3 route v30 records 296->196 at
                    // loop 1698 and repeats the contact on 1699/1700.
                    level2PlayerArmor_ = static_cast<std::int16_t>(
                        level2PlayerArmor_ - 100
                    );
                    level2AudioEvents_.push_back({level2LoopTick_, 6});
                }
            }
            continue;
        }
        for (std::size_t targetPosition=0;targetPosition<collisionCount;++targetPosition) {
            const auto targetIndex=playableCampaignFrames_ ? campaignEntityIndex(collisionIdentities[targetPosition])
                : level2CollisionOrder[targetPosition];
            if(targetIndex>=level2Entities_.size()) {
                //A preceding FD may retire43, but0784 still sees its physical OLD pointer.
                if(playableCampaignFrames_ && collisionIdentities[targetPosition]/level2ActorPoolSize_==3) {
                    auto& target=level2FixedPool_[collisionIdentities[targetPosition]%level2ActorPoolSize_];
                    if(target.type==43 && campaignCollides(source,target)) {
                        target.collisionContactAllocationIndex=sourceIndex;
                        target.collisionContactIsPlayer=false;
                        source.collisionContactAllocationIndex=static_cast<std::size_t>(-1);
                        source.collisionContactIsPlayer=false;
                        sourceCollidedWithActor=true;
                        lastCollisionTargetType=43;
                        lastBombSlot=collisionIdentities[targetPosition]%level2ActorPoolSize_;
                    }
                }
                continue;
            }
            if (
                sourceIndex == targetIndex
                || !campaignActorExistedAtStart(targetIndex, start)
            ) {
                continue;
            }
            const auto targetProjectileSlot =
                level2EntityProjectilePoolSlots_[targetIndex];
            if (
                targetProjectileSlot < level2ProjectilePoolSize_
                && !start.projectiles[targetProjectileSlot]
            ) {
                continue;
            }
            auto& target = level2Entities_[targetIndex];
            const auto targetFlags = static_cast<std::int8_t>(
                target.stateBytes4dTo52[0]
            );
            if (targetFlags == -3 || targetFlags == -4) {
                continue;
            }
            auto collisionTarget = target;
            const auto targetActorSlot =
                level2EntityActorPoolSlots_[targetIndex];
            if (
                sourceActorSlot < level2ActorPoolSize_
                && targetActorSlot < level2ActorPoolSize_
                && motion.valid[targetActorSlot]
                && motion.order[sourceActorSlot]
                    < motion.order[targetActorSlot]
            ) {
                collisionTarget.position =
                    motion.positions[targetActorSlot];
            }
            if (!campaignCollides(source, collisionTarget)) {
                continue;
            }
            if (hostileProjectile) {
                if (
                    !source.ownerIsPlayer
                    && targetActorSlot < level2ActorPoolSize_
                    && targetActorSlot == source.ownerAllocationIndex
                ) {
                    continue;
                }
                // Hostile shots retire on the first non-owner actor contact.
                // The actor is not damaged; the same shared contact rule
                // covers indestructible Type 30 and moving enemies.
                sourceRemoval = sourceIndex;
                sourceRemovalPlayerDamage = 0;
                break;
            }
            level2PrefixGuards_.reset();
            target.collisionContactAllocationIndex = sourceIndex;
            target.collisionContactIsPlayer = false;
            //0784 leaves source+5F untouched for mode3; target still receives contact.
            if (!(playableLevel1 && source.stateBytes4dTo52[4] == skipSourceContactMode)) {
                source.collisionContactAllocationIndex = targetIndex;
                source.collisionContactIsPlayer = false;
            }
            sourceCollidedWithActor = true;
            lastCollisionTargetType = target.type;
            if(playableCampaignFrames_ && target.type==43) {lastBombSlot=level2EntityFixedPoolSlots_[targetIndex];}
            if (
                activeCampaignLevel_ == 1
                &&
                (source.type == 13 || source.type == 47)
                && target.type == 47
                && sourceActorSlot < level2ActorPoolSize_
            ) {
                // FUN_1000:0784 applies mutual Type-47 damage only while
                // DS:51f4 is campaign index 1 (Level 2). Level 3's
                // zero-radius guardian emissions may overlap exactly; they
                // retain the contact but do not destroy one another.
                sourceCollidedWithType47 = true;
            }
        }
        if (sourceRemoval < level2Entities_.size()) {
            break;
        }
        if (
            sourceCollidedWithActor
            && source.stateBytes4dTo52[4] != 3
        ) {
            // FUN_1000:0784 keeps the last contact returned by FUN_1000:0c0a.
            // After FUN_1000:0e50 consumes presentation-state 1/5, the shared
            // backout calls FUN_1000:3d10 when the last target is below Type
            // 46 or the source is below Type 44. Type 13/47 and a Type-43
            // target take their dedicated branches instead. This matters for
            // the Level 3 loop-1194 Type-44/Type-44 overlap: only the later
            // source actor loses its forward step.
            if (source.stateBytes4dTo52[4] == 1) {
                source.stateBytes4dTo52[5] = 4;
                source.behaviorTimer = 0;
            }
            if(playableCampaignFrames_ && lastCollisionTargetType==43) {
                const auto identity=campaignEntityIdentity(sourceIndex);
                auto shadow=source;
                const auto points=playableBombContactScore(shadow.type);
                if(activeCampaignLevel_==0 && shadow.stateBytes4dTo52[4]==5 && shadow.type!=7) {
                    if(lastBombSlot>=level2FixedObjectPoolSize_) {
                        throw std::logic_error("ADCC Bomb contact lost its physical FE owner");
                    }
                    const auto bombIdentity=level2ActorPoolSize_*3+lastBombSlot;
                    const auto bombIndex=campaignEntityIndex(bombIdentity);
                    auto bomb=bombIndex<level2Entities_.size() ? level2Entities_[bombIndex] : level2FixedPool_[lastBombSlot];
                    //0A0A/ADCC precedes source1176(10), even when A6BE's timer is nonzero.
                    shadow.collisionContactAllocationIndex=fixedContactBase+lastBombSlot;
                    level2Entities_[sourceIndex].collisionContactAllocationIndex=shadow.collisionContactAllocationIndex;
                    applyCampaignHit(bomb,20,bombIdentity, combat);
                    const auto liveBomb=campaignEntityIndex(bombIdentity);
                    if(liveBomb>=level2Entities_.size()) {level2FixedPool_[lastBombSlot].stateBytes4dTo52[1]=bomb.stateBytes4dTo52[1];}
                }
                //The target child may append effects or erase its compact FE view.
                //campaignHit resolves this source afresh through its physical identity.
                if(applyCampaignHit(shadow,10,identity, combat)) {level2Score_ += scaledCampaignAward(points);}
                if(phase_==GameplayPhase::endingEntry) {return CampaignContactFlow::suspendFrame;}
                //1176 can append effects/reward and erase compact entries; keep no vector reference.
                continue;
            }
            if (
                lastCollisionTargetType != 43
                && source.type != 13
                && source.type != 47
                && (
                    lastCollisionTargetType < 46
                    || source.type < 44
                )
            ) {
                reverseEntityTranslation(source);
            }
        }
        if (sourceCollidedWithType47) {
            level2Type47MutualHitActorSlots.push_back(sourceActorSlot);
        }
    }
    if (sourceRemoval < level2Entities_.size()) {
        level2PrefixGuards_.reset();
        //04D8 calls0E7C for hostile shots; positive shielded calls return
        //before its impact sound. Source-work receipts own their own audio.
        if(playableCampaignFrames_&&level2EntityProjectilePoolSlots_[sourceRemoval]<level2ProjectilePoolSize_
            &&level1_.systemTimers_[1]==0) {level2AudioEvents_.push_back({level2LoopTick_,6});}
        level2PlayerArmor_ = playableCampaignFrames_
            ? playableArmorAfterDamage(level2PlayerArmor_,static_cast<std::int16_t>(sourceRemovalPlayerDamage/4),
                level1_.systemTimers_[1]!=0 ? PlayableShield::active : PlayableShield::inactive)
            : static_cast<std::int16_t>(level2PlayerArmor_-sourceRemovalPlayerDamage);
        if (
            level2Entities_[sourceRemoval].type == 13
            || level2Entities_[sourceRemoval].type == 47
        ) {
            recordDestruction(level2Entities_[sourceRemoval],level2Entities_[sourceRemoval].position,6);
            // The native Level 2 OPL trace records two complete event-6
            // dispatcher blocks on this loop-170 Type 13 collision path.
            level2AudioEvents_.push_back({level2LoopTick_, 6});
            level2AudioEvents_.push_back({level2LoopTick_, 6});
            if (level2ActiveEnemyCount_ > 0) {
                --level2ActiveEnemyCount_;
            }
            if (activeCampaignLevel_ == 4) {
                level2Entities_[sourceRemoval].stateBytes4dTo52[1] = 0;
                //0A06 replaces the previous actor contact with the player pointer.
                level2Entities_[sourceRemoval].collisionContactAllocationIndex = static_cast<std::size_t>(-1);
                level2Entities_[sourceRemoval].collisionContactIsPlayer = true;
            }
        }
        combat.retire(sourceRemoval);
    }
    for (const auto actorSlot : level2Type47MutualHitActorSlots) {
        const auto actor = std::find(
            level2EntityActorPoolSlots_.begin(),
            level2EntityActorPoolSlots_.end(),
            actorSlot
        );
        if (actor == level2EntityActorPoolSlots_.end()) {
            continue;
        }
        const auto actorIndex = static_cast<std::size_t>(
            actor - level2EntityActorPoolSlots_.begin()
        );
        auto destroyed = level2Entities_[actorIndex];
        destroyed.stateBytes4dTo52[1] = 0;
        const auto distanceBucket = std::min<std::uint32_t>(
            destroyed.presenterDistance / 0x1964U,
            5U
        );
        combat.effect(
            destroyed,
            static_cast<std::uint8_t>(distanceBucket * 3U + 15U),
            static_cast<std::uint8_t>(distanceBucket * 3U + 17U)
        );
        level2AudioEvents_.push_back({level2LoopTick_, 6});
        if (
            destroyed.stateBytes4dTo52[0] == 0xff
            && level2ActiveEnemyCount_ > 0
        ) {
            --level2ActiveEnemyCount_;
        }
        combat.retire(actorIndex);
    }
    return CampaignContactFlow::continueFrame;
}

} // namespace stellar7
