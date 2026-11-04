#include "stellar7/level1_frame.hpp"

#include <algorithm>
#include <cassert>
#include <memory>
#include <stdexcept>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace {
using namespace stellar7;
template<class Function> void rejects(Function function) {
    bool rejected = false;
    try { function(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}

GameplayAssets assets() {
    GameplayAssets result;
    result.horizonFrames.assign(4,{256,16,std::vector<std::uint8_t>(256*16,7)});
    result.artGroups.resize(5);
    result.artGroups[3] = {{120,56,std::vector<std::uint8_t>(120*56,7)},
        {4,1,{0,0,0,0}},{120,18,std::vector<std::uint8_t>(120*18,3)}};
    result.artGroups[4].assign(15,{4,1,{0,0,0,0}});
    result.artGroups[4][6].pixels.back() = 9;
    for (std::size_t index = 7; index < 15; ++index) {
        result.artGroups[4][index] = {8,1,std::vector<std::uint8_t>(8,13)};
    }
    result.models.resize(30);
    result.models[1].collisionRadius = 1342;
    result.models[11].collisionRadius = 748;
    result.models[11].flags = 1;
    result.models[11].animationLimits = {8};
    result.models[29].collisionRadius = 862;
    result.sharedModelRegistry = {256,256};
    return result;
}

void sameOperations(const std::vector<NormalWorkOperation>& a,const std::vector<NormalWorkOperation>& b) {
    assert(a.size() == b.size());
    for (std::size_t index = 0; index < a.size(); ++index) {
        assert(a[index].kind == b[index].kind && a[index].count == b[index].count && a[index].value == b[index].value);
    }
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    auto data = assets();
    Level1State level;
    level.reloadAfterFailure(StellarRng{});
    auto actors = level.entities();
    for (auto& actor : actors) { actor.position.y = -16384; }
    auto ground = level.ground();
    auto rng = level.rng();
    std::array<Level1Projectile,Level1State::projectilePoolSize> shots{};
    std::vector<Level1ObjectRef> order;
    for (std::size_t index = 0; index < actors.size(); ++index) { order.push_back({Level1PresentationKind::entity,index}); }
    const CockpitSettings settings{90,{}};
    const Level1PresentationState presentation{{140,4},false};
    auto pages = CockpitPageState::fromRecoveredMenu();
    auto pixels = std::make_unique<IndexedPages>();
    for (const std::uint16_t page : {0xa000,0xa400,0xa800}) {
        for (std::int16_t y = 0; y < 200; ++y) {
            for (std::int16_t x = 0; x < 320; ++x) { pixels->write(page,{x,y},93); }
        }
    }
    RadarState radar(data.artGroups[3]);
    for (std::size_t index = 0; index < actors.size(); ++index) { radar.initializeAllocation(index); }
    pixels->apply(radar.initializeLevel1(680,{},0,
        {pages.cachePage(),pages.backPage(),pages.frontPage(),pages.backPage(),pages.cachePage()}));
    Level1ProjectileRenderState frame{ground,rng,level.player(),actors,shots,order,0};
    Level1CombatRenderContext context{order,{}};
    auto live = std::unique_ptr<Level1CombatFramePlan>(new Level1CombatFramePlan(
        planLevel1EffectCombatFrame(data,frame,context,presentation,680,settings,pages,radar,*pixels)));
    context.liftMembership = Level1CombatLiftMembership::allowRetiredInitial;
    auto permitted = std::unique_ptr<Level1CombatFramePlan>(new Level1CombatFramePlan(
        planLevel1EffectCombatFrame(data,frame,context,presentation,680,settings,pages,radar,*pixels)));
    sameOperations(live->frame.operations,permitted->frame.operations);
    assert(permitted->frame.animationAfterMain.countdown == 5 && permitted->frame.animationAfterRadar.countdown == 4);

    //Late3768 excludes the killed lift; retained storage/cache are not erased.
    actors[1].stateBytes4dTo52[1] = 0;
    order.erase(order.begin()+1);
    context.liftMembership = Level1CombatLiftMembership::required;
    rejects([&] { planLevel1EffectCombatFrame(data,frame,context,presentation,680,settings,pages,radar,*pixels); });
    context.liftMembership = Level1CombatLiftMembership::allowRetiredInitial;
    //No omitted actor may invoke its callback or require its animation metadata.
    data.models[11].animationLimits.clear();
    const auto retainedCache = live->frame.radarState.projection(1);
    auto dead = std::unique_ptr<Level1CombatFramePlan>(new Level1CombatFramePlan(
        planLevel1EffectCombatFrame(data,frame,context,presentation,680,settings,pages,live->frame.radarState,*pixels)));
    assert(dead->frame.scene.centers.size() == 5 && dead->frame.radarActors.size() == 5);
    assert(std::all_of(dead->frame.scene.animationStates.begin(),dead->frame.scene.animationStates.end(),
        [](const auto& value) { return !value; }));
    assert(std::none_of(dead->frame.radarActors.begin(),dead->frame.radarActors.end(),
        [](const auto& actor) { return actor.type == 11; }));
    const auto& afterCache = dead->frame.radarState.projection(1);
    assert(afterCache.cachedCenter == retainedCache.cachedCenter && afterCache.projectedX == retainedCache.projectedX
        && afterCache.projectedY == retainedCache.projectedY && afterCache.projectedRadius == retainedCache.projectedRadius);
    assert(actors[1].stateBytes4dTo52[3] == 0 && actors[1].modelAnimationVariant == 0);
    assert(dead->effects.empty() && dead->allocationOrderAfterMain == order);
    assert(rng.calls() == 16);
    ground = dead->frame.ground.ground;
    rng = dead->frame.ground.rng;
    pages = dead->frame.pages;
    pages.flip();
    pages.beginFrame();
    frame.loopTick = 1;
    auto adjacent = std::unique_ptr<Level1CombatFramePlan>(new Level1CombatFramePlan(
        planLevel1EffectCombatFrame(data,frame,context,dead->frame.presentationAfter,680,settings,pages,
            dead->frame.radarState,dead->frame.pixels)));
    assert(adjacent->frame.radarActors.size() == 5 && adjacent->frame.scene.animationStates.size() == 5);
    assert(adjacent->frame.radarState.projection(1).cachedCenter == retainedCache.cachedCenter);
    auto invalid = context;
    invalid.liftMembership = static_cast<Level1CombatLiftMembership>(99);
    rejects([&] { planLevel1EffectCombatFrame(data,frame,invalid,presentation,680,settings,pages,radar,*pixels); });
    actors[1].primaryType11AnimationSelector = false;
    rejects([&] { planLevel1EffectCombatFrame(data,frame,context,presentation,680,settings,pages,radar,*pixels); });
    actors[1].primaryType11AnimationSelector = true;
    actors[1].collisionRadius = 0;
    rejects([&] { planLevel1EffectCombatFrame(data,frame,context,presentation,680,settings,pages,radar,*pixels); });
    actors[1].collisionRadius = 748;
    actors[1].stateBytes4dTo52[0] = 0xfc;
    rejects([&] { planLevel1EffectCombatFrame(data,frame,context,presentation,680,settings,pages,radar,*pixels); });
    actors[1].stateBytes4dTo52[0] = 0xff;
    auto duplicate = actors[1];
    duplicate.allocationIndex = actors.size();
    actors.push_back(duplicate);
    rejects([&] { planLevel1EffectCombatFrame(data,frame,context,presentation,680,settings,pages,radar,*pixels); });
    actors.pop_back();
    actors[1].stateBytes4dTo52[1] = 1;
    rejects([&] { planLevel1EffectCombatFrame(data,frame,context,presentation,680,settings,pages,radar,*pixels); });
    actors[1].stateBytes4dTo52[1] = 0;
    order.push_back({Level1PresentationKind::entity,1});
    rejects([&] { planLevel1EffectCombatFrame(data,frame,context,presentation,680,settings,pages,radar,*pixels); });
    return 0;
}
