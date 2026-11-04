#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace stellar7 {

struct SampledVoiceData {
    std::uint16_t nominalRate;
    std::uint8_t timeConstant;
    std::vector<std::uint8_t> samples;
};

SampledVoiceData decodeSampledVoice(const std::vector<std::uint8_t>& sequence);

enum class SampledVoicePhase { unloaded, loaded, pending, playing, irqPending, completionPending, complete, stopped };

struct SampledVoiceObservation {
    SampledVoicePhase phase;
    std::size_t transferred;
    std::size_t sampleCount;
    std::uint64_t clockPhase;
    std::uint64_t clockPeriod;
    std::uint8_t dacValue;
    bool speakerEnabled;
};

// Native digital DAC projection, not a DSP/DMA/IRQ or analog-filter emulator.
// The caller supplies audio service boundaries and the existing output frames.
class SampledVoiceStream {
public:
    static constexpr unsigned outputRate = 44100;
    void load(const std::vector<std::uint8_t>& sequence);
    void validateArm() const;
    void arm();
    void service() noexcept;
    void serviceTransferIrq();
    void stop();
    void release();
    bool busy() const noexcept;
    bool resident() const noexcept;
    SampledVoiceObservation observation() const noexcept;
    void mix(std::vector<std::int16_t>& samples);

private:
    SampledVoiceData data_{};
    SampledVoicePhase phase_ = SampledVoicePhase::unloaded;
    std::size_t transferred_ = 0;
    std::uint64_t clockPhase_ = 0;
    std::uint8_t dacValue_ = 128;
    bool speakerEnabled_ = false;
};

} // namespace stellar7
