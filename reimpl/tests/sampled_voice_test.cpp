#include "stellar7/audio.hpp"
#include "stellar7/normal_clock.hpp"
#include "stellar7/presentation_clock.hpp"
#include "stellar7/resource_codec.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {

using namespace stellar7;
constexpr std::uint64_t dspClock = 1'000'000;

void word(std::vector<std::uint8_t>& bytes, const std::size_t offset, const std::uint16_t value) {
    bytes.at(offset) = static_cast<std::uint8_t>(value);
    bytes.at(offset + 1) = static_cast<std::uint8_t>(value >> 8);
}

std::vector<std::uint8_t> voice(const std::vector<std::uint8_t>& samples) {
    std::vector<std::uint8_t> sequence(81);
    sequence[0] = 0x84;
    sequence[2] = 2;
    sequence[36] = 0xfc;
    word(sequence, 51, 11200);
    word(sequence, 69, static_cast<std::uint16_t>(samples.size()));
    sequence.insert(sequence.end(), samples.begin(), samples.end());
    return sequence;
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

bool sameWrites(const SoundBlasterSynth& first, const SoundBlasterSynth& second) {
    const auto& a = first.registerWrites();
    const auto& b = second.registerWrites();
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
        [](const auto& left, const auto& right) {
            return left.registerNumber == right.registerNumber && left.value == right.value;
        });
}

void sameVoice(const SampledVoiceObservation& a, const SampledVoiceObservation& b) {
    assert(a.phase == b.phase && a.transferred == b.transferred && a.sampleCount == b.sampleCount
        && a.clockPhase == b.clockPhase && a.clockPeriod == b.clockPeriod
        && a.dacValue == b.dacValue && a.speakerEnabled == b.speakerEnabled);
}

void testDecodeAndTypeAdmission() {
    const auto bytes = voice({0, 128, 255});
    const auto data = decodeSampledVoice(bytes);
    assert(data.nominalRate == 11200 && data.timeConstant == 167 && data.samples.size() == 3);
    //19A4/2A54 transfer the declared count; SSM4 segment1 also owns an unused tail.
    auto padded=bytes;padded.insert(padded.end(),{17,250,0});
    const auto prefix=decodeSampledVoice(padded);
    assert(prefix.nominalRate==data.nominalRate && prefix.timeConstant==data.timeConstant
        && prefix.samples==data.samples);
    for (const auto offset : {std::size_t(0), std::size_t(1), std::size_t(2), std::size_t(33), std::size_t(36)}) {
        auto invalid = bytes;
        invalid[offset] ^= 1;
        rejects([&] { decodeSampledVoice(invalid); });
    }
    auto invalid = bytes;
    word(invalid, 51, 1000);
    rejects([&] { decodeSampledVoice(invalid); });
    invalid = bytes;
    invalid.pop_back();
    rejects([&] { decodeSampledVoice(invalid); });
    auto signedRate = bytes;
    word(signedRate, 51, 0x8000);
    rejects([&] { decodeSampledVoice(signedRate); });
    auto doubleMarker = bytes;
    doubleMarker[37] = 0xfc;
    rejects([&] { decodeSampledVoice(doubleMarker); });
    SoundBlasterSynth synth;
    const auto before = synth.driverState();
    const auto writes = synth.registerWrites().size();
    rejects([&] { synth.loadCampaignMusic(bytes); });
    assert(synth.registerWrites().size() == writes && synth.driverState().clockPhase == before.clockPhase);
    assert(!synth.driverState().sequenceActive);
    rejects([&] { synth.loadSampledVoice(invalid); });
    assert(synth.sampledVoiceObservation().phase == SampledVoicePhase::unloaded);
}

void testTransferAndDescriptorBoundaries() {
    SampledVoiceStream stream;
    stream.load(voice({0, 128, 255}));
    assert(stream.observation().clockPeriod == 44100U * 89U);
    stream.arm();
    std::vector<std::int16_t> pending(10);
    stream.mix(pending);
    assert(stream.observation().transferred == 0 && stream.observation().clockPhase == 0);
    rejects([&] { stream.release(); });
    stream.service();
    std::vector<std::int16_t> first(4);
    stream.mix(first);
    assert(first == std::vector<std::int16_t>(4));
    assert(stream.observation().transferred == 1 && stream.observation().dacValue == 0);
    std::vector<std::int16_t> next(1);
    stream.mix(next);
    assert(next[0] == -32768);
    std::vector<std::int16_t> rest(20);
    stream.mix(rest);
    assert(stream.observation().phase == SampledVoicePhase::irqPending && stream.busy());
    assert(stream.observation().transferred == 3 && rest.back() == 32512);
    stream.service();
    assert(stream.observation().phase == SampledVoicePhase::irqPending);
    rejects([&] { stream.stop(); });
    rejects([&] { stream.release(); });
    stream.serviceTransferIrq();
    assert(!stream.busy() && stream.observation().phase == SampledVoicePhase::completionPending);
    stream.service();
    assert(stream.observation().phase == SampledVoicePhase::complete);
    const auto completed = stream.observation();
    rejects([&] { stream.release(); });
    sameVoice(completed, stream.observation());
    assert(stream.resident());
    stream.stop();
    stream.release();
    assert(stream.observation().phase == SampledVoicePhase::unloaded);
    assert(stream.observation().speakerEnabled && stream.observation().dacValue == 255);
    std::vector<std::int16_t> clipped{32767};
    stream.mix(clipped);
    assert(clipped[0] == 32767);
}

void testRetainedSynthAndMaskedOutput() {
    const auto sequence = voice(std::vector<std::uint8_t>(10000, 128));
    SoundBlasterSynth actual, control;
    actual.triggerLevel1Event(0);
    control.triggerLevel1Event(0);
    const auto before = actual.driverState();
    actual.validateSampledVoiceLoad(sequence);
    assert(actual.sampledVoiceObservation().phase == SampledVoicePhase::unloaded);
    actual.loadSampledVoice(sequence);
    rejects([&] { actual.validateSampledVoiceLoad(sequence); });
    assert(sameWrites(actual, control));
    assert(actual.driverState().clockPhase == before.clockPhase);
    actual.startSampledVoice();
    control.resetDirectEffects();
    assert(actual.driverState().directTimers == before.directTimers);
    assert(sameWrites(actual, control));
    assert(actual.render(100) == control.render(100));
    assert(sameWrites(actual, control));
    const auto writes = actual.registerWrites().size();
    actual.stopSampledVoice();
    assert(actual.registerWrites().size() == writes); // Busy0Eh skips physical OPL keyoffs.
    actual.releaseSampledVoice();

    std::vector<std::uint8_t> ramp(3000);
    for (std::size_t index = 0; index < ramp.size(); ++index) {
        ramp[index] = static_cast<std::uint8_t>(index);
    }
    SoundBlasterSynth whole, split;
    whole.loadSampledVoice(voice(ramp));
    split.loadSampledVoice(voice(ramp));
    whole.startSampledVoice();
    split.startSampledVoice();
    const auto expected = whole.render(1200);
    auto result = split.render(700);
    const auto masked = split.renderInterruptMasked(100);
    result.insert(result.end(), masked.begin(), masked.end());
    const auto tail = split.render(400);
    result.insert(result.end(), tail.begin(), tail.end());
    assert(result == expected && sameWrites(whole, split));
    sameVoice(whole.sampledVoiceObservation(), split.sampledVoiceObservation());
    assert(whole.driverState().clockPhase == split.driverState().clockPhase);
    rejects([&] { split.callbackWorkState(); });
    rejects([&] { split.renderClocked(NormalWorkTiming{}, 0); });

    SoundBlasterSynth pending;
    pending.loadSampledVoice(sequence);
    pending.startSampledVoice();
    const auto beforeStop = pending.registerWrites().size();
    pending.stopSampledVoice();
    assert(pending.registerWrites().size() == beforeStop + 18); // Not-busy2DA0 sweeps nine voices.
    pending.render(1);
    assert(pending.sampledVoiceObservation().phase == SampledVoicePhase::stopped);
    pending.releaseSampledVoice();
}

void testStrictClockRejectsNativeVoice() {
    std::vector<std::uint8_t> music(36);
    music[0] = 0x84;
    music.push_back(0xfc);
    SoundBlasterSynth synth({}, {}, music);
    synth.startMusic();
    NormalWorkTiming empty;
    empty.phase.callback = synth.callbackWorkState();
    synth.renderClocked(empty, 0);
    rejects([&] { synth.validateSampledVoiceLoad(voice({128})); });
    rejects([&] { synth.loadSampledVoice(voice({128})); });
    assert(synth.sampledVoiceObservation().phase == SampledVoicePhase::unloaded);
}

std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("missing supplied voice resource");
    }
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void suppliedVoice(const std::filesystem::path& path, const std::filesystem::path& output,
    const std::size_t packet) {
    if (std::filesystem::exists(output)) {
        throw std::invalid_argument("supplied voice output must be fresh");
    }
    const auto groups = decodeSoundSequenceGroup(read(path), 2);
    assert(groups.size() == 1);
    const auto data = decodeSampledVoice(groups[0]);
    assert(data.nominalRate == 11200 && data.timeConstant == 167 && data.samples.size() == 56301);
    std::filesystem::create_directories(output);
    std::ofstream raw(output / "native-dac.pcm", std::ios::binary);
    SoundBlasterSynth synth;
    PresentationClock clock(synth);
    const auto append = [&](const std::vector<std::int16_t>& samples) {
        for (const auto sample : samples) {
            const auto value = static_cast<std::uint16_t>(sample);
            raw.put(static_cast<char>(value));
            raw.put(static_cast<char>(value >> 8));
        }
    };
    append(clock.renderSamples(257));
    const auto loadedAt = clock.elapsedSamples();
    const auto incoming = synth.driverState();
    const auto loadWrites = synth.registerWrites().size();
    synth.loadSampledVoice(groups[0]);
    assert(clock.elapsedSamples() == loadedAt && synth.registerWrites().size() == loadWrites);
    synth.startSampledVoice();
    assert(synth.driverState().clockPhase == incoming.clockPhase);
    const auto callbackBound = synth.driverState().clockPeriod / 1193182 + 2;
    std::size_t polls = 0;
    while (synth.sampledVoiceObservation().phase == SampledVoicePhase::pending) {
        assert(polls++ < callbackBound);
        append(clock.renderSamples(1));
    }
    const auto startedAt = clock.elapsedSamples() - 1;
    const auto frames = (data.samples.size() * synth.sampledVoiceObservation().clockPeriod + dspClock - 1) / dspClock;
    auto remaining = frames - 1;
    while (remaining != 0) {
        const auto count = std::min<std::size_t>(remaining, packet);
        append(clock.renderSamples(count));
        remaining -= count;
    }
    const auto finishedAt = clock.elapsedSamples();
    assert(synth.sampledVoiceObservation().phase == SampledVoicePhase::irqPending);
    assert(synth.sampledVoiceObservation().transferred == 56301);
    rejects([&] { synth.releaseSampledVoice(); });
    // Explicit native output-boundary IRQ service, not an original IRQ ancestry claim.
    synth.serviceSampledVoiceIrq();
    assert(synth.sampledVoiceObservation().phase == SampledVoicePhase::completionPending);
    polls = 0;
    while (synth.sampledVoiceObservation().phase != SampledVoicePhase::complete) {
        assert(polls++ < callbackBound);
        append(clock.renderSamples(1));
    }
    const auto completeAt = clock.elapsedSamples();
    synth.stopSampledVoice();
    const auto stopped = synth.sampledVoiceObservation();
    synth.releaseSampledVoice();
    assert(synth.sampledVoiceObservation().dacValue == stopped.dacValue);
    std::ofstream opl(output / "opl.csv");
    for (const auto& write : synth.registerWrites()) {
        opl << unsigned(write.registerNumber) << ',' << unsigned(write.value) << '\n';
    }
    std::ofstream report(output / "playback.json");
    report << "{\"loaded_at\":" << loadedAt << ",\"started_at\":" << startedAt
        << ",\"transfer_finished_at\":" << finishedAt << ",\"descriptor_complete_at\":" << completeAt
        << ",\"sample_count\":" << data.samples.size() << ",\"time_constant\":167"
        << ",\"digital_period_microseconds\":89,\"last_dac\":" << unsigned(stopped.dacValue)
        << ",\"clock_phase\":" << synth.driverState().clockPhase
        << ",\"original_dma_irq_phase_qualified\":false,\"analog_filter_qualified\":false}\n";
    std::cout << "56301 source bytes; retained synth/clock; explicit completion IRQ; native digital projection only\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        testDecodeAndTypeAdmission();
        testTransferAndDescriptorBoundaries();
        testRetainedSynthAndMaskedOutput();
        testStrictClockRejectsNativeVoice();
        if (argc == 4) {
            const auto packet = std::stoul(argv[3]);
            if (packet == 0) {
                throw std::invalid_argument("positive transport packet size required");
            }
            suppliedVoice(argv[1], argv[2], packet);
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
