#include "retained_failure_scene.hpp"
#include "stellar7/world_scene_work.hpp"
#include "stellar7/level2_projectile_prefix_work.hpp"
#include "stellar7/level1_owned_view_work.hpp"
#include "level1_departure_model.hpp"
#include "stellar7/combat_scene_work.hpp"
#include "stellar7/failure_model_work.hpp"
#include "stellar7/level1_banked_render_work.hpp"
#include "stellar7/level1_projectile_render_work.hpp"
#include "stellar7/level1_bonus_render_work.hpp"
#include "stellar7/level1_arrival_render_work.hpp"

#include "stellar7/polygon_work.hpp"
#include "stellar7/raster_line_work.hpp"
#include "stellar7/world_model_work.hpp"
#include "stellar7/world_traversal.hpp"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace stellar7 {
namespace {

bool collapsed(const std::vector<ProjectedPoint>& points) {
    if (points.size() <= 2) {
        return true;
    }
    auto last = points.front();
    std::size_t count = 0;
    for (auto current = points.rbegin(); current != points.rend(); ++current) {
        if (current->x == last.x && current->y == last.y) {
            continue;
        }
        last = *current;
        ++count;
    }
    return count <= 2 || std::all_of(points.begin(), points.end(),
        [firstY = points.front().y](const auto point) { return point.y == firstY; });
}

}

namespace {
struct CombatSceneContext {
    const std::vector<std::optional<Level1CombatEffectView>>& effects;
    std::uint16_t page;
    std::vector<Level1ObjectRef> allocations;
    std::vector<Level1CombatEffectObservation> observations;
    std::optional<StellarRng> rng;
    std::vector<Level1BeamObservation> beams;
};
}

static WorldSceneWork worldScenePlan(const GameplayAssets& assets, const ViewTransform& view,
    const std::vector<WorldSceneActor>& actors, const WorldSceneProfile profile,
    const std::array<std::int16_t, 3> transformedLight, const WorldSceneMatrixProfile matrixProfile,
    CombatSceneContext* combatEffects = nullptr,
    const Level1OrdinaryFamily ordinaryFamily=Level1OrdinaryFamily::type1) {
    constexpr Viewport viewport{24, 294, 24, 145};
    const auto identity = makeFixedMatrixQ14(0, 0, 0);
    if ((matrixProfile != WorldSceneMatrixProfile::identityOrYaw
            && matrixProfile != WorldSceneMatrixProfile::level1Banked
            && matrixProfile != WorldSceneMatrixProfile::level1Projectile
            && matrixProfile != WorldSceneMatrixProfile::level1Combat
            && matrixProfile != WorldSceneMatrixProfile::level1Bonus
            && matrixProfile != WorldSceneMatrixProfile::level1Arrival
            && matrixProfile != WorldSceneMatrixProfile::level1Deployment
            && matrixProfile != WorldSceneMatrixProfile::level2Periodic
            && matrixProfile != WorldSceneMatrixProfile::level2Combat
            && matrixProfile != WorldSceneMatrixProfile::level2Constructor)
        || actors.empty() || profile.beforeCallback != 0 || profile.afterCallback != 0
        || profile.registryScale != 256 || profile.registryCap != 256
        || !(view.viewport == viewport) || view.perspectiveScale != 128
        || view.matrix.mode > 1 || (view.matrix.mode == identity.mode && view.matrix.values != identity.values)) {
        throw std::invalid_argument("unsupported world scene profile");
    }
    if (matrixProfile == WorldSceneMatrixProfile::level1Bonus
        && std::count_if(actors.begin(),actors.end(),[](const auto& actor) { return actor.type == 42; }) > 1) {
        throw std::invalid_argument("unsupported Type42 scene membership");
    }
    WorldSceneWork result;
    const auto append = [&result](const std::vector<NormalWorkOperation>& operations) {
        result.operations.insert(result.operations.end(), operations.begin(), operations.end());
    };
    const auto deployment=matrixProfile==WorldSceneMatrixProfile::level1Deployment;
    const auto periodic=matrixProfile==WorldSceneMatrixProfile::level2Periodic;
    const auto level2=periodic || matrixProfile==WorldSceneMatrixProfile::level2Combat;
    const auto constructorProfile=matrixProfile==WorldSceneMatrixProfile::level2Constructor;
    if ((matrixProfile==WorldSceneMatrixProfile::level1Arrival || deployment)
        && (std::count_if(actors.begin(),actors.end(),[](const auto& actor) { return actor.type==7; })>1
            || std::count_if(actors.begin(),actors.end(),[](const auto& actor) { return actor.type==42; })>1
            || (deployment && std::count_if(actors.begin(),actors.end(),[](const auto& actor) { return actor.type==44; })>1)
            || (deployment && std::count_if(actors.begin(),actors.end(),[](const auto& actor) { return actor.type==25; })>1))) {
        throw std::invalid_argument("unsupported arrival scene membership");
    }
    for (std::size_t ordinal = 0; ordinal < actors.size(); ++ordinal) {
        const auto& actor = actors[ordinal];
        constexpr std::int32_t maximumLiftAltitude=1000;
        const auto periodicOrdinary=periodic && (actor.type==1 || actor.type==4 || actor.type==8 || actor.type==13 || actor.type==30);
        if(periodicOrdinary && (actor.type==13
            ? actor.actor.position.z<0 || actor.actor.position.z>maximumLiftAltitude : actor.actor.position.z!=0)) {
            throw std::invalid_argument("Level2 periodic scene changed its source ordinary altitude");
        }
        const auto raisedLift=periodic && actor.type==13 && actor.actor.position.z!=0;
        const auto constructorGeometry=constructorProfile || (level2 && !raisedLift && (actor.type==8 || actor.type==13 || actor.type==30));
        if(constructorProfile && actor.type!=8 && actor.type!=13 && actor.type!=30) {
            throw std::invalid_argument("Level2 constructor scene excludes another model family");
        }
        if(level2 && actor.type!=8 && actor.type!=13 && actor.type!=30 && actor.type!=23 && actor.type!=19 && actor.type!=-1
            && !(periodic && (actor.type==1 || actor.type==4 || actor.type==17 || actor.type==20))) {
            throw std::invalid_argument("Level2 world scene excludes another actor family");
        }
        if (combatEffects && combatEffects->effects[ordinal]) {
            const auto& effect = *combatEffects->effects[ordinal];
            if (actor.type != -1 || actor.drawCallback != effect.state.callback
                || actor.actor.flags != effect.state.flags || actor.actor.flags != effect.actor.flags
                || !(actor.actor.position == effect.actor.position) || actor.collisionRadius != effect.radius
                || actor.heapAnimation || actor.inlineAnimation || actor.inlineSelector || actor.animation != 0) {
                throw std::invalid_argument("combat scene FC observation differs from its source actor");
            }
            //Flags88/8C execute B9EB TEST8 and B9EE JNZ, then the same transform.
            auto center = ordinal == 0 ? worldFirstCenterOperations(view, {0, actor.actor.position})
                : worldNextCenterOperations(view, {0, actor.actor.position});
            center.operations.front().count += 2;
            append(center.operations);
            const Level1ObjectRef identity{Level1PresentationKind::deathEffect, effect.poolSlot};
            const auto match = std::find(combatEffects->allocations.begin(), combatEffects->allocations.end(), identity);
            const auto matchingIndex = match == combatEffects->allocations.end() ? std::nullopt
                : std::optional<std::size_t>{static_cast<std::size_t>(match - combatEffects->allocations.begin())};
            if (assets.artGroups.size() <= 5) { throw std::invalid_argument("combat FC requires its retained group5"); }
            const auto presenter = deployment || level2 ? level1GuardianEffectOperations : level1CombatEffectOperations;
            auto child = presenter({center.center, effect.radius, effect.state,
                view.viewport, {159,84}, combatEffects->page, combatEffects->allocations.size(), matchingIndex},
                assets.artGroups[5]);
            append(child.operations);
            for (const auto& pixel : child.pixels) { result.pixels.push_back({pixel.point,pixel.color}); }
            if (child.removal) {
                combatEffects->allocations.at(child.removal->removedIndex) = combatEffects->allocations.at(child.removal->lastIndex);
                combatEffects->allocations.pop_back();
            }
            result.centers.push_back(center.center);
            result.culled.push_back(true); //Flags80 CB7A bypasses model geometry.
            result.polygonCounts.push_back(0);
            result.animationStates.push_back(std::nullopt);
            combatEffects->observations.push_back({ordinal,effect.poolSlot,std::move(child)});
            auto returned = worldModelReturnOperations(ordinal + 1 == actors.size()
                ? WorldActorContinuation::returnToCaller : WorldActorContinuation::nextActor);
            returned.front().count -= 4; //BA88..BC09 child already owns callback gate/cleanup.
            append(returned);
            continue;
        }
        const auto guardian=deployment && actor.type==44;
        const auto beam=deployment && actor.type==7 && actor.drawCallback==0xb0b4;
        const auto deployedProjectile=deployment && actor.type==22;
        const auto reward=deployment && actor.type==25;
        const bool animated=(actor.type==11 || guardian) && actor.heapAnimation.has_value() && actor.drawCallback==0x130c;
        const bool combat = matrixProfile == WorldSceneMatrixProfile::level1Combat || matrixProfile == WorldSceneMatrixProfile::level1Bonus
            || matrixProfile == WorldSceneMatrixProfile::level1Arrival || deployment || level2;
        const bool bonus = (matrixProfile == WorldSceneMatrixProfile::level1Bonus
            || matrixProfile == WorldSceneMatrixProfile::level1Arrival || deployment) && actor.type == 42;
        const bool craft=(matrixProfile==WorldSceneMatrixProfile::level1Arrival || deployment) && actor.type==7;
        const bool airborneProjectile = combat && actor.type==20;
        const bool enemyProjectile = combat && (actor.type == 17 || airborneProjectile || (level2 && actor.type==19));
        const bool projectile = enemyProjectile
            || ((combat || matrixProfile == WorldSceneMatrixProfile::level1Projectile) && actor.type == 23);
        const bool sharedProjectile = enemyProjectile || (level2 && actor.type == 23);
        if ((projectile && (actor.collisionRadius != (enemyProjectile && !airborneProjectile ? 441U : 134U)
                || (level2 ? !actor.inlineSelector || *actor.inlineSelector!=0 || actor.inlineAnimation.has_value()
                    : !actor.inlineAnimation || *actor.inlineAnimation != std::array<std::uint8_t, 6>{})))
            || (!projectile && actor.inlineAnimation) || (!bonus && !craft && !deployedProjectile && !reward && !(level2 && projectile) && actor.inlineSelector)
            || (craft && (!actor.inlineSelector || *actor.inlineSelector>=4
                || (beam ? actor.actor.flags!=8 && actor.actor.flags!=12
                    : deployment ? actor.actor.flags!=0 && actor.actor.flags!=4 : actor.actor.flags!=0)))
            || (deployedProjectile && (!actor.inlineSelector || *actor.inlineSelector>=2 || actor.actor.flags!=0))
            || (reward && (!actor.inlineSelector || *actor.inlineSelector>=4 || actor.actor.flags!=0))
            || (beam!=actor.beamPresentation.has_value())
            || (bonus && (!actor.inlineSelector || (*actor.inlineSelector != 3 && *actor.inlineSelector != 7)))) {
            throw std::invalid_argument("unsupported world inline projectile selector");
        }
        if (actor.animation != 0 || (animated ? actor.drawCallback!=0x130c : beam ? false : actor.drawCallback!=0)
            || (!animated && !guardian && actor.heapAnimation.has_value())
            || (guardian && (!actor.heapAnimation || actor.actor.flags!=0 || actor.heapAnimation->variant>=8
                || actor.heapAnimation->countdown>2 || (actor.heapAnimation->reload!=1 && actor.heapAnimation->reload!=2)))
            || (!projectile && !bonus && !craft && actor.type != 1 && actor.type != 8 && actor.type != 11
                && !(ordinaryFamily==Level1OrdinaryFamily::type1And3 && actor.type==3)
                && !(periodic && actor.type==4) && actor.type != 13 && actor.type != 29 && actor.type != 30 && !guardian && !deployedProjectile && !reward)
            || (actor.type == 11 && !animated)
            || static_cast<std::size_t>(actor.type) >= assets.models.size()) {
            throw std::invalid_argument("unsupported world scene actor");
        }
        const auto& model = assets.models[actor.type];
        if (actor.collisionRadius != model.collisionRadius) {
            throw std::invalid_argument("world actor radius differs from original model");
        }
        std::optional<Level1AnimationWork> animation;
        if (animated) {
            if (guardian) {
                const auto callback=level1GuardianAnimationOperations(*actor.heapAnimation,model);
                animation=Level1AnimationWork{callback.state,callback.operations};
            } else { animation=level1AnimationOperations(*actor.heapAnimation,model); }
        }
        const WorldActorWorkInput centerActor{0,actor.actor.position};
        const auto center=level2
            ? ordinal==0 ? worldFirstCenterOperations(view,actor.actor,WorldSourceFlagsDomain::flagsWithoutCallback)
                : worldNextCenterOperations(view,actor.actor,WorldSourceFlagsDomain::flagsWithoutCallback)
            : ordinal==0 ? worldFirstCenterOperations(view,craft && deployment ? centerActor : actor.actor)
                : worldNextCenterOperations(view,craft && deployment ? centerActor : actor.actor);
        append(center.operations);
        result.centers.push_back(center.center);
        const auto ownedShared = (periodic || !level2) && !constructorProfile
            && (!constructorGeometry || !periodic || (actor.type==13 && center.center.y<=0)
                || std::max({std::abs(std::int64_t{center.center.x}),std::abs(std::int64_t{center.center.y}),
                    std::abs(std::int64_t{center.center.z})})+model.collisionRadius>32000)
            && matrixProfile != WorldSceneMatrixProfile::identityOrYaw
            && matrixProfile != WorldSceneMatrixProfile::level1Projectile
            && (actor.type==1 || (ordinaryFamily==Level1OrdinaryFamily::type1And3 && actor.type==3)
                || actor.type==11 || actor.type==29 || projectile || raisedLift || (periodic && (actor.type==4 || constructorGeometry)))
            && std::max({std::abs(std::int64_t{center.center.x}),std::abs(std::int64_t{center.center.y}),
                std::abs(std::int64_t{center.center.z})}) + model.collisionRadius <= 64000;
        const auto ownedWide = ownedShared && std::max({std::abs(std::int64_t{center.center.x}),
            std::abs(std::int64_t{center.center.y}),std::abs(std::int64_t{center.center.z})})
            + model.collisionRadius > 32000;
        constexpr std::int64_t callerSignedDivisorLimit=32768;
        std::optional<Level1BeamProjectionWork> beamProjection;
        auto projectedRadius=std::uint16_t{};
        if (beam) {
            beamProjection=level1BeamProjectionOperations(center.center,actor.actor.flags,
                actor.beamPresentation->anchor,actor.beamPresentation->radius);
            append(beamProjection->operations);projectedRadius=beamProjection->modelRadius;
        } else {
            const auto caller=deployment && (craft || guardian || deployedProjectile || reward)
                ? level1DeploymentModelCallerOperations(center.center,actor.type)
                : periodic && ownedWide && std::int64_t{center.center.y}-model.collisionRadius>=callerSignedDivisorLimit
                ? level1OwnedWideModelCallerOperations(center.center,model,0,0)
                : level2 ? worldModelCallerOperations(center.center,actor.collisionRadius,actor.actor.flags,0,
                    WorldSourceFlagsDomain::flagsWithoutCallback)
                : ownedWide && std::int64_t{center.center.y}-model.collisionRadius>=callerSignedDivisorLimit
                ? level1OwnedWideModelCallerOperations(center.center,model,actor.actor.flags,0)
                : worldModelCallerOperations(center.center,actor.collisionRadius,actor.actor.flags,0);
            append(caller.operations);projectedRadius=caller.projectedRadius;
        }
        if (animated) {
            result.operations.push_back({NormalWorkKind::instructions, 2}); //BBD7/BBDA post-draw callback gate.
        }
        const auto combatLift = combat && actor.type == 11;
        constexpr std::int64_t modelNearPlane = 128;
        const auto axisTank = combat && actor.type == 1
            && std::abs(std::int64_t{center.center.x}) <= actor.collisionRadius
            && std::int64_t{center.center.y} + actor.collisionRadius >= modelNearPlane;
        constexpr std::int64_t constructorPrecisionLimit=32000;
        const auto constructorSharedSphere=constructorGeometry
            && std::max({std::abs(std::int64_t{center.center.x}),std::abs(std::int64_t{center.center.y}),
                std::abs(std::int64_t{center.center.z})})+model.collisionRadius<=constructorPrecisionLimit;
        const auto cull=constructorSharedSphere ? sharedModelSphereCullOperations(center.center,model)
            : ownedWide ? level1OwnedWideModelCullOperations(center.center,model)
            : ownedShared ? sharedModelSphereCullOperations(center.center,model)
            : deployment && (craft || guardian || deployedProjectile || reward)
            ? level1DeploymentModelCullOperations(center.center,actor.type)
            : craft ? level1ArrivalModelCullOperations(center.center)
            : bonus ? level1BonusModelCullOperations(center.center)
            : combatLift ? level1CombatClippedLiftModelCullOperations(center.center)
            : airborneProjectile ? level1AirborneProjectileModelCullOperations(center.center)
            : sharedProjectile ? sharedModelSphereCullOperations(center.center,model)
            : combat && projectile ? level1CombatProjectileModelCullOperations(center.center, actor.type)
            : projectile ? level1ProjectileModelCullOperations(center.center)
            : axisTank ? level1AxisTankModelCullOperations(center.center)
            : worldModelCullOperations(center.center, actor.collisionRadius);
        if (projectedRadius != cull.projectedRadius) {
            throw std::logic_error("world caller and cull radius disagree");
        }
        append(cull.operations);
        result.culled.push_back(cull.culled);
        result.polygonCounts.push_back(0);
        result.animationStates.push_back(animation ? std::optional<Level1AnimationState>{animation->state}
            : guardian ? actor.heapAnimation : std::nullopt);
        auto beamClipping=PolygonClipMode::Disabled;
        if (!cull.culled) {
            const auto banked = matrixProfile != WorldSceneMatrixProfile::identityOrYaw
                && (actor.type == 11 || airborneProjectile) && actor.angles[1] != 0;
            const WorldModelViewInput modelView{view.matrix, center.relativePosition};
            const auto setup=ownedWide
                ? level1OwnedWideModelSetupOperations(center.center,actor.angles,projectedRadius,model,
                    profile.registryScale,profile.registryCap,modelView)
                : ownedShared
                ? level1OwnedModelSetupOperations(center.center,actor.angles,projectedRadius,model,
                    profile.registryScale,profile.registryCap,modelView)
                : constructorGeometry
                ? level2ConstructorModelSetupOperations(center.center,actor.angles,projectedRadius,actor.type,model,
                    profile.registryScale,profile.registryCap,modelView)
                : deployment && craft
                ? level1DepartureModelSetupOperations(center.center,actor.angles,projectedRadius,model,
                    profile.registryScale,profile.registryCap,modelView)
                : deployment && (craft || guardian || deployedProjectile || reward)
                ? level1DeploymentModelSetupOperations(center.center,actor.angles,projectedRadius,actor.type,model,
                    profile.registryScale,profile.registryCap,modelView)
                : craft
                ? level1ArrivalModelSetupOperations(center.center,actor.angles,projectedRadius,model,
                    profile.registryScale,profile.registryCap,modelView)
                : bonus
                ? level1BonusModelSetupOperations(center.center,actor.angles,projectedRadius,model,
                    profile.registryScale,profile.registryCap,modelView)
                : airborneProjectile
                ? level1AirborneProjectileModelSetupOperations(center.center,actor.angles,projectedRadius,model,
                    profile.registryScale,profile.registryCap,modelView)
                : sharedProjectile
                ? level1SharedCombatProjectileModelSetupOperations(center.center, actor.angles, projectedRadius,
                    actor.type, model, profile.registryScale, profile.registryCap, modelView)
                : projectile
                ? level1ProjectileModelSetupOperations(center.center, actor.angles, projectedRadius,
                    model, profile.registryScale, profile.registryCap, modelView)
                : combatLift && !banked
                ? level1CombatYawLiftModelSetupOperations(center.center, actor.angles, projectedRadius,
                    model, profile.registryScale, profile.registryCap, modelView)
                : banked && combatLift
                ? level1CombatClippedLiftModelSetupOperations(center.center, actor.angles, projectedRadius,
                    model, profile.registryScale, profile.registryCap, modelView)
                : banked
                ? level1BankedModelSetupOperations(center.center, actor.angles, projectedRadius,
                    actor.type, model, profile.registryScale, profile.registryCap, modelView)
                : axisTank
                ? level1AxisTankModelSetupOperations(center.center,actor.angles,projectedRadius,model,
                    profile.registryScale,profile.registryCap,modelView)
                : worldModelSetupOperations(center.center, actor.angles, projectedRadius,
                    actor.type, model, profile.registryScale, profile.registryCap, modelView);
            append(setup.operations);
            const auto clipping = cull.clipped ? PolygonClipMode::Enabled : PolygonClipMode::Disabled;
            auto geometryInput = worldModelGeometryInput(assets, setup, center.center, clipping);
            geometryInput.light = transformedLight;
            if(constructorGeometry && !ownedShared) {geometryInput.matrixProfile=WorldModelMatrixProfile::level2Constructor;}
            if (banked) {
                geometryInput.matrixProfile = WorldModelMatrixProfile::level1Banked;
            }
            if (deployment && (craft || guardian || deployedProjectile || reward)) {
                geometryInput.matrixProfile=WorldModelMatrixProfile::level1Deployment;
                if (guardian) { geometryInput.animationSelectors.assign(3,actor.heapAnimation->variant); }
                else { geometryInput.animationSelectors={*actor.inlineSelector}; }
            } else if (craft) {
                geometryInput.matrixProfile=WorldModelMatrixProfile::level1Arrival;
                geometryInput.animationSelectors={*actor.inlineSelector};
            } else if (bonus) {
                geometryInput.matrixProfile = WorldModelMatrixProfile::level1InlineSpin;
                geometryInput.animationSelectors = {*actor.inlineSelector};
            } else if (animated) {
                geometryInput.animationSelectors = {actor.heapAnimation->variant};
            } else if (projectile) {
                if (airborneProjectile && !level2) { geometryInput.animationSelectors={actor.inlineAnimation->at(0)}; }
                else if (level2) { geometryInput.animationSelectors={*actor.inlineSelector}; }
                else { geometryInput.animationSelectors.assign(actor.inlineAnimation->begin(), actor.inlineAnimation->end()); }
            }
            const auto geometry=ownedWide
                ? level1OwnedWideModelGeometryOperations(model.lods.at(setup.selectedLodIndex),geometryInput,actor.angles,modelView)
                : ownedShared || (level2 && projectile)
                ? level1OwnedModelGeometryOperations(model.lods.at(setup.selectedLodIndex),geometryInput,actor.angles,modelView)
                : constructorGeometry
                ? level2ConstructorModelGeometryOperations(model.lods.at(setup.selectedLodIndex),geometryInput,actor.angles,modelView,actor.type)
                : deployment && craft
                ? level1DepartureModelGeometryOperations(model.lods.at(setup.selectedLodIndex),geometryInput,actor.angles,modelView)
                : deployment && (craft || guardian || deployedProjectile || reward)
                ? level1DeploymentModelGeometryOperations(model.lods.at(setup.selectedLodIndex),geometryInput,actor.angles,modelView,actor.type)
                : craft
                ? level1ArrivalModelGeometryOperations(model.lods.at(setup.selectedLodIndex),geometryInput,actor.angles,modelView)
                : bonus
                ? level1BonusModelGeometryOperations(model.lods.at(setup.selectedLodIndex),geometryInput,actor.angles,modelView)
                : worldModelGeometryOperations(model.lods.at(setup.selectedLodIndex),geometryInput);
            result.polygonCounts.back() = geometry.polygons.size();
            beamClipping=setup.vertexPrecision==1 ? PolygonClipMode::Disabled : clipping;
            for (const auto& polygon : geometry.polygons) {
                append(polygon.prefix);
                if (ownedShared || constructorGeometry || (level2 && projectile)
                    || craft || guardian || deployedProjectile || reward) {
                    const auto raster=ownedPolygonOperations(polygon.points,viewport,
                        setup.vertexPrecision==1 ? PolygonClipMode::Disabled : clipping,
                        polygon.filled ? FailurePolygonStyle::filled : FailurePolygonStyle::outline,
                        polygon.primaryColor,polygon.secondaryColor);
                    append(raster.operations);
                    result.pixels.insert(result.pixels.end(),raster.pixels.begin(),raster.pixels.end());
                } else if (!polygon.filled || polygon.primaryColor != polygon.secondaryColor) {
                    const auto raster = ownedPolygonOperations(polygon.points, viewport, clipping,
                        polygon.filled ? FailurePolygonStyle::filled : FailurePolygonStyle::outline,
                        polygon.primaryColor, polygon.secondaryColor);
                    append(raster.operations);
                    result.pixels.insert(result.pixels.end(), raster.pixels.begin(), raster.pixels.end());
                } else if (enemyProjectile && polygon.points.size() == 2 && clipping == PolygonClipMode::Enabled) {
                    const auto raster = level1CombatLinePolygonOperations({polygon.points[0], polygon.points[1]}, viewport);
                    append(raster.operations);
                    for (const auto point : raster.pixels) {
                        result.pixels.push_back({point, polygon.primaryColor});
                    }
                } else if (collapsed(clipPolygon(polygon.points, viewport, clipping).points)) {
                    const auto raster = matrixProfile == WorldSceneMatrixProfile::identityOrYaw || constructorProfile || (level2 && !projectile) || polygon.points.size() < 3
                        ? collapsedPolygonOperations(polygon.points, viewport, clipping)
                        : ownedCollapsedPolygonOperations(polygon.points, viewport, clipping);
                    append(raster.operations);
                    for (const auto point : raster.pixels) {
                        result.pixels.push_back({point, polygon.primaryColor});
                    }
                } else {
                    const auto raster = solidPolygonOperations(polygon.points, viewport, clipping);
                    append(raster.operations);
                    for (std::size_t row = 0; row < raster.raster.polygon.rows.size(); ++row) {
                        const auto span = raster.raster.polygon.rows[row];
                        const auto y = static_cast<std::int16_t>(raster.raster.polygon.firstY + row);
                        for (int x = span.left; x <= span.right; ++x) {
                            result.pixels.push_back({{static_cast<std::int16_t>(x), y}, polygon.primaryColor});
                        }
                    }
                }
            }
            append(geometry.suffix);
        }
        auto returned = worldModelReturnOperations(ordinal + 1 == actors.size()
            ? WorldActorContinuation::returnToCaller : WorldActorContinuation::nextActor);
        if (beam) {
            if (!combatEffects || !combatEffects->rng || !beamProjection) { throw std::logic_error("beam requires retained scene RNG"); }
            append({{NormalWorkKind::instructions,9}});
            auto child=level1CompletionBeamOperations(beamProjection->projection,actor.beamPresentation->timer,
                *combatEffects->rng,CompletionBeamView::main,beamClipping);
            append(child.operations);*combatEffects->rng=child.rng;
            for (const auto& pixel:child.pixels) { result.pixels.push_back({pixel.point,pixel.color}); }
            combatEffects->beams.push_back({ordinal,beamProjection->projection,child.beam});
            append({{NormalWorkKind::instructions,1}});returned.front().count-=4;
        } else if (animated) {
            result.operations.push_back({NormalWorkKind::instructions, 4 + 2 + 3}); //BBEE..BC03 through callback CALL.
            append(animation->operations);
            result.operations.push_back({NormalWorkKind::instructions, 1}); //BC06 argument cleanup.
            returned.front().count -= 4;
        }
        append(returned);
    }
    return result;
}

WorldSceneWork worldSceneOperations(const GameplayAssets& assets, const ViewTransform& view,
    const std::vector<WorldSceneActor>& actors, const WorldSceneProfile profile,
    const std::array<std::int16_t,3> transformedLight, const WorldSceneMatrixProfile matrixProfile) {
    return worldScenePlan(assets,view,actors,profile,transformedLight,matrixProfile);
}

static Level1CombatSceneWork combatSceneOperations(const GameplayAssets& assets,
    const ViewTransform& view, const std::vector<WorldSceneActor>& actors,
    const std::vector<std::optional<Level1CombatEffectView>>& effects,
    const WorldSceneProfile profile, const std::array<std::int16_t,3> transformedLight,
    const std::uint16_t page, const std::vector<Level1ObjectRef>& allocationOrder, const bool bonus,const bool arrival=false,const StellarRng* deploymentRng=nullptr,
    const Level1OrdinaryFamily ordinaryFamily=Level1OrdinaryFamily::type1) {
    if (effects.size() != actors.size() || allocationOrder.size() > 32
        || (page != 0xa000 && page != 0xa800)) {
        throw std::invalid_argument("unsupported mixed Level1 combat scene context");
    }
    std::array<bool,Level1State::deathEffectPoolSize> selected{};
    for (const auto& effect : effects) {
        if (!effect) { continue; }
        if (effect->poolSlot >= selected.size() || selected[effect->poolSlot]) {
            throw std::invalid_argument("duplicate or invalid combat effect pool identity");
        }
        selected[effect->poolSlot] = true;
    }
    for (std::size_t index = 0; index < allocationOrder.size(); ++index) {
        if (std::find(allocationOrder.begin(),allocationOrder.begin()+index,allocationOrder[index])
            != allocationOrder.begin()+index) {
            throw std::invalid_argument("mixed scene registry identities must be unique");
        }
    }
    CombatSceneContext context{effects,page,allocationOrder,{},deploymentRng ? std::optional<StellarRng>{*deploymentRng} : std::nullopt,{}};
    auto scene = worldScenePlan(assets,view,actors,profile,transformedLight,
        deploymentRng ? WorldSceneMatrixProfile::level1Deployment : arrival ? WorldSceneMatrixProfile::level1Arrival
            : bonus ? WorldSceneMatrixProfile::level1Bonus : WorldSceneMatrixProfile::level1Combat,&context,ordinaryFamily);
    return {std::move(scene),std::move(context.observations),std::move(context.allocations),std::move(context.rng),std::move(context.beams)};
}

Level1CombatSceneWork level1CombatSceneOperations(const GameplayAssets& assets,
    const ViewTransform& view, const std::vector<WorldSceneActor>& actors,
    const std::vector<std::optional<Level1CombatEffectView>>& effects,
    const WorldSceneProfile profile, const std::array<std::int16_t,3> transformedLight,
    const std::uint16_t page, const std::vector<Level1ObjectRef>& allocationOrder) {
    return combatSceneOperations(assets,view,actors,effects,profile,transformedLight,page,allocationOrder,false);
}

Level1CombatSceneWork level1BonusCombatSceneOperations(const GameplayAssets& assets,
    const ViewTransform& view, const std::vector<WorldSceneActor>& actors,
    const std::vector<std::optional<Level1CombatEffectView>>& effects,
    const WorldSceneProfile profile, const std::array<std::int16_t,3> transformedLight,
    const std::uint16_t page, const std::vector<Level1ObjectRef>& allocationOrder) {
    return combatSceneOperations(assets,view,actors,effects,profile,transformedLight,page,allocationOrder,true);
}

Level1CombatSceneWork level1ArrivalCombatSceneOperations(const GameplayAssets& assets,
    const ViewTransform& view, const std::vector<WorldSceneActor>& actors,
    const std::vector<std::optional<Level1CombatEffectView>>& effects,
    const WorldSceneProfile profile, const std::array<std::int16_t,3> transformedLight,
    const std::uint16_t page, const std::vector<Level1ObjectRef>& allocationOrder) {
    return combatSceneOperations(assets,view,actors,effects,profile,transformedLight,page,allocationOrder,true,true);
}

Level1CombatSceneWork level1DeploymentCombatSceneOperations(const GameplayAssets& assets,
    const ViewTransform& view,const std::vector<WorldSceneActor>& actors,
    const std::vector<std::optional<Level1CombatEffectView>>& effects,
    const WorldSceneProfile profile,const std::array<std::int16_t,3> light,
    const std::uint16_t page,const std::vector<Level1ObjectRef>& allocations,const StellarRng& rng) {
    return combatSceneOperations(assets,view,actors,effects,profile,light,page,allocations,true,true,&rng);
}

Level1CombatSceneWork level1Type3CombatSceneOperations(const GameplayAssets& assets,
    const ViewTransform& view,const std::vector<WorldSceneActor>& actors,
    const std::vector<std::optional<Level1CombatEffectView>>& effects,
    const WorldSceneProfile profile,const std::array<std::int16_t,3> light,const std::uint16_t page,
    const std::vector<Level1ObjectRef>& allocations,const Level1OrdinaryRenderFamily family,
    const StellarRng* rng) {
    if ((family!=Level1OrdinaryRenderFamily::combatEffects && family!=Level1OrdinaryRenderFamily::bonus
            && family!=Level1OrdinaryRenderFamily::arrival && family!=Level1OrdinaryRenderFamily::deployment)
        || ((family==Level1OrdinaryRenderFamily::deployment)!=(rng!=nullptr))) {
        throw std::invalid_argument("invalid Type3 mixed scene family");
    }
    return combatSceneOperations(assets,view,actors,effects,profile,light,page,allocations,
        family!=Level1OrdinaryRenderFamily::combatEffects,
        family==Level1OrdinaryRenderFamily::arrival || family==Level1OrdinaryRenderFamily::deployment,
        rng,Level1OrdinaryFamily::type1And3);
}

enum class FailureSceneDomain { legacy, retainedRoster, level2 };

static FailureModelSceneWork failureModelScenePlan(const GameplayAssets& assets,
    const ViewTransform& view, const std::vector<FailureModelSceneItem>& items,
    const WorldSceneProfile profile, const std::array<std::int16_t, 3> transformedLight,const FailureSceneDomain domain) {
    constexpr Viewport viewport{0, 318, 0, 199};
    constexpr std::array<std::int16_t, 10> types{0, 1, 11, 23, 29, 36, 37, 38, 39, 40};
    if (items.empty() || profile.beforeCallback != 0 || profile.afterCallback != 0
        || profile.registryScale != 256 || profile.registryCap != 256
        || !(view.viewport == viewport) || view.perspectiveScale != 128 || view.matrix.mode > 3) {
        throw std::invalid_argument("unsupported failure model scene profile");
    }
    FailureModelSceneWork result;
    auto& scene = result.scene;
    const auto append = [&](const auto& operations) {
        scene.operations.insert(scene.operations.end(), operations.begin(), operations.end());
    };
    for (std::size_t ordinal = 0; ordinal < items.size(); ++ordinal) {
        const auto& item = items[ordinal];
        const auto& actor = item.actor;
        const auto level2=domain==FailureSceneDomain::level2;
        const auto level2Type=actor.type==0 || actor.type==4 || actor.type==8 || actor.type==13 || actor.type==19 || actor.type==23 || actor.type==30 || (actor.type>=36 && actor.type<=40);
        const auto guardian=domain==FailureSceneDomain::retainedRoster && actor.type==44;
        const auto animated = (actor.type == 11 || guardian) && actor.heapAnimation.has_value() && actor.drawCallback==0x130c;
        const auto projectile = actor.type == 23 || (level2 && actor.type==19);
        if ((level2 ? !level2Type : !guardian && std::find(types.begin(), types.end(), actor.type) == types.end())
            || actor.type < 0 || static_cast<std::size_t>(actor.type) >= assets.models.size()
            || actor.animation != 0 || actor.inlineSelector || (actor.actor.flags & ~4U) != 0
            || (animated ? actor.drawCallback != 0x130c : actor.drawCallback != 0)
            || (!animated && !guardian && actor.heapAnimation) || (actor.type == 11 && !animated)
            || (guardian && (!actor.heapAnimation || actor.actor.flags!=0 || actor.heapAnimation->variant>=8
                || actor.heapAnimation->countdown>2 || (actor.heapAnimation->reload!=1 && actor.heapAnimation->reload!=2)))
            || (projectile ? !actor.inlineAnimation
                    || (!level2 && *actor.inlineAnimation != std::array<std::uint8_t, 6>{})
                : actor.inlineAnimation.has_value())) {
            throw std::invalid_argument("unsupported failure model scene actor");
        }
        const auto& model = assets.models[actor.type];
        if (actor.collisionRadius != model.collisionRadius) {
            throw std::invalid_argument("failure actor radius differs from original model");
        }
        std::optional<Level1AnimationWork> animation;
        if (animated) {
            if (guardian) {
                const auto child=level1GuardianAnimationOperations(*actor.heapAnimation,model);
                animation=Level1AnimationWork{child.state,child.operations};
            } else {animation=level1AnimationOperations(*actor.heapAnimation,model);}
        }
        const auto center = ordinal == 0 ? failureFirstCenterOperations(view, actor.actor)
            : failureNextCenterOperations(view, actor.actor);
        append(center.operations);
        scene.centers.push_back(center.center);
        const auto fixedWide=domain==FailureSceneDomain::retainedRoster && actor.type==retainedFailureFixedType
            && std::max({std::abs(std::int64_t{center.center.x}),std::abs(std::int64_t{center.center.y}),
                std::abs(std::int64_t{center.center.z})})+model.collisionRadius>32000;
        const auto caller = (fixedWide || level2) && std::int64_t{center.center.y}-model.collisionRadius>=32768
            ? level1OwnedWideModelCallerOperations(center.center,model,0,0)
            : worldModelCallerOperations(center.center, actor.collisionRadius, 0, 0);
        append(caller.operations);
        if (animated) {
            scene.operations.push_back({NormalWorkKind::instructions, 2}); //BBD7/BBDA.
        }
        const auto cull = level2 ? level2FailureModelCullOperations(center.center,model)
            : fixedWide ? failureFixedWideCullOperations(center.center,model)
            : failureModelCullOperations(center.center, actor.collisionRadius, viewport);
        if (caller.projectedRadius != cull.projectedRadius) {
            throw std::logic_error("failure caller and sphere projected radius disagree");
        }
        append(cull.operations);
        scene.culled.push_back(cull.culled);
        scene.polygonCounts.push_back(0);
        scene.animationStates.push_back(animation ? std::optional<Level1AnimationState>{animation->state} : std::nullopt);
        if (!cull.culled) {
            if (guardian) {throw std::invalid_argument("retained failure guardian requires its side-culled branch");}
            const auto setup = level2
                ? level2FailureModelSetupOperations(center.center,actor.angles,caller.projectedRadius,actor.type,model,
                    profile.registryScale,profile.registryCap,{view.matrix,center.relativePosition})
                : fixedWide
                ? failureFixedWideSetupOperations(center.center,actor.angles,caller.projectedRadius,model,
                    profile.registryScale,profile.registryCap,{view.matrix,center.relativePosition})
                : failureModelSetupOperations(center.center, actor.angles, caller.projectedRadius,
                actor.type, model, profile.registryScale, profile.registryCap, {view.matrix, center.relativePosition});
            append(setup.operations);
            const auto clipping = cull.clipped ? PolygonClipMode::Enabled : PolygonClipMode::Disabled;
            auto input = worldModelGeometryInput(assets, setup, center.center, clipping);
            input.light = transformedLight;
            input.screenCenter = {159, 99};
            if (animated) {
                input.animationSelectors = {actor.heapAnimation->variant};
            } else if (projectile) {
                input.animationSelectors.assign(actor.inlineAnimation->begin(), actor.inlineAnimation->end());
            }
            const auto actorMatrix = makeFixedMatrixQ14(actor.angles[0], actor.angles[1], actor.angles[2]);
            const auto directFacing = actorMatrix.mode == 0 ? std::optional<FixedPosition>{center.relativePosition} : std::nullopt;
            const auto geometry = level2
                ? level2FailureModelGeometryOperations(model.lods.at(setup.selectedLodIndex),input,actor.angles,
                    {view.matrix,center.relativePosition},actor.type)
                : fixedWide
                ? failureFixedWideGeometryOperations(model.lods.at(setup.selectedLodIndex),input,{view.matrix,center.relativePosition})
                : failureModelGeometryOperations(model.lods.at(setup.selectedLodIndex), input, directFacing);
            scene.polygonCounts.back() = geometry.polygons.size();
            for (const auto& polygon : geometry.polygons) {
                append(polygon.prefix);
                const auto raster = failurePolygonOperations(polygon.points, viewport, clipping,
                    polygon.filled ? FailurePolygonStyle::filled : FailurePolygonStyle::outline, polygon.primaryColor, polygon.secondaryColor);
                append(raster.operations);
                scene.pixels.insert(scene.pixels.end(), raster.pixels.begin(), raster.pixels.end());
            }
            append(geometry.suffix);
        }
        auto returned = worldModelReturnOperations(ordinal + 1 == items.size()
            ? WorldActorContinuation::returnToCaller : WorldActorContinuation::nextActor);
        if (animated) {
            scene.operations.push_back({NormalWorkKind::instructions, 9}); //BBEE..BC03 callback caller.
            append(animation->operations);
            scene.operations.push_back({NormalWorkKind::instructions, 1}); //BC06 cleanup.
            returned.front().count -= 4;
        }
        append(returned);
        result.observations.push_back({item.allocationIdentity, scene.animationStates.back()});
    }
    return result;
}

static FailureSceneWork failureScenePlan(const GameplayAssets& assets, const ViewTransform& view,
    const std::vector<FailureSceneItem>& items, const WorldSceneProfile profile,
    const std::array<std::int16_t, 3> transformedLight, const std::uint16_t page,
    const std::vector<std::uint16_t>& allocationIdentities, const FailureEffectProfile effectProfile,const FailureSceneDomain domain) {
    if (items.empty() || allocationIdentities.size() > 64 || effectProfile != FailureEffectProfile::bitmap
        || (page != 0xa000 && page != 0xa800) || assets.artGroups.size() <= 5
        || profile.registryScale != 256 || profile.registryCap != 256
        || profile.beforeCallback != 0 || profile.afterCallback != 0
        || !(view.viewport == Viewport{0, 318, 0, 199}) || view.perspectiveScale != 128 || view.matrix.mode > 3) {
        throw std::invalid_argument("unsupported mixed failure scene profile");
    }
    FailureSceneWork result;
    result.allocationIdentities = allocationIdentities;
    const auto append = [&](const auto& operations) {
        result.scene.operations.insert(result.scene.operations.end(), operations.begin(), operations.end());
    };
    for (std::size_t ordinal = 0; ordinal < items.size(); ++ordinal) {
        if (const auto* model = std::get_if<FailureModelSceneItem>(&items[ordinal])) {
            auto child = failureModelScenePlan(assets, view, {*model}, profile, transformedLight,domain);
            if (ordinal != 0) {
                child.scene.operations.front().count -= 8; //First-list setup excluded after B9D0.
            }
            if (ordinal + 1 != items.size()) {
                child.scene.operations.back().count -= 6; //BC18 replaces BC1B..BC3B return.
            }
            append(child.scene.operations);
            for (const auto pixel : child.scene.pixels) {
                result.pixels.push_back({pixel.point, pixel.color, page});
            }
            result.scene.pixels.insert(result.scene.pixels.end(), child.scene.pixels.begin(), child.scene.pixels.end());
            result.scene.centers.push_back(child.scene.centers.front());
            result.scene.culled.push_back(child.scene.culled.front());
            result.scene.polygonCounts.push_back(child.scene.polygonCounts.front());
            result.scene.animationStates.push_back(child.scene.animationStates.front());
            result.observations.push_back({model->allocationIdentity, child.observations.front().animation, std::nullopt});
            continue;
        }
        const auto& effect = std::get<FailureBitmapSceneItem>(items[ordinal]);
        if (effect.actor.flags != effect.state.flags) {
            throw std::invalid_argument("failure effect projection flags disagree with actor");
        }
        const auto center = ordinal == 0 ? failureFirstCenterOperations(view, effect.actor)
            : failureNextCenterOperations(view, effect.actor);
        append(center.operations);
        const auto match = std::find(result.allocationIdentities.begin(), result.allocationIdentities.end(), effect.allocationIdentity);
        const auto matchingIndex = match == result.allocationIdentities.end() ? std::nullopt
            : std::optional<std::size_t>{static_cast<std::size_t>(match - result.allocationIdentities.begin())};
        const auto effectInput = FailureEffectInput{center.center, effect.collisionRadius, effect.state,
            view.viewport, {159, 99}, page, result.allocationIdentities.size(), matchingIndex};
        auto child = domain==FailureSceneDomain::level2
            ? level2FailureEffectOperations(effectInput,assets.artGroups[5])
            : failureEffectOperations(effectInput,assets.artGroups[5]);
        append(child.operations);
        result.pixels.insert(result.pixels.end(), child.pixels.begin(), child.pixels.end());
        if (child.removal) {
            result.allocationIdentities.at(child.removal->removedIndex) = result.allocationIdentities.at(child.removal->lastIndex);
            result.allocationIdentities.pop_back();
        }
        result.scene.centers.push_back(center.center);
        result.scene.culled.push_back(true); //Source flags80 bypasses model geometry.
        result.scene.polygonCounts.push_back(0);
        result.scene.animationStates.push_back(std::nullopt);
        result.observations.push_back({effect.allocationIdentity, std::nullopt,
            FailureEffectObservation{child.state, child.cachedCenter, child.projected, child.retired,
                std::move(child.stateWrites), child.removal}});
        auto returned = worldModelReturnOperations(ordinal + 1 == items.size()
            ? WorldActorContinuation::returnToCaller : WorldActorContinuation::nextActor);
        returned.front().count -= 4; //BA88..BC09 effect child already owns the callback gate.
        append(returned);
    }
    return result;
}

}


namespace stellar7 {
Level1CombatSceneWork level2CombatSceneOperations(const GameplayAssets& assets,
    const ViewTransform& view, const std::vector<WorldSceneActor>& actors,
    const std::vector<std::optional<Level1CombatEffectView>>& effects,
    const WorldSceneProfile profile, const std::array<std::int16_t,3> light,
    const std::uint16_t page, const std::vector<Level1ObjectRef>& allocations) {
    constexpr std::size_t maximumAllocations=7+Level1State::projectilePoolSize+Level1State::deathEffectPoolSize;
    if(actors.size()!=effects.size() || allocations.size()>maximumAllocations
        || (page!=0xa000 && page!=0xa800)) {
        throw std::invalid_argument("unsupported Level2 combat scene context");
    }
    std::array<std::size_t,5> counts{};std::array<bool,Level1State::deathEffectPoolSize> selected{};
    for(std::size_t index=0;index<actors.size();++index) {
        const auto& actor=actors[index];
        if(actor.type==8) {++counts[0];}
        else if(actor.type==13) {++counts[1];}
        else if(actor.type==30) {++counts[2];}
        else if(actor.type==23 || actor.type==19) {++counts[3];}
        else if(actor.type==-1) {++counts[4];}
        else {throw std::invalid_argument("Level2 combat scene excludes another actor family");}
        if(effects[index].has_value()!=(actor.type==-1)) {
            throw std::invalid_argument("Level2 combat scene lost its FC callback identity");
        }
        if(effects[index]) {
            const auto slot=effects[index]->poolSlot;
            if(slot>=selected.size() || selected[slot]) {throw std::invalid_argument("Level2 combat scene aliases an FC slot");}
            selected[slot]=true;
        }
    }
    if(counts[0]>3 || counts[1]>2 || counts[2]>2 || counts[3]>Level1State::projectilePoolSize
        || counts[4]>Level1State::deathEffectPoolSize) {
        throw std::invalid_argument("Level2 combat scene exceeds its physical constructor/pool membership");
    }
    for(std::size_t i=0;i<allocations.size();++i) {
        if(std::find(allocations.begin(),allocations.begin()+i,allocations[i])!=allocations.begin()+i) {
            throw std::invalid_argument("Level2 combat scene has duplicate registry identities");
        }
    }
    CombatSceneContext context{effects,page,allocations,{},{},{}};
    auto scene=worldScenePlan(assets,view,actors,profile,light,WorldSceneMatrixProfile::level2Combat,&context);
    return {std::move(scene),std::move(context.observations),std::move(context.allocations),{}, {}};
}

Level1CombatSceneWork level2PeriodicSceneOperations(const GameplayAssets& assets,
    const ViewTransform& view, const std::vector<WorldSceneActor>& actors,
    const std::vector<std::optional<Level1CombatEffectView>>& effects,
    const WorldSceneProfile profile, const std::array<std::int16_t,3> light,
    const std::uint16_t page, const std::vector<Level1ObjectRef>& allocations) {
    constexpr std::size_t maximumAllocations=18+Level1State::projectilePoolSize+Level1State::deathEffectPoolSize;
    if(actors.size()!=effects.size() || allocations.size()>maximumAllocations
        || (page!=0xa000 && page!=0xa800)) {
        throw std::invalid_argument("unsupported Level2 combat scene context");
    }
    std::array<std::size_t,5> counts{};std::array<bool,Level1State::deathEffectPoolSize> selected{};
    for(std::size_t index=0;index<actors.size();++index) {
        const auto& actor=actors[index];
        if(actor.type==8 || actor.type==1 || actor.type==4) {++counts[0];}
        else if(actor.type==13) {++counts[1];}
        else if(actor.type==30) {++counts[2];}
        else if(actor.type==23 || actor.type==19 || actor.type==17 || actor.type==20) {++counts[3];}
        else if(actor.type==-1) {++counts[4];}
        else {throw std::invalid_argument("Level2 combat scene excludes another actor family");}
        if(effects[index].has_value()!=(actor.type==-1)) {
            throw std::invalid_argument("Level2 combat scene lost its FC callback identity");
        }
        if(effects[index]) {
            const auto slot=effects[index]->poolSlot;
            if(slot>=selected.size() || selected[slot]) {throw std::invalid_argument("Level2 combat scene aliases an FC slot");}
            selected[slot]=true;
        }
    }
    if(counts[0]+counts[1]>16 || counts[1]>maximumLevel2PeriodicLiveType13 || counts[2]>2 || counts[3]>Level1State::projectilePoolSize
        || counts[4]>Level1State::deathEffectPoolSize) {
        throw std::invalid_argument("Level2 combat scene exceeds its physical constructor/pool membership");
    }
    for(std::size_t i=0;i<allocations.size();++i) {
        if(std::find(allocations.begin(),allocations.begin()+i,allocations[i])!=allocations.begin()+i) {
            throw std::invalid_argument("Level2 combat scene has duplicate registry identities");
        }
    }
    CombatSceneContext context{effects,page,allocations,{},{},{}};
    auto scene=worldScenePlan(assets,view,actors,profile,light,WorldSceneMatrixProfile::level2Periodic,&context);
    return {std::move(scene),std::move(context.observations),std::move(context.allocations),{}, {}};
}
} // namespace stellar7

namespace stellar7 {
FailureModelSceneWork failureModelSceneOperations(const GameplayAssets& assets,const ViewTransform& view,
    const std::vector<FailureModelSceneItem>& items,const WorldSceneProfile profile,
    const std::array<std::int16_t,3> light) {
    return failureModelScenePlan(assets,view,items,profile,light,FailureSceneDomain::legacy);
}
FailureSceneWork failureSceneOperations(const GameplayAssets& assets,const ViewTransform& view,
    const std::vector<FailureSceneItem>& items,const WorldSceneProfile profile,
    const std::array<std::int16_t,3> light,const std::uint16_t page,
    const std::vector<std::uint16_t>& allocations,const FailureEffectProfile effectProfile) {
    return failureScenePlan(assets,view,items,profile,light,page,allocations,effectProfile,FailureSceneDomain::legacy);
}
FailureSceneWork retainedFailureSceneOperations(const GameplayAssets& assets,const ViewTransform& view,
    const std::vector<FailureSceneItem>& items,const WorldSceneProfile profile,
    const std::array<std::int16_t,3> light,const std::uint16_t page,
    const std::vector<std::uint16_t>& allocations) {
    return failureScenePlan(assets,view,items,profile,light,page,allocations,FailureEffectProfile::bitmap,FailureSceneDomain::retainedRoster);
}
} //namespace stellar7

namespace stellar7 {
FailureSceneWork retainedLevel2FailureSceneOperations(const GameplayAssets& assets,const ViewTransform& view,
    const std::vector<FailureSceneItem>& items,const WorldSceneProfile profile,
    const std::array<std::int16_t,3> light,const std::uint16_t page,
    const std::vector<std::uint16_t>& allocations) {
    return failureScenePlan(assets,view,items,profile,light,page,allocations,FailureEffectProfile::bitmap,FailureSceneDomain::level2);
}
} // namespace stellar7
