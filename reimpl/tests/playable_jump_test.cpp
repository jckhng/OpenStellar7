#include "playable_jump.hpp"

#include <cassert>
#include <stdexcept>

using namespace stellar7;
namespace {
PlayableJumpState initial() {
    PlayableJumpState state{};
    state.statuses.fill(3);state.selectedSystem=-1;state.timers[3]=17;
    return state;
}
template<class Call> void rejects(Call call) {
    bool rejected=false;
    try {call();} catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
}
void chargeAndFlight() {
    auto state=initial();
    for(int charge=3;charge>0;--charge) {
        const auto activated=applyPlayableJumpAction(state,GameplaySystemAction::jump);
        state=activated.state;
        assert(state.selectedSystem==2 && state.statuses[2]==-(charge-1) && state.timers[2]==220);
        const auto& panel=std::get<SystemPanel>(activated.effects.back());
        assert(panel.system==2 && panel.style==SystemPanelStyle::active && panel.remainingCharges==charge-1);
        const auto repeated=applyPlayableJumpAction(state,GameplaySystemAction::jump);
        assert(repeated.state.timers==state.timers && repeated.state.statuses==state.statuses);
        for(int step=1;step<=45;++step) {
            const auto frame=advancePlayableJump(state);
            state=frame.state;
            const auto height=step==45 ? 0 : 220*step-5*step*(step-1);
            assert(state.playerZ==height && state.timers[2]==(step==45 ? 0 : 220-10*step));
            assert(state.timers[3]==17);
            if(step==22 || step==23) {assert(state.playerZ==2530);}
            if(step<45) {assert(frame.effects.empty());}
            if(step==45) {
                const auto& landed=std::get<SystemPanel>(frame.effects.front());
                assert(landed.remainingCharges==charge-1);
                assert(landed.style==(charge==1 ? SystemPanelStyle::unavailable : SystemPanelStyle::ready));
            }
        }
        assert(state.statuses[2]==charge-1 && state.timers[2]==0);
        if(charge==1) {assert(state.selectedSystem==3);}
    }
    const auto unavailable=applyPlayableJumpAction(state,GameplaySystemAction::jump);
    assert(unavailable.state.selectedSystem==-1 && unavailable.state.timers[2]==0 && unavailable.state.statuses[2]==0);
    assert(std::get<SystemAudio>(unavailable.effects.back()).event==8);
}
void selectedAndReset() {
    auto state=initial();state.selectedSystem=2;
    const auto activated=applyPlayableJumpAction(state,GameplaySystemAction::activateSelected);
    assert(activated.effects.size()==2 && std::get<SystemAudio>(activated.effects.front()).event==8);
    assert(activated.state.statuses[2]==-2 && activated.state.timers[2]==220);
    auto reset=activated.state;reset.timers[2]=0;reset.playerZ=0;
    const auto normalized=normalizePlayableSystemsAfterReset(reset);
    assert(normalized.state.statuses[2]==2 && normalized.state.selectedSystem==2 && normalized.state.timers[3]==17);
    assert(normalized.effects.size()==11);
    const auto& jumpPanel=std::get<SystemPanel>(normalized.effects[2]);
    assert(jumpPanel.system==2 && jumpPanel.remainingCharges==2 && jumpPanel.style==SystemPanelStyle::ready);
    auto other=initial();other.selectedSystem=1;
    rejects([&]{applyPlayableJumpAction(other,GameplaySystemAction::activateSelected);});
    assert(other.statuses==initial().statuses && other.timers==initial().timers);
}
void malformed() {
    const auto valid=initial();
    for(const auto badHeight:{-1,1,2531}) {auto bad=valid;bad.playerZ=badHeight;rejects([&]{advancePlayableJump(bad);});}
    for(const auto badIncrement:{-230,230,219}) {auto bad=valid;bad.timers[2]=static_cast<std::int16_t>(badIncrement);rejects([&]{advancePlayableJump(bad);});}
    auto bad=valid;bad.statuses[2]=-2;rejects([&]{advancePlayableJump(bad);});
    bad=valid;bad.timers[1]=1;rejects([&]{advancePlayableJump(bad);});
    bad=valid;bad.selectedSystem=7;rejects([&]{applyPlayableJumpAction(bad,GameplaySystemAction::jump);});
    auto apex=valid;apex.playerZ=2530;apex.timers[2]=0;apex.selectedSystem=2;
    rejects([&]{applyPlayableJumpAction(apex,GameplaySystemAction::jump);});
    assert(apex.playerZ==2530 && apex.statuses[2]==3);
}
} //namespace
int main() {chargeAndFlight();selectedAndReset();malformed();}
