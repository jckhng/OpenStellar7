#include "stellar7/audio.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {

constexpr std::uint32_t driverDivisorNumerator = 1331000;
constexpr std::uint32_t pitClock = 1193182;
constexpr std::int16_t speakerAmplitude = 9993;

std::uint16_t frequencyTableValue(const int quarterTone) {
    if (quarterTone < 0) {
        return 0;
    }
    const double frequency =
        32.70319566257483 * std::pow(2.0, quarterTone / 48.0);
    std::uint16_t value = static_cast<std::uint16_t>(
        std::floor(frequency + 0.5)
    );
    // The shipped STD driver table is one unit below mathematical rounding
    // at these entries. Preserve its literal values.
    switch (quarterTone) {
    case 46:
    case 88:
    case 141:
    case 202:
    case 240:
    case 252:
    case 256:
    case 265:
    case 267:
    case 284:
    case 311:
    case 323:
    case 329:
    case 333:
    case 355:
    case 369:
        --value;
        break;
    default:
        break;
    }
    return value;
}

std::size_t sequenceDataStart(
    const std::vector<std::uint8_t>& sequence
) {
    if (sequence.size() < 36) {
        throw std::runtime_error("truncated SNG sequence");
    }
    return sequence[0] == 0x84 ? 2 : 0;
}

std::uint8_t selectedChannel(
    const std::vector<std::uint8_t>& sequence,
    const std::size_t base
) {
    const unsigned channelCount = sequence[base] == 2 ? 15 : 16;
    for (unsigned channel = 0; channel < channelCount; ++channel) {
        const std::size_t offset = base + 2 + channel * 2;
        if (offset >= sequence.size()) {
            break;
        }
        if ((sequence[offset] & 0x20) != 0) {
            return static_cast<std::uint8_t>(channel);
        }
    }
    return 0xff;
}

}  // namespace

std::uint16_t pcSpeakerDivisor(
    const std::uint8_t note,
    const std::uint16_t pitchBend
) {
    if (note < 24 || note > 119) {
        return 0;
    }
    const int bendDistance = std::abs(
        static_cast<int>(pitchBend) - 0x2000
    ) / 171;
    int tableIndex = (static_cast<int>(note) - 24) * 4;
    tableIndex += pitchBend > 0x2000 ? bendDistance : -bendDistance;
    const std::uint16_t frequency = frequencyTableValue(tableIndex);
    return frequency == 0
        ? 0
        : static_cast<std::uint16_t>(
            driverDivisorNumerator / frequency
        );
}

int level1EventSequenceIndex(const int eventId) noexcept {
    switch (eventId) {
    case 0:
        return 4;
    case 1:
        return 6;
    case 2:
        return 14;
    case 3:
        return 8;
    case 4:
    case 5:
        return 12;
    case 6:
    case 18:
    case 19:
    case 21:
        return 13;
    case 7:
    case 8:
        return 9;
    case 9:
    case 10:
        return 10;
    case 12:
        return 5;
    case 14:
        return 3;
    case 15:
        return 2;
    case 16:
        return 1;
    case 24:
        return 7;
    default:
        return -1;
    }
}

PcSpeakerSynth::PcSpeakerSynth(
    std::vector<std::vector<std::uint8_t>> sequences
) : sequences_(std::move(sequences)) {}

void PcSpeakerSynth::startMusic() {
    startSequence(0);
}

void PcSpeakerSynth::triggerLevel1Event(const int eventId) {
    const int index = level1EventSequenceIndex(eventId);
    if (index >= 0) {
        startSequence(static_cast<std::size_t>(index));
    }
}

void PcSpeakerSynth::startSequence(const std::size_t index) {
    if (index >= sequences_.size()) {
        return;
    }
    const auto& sequence = sequences_[index];
    const std::size_t base = sequenceDataStart(sequence);
    active_ = &sequence;
    selectedChannel_ = selectedChannel(sequence, base);
    position_ = base + 34;
    runningStatus_ = 0;
    delay_ = 0;
    pitchBend_ = 0x2000;
    note_ = -1;
    divisor_ = 0;
    sequenceSamplePhase_ = sampleRate;
    squarePhase_ = 0;
}

void PcSpeakerSynth::stopSequence() {
    active_ = nullptr;
    note_ = -1;
    divisor_ = 0;
}

bool PcSpeakerSynth::readMessage(
    std::uint8_t& status,
    std::uint8_t& first,
    std::uint8_t& second,
    std::uint16_t& delay
) {
    if (active_ == nullptr || position_ >= active_->size()) {
        return false;
    }
    const auto& data = *active_;
    std::uint8_t value = data[position_++];
    if (value == 0xfc) {
        return false;
    }
    if ((value & 0x80) != 0) {
        runningStatus_ = value;
        if ((value & 0xf0) == 0xf0) {
            while (position_ < data.size() && data[position_++] != 0xf7) {
            }
            status = value;
            first = 0;
            second = 0;
        } else {
            if (position_ >= data.size()) {
                return false;
            }
            status = value;
            first = data[position_++];
            second = 0;
            if ((status & 0xf0) != 0xc0 && (status & 0xf0) != 0xd0) {
                if (position_ >= data.size()) {
                    return false;
                }
                second = data[position_++];
            }
        }
    } else {
        if (runningStatus_ == 0) {
            return false;
        }
        status = runningStatus_;
        first = value;
        second = 0;
        if ((status & 0xf0) != 0xc0 && (status & 0xf0) != 0xd0) {
            if (position_ >= data.size()) {
                return false;
            }
            second = data[position_++];
        }
    }

    delay = 0;
    while (position_ < data.size() && data[position_] == 0xf8) {
        delay = static_cast<std::uint16_t>(delay + 240);
        ++position_;
    }
    if (position_ >= data.size() || data[position_] == 0xfc) {
        return false;
    }
    delay = static_cast<std::uint16_t>(delay + data[position_++]);
    return true;
}

void PcSpeakerSynth::applyMessage(
    const std::uint8_t status,
    const std::uint8_t first,
    const std::uint8_t second
) {
    if ((status & 0x0f) != selectedChannel_) {
        return;
    }
    switch (status & 0xf0) {
    case 0x80:
        if (note_ == first) {
            note_ = -1;
            divisor_ = 0;
        }
        break;
    case 0x90:
        if (second == 0) {
            if (note_ == first) {
                note_ = -1;
                divisor_ = 0;
            }
        } else {
            note_ = first;
            divisor_ = pcSpeakerDivisor(first, pitchBend_);
            squarePhase_ = 0;
        }
        break;
    case 0xb0:
        if (first == 0x4b && second == 0) {
            note_ = -1;
            divisor_ = 0;
        }
        break;
    case 0xe0:
        pitchBend_ = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(second) << 7) | first
        );
        if (note_ >= 0) {
            divisor_ = pcSpeakerDivisor(
                static_cast<std::uint8_t>(note_),
                pitchBend_
            );
            squarePhase_ = 0;
        }
        break;
    default:
        break;
    }
}

void PcSpeakerSynth::advanceSequenceTick() {
    if (active_ == nullptr) {
        return;
    }
    if (delay_ > 0) {
        --delay_;
        return;
    }
    while (active_ != nullptr) {
        std::uint8_t status = 0;
        std::uint8_t first = 0;
        std::uint8_t second = 0;
        std::uint16_t delay = 0;
        if (!readMessage(status, first, second, delay)) {
            stopSequence();
            return;
        }
        applyMessage(status, first, second);
        if (delay > 0) {
            delay_ = static_cast<std::uint16_t>(delay - 1);
            return;
        }
    }
}

std::vector<std::int16_t> PcSpeakerSynth::render(
    const std::size_t sampleCount
) {
    std::vector<std::int16_t> output(sampleCount, 0);
    for (std::size_t index = 0; index < sampleCount; ++index) {
        sequenceSamplePhase_ += sequenceRate;
        if (sequenceSamplePhase_ >= sampleRate) {
            sequenceSamplePhase_ -= sampleRate;
            advanceSequenceTick();
        }
        if (divisor_ != 0) {
            squarePhase_ += pitClock;
            const std::uint64_t period =
                static_cast<std::uint64_t>(divisor_) * sampleRate;
            while (squarePhase_ >= period) {
                squarePhase_ -= period;
            }
            output[index] = squarePhase_ * 2 < period
                ? speakerAmplitude
                : 0;
        }
    }
    return output;
}

}  // namespace stellar7
