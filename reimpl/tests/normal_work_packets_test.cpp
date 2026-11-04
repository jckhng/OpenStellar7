#include "../src/normal_work_packets.hpp"
#include "stellar7/timer_install_work.hpp"
#include "stellar7/timer_registration_work.hpp"
#include <cassert>
#include <tuple>

namespace {
using namespace stellar7;
SoundBlasterClockPhase installedPhase() {
    auto phase=normalWorkTiming(nativeTimerInstallPhase(),timerInstallOperations(),1000).phase;
    SoundBlasterCallbackState callback{};
    callback.domain=SoundBlasterCallbackDomain::noHandle;
    callback.operation=0x1e;
    callback.active=false;
    phase.callback=callback;
    phase.soundBlasterInitialization=AudioClockSoundBlasterInitializationPhase{158,true};
    return normalWorkTiming(phase,timerRegistrationOperations(),1000).phase;
}
void batchingPreservesSourceBoundaries() {
    std::vector<NormalWorkOperation> operations;
    for(unsigned row=0;row<700;++row) {
        operations.insert(operations.end(),{{NormalWorkKind::instructions,31},
            {NormalWorkKind::repeatString,80},{NormalWorkKind::instructions,3},
            {NormalWorkKind::port8Write,1},{NormalWorkKind::port16Write,1},{NormalWorkKind::string,3}});
    }
    const auto incoming=installedPhase();
    const auto reference=normalWorkTiming(incoming,operations,120000);
    assert(reference.total.terminalCharges!=0 && !reference.callbacks.empty());
    for(const std::size_t budget:{1,17,257,10000}) {
        auto phase=incoming;
        SoundBlasterClockStep total{};
        std::uint64_t strings=0,restarts=0;
        std::vector<std::tuple<std::int64_t,SoundBlasterCallbackKind>> callbacks;
        for(const auto& packet:normalWorkPackets(operations,budget)) {
            const auto work=normalWorkTiming(phase,packet,120000);
            for(const auto& callback:work.callbacks) {
                callbacks.emplace_back(total.elapsedCycles+callback.cycleOffset,callback.advance.kind);
            }
            total.dispatches+=work.total.dispatches;total.delayCycles+=work.total.delayCycles;
            total.terminalCharges+=work.total.terminalCharges;total.elapsedCycles+=work.total.elapsedCycles;
            strings+=work.stringIterations;restarts+=work.stringRestarts;phase=work.phase;
        }
        assert(total.dispatches==reference.total.dispatches && total.delayCycles==reference.total.delayCycles);
        assert(total.terminalCharges==reference.total.terminalCharges && total.elapsedCycles==reference.total.elapsedCycles);
        assert(strings==reference.stringIterations && restarts==reference.stringRestarts);
        assert(phase.remainingCycles==reference.phase.remainingCycles && phase.deferredCycles==reference.phase.deferredCycles);
        assert(phase.linesDone==reference.phase.linesDone && phase.devices->picTicks==reference.phase.devices->picTicks);
        assert(phase.timer->globalCount==reference.phase.timer->globalCount && phase.timer->callbackCount==reference.phase.timer->callbackCount);
        assert(phase.events.size()==reference.phase.events.size() && callbacks.size()==reference.callbacks.size());
        for(std::size_t index=0;index<phase.events.size();++index) {
            const auto& a=phase.events[index];const auto& b=reference.phase.events[index];
            assert(a.kind==b.kind && a.value==b.value && a.deadlineMilliseconds==b.deadlineMilliseconds);
        }
        for(std::size_t index=0;index<callbacks.size();++index) {
            assert(callbacks[index]==std::make_tuple(reference.callbacks[index].cycleOffset,reference.callbacks[index].advance.kind));
        }
    }
}
}
int main() {
    batchingPreservesSourceBoundaries();
    assert(stellar7::normalWorkPackets({},1).empty());
    bool rejected=false;
    try {stellar7::normalWorkPackets({},0);} catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
}
