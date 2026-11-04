#include "stellar7/sampled_voice.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {

constexpr std::size_t envelopeSize = 2;
constexpr std::size_t bodyHeaderSize = 34;
constexpr std::size_t sampleHeaderSize = 45;
constexpr std::size_t rateOffset = 15;
constexpr std::size_t countOffset = 33;
constexpr std::uint32_t dspClock = 1'000'000;
constexpr std::uint16_t minimumDriverRate = 0x0f34;
constexpr int unsignedSilence = 128;
constexpr int byteAmplitude = 256;

std::uint16_t word(const std::vector<std::uint8_t>& sequence, const std::size_t offset) {
    if (offset > sequence.size() || sequence.size() - offset < 2) {
        throw std::invalid_argument("truncated sampled voice word");
    }
    return static_cast<std::uint16_t>(sequence[offset]
        | (static_cast<std::uint16_t>(sequence[offset + 1]) << 8));
}

} // namespace

SampledVoiceData decodeSampledVoice(const std::vector<std::uint8_t>& sequence) {
    constexpr auto header = envelopeSize + bodyHeaderSize;
    constexpr auto samples = header + sampleHeaderSize;
    if (sequence.size() < samples || sequence[0] != 0x84 || sequence[1] != 0
        || sequence[envelopeSize] != 2 || word(sequence, envelopeSize + 31) != 0
        || sequence[header] != 0xfc || sequence[header + 1] == 0xfc) {
        throw std::invalid_argument("sampled voice requires the admitted type2 zero-offset SNG header");
    }
    const auto rate = word(sequence, header + rateOffset);
    const auto count = word(sequence, header + countOffset);
    if (rate < minimumDriverRate || rate > std::numeric_limits<std::int16_t>::max()
        || count == 0 || sequence.size() - samples < count) {
        throw std::invalid_argument("unsupported sampled voice rate or truncated byte extent");
    }
    const auto timeConstant = static_cast<std::uint8_t>(0U - dspClock / rate);
    //19A4 stores the header count at CS31; bytes beyond that DMA extent are unused.
    return {rate, timeConstant, {sequence.begin() + samples, sequence.begin() + samples + count}};
}

void SampledVoiceStream::load(const std::vector<std::uint8_t>& sequence) {
    if (phase_ != SampledVoicePhase::unloaded) {
        throw std::invalid_argument("sampled voice load requires released prior storage");
    }
    auto data = decodeSampledVoice(sequence);
    data_ = std::move(data);
    phase_ = SampledVoicePhase::loaded;
    transferred_ = 0;
    clockPhase_ = 0;
}

void SampledVoiceStream::validateArm() const {
    if (phase_ != SampledVoicePhase::loaded && phase_ != SampledVoicePhase::complete
        && phase_ != SampledVoicePhase::stopped) {
        throw std::invalid_argument("sampled voice start requires loaded and inactive storage");
    }
}

void SampledVoiceStream::arm() {
    validateArm();
    transferred_ = 0;
    clockPhase_ = 0;
    speakerEnabled_ = true; //19A4/2A36, volume15 -> DSP D1; no digital gain table.
    phase_ = SampledVoicePhase::pending;
}

void SampledVoiceStream::service() noexcept {
    if (phase_ == SampledVoicePhase::pending) {
        phase_ = SampledVoicePhase::playing;
    } else if (phase_ == SampledVoicePhase::completionPending) {
        phase_ = SampledVoicePhase::complete;
    }
}

void SampledVoiceStream::serviceTransferIrq() {
    if (phase_ != SampledVoicePhase::irqPending) {
        throw std::invalid_argument("sampled voice completion IRQ requires a finished transfer");
    }
    phase_ = SampledVoicePhase::completionPending;
}

void SampledVoiceStream::stop() {
    if (phase_ == SampledVoicePhase::irqPending) {
        throw std::invalid_argument("sampled voice cancellation racing a pending IRQ is unsupported");
    }
    if (phase_ != SampledVoicePhase::unloaded && phase_ != SampledVoicePhase::loaded) {
        phase_ = SampledVoicePhase::stopped;
    }
    //DSP D0 cancels DMA, not D3 speaker-disable or a neutral DAC write.
}

void SampledVoiceStream::release() {
    if (phase_ != SampledVoicePhase::unloaded && phase_ != SampledVoicePhase::loaded
        && phase_ != SampledVoicePhase::stopped) {
        throw std::invalid_argument("sampled voice storage requires stopped or never-started ownership");
    }
    data_ = {};
    phase_ = SampledVoicePhase::unloaded;
    transferred_ = 0;
    clockPhase_ = 0;
}

bool SampledVoiceStream::busy() const noexcept {
    return phase_ == SampledVoicePhase::playing || phase_ == SampledVoicePhase::irqPending;
}

bool SampledVoiceStream::resident() const noexcept {
    return phase_ == SampledVoicePhase::pending || phase_ == SampledVoicePhase::playing
        || phase_ == SampledVoicePhase::irqPending || phase_ == SampledVoicePhase::completionPending
        || phase_ == SampledVoicePhase::complete;
}

SampledVoiceObservation SampledVoiceStream::observation() const noexcept {
    const auto period = data_.samples.empty() ? 0U
        : static_cast<std::uint64_t>(outputRate) * (256U - data_.timeConstant);
    return {phase_, transferred_, data_.samples.size(), clockPhase_, period, dacValue_, speakerEnabled_};
}

void SampledVoiceStream::mix(std::vector<std::int16_t>& samples) {
    const auto period = observation().clockPeriod;
    for (auto& sample : samples) {
        // Digital hold and unity signed8->signed16 conversion are explicit native
        // output policy. Original DAC/filter/mixer phase is not qualified here.
        const auto voice = speakerEnabled_ ? (static_cast<int>(dacValue_) - unsignedSilence) * byteAmplitude : 0;
        const auto mixed = static_cast<int>(sample) + voice;
        sample = static_cast<std::int16_t>(std::clamp(mixed,
            static_cast<int>(std::numeric_limits<std::int16_t>::min()),
            static_cast<int>(std::numeric_limits<std::int16_t>::max())));
        if (phase_ != SampledVoicePhase::playing) {
            continue;
        }
        clockPhase_ += dspClock;
        while (clockPhase_ >= period) {
            clockPhase_ -= period;
            dacValue_ = data_.samples.at(transferred_++);
            if (transferred_ == data_.samples.size()) {
                phase_ = SampledVoicePhase::irqPending;
                break;
            }
        }
    }
}

} // namespace stellar7
