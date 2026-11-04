#include "stellar7/presentation_clock.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using namespace stellar7;

std::vector<std::uint8_t> song(const std::vector<std::uint8_t>& commands) {
    std::vector<std::uint8_t> sequence(36);
    sequence[0] = 0x84;
    sequence[5] = 1;
    sequence[6] = 4;
    sequence.insert(sequence.end(), commands.begin(), commands.end());
    return sequence;
}

std::vector<std::uint8_t> presentationSong() {
    return song({0x91, 60, 64, 1, 0xfc});
}

void retainReservedVelocities(SoundBlasterSynth& synth, PresentationClock& clock) {
    auto history = song({});
    constexpr std::uint8_t voiceCount = 9;
    constexpr std::uint8_t firstVelocity = 41;
    history[5] = voiceCount;
    for (std::uint8_t voice = 0; voice < voiceCount; ++voice) {
        history.insert(history.end(), {0x91, static_cast<std::uint8_t>(60 + voice),
            static_cast<std::uint8_t>(firstVelocity + voice),
            static_cast<std::uint8_t>(voice == voiceCount - 1 ? 1 : 0)});
    }
    history.push_back(0xfc);
    synth.loadCampaignMusic(history);
    clock.renderSamples(SoundBlasterSynth::sampleRate / SoundBlasterSynth::sequenceRate);
    synth.stopMusic();
    for (std::size_t voice = 6; voice < voiceCount; ++voice) {
        assert(synth.driverState().velocities[voice] == firstVelocity + voice);
    }
}

std::vector<std::uint8_t> sampledVoice() {
    constexpr std::size_t headerSize = 81;
    constexpr std::size_t payloadSize = 1000;
    std::vector<std::uint8_t> bytes(headerSize);
    bytes.resize(headerSize + payloadSize, 201);
    bytes[0] = 0x84;
    bytes[2] = 2;
    bytes[36] = 0xfc;
    bytes[51] = 0xc0;
    bytes[52] = 0x2b;
    bytes[69] = static_cast<std::uint8_t>(payloadSize);
    bytes[70] = static_cast<std::uint8_t>(payloadSize >> 8);
    return bytes;
}

std::vector<std::uint8_t> instrumentResource() {
    constexpr std::size_t headerSize = 19;
    constexpr std::size_t bankHalfSize = 0x540;
    std::vector<std::uint8_t> bytes(headerSize + bankHalfSize * 2 + 2);
    bytes[0] = 0x89;
    bytes[1] = 0x11;
    bytes[headerSize + bankHalfSize] = 0xab;
    bytes[headerSize + bankHalfSize + 1] = 0xcd;
    return bytes;
}

SoundBlasterSynth gameplaySynth(const std::vector<std::uint8_t>& presentation) {
    const auto terminated = song({0xfc});
    SoundBlasterSynth synth({}, instrumentResource(), terminated, presentation, {}, {}, terminated);
    synth.beginLogo();
    for (unsigned irq = 0; irq < 240; ++irq) {
        synth.advanceStartupIrq();
    }
    synth.triggerLogoEvent();
    synth.finishLogo(LogoCompletion::countdown);
    for (unsigned irq = 0; irq < 480; ++irq) {
        synth.advanceStartupIrq();
    }
    synth.stopLogo();
    synth.beginPresentation();
    synth.beginGameplay();
    assert(synth.startupPhase() == SoundBlasterStartupPhase::gameplay);
    synth.stopMusic();
    return synth;
}

void sameWrites(const std::vector<OplRegisterWrite>& left,
    const std::vector<OplRegisterWrite>& right) {
    assert(left.size() == right.size());
    assert(std::equal(left.begin(), left.end(), right.begin(),
        [](const auto& a, const auto& b) {
            return a.registerNumber == b.registerNumber && a.value == b.value;
        }));
}

void sameDriver(const SoundBlasterDriverState& a, const SoundBlasterDriverState& b) {
    assert(a.notes == b.notes && a.velocities == b.velocities);
    assert(a.channelVoiceCursors == b.channelVoiceCursors && a.directTimers == b.directTimers);
    assert(a.directLevelLatch == b.directLevelLatch && a.clockSource == b.clockSource);
    assert(a.normalClockCycles == b.normalClockCycles && a.clockPhase == b.clockPhase);
    assert(a.clockPeriod == b.clockPeriod && a.sequencePosition == b.sequencePosition);
    assert(a.sequenceWait == b.sequenceWait && a.runningStatus == b.runningStatus);
    assert(a.fade == b.fade && a.fadeDivider == b.fadeDivider && a.sequenceActive == b.sequenceActive);
    assert(a.carrierKsl == b.carrierKsl && a.carrierFallback == b.carrierFallback);
    assert(a.operatorKsl == b.operatorKsl && a.operatorFallback == b.operatorFallback);
    assert(a.operatorEnabled == b.operatorEnabled);
}

void prepareReleasedVoice(SoundBlasterSynth& synth, PresentationClock& clock) {
    retainReservedVelocities(synth, clock);
    clock.renderSamples(137);
    const auto retained = synth.driverState().velocities;
    synth.loadSampledVoice(sampledVoice());
    synth.startSampledVoice();
    assert(synth.driverState().velocities == retained);
    clock.renderSamples(1000);
    const auto played = synth.sampledVoiceObservation();
    assert(played.phase == SampledVoicePhase::playing && played.speakerEnabled
        && played.dacValue == 201);
    synth.stopSampledVoice();
    synth.releaseSampledVoice();
    const auto held = synth.sampledVoiceObservation();
    assert(held.phase == SampledVoicePhase::unloaded && held.speakerEnabled
        && held.dacValue == played.dacValue);
    synth.triggerLevel1Event(14);
    assert(synth.driverState().directTimers[0] != 0);
}

void sourceReturnAndRepeat() {
    const auto presentation = presentationSong();
    auto synth = gameplaySynth(presentation);
    auto control = gameplaySynth(presentation);
    PresentationClock clock(synth);
    PresentationClock controlClock(control);
    prepareReleasedVoice(synth, clock);
    prepareReleasedVoice(control, controlClock);
    retainReservedVelocities(synth, clock);
    retainReservedVelocities(control, controlClock);
    sameDriver(synth.driverState(), control.driverState());
    sameWrites(synth.registerWrites(), control.registerWrites());

    const auto before = synth.driverState();
    const auto held = synth.sampledVoiceObservation();
    const auto elapsed = clock.elapsedSamples();
    const auto oldWrites = synth.registerWrites();
    const auto countdown = synth.startupCountdown();
    synth.validateMenuAudioReturn();
    synth.restartMenuAudio();
    control.resetDirectEffects();
    control.loadCampaignMusic(presentation);

    assert(synth.startupPhase() == SoundBlasterStartupPhase::presentation);
    assert(synth.startupCountdown() == countdown && countdown == 2000);
    assert(clock.elapsedSamples() == elapsed && controlClock.elapsedSamples() == elapsed);
    const auto after = synth.driverState();
    assert(after.sequenceActive && after.sequencePosition == 36 && after.sequenceWait == 0);
    assert(after.clockSource == SoundBlasterClockSource::samples);
    assert(after.clockPhase == before.clockPhase && after.clockPeriod == before.clockPeriod);
    assert(after.directTimers == before.directTimers);
    //206B and1821 preserve CS:9B..9D despite resetting other direct state.
    for (std::size_t voice = 6; voice < after.velocities.size(); ++voice) {
        assert(after.velocities[voice] == before.velocities[voice]);
    }
    sameDriver(after, control.driverState());
    sameWrites(synth.registerWrites(), oldWrites);
    sameWrites(synth.registerWrites(), control.registerWrites());
    const auto heldAfter = synth.sampledVoiceObservation();
    assert(heldAfter.phase == SampledVoicePhase::unloaded
        && heldAfter.speakerEnabled == held.speakerEnabled
        && heldAfter.dacValue == held.dacValue
        && heldAfter.clockPhase == held.clockPhase);

    const auto output = clock.renderSamples(6000);
    assert(output == controlClock.renderSamples(6000));
    sameDriver(synth.driverState(), control.driverState());
    sameWrites(synth.registerWrites(), control.registerWrites());
    assert(std::count_if(synth.registerWrites().begin() + oldWrites.size(),
        synth.registerWrites().end(),
        [](const auto& write) { return write.registerNumber == 0xb0 && (write.value & 0x20) != 0; }) >= 2);

    synth.beginMenu();
    assert(synth.startupCountdown() == 30000);
    assert(synth.startupPhase() == SoundBlasterStartupPhase::presentation);
}

void rejectsUnchanged(SoundBlasterSynth& synth) {
    const auto before = synth.driverState();
    const auto voice = synth.sampledVoiceObservation();
    const auto writes = synth.registerWrites();
    const auto phase = synth.startupPhase();
    const auto countdown = synth.startupCountdown();
    bool validationRejected = false;
    bool operationRejected = false;
    try {
        synth.validateMenuAudioReturn();
    } catch (const std::exception&) {
        validationRejected = true;
    }
    try {
        synth.restartMenuAudio();
    } catch (const std::exception&) {
        operationRejected = true;
    }
    assert(validationRejected && operationRejected);
    sameDriver(before, synth.driverState());
    sameWrites(writes, synth.registerWrites());
    assert(synth.startupPhase() == phase && synth.startupCountdown() == countdown);
    const auto after = synth.sampledVoiceObservation();
    assert(after.phase == voice.phase && after.transferred == voice.transferred
        && after.clockPhase == voice.clockPhase && after.dacValue == voice.dacValue
        && after.speakerEnabled == voice.speakerEnabled);
}

void rejectionDomains() {
    const auto terminated = song({0xfc});
    SoundBlasterSynth missing({}, {}, terminated, {}, {}, {}, terminated);
    rejectsUnchanged(missing);

    SoundBlasterSynth incomplete({}, {}, terminated,
        std::vector<std::uint8_t>(35), {}, {}, terminated);
    rejectsUnchanged(incomplete);

    auto wrongType = presentationSong();
    wrongType[2] = 2;
    SoundBlasterSynth sampledHeader({}, {}, terminated, wrongType, {}, {}, terminated);
    rejectsUnchanged(sampledHeader);

    auto resident = gameplaySynth(presentationSong());
    resident.loadSampledVoice(sampledVoice());
    rejectsUnchanged(resident);

    auto allocated = gameplaySynth(presentationSong());
    allocated.startMusic();
    rejectsUnchanged(allocated);

    SoundBlasterSynth wrongPhase({}, {}, terminated, presentationSong(), {}, {}, terminated);
    rejectsUnchanged(wrongPhase);

    auto strict = gameplaySynth(presentationSong());
    strict.startMusic();
    NormalWorkTiming empty;
    empty.phase.callback = strict.callbackWorkState();
    strict.renderClocked(empty, 0);
    strict.stopMusic();
    rejectsUnchanged(strict);
}
} // namespace

int main() {
    try {
        sourceReturnAndRepeat();
        rejectionDomains();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "04BF retained repeating SNG1 and return preflight\n";
}
