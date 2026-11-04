#include "stellar7/audio.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

namespace {

constexpr std::uint64_t pitInputClock = 1'193'182;

std::vector<std::uint8_t> oneNoteSequence() {
    std::vector<std::uint8_t> sequence(36, 0);
    sequence[0] = 0x84;
    sequence[5] = 1;
    sequence[6] = 0x24;
    sequence.insert(sequence.end(), {0xc1, 0, 0, 0x91, 60, 64, 1, 0x81, 60, 64, 0xfc});
    return sequence;
}

std::vector<std::uint8_t> instrumentResource() {
    constexpr std::size_t headerSize = 19;
    constexpr std::size_t bankHalfSize = 0x540;
    std::vector<std::uint8_t> resource(headerSize + bankHalfSize * 2 + 2, 0);
    resource[0] = 0x89;
    resource[1] = 0x11;
    resource[headerSize + bankHalfSize] = 0xab;
    resource[headerSize + bankHalfSize + 1] = 0xcd;
    auto* patch = resource.data() + headerSize;
    patch[1] = 1;
    patch[3] = 15;
    patch[4] = 7;
    patch[7] = 7;
    patch[14] = 1;
    patch[16] = 15;
    patch[17] = 7;
    patch[20] = 7;
    return resource;
}

void sameDriver(const stellar7::SoundBlasterDriverState& left,
    const stellar7::SoundBlasterDriverState& right) {
    assert(left.clockPhase == right.clockPhase && left.clockPeriod == right.clockPeriod);
    assert(left.notes == right.notes && left.velocities == right.velocities);
    assert(left.sequencePosition == right.sequencePosition && left.sequenceWait == right.sequenceWait);
    assert(left.runningStatus == right.runningStatus && left.sequenceActive == right.sequenceActive);
    assert(left.directTimers == right.directTimers && left.channelVoiceCursors == right.channelVoiceCursors);
}

void sameWrites(const std::vector<stellar7::OplRegisterWrite>& left,
    const std::vector<stellar7::OplRegisterWrite>& right) {
    assert(left.size() == right.size());
    for (std::size_t index = 0; index < left.size(); ++index) {
        assert(left[index].registerNumber == right[index].registerNumber);
        assert(left[index].value == right[index].value);
    }
}

}  // namespace

int main() {
    stellar7::SoundBlasterSynth masked({}, instrumentResource(), oneNoteSequence());
    stellar7::SoundBlasterSynth reference({}, instrumentResource(), oneNoteSequence());
    masked.startMusic();
    reference.startMusic();
    assert(masked.render(1) == reference.render(1));

    const auto incoming = masked.driverState();
    const auto untilIrq = static_cast<std::size_t>(
        (incoming.clockPeriod - incoming.clockPhase + pitInputClock - 1) / pitInputClock
    );
    constexpr std::size_t samplesBeforeIrq = 4;
    constexpr std::size_t maskedSamples = samplesBeforeIrq * 2;
    assert(untilIrq > samplesBeforeIrq);
    assert(masked.render(untilIrq - samplesBeforeIrq)
        == reference.render(untilIrq - samplesBeforeIrq));
    masked.clearRegisterWrites();
    reference.clearRegisterWrites();

    assert(masked.renderInterruptMasked(maskedSamples).size() == maskedSamples);
    reference.render(maskedSamples);
    const auto pending = masked.driverState();
    assert(pending.clockPhase >= pending.clockPeriod);
    assert(masked.registerWrites().empty());
    assert(!reference.registerWrites().empty());
    assert(masked.render(0).empty());
    sameDriver(pending, masked.driverState());
    assert(masked.registerWrites().empty());

    masked.resumeSampleInterrupts();
    sameDriver(masked.driverState(), reference.driverState());
    sameWrites(masked.registerWrites(), reference.registerWrites());
    const auto serviced = masked.driverState();
    const auto writes = masked.registerWrites();
    masked.resumeSampleInterrupts();
    sameDriver(masked.driverState(), serviced);
    sameWrites(masked.registerWrites(), writes);

    stellar7::SoundBlasterSynth noEdge({}, instrumentResource(), oneNoteSequence());
    noEdge.startMusic();
    noEdge.render(1);
    noEdge.clearRegisterWrites();
    noEdge.renderInterruptMasked(2);
    const auto beforeNoEdge = noEdge.driverState();
    noEdge.resumeSampleInterrupts();
    sameDriver(noEdge.driverState(), beforeNoEdge);
    assert(noEdge.registerWrites().empty());
    noEdge.resumeSampleInterrupts();
    sameDriver(noEdge.driverState(), beforeNoEdge);
}
