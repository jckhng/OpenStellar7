#include "stellar7/radar_work.hpp"
#include "stellar7/level2_projectile_prefix_work.hpp"
#include "stellar7/raster_line_work.hpp"
#include "stellar7/raster_point_work.hpp"
#include "stellar7/level1_deployment_render_work.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int32_t radarDepth = 200000;
constexpr std::int32_t maximumLiftAltitude = 1000;
constexpr std::int32_t maximumDeploymentEffectAltitude = 65535; //Existing signed main0AC8 bounds.
constexpr std::int16_t airborneProjectileType = 20;
constexpr std::int16_t completionCraftType = 7;
constexpr std::int32_t completionCraftAltitude = 5000;
constexpr std::uint16_t completionBeamCallback = 0xb0b4;
constexpr std::int16_t guardianType = 44;
constexpr std::int16_t openGuardianType = 45;
constexpr std::int16_t guardianFixedType = 46;
constexpr std::uint16_t guardianAnimationCallback = 0x130c;
constexpr std::uint32_t guardianRadius = 1147;
constexpr std::uint32_t level5GuardianRadius = 1508;
constexpr std::int16_t type11 = 11;
constexpr std::int16_t level1OrdinaryType = 1;
constexpr std::int16_t level1FixedType = 29;
constexpr std::uint32_t level1OrdinaryRadius = 1342;
constexpr std::uint32_t level1FixedRadius = 862;
constexpr std::int16_t level3OrdinaryType = 10;
constexpr std::int16_t level3GroundType = 2;
constexpr std::int16_t level3FixedAnimatedType = 24;
constexpr std::int16_t level3FixedInlineType = 31;
constexpr std::uint32_t type11Radius = 748;
constexpr std::int32_t type11MinimumAltitude = 150;
constexpr std::int32_t type11AltitudeStep = 50;
constexpr std::uint32_t guardianFixedRadius = 2260;
constexpr std::int16_t chasingActorType = 47;
constexpr std::int16_t descendingActorType = 48;
constexpr std::uint32_t inlineActorRadius = 559;
constexpr std::int32_t inlineLaunchAltitude = 500;
constexpr std::int32_t inlineDescentStep = 50;
constexpr std::int16_t completionPickupType = 25;
constexpr std::int16_t systemBonusType = 42;
constexpr std::uint32_t systemBonusRadius = 330;
constexpr std::int32_t systemBonusAltitude = 100;
constexpr std::uint32_t completionPickupRadius = 897;
constexpr std::uint8_t completionPickupColor = 234;
// Conservative controlled arithmetic domain: shift7 numerator stays signed32.
constexpr std::int64_t maximumRelativeCoordinate = 1000000;
constexpr std::int32_t projectionScale = 128;
constexpr std::uint8_t radarFlags = 0x88;

struct Division {
    std::int32_t value;
    std::uint64_t dispatches;
};

Division divide(const std::int32_t coordinate, const std::uint32_t denominator) {
    // 5413 takes the signed 64-bit numerator, then5392 normalizes both operands.
    // This profile has two right shifts; discarded bits are part of the source result.
    const auto numerator = static_cast<std::int64_t>(coordinate) * projectionScale;
    const auto magnitude = static_cast<std::uint32_t>(numerator < 0 ? -numerator : numerator);
    const auto quotient = (magnitude >> 2) / (denominator >> 2);
    return {numerator < 0 ? -static_cast<std::int32_t>(quotient) : static_cast<std::int32_t>(quotient),
        64U + (numerator < 0 ? 18U : 0U)};
}

void instructions(std::vector<NormalWorkOperation>& operations, const std::uint64_t count) {
    operations.push_back({NormalWorkKind::instructions, count});
}

enum class BoundSide { minimum, maximum };
enum class RadarExecution { sourceWork, nativePixels, level1Source, level1Combat, level1CombatEffects, level1Bonus, level1Arrival, level1Deployment, level2Constructor, level2Combat, playablePixels };

bool pixelExecution(const RadarExecution execution) {
    return execution==RadarExecution::nativePixels || execution==RadarExecution::playablePixels;
}

bool playableRadarActor(const RadarActorInput& actor) {
    if (actor.type==-1) {
        return actor.category==-4 && actor.radius==0
            && actor.renderProfile==WorldActorRenderProfile::bitmapEffectCallback
            && (actor.callback==0 || actor.callback==0x0ac8);
    }
    if (actor.type<0 || actor.type>48 || actor.radius==0 || actor.radius>=32768
        || actor.category < -4 || actor.category > -1) {
        return false;
    }
    switch (actor.renderProfile) {
    case WorldActorRenderProfile::noHandles:
    case WorldActorRenderProfile::heapSelectorNoCallback:
    case WorldActorRenderProfile::inlineSelectorNoCallback:
        return actor.callback==0;
    case WorldActorRenderProfile::heapSelectorDeferredAnimation:
        return actor.callback==guardianAnimationCallback;
    case WorldActorRenderProfile::inlineSelectorBeamCallback:
        return actor.type==completionCraftType && actor.category==-1 && actor.callback==completionBeamCallback;
    case WorldActorRenderProfile::inlineSelectorDeparture:
        return actor.type==completionCraftType && actor.category==-1 && actor.callback==0;
    default:
        return false;
    }
}

bool combatExecution(const RadarExecution execution) {
    return execution == RadarExecution::level1Combat || execution == RadarExecution::level1CombatEffects
        || execution == RadarExecution::level1Bonus || execution == RadarExecution::level1Arrival
        || execution == RadarExecution::level1Deployment;
}


bool level1Source(const RadarExecution execution) {
    return execution == RadarExecution::level1Source || combatExecution(execution);
}

bool bound(std::vector<NormalWorkOperation>& operations, const std::int32_t coordinate,
    const std::int16_t limit, const BoundSide side) {
    const bool lower = side == BoundSide::minimum;
    // MOV/CWD, high-word CMP/conditional branch, then second branch and low comparison.
    const auto high = coordinate >> 16;
    if ((lower && high < 0) || (!lower && high > 0)) {
        instructions(operations, 4);
        return false;
    }
    if (high != 0) {
        instructions(operations, 5);
        return true;
    }
    instructions(operations, 7);
    return lower ? coordinate >= limit : coordinate <= limit;
}
}

static RadarTraversalWork radarTraversal(const WorldViewSetupInput& view,
    const std::vector<RadarActorInput>& actors, const RadarTraversalProfile profile, const RadarExecution execution,
    Level1AnimationState* animation = nullptr, const Model3d* type11Model = nullptr,
    std::vector<std::optional<Level1AnimationState>>* animations = nullptr,
    const Model3d* guardianModel = nullptr,
    const Level1OrdinaryFamily ordinaryFamily=Level1OrdinaryFamily::type1,
    const Level2RadarDomain level2Domain=Level2RadarDomain::combat) {
    if (profile.beforeCallback != 0 || profile.afterCallback != 0 || actors.size() > 32767) {
        throw std::invalid_argument("unsupported radar traversal callbacks/list");
    }
    const auto periodic=execution==RadarExecution::level2Combat
        && level2Domain==Level2RadarDomain::periodicLifecycle;
    RadarTraversalWork result{pixelExecution(execution)
        ? radarViewSetupPixels(view) : radarViewSetupOperations(view), {}, {}};
    for (const auto& actor : actors) {
        const auto relativeX = static_cast<std::int64_t>(actor.actor.position.x) - view.origin.x;
        const auto relativeY = static_cast<std::int64_t>(actor.actor.position.y) - view.origin.y;
        const auto altitude = actor.actor.position.z;
        if (execution==RadarExecution::playablePixels) {
            if (!playableRadarActor(actor) || actor.actor.flags!=radarFlags
                || std::abs(std::int64_t{altitude})>maximumDeploymentEffectAltitude
                || std::abs(relativeX)>maximumRelativeCoordinate || std::abs(relativeY)>maximumRelativeCoordinate) {
                throw std::invalid_argument("invalid playable radar actor");
            }
            continue;
        }
        const bool departure = (execution == RadarExecution::nativePixels || execution == RadarExecution::level1Deployment)
            && actor.type == completionCraftType && actor.category == -1 && actor.callback == 0
            && actor.renderProfile == WorldActorRenderProfile::inlineSelectorDeparture;
        const bool ordinary = actor.renderProfile == WorldActorRenderProfile::noHandles
            && (actor.type == 8 || actor.type == 13 || actor.type == 30
                || (execution==RadarExecution::level2Combat && (actor.type==1 || actor.type==4)
                    && actor.category==-1 && actor.callback==0 && altitude==0)
                || (execution == RadarExecution::nativePixels
                    && (actor.type == level3OrdinaryType || actor.type == level3GroundType)
                    && actor.category == -1 && actor.callback == 0 && altitude == 0)
                || (execution == RadarExecution::nativePixels && (actor.type == 1 || actor.type == 4 || actor.type == 5)
                    && actor.category == -1)
                || (execution == RadarExecution::nativePixels && actor.type == guardianFixedType
                    && actor.category == -2 && actor.radius == guardianFixedRadius && actor.callback == 0)
                || (level1Source(execution) && actor.callback == 0 && altitude == 0
                    && ((actor.type == level1OrdinaryType && actor.category == -1 && actor.radius == level1OrdinaryRadius)
                        || (ordinaryFamily==Level1OrdinaryFamily::type1And3 && actor.type==3
                            && actor.category==-1 && actor.radius==770)
                        || (actor.type == level1FixedType && actor.category == -2 && actor.radius == level1FixedRadius))));
        const bool projectile = actor.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback
            && (actor.type == 19 || actor.type == 23
                || ((combatExecution(execution) || periodic) && ((actor.type == 17 && actor.radius == 441)
                    || (actor.type==20 && actor.radius==134)))
                || (execution == RadarExecution::level1Deployment && actor.type==22 && actor.radius==134)
                || (execution == RadarExecution::nativePixels && (actor.type == 17 || actor.type == 20
                    || (actor.type == 22 && actor.radius == 134))))
            && actor.category == -3;
        const bool airborneProjectile = (execution == RadarExecution::nativePixels || combatExecution(execution) || periodic)
            && projectile && actor.type == airborneProjectileType;
        const bool craft = (execution == RadarExecution::nativePixels || execution == RadarExecution::level1Arrival || execution == RadarExecution::level1Deployment)
            && actor.type == completionCraftType && actor.category == -1
            && ((actor.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback && actor.callback == 0)
                || (actor.renderProfile == WorldActorRenderProfile::inlineSelectorBeamCallback && actor.callback == completionBeamCallback)
                || departure);
        const bool deployedGuardian=execution==RadarExecution::level1Deployment && actor.type==44
            && actor.category==-1 && altitude==0 && actor.radius==2792
            && ((actor.callback==0x130c && actor.renderProfile==WorldActorRenderProfile::heapSelectorDeferredAnimation)
                || (actor.callback==0 && actor.renderProfile==WorldActorRenderProfile::heapSelectorNoCallback));
        const bool guardian = execution == RadarExecution::nativePixels
            && (actor.type == guardianType || actor.type == openGuardianType)
            && actor.category == -1 && altitude == 0
            && ((actor.radius == guardianRadius
                    && actor.renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation
                    && actor.callback == guardianAnimationCallback)
                || (actor.type == guardianType && actor.radius == level5GuardianRadius
                    && ((actor.renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation
                            && actor.callback == guardianAnimationCallback)
                        || (actor.renderProfile == WorldActorRenderProfile::heapSelectorNoCallback
                            && actor.callback == 0))));
        const bool animatedType11 = (execution == RadarExecution::nativePixels || level1Source(execution))
            && actor.type == type11 && actor.category == -1 && actor.radius == type11Radius
            && actor.renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation
            && actor.callback == guardianAnimationCallback;
        const bool level3Animated = execution == RadarExecution::nativePixels
            && actor.type == level3FixedAnimatedType && actor.category == -4 && altitude == 0
            && actor.renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation
            && actor.callback == guardianAnimationCallback;
        const bool level3Inline = execution == RadarExecution::nativePixels
            && actor.type == level3FixedInlineType && actor.category == -2 && altitude == 0
            && actor.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback
            && actor.callback == 0;
        const bool effect = (execution == RadarExecution::nativePixels || (execution == RadarExecution::level1CombatEffects || execution == RadarExecution::level1Bonus || execution == RadarExecution::level1Arrival || execution == RadarExecution::level1Deployment || execution == RadarExecution::level2Combat))
            && actor.renderProfile == WorldActorRenderProfile::bitmapEffectCallback
            && actor.type == -1 && actor.category == -4 && actor.radius == 0
            && (actor.callback == 0 || actor.callback == 0x0ac8);
        const bool inlineFamily = execution == RadarExecution::nativePixels
            && (actor.type == chasingActorType || actor.type == descendingActorType)
            && actor.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback
            && actor.category == -1 && actor.callback == 0 && actor.radius == inlineActorRadius;
        const bool pickup = (execution == RadarExecution::nativePixels || execution == RadarExecution::level1Deployment) && actor.type == completionPickupType
            && actor.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback
            && actor.category == -2 && actor.callback == 0 && actor.radius == completionPickupRadius && altitude == 0;
        const bool bonus = (execution == RadarExecution::nativePixels || execution == RadarExecution::level1Bonus || execution == RadarExecution::level1Arrival || execution == RadarExecution::level1Deployment)
            && actor.type == systemBonusType
            && actor.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback
            && actor.category == -2 && actor.callback == 0 && actor.radius == systemBonusRadius
            && altitude == systemBonusAltitude;
        const bool supportedAltitude = execution==RadarExecution::level1Deployment && effect
                ? std::abs(std::int64_t{altitude})<=maximumDeploymentEffectAltitude
            : execution==RadarExecution::level1Deployment && (craft || departure || deployedGuardian || actor.type==22)
                ? std::abs(std::int64_t{altitude})<=maximumRelativeCoordinate
            : animatedType11 && combatExecution(execution)
                ? altitude >= 0 && altitude <= maximumLiftAltitude
            : animatedType11 ? altitude >= type11MinimumAltitude
                && altitude <= maximumLiftAltitude && altitude % type11AltitudeStep == 0
            : effect && (execution == RadarExecution::level1CombatEffects || execution == RadarExecution::level1Bonus || execution == RadarExecution::level1Arrival || execution == RadarExecution::level1Deployment || execution == RadarExecution::level2Combat)
                ? altitude >= 0 && altitude <= maximumLiftAltitude
            : (inlineFamily || effect) ? altitude >= 0 && altitude <= inlineLaunchAltitude
                && altitude % inlineDescentStep == 0
            : bonus ? altitude == systemBonusAltitude
            : departure ? altitude >= 0 && altitude <= maximumRelativeCoordinate
            : craft ? altitude == completionCraftAltitude
            : altitude >= 0 && altitude <= maximumLiftAltitude
                && (altitude == 0 || actor.type == 13 || airborneProjectile);
        if (actor.actor.flags != radarFlags || !supportedAltitude || (actor.callback != 0 && !effect && !craft && !guardian && !deployedGuardian && !animatedType11 && !level3Animated)
            || (!ordinary && !projectile && !effect && !craft && !guardian && !deployedGuardian && !inlineFamily && !pickup && !animatedType11 && !bonus
                && !level3Animated && !level3Inline)
            || (actor.radius == 0 && !effect) || actor.radius >= 32768
            || relativeX < -maximumRelativeCoordinate || relativeX > maximumRelativeCoordinate
            || relativeY < -maximumRelativeCoordinate || relativeY > maximumRelativeCoordinate) {
            throw std::invalid_argument("unsupported stationary radar actor");
        }
    }

    instructions(result.operations, 8); // b996 prefix, including setup CALL.
    result.operations.insert(result.operations.end(), result.setup.operations.begin(), result.setup.operations.end());
    if (actors.empty()) {
        instructions(result.operations, 14); // Globals, empty-list jump, near epilogue.
        return result;
    }
    for (std::size_t index = 0; index < actors.size(); ++index) {
        const auto& actor = actors[index];
        const auto deployment=execution==RadarExecution::level1Deployment || execution==RadarExecution::playablePixels;
        const auto guardian=deployment && actor.type==44;
        const bool animated = level1Source(execution) && (actor.type==type11 || guardian) && actor.callback==0x130c;
        const auto beamCallback=deployment && actor.type==7 && actor.callback==0xb0b4;
        const bool effectCallback = (execution == RadarExecution::level1CombatEffects || execution == RadarExecution::level1Bonus || execution == RadarExecution::level1Arrival || execution == RadarExecution::level1Deployment || execution == RadarExecution::level2Combat)
            && actor.category == -4 && actor.callback == 0x0ac8;
        const FixedPosition relative{static_cast<std::int32_t>(static_cast<std::int64_t>(actor.actor.position.x) - view.origin.x),
            static_cast<std::int32_t>(static_cast<std::int64_t>(actor.actor.position.y) - view.origin.y),
            actor.actor.position.z - radarDepth};
        const auto center = transformToView(actor.actor.position, result.setup.view);
        const auto depth = center.y;
        constexpr std::uint32_t minimumTwoShiftDivisor = 0x20000;
        constexpr std::uint32_t maximumTwoShiftDivisor = 0x40000;
        if (!deployment && (depth < minimumTwoShiftDivisor || depth >= maximumTwoShiftDivisor
            || depth - actor.radius < minimumTwoShiftDivisor || depth - actor.radius >= maximumTwoShiftDivisor)) {
            throw std::invalid_argument("unsupported radar transformed depth");
        }
        RadarActorOutput output{center, center, radarFlags, actor.projectedX, actor.projectedY,
            actor.projectedRadius, false};
        instructions(result.operations, index == 0 ? 47 : 39);
        instructions(result.operations, vectorTransformDispatches(relative, result.setup.view.matrix));
        constexpr std::uint64_t depthPrefixAndRadiusCall = 27;
        const auto rawDepth=std::int64_t{depth}-actor.radius;
        const auto divisor=static_cast<std::uint32_t>(std::max<std::int64_t>(128,rawDepth));
        const auto projectionDivide=[deployment](const std::int32_t coordinate,const std::uint32_t denominator) {
            if (!deployment) { return divide(coordinate,denominator); }
            if (denominator==0 || denominator>2000000 || std::abs(std::int64_t{coordinate})>2000000) {
                throw std::invalid_argument("unsupported deployment radar division");
            }
            auto magnitude=static_cast<std::uint64_t>(std::abs(std::int64_t{coordinate})*128);
            auto divisor=denominator;unsigned shifts=0;
            while (divisor>65535) { magnitude>>=1;divisor>>=1;++shifts; }
            const auto work=shifts ? 52+6*shifts
                : 53+(static_cast<std::int16_t>(divisor)<=static_cast<int>(magnitude>>16) ? 3 : 0);
            return Division{static_cast<std::int32_t>((coordinate<0 ? -1 : 1)*static_cast<std::int64_t>(magnitude/divisor)),
                work+(coordinate<0 ? 18U : 0U)};
        };
        instructions(result.operations,deployment ? 22+(rawDepth>=65536 ? 0 : rawDepth<0 ? 4 : rawDepth<128 ? 6 : 3)+5 : depthPrefixAndRadiusCall);
        auto radius = projectionDivide(static_cast<std::int32_t>(actor.radius), divisor);
        instructions(result.operations, 58 + radius.dispatches); // 4b57 shift7 wrapper.
        instructions(result.operations, 16+(deployment && radius.value>256 ? 2 : 0)); // Clamp checks, flags8 and cache-copy setup.
        result.operations.push_back({NormalWorkKind::repeatString, 6});
        if (deployment && depth<128) {
            instructions(result.operations,depth<0 ? 6 : 9);
        } else {
        instructions(result.operations,deployment && depth<65536 ? 13 : 11); //Clearbit4, near check and projection CALL.
        const auto x = projectionDivide(center.x, depth);
        const auto z = projectionDivide(center.z, depth);
        instructions(result.operations, 52 + x.dispatches);
        instructions(result.operations, 50 + z.dispatches + 6); //4abf second numerator and epilogue.
        const auto midpointX = view.viewport.left + (view.viewport.right - view.viewport.left) / 2;
        const auto midpointY = view.viewport.top + (view.viewport.bottom - view.viewport.top) / 2;
        const auto projectedX = midpointX + x.value;
        const auto projectedY = midpointY - z.value;
        instructions(result.operations, 11);
        if (bound(result.operations, projectedX, view.viewport.left, BoundSide::minimum)
            && bound(result.operations, projectedX, view.viewport.right, BoundSide::maximum)
            && bound(result.operations, projectedY, view.viewport.top, BoundSide::minimum)
            && bound(result.operations, projectedY, view.viewport.bottom, BoundSide::maximum)) {
            instructions(result.operations, 8);
            output.flags |= 4;
            output.projectedX = static_cast<std::int16_t>(projectedX);
            output.projectedY = static_cast<std::int16_t>(projectedY);
            output.projectedRadius = static_cast<std::uint16_t>(deployment ? std::min(256,radius.value) : radius.value);
            output.projected = true;
        }
        }
        instructions(result.operations, animated || effectCallback || beamCallback ? 7 : 5); // Pre-model callback guard and model CALL.
        instructions(result.operations, 14); // cb7a flags80 early return.
        if (animated) {
            auto* actorAnimation = animations ? &animations->at(index).value() : animation;
            if (actorAnimation == nullptr || type11Model == nullptr || (guardian && guardianModel==nullptr)) {
                throw std::invalid_argument("missing Level1 radar animation state");
            }
            instructions(result.operations, 9); // BBEE..BC03: deferred callback guard, arguments and CALL.
            Level1AnimationWork callback;
            if (guardian) {
                const auto child=level1GuardianAnimationOperations(*actorAnimation,*guardianModel);
                callback={child.state,child.operations};
            } else { callback=level1AnimationOperations(*actorAnimation,*type11Model); }
            result.operations.insert(result.operations.end(),callback.operations.begin(),callback.operations.end());
            *actorAnimation=callback.state;
            instructions(result.operations, index + 1 == actors.size() ? 13 : 7);
        } else if (beamCallback) {
            instructions(result.operations,9+12); //Deferred B0B4; radar-pass branch, no RNG or raster.
            instructions(result.operations,index+1==actors.size() ? 13 : 7);
        } else if (effectCallback) {
            instructions(result.operations, 9); //BBEE..BC03 deferred callback CALL.
            instructions(result.operations, 13); //0AC8..0AD7 radar-pass branch;0BC2..0BC7 epilogue.
            instructions(result.operations, index + 1 == actors.size() ? 13 : 7);
        } else {
            instructions(result.operations, index + 1 == actors.size() ? 16 : 10);
        }
        result.actors.push_back(output);
    }
    return result;
}
static RadarWork radarDraw(const RadarRetainedState& state, const WorldViewSetupInput& view,
    const FixedPosition playerPosition, const std::uint16_t playerYaw,
    const std::vector<RadarActorInput>& actors, const RadarTraversalProfile profile,
    const IndexedBitmap& panel, const RadarMarkerColors colors, const RadarExecution execution,
    Level1AnimationState* animation = nullptr, const Model3d* type11Model = nullptr,
    std::vector<std::optional<Level1AnimationState>>* animations = nullptr,
    const Model3d* guardianModel = nullptr,
    const Level1OrdinaryFamily ordinaryFamily=Level1OrdinaryFamily::type1,
    const Level2RadarDomain level2Domain=Level2RadarDomain::combat,
    const RadarPageLayout layout=RadarPageLayout::compatibility) {
    const auto lifecycle=level2Domain==Level2RadarDomain::playerLifecycle
        || level2Domain==Level2RadarDomain::periodicLifecycle;
    if((level2Domain!=Level2RadarDomain::combat && !lifecycle)
        || (lifecycle && execution!=RadarExecution::level2Combat)) {
        throw std::invalid_argument("signed Level2 HUD requires its explicit source roster");
    }
    const auto retainedPages = level1Source(execution) || execution == RadarExecution::level2Constructor
        || execution == RadarExecution::level2Combat
        || (pixelExecution(execution) && layout == RadarPageLayout::recoveredStartup);
    const std::uint16_t cachedPage = retainedPages ? 0xa400 : 0xa800;
    constexpr std::uint16_t firstPage = 0xa000;
    const std::uint16_t secondPage = retainedPages ? 0xa800 : 0xa400;
    constexpr std::uint16_t panelWidth = 120;
    constexpr std::uint16_t panelHeight = 56;
    constexpr ProjectedPoint panelPosition{96, 127};
    constexpr Viewport markerViewport{112, 206, 146, 179};
    constexpr std::int16_t maximumArmor = 680;
    constexpr std::int16_t minimumLethalArmor = -128;
    constexpr std::int16_t minimumCombatArmor = -320;
    const auto minimumArmor = lifecycle ? -level2MaximumFrameDamage : combatExecution(execution) ? minimumCombatArmor
        : execution == RadarExecution::playablePixels ? minimumCombatArmor
        : execution == RadarExecution::nativePixels ? minimumLethalArmor : 0;
    constexpr std::int16_t armorUnitsPerColumn = 16;
    constexpr std::int16_t armorFirstColumn = 112;
    constexpr std::int16_t armorColumnSpacing = 2;
    constexpr std::int16_t armorTop = 189;
    constexpr std::uint16_t armorHeight = 7;
    constexpr RadarMarkerColors sourceColors{228, 138};
    const auto displayPage = [firstPage, secondPage](const std::uint16_t page) {
        return page == firstPage || page == secondPage;
    };
    if (colors.ordinary != sourceColors.ordinary || colors.fixed != sourceColors.fixed
        || colors.projectile != sourceColors.projectile || colors.effect != sourceColors.effect
        || (!pixelExecution(execution) && (playerYaw & 0x000fU) != 0)
        || !state.armorStrip || state.armor > maximumArmor
        || state.armor < minimumArmor
        || *state.armorStrip < minimumArmor / armorUnitsPerColumn
        || *state.armorStrip > maximumArmor / armorUnitsPerColumn || state.pages.cached != cachedPage
        || !displayPage(state.pages.back) || !displayPage(state.pages.front)
        || state.pages.back == state.pages.front || !displayPage(state.pages.draw)
        || (state.pages.copy != cachedPage && !displayPage(state.pages.copy))
        || panel.width != panelWidth || panel.height != panelHeight
        || panel.pixels.size() != static_cast<std::size_t>(panelWidth) * panelHeight) {
        throw std::invalid_argument("unsupported radar armor profile");
    }
    auto temporary = actors;
    for (auto& actor : temporary) {
        if (execution==RadarExecution::playablePixels) {
            if (!playableRadarActor(actor) || (actor.actor.flags & ~0x8cU)!=0) {
                throw std::invalid_argument("invalid playable radar flags/profile");
            }
            actor.actor.flags=radarFlags;
            continue;
        }
        const bool effect = (execution == RadarExecution::nativePixels || (execution == RadarExecution::level1CombatEffects || execution == RadarExecution::level1Bonus || execution == RadarExecution::level1Arrival || execution == RadarExecution::level1Deployment || execution == RadarExecution::level2Combat))
            && actor.renderProfile == WorldActorRenderProfile::bitmapEffectCallback && actor.category == -4
            && (actor.actor.flags == 0x88 || actor.actor.flags == 0x8c);
        const bool craft = (execution == RadarExecution::nativePixels || execution == RadarExecution::level1Deployment) && actor.type == completionCraftType
            && actor.category == -1
            && ((actor.renderProfile == WorldActorRenderProfile::inlineSelectorBeamCallback
                    && actor.callback == completionBeamCallback && (actor.actor.flags == 8 || actor.actor.flags == 12))
                || ((actor.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback
                        || actor.renderProfile == WorldActorRenderProfile::inlineSelectorDeparture)
                    && actor.callback == 0 && (actor.actor.flags == 0 || actor.actor.flags == 4)));
        const bool level3Animated = execution == RadarExecution::nativePixels
            && actor.type == level3FixedAnimatedType && actor.category == -4
            && actor.renderProfile == WorldActorRenderProfile::heapSelectorDeferredAnimation
            && actor.callback == guardianAnimationCallback;
        if (!effect && !craft && !level3Animated
            && ((execution==RadarExecution::level2Combat ? actor.actor.flags!=0 && actor.actor.flags!=4 : actor.actor.flags!=0)
                || (actor.category != -1 && actor.category != -2 && actor.category != -3))) {
            throw std::invalid_argument("unsupported radar actor flags/category");
        }
        if (level3Animated && actor.actor.flags != 0) {
            throw std::invalid_argument("unsupported radar actor flags/category");
        }
        actor.actor.flags = radarFlags;
    }
    auto radarView = view;
    radarView.origin.x = playerPosition.x;
    radarView.origin.y = playerPosition.y;
    radarView.actorAngles[2] = playerYaw;
    const auto traversal = radarTraversal(radarView, temporary, profile, execution, animation, type11Model, animations,guardianModel,ordinaryFamily,level2Domain);
    RadarWork result{traversal.setup, traversal.actors, {}, {}, state, markerViewport,
        1, 0, 0xe8, 0xe8, 0};
    const auto append = [&result](const std::vector<NormalWorkOperation>& operations) {
        result.operations.insert(result.operations.end(), operations.begin(), operations.end());
    };
    // Caller near CALL, camera copy, save-flags loop, panel CALLF and JMPF stub.
    instructions(result.operations, 25 + (actors.empty() ? 0 : 1 + actors.size() * 10) + 8);
    append(planarBitmapOperations(panel, panelPosition.x, PlanarBlitMode::opaque));
    for (std::uint16_t y = 0; y < panel.height; ++y) {
        for (std::uint16_t x = 0; x < panel.width; ++x) {
            result.pixels.push_back({{static_cast<std::int16_t>(panelPosition.x + x),
                static_cast<std::int16_t>(panelPosition.y + y)},
                panel.pixels[static_cast<std::size_t>(y) * panel.width + x], state.pages.draw});
        }
    }
    instructions(result.operations, 7); // Panel cleanup, radar-pass flag and b996 CALL.
    append(traversal.operations);
    instructions(result.operations, 12 + (actors.empty() ? 0 : 1));
    for (std::size_t index = 0; index < actors.size(); ++index) {
        const auto& actor = actors[index];
        auto& output = result.actors[index];
        const bool ordinary = actor.category == -1;
        if (ordinary) {
            result.enemyFlag = 0;
        }
        instructions(result.operations, ordinary ? 7 : 6);
        if (output.projected) {
            constexpr std::int16_t fixedCategory = -2;
            constexpr std::uint64_t markerCaller = 15;
            constexpr std::uint64_t fixedTypeGate = 2; //6A6E skips6A70/6A74 for FD.
            constexpr std::uint64_t pickupColorDispatches = 3; //6A76 MOV AL,[12EA];6A79 CBW;6A7A MOV CX,AX.
            instructions(result.operations, markerCaller + (actor.category == fixedCategory ? fixedTypeGate : 0)
                + (actor.category == fixedCategory && actor.type == completionPickupType ? pickupColorDispatches : 0));
            const auto marker = rasterPointOperations(
                {static_cast<std::int16_t>(output.projectedX - 4), output.projectedY}, markerViewport);
            append(marker.operations);
            for (const auto pixel : marker.pixels) {
                result.pixels.push_back({pixel, ordinary ? colors.ordinary : actor.category == -3 ? colors.projectile
                    : actor.category == -4 ? colors.effect
                    : actor.type == completionPickupType ? completionPickupColor : colors.fixed, state.pages.draw});
            }
            instructions(result.operations, 1);
        }
        output.flags = actor.actor.flags;
        output.projectedY = 0;
        instructions(result.operations, 7); // Restore flags, clearY and advance ordered list.
    }
    //698a uses SAR DI,4; negative health rounds down rather than toward zero.
    const auto strip = static_cast<std::int16_t>(state.armor >= 0 ? state.armor / armorUnitsPerColumn
        : -((-static_cast<int>(state.armor) + armorUnitsPerColumn - 1) / armorUnitsPerColumn));
    if (*state.armorStrip == strip) {
        instructions(result.operations, 16);
        result.state.armorStrip = strip;
        return result;
    }
    const bool erasing = strip < *state.armorStrip;
    const auto minimum = erasing ? strip : *state.armorStrip;
    const auto maximum = erasing ? *state.armorStrip : strip;
    constexpr std::uint8_t armorColor = 239;
    result.drawColor = erasing ? 0 : armorColor;
    // Shared tail through takenJNZ, choose range/color, then loop setup.
    instructions(result.operations, 9 + 2 + (erasing ? 4 : 5) + 4 + 10);
    const auto firstColumn = static_cast<std::int16_t>(armorFirstColumn + armorColumnSpacing * minimum);
    for (auto column = minimum; column <= maximum; ++column) {
        const auto x = static_cast<std::int16_t>(armorFirstColumn + armorColumnSpacing * column);
        instructions(result.operations, 7);
        if (pixelExecution(execution) && x < armorFirstColumn) {
            for (std::int16_t y = armorTop; y < armorTop + armorHeight; ++y) {
                result.pixels.push_back({{x, y}, result.drawColor, state.pages.draw});
            }
            continue;
        }
        const auto line = lifecycle ? rasterLevel2LifecycleArmorColumnOperations(x) : combatExecution(execution)
            ? rasterCombatArmorColumnOperations(x) : rasterArmorColumnOperations(x);
        append(line.operations);
        for (const auto pixel : line.pixels) {
            result.pixels.push_back({pixel, result.drawColor, state.pages.draw});
        }
        instructions(result.operations, 5);
    }
    result.state.pages.copy = state.pages.back;
    result.state.pages.draw = state.pages.front;
    const auto width = static_cast<std::uint16_t>(armorColumnSpacing * (maximum - minimum + 3));
    instructions(result.operations, 1 + 18 + 1); // SavedDI, page/copy CALLF,22a1 JMPF.
    append(planarCopyOperations(static_cast<std::uint16_t>(firstColumn), width, armorHeight));
    const auto groups = planarCopyWork(static_cast<std::uint16_t>(firstColumn), width, armorHeight).stringIterations / armorHeight;
    result.pageCopy = RadarPageCopy{state.pages.back, state.pages.front,
        {static_cast<std::int16_t>((firstColumn / 4) * 4), armorTop}, static_cast<std::uint16_t>(groups * 4), armorHeight};
    instructions(result.operations, 1 + 6); // Argument cleanup, retained strip store and RET.
    result.state.armorStrip = strip;
    return result;
}
RadarTraversalWork radarTraversalOperations(const WorldViewSetupInput& view,
    const std::vector<RadarActorInput>& actors, const RadarTraversalProfile profile) {
    return radarTraversal(view, actors, profile, RadarExecution::sourceWork);
}

RadarWork radarOperations(const RadarRetainedState& state, const WorldViewSetupInput& view,
    const FixedPosition playerPosition, const std::uint16_t playerYaw, const std::vector<RadarActorInput>& actors,
    const RadarTraversalProfile profile, const IndexedBitmap& panel, const RadarMarkerColors colors) {
    return radarDraw(state, view, playerPosition, playerYaw, actors, profile, panel, colors, RadarExecution::sourceWork);
}

RadarWork radarPresentationPixels(const RadarRetainedState& state, const WorldViewSetupInput& view,
    const FixedPosition playerPosition, const std::uint16_t playerYaw, const std::vector<RadarActorInput>& actors,
    const RadarTraversalProfile profile, const IndexedBitmap& panel, const RadarMarkerColors colors,
    const RadarPageLayout layout, const RadarPixelDomain domain) {
    if (layout!=RadarPageLayout::compatibility && layout!=RadarPageLayout::recoveredStartup) {
        throw std::invalid_argument("invalid native radar page layout");
    }
    if (domain!=RadarPixelDomain::retained && domain!=RadarPixelDomain::playableCampaign) {
        throw std::invalid_argument("invalid native radar pixel domain");
    }
    auto result = radarDraw(state, view, playerPosition, playerYaw, actors, profile, panel, colors,
        domain==RadarPixelDomain::playableCampaign ? RadarExecution::playablePixels : RadarExecution::nativePixels,
        nullptr,nullptr,nullptr,nullptr,Level1OrdinaryFamily::type1,
        Level2RadarDomain::combat,layout);
    result.operations.clear();
    result.setup.operations.clear();
    result.cpuWorkQualified = false;
    return result;
}

RadarWork level1RadarInitializationOperations(const RadarRetainedState& state,
    const WorldViewSetupInput& view, const FixedPosition playerPosition, const std::uint16_t playerYaw,
    const IndexedBitmap& panel, const RadarMarkerColors colors) {
    if (playerYaw != 0) {
        throw std::invalid_argument("unsupported initial Level1 radar yaw");
    }
    return radarDraw(state, view, playerPosition, playerYaw, {}, {}, panel, colors, RadarExecution::level1Source);
}

namespace {
enum class Level1RadarList { entities, primaryProjectile, combatProjectiles };

Level1RadarWork level1Radar(const RadarRetainedState& state, const WorldViewSetupInput& view,
    const FixedPosition playerPosition, const std::uint16_t playerYaw, const std::vector<RadarActorInput>& actors,
    const RadarTraversalProfile profile, const IndexedBitmap& panel, const RadarMarkerColors colors,
    const Model3d& type11Model, Level1AnimationState animation, const Level1RadarList list) {
    std::size_t ordinary = 0;
    std::size_t animated = 0;
    std::size_t fixed = 0;
    std::size_t projectiles = 0;
    for (const auto& actor : actors) {
        if (actor.type == level1OrdinaryType) {
            ++ordinary;
        } else if (actor.type == type11 && (actor.actor.position.z == maximumLiftAltitude
            || (list == Level1RadarList::combatProjectiles
                && actor.actor.position.z >= 0 && actor.actor.position.z <= maximumLiftAltitude))) {
            ++animated;
        } else if (actor.type == level1FixedType) {
            ++fixed;
        } else if (list != Level1RadarList::entities && actor.category == -3
            && ((actor.type == 23 && actor.radius == 134)
                || (list == Level1RadarList::combatProjectiles && actor.type == 17 && actor.radius == 441))
            && actor.actor.position.z == 0 && actor.callback == 0
            && actor.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback) {
            ++projectiles;
        } else {
            throw std::invalid_argument("unsupported initial Level1 radar list");
        }
    }
    if (ordinary != 1 || animated != 1 || fixed != 4 || (playerYaw & 15U) != 0
        || (list == Level1RadarList::entities ? projectiles != 0
            : projectiles > Level1State::projectilePoolSize
                || (list == Level1RadarList::primaryProjectile && projectiles == 0))) {
        throw std::invalid_argument("unsupported initial Level1 radar composition");
    }
    auto radar = radarDraw(state, view, playerPosition, playerYaw, actors, profile,
        panel, colors, list == Level1RadarList::combatProjectiles ? RadarExecution::level1Combat
            : RadarExecution::level1Source, &animation, &type11Model);
    return {std::move(radar), animation};
}

}

Level1RadarWork level1RadarOperations(const RadarRetainedState& state, const WorldViewSetupInput& view,
    const FixedPosition playerPosition, const std::uint16_t playerYaw, const std::vector<RadarActorInput>& actors,
    const RadarTraversalProfile profile, const IndexedBitmap& panel, const RadarMarkerColors colors,
    const Model3d& type11Model, const Level1AnimationState animation) {
    return level1Radar(state, view, playerPosition, playerYaw, actors, profile, panel, colors,
        type11Model, animation, Level1RadarList::entities);
}

Level1RadarWork level1ProjectileRadarOperations(const RadarRetainedState& state, const WorldViewSetupInput& view,
    const FixedPosition playerPosition, const std::uint16_t playerYaw, const std::vector<RadarActorInput>& actors,
    const RadarTraversalProfile profile, const IndexedBitmap& panel, const RadarMarkerColors colors,
    const Model3d& type11Model, const Level1AnimationState animation) {
    return level1Radar(state, view, playerPosition, playerYaw, actors, profile, panel, colors,
        type11Model, animation, Level1RadarList::primaryProjectile);
}

Level1RadarWork level1CombatRadarOperations(const RadarRetainedState& state, const WorldViewSetupInput& view,
    const FixedPosition playerPosition, const std::uint16_t playerYaw, const std::vector<RadarActorInput>& actors,
    const RadarTraversalProfile profile, const IndexedBitmap& panel, const RadarMarkerColors colors,
    const Model3d& type11Model, const Level1AnimationState animation) {
    return level1Radar(state, view, playerPosition, playerYaw, actors, profile, panel, colors,
        type11Model, animation, Level1RadarList::combatProjectiles);
}


Level1DynamicRadarWork level1DynamicCombatRadarOperations(const RadarRetainedState& state,
    const WorldViewSetupInput& view, const FixedPosition playerPosition, const std::uint16_t playerYaw,
    const std::vector<RadarActorInput>& actors, const RadarTraversalProfile profile,
    const IndexedBitmap& panel, const RadarMarkerColors colors, const Model3d& type11Model,
    std::vector<std::optional<Level1AnimationState>> animations) {
    if (animations.size() != actors.size() || (playerYaw & 15U) != 0) {
        throw std::invalid_argument("dynamic radar requires ordered callback ownership and source yaw");
    }
    std::size_t ordinary = 0, fixed = 0, projectiles = 0;
    for (std::size_t index = 0; index < actors.size(); ++index) {
        const auto& actor = actors[index];
        if (animations[index].has_value() != (actor.type == type11)) {
            throw std::invalid_argument("dynamic radar callback must belong to its Type11 actor");
        }
        if (actor.type == 1 || actor.type == type11) {
            ++ordinary;
        } else if (actor.type == level1FixedType) {
            ++fixed;
        } else if (actor.type == 17 || actor.type == 20 || actor.type == 23) {
            ++projectiles;
        } else {
            throw std::invalid_argument("unsupported dynamic radar actor type");
        }
    }
    if (ordinary == 0 || ordinary > 10 || fixed != 4 || projectiles > Level1State::projectilePoolSize) {
        throw std::invalid_argument("unsupported dynamic radar composition");
    }
    auto radar = radarDraw(state, view, playerPosition, playerYaw, actors, profile, panel, colors,
        RadarExecution::level1Combat, nullptr, &type11Model, &animations);
    return {std::move(radar), std::move(animations)};
}


static Level1DynamicRadarWork combatEffectRadarOperations(const RadarRetainedState& state,
    const WorldViewSetupInput& view, const FixedPosition playerPosition, const std::uint16_t playerYaw,
    const std::vector<RadarActorInput>& actors, const RadarTraversalProfile profile,
    const IndexedBitmap& panel, const RadarMarkerColors colors, const Model3d& type11Model,
    std::vector<std::optional<Level1AnimationState>> animations, const bool bonus,const bool arrival=false,const Model3d* guardianModel=nullptr,
    const Level1OrdinaryFamily ordinaryFamily=Level1OrdinaryFamily::type1) {
    if (animations.size() != actors.size() || (playerYaw & 15U) != 0 || actors.size() > 32) {
        throw std::invalid_argument("combat effect radar requires ordered source callback ownership");
    }
    const auto deployment=guardianModel!=nullptr;
    std::size_t ordinary = 0, fixed = 0, projectiles = 0, effects = 0, bonuses = 0, crafts = 0, guardians=0, rewards=0;
    for (std::size_t index = 0; index < actors.size(); ++index) {
        const auto& actor = actors[index];
        if (animations[index].has_value() != (actor.type==type11 || (deployment && actor.type==44))) {
            throw std::invalid_argument("combat effect radar callback must belong to its Type11 actor");
        }
        if (actor.type == 1 || actor.type == type11
            || (ordinaryFamily==Level1OrdinaryFamily::type1And3 && actor.type==3)) {
            ++ordinary;
        } else if (actor.type == level1FixedType) {
            ++fixed;
        } else if (actor.type==17 || actor.type==20 || actor.type==23 || (deployment && actor.type==22)) {
            ++projectiles;
        } else if (deployment && actor.type==44 && actor.category==-1 && actor.radius==2792
            && actor.actor.position.z==0 && (actor.callback==0 || actor.callback==0x130c)) {
            ++guardians;
        } else if (arrival && actor.type==completionCraftType && actor.category==-1
            && actor.radius==13052 && (deployment || (actor.actor.position.z==5000 && actor.callback==0))
            && (actor.renderProfile==WorldActorRenderProfile::inlineSelectorNoCallback
                || (deployment && (actor.renderProfile==WorldActorRenderProfile::inlineSelectorBeamCallback
                    || actor.renderProfile==WorldActorRenderProfile::inlineSelectorDeparture)))) {
            ++crafts;
        } else if (deployment && actor.type==completionPickupType && actor.category==-2
            && actor.radius==completionPickupRadius && actor.actor.position.z==0 && actor.callback==0
            && actor.renderProfile==WorldActorRenderProfile::inlineSelectorNoCallback) {
            ++rewards;
        } else if (bonus && actor.type == systemBonusType && actor.category == -2 && actor.radius == systemBonusRadius
            && actor.actor.position.z == systemBonusAltitude && actor.callback == 0
            && actor.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback) {
            ++bonuses;
        } else if (actor.type == -1 && actor.category == -4 && actor.radius == 0
            && actor.renderProfile == WorldActorRenderProfile::bitmapEffectCallback
            && (actor.callback == 0 || actor.callback == 0x0ac8)) {
            ++effects;
        } else {
            throw std::invalid_argument("unsupported live combat effect radar actor");
        }
    }
    if (ordinary > 10 || fixed > 4 || projectiles > Level1State::projectilePoolSize
        || effects > Level1State::deathEffectPoolSize || bonuses > (bonus ? 1U : 0U) || crafts>(arrival ? 1U : 0U) || guardians>(deployment ? 1U : 0U) || rewards>(deployment ? 1U : 0U)) {
        throw std::invalid_argument("unsupported live combat effect radar membership");
    }
    auto radar = radarDraw(state, view, playerPosition, playerYaw, actors, profile, panel, colors,
        deployment ? RadarExecution::level1Deployment : arrival ? RadarExecution::level1Arrival : bonus ? RadarExecution::level1Bonus : RadarExecution::level1CombatEffects, nullptr, &type11Model, &animations,guardianModel,ordinaryFamily);
    return {std::move(radar),std::move(animations)};
}

Level1DynamicRadarWork level1Type3RadarOperations(const RadarRetainedState& state,
    const WorldViewSetupInput& view,const FixedPosition playerPosition,const std::uint16_t playerYaw,
    const std::vector<RadarActorInput>& actors,const RadarTraversalProfile profile,
    const IndexedBitmap& panel,const RadarMarkerColors colors,const Model3d& type11Model,
    const Model3d* guardianModel,std::vector<std::optional<Level1AnimationState>> animations,
    const Level1OrdinaryRenderFamily family) {
    if ((family!=Level1OrdinaryRenderFamily::combatEffects && family!=Level1OrdinaryRenderFamily::bonus
            && family!=Level1OrdinaryRenderFamily::arrival && family!=Level1OrdinaryRenderFamily::deployment)
        || ((family==Level1OrdinaryRenderFamily::deployment)!=(guardianModel!=nullptr))) {
        throw std::invalid_argument("invalid Type3 mixed radar family");
    }
    if (guardianModel && (guardianModel->flags!=3 || guardianModel->collisionRadius!=2792
        || guardianModel->animationLimits!=std::vector<std::uint8_t>{8,8,8})) {
        throw std::invalid_argument("Type3 deployment radar requires its actual guardian model");
    }
    return combatEffectRadarOperations(state,view,playerPosition,playerYaw,actors,profile,panel,colors,
        type11Model,std::move(animations),family!=Level1OrdinaryRenderFamily::combatEffects,
        family==Level1OrdinaryRenderFamily::arrival || family==Level1OrdinaryRenderFamily::deployment,
        guardianModel,Level1OrdinaryFamily::type1And3);
}

Level1DynamicRadarWork level1CombatEffectRadarOperations(const RadarRetainedState& state,
    const WorldViewSetupInput& view, const FixedPosition playerPosition, const std::uint16_t playerYaw,
    const std::vector<RadarActorInput>& actors, const RadarTraversalProfile profile,
    const IndexedBitmap& panel, const RadarMarkerColors colors, const Model3d& type11Model,
    std::vector<std::optional<Level1AnimationState>> animations) {
    return combatEffectRadarOperations(state,view,playerPosition,playerYaw,actors,profile,panel,colors,
        type11Model,std::move(animations),false);
}

Level1DynamicRadarWork level1BonusCombatRadarOperations(const RadarRetainedState& state,
    const WorldViewSetupInput& view, const FixedPosition playerPosition, const std::uint16_t playerYaw,
    const std::vector<RadarActorInput>& actors, const RadarTraversalProfile profile,
    const IndexedBitmap& panel, const RadarMarkerColors colors, const Model3d& type11Model,
    std::vector<std::optional<Level1AnimationState>> animations) {
    return combatEffectRadarOperations(state,view,playerPosition,playerYaw,actors,profile,panel,colors,
        type11Model,std::move(animations),true);
}

Level1DynamicRadarWork level1ArrivalCombatRadarOperations(const RadarRetainedState& state,
    const WorldViewSetupInput& view, const FixedPosition playerPosition, const std::uint16_t playerYaw,
    const std::vector<RadarActorInput>& actors, const RadarTraversalProfile profile,
    const IndexedBitmap& panel, const RadarMarkerColors colors, const Model3d& type11Model,
    std::vector<std::optional<Level1AnimationState>> animations) {
    return combatEffectRadarOperations(state,view,playerPosition,playerYaw,actors,profile,panel,colors,
        type11Model,std::move(animations),true,true);
}

Level1DynamicRadarWork level1DeploymentCombatRadarOperations(const RadarRetainedState& state,
    const WorldViewSetupInput& view,const FixedPosition playerPosition,const std::uint16_t playerYaw,
    const std::vector<RadarActorInput>& actors,const RadarTraversalProfile profile,
    const IndexedBitmap& panel,const RadarMarkerColors colors,const Model3d& type11Model,
    const Model3d& guardianModel,std::vector<std::optional<Level1AnimationState>> animations) {
    if (guardianModel.flags!=3 || guardianModel.collisionRadius!=2792
        || guardianModel.animationLimits!=std::vector<std::uint8_t>{8,8,8}) {
        throw std::invalid_argument("deployment radar requires Level1 guardian metadata");
    }
    for (std::size_t index=0;index<actors.size();++index) {
        if (actors[index].type!=44) { continue; }
        if (index>=animations.size() || !animations[index] || animations[index]->variant>=8
            || animations[index]->countdown>2 || (animations[index]->reload!=1 && animations[index]->reload!=2)) {
            throw std::invalid_argument("invalid retained guardian radar selector");
        }
    }
    return combatEffectRadarOperations(state,view,playerPosition,playerYaw,actors,profile,panel,colors,
        type11Model,std::move(animations),true,true,&guardianModel);
}

}


namespace stellar7 {
RadarWork level2ConstructorRadarOperations(const RadarRetainedState& state,
    const WorldViewSetupInput& view, const FixedPosition playerPosition, const std::uint16_t playerYaw,
    const std::vector<RadarActorInput>& actors, const RadarTraversalProfile profile,
    const IndexedBitmap& panel, const RadarMarkerColors colors) {
    std::array<std::size_t,3> counts{};
    for(const auto& actor:actors) {
        const auto tank=actor.type==8;const auto lift=actor.type==13;const auto fixed=actor.type==30;
        if((!tank && !lift && !fixed) || actor.category!=(fixed ? -2 : -1)
            || actor.radius!=(tank ? 860U : lift ? 559U : 868U) || actor.callback!=0
            || actor.renderProfile!=WorldActorRenderProfile::noHandles) {
            throw std::invalid_argument("Level2 constructor radar requires actual Type8/13/30 records");
        }
        ++counts[tank ? 0 : lift ? 1 : 2];
    }
    if(counts!=std::array<std::size_t,3>{3,2,2}) {
        throw std::invalid_argument("Level2 constructor radar requires its seven source allocations");
    }
    return radarDraw(state,view,playerPosition,playerYaw,actors,profile,panel,colors,RadarExecution::level2Constructor);
}
} // namespace stellar7


namespace stellar7 {
RadarWork level2CombatRadarOperations(const RadarRetainedState& state,
    const WorldViewSetupInput& view, const FixedPosition playerPosition, const std::uint16_t playerYaw,
    const std::vector<RadarActorInput>& actors, const RadarTraversalProfile profile,
    const IndexedBitmap& panel, const RadarMarkerColors colors, const Level2RadarDomain domain) {
    const auto periodic=domain==Level2RadarDomain::periodicLifecycle;
    std::array<std::size_t,5> counts{};
    for(const auto& actor:actors) {
        const auto tank=actor.type==8 || (periodic && (actor.type==1 || actor.type==4));const auto lift=actor.type==13;const auto fixed=actor.type==30;
        const auto primary=actor.type==23 || actor.type==19 || (periodic && (actor.type==17 || actor.type==20));const auto effect=actor.type==-1;
        if(!tank && !lift && !fixed && !primary && !effect) {
            throw std::invalid_argument("Level2 combat radar excludes another actor family");
        }
        ++counts[tank?0:lift?1:fixed?2:primary?3:4];
        const auto category=effect?-4:primary?-3:fixed?-2:-1;
        const auto radius=actor.type==1?1342U:actor.type==4?928U:tank?860U:lift?559U:fixed?868U
            :actor.type==19 || actor.type==17?441U:primary?134U:0U;
        const auto renderProfile=effect?WorldActorRenderProfile::bitmapEffectCallback
            :primary?WorldActorRenderProfile::inlineSelectorNoCallback:WorldActorRenderProfile::noHandles;
        if(actor.category!=category || actor.radius!=radius || actor.renderProfile!=renderProfile
            || (effect ? actor.callback!=0 && actor.callback!=0x0ac8 : actor.callback!=0)) {
            throw std::invalid_argument("Level2 combat radar lost source category/callback metadata");
        }
    }
    if((periodic ? counts[0]+counts[1]>16 : counts[0]>3) || counts[1]>(periodic ? maximumLevel2PeriodicLiveType13 : 2) || counts[2]>2 || counts[3]>Level1State::projectilePoolSize
        || counts[4]>Level1State::deathEffectPoolSize) {
        throw std::invalid_argument("Level2 combat radar exceeds its physical pool membership");
    }
    return radarDraw(state,view,playerPosition,playerYaw,actors,profile,panel,colors,RadarExecution::level2Combat,
        nullptr,nullptr,nullptr,nullptr,Level1OrdinaryFamily::type1,domain);
}
} // namespace stellar7
