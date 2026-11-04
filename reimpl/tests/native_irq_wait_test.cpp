#include "stellar7/presentation_clock.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
using namespace stellar7;
constexpr std::uint64_t pitInput = 1'193'182;
constexpr std::uint64_t irqPeriod = 44'100ULL * 5'041;
constexpr std::uint64_t callbackPeriod = irqPeriod * 4;

std::size_t samplesToEdges(const SoundBlasterSynth& synth, const unsigned edges) {
    assert(edges != 0);
    const auto state = synth.driverState();
    assert(state.clockPeriod == callbackPeriod);
    const auto distance = edges * irqPeriod - state.clockPhase % irqPeriod;
    return static_cast<std::size_t>((distance + pitInput - 1) / pitInput);
}

template<class Function>
void rejects(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::exception&) {
        rejected = true;
    }
    assert(rejected);
}

std::vector<std::uint8_t> tinyVoice() {
    std::vector<std::uint8_t> bytes(84);
    bytes[0] = 0x84;
    bytes[2] = 2;
    bytes[36] = 0xfc;
    bytes[51] = 0xc0; //11200Hz; effective DSP period89us.
    bytes[52] = 0x2b;
    bytes[69] = 3;
    bytes[81] = 0;
    bytes[82] = 255;
    bytes[83] = 128;
    return bytes;
}

void sameState(const SoundBlasterDriverState& a, const SoundBlasterDriverState& b) {
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

void firstEdgeAndPendingCallback() {
    SoundBlasterSynth synth;
    PresentationClock clock(synth);
    clock.validateNativeIrqWait();
    NativeIrqWait wait{1};
    const auto count = samplesToEdges(synth, 1);
    assert(clock.nativeIrqWaitSamples(1) == count);
    assert(clock.renderIrqWait(wait, count - 1).size() == count - 1 && wait.remaining == 1);
    assert(clock.renderIrqWait(wait, 1).size() == 1 && wait.remaining == 0);
    const auto elapsed = clock.elapsedSamples();
    const auto state = synth.driverState();
    const auto writes = synth.registerWrites().size();
    assert(clock.renderIrqWait(wait, 1000).empty());
    assert(clock.elapsedSamples() == elapsed && synth.registerWrites().size() == writes);
    sameState(state, synth.driverState());

    NativeIrqWait fourth{3};
    const auto untilFourth = samplesToEdges(synth, 3);
    assert(clock.renderIrqWait(fourth, untilFourth).size() == untilFourth && fourth.remaining == 0);
    // Rendering stops on the edge; callback service remains pending until next sample.
    assert(synth.driverState().clockPhase >= synth.driverState().clockPeriod);
    NativeIrqWait next{1};
    const auto untilNext = samplesToEdges(synth, 1);
    assert(untilNext > 1);
    assert(clock.renderIrqWait(next, 1).size() == 1 && next.remaining == 1);
    assert(clock.renderIrqWait(next, untilNext - 2).size() == untilNext - 2 && next.remaining == 1);
    assert(clock.renderIrqWait(next, 1).size() == 1 && next.remaining == 0);
}

struct Result {
    std::vector<std::int16_t> samples;
    SoundBlasterDriverState driver;
    SampledVoiceObservation voice;
    std::vector<OplRegisterWrite> writes;
};

Result packetWait(const std::size_t packet) {
    SoundBlasterSynth synth;
    PresentationClock clock(synth);
    clock.renderSamples(137); //Retain an incoming nonzero phase rather than reseeding.
    synth.loadSampledVoice(tinyVoice());
    synth.startSampledVoice();
    const auto expected = samplesToEdges(synth, 24);
    NativeIrqWait wait{24};
    std::vector<std::int16_t> samples;
    while (wait.remaining != 0) {
        const auto output = clock.renderIrqWait(wait, packet);
        assert(!output.empty());
        samples.insert(samples.end(), output.begin(), output.end());
        assert(samples.size() <= expected);
    }
    assert(samples.size() == expected && clock.elapsedSamples() == 137 + expected);
    const auto voice = synth.sampledVoiceObservation();
    assert(voice.phase == SampledVoicePhase::irqPending && voice.transferred == 3);
    //Waiting never invents the missing DMA completion interrupt.
    synth.serviceSampledVoiceIrq();
    assert(synth.sampledVoiceObservation().phase == SampledVoicePhase::completionPending);
    NativeIrqWait service{4};
    clock.renderIrqWait(service, samplesToEdges(synth, 4));
    assert(service.remaining == 0 && synth.sampledVoiceObservation().phase == SampledVoicePhase::complete);
    return {std::move(samples), synth.driverState(), synth.sampledVoiceObservation(), synth.registerWrites()};
}

void packetInvariance() {
    const auto single = packetWait(1);
    const auto packet = packetWait(137);
    const auto whole = packetWait(100'000);
    for (const auto* result : {&packet, &whole}) {
        assert(result->samples == single.samples);
        sameState(result->driver, single.driver);
        assert(result->writes.size() == single.writes.size());
        assert(std::equal(result->writes.begin(), result->writes.end(), single.writes.begin(),
            [](const auto& x, const auto& y) {
                return x.registerNumber == y.registerNumber && x.value == y.value;
            }));
        assert(result->voice.phase == single.voice.phase && result->voice.transferred == single.voice.transferred);
        assert(result->voice.clockPhase == single.voice.clockPhase && result->voice.dacValue == single.voice.dacValue);
    }
}

void strictRejectsBeforeMutation() {
    std::vector<std::uint8_t> music(36);
    music[0] = 0x84;
    music.push_back(0xfc);
    SoundBlasterSynth synth({}, {}, music);
    synth.startMusic();
    PresentationClock clock(synth);
    NormalWorkTiming empty;
    empty.phase.callback = synth.callbackWorkState();
    synth.renderClocked(empty, 0);
    const auto state = synth.driverState();
    const auto elapsed = clock.elapsedSamples();
    const auto writes = synth.registerWrites().size();
    NativeIrqWait wait{24};
    rejects([&] { clock.validateNativeIrqWait(); });
    rejects([&] { clock.renderIrqWait(wait, 137); });
    assert(wait.remaining == 24 && clock.elapsedSamples() == elapsed);
    assert(synth.registerWrites().size() == writes);
    sameState(state, synth.driverState());
}
} //namespace

int main() {
    try {
        firstEdgeAndPendingCallback();
        packetInvariance();
        strictRejectsBeforeMutation();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "native raw IRQ edges, retained phase, chunking, PCM and strict rejection\n";
}
