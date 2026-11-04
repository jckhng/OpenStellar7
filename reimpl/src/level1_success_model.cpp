#include "stellar7/gameplay.hpp"
#include "stellar7/success_session.hpp"
#include "stellar7/level1_deployment_render_work.hpp"

#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::uint16_t successResume = 0x0809;
constexpr std::size_t ordinaryPoolCapacity = 16;
constexpr std::size_t effectAllocationBase = 32;
constexpr std::int16_t rewardType = 25;
constexpr std::uint32_t rewardRadius = 897;
constexpr std::uint8_t fixedCategory = 254;
constexpr std::uint8_t maximumRewardHealth = 100;
constexpr std::size_t oldListCapacity = 32;
constexpr std::int16_t liftType = 11;
constexpr std::int16_t guardianType = 44;
constexpr std::uint16_t animationCallback = 0x130c;
constexpr std::uint16_t beamCallback = 0xb0b4;
constexpr std::uint16_t effectCallback = 0x0ac8;
constexpr Viewport mainViewport{24,294,24,145};
constexpr std::uint8_t sightColor = 95;

WorldActorRenderProfile successRadarProfile(const Level1Entity& actor) {
    const auto legacyNoHandles = (actor.type == 1 && actor.stateBytes4dTo52[0] == 255)
        || (actor.type == 29 && actor.stateBytes4dTo52[0] == fixedCategory);
    if (actor.renderProfile == WorldActorRenderProfile::unknown && legacyNoHandles) {
        //The ordinary Level1 frame projects these constructor records identically.
        return WorldActorRenderProfile::noHandles;
    }
    return actor.renderProfile;
}

std::uint16_t callback(const Level1Entity& actor, const WorldActorRenderProfile profile) {
    switch (profile) {
    case WorldActorRenderProfile::heapSelectorDeferredAnimation:
        if (!actor.animatedModelPresenterEnabled) {
            throw std::invalid_argument("success radar has inconsistent heap callback ownership");
        }
        return animationCallback;
    case WorldActorRenderProfile::heapSelectorNoCallback:
        if (actor.animatedModelPresenterEnabled) {
            throw std::invalid_argument("success radar has inconsistent cleared heap callback");
        }
        return 0;
    case WorldActorRenderProfile::inlineSelectorBeamCallback: return beamCallback;
    case WorldActorRenderProfile::noHandles:
    case WorldActorRenderProfile::inlineSelectorNoCallback:
    case WorldActorRenderProfile::inlineSelectorDeparture: return 0;
    default: throw std::invalid_argument("success radar requires qualified source callback ownership");
    }
}
}

std::optional<Level1RewardSuccessChild> GameplayModel::recoveredSuccessContact() const {
    if (!recoveredSuccessPause_ || recoveredSuccessReturned_ || phase_!=GameplayPhase::level1) {
        return std::nullopt;
    }
    const auto& pause=*recoveredSuccessPause_;
    if (level1_.levelIndex_!=0 || !level1_.missionActive_
        || level1_.failureState_ || level1_.successState_ || level1_.exitState_
        || pause.sourceResume!=successResume || pause.oldListCursor!=0
        || pause.incomingLoopTick!=level1_.loopTick_
        || pause.selectedEntityIndex!=pause.rewardEntityIndex || pause.rewardEntityIndex>=level1_.entities_.size()
        || level2PickupIndex_!=pause.rewardEntityIndex) {
        throw std::invalid_argument("recovered success requires the retained0802 contact pause");
    }
    const auto& selected=level1_.entities_[pause.selectedEntityIndex];
    if (!selected.collisionContactIsPlayer || selected.collisionContactAllocationIndex!=static_cast<std::size_t>(-1)
        || selected.allocationIndex!=pause.selectedEntityIndex || selected.type!=rewardType
        || selected.flags!=0 || selected.collisionRadius!=rewardRadius || selected.stateBytes4dTo52[0]!=fixedCategory
        || selected.stateBytes4dTo52[1]==0 || selected.stateBytes4dTo52[1]>maximumRewardHealth || selected.stateBytes4dTo52[4]!=0
        || selected.renderProfile!=WorldActorRenderProfile::inlineSelectorNoCallback) {
        throw std::invalid_argument("recovered success pause lost its actual Type25 reward");
    }
    return level1RewardContactOperations(selected,0).success;
}

SuccessDisplayWork GameplayModel::beginRecoveredSuccess(const GameplayAssets& assets,
    const CockpitPageState& pages,const RadarState& radar,const CockpitWorkBitmaps& bitmaps) {
    if (!recoveredSuccessContact() || !assets.cockpitSettings || assets.models.size()<=guardianType
        || level1_.objectListOrder_.size()>oldListCapacity) {
        throw std::invalid_argument("recovered success requires its paused source display owners");
    }
    std::vector<RadarStateActor> actors;
    std::vector<std::optional<Level1AnimationState>> animations;
    SightRefreshInput refresh{{},level1_.player_.yaw};
    actors.reserve(level1_.objectListOrder_.size());
    animations.reserve(level1_.objectListOrder_.size());
    for (const auto object:level1_.objectListOrder_) {
        if (object.kind==Level1State::ObjectKind::projectile) {
            const auto& shot=level1_.projectiles_.at(object.index);
            actors.push_back({ordinaryPoolCapacity+object.index,{shot.flags,shot.position},shot.collisionRadius,
                shot.type,static_cast<std::int8_t>(shot.stateBytes4dTo54[0]),0,
                WorldActorRenderProfile::inlineSelectorNoCallback});
            animations.push_back(std::nullopt);
            refresh.targets.push_back({shot.type,shot.targetHeading});
            continue;
        }
        const auto effect=object.kind==Level1State::ObjectKind::deathEffect;
        if (!effect && object.kind!=Level1State::ObjectKind::entity) {
            throw std::invalid_argument("success radar contains an unknown OLD list identity");
        }
        const auto& actor=effect ? level1_.deathEffects_.at(object.index).entity : level1_.entities_.at(object.index);
        if (!effect && (object.index>=ordinaryPoolCapacity || actor.allocationIndex!=object.index)) {
            throw std::invalid_argument("success radar lost physical ordinary allocation identity");
        }
        const auto profile=effect ? WorldActorRenderProfile::bitmapEffectCallback : successRadarProfile(actor);
        const auto drawCallback=effect ? static_cast<std::uint16_t>(actor.stateBytes4dTo52[4]!=0 ? effectCallback : 0)
            : callback(actor,profile);
        actors.push_back({effect ? effectAllocationBase+object.index : object.index,{actor.flags,actor.position},
            actor.collisionRadius,actor.type,static_cast<std::int8_t>(actor.stateBytes4dTo52[0]),drawCallback,profile});
        if (!effect && (actor.type==liftType || actor.type==guardianType)) {
            animations.push_back(Level1AnimationState{actor.stateBytes4dTo52[2],actor.stateBytes4dTo52[3],actor.modelAnimationVariant});
        } else {
            animations.push_back(std::nullopt);
        }
        refresh.targets.push_back({actor.type,actor.targetHeading});
    }
    //6FBB draws matching CACHE;7043/7046 consume the unchanged OLD list on BACK.
    auto cockpit=cockpitOperations({cockpitIndicatorX_,cockpitIndicatorVelocity_,assets.cockpitSettings->viewSize,
        CockpitPageRelation::matching},bitmaps);
    auto sight=sightOperations(level1_.loopTick_,alternateSightPhase_,mainViewport,sightColor,refresh);
    auto retainedRadar=radar;
    const RadarPages radarPages{pages.cachePage(),pages.backPage(),pages.frontPage(),pages.backPage(),pages.cachePage()};
    auto drawn=retainedRadar.drawLevel1DeploymentCombat(level1_.playerArmor_,level1_.player_.position,
        level1_.player_.yaw,actors,radarPages,{},assets.models[liftType],assets.models[guardianType],std::move(animations));
    if (!drawn.radar.cpuWorkQualified || drawn.radar.actors.size()!=actors.size()
        || drawn.animations.size()!=actors.size()) {
        throw std::invalid_argument("success requires a qualified complete radar observation");
    }
    for (std::size_t ordinal=0;ordinal<level1_.objectListOrder_.size();++ordinal) {
        const auto object=level1_.objectListOrder_[ordinal];
        const auto& observation=drawn.radar.actors[ordinal];
        if (object.kind==Level1State::ObjectKind::projectile) {
            level1_.projectiles_[object.index].flags=observation.flags;
            continue;
        }
        if (object.kind==Level1State::ObjectKind::deathEffect) {
            auto& effect=level1_.deathEffects_[object.index];
            effect.entity.flags=observation.flags;
            effect.projected={observation.projectedX,observation.projectedY};
            continue;
        }
        auto& actor=level1_.entities_[object.index];
        actor.flags=observation.flags;
        if (drawn.animations[ordinal]) {
            const auto& animation=*drawn.animations[ordinal];
            actor.stateBytes4dTo52[2]=animation.reload;
            actor.stateBytes4dTo52[3]=animation.countdown;
            actor.modelAnimationVariant=animation.variant;
            if (actor.type==liftType && actor.primaryType11AnimationSelector) {
                level1_.type11AnimationVariant_=animation.variant;
            }
        }
    }
    cockpitIndicatorX_=cockpit.state.indicatorX;
    cockpitIndicatorVelocity_=cockpit.state.indicatorVelocity;
    alternateSightPhase_=sight.alternatePhase;
    level2ArmorWarningActive_=false; //6F5B clears the existing129C owner.
    phase_=GameplayPhase::successPresentation;
    return {std::move(cockpit),std::move(sight),std::move(drawn.radar),std::move(retainedRadar)};
}

void GameplayModel::finishRecoveredSuccess() {
    if (phase_!=GameplayPhase::successPresentation || !recoveredSuccessPause_ || recoveredSuccessReturned_
        || recoveredSuccessPause_->sourceResume!=successResume
        || recoveredSuccessPause_->incomingLoopTick!=level1_.loopTick_) {
        throw std::invalid_argument("recovered success return requires its suspended source parent");
    }
    recoveredSuccessReturned_=true; //720E returned; root resumes0809 from current state.
}
} // namespace stellar7
