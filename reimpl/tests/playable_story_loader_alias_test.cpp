#include "playable_detail_render.hpp"
#include "stellar7/level1_loader_work.hpp"

#include <cassert>
#include <stdexcept>

namespace {
using namespace stellar7;
template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
GameplayAssets assets() {
    //Geometry-free composition profile from level1_frame_test; actual source assets are integrated separately.
    GameplayAssets result;
    result.cockpit = {std::vector<std::uint8_t>(320 * 200,93),std::vector<Rgb>(256)};
    result.cockpitSettings = CockpitSettings{81,{}};
    result.scoreFont = {32,8,0,std::vector<IndexedBitmap>(96,{4,8,std::vector<std::uint8_t>(32,1)})};
    result.horizonFrames.assign(4,{256,16,std::vector<std::uint8_t>(256 * 16,7)});
    result.artGroups.resize(5);
    result.artGroups[3] = {{120,56,std::vector<std::uint8_t>(120 * 56,7)},
        {4,1,{0,0,0,0}},{120,18,std::vector<std::uint8_t>(120 * 18,3)}};
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


void compare(const Level1LoaderPages& standard, const Level1LoaderPages& transferred) {
    assert(standard.operations.size()==transferred.operations.size());
    for(std::size_t index=0; index<standard.operations.size(); ++index) {
        const auto& a=standard.operations[index]; const auto& b=transferred.operations[index];
        assert(a.kind==b.kind && a.count==b.count && a.value==b.value);
    }
    assert(standard.pages.frontPage()==0xa000 && standard.pages.backPage()==0xa800);
    assert(transferred.pages.frontPage()==0xa800 && transferred.pages.backPage()==0xa000);
    assert(standard.pages.cachePage()==0xa400 && transferred.pages.cachePage()==0xa400);
    for(std::int16_t y=0; y<200; ++y) {
        for(std::int16_t x=0; x<320; ++x) {
            const ProjectedPoint point{x,y};
            assert(standard.pixels.read(standard.pages.frontPage(),point)
                ==transferred.pixels.read(transferred.pages.frontPage(),point));
            assert(standard.pixels.read(standard.pages.backPage(),point)
                ==transferred.pixels.read(transferred.pages.backPage(),point));
            assert(standard.pixels.read(0xa400,point)==transferred.pixels.read(0xa400,point));
        }
    }
}
}

int main() {
    using namespace stellar7;
    const auto data=assets();
    const auto ordinary=CockpitPageState::fromRecoveredMenu();
    auto transferred=ordinary; transferred.flip();
    IndexedPages pixels;
    auto bounded=GameplayModel::fromNativeStartup();
    const auto boundedCalls=bounded.level1State().rng().calls();
    //Old strict and bounded freshBegin contracts must reject the alternate pair.
    rejects([&] { planLevel1LoaderPages(data,bounded,transferred,pixels); });
    rejects([&] { planPlayableLevel1LoaderPages(data,bounded,transferred,pixels,81); });
    const auto first=planStoryLevel1LoaderPages(data,bounded,ordinary,pixels,81);
    const auto escaped=planStoryLevel1LoaderPages(data,bounded,transferred,pixels,81);
    compare(first,escaped);
    assert(bounded.ticks()==0 && bounded.level1State().rng().calls()==boundedCalls);
    for(const std::int16_t detail : {0,20,21,50,51,70,71,81,82}) {
        auto full=GameplayModel::fromNativeStartup(detail);
        const auto calls=full.level1State().rng().calls();
        rejects([&] { planFullDetailPlayableLevel1LoaderPages(data,full,transferred,pixels,detail); });
        const auto standard=planFullDetailPlayableLevel1LoaderPages(data,full,ordinary,pixels,detail,
            Level1LoaderPageDomain::freshBegin,NativeLevel1BeginPages::storyTransferred);
        const auto alternate=planFullDetailPlayableLevel1LoaderPages(data,full,transferred,pixels,detail,
            Level1LoaderPageDomain::freshBegin,NativeLevel1BeginPages::storyTransferred);
        compare(standard,alternate);
        assert(full.ticks()==0 && full.level1State().rng().calls()==calls);
        assert(full.level2CockpitIndicatorX()==(detail<=50 ? 130 : 138));
    }
    auto full=GameplayModel::fromNativeStartup(0);
    rejects([&] { planFullDetailPlayableLevel1LoaderPages(data,full,ordinary,pixels,0,
        Level1LoaderPageDomain::freshBegin,static_cast<NativeLevel1BeginPages>(99)); });
    rejects([&] { planFullDetailPlayableLevel1LoaderPages(data,full,ordinary,pixels,0,
        Level1LoaderPageDomain::recoveredRetry,NativeLevel1BeginPages::storyTransferred); });
}
