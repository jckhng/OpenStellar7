#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif
#define main unusedEnemyFixtureMain
#include "audio_fixture.inc"
#undef main
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    //The fixture uses real startup/selection/reset on a synthetic supplied bank.
    synthReplay();
    const auto initial=history();
    const auto phase=phaseFor(initial);
    const auto work=normalWorkTiming(phase,{enemyFire},horizon);
    assert(!work.soundBlasterEffects.empty());
    assert(directReceipt(work).value==9);
    assert(work.phase.callback->voices.directTimers[0]==30);
    assert(work.phase.callback->voices.notes[6]==39);
    auto without=phase;without.sequenceSelection.reset();
    rejects([&] {normalWorkTiming(without,{enemyFire},horizon);});
    auto destination4=phase;destination4.sequenceSelection->songOrdinal=4;
    rejects([&] {normalWorkTiming(destination4,{enemyFire},horizon);});
    auto noReset=phase;noReset.sequenceSelection->level1DirectResetCompleted=false;
    rejects([&] {normalWorkTiming(noReset,{enemyFire},horizon);});
    auto wrong=enemyFire;wrong.value=6;
    rejects([&] {normalWorkTiming(phase,{wrong},horizon);});
    rejects([&] {normalWorkTiming(phase,{{NormalWorkKind::gameplayEnemyFireEvent,1,9}},horizon);});
    rejects([&] {soundBlasterDirectAdvance(initial,gameplaySolidContactDirectRecord,SoundBlasterDirectCaller::gameplayEnemyFire);});
    for(const auto timers:{std::array<std::uint16_t,3>{0,0,0},{4,0,0},{4,2,0},{4,2,1}}) {
        auto state=initial;state.voices.directTimers=timers;
        const auto next=normalWorkTiming(phaseFor(state),{enemyFire},horizon);
        const auto prefix=normalWorkTiming(phaseFor(state),{{NormalWorkKind::instructions,47},{NormalWorkKind::cli,1}},horizon);
        const auto direct=soundBlasterDirectAdvance(*prefix.phase.callback,gameplaySolidContactDirectRecord,SoundBlasterDirectCaller::gameplaySolidContact);
        assert(callbackFields(*directReceipt(next).directState)==callbackFields(direct.state));
    }
    auto faded=initial;faded.fade=1;
    rejects([&] {soundBlasterDirectAdvance(faded,gameplaySolidContactDirectRecord,SoundBlasterDirectCaller::gameplaySolidContact);});
    return 0;
}
