#include "stellar7/radar_state.hpp"

#include <cassert>
#include <stdexcept>

namespace {
template<class Function>
void reject(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}
}

int main() {
    using namespace stellar7;
    const std::vector<IndexedBitmap> group{{120, 56, std::vector<std::uint8_t>(120 * 56, 7)}};
    RadarState owner(group);
    const RadarPages startup{0xa800, 0xa400, 0xa000, 0xa400, 0xa800};
    RadarStateActor first{3, {0, {}}, 1024, 13, -1, 0, WorldActorRenderProfile::noHandles};
    assert(owner.armorStrip() == 0);
    reject([&] { owner.draw(488, {}, 0, {first}, startup, {}); });
    const auto setup = owner.initialize(488, {}, 0, startup);
    assert(setup.actors.empty() && owner.armorStrip() == 30);
    assert(setup.pageCopy && setup.state.pages.draw == startup.front);
    assert(owner.cache().frames.front().header.offset == 0);
    reject([&] { owner.initialize(488, {}, 0, startup); });
    reject([&] { owner.draw(488, {}, 0, {first}, startup, {}); });
    owner.initializeAllocation(first.allocation);
    const auto projected = owner.draw(680, {}, 0, {first}, startup, {});
    assert(owner.armorStrip() == 42 && projected.pageCopy);
    assert(projected.actors[0].projectedX == 158 && projected.actors[0].projectedY == 0);
    const auto retainedX = owner.projection(first.allocation).projectedX;
    first.actor.position.y = 100000;
    const auto clipped = owner.draw(680, {}, 0, {first}, startup, {});
    assert(!clipped.pageCopy && !clipped.actors[0].projected);
    assert(clipped.actors[0].projectedX == retainedX && clipped.actors[0].projectedY == 0);
    assert(owner.projection(first.allocation).cachedCenter.z == 100000);

    RadarStateActor second{47, {0, {1000, 0, 0}}, 2048, 8, -1, 0, WorldActorRenderProfile::noHandles};
    owner.initializeAllocation(second.allocation);
    owner.draw(680, {}, 0, {first, second}, startup, {});
    const auto secondX = owner.projection(second.allocation).projectedX;
    second.actor.position.y = 100000;
    const auto reordered = owner.draw(680, {}, 0, {second, first}, startup, {});
    assert(reordered.actors[0].projectedX == secondX);
    assert(reordered.actors[1].projectedX == retainedX);
    reject([&] { owner.draw(680, {}, 0, {first, first}, startup, {}); });
    reject([&] { owner.initializeAllocation(48); });

    owner.resetArmorStrip();
    assert(owner.armorStrip() == 0);
    assert(owner.projection(first.allocation).projectedX == retainedX);
    owner.initialize(488, {}, 0, startup);
    owner.initializeAllocation(first.allocation);
    const auto reused = owner.draw(488, {}, 0, {first}, startup, {});
    assert(reused.actors[0].projectedX == 0);

    auto bad = first;
    bad.callback = 1;
    reject([&] { owner.draw(488, {}, 0, {bad}, startup, {}); });
    assert(owner.projection(first.allocation).projectedX == 0);
    auto wrongStartup = startup;
    wrongStartup.draw = wrongStartup.cached;
    RadarState fresh(group);
    reject([&] { fresh.initialize(488, {}, 0, wrongStartup); });
    reject([&] { RadarState invalid({{4, 1, {0, 0, 0, 0}}}); });

    RadarState projectileOwner(group);
    projectileOwner.initialize(488, {}, 0, startup);
    constexpr std::size_t projectileSlot = 32;
    constexpr std::uint32_t projectileRadius = 441;
    projectileOwner.initializeAllocation(projectileSlot);
    RadarStateActor projectile{projectileSlot, {0, {}}, projectileRadius, 19, -3, 0,
        WorldActorRenderProfile::inlineSelectorNoCallback};
    const auto visibleProjectile = projectileOwner.draw(488, {}, 0, {projectile}, startup, {});
    const auto previousProjectile = projectileOwner.projection(projectileSlot);
    assert(visibleProjectile.actors[0].projected && visibleProjectile.pixels.back().color == 238);
    projectile.actor.position.y = -30000;
    const auto clippedProjectile = projectileOwner.draw(488, {}, 0, {projectile}, startup, {});
    assert(!clippedProjectile.actors[0].projected);
    assert(clippedProjectile.actors[0].projectedX == previousProjectile.projectedX);
    assert(clippedProjectile.actors[0].projectedRadius == previousProjectile.projectedRadius);
    assert(clippedProjectile.actors[0].projectedY == 0 && clippedProjectile.actors[0].flags == 0);
    assert(clippedProjectile.actors[0].cachedCenter.z == -30000);
    assert(clippedProjectile.pixels.size() == group.front().pixels.size());
    // Retirement and44ba partial reuse do not perform48d4's full pool reset.
    projectileOwner.draw(488, {}, 0, {}, startup, {});
    projectile.actor.position.x = 100000;
    const auto partialReuse = projectileOwner.draw(488, {}, 0, {projectile}, startup, {});
    assert(!partialReuse.actors[0].projected);
    assert(partialReuse.actors[0].projectedX == previousProjectile.projectedX);
    assert(partialReuse.actors[0].projectedRadius == previousProjectile.projectedRadius);
    assert(partialReuse.actors[0].cachedCenter.x == 100000);
    auto unknownProjectile = projectile;
    unknownProjectile.renderProfile = WorldActorRenderProfile::unknown;
    reject([&] { projectileOwner.draw(488, {}, 0, {unknownProjectile}, startup, {}); });

    RadarState effects(group);
    effects.initialize(488, {}, 0, startup);
    constexpr std::size_t effectSlot = 40;
    effects.initializeAllocation(effectSlot);
    ViewTransform mainView = initialLevel1MainView();
    RadarStateActor effect{effectSlot, {0x8c, {0, 50000, 0}}, 0, -1, -4, 0x0ac8,
        WorldActorRenderProfile::bitmapEffectCallback};
    const auto mainProjection = effects.projectBitmapEffect(effectSlot, effect.actor, mainView);
    assert(mainProjection.projected && mainProjection.projectedX == 159 && mainProjection.projectedY == 84);
    const auto clippedEffect = effects.draw(488, {}, 0, {effect}, startup, {});
    assert(!clippedEffect.actors[0].projected && clippedEffect.actors[0].projectedX == 159);
    assert(clippedEffect.actors[0].projectedY == 0 && clippedEffect.actors[0].flags == 0x8c);
    assert(!clippedEffect.cpuWorkQualified && clippedEffect.operations.empty());
    assert(clippedEffect.setup.operations.empty());
    effects.draw(488, {}, 0, {}, startup, {});
    assert(effects.projection(effectSlot).projectedX == 159);
    effect.actor.position = {0, 127, 0};
    const auto nearEffect = effects.projectBitmapEffect(effectSlot, effect.actor, mainView);
    assert(!nearEffect.projected && nearEffect.projectedX == 159);
    effect.actor.flags = nearEffect.flags;
    const auto visibleEffect = effects.draw(488, {}, 0, {effect}, startup, {});
    assert(visibleEffect.actors[0].projected && visibleEffect.pixels.back().color == 78);
    const RadarActorInput strictEffect{effect.actor, 0, -1, -4, 0x0ac8,
        WorldActorRenderProfile::bitmapEffectCallback, 0, 0, 0};
    reject([&] { radarOperations({488, 0, startup}, effects.view(), {}, 0, {strictEffect}, {}, group.front(), {228, 138}); });
    effect.actor = {0x8c, {0, 500, 50}};
    const auto elevatedMain = effects.projectBitmapEffect(effectSlot, effect.actor, mainView);
    assert(elevatedMain.projected && elevatedMain.projectedX == 159 && elevatedMain.projectedY == 72);
    assert(elevatedMain.cachedCenter.z == 50);
    const auto elevatedRadar = effects.draw(488, {}, 0, {effect}, startup, {});
    assert(elevatedRadar.actors[0].cachedCenter.y == 199950);
    assert(elevatedRadar.actors[0].flags == 0x8c && elevatedRadar.actors[0].projectedY == 0);
    assert(!elevatedRadar.cpuWorkQualified && elevatedRadar.operations.empty());
    for (const auto invalidAltitude : {-1, 51, 501}) {
        effect.actor.position.z = invalidAltitude;
        reject([&] { effects.projectBitmapEffect(effectSlot, effect.actor, mainView); });
        reject([&] { effects.draw(488, {}, 0, {effect}, startup, {}); });
    }
    for (const auto altitude : {600, 751, 1400}) {
        effect.actor = {0x8c, {0, 50000, altitude}};
        reject([&] { effects.projectBitmapEffect(effectSlot, effect.actor, mainView); });
        const auto airborne = effects.projectBitmapEffect(effectSlot, effect.actor, mainView,
            RadarPixelDomain::playableCampaign);
        assert(airborne.projected && airborne.projectedX == 159);
        assert(airborne.projectedY == 84 - altitude * 128 / 50000 && airborne.cachedCenter.z == altitude);
        const auto airborneRadar = effects.draw(488, {}, 0, {effect}, startup, {},
            RadarPageLayout::compatibility, RadarPixelDomain::playableCampaign);
        assert(!airborneRadar.cpuWorkQualified && airborneRadar.operations.empty());
        assert(effects.projection(effectSlot).projectedX == airborne.projectedX);
    }
    reject([&] { effects.projectBitmapEffect(effectSlot, effect.actor, mainView,
        static_cast<RadarPixelDomain>(255)); });

    effect.actor = {0x8c, {0, 50000, 0}};
    for (const auto altitude : {220, 2530}) {
        auto jumpView = mainView;
        jumpView.origin.z = altitude;
        reject([&] { effects.projectBitmapEffect(effectSlot, effect.actor, jumpView); });
        const auto projected = effects.projectBitmapEffect(effectSlot, effect.actor, jumpView,
            RadarPixelDomain::playableCampaign);
        assert(projected.projected && projected.projectedX == 159
            && projected.projectedY == 84 + altitude * 128 / 50000
            && projected.cachedCenter.z == -altitude);
    }
    for (const auto altitude : {-1, 2531}) {
        auto badView = mainView;
        badView.origin.z = altitude;
        reject([&] { effects.projectBitmapEffect(effectSlot, effect.actor, badView,
            RadarPixelDomain::playableCampaign); });
    }

    RadarState delivery(group);
    delivery.initialize(48, {}, 0, startup);
    RadarStateActor craft{0, {0, {0, 0, 5000}}, 13052, 7, -1, 0,
        WorldActorRenderProfile::inlineSelectorNoCallback};
    RadarStateActor fixed{16, {0, {1000, 0, 0}}, 868, 30, -2, 0,
        WorldActorRenderProfile::noHandles};
    delivery.initializeAllocation(craft.allocation);
    delivery.initializeAllocation(fixed.allocation);
    const auto firstCraft = delivery.draw(48, {}, 0, {fixed, craft}, startup, {});
    assert(!firstCraft.cpuWorkQualified && firstCraft.operations.empty());
    const auto craftCache = delivery.projection(craft.allocation);
    const auto fixedCache = delivery.projection(fixed.allocation);
    craft.actor.position.x = 100000;
    const auto clippedCraft = delivery.draw(48, {}, 0, {craft, fixed}, startup, {});
    assert(!clippedCraft.actors[0].projected);
    assert(clippedCraft.actors[0].projectedX == craftCache.projectedX);
    assert(clippedCraft.actors[0].projectedRadius == craftCache.projectedRadius);
    assert(clippedCraft.actors[0].flags == 0 && clippedCraft.actors[0].projectedY == 0);
    assert(clippedCraft.actors[0].cachedCenter.x != craftCache.cachedCenter.x);
    assert(delivery.projection(fixed.allocation).projectedX == fixedCache.projectedX);

    CompletionBeamPresentation beam{};
    beam.center = {1, 10000, 5000};
    beam.projected = true;
    beam.anchor = {159, 24};
    beam.radius = 256;
    delivery.retainCompletionBeam(craft.allocation, beam);
    assert(delivery.projection(craft.allocation).projectedX == 159);
    assert(delivery.projection(craft.allocation).projectedY == 24);
    assert(delivery.projection(craft.allocation).projectedRadius == 256);
    beam.projected = false;
    beam.center.x = 100000;
    delivery.retainCompletionBeam(craft.allocation, beam);
    assert(delivery.projection(craft.allocation).flags == 8);
    assert(delivery.projection(craft.allocation).cachedCenter.x == 100000);
    assert(delivery.projection(craft.allocation).projectedX == 159);
    assert(delivery.projection(craft.allocation).projectedY == 24);
    assert(delivery.projection(craft.allocation).projectedRadius == 256);
    craft.renderProfile = WorldActorRenderProfile::inlineSelectorBeamCallback;
    craft.callback = 0xb0b4;
    craft.actor.flags = 8;
    const auto beamRadar = delivery.draw(48, {}, 0, {craft, fixed}, startup, {});
    assert(!beamRadar.cpuWorkQualified && beamRadar.operations.empty());
    assert(!beamRadar.actors[0].projected && beamRadar.actors[0].flags == 8);
    assert(beamRadar.actors[0].projectedY == 0 && beamRadar.actors[0].projectedX == 159);
    assert(beamRadar.actors[0].projectedRadius == 256);
    assert(delivery.projection(fixed.allocation).projectedX == fixedCache.projectedX);
    delivery.initializeAllocation(craft.allocation);
    const auto reusedCraft = delivery.draw(48, {}, 0, {fixed, craft}, startup, {});
    assert(reusedCraft.actors[1].projectedX == 0 && reusedCraft.actors[1].projectedRadius == 0);
    assert(delivery.projection(fixed.allocation).projectedX == fixedCache.projectedX);
}
