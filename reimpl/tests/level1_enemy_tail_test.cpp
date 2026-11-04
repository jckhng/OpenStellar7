#include "stellar7/frame_tail_work.hpp"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace {
using namespace stellar7;

std::uint64_t operationHash(const std::vector<NormalWorkOperation>& operations) {
    std::vector<NormalWorkOperation> canonical;
    for (const auto& operation : operations) {
        if (!canonical.empty() && operation.kind == NormalWorkKind::instructions
            && canonical.back().kind == operation.kind) {
            canonical.back().count += operation.count;
        } else {
            canonical.push_back(operation);
        }
    }
    std::uint64_t result = 14695981039346656037ULL;
    for (const auto& operation : canonical) {
        for (const auto value : {static_cast<std::uint64_t>(operation.kind),operation.count,
                static_cast<std::uint64_t>(operation.value)}) {
            for (unsigned shift = 0; shift < 64; shift += 8) {
                result = (result ^ ((value >> shift) & 255U)) * 1099511628211ULL;
            }
        }
    }
    return result;
}

template<class Function>
void rejects(Function function) {
    bool rejected = false;
    try { function(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
}

int main() {
#ifdef _MSC_VER
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
    using namespace stellar7;
    StellarRng rng;
    for (unsigned draw = 0; draw < 28; ++draw) { rng.next(); }
    FrameTailGuards guards{680,{}, {},0x1e,0x1e,false,false,false,false,false,true,{}};
    FrameTailState state{29,{321,-654,0},0,0,{0,3,12},
        {{1,{-2048,2048,0}},{11,{8192,8192,1000}},{29,{16384,16384,0}},
            {29,{16384,-16384,0}},{29,{-16384,16384,0}},{29,{-16384,-16384,0}},
            {17,{500,1500,0}}}};
    //Pinned original whole0312..03E8 controls, including retained timer work.
    const auto ordinary = level1CombatFrameTailOperations(state,guards,rng);
    assert(operationHash(ordinary.afterEntryFade) == 13288641477616345650ULL);
    assert((ordinary.state.actors.back().position == FixedPosition{179,2154,0}));
    assert((ordinary.state.weaponTimers == std::array<std::uint8_t,3>{0,2,11}));
    assert(ordinary.rng.calls() == rng.calls());
    guards.armor = 640;
    const auto damaged = level1CombatFrameTailOperations(state,guards,rng);
    assert(operationHash(damaged.afterEntryFade) == operationHash(ordinary.afterEntryFade));
    assert(damaged.state.actors.back().position == ordinary.state.actors.back().position);
    guards.armor = 199;
    rejects([&] { level1CombatFrameTailOperations(state,guards,rng); });
    guards.armor = 681;
    rejects([&] { level1CombatFrameTailOperations(state,guards,rng); });
    guards.armor = 680;
    rejects([&] { level1ProjectileFrameTailOperations(state,guards,rng); });
    rejects([&] { level1FrameTailOperations(state,guards,rng); });
    state.loopTick = 32;
    state.playerPosition = {50000,-50000,0};
    guards.periodic = FrameTailPeriodicInput{0,2,2,10,10,rng.indexA(),rng.indexB(),0};
    auto periodic = level1CombatFrameTailOperations(state,guards,rng);
    assert(operationHash(periodic.afterEntryFade) == 14961967641381174426ULL);
    assert((periodic.state.actors.back().position == FixedPosition{16036,-14036,0}));
    guards.armor = 640;
    const auto damagedPeriodic = level1CombatFrameTailOperations(state,guards,rng);
    assert(operationHash(damagedPeriodic.afterEntryFade) == operationHash(periodic.afterEntryFade));
    assert(damagedPeriodic.state.actors.back().position == periodic.state.actors.back().position);
    guards.armor = 680;
    auto expected = rng;
    expected.next();
    assert(periodic.rng.calls() == expected.calls());
    for (unsigned word = 0; word < 2 * StellarRng::wordCount; ++word) {
        assert(periodic.rng.next() == expected.next());
    }
    state.actors.back().type = 20;
    rejects([&] { level1CombatFrameTailOperations(state,guards,rng); });
}
