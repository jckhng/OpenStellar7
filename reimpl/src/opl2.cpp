#include "stellar7/audio.hpp"

#include "ymfm_opl.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace stellar7 {

class Opl2Chip::Impl final : public ymfm::ymfm_interface {
public:
    Impl() : chip_(*this), nativeSampleRate_(chip_.sample_rate(inputClock)) {
        reset();
    }

    void reset() {
        chip_.reset();
        nativePhase_ = 0;
        currentSample_ = 0;
    }

    void writeRegister(
        const std::uint8_t registerNumber,
        const std::uint8_t value
    ) {
        chip_.write_address(registerNumber);
        chip_.write_data(value);
    }

    std::vector<std::int16_t> render(const std::size_t sampleCount) {
        std::vector<std::int16_t> output(sampleCount);
        for (std::int16_t& sample : output) {
            nativePhase_ += nativeSampleRate_;
            while (nativePhase_ >= sampleRate) {
                nativePhase_ -= sampleRate;
                ymfm::ym3812::output_data generated{};
                chip_.generate(&generated);
                currentSample_ = std::clamp(
                    generated.data[0],
                    static_cast<std::int32_t>(
                        std::numeric_limits<std::int16_t>::min()
                    ),
                    static_cast<std::int32_t>(
                        std::numeric_limits<std::int16_t>::max()
                    )
                );
            }
            sample = static_cast<std::int16_t>(currentSample_);
        }
        return output;
    }

    unsigned nativeSampleRate() const noexcept {
        return nativeSampleRate_;
    }

private:
    ymfm::ym3812 chip_;
    unsigned nativeSampleRate_;
    std::uint64_t nativePhase_ = 0;
    std::int32_t currentSample_ = 0;
};

Opl2Chip::Opl2Chip() : impl_(std::make_unique<Impl>()) {}
Opl2Chip::~Opl2Chip() = default;
Opl2Chip::Opl2Chip(Opl2Chip&&) noexcept = default;
Opl2Chip& Opl2Chip::operator=(Opl2Chip&&) noexcept = default;

void Opl2Chip::reset() {
    impl_->reset();
}

void Opl2Chip::writeRegister(
    const std::uint8_t registerNumber,
    const std::uint8_t value
) {
    impl_->writeRegister(registerNumber, value);
}

std::vector<std::int16_t> Opl2Chip::render(
    const std::size_t sampleCount
) {
    return impl_->render(sampleCount);
}

unsigned Opl2Chip::nativeSampleRate() const noexcept {
    return impl_->nativeSampleRate();
}

}  // namespace stellar7
