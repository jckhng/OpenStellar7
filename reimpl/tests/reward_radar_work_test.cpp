#include "stellar7/level1_deployment_render_work.hpp"
#include <cassert>
#include <stdexcept>
#include <array>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace {
using namespace stellar7;
void add(std::uint64_t& hash,const std::uint64_t value) { hash=(hash^value)*1099511628211ULL; }
std::uint64_t operationHash(const std::vector<NormalWorkOperation>& operations) {
    std::uint64_t hash=14695981039346656037ULL,pending=0;
    for (const auto operation:operations) {
        if (operation.kind==NormalWorkKind::instructions) { pending+=operation.count;continue; }
        if (pending!=0) { add(hash,0);add(hash,pending);pending=0; }
        add(hash,static_cast<std::uint64_t>(operation.kind));add(hash,operation.count);
    }
    if (pending!=0) { add(hash,0);add(hash,pending); }
    return hash;
}
std::uint64_t pixelHash(const std::vector<RadarPixelWrite>& pixels) {
    std::uint64_t hash=14695981039346656037ULL;
    for (const auto& pixel:pixels) {
        add(hash,static_cast<std::uint64_t>(pixel.point.x));add(hash,static_cast<std::uint64_t>(pixel.point.y));
        add(hash,pixel.color);add(hash,pixel.page);
    }
    return hash;
}
void rewardRadar() {
    Model3d lift{};lift.flags=1;lift.collisionRadius=748;lift.animationLimits={8};
    Model3d guardian{};guardian.flags=3;guardian.collisionRadius=2792;guardian.animationLimits={8,8,8};
    const IndexedBitmap panel{120,56,std::vector<std::uint8_t>(6720,7)};
    const RadarRetainedState state{680,42,{0xa400,0xa800,0xa000,0xa800,0xa400}};
    const WorldViewSetupInput view{{0,0,200000},{0xc000,0,0},{},0x88,7,{112,205,146,180},{256,-256,362}};
    constexpr std::array<std::uint64_t,2> workHashes{0x8583d7b6b985c4c5ULL,0x95217f5613cfafe0ULL};
    constexpr std::array<std::uint64_t,2> pixelHashes{0xf0782fd0ef434215ULL,0x31739e2c2d161935ULL};
    constexpr std::array<std::size_t,2> pixelCounts{6721,6720};
    for (std::size_t index=0;index<2;++index) {
        const std::vector<RadarActorInput> actors{{{0,{index==0 ? 0 : 300000,6000,0}},897,25,-2,0,
            WorldActorRenderProfile::inlineSelectorNoCallback,17,18,19}};
        const auto result=level1DeploymentCombatRadarOperations(state,view,{},0,actors,{},panel,{228,138},
            lift,guardian,{std::nullopt});
        assert(operationHash(result.radar.operations)==workHashes[index]);
        assert(pixelHash(result.radar.pixels)==pixelHashes[index] && result.radar.pixels.size()==pixelCounts[index]);
        assert(result.radar.actors[0].flags==0 && result.radar.actors[0].projectedY==0 && !result.animations[0]);
    }
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    rewardRadar();return 0;
}
