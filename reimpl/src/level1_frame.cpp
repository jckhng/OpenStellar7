#include "level1_retained_roster_frame.hpp"

#include "stellar7/indexed_render.hpp"
#include "stellar7/ground_steering_work.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::size_t actorCount = 6;
constexpr std::size_t panelGroup = 3;
constexpr std::size_t overlayGroup = 4;
constexpr std::size_t firstGroundBitmap = 7;
constexpr std::size_t indicatorIndex = 6;
constexpr std::size_t type11 = 11;
constexpr std::uint16_t animationCallback = 0x130c;
constexpr Viewport groundViewport{24, 295, 24, 145};
constexpr Viewport worldViewport{24, 294, 24, 145};
constexpr std::uint64_t worldSetupCallerDispatches = 13;
constexpr std::uint8_t sightColor = 95;

CockpitWorkBitmaps cockpitBitmaps(const GameplayAssets& assets) {
    constexpr std::size_t stripIndex = 1;
    constexpr std::size_t backgroundIndex = 2;
    if (assets.artGroups[panelGroup].size() <= backgroundIndex
        || assets.artGroups[overlayGroup].size() <= indicatorIndex) {
        throw std::invalid_argument("Level1 cockpit requires the source bitmap groups");
    }
    CockpitWorkBitmaps result{};
    result.strip = assets.artGroups[panelGroup][stripIndex];
    result.matchingPageBackground = assets.artGroups[panelGroup][backgroundIndex];
    result.indicator = assets.artGroups[overlayGroup][indicatorIndex];
    for (std::size_t index = 0; index < result.overlays.size(); ++index) {
        result.overlays[index] = assets.artGroups[overlayGroup][index];
    }
    return result;
}
}

namespace {
enum class FrameComposition { entities, primaryProjectile, combatProjectiles, periodicCombat, effectCombat };
enum class LiftDomain { initial, retainedRoster };
constexpr std::size_t projectileAllocationBase = 16;
constexpr std::int16_t primaryProjectileType = 23;
constexpr std::uint32_t primaryProjectileRadius = 134;
constexpr std::size_t missingActorOrdinal = std::numeric_limits<std::size_t>::max();

struct FrameView {
    const Level1GroundState& ground;
    const StellarRng& rng;
    const Level1Entity& player;
    std::uint16_t loopTick;
};
struct FrameObjects {
    FrameComposition composition;
    std::vector<WorldSceneActor> world;
    std::vector<RadarStateActor> radar;
    SightRefreshInput refresh;
    std::size_t liftOrdinal = missingActorOrdinal;
    std::vector<std::optional<Level1CombatEffectView>> effects;
    std::vector<Level1ObjectRef> allocations;
    bool bonus = false;
    bool arrival = false;
    bool deployment = false;
    Level1OrdinaryFamily ordinaryFamily = Level1OrdinaryFamily::type1;
};
struct CombatFrameObservations {
    std::vector<Level1CombatEffectObservation> effects;
    std::vector<Level1ObjectRef> allocations;
};

FrameObjects frameObjects(const Level1Entity& player, const std::vector<Level1Entity>& entities,
    const std::vector<Level1ObjectRef>& order,
    const std::array<Level1Projectile, Level1State::projectilePoolSize>* projectiles,
    const FrameComposition composition, const Level1CombatRenderContext* effectContext = nullptr,
    const LiftDomain liftDomain = LiftDomain::initial) {
    const auto effects = composition == FrameComposition::effectCombat;
    const auto ordinaryFamily = effects && effectContext ? effectContext->ordinaryFamily : Level1OrdinaryFamily::type1;
    if (ordinaryFamily != Level1OrdinaryFamily::type1 && ordinaryFamily != Level1OrdinaryFamily::type1And3) {
        throw std::invalid_argument("unsupported Level1 ordinary render family");
    }
    const auto type3Family = ordinaryFamily == Level1OrdinaryFamily::type1And3;
    const auto roster=effects && effectContext
        && effectContext->projectileAllocationDomain==Level1ProjectileAllocationDomain::retainedRoster;
    const auto optionalLift = liftDomain == LiftDomain::retainedRoster;
    if (optionalLift && (!roster || effectContext->liftMembership != Level1CombatLiftMembership::allowRetiredInitial)) {
        throw std::invalid_argument("optional lift requires the explicit retained Level1 roster");
    }
    if (effects && effectContext
        && effectContext->projectileAllocationDomain!=Level1ProjectileAllocationDomain::freshOnly
        && effectContext->projectileAllocationDomain!=Level1ProjectileAllocationDomain::retained
        && effectContext->projectileAllocationDomain!=Level1ProjectileAllocationDomain::retainedAirborne && !roster) {
        throw std::invalid_argument("unsupported retained projectile render domain");
    }
    if (effects && effectContext
        && effectContext->liftMembership != Level1CombatLiftMembership::required
        && effectContext->liftMembership != Level1CombatLiftMembership::allowRetiredInitial) {
        throw std::invalid_argument("unsupported combat lift membership");
    }
    const auto bonus = effects && effectContext
        && effectContext->bonusMembership == Level1CombatBonusMembership::oneType42;
    if (effects && effectContext && effectContext->bonusMembership != Level1CombatBonusMembership::none && !bonus) {
        throw std::invalid_argument("unsupported combat bonus membership");
    }
    const auto deployment=effects && effectContext
        && effectContext->arrivalMembership==Level1CombatArrivalMembership::deployment;
    const auto arrival=deployment || (effects && effectContext
        && effectContext->arrivalMembership==Level1CombatArrivalMembership::approachCraft);
    if (effects && effectContext && effectContext->arrivalMembership!=Level1CombatArrivalMembership::none && !arrival) {
        throw std::invalid_argument("unsupported arrival frame membership");
    }
    const auto dynamic = effects || composition == FrameComposition::periodicCombat;
    const auto combat = dynamic || composition == FrameComposition::combatProjectiles;
    if (effects ? !effectContext || !projectiles || entities.size() < actorCount || entities.size() > (arrival ? 16U : bonus ? 15U : 14U)
            || order.empty() || order.size() > 32 || effectContext->allocationOrder.size() != order.size()
            || effectContext->effects.size() > Level1State::deathEffectPoolSize
        : dynamic ? entities.size() < actorCount || entities.size() > 14 || !projectiles
            || order.size() < entities.size() || order.size() > entities.size() + projectiles->size()
        : entities.size() != actorCount || (projectiles
            ? order.size() < actorCount || (!combat && order.size() == actorCount)
                || order.size() > actorCount + projectiles->size()
            : order.size() != actorCount)) {
        throw std::invalid_argument("unsupported Level1 frame object composition");
    }
    std::vector<bool> selected(entities.size());
    std::array<bool, Level1State::projectilePoolSize> selectedProjectiles{};
    std::size_t ordinary = 0;
    std::size_t lifts = 0;
    std::size_t fixed = 0;
    std::size_t shots = 0;
    std::size_t bonuses = 0;
    std::size_t crafts = 0;
    std::size_t guardians = 0;
    std::size_t rewards = 0;
    FrameObjects result{composition, {}, {}, {{}, player.yaw}};
    result.bonus = bonus; result.ordinaryFamily = ordinaryFamily;
    result.arrival = arrival;result.deployment=deployment;
    std::array<bool,Level1State::deathEffectPoolSize> selectedEffects{};
    if (effects) {
        result.allocations = effectContext->allocationOrder;
        std::array<bool,Level1State::deathEffectPoolSize> supplied{};
        for (const auto& effect : effectContext->effects) {
            if (effect.poolSlot >= supplied.size() || supplied[effect.poolSlot]) {
                throw std::invalid_argument("invalid or duplicate retained FC view");
            }
            supplied[effect.poolSlot] = true;
        }
        for (const auto ref : effectContext->allocationOrder) {
            if (std::count(effectContext->allocationOrder.begin(),effectContext->allocationOrder.end(),ref) != 1
                || std::find(order.begin(),order.end(),ref) == order.end()) {
                throw std::invalid_argument("combat main list differs from its late rebuilt registry");
            }
        }
    }
    for (const auto reference : order) {
        const auto index = reference.index;
        if (reference.kind == Level1PresentationKind::deathEffect && effects) {
            const auto match = std::find_if(effectContext->effects.begin(),effectContext->effects.end(),
                [index](const auto& value) { return value.poolSlot == index; });
            if (index >= selectedEffects.size() || selectedEffects[index] || match == effectContext->effects.end()
                || match->actor.flags != match->state.flags) {
                throw std::invalid_argument("invalid combat FC list identity/state");
            }
            selectedEffects[index] = true;
            result.world.push_back({match->actor,{},-1,match->radius,match->state.callback,0});
            result.effects.push_back(*match);
            constexpr std::size_t effectAllocationBase = 32;
            result.radar.push_back({effectAllocationBase+index,match->actor,match->radius,-1,-4,
                match->state.callback,WorldActorRenderProfile::bitmapEffectCallback});
            result.refresh.targets.push_back({-1,match->targetHeading});
            continue;
        }
        if (reference.kind == Level1PresentationKind::projectile && projectiles) {
            if (index >= projectiles->size() || selectedProjectiles[index]) {
                throw std::invalid_argument("invalid Level1 frame projectile identity");
            }
            const auto& shot = (*projectiles)[index];
            const auto primary = shot.type == primaryProjectileType && shot.collisionRadius == primaryProjectileRadius
                && shot.ownerIsPlayer && shot.ownerAllocationIndex == static_cast<std::size_t>(-1)
                && shot.stateBytes4dTo54[1] <= 64;
            const auto airborne = combat && shot.type==20 && shot.collisionRadius==134 && !shot.ownerIsPlayer
                && shot.stateBytes4dTo54[1]<=60 && shot.ownerAllocationIndex<entities.size()
                && (roster ? level1RetainedEmitterIdentity(entities,shot.ownerAllocationIndex)
                    : (entities[shot.ownerAllocationIndex].type==11
                    || (type3Family && entities[shot.ownerAllocationIndex].type==3
                        && entities[shot.ownerAllocationIndex].position.z==0
                        && entities[shot.ownerAllocationIndex].pitch==0 && entities[shot.ownerAllocationIndex].roll==0)))
                && (roster || effects || entities[shot.ownerAllocationIndex].active());
            const auto flatAirborne = !roster && airborne && entities[shot.ownerAllocationIndex].type == 3;
            const auto enemy = combat && shot.type == 17
                && shot.collisionRadius == 441 && !shot.ownerIsPlayer && shot.stateBytes4dTo54[1] <= 54
                && shot.ownerAllocationIndex < entities.size() && (roster ? level1RetainedEmitterIdentity(entities,shot.ownerAllocationIndex)
                    : (entities[shot.ownerAllocationIndex].type == 1
                    || (arrival && entities[shot.ownerAllocationIndex].type == 7)))
                && (roster || effects || entities[shot.ownerAllocationIndex].active());
            const auto deployed=deployment && shot.type==22 && shot.collisionRadius==134
                && !shot.ownerIsPlayer && shot.ownerAllocationIndex<entities.size()
                && entities[shot.ownerAllocationIndex].type==44 && shot.stateBytes4dTo54[1]<=64;
            if (shot.poolSlot != index || shot.flags != 0 || (!primary && !enemy && !airborne && !deployed)
                || shot.stateBytes4dTo54[0] != 0xfd || !shot.active()
                || (flatAirborne && (shot.roll != 0 || shot.position.z != 0))
                || (!deployed && (shot.pitch!=0 || (!airborne && shot.roll!=0)
                    || shot.position.z<0 || shot.position.z>(airborne ? 1000 : 0)
                    || shot.pitchVelocity!=0 || shot.rollVelocity!=0 || shot.yawVelocity!=0
                    || shot.xVelocity!=0 || shot.yVelocity!=0 || shot.zVelocity!=0)) || shot.forwardVelocity!=500) {
                throw std::invalid_argument("unsupported Level1 projectile render state");
            }
            std::array<std::uint8_t, 6> selector{};
            std::copy(shot.stateBytes4dTo54.begin() + 2, shot.stateBytes4dTo54.end(), selector.begin());
            if (deployed ? shot.stateBytes4dTo54[2]!=1 : selector!=std::array<std::uint8_t,6>{}) {
                throw std::invalid_argument("unsupported Level1 projectile inline selector");
            }
            selectedProjectiles[index] = true;
            ++shots;
            WorldSceneActor scene{{shot.flags, shot.position}, {shot.pitch, shot.roll, shot.yaw},
                shot.type, shot.collisionRadius, 0, 0};
            if (deployed) { scene.inlineSelector=shot.stateBytes4dTo54[2]; }
            else { scene.inlineAnimation=selector; }
            result.world.push_back(scene);
            if (effects) { result.effects.push_back(std::nullopt); }
            result.radar.push_back({projectileAllocationBase + index, {shot.flags, shot.position},
                shot.collisionRadius, shot.type, -3, 0, WorldActorRenderProfile::inlineSelectorNoCallback});
            result.refresh.targets.push_back({shot.type, shot.targetHeading});
            continue;
        }
        if (reference.kind != Level1PresentationKind::entity || index >= entities.size() || selected[index]) {
            throw std::invalid_argument("invalid Level1 frame actor order");
        }
        selected[index] = true;
        const auto& actor = entities[index];
        if (actor.allocationIndex != index || !actor.active()) {
            throw std::invalid_argument("invalid Level1 frame allocation identity");
        }
        WorldSceneActor scene{{actor.flags, actor.position}, {actor.pitch, actor.roll, actor.yaw},
            actor.type, actor.collisionRadius, 0, 0};
        auto profile = WorldActorRenderProfile::noHandles;
        const auto category = static_cast<std::int8_t>(actor.stateBytes4dTo52[0]);
        if (actor.type == type11 && actor.animatedModelPresenterEnabled && category == -1
            && (actor.primaryType11AnimationSelector
                || (dynamic && actor.renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation))) {
            ++lifts;
            if (result.liftOrdinal == missingActorOrdinal) { result.liftOrdinal = result.world.size(); }
            scene.drawCallback = animationCallback;
            scene.heapAnimation = Level1AnimationState{actor.stateBytes4dTo52[2],
                actor.stateBytes4dTo52[3], actor.modelAnimationVariant};
            profile = WorldActorRenderProfile::heapSelectorDeferredAnimation;
        } else if ((actor.type == 1 || (type3Family && actor.type == 3
                && actor.collisionRadius == 770 && actor.pitch == 0 && actor.roll == 0 && actor.position.z == 0
                && actor.renderProfile == WorldActorRenderProfile::noHandles)) && category == -1) {
            ++ordinary;
        } else if (arrival && actor.type==7 && category==-1) {
            const auto state=actor.stateBytes4dTo52[5];
            const auto beam=deployment && state==3 && (actor.flags==8 || actor.flags==12)
                && actor.renderProfile==WorldActorRenderProfile::inlineSelectorBeamCallback;
            const auto departure=deployment && state==4 && (actor.flags==0 || actor.flags==4)
                && actor.renderProfile==WorldActorRenderProfile::inlineSelectorDeparture;
            const auto approach=state<=2 && actor.flags==0 && actor.position.z==5000 && actor.pitch==0
                && actor.renderProfile==WorldActorRenderProfile::inlineSelectorNoCallback;
            //At timer29 source clears flags8/callback but remains state3 until timer0.
            const auto hatchReturn=deployment && state==3 && actor.behaviorTimer<30
                && (actor.flags==0 || actor.flags==4)
                && actor.renderProfile==WorldActorRenderProfile::inlineSelectorNoCallback;
            if (index>=16 || actor.collisionRadius!=13052 || actor.stateBytes4dTo52[1]!=100
                || actor.stateBytes4dTo52[2]>=4 || actor.stateBytes4dTo52[4]!=5
                || (!approach && !beam && !departure && !hatchReturn)
                || (beam && (actor.behaviorTimer<30 || actor.behaviorTimer>70))) {
                throw std::invalid_argument("unsupported approach craft frame state");
            }
            ++crafts;scene.inlineSelector=actor.stateBytes4dTo52[2];profile=actor.renderProfile;
            if (beam) {
                scene.drawCallback=0xb0b4;scene.beamPresentation=CompletionBeamPresentation{};
                scene.beamPresentation->timer=actor.behaviorTimer;
            }
        } else if (deployment && actor.type==44 && category==-1) {
            if (index>=16 || actor.flags!=0 || actor.collisionRadius!=2792 || actor.position.z!=0
                || actor.pitch!=0 || actor.roll!=0 || actor.stateBytes4dTo52[1]>15
                || actor.stateBytes4dTo52[4]!=5 || actor.stateBytes4dTo52[2]<1 || actor.stateBytes4dTo52[2]>2
                || actor.stateBytes4dTo52[3]>2 || actor.modelAnimationVariant>=8
                || (actor.animatedModelPresenterEnabled
                    ? actor.renderProfile!=WorldActorRenderProfile::heapSelectorDeferredAnimation
                    : actor.renderProfile!=WorldActorRenderProfile::heapSelectorNoCallback)) {
                throw std::invalid_argument("unsupported Level1 guardian frame state");
            }
            ++guardians;scene.drawCallback=actor.animatedModelPresenterEnabled ? 0x130c : 0;
            scene.heapAnimation=Level1AnimationState{actor.stateBytes4dTo52[2],actor.stateBytes4dTo52[3],actor.modelAnimationVariant};
            profile=actor.renderProfile;
        } else if (deployment && actor.type==25 && category==-2) {
            const auto selector=actor.stateBytes4dTo52[2];
            const auto direction=actor.stateBytes4dTo52[5];
            if (index>=16 || actor.flags!=0 || actor.collisionRadius!=897 || actor.position.z!=0
                || actor.pitch!=0 || actor.roll!=0 || actor.pitchVelocity!=0 || actor.rollVelocity!=0
                || (actor.yawVelocity!=2560 && actor.yawVelocity!=-2816
                    && actor.yawVelocity!=3072 && actor.yawVelocity!=-3328)
                || actor.stateBytes4dTo52[1]<1 || actor.stateBytes4dTo52[1]>100
                || actor.stateBytes4dTo52[4]!=0 || selector>=4 || (direction!=1 && direction!=255)
                || actor.renderProfile!=WorldActorRenderProfile::inlineSelectorNoCallback) {
                throw std::invalid_argument("unsupported Level1 reward presentation state");
            }
            ++rewards;scene.inlineSelector=selector;profile=actor.renderProfile;
        } else if (bonus && actor.type == 42 && category == -2) {
            const auto selector = actor.stateBytes4dTo52[2];
            if (index > 14 || actor.flags != 0 || actor.collisionRadius != 330 || actor.position.z != 100
                || actor.pitch != 0 || actor.roll % 0x800 != 0 || actor.yaw % 0x800 != 0
                || actor.pitchVelocity != 0 || actor.rollVelocity != 0x800 || actor.yawVelocity != 0x800
                || actor.xVelocity != 0 || actor.yVelocity != 0 || actor.zVelocity != 0 || actor.forwardVelocity != 0
                || actor.stateBytes4dTo52[1] != 1 || (selector != 3 && selector != 7)
                || actor.stateBytes4dTo52[5] != selector
                || actor.renderProfile != WorldActorRenderProfile::inlineSelectorNoCallback) {
                throw std::invalid_argument("unsupported Type42 bonus presentation state");
            }
            ++bonuses;
            scene.inlineSelector = selector;
            profile = WorldActorRenderProfile::inlineSelectorNoCallback;
        } else if (actor.type == 29 && category == -2) {
            ++fixed;
        } else {
            throw std::invalid_argument("unsupported Level1 frame actor profile");
        }
        result.world.push_back(scene);
        if (effects) { result.effects.push_back(std::nullopt); }
        result.radar.push_back({actor.allocationIndex, {actor.flags, actor.position}, actor.collisionRadius,
            actor.type, category, scene.drawCallback, profile});
        result.refresh.targets.push_back({actor.type, actor.targetHeading});
    }
    const auto hasLift = result.liftOrdinal != missingActorOrdinal;
    bool retiredInitialLift = false;
    if (effects && !hasLift && effectContext->liftMembership == Level1CombatLiftMembership::allowRetiredInitial) {
        const auto retained = std::find_if(entities.begin(), entities.end(), [](const auto& actor) {
            return actor.type == type11 && actor.primaryType11AnimationSelector;
        });
        retiredInitialLift = deployment || (retained != entities.end() && !retained->active()
            && retained->stateBytes4dTo52[0] == 0xff && retained->collisionRadius == 748
            && std::count_if(entities.begin(), entities.end(), [](const auto& actor) {
                return actor.type == type11 && actor.primaryType11AnimationSelector;
            }) == 1);
    }
    if ((effects ? ordinary + lifts > 10 || fixed > 4
            : dynamic ? ordinary + lifts + 4 != entities.size() : ordinary != 1)
        || bonuses > (bonus ? 1U : 0U) || crafts>(arrival ? 1U : 0U) || guardians>(deployment ? 1U : 0U) || rewards>(deployment ? 1U : 0U)
        || (!effects && fixed != 4) || (!hasLift && !retiredInitialLift && !optionalLift)
        || (projectiles ? !combat && shots == 0 : shots != 0)) {
        throw std::invalid_argument("Level1 frame requires six entities and its declared projectiles");
    }
    if (effects) {
        for (std::size_t index = 0; index < entities.size(); ++index) {
            if (entities[index].active() != selected[index]) {
                throw std::invalid_argument("combat main list omits a live entity or selects dead storage");
            }
        }
        for (const auto& effect : effectContext->effects) {
            if (!selectedEffects[effect.poolSlot]) {
                throw std::invalid_argument("unlisted live combat FC view");
            }
        }
    }
    if (projectiles) {
        for (std::size_t index = 0; index < projectiles->size(); ++index) {
            if ((*projectiles)[index].active() != selectedProjectiles[index]) {
                throw std::invalid_argument("unlisted live Level1 projectile");
            }
        }
    }
    return result;
}

Level1FramePlan planFrame(const GameplayAssets& assets, const FrameView& frame, FrameObjects objects,
    const Level1PresentationState presentation, const std::int16_t armor, const CockpitSettings& settings,
    const CockpitPageState& pages, const RadarState& radar, const IndexedPages& pixels,
    CombatFrameObservations* observations = nullptr) {
    constexpr std::uint16_t firstDisplay = 0xa000;
    constexpr std::uint16_t secondDisplay = 0xa800;
    constexpr std::uint16_t cache = 0xa400;
    constexpr std::size_t groundBitmapCount = 8;
    if (assets.models.size() <= (objects.composition == FrameComposition::entities ? type11 : primaryProjectileType)
        || assets.artGroups.size() <= overlayGroup
        || assets.artGroups[overlayGroup].size() < firstGroundBitmap + groundBitmapCount
        || pages.cachePage() != cache || pages.drawPage() != pages.backPage()
        || !((pages.frontPage() == firstDisplay && pages.backPage() == secondDisplay)
            || (pages.frontPage() == secondDisplay && pages.backPage() == firstDisplay))) {
        throw std::invalid_argument("unsupported first Level1 frame composition");
    }
    Level1FramePlan plan{{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, std::move(objects.radar), {},
        pages, radar, pixels, {}, {}};
    for (std::size_t index = 0; index < plan.groundBitmaps.size(); ++index) {
        plan.groundBitmaps[index] = assets.artGroups[overlayGroup][firstGroundBitmap + index];
    }
    const auto& player = frame.player;
    const HorizonWorkState horizon{24, 145, static_cast<std::int16_t>(player.yaw), settings.viewSize,
        256, 16, HorizonBackground::gradient, HorizonOverlay::enabled, 233, 246};
    plan.horizon = horizonExecution(horizon, assets.horizonFramesForLevel(0));
    plan.operations = plan.horizon.operations;
    const auto append = [&](const auto& operations) {
        plan.operations.insert(plan.operations.end(), operations.begin(), operations.end());
    };
    plan.ground = level1GroundSpriteExecution({frame.ground.points(), player.yawVelocity,
        player.forwardVelocity, player.position.z}, frame.rng, plan.groundBitmaps, groundViewport);
    append(plan.ground.operations);
    plan.world = worldViewSetupOperations({player.position, {}, {player.pitch, player.roll, player.yaw},
        0x88, 7, worldViewport, {256, -256, 362}});
    plan.operations.push_back({NormalWorkKind::instructions, worldSetupCallerDispatches});
    append(plan.world.operations);
    if (objects.composition == FrameComposition::effectCombat) {
        if (!observations) { throw std::logic_error("missing mixed combat observation sink"); }
        const auto compose = objects.arrival ? level1ArrivalCombatSceneOperations
            : objects.bonus ? level1BonusCombatSceneOperations : level1CombatSceneOperations;
        for (std::size_t ordinal=0;ordinal<objects.world.size();++ordinal) {
            auto& actor=objects.world[ordinal];
            if (!actor.beamPresentation) { continue; }
            const auto& cache=plan.radarState.projection(plan.radarActors[ordinal].allocation);
            actor.beamPresentation->center=cache.cachedCenter;
            actor.beamPresentation->anchor={cache.projectedX,cache.projectedY};
            actor.beamPresentation->radius=cache.projectedRadius;
        }
        const WorldSceneProfile sceneProfile{assets.sharedModelRegistry[0],assets.sharedModelRegistry[1],0,0};
        const std::array<std::int16_t,3> light{static_cast<std::int16_t>(plan.world.transformedLight.x),
            static_cast<std::int16_t>(plan.world.transformedLight.y),static_cast<std::int16_t>(plan.world.transformedLight.z)};
        const auto ordinaryRenderFamily = objects.deployment ? Level1OrdinaryRenderFamily::deployment
            : objects.arrival ? Level1OrdinaryRenderFamily::arrival
            : objects.bonus ? Level1OrdinaryRenderFamily::bonus : Level1OrdinaryRenderFamily::combatEffects;
        auto scene=objects.ordinaryFamily == Level1OrdinaryFamily::type1And3
            ? level1Type3CombatSceneOperations(assets,plan.world.view,objects.world,objects.effects,
                sceneProfile,light,pages.drawPage(),objects.allocations,ordinaryRenderFamily,
                objects.deployment ? &plan.ground.rng : nullptr)
            : objects.deployment
            ? level1DeploymentCombatSceneOperations(assets,plan.world.view,objects.world,objects.effects,
                sceneProfile,light,pages.drawPage(),objects.allocations,plan.ground.rng)
            : compose(assets,plan.world.view,objects.world,objects.effects,sceneProfile,light,pages.drawPage(),objects.allocations);
        plan.rngAfterScene=std::move(scene.rngAfterMain);plan.beams=std::move(scene.beams);
        for (const auto& beam:plan.beams) {
            auto& actor=plan.radarActors.at(beam.ordinal);
            actor.actor.flags=beam.projection.flags;
            plan.radarState.retainCompletionBeam(actor.allocation,beam.beam);
        }
        plan.scene = std::move(scene.scene);
        observations->effects = std::move(scene.effects);
        observations->allocations = std::move(scene.allocationOrderAfterMain);
        for (const auto& effect : observations->effects) {
            auto& actor = plan.radarActors.at(effect.ordinal);
            actor.actor.flags = effect.work.state.flags;
            actor.callback = effect.work.state.callback;
            plan.radarState.retainLevel1CombatEffectProjection(actor.allocation,effect.work,
                objects.deployment ? Level1EffectProjectionDomain::guardianCombat : Level1EffectProjectionDomain::ordinaryCombat);
        }
    } else {
        plan.scene = worldSceneOperations(assets, plan.world.view, objects.world,
            {assets.sharedModelRegistry[0], assets.sharedModelRegistry[1], 0, 0},
            {static_cast<std::int16_t>(plan.world.transformedLight.x),
                static_cast<std::int16_t>(plan.world.transformedLight.y),
                static_cast<std::int16_t>(plan.world.transformedLight.z)},
            objects.composition == FrameComposition::entities ? WorldSceneMatrixProfile::level1Banked
                : (objects.composition == FrameComposition::combatProjectiles || objects.composition == FrameComposition::periodicCombat)
                    ? WorldSceneMatrixProfile::level1Combat
                : WorldSceneMatrixProfile::level1Projectile);
    }
    append(plan.scene.operations);
    const auto hasLift = objects.liftOrdinal != missingActorOrdinal;
    if (hasLift) {
        if (!plan.scene.animationStates[objects.liftOrdinal]) {
            throw std::logic_error("Level1 main callback did not return its selector");
        }
        plan.animationAfterMain = *plan.scene.animationStates[objects.liftOrdinal];
    }
    plan.sight = sightOperations(frame.loopTick, presentation.alternateSightPhase, worldViewport, sightColor, objects.refresh);
    append(plan.sight.operations);
    plan.cockpitBitmaps = cockpitBitmaps(assets);
    plan.pages.selectCockpitCopy();
    const auto relation = plan.pages.cachedPageDiffersFromDraw() ? CockpitPageRelation::differing : CockpitPageRelation::matching;
    plan.cockpit = cockpitOperations({presentation.indicator.x, presentation.indicator.velocity, settings.viewSize, relation},
        plan.cockpitBitmaps);
    append(plan.cockpit.operations);
    plan.presentationAfter = {{plan.cockpit.state.indicatorX, plan.cockpit.state.indicatorVelocity}, plan.sight.alternatePhase};
    plan.radarEntryPages = {plan.pages.cachePage(), plan.pages.backPage(), plan.pages.frontPage(),
        plan.pages.drawPage(), plan.pages.copyPage()};
    std::optional<Level1DynamicRadarWork> dynamicRadar;
    if (objects.composition == FrameComposition::periodicCombat || objects.composition == FrameComposition::effectCombat) {
        auto animations = plan.scene.animationStates;
        const auto ordinaryRenderFamily = objects.deployment ? Level1OrdinaryRenderFamily::deployment
            : objects.arrival ? Level1OrdinaryRenderFamily::arrival
            : objects.bonus ? Level1OrdinaryRenderFamily::bonus : Level1OrdinaryRenderFamily::combatEffects;
        dynamicRadar = objects.ordinaryFamily == Level1OrdinaryFamily::type1And3
            ? plan.radarState.drawLevel1Type3Combat(armor,player.position,player.yaw,
                plan.radarActors,plan.radarEntryPages,{0,0},assets.models[type11],
                objects.deployment ? &assets.models.at(44) : nullptr,std::move(animations),ordinaryRenderFamily)
            : objects.composition == FrameComposition::effectCombat
            ? (objects.deployment ? plan.radarState.drawLevel1DeploymentCombat(armor,player.position,player.yaw,
                plan.radarActors,plan.radarEntryPages,{0,0},assets.models[type11],assets.models.at(44),std::move(animations))
                : objects.arrival ? plan.radarState.drawLevel1ArrivalCombat(armor,player.position,player.yaw,
                plan.radarActors,plan.radarEntryPages,{0,0},assets.models[type11],std::move(animations))
                : objects.bonus ? plan.radarState.drawLevel1BonusCombat(armor,player.position,player.yaw,
                plan.radarActors,plan.radarEntryPages,{0,0},assets.models[type11],std::move(animations))
                : plan.radarState.drawLevel1CombatEffects(armor,player.position,player.yaw,
                plan.radarActors,plan.radarEntryPages,{0,0},assets.models[type11],std::move(animations)))
            : plan.radarState.drawLevel1DynamicCombat(armor, player.position, player.yaw,
                plan.radarActors, plan.radarEntryPages, {0, 0}, assets.models[type11], std::move(animations));
    }
    auto radarWork = dynamicRadar
        ? Level1RadarWork{std::move(dynamicRadar->radar),
            hasLift ? dynamicRadar->animations[objects.liftOrdinal].value() : Level1AnimationState{}}
        : objects.composition == FrameComposition::entities
        ? plan.radarState.drawLevel1(armor, player.position, player.yaw, plan.radarActors,
            plan.radarEntryPages, {0, 0}, assets.models[type11], plan.animationAfterMain)
        : objects.composition == FrameComposition::combatProjectiles
        ? plan.radarState.drawLevel1Combat(armor, player.position, player.yaw, plan.radarActors,
            plan.radarEntryPages, {0, 0}, assets.models[type11], plan.animationAfterMain)
        : plan.radarState.drawLevel1Projectile(armor, player.position, player.yaw, plan.radarActors,
            plan.radarEntryPages, {0, 0}, assets.models[type11], plan.animationAfterMain);
    plan.animationAfterRadar = radarWork.animation;
    plan.animationsAfterRadar = dynamicRadar ? std::move(dynamicRadar->animations) : plan.scene.animationStates;
    if (!dynamicRadar && hasLift) { plan.animationsAfterRadar[objects.liftOrdinal] = radarWork.animation; }
    plan.radar = std::move(radarWork.radar);
    if (!plan.radar.cpuWorkQualified) {
        throw std::logic_error("Level1 radar returned an unqualified work plan");
    }
    append(plan.radar.operations);
    const auto drawPage = plan.pages.drawPage();
    const auto appendPixels = [&](const auto& writes) {
        for (const auto& pixel : writes) {
            plan.effects.emplace_back(RadarPixelWrite{pixel.point, pixel.color, drawPage});
        }
    };
    appendPixels(plan.horizon.pixels);
    appendPixels(plan.ground.pixels);
    appendPixels(plan.scene.pixels);
    appendPixels(plan.sight.pixels);
    const auto cockpitEffects = cockpitPageEffects(plan.cockpit, plan.cockpitBitmaps, plan.pages.cachePage(), drawPage);
    plan.effects.insert(plan.effects.end(), cockpitEffects.begin(), cockpitEffects.end());
    for (const auto& pixel : plan.radar.pixels) {
        plan.effects.emplace_back(pixel);
    }
    if (plan.radar.pageCopy) {
        plan.effects.emplace_back(*plan.radar.pageCopy);
        plan.pages.selectArmorCopy();
    }
    plan.pixels.apply(plan.effects);
    return plan;
}

}

Level1FramePlan planLevel1Frame(const GameplayAssets& assets, const Level1RenderState& frame,
    const Level1PresentationState presentation, const std::int16_t armor, const CockpitSettings& settings,
    const CockpitPageState& pages, const RadarState& radar, const IndexedPages& pixels) {
    if (frame.entities.size() != actorCount || frame.order.size() != actorCount) {
        throw std::invalid_argument("unsupported first Level1 frame composition");
    }
    std::vector<Level1ObjectRef> order;
    for (const auto index : frame.order) {
        order.push_back({Level1PresentationKind::entity, index});
    }
    return planFrame(assets, {frame.ground, frame.rng, frame.player, frame.loopTick},
        frameObjects(frame.player, frame.entities, order, nullptr, FrameComposition::entities),
        presentation, armor, settings, pages, radar, pixels);
}

Level1FramePlan planLevel1ProjectileFrame(const GameplayAssets& assets, const Level1ProjectileRenderState& frame,
    const Level1PresentationState presentation, const std::int16_t armor, const CockpitSettings& settings,
    const CockpitPageState& pages, const RadarState& radar, const IndexedPages& pixels) {
    return planFrame(assets, {frame.ground, frame.rng, frame.player, frame.loopTick},
        frameObjects(frame.player, frame.entities, frame.order, &frame.projectiles, FrameComposition::primaryProjectile),
        presentation, armor,
        settings, pages, radar, pixels);
}

Level1FramePlan planLevel1CombatFrame(const GameplayAssets& assets, const Level1ProjectileRenderState& frame,
    const Level1PresentationState presentation, const std::int16_t armor, const CockpitSettings& settings,
    const CockpitPageState& pages, const RadarState& radar, const IndexedPages& pixels) {
    return planFrame(assets, {frame.ground, frame.rng, frame.player, frame.loopTick},
        frameObjects(frame.player, frame.entities, frame.order, &frame.projectiles, FrameComposition::combatProjectiles),
        presentation, armor, settings, pages, radar, pixels);
}

Level1FramePlan planLevel1PeriodicCombatFrame(const GameplayAssets& assets,
    const Level1ProjectileRenderState& frame, const Level1PresentationState presentation,
    const std::int16_t armor, const CockpitSettings& settings, const CockpitPageState& pages,
    const RadarState& radar, const IndexedPages& pixels) {
    return planFrame(assets, {frame.ground, frame.rng, frame.player, frame.loopTick},
        frameObjects(frame.player, frame.entities, frame.order, &frame.projectiles, FrameComposition::periodicCombat),
        presentation, armor, settings, pages, radar, pixels);
}

namespace {
Level1CombatFramePlan effectCombatFrame(const GameplayAssets& assets,
    const Level1ProjectileRenderState& frame, const Level1CombatRenderContext& context,
    const Level1PresentationState presentation, const std::int16_t armor,
    const CockpitSettings& settings, const CockpitPageState& pages,
    const RadarState& radar, const IndexedPages& pixels, const LiftDomain liftDomain) {
    constexpr std::size_t effectAllocationBase = 32;
    for (const auto& effect : context.effects) {
        if (effect.poolSlot >= Level1State::deathEffectPoolSize) {
            throw std::invalid_argument("invalid combat FC cache identity");
        }
        const auto& cached = radar.projection(effectAllocationBase+effect.poolSlot);
        if (effect.state.projected.x != cached.projectedX || effect.state.projected.y != cached.projectedY
            || effect.state.projectedRadius != cached.projectedRadius) {
            throw std::invalid_argument("combat FC view must use its retained projection cache");
        }
    }
    CombatFrameObservations observations;
    auto plan = planFrame(assets,{frame.ground,frame.rng,frame.player,frame.loopTick},
        frameObjects(frame.player,frame.entities,frame.order,&frame.projectiles,FrameComposition::effectCombat,&context,liftDomain),
        presentation,armor,settings,pages,radar,pixels,&observations);
    return {std::move(plan),std::move(observations.effects),std::move(observations.allocations)};
}

} // namespace

Level1CombatFramePlan planLevel1EffectCombatFrame(const GameplayAssets& assets,
    const Level1ProjectileRenderState& frame, const Level1CombatRenderContext& context,
    const Level1PresentationState presentation, const std::int16_t armor,
    const CockpitSettings& settings, const CockpitPageState& pages,
    const RadarState& radar, const IndexedPages& pixels) {
    return effectCombatFrame(assets,frame,context,presentation,armor,settings,pages,radar,pixels,LiftDomain::initial);
}

Level1CombatFramePlan planLevel1RetainedRosterFrame(const GameplayAssets& assets,
    const Level1ProjectileRenderState& frame, const Level1CombatRenderContext& context,
    const Level1PresentationState presentation, const std::int16_t armor,
    const CockpitSettings& settings, const CockpitPageState& pages,
    const RadarState& radar, const IndexedPages& pixels) {
    return effectCombatFrame(assets,frame,context,presentation,armor,settings,pages,radar,pixels,LiftDomain::retainedRoster);
}

void commitLevel1Frame(const Level1FramePlan& plan, const Level1RenderState frame,
    CockpitPageState& pages, RadarState& radar, IndexedPages& pixels) {
    frame.ground = plan.ground.ground;
    frame.rng = plan.rngAfterScene.value_or(plan.ground.rng);
    pages = plan.pages;
    radar = plan.radarState;
    pixels = plan.pixels;
}

void commitLevel1ProjectileFrame(const Level1FramePlan& plan, const Level1ProjectileRenderState frame,
    CockpitPageState& pages, RadarState& radar, IndexedPages& pixels) {
    frame.ground = plan.ground.ground;
    frame.rng = plan.rngAfterScene.value_or(plan.ground.rng);
    pages = plan.pages;
    radar = plan.radarState;
    pixels = plan.pixels;
}

}
