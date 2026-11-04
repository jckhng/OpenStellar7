#include "stellar7/gameplay.hpp"
#include "stellar7/level2_projectile_prefix_work.hpp"
#include "stellar7/level1_arrival_work.hpp"
#include "stellar7/audio_direct_work.hpp"
#include "stellar7/campaign_loader.hpp"
#include "stellar7/weapon_lamp_work.hpp"

#include <algorithm>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::uint8_t supportedCampaignDifficulty=0;
}

void GameplayModel::commitRecoveredLevel2WeaponLamp(const WeaponLampWork& work) {
    if(phase_!=GameplayPhase::level2 || activeCampaignLevel_!=1 || !level2PrefixGuards_
        || !level2HasAdvanced_ || level2TailConsumed_ || work.dirty>1) {
        throw std::invalid_argument("Level2 lamp commit requires its source7BAE return");
    }
    weaponIndicatorVisible_=true;weaponReadyIndicator_=work.dirty==1;
}

void GameplayModel::commitRecoveredLevel2CombatTail(const FrameTailState& tail) {
    commitLevel2CombatTail(tail,Level1PlayerCombatDomain::survivingOnly);
}

void GameplayModel::commitRecoveredLevel2LifecycleTail(const Level2LifecycleTailWork& tail) {
    validateLevel2LifecycleTail(tail);
    commitLevel2CombatTail(tail.work.state,Level1PlayerCombatDomain::expiryAndLoss);
    finishLevel2LifecycleTail(tail);
}

void GameplayModel::validateLevel2LifecycleTail(const Level2LifecycleTailWork& tail,const Level1ArrivalWork* arrival) const {
    const auto failed=level2PlayerArmor_<0;
    const auto warning=level2PlayerArmor_<200 && (level2LoopTick_&63U)==0;
    const auto born=arrival && arrival->birth.has_value();
    const auto warningIndex=born ? 1U : 0U;
    if(tail.failureRequested!=failed || tail.missionActive==failed
        || tail.lowArmorWarningRequested!=warning || tail.audioEvents.size()!=warningIndex+(warning ? 1U : 0U)
        || (born && (tail.audioEvents.front().loopTick!=level2LoopTick_ || tail.audioEvents.front().eventId!=gameplayArrivalEvent))
        || (warning && (tail.audioEvents[warningIndex].loopTick!=level2LoopTick_ || tail.audioEvents[warningIndex].eventId!=3))) {
        throw std::invalid_argument("Level2 lifecycle tail changed its source warning/death writes");
    }
}

void GameplayModel::finishLevel2LifecycleTail(const Level2LifecycleTailWork& tail) {
    level2AudioEvents_.insert(level2AudioEvents_.end(),tail.audioEvents.begin(),tail.audioEvents.end());
    if(tail.lowArmorWarningRequested) {level2ArmorWarningActive_=true;}
    if(tail.failureRequested) {
        failureFromLevel2_=true;level2FailureTailFinished_=true;
        failureSourceLoopTick_=level2LoopTick_;
        phase_=GameplayPhase::failurePreExplosionBlackout;presentationFrame_=0;
    }
}

void GameplayModel::validateLevel2CombatTail(const FrameTailState& tail,const Level1PlayerCombatDomain domain) const {
    const auto minimumArmor=domain==Level1PlayerCombatDomain::expiryAndLoss ? -level2MaximumFrameDamage : 200;
    if(phase_!=GameplayPhase::level2 || activeCampaignLevel_!=1 || !level2PrefixGuards_
        || !level2HasAdvanced_ || level2TailConsumed_ || level2PlayerArmor_<minimumArmor || level2PlayerArmor_>680
        || level2Complete_ || tail.loopTick!=static_cast<std::uint16_t>(level2LoopTick_+1)
        || tail.actors.size()!=level2ActiveListOrder_.size()) {
        throw std::invalid_argument("Level2 combat tail changed its source loop/OLD domain");
    }
    for(std::size_t position=0;position<tail.actors.size();++position) {
        if(tail.actors[position].type!=level2Entities_.at(level2ActiveListOrder_[position]).type) {
            throw std::invalid_argument("Level2 combat tail changed an OLD physical actor");
        }
    }
    recoveredLevel2RenderKeys();
}

void GameplayModel::commitLevel2CombatTail(const FrameTailState& tail,const Level1PlayerCombatDomain domain) {
    validateLevel2CombatTail(tail,domain);
    const auto periodic=(level2LoopTick_&15U)==0;
    const auto scheduler=periodic?FrameTailScheduler::zeroThresholdDraw:FrameTailScheduler::none;
    const auto table=campaignLevelDefinition(1,supportedCampaignDifficulty).table;
    if(tail.scheduler!=scheduler || (periodic && (level2ActiveEnemyCount_<0 || level2ActiveEnemyCount_>5
        || level2SpawnedEnemyCount_!=5 || table.enemyLimit!=12
        || campaignSpawnThreshold(1,static_cast<std::uint8_t>(level2ActiveEnemyCount_))!=0))) {
        throw std::invalid_argument("Level2 combat tail reached an unsupported positive spawn-threshold branch");
    }
    if(periodic) {level2Rng_.next();}
    applyLevel2CombatTailState(tail);
    level2LoopTick_=tail.loopTick;level2TailConsumed_=true;
}

void GameplayModel::applyLevel2CombatTailState(const FrameTailState& tail) {
    level2Player_.position=tail.playerPosition;level2Player_.yawVelocity=tail.yawVelocity;
    level2Player_.forwardVelocity=tail.forwardVelocity;level2Player_.weaponCooldown=tail.weaponTimers[0];
    level2Player_.weaponSlotTimers[0]=tail.weaponTimers[1];level2Player_.weaponSlotTimers[1]=tail.weaponTimers[2];
    for(std::size_t position=0;position<tail.actors.size();++position) {
        level2Entities_[level2ActiveListOrder_[position]].position=tail.actors[position].position;
    }
    for(std::size_t index=0;index<level2Entities_.size();++index) {
        const auto& actor=level2Entities_[index];
        const auto ordinary=level2EntityActorPoolSlots_[index],fixed=level2EntityFixedPoolSlots_[index];
        const auto shot=level2EntityProjectilePoolSlots_[index],effect=level2EntityDeathEffectPoolSlots_[index];
        if(ordinary<level2ActorPoolSize_) {level2ActorPool_[ordinary]=actor;}
        else if(fixed<level2FixedObjectPoolSize_) {level2FixedPool_[fixed]=actor;}
        else if(shot<level2ProjectilePoolSize_) {level2ProjectilePool_[shot]=actor;}
        else if(effect<level2DeathEffectPoolSize_) {level2DeathEffectPool_[effect]=actor;}
    }
}
} // namespace stellar7
