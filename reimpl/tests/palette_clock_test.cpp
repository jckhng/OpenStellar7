#include "stellar7/palette_clock.hpp"
#include "stellar7/level2_music_work.hpp"
#include "stellar7/timer_install_work.hpp"
#include "stellar7/timer_registration_work.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <iostream>
#include <tuple>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace {
stellar7::SoundBlasterClockPhase phase() {
    using namespace stellar7;
    SoundBlasterClockPhase p{};
    p.remainingCycles = 5000;
    p.interrupts = AudioClockInterrupts::enabled;
    p.isa8DelayNanoseconds = 1020;
    p.dmaDacRate = 44100;
    p.pitPeriodMilliseconds = 55;
    p.irqDelayMilliseconds = 0.002;
    p.singleLinePeriodMilliseconds = 0.03;
    p.horizontalPeriodMilliseconds = 0.03;
    p.linesTotal = 400;
    p.minimumLatchDelayMilliseconds = 0.001;
    p.frameStart = AudioClockFrameStart::activeUnlockedVgaSurface;
    p.interruptDelivery = AudioClockInterruptDelivery::residentTimerIrq0;
    p.vgaInterrupt = AudioClockVgaInterrupt::fixedVgaNoDemand;
    p.devices = AudioClockDevicePhase{};
    p.devices->fixedPitAndMaskedDma = true;
    p.devices->framePeriodMilliseconds = 14;
    p.devices->displayEndMilliseconds = 12;
    p.devices->retraceStartMilliseconds = 0.001;
    p.devices->retraceEndMilliseconds = 10;
    p.devices->frame = AudioClockVgaFramePhase{1000.0 / 14, 0, 0};
    p.timer = AudioClockTimerPhase{1, 0, 5, 20, 20, 5};
    return p;
}

bool rejects(const stellar7::SoundBlasterClockPhase& p, const stellar7::PaletteWorkState& state,
    const stellar7::PaletteRequest& request, const std::int64_t horizon = 120000) {
    try {
        stellar7::paletteClockTiming(p, state, request, horizon);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

stellar7::SoundBlasterClockPhase retainedProgramPhase() {
    using namespace stellar7;
    auto p=normalWorkTiming(nativeTimerInstallPhase(),timerInstallOperations(),1000).phase;
    SoundBlasterCallbackState stopped{};
    stopped.domain=SoundBlasterCallbackDomain::noHandle;stopped.operation=0x1e;stopped.active=false;
    p.callback=stopped;
    p.soundBlasterInitialization=AudioClockSoundBlasterInitializationPhase{158,true};
    p=normalWorkTiming(p,timerRegistrationOperations(),1000).phase;
    SoundBlasterCallbackState selected{};
    selected.program.emplace();
    selected.program->instrumentBank.resize(96*28);
    selected.program->volumeTable.resize(16*64);
    selected.sequence={0xfc};selected.wait=32;
    p.callback=selected;p.sequenceSelection=AudioClockSequenceSelectionPhase{selected};
    p.sequenceSelection->published=true;p.sequenceSelection->songOrdinal=2;
    p.sequenceSelection->level1DirectResetCompleted=true;
    p.timer->callbackCount=1; //Synthetic first registered IRQ uses the qualified positive-wait body.
    return p;
}

void retainedProgramPalette() {
    using namespace stellar7;
    constexpr std::int64_t ordinaryHorizon=120000;
    constexpr std::int64_t programHorizon=2*ordinaryHorizon;
    PaletteWorkState source{};source.source.fill(63);
    const auto incoming=retainedProgramPhase();
    const PaletteRequest damageFade{0,256,238,59};
    //Admission regression, not a fitted IRQ schedule or retained284 fixture.
    const auto admitted=paletteClockTiming(incoming,source,damageFade,programHorizon);
    assert(admitted.state.dac.colors==admitted.state.output);
    const PaletteRequest shortRequest{17,1,0,3};
    const auto ordinary=paletteClockTiming(incoming,source,shortRequest,ordinaryHorizon);
    const auto extended=paletteClockTiming(incoming,source,shortRequest,programHorizon);
    const auto totals=[](const SoundBlasterClockStep& v) {
        return std::make_tuple(v.dispatches,v.delayCycles,v.terminalCharges,v.elapsedCycles,
            v.remainingCycles,v.deferredCycles,v.linesDone,v.millisecondRollovers);
    };
    assert(totals(ordinary.timing.total)==totals(extended.timing.total));
    assert(ordinary.highReads==extended.highReads && ordinary.waitEntryCycles==extended.waitEntryCycles);
    assert(ordinary.uploadStartCycles==extended.uploadStartCycles && ordinary.uploadEndCycles==extended.uploadEndCycles);
    assert(ordinary.timing.stringIterations==extended.timing.stringIterations);
    assert(ordinary.timing.stringRestarts==extended.timing.stringRestarts);
    assert(ordinary.writes.size()==extended.writes.size() && ordinary.commits.size()==extended.commits.size());
    for(std::size_t i=0;i<ordinary.writes.size();++i) {
        const auto& a=ordinary.writes[i];const auto& b=extended.writes[i];
        assert(std::tie(a.cycleOffset,a.port,a.value)==std::tie(b.cycleOffset,b.port,b.value));
    }
    for(std::size_t i=0;i<ordinary.commits.size();++i) {
        const auto& a=ordinary.commits[i];const auto& b=extended.commits[i];
        assert(std::tie(a.cycleOffset,a.index,a.color)==std::tie(b.cycleOffset,b.index,b.color));
    }
    assert(!ordinary.timing.callbacks.empty() && ordinary.timing.callbacks.size()==extended.timing.callbacks.size());
    for(std::size_t i=0;i<ordinary.timing.callbacks.size();++i) {
        const auto& a=ordinary.timing.callbacks[i];const auto& b=extended.timing.callbacks[i];
        assert(a.cycleOffset==b.cycleOffset && a.advance.kind==b.advance.kind && a.advance.dispatches==b.advance.dispatches);
        assert(totals(a.timing.total)==totals(b.timing.total));
        assert(a.advance.state.wait==b.advance.state.wait && a.advance.state.position==b.advance.state.position);
        assert(a.advance.state.voices.directTimers==b.advance.state.voices.directTimers);
    }
    assert(ordinary.state.source==extended.state.source && ordinary.state.output==extended.state.output);
    assert(ordinary.state.dac.colors==extended.state.dac.colors && ordinary.state.dac.mode==extended.state.dac.mode);
    assert(ordinary.state.dac.writeIndex==extended.state.dac.writeIndex && ordinary.state.dac.readIndex==extended.state.dac.readIndex);
    assert(ordinary.state.dac.pelIndex==extended.state.dac.pelIndex && ordinary.state.dac.latch==extended.state.dac.latch);
    const auto& a=ordinary.timing.phase;const auto& b=extended.timing.phase;
    assert(a.remainingCycles==b.remainingCycles && a.deferredCycles==b.deferredCycles && a.interrupts==b.interrupts);
    assert(a.irqPending==b.irqPending && a.irqCheckRequested==b.irqCheckRequested);
    assert(a.devices->picTicks==b.devices->picTicks && a.devices->pitNowMilliseconds==b.devices->pitNowMilliseconds);
    assert(a.devices->frameStartMilliseconds==b.devices->frameStartMilliseconds);
    assert(a.devices->frame->framesSinceTimeBase==b.devices->frame->framesSinceTimeBase);
    assert(a.timer->biosCount==b.timer->biosCount && a.timer->callbackCount==b.timer->callbackCount);
    assert(a.callback->wait==b.callback->wait && a.callback->program->operators==b.callback->program->operators);
    assert(a.events.size()==b.events.size());
    for(std::size_t i=0;i<a.events.size();++i) {
        assert(std::tie(a.events[i].kind,a.events[i].deadlineMilliseconds,a.events[i].value)
            ==std::tie(b.events[i].kind,b.events[i].deadlineMilliseconds,b.events[i].value));
    }
    for(unsigned mutation=0;mutation<6;++mutation) {
        auto invalid=incoming;
        if(mutation==0) {invalid.sequenceSelection.reset();}
        if(mutation==1) {invalid.sequenceSelection->published=false;}
        if(mutation==2) {invalid.sequenceSelection->songOrdinal=1;}
        if(mutation==3) {invalid.sequenceSelection->level1DirectResetCompleted=false;}
        if(mutation==4) {invalid.callback->domain=SoundBlasterCallbackDomain::noHandle;}
        if(mutation==5) {invalid.callback->program.reset();}
        assert(rejects(invalid,source,damageFade,programHorizon));
    }
    auto noCallback=incoming;noCallback.callback.reset();
    assert(rejects(noCallback,source,damageFade,programHorizon));
    assert(rejects(incoming,source,damageFade,programHorizon+1));
    bool normalRejected=false;
    try {normalWorkTiming(incoming,{{NormalWorkKind::instructions,1}},ordinaryHorizon+1);}
    catch(const std::invalid_argument&) {normalRejected=true;}
    assert(normalRejected);
}

void retainedLevel2RetryPalette() {
    using namespace stellar7;
    constexpr std::int64_t ordinaryHorizon=120000;
    constexpr std::int64_t programHorizon=2*ordinaryHorizon;
    auto incoming=retainedProgramPhase();
    incoming.sequenceSelection->songOrdinal=level2MusicIdentity;
    incoming.sequenceSelection->level1DirectResetCompleted=false;
    incoming.successAudio.emplace();incoming.successAudio->stage=SuccessAudioStage::returned;
    incoming.level2RetryMusic=AudioClockLevel2RetryMusicPhase{AudioClockLevel2RetryMusicStage::playing};
    PaletteWorkState source{};source.source.fill(63);
    const PaletteRequest full{0,256,0,0};
    const auto admitted=paletteClockTiming(incoming,source,full,programHorizon);
    assert(admitted.state.dac.colors==admitted.state.output);
    const PaletteRequest shortRequest{17,1,0,3};
    const auto first=paletteClockTiming(incoming,source,shortRequest,ordinaryHorizon);
    const auto second=paletteClockTiming(incoming,source,shortRequest,programHorizon);
    assert(first.timing.total.elapsedCycles==second.timing.total.elapsedCycles
        && first.timing.total.dispatches==second.timing.total.dispatches);
    assert(first.writes.size()==second.writes.size() && first.commits.size()==second.commits.size());
    for(std::size_t i=0;i<first.writes.size();++i) {
        const auto& a=first.writes[i];const auto& b=second.writes[i];
        assert(std::tie(a.cycleOffset,a.port,a.value)==std::tie(b.cycleOffset,b.port,b.value));
    }
    assert(first.state.dac.colors==second.state.dac.colors && first.state.dac.latch==second.state.dac.latch);
    assert(first.timing.phase.remainingCycles==second.timing.phase.remainingCycles
        && first.timing.phase.deferredCycles==second.timing.phase.deferredCycles
        && first.timing.phase.callback->wait==second.timing.phase.callback->wait);
    for(unsigned mutation=0;mutation<9;++mutation) {
        auto bad=incoming;
        if(mutation==0) {bad.level2RetryMusic.reset();}
        if(mutation==1) {bad.level2RetryMusic->stage=AudioClockLevel2RetryMusicStage::entered;}
        if(mutation==2) {bad.level2RetryMusic->stage=AudioClockLevel2RetryMusicStage::resourceBound;}
        if(mutation==3) {bad.sequenceSelection->songOrdinal=2;}
        if(mutation==4) {bad.sequenceSelection->published=false;}
        if(mutation==5) {bad.sequenceSelection->prepared.sequence.push_back(0);}
        if(mutation==6) {bad.successAudio.reset();}
        if(mutation==7) {bad.successAudio->stage=SuccessAudioStage::playing;}
        if(mutation==8) {bad.callback->program.reset();}
        assert(rejects(bad,source,full,programHorizon));
    }
    assert(rejects(incoming,source,full,programHorizon+1));
}

}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    try {retainedProgramPalette();retainedLevel2RetryPalette();}
    catch(const std::exception& error) {
        std::cerr << "retained program palette admission: " << error.what() << '\n';
        return 1;
    }
    using namespace stellar7;
    PaletteWorkState state{};
    state.source.fill(63);
    state.source[0] = 1;
    state.source[1] = 2;
    state.source[2] = 3;
    state.source[17 * 3] = 22;
    state.source[17 * 3 + 1] = 43;
    state.output.fill(9);
    state.dac.colors.fill(27);
    state.dac.mode = PaletteDacMode::read;
    state.dac.writeIndex = 5;
    state.dac.readIndex = 7;
    state.dac.pelIndex = 2;
    const PaletteRequest partial{17, 1, 0, 3};
    const auto readMode = paletteClockTiming(phase(), state, partial, 120000);
    assert(readMode.highReads == 1);
    assert(readMode.timing.total.dispatches == 145);
    assert(readMode.timing.stringIterations == 9);
    assert(readMode.timing.total.delayCycles == 22);
    assert(readMode.timing.total.elapsedCycles == 176);
    assert(readMode.writes.size() == 4 && readMode.writes[0].port == 0x3c8);
    assert(readMode.state.output[17 * 3] == 1);
    assert(readMode.state.output[17 * 3 + 1] == 3);
    assert(readMode.state.output[17 * 3 + 2] == 5);
    assert(readMode.state.output[17 * 3 - 1] == 9 && readMode.state.output[18 * 3] == 9);
    assert(readMode.state.dac.colors[5 * 3 + 2] == 27);
    assert(readMode.state.dac.colors[17 * 3] == 1 && readMode.state.dac.writeIndex == 18);
    assert(readMode.state.dac.pelIndex == 0 && readMode.state.dac.mode == PaletteDacMode::write);
    assert(readMode.state.dac.readIndex == 17);
    assert(readMode.commits.size() == 1 && readMode.commits[0].cycleOffset == readMode.writes[3].cycleOffset);
    assert((*readMode.state.dac.latch == std::array<std::uint8_t, 3>{1, 3, 5}));
    assert(readMode.state.source == state.source && state.dac.colors[17 * 3] == 27);

    auto writeState = state;
    writeState.dac.mode = PaletteDacMode::write;
    writeState.dac.latch = std::array<std::uint8_t, 3>{11, 12, 13};
    const auto writeMode = paletteClockTiming(phase(), writeState, partial, 120000);
    assert(writeMode.writes.size() == 5 && writeMode.writes[0].port == 0x3c9);
    assert(writeMode.writes[0].value == 0 && writeMode.state.dac.colors[5 * 3 + 2] == 0);
    assert(writeMode.state.dac.colors[5 * 3] == 11 && writeMode.state.dac.colors[5 * 3 + 1] == 12);
    assert(writeMode.commits.size() == 2 && writeMode.commits[0].index == 5);
    assert(writeMode.commits[0].cycleOffset == writeMode.writes[0].cycleOffset);
    assert(writeMode.timing.total.elapsedCycles == readMode.timing.total.elapsedCycles + 5);
    for (const auto component : {0, 1}) {
        writeState.dac.pelIndex = static_cast<std::uint8_t>(component);
        writeState.dac.latch.reset();
        const auto incomplete = paletteClockTiming(phase(), writeState, partial, 120000);
        assert(incomplete.commits.size() == 1 && incomplete.state.dac.colors[5 * 3 + 2] == 27);
        assert(incomplete.commits[0].cycleOffset == incomplete.writes[4].cycleOffset);
    }
    writeState.dac.pelIndex = 2;
    assert(rejects(phase(), writeState, partial));

    const PaletteRequest full{0, 256, 0, 3};
    assert(rejects(phase(), state, full, 120001));
    auto fading = phase();
    fading.callback.emplace();
    fading.callback->program.emplace();
    fading.callback->fade = 1;
    assert(rejects(fading, state, full, 240001));
    fading.callback->program.reset();
    assert(rejects(fading, state, full, 120001));
    const auto complete = paletteClockTiming(phase(), state, full, 120000);
    // Independently counted077a->069e:17450 instructions and1536 string transfers.
    assert(complete.waitEntryCycles == 18986 + 3); // Three spent5000-cycle slices.
    assert(complete.timing.total.dispatches == 19780);
    assert(complete.timing.stringIterations == 2304);
    assert(complete.timing.total.delayCycles == 2317);
    assert(complete.timing.total.terminalCharges == 4);
    assert(complete.timing.total.elapsedCycles == 24401 + 4);
    assert(complete.uploadEndCycles - complete.uploadStartCycles == 1 + 7 * 768 + 1);
    assert(complete.state.dac.colors == complete.state.output);
    assert(complete.state.dac.writeIndex == 0 && complete.state.dac.pelIndex == 0);
    assert(complete.state.dac.readIndex == 255 && complete.commits.size() == 256);

    // Completed full-upload effects must agree with the instruction-clock leaf.
    const auto projected = completedFullPaletteUpload(state, full);
    assert(projected.source == complete.state.source && projected.output == complete.state.output);
    assert(projected.dac.colors == complete.state.dac.colors);
    assert(projected.dac.mode == complete.state.dac.mode);
    assert(projected.dac.readIndex == complete.state.dac.readIndex);
    assert(projected.dac.writeIndex == complete.state.dac.writeIndex);
    assert(projected.dac.pelIndex == complete.state.dac.pelIndex);
    assert(projected.dac.latch == complete.state.dac.latch);
    bool partialRejected = false;
    try {
        completedFullPaletteUpload(state, partial);
    } catch (const std::invalid_argument&) {
        partialRejected = true;
    }
    assert(partialRejected);

    // Incoming high is consumed directly; a low-then-high page-flip wait is wrong.
    auto late = phase();
    late.devices->retraceStartMilliseconds = 0.100;
    const auto waited = paletteClockTiming(late, state, partial, 120000);
    assert(waited.highReads > 1 && waited.timing.total.elapsedCycles > readMode.timing.total.elapsedCycles);
    const auto prefix = normalWorkTiming(phase(), {{NormalWorkKind::instructions, 10}}, 120000);
    const auto continued = paletteClockTiming(prefix.phase, state, partial, 120000);
    auto direct = phase();
    direct.remainingCycles -= 10;
    const auto equivalent = paletteClockTiming(direct, state, partial, 120000);
    assert(continued.timing.total.elapsedCycles == equivalent.timing.total.elapsedCycles);
    assert(continued.timing.phase.remainingCycles == equivalent.timing.phase.remainingCycles);
    const auto second = paletteClockTiming(complete.timing.phase, complete.state, {0, 256, 0, 59}, 120000);
    assert(second.state.source == state.source);
    assert(second.state.dac.mode == PaletteDacMode::write);
    assert(second.writes.front().port == 0x3c9 && second.writes.front().value == 0);

    // A pending IRQ during the masked upload is delivered after STI's POP shadow.
    auto maskedWindow = phase();
    maskedWindow.events = {{4.0, AudioClockEventKind::irqCheck, 0}};
    maskedWindow.irqPending = true;
    maskedWindow.devices->irq0Requested = true;
    maskedWindow.irq0ActivationPossible = true;
    maskedWindow.timer->biosCount = 1;
    maskedWindow.timer->callbackCount = 1;
    maskedWindow.callback = SoundBlasterCallbackState{};
    maskedWindow.callback->wait = 2;
    maskedWindow.bios = AudioClockBiosPhase{};
    maskedWindow.bios->domain = AudioClockBiosDomain::normalVgaDefaultTimer;
    const auto interrupted = paletteClockTiming(maskedWindow, state, full, 120000);
    assert(interrupted.timing.callbacks.size() == 1);
    assert(interrupted.timing.callbacks[0].cycleOffset >= interrupted.uploadEndCycles + 2);
    assert(interrupted.timing.phase.callback->wait == 1);
    assert(interrupted.timing.phase.bios->timerTicks == 1);
    assert(interrupted.timing.phase.interrupts == AudioClockInterrupts::enabled);
    assert(interrupted.uploadEndCycles - interrupted.uploadStartCycles == complete.uploadEndCycles - complete.uploadStartCycles);

    auto invalid = state;
    invalid.source[0] = 64;
    assert(rejects(phase(), invalid, full));
    invalid = state;
    invalid.dac.mode = static_cast<PaletteDacMode>(1);
    assert(rejects(phase(), invalid, full));
    invalid = state;
    invalid.dac.pelIndex = 3;
    assert(rejects(phase(), invalid, full));
    invalid = state;
    invalid.dac.latch = std::array<std::uint8_t, 3>{64, 0, 0};
    assert(rejects(phase(), invalid, full));
    invalid = state;
    invalid.dac.profile = static_cast<PaletteDacProfile>(1);
    assert(rejects(phase(), invalid, full));
    assert(rejects(phase(), state, {255, 2, 0, 3}));
    assert(rejects(phase(), state, {0, 0, 0, 3}));
    assert(rejects(phase(), state, {0, 256, 256, 3}));
    assert(rejects(phase(), state, {0, 256, 0, 64}));
    invalid = state;
    const auto wrap = paletteClockTiming(phase(), invalid, {255, 1, 0, 3}, 120000);
    assert(wrap.state.dac.writeIndex == 0 && wrap.state.dac.pelIndex == 0 && wrap.state.dac.readIndex == 255);
    auto missing = phase();
    missing.devices.reset();
    assert(rejects(missing, state, full));
    assert(rejects(phase(), state, full, 240000));
}
