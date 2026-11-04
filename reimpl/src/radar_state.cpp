#include "stellar7/radar_state.hpp"
#include "stellar7/playable_system_effects.hpp"
#include "stellar7/failure_effect_work.hpp"

#include <algorithm>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr RadarMarkerColors markerColors{228, 138};
constexpr std::uint16_t panelWidth = 120;
constexpr std::uint16_t panelHeight = 56;
constexpr std::int16_t completionCraftType = 7;
constexpr std::int16_t guardianFixedType = 46;
constexpr std::int16_t chasingActorType = 47;
constexpr std::int16_t descendingActorType = 48;
constexpr std::int32_t maximumEffectAltitude = 500;
constexpr std::int32_t effectAltitudeStep = 50;
constexpr std::int16_t completionPickupType = 25;
constexpr std::int16_t systemBonusType = 42;
}

RadarState::RadarState(const std::vector<IndexedBitmap>& panelGroup)
    : cache_(cacheBitmapGroup(panelGroup, 0)),
      view_{{0, 0, 200000}, {0xc000, 0, 0}, {}, 0x88, 7,
          {112, 205, 146, 180}, {256, -256, 362}} {
    const auto& panel = cache_.frames.front().header;
    if (panel.segment != 0xac00 || panel.offset != 0
        || panel.width != panelWidth || panel.height != panelHeight) {
        throw std::invalid_argument("unsupported source radar panel cache");
    }
}

void RadarState::resetArmorStrip() noexcept {
    armorStrip_ = 0;
    initialized_ = false;
}

void RadarState::initializeAllocation(const std::size_t allocation) {
    if (allocation >= projections_.size()) {
        throw std::invalid_argument("unsupported radar allocation identity");
    }
    projections_[allocation] = RadarActorOutput{};
}

const RadarActorOutput& RadarState::projection(const std::size_t allocation) const {
    if (allocation >= projections_.size() || !projections_[allocation]) {
        throw std::invalid_argument("unknown radar allocation identity");
    }
    return *projections_[allocation];
}

RadarActorOutput RadarState::projectBitmapEffect(const std::size_t allocation,
    const WorldActorWorkInput& actor, const ViewTransform& view, const RadarPixelDomain domain) {
    constexpr std::int32_t nearDepth = 128;
    constexpr std::int32_t projectionScale = 128;
    if ((domain != RadarPixelDomain::retained && domain != RadarPixelDomain::playableCampaign)
        || (actor.flags != 0x88 && actor.flags != 0x8c)
        || (domain == RadarPixelDomain::retained && (actor.position.z < 0
            || actor.position.z > maximumEffectAltitude || actor.position.z % effectAltitudeStep != 0))
        || (domain == RadarPixelDomain::retained && view.origin.z != 0)
        || (domain == RadarPixelDomain::playableCampaign
            && (view.origin.z < 0 || view.origin.z > maximumPlayableJumpAltitude))
        || view.perspectiveScale != projectionScale || view.viewport.left != 24 || view.viewport.right != 294
        || view.viewport.top != 24 || view.viewport.bottom != 145) {
        throw std::invalid_argument("unsupported bitmap-effect main projection");
    }
    auto output = projection(allocation);
    output.center = output.cachedCenter = transformToView(actor.position, view);
    output.flags = static_cast<std::uint8_t>(actor.flags & ~4U);
    output.projected = false;
    if (output.center.y >= nearDepth) {
        const auto divide = [projectionScale](const std::int32_t coordinate, std::uint32_t denominator) {
            const auto numerator = static_cast<std::int64_t>(coordinate) * projectionScale;
            auto magnitude = static_cast<std::uint64_t>(numerator < 0 ? -numerator : numerator);
            while (denominator > 0xffffU) {
                denominator >>= 1U;
                magnitude >>= 1U;
            }
            const auto quotient = static_cast<std::int32_t>(magnitude / denominator);
            return numerator < 0 ? -quotient : quotient;
        };
        const auto x = view.viewport.left + (view.viewport.right - view.viewport.left) / 2
            + divide(output.center.x, output.center.y);
        const auto y = view.viewport.top + (view.viewport.bottom - view.viewport.top) / 2
            - divide(output.center.z, output.center.y);
        if (x >= view.viewport.left && x <= view.viewport.right && y >= view.viewport.top && y <= view.viewport.bottom) {
            output.projectedX = static_cast<std::int16_t>(x);
            output.projectedY = static_cast<std::int16_t>(y);
            output.projectedRadius = 0;
            output.flags |= 4;
            output.projected = true;
        }
    }
    // B996 and698a share these actor fields; the callback frame was already advanced by the model.
    projections_[allocation] = output;
    return output;
}

RadarWork RadarState::initialize(const std::int16_t armor, const FixedPosition playerPosition,
    const std::uint16_t playerYaw, const RadarPages& pages, const RadarPageLayout layout) {
    if (initialized_ || pages.draw != pages.back || pages.copy != pages.cached) {
        throw std::invalid_argument("unsupported radar startup order");
    }
    //50c4 runs698a after the second cockpit draw, with the reset empty list.
    auto work = layout==RadarPageLayout::compatibility
        ? radarOperations({armor, armorStrip_, pages}, view_, playerPosition,
            playerYaw, {}, {0, 0}, cache_.frames.front().bitmap, markerColors)
        : radarPresentationPixels({armor, armorStrip_, pages}, view_, playerPosition,
            playerYaw, {}, {0, 0}, cache_.frames.front().bitmap, markerColors,layout);
    armorStrip_ = *work.state.armorStrip;
    view_.origin.x = playerPosition.x;
    view_.origin.y = playerPosition.y;
    view_.actorAngles[2] = playerYaw;
    initialized_ = true;
    return work;
}

RadarWork RadarState::initializeLevel1(const std::int16_t armor, const FixedPosition playerPosition,
    const std::uint16_t playerYaw, const RadarPages& pages) {
    if (initialized_ || pages.draw != pages.back || pages.copy != pages.cached) {
        throw std::invalid_argument("unsupported Level1 radar startup order");
    }
    auto work = level1RadarInitializationOperations({armor, armorStrip_, pages}, view_, playerPosition,
        playerYaw, cache_.frames.front().bitmap, markerColors);
    armorStrip_ = *work.state.armorStrip;
    view_.origin.x = playerPosition.x;
    view_.origin.y = playerPosition.y;
    view_.actorAngles[2] = playerYaw;
    initialized_ = true;
    return work;
}

Level1RadarWork RadarState::drawLevel1(const std::int16_t armor, const FixedPosition playerPosition,
    const std::uint16_t playerYaw, const std::vector<RadarStateActor>& actors,
    const RadarPages& pages, const RadarTraversalProfile profile,
    const Model3d& type11Model, const Level1AnimationState animation) {
    return drawLevel1List(armor, playerPosition, playerYaw, actors, pages, profile, type11Model,
        animation, Level1List::entities);
}

Level1RadarWork RadarState::drawLevel1Projectile(const std::int16_t armor, const FixedPosition playerPosition,
    const std::uint16_t playerYaw, const std::vector<RadarStateActor>& actors,
    const RadarPages& pages, const RadarTraversalProfile profile,
    const Model3d& type11Model, const Level1AnimationState animation) {
    return drawLevel1List(armor, playerPosition, playerYaw, actors, pages, profile, type11Model,
        animation, Level1List::primaryProjectile);
}

Level1RadarWork RadarState::drawLevel1Combat(const std::int16_t armor, const FixedPosition playerPosition,
    const std::uint16_t playerYaw, const std::vector<RadarStateActor>& actors,
    const RadarPages& pages, const RadarTraversalProfile profile,
    const Model3d& type11Model, const Level1AnimationState animation) {
    return drawLevel1List(armor, playerPosition, playerYaw, actors, pages, profile, type11Model,
        animation, Level1List::combatProjectiles);
}

Level1RadarWork RadarState::drawLevel1List(const std::int16_t armor, const FixedPosition playerPosition,
    const std::uint16_t playerYaw, const std::vector<RadarStateActor>& actors,
    const RadarPages& pages, const RadarTraversalProfile profile,
    const Model3d& type11Model, const Level1AnimationState animation, const Level1List list) {
    const auto inputs = level1Inputs(actors);
    auto work = list == Level1List::entities
        ? level1RadarOperations({armor, armorStrip_, pages}, view_, playerPosition,
            playerYaw, inputs, profile, cache_.frames.front().bitmap, markerColors, type11Model, animation)
        : list == Level1List::combatProjectiles
        ? level1CombatRadarOperations({armor, armorStrip_, pages}, view_, playerPosition,
            playerYaw, inputs, profile, cache_.frames.front().bitmap, markerColors, type11Model, animation)
        : level1ProjectileRadarOperations({armor, armorStrip_, pages}, view_, playerPosition,
            playerYaw, inputs, profile, cache_.frames.front().bitmap, markerColors, type11Model, animation);
    retainLevel1Draw(work.radar, actors, playerPosition, playerYaw);
    return work;
}

std::vector<RadarActorInput> RadarState::level1Inputs(const std::vector<RadarStateActor>& actors) const {
    if (!initialized_) {
        throw std::invalid_argument("Level1 radar requires source startup draw");
    }
    std::array<bool, allocationCapacity> selected{};
    std::vector<RadarActorInput> inputs;
    inputs.reserve(actors.size());
    for (const auto& actor : actors) {
        const auto& previous = projection(actor.allocation);
        if (selected[actor.allocation]) {
            throw std::invalid_argument("duplicate Level1 radar actor identity");
        }
        selected[actor.allocation] = true;
        inputs.push_back({actor.actor, actor.radius, actor.type, actor.category,
            actor.callback, actor.renderProfile, previous.projectedX, previous.projectedY,
            previous.projectedRadius});
    }
    return inputs;
}

void RadarState::retainLevel1Draw(const RadarWork& work, const std::vector<RadarStateActor>& actors,
    const FixedPosition playerPosition, const std::uint16_t playerYaw) {
    for (std::size_t index = 0; index < actors.size(); ++index) {
        projections_[actors[index].allocation] = work.actors[index];
    }
    armorStrip_ = *work.state.armorStrip;
    view_.origin.x = playerPosition.x;
    view_.origin.y = playerPosition.y;
    view_.actorAngles[2] = playerYaw;
}

Level1DynamicRadarWork RadarState::drawLevel1DynamicCombat(const std::int16_t armor,
    const FixedPosition playerPosition, const std::uint16_t playerYaw,
    const std::vector<RadarStateActor>& actors, const RadarPages& pages,
    const RadarTraversalProfile profile, const Model3d& type11Model,
    std::vector<std::optional<Level1AnimationState>> animations) {
    auto work = level1DynamicCombatRadarOperations({armor, armorStrip_, pages}, view_, playerPosition,
        playerYaw, level1Inputs(actors), profile, cache_.frames.front().bitmap, markerColors,
        type11Model, std::move(animations));
    retainLevel1Draw(work.radar, actors, playerPosition, playerYaw);
    return work;
}

void RadarState::retainLevel1CombatEffectProjection(const std::size_t allocation,
    const FailureEffectWork& work) {
    retainLevel1CombatEffectProjection(allocation,work,Level1EffectProjectionDomain::ordinaryCombat);
}

void RadarState::retainLevel1CombatEffectProjection(const std::size_t allocation,
    const FailureEffectWork& work, const Level1EffectProjectionDomain domain) {
    const std::size_t firstEffectAllocation = domain==Level1EffectProjectionDomain::level2Combat ? 40 : 32;
    constexpr Viewport mainViewport{24,294,24,145};
    const auto& state = work.state;
    const auto nonlethal = state.endAge == 12 && state.age >= 2 && state.age < state.endAge;
    const auto guardian = domain==Level1EffectProjectionDomain::guardianCombat
        || domain==Level1EffectProjectionDomain::level2Combat;
    const auto lastLethalEnd = guardian ? 32 : 29;
    const auto remainingFrames = guardian ? 3 : 2;
    const auto lethal = state.endAge >= 17 && state.endAge <= lastLethalEnd && (state.endAge-17) % 3 == 0
        && state.age >= state.endAge-remainingFrames && state.age < state.endAge;
    const auto terminal = state.callback == 0 && state.age == 0 && state.endAge == 0;
    if ((domain!=Level1EffectProjectionDomain::ordinaryCombat && !guardian)
        || allocation < firstEffectAllocation || allocation >= firstEffectAllocation+Level1State::deathEffectPoolSize
        || (state.flags != 0x88 && state.flags != 0x8c) || state.projectedRadius != 0
        || work.projected != ((state.flags & 4U) != 0)
        || (work.retired ? !terminal : state.callback != 0x0ac8 || (!nonlethal && !lethal))
        || (!work.retired && work.removal)
        || (work.removal && (work.removal->removedIndex > work.removal->lastIndex || work.removal->lastIndex >= 64))
        || work.cachedCenter.y < -32768 || work.cachedCenter.y > 32767
        || work.cachedCenter.x < -65535 || work.cachedCenter.x > 65535
        || work.cachedCenter.z < -65535 || work.cachedCenter.z > 65535
        || (work.projected && (state.projected.x < mainViewport.left || state.projected.x > mainViewport.right
            || state.projected.y < mainViewport.top || state.projected.y > mainViewport.bottom))) {
        throw std::invalid_argument("unsupported ordinary effect projection receipt");
    }
    projections_[allocation] = RadarActorOutput{work.cachedCenter,work.cachedCenter,state.flags,
        state.projected.x,state.projected.y,state.projectedRadius,work.projected};
}

Level1DynamicRadarWork RadarState::drawLevel1Type3Combat(const std::int16_t armor,
    const FixedPosition playerPosition,const std::uint16_t playerYaw,
    const std::vector<RadarStateActor>& actors,const RadarPages& pages,
    const RadarTraversalProfile profile,const Model3d& type11Model,const Model3d* guardianModel,
    std::vector<std::optional<Level1AnimationState>> animations,const Level1OrdinaryRenderFamily family) {
    auto work=level1Type3RadarOperations({armor,armorStrip_,pages},view_,playerPosition,playerYaw,
        level1Inputs(actors),profile,cache_.frames.front().bitmap,markerColors,type11Model,
        guardianModel,std::move(animations),family);
    retainLevel1Draw(work.radar,actors,playerPosition,playerYaw);
    return work;
}

Level1DynamicRadarWork RadarState::drawLevel1CombatEffects(const std::int16_t armor,
    const FixedPosition playerPosition, const std::uint16_t playerYaw,
    const std::vector<RadarStateActor>& actors, const RadarPages& pages,
    const RadarTraversalProfile profile, const Model3d& type11Model,
    std::vector<std::optional<Level1AnimationState>> animations) {
    auto work = level1CombatEffectRadarOperations({armor,armorStrip_,pages}, view_, playerPosition,
        playerYaw, level1Inputs(actors), profile, cache_.frames.front().bitmap, markerColors,
        type11Model, std::move(animations));
    retainLevel1Draw(work.radar,actors,playerPosition,playerYaw);
    return work;
}

Level1DynamicRadarWork RadarState::drawLevel1BonusCombat(const std::int16_t armor,
    const FixedPosition playerPosition, const std::uint16_t playerYaw,
    const std::vector<RadarStateActor>& actors, const RadarPages& pages,
    const RadarTraversalProfile profile, const Model3d& type11Model,
    std::vector<std::optional<Level1AnimationState>> animations) {
    auto work = level1BonusCombatRadarOperations({armor,armorStrip_,pages}, view_, playerPosition,
        playerYaw, level1Inputs(actors), profile, cache_.frames.front().bitmap, markerColors,
        type11Model, std::move(animations));
    retainLevel1Draw(work.radar,actors,playerPosition,playerYaw);
    return work;
}

Level1DynamicRadarWork RadarState::drawLevel1ArrivalCombat(const std::int16_t armor,
    const FixedPosition playerPosition, const std::uint16_t playerYaw,
    const std::vector<RadarStateActor>& actors, const RadarPages& pages,
    const RadarTraversalProfile profile, const Model3d& type11Model,
    std::vector<std::optional<Level1AnimationState>> animations) {
    auto work = level1ArrivalCombatRadarOperations({armor,armorStrip_,pages}, view_, playerPosition,
        playerYaw, level1Inputs(actors), profile, cache_.frames.front().bitmap, markerColors,
        type11Model, std::move(animations));
    retainLevel1Draw(work.radar,actors,playerPosition,playerYaw);
    return work;
}

Level1DynamicRadarWork RadarState::drawLevel1DeploymentCombat(const std::int16_t armor,
    const FixedPosition playerPosition,const std::uint16_t playerYaw,
    const std::vector<RadarStateActor>& actors,const RadarPages& pages,
    const RadarTraversalProfile profile,const Model3d& type11Model,const Model3d& guardianModel,
    std::vector<std::optional<Level1AnimationState>> animations) {
    auto work=level1DeploymentCombatRadarOperations({armor,armorStrip_,pages},view_,playerPosition,
        playerYaw,level1Inputs(actors),profile,cache_.frames.front().bitmap,markerColors,
        type11Model,guardianModel,std::move(animations));
    retainLevel1Draw(work.radar,actors,playerPosition,playerYaw);return work;
}

void RadarState::retainCompletionBeam(const std::size_t allocation, const CompletionBeamPresentation& beam) {
    auto output = projection(allocation);
    output.center = output.cachedCenter = beam.center;
    output.flags = beam.projected ? 12 : 8;
    output.projected = beam.projected;
    if (beam.projected) {
        output.projectedX = beam.anchor.x;
        output.projectedY = beam.anchor.y;
        output.projectedRadius = beam.radius;
    }
    projections_[allocation] = output;
}

RadarWork RadarState::draw(const std::int16_t armor, const FixedPosition playerPosition,
    const std::uint16_t playerYaw, const std::vector<RadarStateActor>& actors,
    const RadarPages& pages, const RadarTraversalProfile profile, const RadarPageLayout layout,
    const RadarPixelDomain domain) {
    if (!initialized_) {
        throw std::invalid_argument("radar requires source startup draw");
    }
    std::array<bool, allocationCapacity> selected{};
    std::vector<RadarActorInput> inputs;
    inputs.reserve(actors.size());
    for (const auto& actor : actors) {
        const auto& previous = projection(actor.allocation);
        if (selected[actor.allocation]) {
            throw std::invalid_argument("duplicate radar actor identity");
        }
        selected[actor.allocation] = true;
        inputs.push_back({actor.actor, actor.radius, actor.type, actor.category,
            actor.callback, actor.renderProfile, previous.projectedX, previous.projectedY,
            previous.projectedRadius});
    }
    const auto pixelsOnly = domain==RadarPixelDomain::playableCampaign
        || layout!=RadarPageLayout::compatibility || armor < 0 || (playerYaw & 0x000fU) != 0
        || std::any_of(actors.begin(), actors.end(), [](const auto& actor) {
        return actor.renderProfile == WorldActorRenderProfile::bitmapEffectCallback
            || actor.renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation
            || actor.renderProfile == WorldActorRenderProfile::heapSelectorNoCallback
            || actor.type == 1 || actor.type == 2 || actor.type == 4 || actor.type == 5 || actor.type == 10
            || actor.type == 17 || actor.type == 20 || actor.type == 31
            || actor.type == completionCraftType || actor.type == guardianFixedType
            || actor.type == chasingActorType || actor.type == descendingActorType || actor.type == completionPickupType
            || actor.type == systemBonusType;
    });
    auto work = pixelsOnly
        ? radarPresentationPixels({armor, armorStrip_, pages}, view_, playerPosition,
            playerYaw, inputs, profile, cache_.frames.front().bitmap, markerColors,layout,domain)
        : radarOperations({armor, armorStrip_, pages}, view_, playerPosition,
            playerYaw, inputs, profile, cache_.frames.front().bitmap, markerColors);
    for (std::size_t index = 0; index < actors.size(); ++index) {
        projections_[actors[index].allocation] = work.actors[index];
    }
    armorStrip_ = *work.state.armorStrip;
    view_.origin.x = playerPosition.x;
    view_.origin.y = playerPosition.y;
    view_.actorAngles[2] = playerYaw;
    return work;
}

}


namespace stellar7 {
RadarWork RadarState::drawLevel2Constructor(const std::int16_t armor,const FixedPosition playerPosition,
    const std::uint16_t playerYaw,const std::vector<RadarStateActor>& actors,
    const RadarPages& pages,const RadarTraversalProfile profile) {
    constexpr std::size_t fixedBase=16;
    constexpr std::size_t projectileBase=32;
    for(const auto& actor:actors) {
        if(actor.category==-2 ? actor.allocation<fixedBase || actor.allocation>=projectileBase
            : actor.allocation>=fixedBase) {
            throw std::invalid_argument("Level2 constructor radar lost its physical cache identity");
        }
    }
    auto work=level2ConstructorRadarOperations({armor,armorStrip_,pages},view_,playerPosition,playerYaw,
        level1Inputs(actors),profile,cache_.frames.front().bitmap,markerColors);
    retainLevel1Draw(work,actors,playerPosition,playerYaw);
    return work;
}
} // namespace stellar7


namespace stellar7 {
RadarWork RadarState::drawLevel2Combat(const std::int16_t armor,const FixedPosition playerPosition,
    const std::uint16_t playerYaw,const std::vector<RadarStateActor>& actors,
    const RadarPages& pages,const RadarTraversalProfile profile,const Level2RadarDomain domain) {
    for(const auto& actor:actors) {
        const auto key=actor.allocation;
        const auto valid=actor.type==-1 ? key>=40 && key<48 : (actor.type==23 || actor.type==19
            || (domain==Level2RadarDomain::periodicLifecycle && (actor.type==17 || actor.type==20))) ? key>=32 && key<40
            : actor.type==30 ? key>=16 && key<32 : key<16;
        if(!valid) {throw std::invalid_argument("Level2 combat radar lost its physical cache identity");}
    }
    auto work=level2CombatRadarOperations({armor,armorStrip_,pages},view_,playerPosition,playerYaw,
        level1Inputs(actors),profile,cache_.frames.front().bitmap,markerColors,domain);
    retainLevel1Draw(work,actors,playerPosition,playerYaw);
    return work;
}
} // namespace stellar7
