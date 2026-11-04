#include "stellar7/gameplay.hpp"
#include "stellar7/radar_state.hpp"

#include <cassert>
#include <stdexcept>

namespace stellar7 {
struct Level1ReloadTestAccess {
    static void level(Level1State& state) {state.levelIndex_=1;}
};
struct GameplayModelTestAccess {
    static GameplayModel mapped() {
        GameplayModel model;model.activeCampaignLevel_=1;Level1ReloadTestAccess::level(model.level1_);
        model.level2Entities_.assign(7,{});
        model.level2EntityActorPoolSlots_={0,1,2,3,16,16,4};
        model.level2EntityFixedPoolSlots_={16,16,16,16,0,1,16};
        model.level2EntityProjectilePoolSlots_.assign(7,8);
        model.level2EntityDeathEffectPoolSlots_.assign(7,8);
        return model;
    }
};
}
namespace {
using namespace stellar7;
template<class Function> void rejects(Function action) {
    bool rejected=false;try {action();} catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
}
bool same(const std::vector<NormalWorkOperation>& a,const std::vector<NormalWorkOperation>& b) {
    if(a.size()!=b.size()) {return false;}
    for(std::size_t i=0;i<a.size();++i) {
        if(a[i].kind!=b[i].kind || a[i].count!=b[i].count || a[i].value!=b[i].value) {return false;}
    }
    return true;
}
}
int main() {
    using namespace stellar7;
    const IndexedBitmap panel{120,56,std::vector<std::uint8_t>(120*56,7)};
    const WorldViewSetupInput view{{0,0,200000},{0xc000,0,0},{0,0,0},
        0x88,7,{112,205,146,180},{256,-256,362}};
    std::vector<RadarActorInput> actors;
    const std::array<std::int16_t,7> types{13,8,8,13,30,30,8};
    for(std::size_t i=0;i<types.size();++i) {
        const auto type=types[i];
        actors.push_back({{0,{static_cast<std::int32_t>(i*2048)-6144,4096,0}},
            type==8?860U:type==13?559U:868U,type,static_cast<std::int16_t>(type==30?-2:-1),
            0,WorldActorRenderProfile::noHandles,17,18,19});
    }
    const RadarPages lifecyclePages{0xa400,0xa000,0xa800,0xa000,0xa400};
    auto waveActors=actors;
    waveActors.push_back(actors.front());
    const RadarRetainedState waveIncoming{680,42,lifecyclePages};
    rejects([&] {level2CombatRadarOperations(waveIncoming,view,{},0,waveActors,{},panel,{228,138},
        Level2RadarDomain::playerLifecycle);});
    const auto wave=level2CombatRadarOperations(waveIncoming,view,{},0,waveActors,{},panel,{228,138},
        Level2RadarDomain::periodicLifecycle);
    assert(wave.cpuWorkQualified && wave.actors.size()==waveActors.size());
    waveActors.push_back(actors.front());
    rejects([&] {level2CombatRadarOperations(waveIncoming,view,{},0,waveActors,{},panel,{228,138},
        Level2RadarDomain::periodicLifecycle);});
    for(const auto armor:{199,0,-1,-320,-896}) {
        const RadarRetainedState incoming{static_cast<std::int16_t>(armor),42,lifecyclePages};
        const auto actual=level2CombatRadarOperations(incoming,view,{},0,actors,{},panel,{228,138},
            Level2RadarDomain::playerLifecycle);
        assert(actual.cpuWorkQualified && actual.pageCopy && !actual.operations.empty());
        assert(actual.state.armorStrip==(armor>=0 ? armor/16 : (armor-15)/16));
        assert(actual.pageCopy->position.x>=0 && actual.pageCopy->position.x+actual.pageCopy->coveredWidth<=320);
        for(const auto& pixel:actual.pixels) {assert(pixel.point.x>=0 && pixel.point.x<320);}
        if(armor<0) {
            rejects([&]{level2CombatRadarOperations(incoming,view,{},0,actors,{},panel,{228,138});});
        } else {
            const auto old=level2CombatRadarOperations(incoming,view,{},0,actors,{},panel,{228,138});
            assert(same(actual.operations,old.operations) && actual.pixels.size()==old.pixels.size());
        }
    }
    rejects([&]{level2CombatRadarOperations({-897,42,lifecyclePages},view,{},0,actors,{},panel,{228,138},
        Level2RadarDomain::playerLifecycle);});
    // Periodic FD children reuse the established Type19/23 radar arithmetic.
    for(const std::int16_t type:{17,20}) {
        const auto radius=type==17?441U:134U;
        const RadarActorInput child{{0,{-8340,-12480,0}},radius,type,-3,0,
            WorldActorRenderProfile::inlineSelectorNoCallback,154,0,0};
        rejects([&]{level2CombatRadarOperations({680,42,lifecyclePages},view,{},22896,
            {child},{},panel,{228,138});});
        rejects([&]{level2CombatRadarOperations({680,42,lifecyclePages},view,{},22896,
            {child},{},panel,{228,138},Level2RadarDomain::playerLifecycle);});
        const auto actual=level2CombatRadarOperations({680,42,lifecyclePages},view,{},22896,
            {child},{},panel,{228,138},Level2RadarDomain::periodicLifecycle);
        auto established=child;established.type=type==17?19:23;
        const auto reference=level2CombatRadarOperations({680,42,lifecyclePages},view,{},22896,
            {established},{},panel,{228,138});
        assert(actual.cpuWorkQualified && same(actual.operations,reference.operations));
        assert(actual.actors.size()==1 && actual.pixels.size()==reference.pixels.size());
        assert(actual.actors[0].center==reference.actors[0].center);
        assert(actual.actors[0].cachedCenter==reference.actors[0].cachedCenter);
        assert(actual.actors[0].projectedX==reference.actors[0].projectedX);
        assert(actual.actors[0].projectedY==reference.actors[0].projectedY);
        assert(actual.actors[0].projectedRadius==reference.actors[0].projectedRadius);
        for(std::size_t i=0;i<actual.pixels.size();++i) {
            assert(actual.pixels[i].point.x==reference.pixels[i].point.x);
            assert(actual.pixels[i].point.y==reference.pixels[i].point.y);
            assert(actual.pixels[i].color==reference.pixels[i].color);
            assert(actual.pixels[i].page==reference.pixels[i].page);
        }
        auto wrong=child;++wrong.radius;
        rejects([&]{level2CombatRadarOperations({680,42,lifecyclePages},view,{},22896,
            {wrong},{},panel,{228,138},Level2RadarDomain::periodicLifecycle);});
        wrong=child;wrong.callback=1;
        rejects([&]{level2CombatRadarOperations({680,42,lifecyclePages},view,{},22896,
            {wrong},{},panel,{228,138},Level2RadarDomain::periodicLifecycle);});
    }
    const auto model=GameplayModelTestAccess::mapped();
    const auto keys=model.recoveredLevel2RenderKeys();
    assert((keys==std::vector<std::size_t>{0,1,2,3,16,17,4}));
    for(const auto back:{std::uint16_t{0xa000},std::uint16_t{0xa800}}) {
        const auto front=static_cast<std::uint16_t>(back==0xa000?0xa800:0xa000);
        const RadarPages pages{0xa400,back,front,back,0xa400};
        RadarState owner({panel});owner.initializeLevel1(680,{},0,pages);
        for(const auto key:{0U,1U,2U,3U,4U,5U,6U,16U,17U}) {owner.initializeAllocation(key);}
        std::vector<RadarStateActor> inputs;
        for(std::size_t i=0;i<actors.size();++i) {
            const auto& a=actors[i];inputs.push_back({keys[i],a.actor,a.radius,a.type,a.category,a.callback,a.renderProfile});
        }
        for(const auto yaw:{std::uint16_t{0},std::uint16_t{0xffb0}}) {
            const auto actual=owner.drawLevel2Constructor(680,{},yaw,inputs,pages,{});
            assert(actual.cpuWorkQualified && !actual.operations.empty() && !actual.pageCopy);
            assert(actual.state.pages.cached==0xa400 && actual.state.pages.back==back);
            assert(owner.projection(16).cachedCenter.y!=0 && owner.projection(17).cachedCenter.y!=0);
            assert(owner.projection(5).cachedCenter==FixedPosition{} && owner.projection(6).cachedCenter==FixedPosition{});
            const RadarRetainedState retained{680,42,pages};
            const auto explicitWork=level2ConstructorRadarOperations(retained,view,{},yaw,actors,{},panel,{228,138});
            auto legacy=retained;
            legacy.pages={0xa800,0xa000,0xa400,0xa000,0xa800};
            const auto sameBody=radarOperations(legacy,view,{},yaw,actors,{},panel,{228,138});
            assert(same(explicitWork.operations,sameBody.operations));
            rejects([&] {radarOperations(retained,view,{},yaw,actors,{},panel,{228,138});});
            rejects([&] {level2ConstructorRadarOperations(legacy,view,{},yaw,actors,{},panel,{228,138});});
        }
        auto semantic=inputs;semantic[4].allocation=5;semantic[5].allocation=6;
        rejects([&] {owner.drawLevel2Constructor(680,{},0,semantic,pages,{});});
        auto wrong=actors;wrong.front().type=11;
        rejects([&] {level2ConstructorRadarOperations({680,42,pages},view,{},0,wrong,{},panel,{228,138});});
    }
}
