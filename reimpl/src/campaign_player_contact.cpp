#include "campaign_contact.hpp"
#include "hazard_contact.hpp"
#include "playable_jump.hpp"
#include <algorithm>
#include <stdexcept>

namespace stellar7 {

GameplayModel::CampaignPlayerContactResult GameplayModel::resolveCampaignPlayerContact(
    const CampaignContactStart& start, const Level2Continuation continuation,
    Level2FrameRenderer* renderer, const CampaignCombatCallbacks& combat) {
    CampaignPlayerContactResult result;
    // FUN_1000:0784 integrates the player first. FUN_1000:0c0a then marks
    // every qualifying contact and returns the last one for the shared
    // translation-only backout. Transient effects and projectile actors are
    // not solid targets for this pass.
    auto playerCollisionIndex = static_cast<std::size_t>(-1);
    for (const auto index : level2ActiveListOrder_) {
        if (continuation == Level2Continuation::successReturn || index >= level2Entities_.size()) {
            continue;
        }
        if (!campaignActorExistedAtStart(index, start)) {
            continue;
        }
        auto& entity = level2Entities_[index];
        const auto flags = static_cast<std::int8_t>(
            entity.stateBytes4dTo52[0]
        );
        if (
            flags == -3
            || flags == -4
            || !campaignCollides(entity, level2Player_)
        ) {
            continue;
        }
        entity.collisionContactAllocationIndex =
            static_cast<std::size_t>(-1);
        entity.collisionContactIsPlayer = true;
        playerCollisionIndex = index;
    }
    if (playerCollisionIndex < level2Entities_.size()) {
        level2PrefixGuards_.reset();
        const auto contactIdentity=playableCampaignFrames_ ? campaignEntityIdentity(playerCollisionIndex) : 0;
        const auto contactRecord=[&]() -> Level1Entity& {
            if(!playableCampaignFrames_) {return level2Entities_[playerCollisionIndex];}
            const auto live=campaignEntityIndex(contactIdentity);
            if(live<level2Entities_.size()) {return level2Entities_[live];}
            if(contactIdentity<level2ActorPoolSize_) {return level2ActorPool_[contactIdentity];}
            return level2FixedPool_[contactIdentity-level2ActorPoolSize_*3];
        };
        const auto retireContact=[&]() {
            if(!playableCampaignFrames_) {combat.retire(playerCollisionIndex);return;}
            const auto live=campaignEntityIndex(contactIdentity);
            if(live<level2Entities_.size()) {combat.retire(live);}
        };
        if(playableCampaignFrames_ && level1_.systemTimers_[1]!=0) {
            auto shadow=contactRecord();
            const auto amount=static_cast<std::uint8_t>(level1_.systemTimers_[6]!=0 ? 15 : 5);
            if(applyCampaignHit(shadow,amount,contactIdentity, combat)) {
                level2AudioEvents_.push_back({level2LoopTick_,21});
                constexpr std::array<std::uint16_t,14> points{0,20,35,20,30,75,90,0,25,30,40,50,60,10};
                if(shadow.type>=0 && shadow.type<static_cast<std::int16_t>(points.size())) {
                    level2Score_ += scaledCampaignAward(points[shadow.type]);
                }
            }
            if(phase_==GameplayPhase::endingEntry) {result.flow = CampaignContactFlow::suspendFrame; return result;}
            playerCollisionIndex=campaignEntityIndex(contactIdentity);
        }
        const auto collisionType=contactRecord().type;
        if (collisionType == 25 || collisionType == 26) {
            if(playableCampaignFrames_ && level2PresentationAllocationRevision_!=level2NonProjectileAllocationRevision_) {
                //Shield impact4580 appends49F8;6F52 radar still walks inherited45D2.
                level2ActiveListOrder_=remapLevel2BoundaryOrder(start.identities);
                snapshotLevel2Presentation();
            }
            level2Complete_ = true;
            if (playableCampaignFrames_) {
                playableExit_=static_cast<CampaignExit>(collisionType);
                level2PickupIndex_=playerCollisionIndex;
            }
            level3CompletionByPickup_ = activeCampaignLevel_ == 2;
            if (playableCampaignFrames_ || activeCampaignLevel_ == 1 || activeCampaignLevel_ == 4) {
                if (activeCampaignLevel_ == 4 && collisionType == 25) {
                    level5SuccessContinuation_ = Level5SuccessContinuation{
                        static_cast<std::uint16_t>(level2LoopTick_ - 1),
                        level2EntityFixedPoolSlots_.at(playerCollisionIndex), level2AllocationRevision_};
                }
                if (playableCampaignFrames_ || (activeCampaignLevel_ == 1 && collisionType == 25)) {
                    std::vector<std::int16_t> types;
                    types.reserve(level2Entities_.size());
                    for (const auto& actor : level2Entities_) {
                        types.push_back(actor.type);
                    }
                    level2SuccessContinuation_ = Level2SuccessContinuation{
                        static_cast<std::uint16_t>(level2LoopTick_ - 1), playerCollisionIndex,
                        level2EntityFixedPoolSlots_.at(playerCollisionIndex), level2AllocationRevision_,
                        level2ActiveListOrder_, level2EntityFixedPoolSlots_, std::move(types)};
                }
                // Native 0784 calls the blocking 6f52 presenter here,
                // before the actor walk and ground update. Preserve that
                // entry state; do not simulate the remainder of this loop
                // while the success presentation is active.
                // Intact controlled Level 2 loop 2298 exercises this path.
                if (renderer != nullptr) {
                    advanceCockpitIndicator();
                }
                refreshLevel2SightPhase();
                result.flow = CampaignContactFlow::suspendFrame; return result;
            }
        } else if (playableCampaignFrames_ && (collisionType==24 || collisionType==FuelBayRecovery::stationType)) {
            //081A/0910 bypass solid rebound; the inner700-unit gate opens the bay.
            if (contactRecord().presenterDistance < FuelBayRecovery::entryRange) {
                for (const auto index : level2ActiveListOrder_) {
                    auto& station=level2Entities_[index];
                    if (station.type!=FuelBayRecovery::stationType) {continue;}
                    const auto slot=level2EntityFixedPoolSlots_[index];
                    if (slot>=level2FixedObjectPoolSize_) {throw std::logic_error("fuel station has no physical FE owner");}
                    station.stateBytes4dTo52[2]=1;level2FixedPool_[slot]=station;
                    fuelBay_.fixedSlot=slot;level2AudioEvents_.push_back({level2LoopTick_,FuelBayRecovery::entryEvent});
                }
            }
        } else if (playableCampaignFrames_ && collisionType==43) {
            contactRecord().collisionContactAllocationIndex=static_cast<std::size_t>(-1);
            contactRecord().collisionContactIsPlayer=false;
        } else if (collisionType == 48) {
            // FUN_1000:0784 handles the airborne guardian emission before
            // the shared solid-contact backout. It retires Type 0x30,
            // dispatches event 9, then FUN_1000:0e7c(5) dispatches event 6
            // and removes 5 * 4 armor. Player translation and momentum are
            // deliberately left unchanged.
            contactRecord()
                .stateBytes4dTo52[1] = 0;
            contactRecord().collisionContactIsPlayer = false;
            result.retiredType48ActorSlot =
                playableCampaignFrames_ ? contactIdentity : level2EntityActorPoolSlots_[playerCollisionIndex];
            level2AudioEvents_.push_back({level2LoopTick_, 9});
            level2AudioEvents_.push_back({level2LoopTick_, 6});
            level2PlayerArmor_ = playableCampaignFrames_
                ? playableArmorAfterDamage(level2PlayerArmor_,5,level1_.systemTimers_[1]!=0
                    ? PlayableShield::active : PlayableShield::inactive)
                : static_cast<std::int16_t>(level2PlayerArmor_-20);
        } else if (collisionType == 46) {
            // FUN_1000:0784 marks the guardian pulse's transient display
            // effect and returns without applying the shared solid-contact
            // translation backout. DS:51ec quarters both control velocities
            // on the following input update.
            level2Type46ContactSlowdown_ = true;
            if (playableCampaignFrames_ && activeCampaignLevel_ == 5) {
                --level2PlayerArmor_; //0890 decrements armor directly, without0E7C/audio.
            }
        } else if (collisionType == 42) {
            const auto payload = contactRecord().stateBytes4dTo52[5];
            if (playableCampaignFrames_) {
                constexpr std::uint8_t jumpPayload = 3;
                constexpr std::size_t jumpSystem = jumpPayload - 1;
                if (payload < 1 || payload > Level1State::systemCount) {
                    throw std::invalid_argument("invalid playable bonus payload");
                }
                validatePlayableJumpState(playableJumpState(),PlayableSystemDomain::fullBundle);
                if (payload == jumpPayload
                    && (level2Player_.position.z != 0 || level1_.systemTimers_[jumpSystem] != 0)) {
                    throw std::invalid_argument("playable bonus cannot repair Jump during flight");
                }
            }
            if (activeCampaignLevel_ == 4 && !playableCampaignFrames_
                && (payload < 1 || payload > Level1State::systemCount
                    || level1_.systemStatusLevels_[payload - 1] < 0
                    || level1_.selectedSystem_ != -1
                    || std::any_of(level1_.systemTimers_.begin(), level1_.systemTimers_.end(),
                        [](const auto timer) { return timer != 0; }))) {
                throw std::invalid_argument("Level5 bonus repair requires an untimed unselected system");
            }
            contactRecord().stateBytes4dTo52[1] = 0;
            if (activeCampaignLevel_ != 4) {
                contactRecord().collisionContactIsPlayer = false;
            }
            level2Score_ += scaledCampaignAward(100);
            level2AudioEvents_.push_back({level2LoopTick_, 24});
            if (playableCampaignFrames_ || activeCampaignLevel_ == 4) {
                level1_.repairBonusSystem(payload);
            }
            retireContact();
            level2PickupIndex_ = static_cast<std::size_t>(-1);
        } else {
            const auto contactPose=contactRecord();
            if (playableCampaignFrames_) {
                if(collisionType==4) {
                    //08BC retains the Type4 pointer even after shield retirement.
                    constexpr std::int16_t contactDamageUnits=-25;
                    level2AudioEvents_.push_back({level2LoopTick_,6});
                    level2PlayerArmor_=playableArmorAfterDamage(level2PlayerArmor_,contactDamageUnits,
                        level1_.systemTimers_[1]!=0 ? PlayableShield::active : PlayableShield::inactive);
                }
                retireContactHazard(contactRecord(),level2ActiveEnemyCount_,
                    [&]() {recordDestruction(contactPose,contactPose.position,6);retireContact();},
                    [&](const std::int16_t units) {
                        level2AudioEvents_.push_back({level2LoopTick_,6});
                        level2PlayerArmor_=playableArmorAfterDamage(level2PlayerArmor_,units,
                            level1_.systemTimers_[1]!=0 ? PlayableShield::active : PlayableShield::inactive);
                    });
            }
            if (level2Player_.forwardVelocity == 0) {
                level2Player_.forwardVelocity = 10;
            }
            do {
                reverseEntityTranslation(level2Player_);
            } while (
                campaignCollides(
                    contactPose,
                    level2Player_
                )
            );
            level2Player_.forwardVelocity = static_cast<std::int16_t>(
                -level2Player_.forwardVelocity
            );
            level2PlayerCollisionMomentum_ =
                level2Player_.forwardVelocity;
            level2AudioEvents_.push_back({level2LoopTick_, 9});
            if (collisionType == 13) {
                level2PlayerArmor_ = playableCampaignFrames_
                    ? playableArmorAfterDamage(level2PlayerArmor_,32,level1_.systemTimers_[1]!=0
                        ? PlayableShield::active : PlayableShield::inactive)
                    : static_cast<std::int16_t>(level2PlayerArmor_-128);
                level2AudioEvents_.push_back({level2LoopTick_, 6});
                level2AudioEvents_.push_back({level2LoopTick_, 6});
                if (level2ActiveEnemyCount_ > 0) {
                    --level2ActiveEnemyCount_;
                }
                if (activeCampaignLevel_ == 4) {
                    contactRecord().stateBytes4dTo52[1] = 0;
                    contactRecord().collisionContactIsPlayer = true;
                }
                retireContact();
            }
        }
    }
    return result;
}

} // namespace stellar7
