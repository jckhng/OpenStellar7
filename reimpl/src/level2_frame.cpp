#include "stellar7/level2_frame.hpp"
#include "stellar7/ground_steering_work.hpp"
#include "stellar7/indexed_render.hpp"

#include <optional>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::size_t actorCount = 7;
constexpr std::size_t panelGroup = 3;
constexpr std::size_t overlayGroup = 4;
constexpr std::size_t firstGroundBitmap = 15;
constexpr std::size_t groundBitmapCount = 8;
constexpr std::size_t indicatorIndex = 6;
constexpr Viewport groundViewport{24, 295, 24, 145};
constexpr Viewport worldViewport{24, 294, 24, 145};
constexpr std::uint64_t worldSetupCallerDispatches = 13;
constexpr std::uint8_t sightColor = 95;
constexpr HorizonColors level2Colors{64, 78};

CockpitWorkBitmaps cockpitBitmaps(const GameplayAssets& assets) {
    constexpr std::size_t stripIndex = 1;
    constexpr std::size_t backgroundIndex = 2;
    if (assets.artGroups.size() <= overlayGroup || assets.artGroups[panelGroup].size() <= stripIndex
        || assets.artGroups[overlayGroup].size() <= indicatorIndex) {
        throw std::invalid_argument("owned cockpit requires original bitmaps");
    }
    CockpitWorkBitmaps bitmaps{};
    bitmaps.strip = assets.artGroups[panelGroup][stripIndex];
    bitmaps.indicator = assets.artGroups[overlayGroup][indicatorIndex];
    if (assets.artGroups[panelGroup].size() > backgroundIndex) {
        bitmaps.matchingPageBackground = assets.artGroups[panelGroup][backgroundIndex];
    }
    for (std::size_t i = 0; i < bitmaps.overlays.size(); ++i) {
        bitmaps.overlays[i] = assets.artGroups[overlayGroup][i];
    }
    return bitmaps;
}
}

Level2FramePlan planLevel2Frame(const GameplayAssets& assets, const Level2RenderState& frame,
    const std::uint16_t loopTick, const std::int16_t armor, const HorizonWorkState& horizon,
    const CockpitSettings& settings, const CockpitPageState& pages,
    const RadarState& radar, const IndexedPages& pixels) {
    return planLevel2Frame(assets,frame,loopTick,armor,horizon,settings,pages,radar,pixels,
        Level2FrameDomain::retainedForward);
}

static Level2FramePlan level2FramePlanCore(const GameplayAssets& assets, const Level2RenderState& frame,
    const std::uint16_t loopTick, const std::int16_t armor, const HorizonWorkState& horizon,
    const CockpitSettings& settings, const CockpitPageState& pages,
    const RadarState& radar, const IndexedPages& pixels, const Level2FrameDomain domain, const std::vector<std::size_t>* physicalKeys, const std::vector<Level1ObjectRef>* allocations,
    const Level2RadarDomain radarDomain=Level2RadarDomain::combat) {
    const auto periodic=radarDomain==Level2RadarDomain::periodicLifecycle;
    if ((domain != Level2FrameDomain::retainedForward && domain != Level2FrameDomain::flatControls)
        || (allocations ? !physicalKeys || frame.entities.size()<actorCount
                || frame.entities.size()>(periodic ? 18 : actorCount)+Level1State::projectilePoolSize+Level1State::deathEffectPoolSize
                || frame.order.size()>frame.entities.size()
            : frame.entities.size()!=actorCount || frame.order.size()!=actorCount)
        || assets.artGroups.size() <= overlayGroup
        || assets.artGroups[overlayGroup].size() < firstGroundBitmap + groundBitmapCount) {
        throw std::invalid_argument("complete Level2 frame requires seven retained actors and original ground bitmaps");
    }
    if (physicalKeys) {
        if (physicalKeys->size()!=frame.entities.size()) {
            throw std::invalid_argument("Level2 constructor frame requires current physical keys");
        }
        std::array<bool,48> selected{};
        for (const auto key:*physicalKeys) {
            if (key>=selected.size() || selected[key]) {
                throw std::invalid_argument("Level2 constructor frame has duplicate/invalid physical keys");
            }
            selected[key]=true;
        }
    }
    Level2FramePlan plan{{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, pages, radar, pixels, {}};
    for (std::size_t i = 0; i < plan.groundBitmaps.size(); ++i) {
        plan.groundBitmaps[i] = assets.artGroups[overlayGroup][firstGroundBitmap+i];
    }
    const auto& player = frame.player;
    const GroundControlState ground{frame.ground.points(),player.yawVelocity,player.forwardVelocity,player.position.z};
    plan.ground = periodic ? level2GroundSpriteExecution(ground,frame.rng,plan.groundBitmaps,groundViewport)
        : domain == Level2FrameDomain::flatControls
        ? groundSteeringExecution(ground,frame.rng,plan.groundBitmaps,groundViewport)
        : groundClippedExecution(ground,frame.rng,plan.groundBitmaps,groundViewport);
    const auto sourceHorizon = physicalKeys
        ? std::optional<HorizonExecutionWork>{level2HorizonExecution(horizon, assets.horizonFramesForLevel(1))}
        : std::nullopt;
    plan.operations = sourceHorizon ? sourceHorizon->operations : horizonOperations(horizon);
    const auto append = [&](const auto& work) {
        plan.operations.insert(plan.operations.end(), work.begin(), work.end());
    };
    append(plan.ground.operations);
    plan.world = worldViewSetupOperations({player.position, {0, 0, 0},
        {player.pitch, player.roll, player.yaw}, 0x88, 7, worldViewport, {256, -256, 362}});
    plan.operations.push_back({NormalWorkKind::instructions, worldSetupCallerDispatches});
    append(plan.world.operations);
    std::vector<WorldSceneActor> actors;
    std::vector<std::optional<Level1CombatEffectView>> effectViews;
    for (const auto index : frame.order) {
        const auto& actor = frame.entities.at(index);
        const auto key=physicalKeys ? physicalKeys->at(index) : actor.allocationIndex;
        WorldSceneActor scene{{actor.flags,actor.position},{actor.pitch,actor.roll,actor.yaw},
            actor.type,actor.collisionRadius,0,0};
        std::optional<Level1CombatEffectView> effect;
        auto profile=actor.renderProfile;
        std::uint16_t callback=0;
        if(allocations && (actor.type==23 || actor.type==19 || (periodic && (actor.type==17 || actor.type==20)))) {
            if(key<32 || key>=40 || actor.renderProfile!=WorldActorRenderProfile::inlineSelectorNoCallback
                || actor.stateBytes4dTo52[0]!=253 || actor.stateBytes4dTo52[2]!=0) {
                throw std::invalid_argument("Level2 main lost its actual FD identity/inline selector");
            }
            scene.inlineSelector=actor.stateBytes4dTo52[2];
        } else if(allocations && actor.type==-1) {
            if(key<40 || key>=48 || actor.stateBytes4dTo52[0]!=252) {
                throw std::invalid_argument("Level2 main lost its actual FC identity");
            }
            const auto& cache=plan.radarState.projection(key);
            callback=actor.stateBytes4dTo52[4]!=0 ? 0x0ac8 : 0;
            scene.drawCallback=callback;profile=WorldActorRenderProfile::bitmapEffectCallback;
            effect=Level1CombatEffectView{key-40,{actor.flags,actor.position},actor.collisionRadius,
                {actor.flags,{cache.projectedX,cache.projectedY},cache.projectedRadius,
                    actor.stateBytes4dTo52[1],actor.stateBytes4dTo52[4],callback},actor.targetHeading};
        } else if(actor.renderProfile!=WorldActorRenderProfile::noHandles) {
            throw std::invalid_argument("world scene requires owned no-handle actor provenance");
        }
        actors.push_back(std::move(scene));effectViews.push_back(std::move(effect));
        plan.radarActors.push_back({key,{actor.flags,actor.position},actor.collisionRadius,
            actor.type,static_cast<std::int8_t>(actor.stateBytes4dTo52[0]),callback,profile});
    }
    const WorldSceneProfile sceneProfile{assets.sharedModelRegistry[0],assets.sharedModelRegistry[1],0,0};
    if(allocations) {
        const std::array<std::int16_t,3> light{static_cast<std::int16_t>(plan.world.transformedLight.x),
            static_cast<std::int16_t>(plan.world.transformedLight.y),static_cast<std::int16_t>(plan.world.transformedLight.z)};
        const auto scene=periodic ? level2PeriodicSceneOperations : level2CombatSceneOperations;
        plan.combat=scene(assets,plan.world.view,actors,effectViews,sceneProfile,light,
            pages.drawPage(),*allocations);
        plan.scene=plan.combat->scene;
        for(const auto& observation:plan.combat->effects) {
            auto& radarActor=plan.radarActors.at(observation.ordinal);
            radarActor.actor.flags=observation.work.state.flags;radarActor.callback=observation.work.state.callback;
            plan.radarState.retainLevel1CombatEffectProjection(radarActor.allocation,observation.work,
                Level1EffectProjectionDomain::level2Combat);
        }
    } else {
        const std::array<std::int16_t,3> light{static_cast<std::int16_t>(plan.world.transformedLight.x),
            static_cast<std::int16_t>(plan.world.transformedLight.y),static_cast<std::int16_t>(plan.world.transformedLight.z)};
        plan.scene=physicalKeys ? worldSceneOperations(assets,plan.world.view,actors,sceneProfile,light,
            WorldSceneMatrixProfile::level2Constructor)
            : worldSceneOperations(assets,plan.world.view,actors,sceneProfile);
    }
    append(plan.scene.operations);
    SightRefreshInput refresh{{}, player.yaw};
    for (const auto index : frame.order) {
        const auto& actor = frame.entities.at(index);
        refresh.targets.push_back({actor.type, actor.targetHeading});
    }
    plan.sight = sightOperations(loopTick, frame.alternateSightPhase, plan.world.view.viewport, sightColor, refresh);
    append(plan.sight.operations);
    plan.cockpitBitmaps = cockpitBitmaps(assets);
    plan.pages.selectCockpitCopy();
    const auto relation = plan.pages.cachedPageDiffersFromDraw() ? CockpitPageRelation::differing : CockpitPageRelation::matching;
    plan.cockpit = cockpitOperations({frame.cockpitIndicatorX, frame.cockpitIndicatorVelocity, settings.viewSize, relation},
        plan.cockpitBitmaps);
    append(plan.cockpit.operations);
    plan.radarEntryPages = {plan.pages.cachePage(), plan.pages.backPage(), plan.pages.frontPage(),
        plan.pages.drawPage(), plan.pages.copyPage()};
    plan.radar = allocations
        ? plan.radarState.drawLevel2Combat(armor,player.position,player.yaw,plan.radarActors,plan.radarEntryPages,{0,0},radarDomain)
        : physicalKeys
        ? plan.radarState.drawLevel2Constructor(armor,player.position,player.yaw,plan.radarActors,plan.radarEntryPages,{0,0})
        : plan.radarState.draw(armor,player.position,player.yaw,plan.radarActors,plan.radarEntryPages,{0,0});
    append(plan.radar.operations);
    const auto drawPage = plan.pages.drawPage();
    auto effects = sourceHorizon ? std::vector<IndexedPageEffect>{}
        : horizonPageEffects(horizon, assets.horizonFramesForLevel(1), level2Colors, drawPage);
    const auto appendPixels = [&](const auto& writes) {
        for (const auto& pixel : writes) {
            effects.emplace_back(RadarPixelWrite{pixel.point, pixel.color, drawPage});
        }
    };
    if (sourceHorizon) {
        appendPixels(sourceHorizon->pixels);
    }
    appendPixels(plan.ground.pixels);
    appendPixels(plan.scene.pixels);
    appendPixels(plan.sight.pixels);
    const auto cockpitEffects = cockpitPageEffects(plan.cockpit, plan.cockpitBitmaps, plan.pages.cachePage(), drawPage);
    effects.insert(effects.end(), cockpitEffects.begin(), cockpitEffects.end());
    for (const auto& pixel : plan.radar.pixels) {
        effects.emplace_back(pixel);
    }
    if (plan.radar.pageCopy) {
        effects.emplace_back(*plan.radar.pageCopy);
    }
    plan.pixels.apply(effects);
    return plan;
}

Level2FramePlan planLevel2Frame(const GameplayAssets& assets, const Level2RenderState& frame,
    const std::uint16_t loopTick, const std::int16_t armor, const HorizonWorkState& horizon,
    const CockpitSettings& settings, const CockpitPageState& pages,
    const RadarState& radar, const IndexedPages& pixels, const Level2FrameDomain domain) {
    return level2FramePlanCore(assets,frame,loopTick,armor,horizon,settings,pages,radar,pixels,domain,nullptr,nullptr);
}

Level2FramePlan planRecoveredLevel2ConstructorFrame(const GameplayAssets& assets, const Level2RenderState& frame,
    const std::uint16_t loopTick, const std::int16_t armor, const HorizonWorkState& horizon,
    const CockpitSettings& settings, const CockpitPageState& pages,
    const RadarState& radar, const IndexedPages& pixels, const Level2FrameDomain domain,
    const std::vector<std::size_t>& physicalKeys) {
    return level2FramePlanCore(assets,frame,loopTick,armor,horizon,settings,pages,radar,pixels,domain,&physicalKeys,nullptr);
}

void commitLevel2Frame(const Level2FramePlan& plan, Level2RenderState frame,
    CockpitPageState& pages, RadarState& radar, IndexedPages& pixels) {
    frame.ground = plan.ground.ground;
    frame.rng = plan.ground.rng;
    frame.alternateSightPhase = plan.sight.alternatePhase;
    frame.sightExecution = Level2SightExecution::rendered;
    frame.cockpitIndicatorX = plan.cockpit.state.indicatorX;
    frame.cockpitIndicatorVelocity = plan.cockpit.state.indicatorVelocity;
    frame.cockpitExecution = Level2CockpitExecution::rendered;
    pages = plan.pages;
    pixels = plan.pixels;
    if (plan.radar.pageCopy) {
        pages.selectArmorCopy();
    }
    radar = plan.radarState;
}

}

namespace stellar7 {
Level2FramePlan planRecoveredLevel2CombatFrame(const GameplayAssets& assets, const Level2RenderState& frame,
    const std::uint16_t loopTick, const std::int16_t armor, const HorizonWorkState& horizon,
    const CockpitSettings& settings, const CockpitPageState& pages,
    const RadarState& radar, const IndexedPages& pixels,
    const std::vector<std::size_t>& physicalKeys, const std::vector<Level1ObjectRef>& allocationOrder,
    const Level2RadarDomain domain) {
    return level2FramePlanCore(assets,frame,loopTick,armor,horizon,settings,pages,radar,pixels,
        Level2FrameDomain::flatControls,&physicalKeys,&allocationOrder,domain);
}
Level2FramePlan planRecoveredLevel2PeriodicFrame(const GameplayAssets& assets, const Level2RenderState& frame,
    const std::uint16_t loopTick, const std::int16_t armor, const HorizonWorkState& horizon,
    const CockpitSettings& settings, const CockpitPageState& pages,
    const RadarState& radar, const IndexedPages& pixels,
    const std::vector<std::size_t>& physicalKeys, const std::vector<Level1ObjectRef>& allocationOrder,
    const Level2RadarDomain domain) {
    if(domain!=Level2RadarDomain::periodicLifecycle) {throw std::invalid_argument("Level2 periodic frame requires explicit source ancestry");}
    return level2FramePlanCore(assets,frame,loopTick,armor,horizon,settings,pages,radar,pixels,
        Level2FrameDomain::flatControls,&physicalKeys,&allocationOrder,domain);
}
} // namespace stellar7
