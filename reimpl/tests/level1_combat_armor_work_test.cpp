#include "stellar7/radar_work.hpp"
#include "stellar7/raster_line_work.hpp"
#include <array>
#include <cassert>
#include <stdexcept>
#include <utility>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif
namespace {
using namespace stellar7;
std::uint64_t operationHash(const std::vector<NormalWorkOperation>& operations) {
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t hash = 14695981039346656037ULL;
    std::uint64_t pending = 0;
    const auto append = [&](const auto kind, const auto count) {
        hash = (hash ^ static_cast<std::uint64_t>(kind)) * prime;
        hash = (hash ^ count) * prime;
    };
    for (const auto& operation : operations) {
        if (operation.kind == NormalWorkKind::instructions) {
            pending += operation.count;
            continue;
        }
        if (pending != 0) {
            append(NormalWorkKind::instructions, pending);
            pending = 0;
        }
        append(operation.kind, operation.count);
    }
    if (pending != 0) { append(NormalWorkKind::instructions, pending); }
    return hash;
}

template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
struct Control {
    std::int16_t armor,prior,strip;
    std::uint64_t hash;
    std::size_t columns;
    std::int16_t copyX;
    std::uint16_t copyWidth;
};
constexpr std::array<Control,10> controls{{
    {0,0,0,0x08327807b4eb54bdULL,0,0,0},
    {-1,0,-1,0x652817b4bb2e7423ULL,2,108,12},
    {-16,0,-1,0x652817b4bb2e7423ULL,2,108,12},
    {-17,0,-2,0xbd3b8a3c41f58061ULL,3,108,12},
    {-40,0,-3,0x25b7f3c951cbfe7aULL,4,104,16},
    {-320,0,-20,0x1937bcd2f7903bf4ULL,21,72,48},
    {-320,-20,-20,0x08327807b4eb54bdULL,0,0,0},
    {0,-20,0,0x2e16f37902b65cfdULL,21,72,48},
    {680,-20,42,0x3b899af19ee6dc6eULL,63,72,132},
    {-320,42,-20,0x8b915627d9e85cabULL,63,72,132},
}};
enum class ArmorCaller { combat, effects, bonus, arrival, deployment };
void runControls(const ArmorCaller caller) {
    const WorldViewSetupInput view{{0, 0, 200000}, {0xc000, 0, 0}, {},
        0x88, 7, {112, 205, 146, 180}, {256, -256, 362}};
    const IndexedBitmap panel{120, 56, std::vector<std::uint8_t>(120 * 56, 7)};
    Model3d lift{};
    lift.flags = 1;
    lift.animationLimits = {8};
    std::vector<RadarActorInput> actors{
        {{0, {-2048, 2048, 0}}, 1342, 1, -1, 0, WorldActorRenderProfile::noHandles, 0, 0, 0},
        {{0, {8192, 8192, 1000}}, 748, 11, -1, 0x130c,
            WorldActorRenderProfile::heapSelectorDeferredAnimation, 0, 0, 0},
        {{0, {16384, 16384, 0}}, 862, 29, -2, 0, WorldActorRenderProfile::noHandles, 0, 0, 0},
        {{0, {16384, -16384, 0}}, 862, 29, -2, 0, WorldActorRenderProfile::noHandles, 0, 0, 0},
        {{0, {-16384, 16384, 0}}, 862, 29, -2, 0, WorldActorRenderProfile::noHandles, 0, 0, 0},
        {{0, {-16384, -16384, 0}}, 862, 29, -2, 0, WorldActorRenderProfile::noHandles, 0, 0, 0}};

    const auto entities = actors;
    actors.push_back({{0,{}},441,17,-3,0,WorldActorRenderProfile::inlineSelectorNoCallback,0,0,0});
    for (const auto shot : {false,true}) {
        const auto& list = shot ? actors : entities;
        const auto run = [&](const RadarRetainedState& state) {
            if (caller==ArmorCaller::combat) {
                return level1CombatRadarOperations(state,view,{},0,list,{},panel,{228,138},lift,{5,3,1});
            }
            auto mixed=list;
            mixed.push_back({{0x88,{0,5000,0}},0,-1,-4,0x0ac8,
                WorldActorRenderProfile::bitmapEffectCallback,0,0,0});
            std::vector<std::optional<Level1AnimationState>> animations(mixed.size());
            animations[1]=Level1AnimationState{5,3,1};
            auto work=[&]() {
                if (caller==ArmorCaller::effects) {
                    return level1CombatEffectRadarOperations(state,view,{},0,mixed,{},panel,{228,138},lift,animations);
                }
                if (caller==ArmorCaller::bonus) {
                    return level1BonusCombatRadarOperations(state,view,{},0,mixed,{},panel,{228,138},lift,animations);
                }
                if (caller==ArmorCaller::arrival) {
                    return level1ArrivalCombatRadarOperations(state,view,{},0,mixed,{},panel,{228,138},lift,animations);
                }
                Model3d guardian{};guardian.flags=3;guardian.collisionRadius=2792;
                guardian.animationLimits={8,8,8};
                return level1DeploymentCombatRadarOperations(state,view,{},0,mixed,{},panel,{228,138},lift,guardian,animations);
            }();
            assert(work.radar.actors.size()==mixed.size()&&work.animations.size()==mixed.size());
            assert(work.animations[1]&&work.animations[1]->countdown==2);
            for (std::size_t index=0;index<work.animations.size();++index) {
                assert(work.animations[index].has_value()==(index==1));
            }
            return Level1RadarWork{std::move(work.radar),*work.animations[1]};
        };
        const RadarRetainedState initial{0,0,{0xa400,0xa800,0xa000,0xa800,0xa400}};
        const auto baseline = run(initial);
        const auto prefixCount = baseline.radar.operations.size()-1;
        assert(baseline.radar.operations.back().count == 16);
        if (!shot&&caller==ArmorCaller::combat) {
            const auto strict = level1RadarOperations(initial,view,{},0,entities,{},panel,{228,138},lift,{5,3,1});
            assert(operationHash(strict.radar.operations) == operationHash(baseline.radar.operations));
        }
        for (std::size_t index = 0; index < controls.size(); ++index) {
            const auto& c = controls[index];
            const auto back = static_cast<std::uint16_t>(index%2 == 0 ? 0xa800 : 0xa000);
            const auto front = static_cast<std::uint16_t>(index%2 == 0 ? 0xa000 : 0xa800);
            const RadarRetainedState state{c.armor,c.prior,{0xa400,back,front,back,0xa400}};
            const auto work = run(state);
            const std::vector<NormalWorkOperation> suffix(work.radar.operations.begin()+prefixCount,work.radar.operations.end());
            assert(operationHash(suffix) == c.hash && work.radar.cpuWorkQualified);
            assert(work.radar.state.armorStrip == c.strip && work.animation.countdown == 2);
            assert(work.radar.pixels.size() == baseline.radar.pixels.size()+7*c.columns);
            assert(work.radar.state.pages.cached == 0xa400 && work.radar.state.pages.back == back
                && work.radar.state.pages.front == front);
            if (c.columns == 0) {
                assert(!work.radar.pageCopy && work.radar.drawColor == 232);
                assert(work.radar.state.pages.draw == back && work.radar.state.pages.copy == 0xa400);
                continue;
            }
            assert(work.radar.pageCopy && work.radar.pageCopy->sourcePage == back
                && work.radar.pageCopy->destinationPage == front);
            assert(work.radar.pageCopy->position.x == c.copyX && work.radar.pageCopy->position.y == 189);
            assert(work.radar.pageCopy->coveredWidth == c.copyWidth && work.radar.pageCopy->height == 7);
            assert(work.radar.state.pages.draw == front && work.radar.state.pages.copy == back);
            const auto color = static_cast<std::uint8_t>(c.strip < c.prior ? 0 : 239);
            const auto first = 112+2*(c.strip < c.prior ? c.strip : c.prior);
            for (std::size_t column = 0; column < c.columns; ++column) {
                for (std::size_t row = 0; row < 7; ++row) {
                    const auto& pixel = work.radar.pixels[baseline.radar.pixels.size()+7*column+row];
                    assert(pixel.point.x == first+2*column && pixel.point.y == 189+row);
                    assert(pixel.color == color && pixel.page == back);
                }
            }
        }
        for (unsigned mutation = 0; mutation < 5; ++mutation) {
            auto state = initial;
            if (mutation == 0) { state.armor = -321; }
            if (mutation == 1) { state.armor = 681; }
            if (mutation == 2) { state.armorStrip = -21; }
            if (mutation == 3) { state.armorStrip = 43; }
            if (mutation == 4) { state.armorStrip.reset(); }
            rejects([&] { run(state); });
        }
    }
    const RadarRetainedState negative{-1,0,{0xa400,0xa800,0xa000,0xa800,0xa400}};
    rejects([&] { level1RadarOperations(negative,view,{},0,entities,{},panel,{228,138},lift,{5,3,1}); });
    auto primary = entities;
    primary.push_back({{0,{}},134,23,-3,0,WorldActorRenderProfile::inlineSelectorNoCallback,0,0,0});
    rejects([&] { level1ProjectileRadarOperations(negative,view,{},0,primary,{},panel,{228,138},lift,{5,3,1}); });
    const RadarRetainedState positive{0,0,{0xa400,0xa800,0xa000,0xa800,0xa400}};
    rejects([&] { level1ProjectileRadarOperations(positive,view,{},0,entities,{},panel,{228,138},lift,{5,3,1}); });
    const RadarRetainedState strict{-1,0,{0xa800,0xa000,0xa400,0xa000,0xa800}};
    rejects([&] { radarOperations(strict,view,{},0,{}, {},panel,{228,138}); });
    auto native = strict; native.armor = -128;
    assert(!radarPresentationPixels(native,view,{},0,{}, {},panel,{228,138}).cpuWorkQualified);
    native.armor = -129;
    rejects([&] { radarPresentationPixels(native,view,{},0,{}, {},panel,{228,138}); });
}
void lineGuards() {
    for (const auto x : {72,110,112,196}) {
        const auto line = rasterCombatArmorColumnOperations(static_cast<std::int16_t>(x));
        assert(line.pixels.size() == 7 && line.pixels.front().x == x && line.pixels.front().y == 189);
        assert(line.pixels.back().x == x && line.pixels.back().y == 195);
    }
    for (const auto x : {70,73,198}) {
        rejects([&] { rasterCombatArmorColumnOperations(static_cast<std::int16_t>(x)); });
    }
    rejects([] { rasterArmorColumnOperations(110); });
    assert(operationHash(rasterCombatArmorColumnOperations(112).operations)
        == operationHash(rasterArmorColumnOperations(112).operations));
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    _set_error_mode(_OUT_TO_STDERR);
#endif
    for (const auto caller : {ArmorCaller::combat,ArmorCaller::effects,ArmorCaller::bonus,
            ArmorCaller::arrival,ArmorCaller::deployment}) {
        runControls(caller);
    }
    lineGuards();
}
