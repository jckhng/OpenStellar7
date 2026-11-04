#include "stellar7/combat_effect_work.hpp"
#include "stellar7/radar_state.hpp"
#include <cassert>
#include <stdexcept>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace {
using namespace stellar7;
template<class Action> void rejects(Action action) {
    bool rejected=false;
    try { action(); } catch (const std::invalid_argument&) { rejected=true; }
    assert(rejected);
}
void sameProjection(const RadarState& owner,const FailureEffectWork& receipt) {
    const auto& projection=owner.projection(32);
    assert(projection.cachedCenter==receipt.cachedCenter && projection.flags==receipt.state.flags);
    assert(projection.projectedX==receipt.state.projected.x && projection.projectedY==receipt.state.projected.y
        && projection.projectedRadius==receipt.state.projectedRadius && projection.projected==receipt.projected);
}
void handoff() {
    const IndexedBitmap panel{120,56,std::vector<std::uint8_t>(6720,7)};
    RadarState owner({panel,{4,1,{0,0,0,0}},{120,18,std::vector<std::uint8_t>(2160,3)}});
    owner.initializeLevel1(600,{},0,{0xa400,0xa800,0xa000,0xa800,0xa400});
    std::vector<IndexedBitmap> frames(31,{4,4,std::vector<std::uint8_t>(16,7)});
    FailureEffectInput input{{0,6000,0},0,{0x88,{},0,13,17,0x0ac8},{24,294,24,145},{159,84},0xa000,1,0};
    const auto first=level1GuardianEffectOperations(input,frames);
    assert(first.state.age==14 && first.state.endAge==17 && !first.retired);
    rejects([&] { owner.retainLevel1CombatEffectProjection(32,first); });
    rejects([&] { owner.retainLevel1CombatEffectProjection(32,first,Level1EffectProjectionDomain::ordinaryCombat); });
    owner.retainLevel1CombatEffectProjection(32,first,Level1EffectProjectionDomain::guardianCombat);
    sameProjection(owner,first);
    for (const auto age : {28,30}) {
        input.state.age=age;input.state.endAge=32;
        const auto receipt=level1GuardianEffectOperations(input,frames);
        assert(receipt.state.age==age+1 && receipt.state.endAge==32 && !receipt.retired);
        rejects([&] { owner.retainLevel1CombatEffectProjection(32,receipt); });
        rejects([&] { owner.retainLevel1CombatEffectProjection(32,receipt,Level1EffectProjectionDomain::ordinaryCombat); });
        owner.retainLevel1CombatEffectProjection(32,receipt,Level1EffectProjectionDomain::guardianCombat);
        sameProjection(owner,receipt);
    }
    input.state.age=31;
    const auto terminal=level1GuardianEffectOperations(input,frames);
    assert(terminal.retired && terminal.removal && terminal.pixels.empty());
    assert(terminal.state.age==0 && terminal.state.endAge==0 && terminal.state.callback==0);
    owner.retainLevel1CombatEffectProjection(32,terminal,Level1EffectProjectionDomain::guardianCombat);
    sameProjection(owner,terminal);
    //Terminal clears erase the originating family; the existing ordinary terminal remains valid.
    owner.retainLevel1CombatEffectProjection(32,terminal);
    sameProjection(owner,terminal);
    input.state.age=14;input.state.endAge=17;
    const auto ordinary=level1CombatEffectOperations(input,frames);
    owner.retainLevel1CombatEffectProjection(32,ordinary);
    sameProjection(owner,ordinary);
    auto malformed=first;malformed.state.age=13;
    rejects([&] { owner.retainLevel1CombatEffectProjection(32,malformed,Level1EffectProjectionDomain::guardianCombat); });
    malformed=first;malformed.state.endAge=18;
    rejects([&] { owner.retainLevel1CombatEffectProjection(32,malformed,Level1EffectProjectionDomain::guardianCombat); });
    malformed=first;malformed.projected=false;
    rejects([&] { owner.retainLevel1CombatEffectProjection(32,malformed,Level1EffectProjectionDomain::guardianCombat); });
    rejects([&] { owner.retainLevel1CombatEffectProjection(31,first,Level1EffectProjectionDomain::guardianCombat); });
    rejects([&] { owner.retainLevel1CombatEffectProjection(40,first,Level1EffectProjectionDomain::guardianCombat); });
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    handoff();return 0;
}
