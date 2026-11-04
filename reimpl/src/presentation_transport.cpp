#include "stellar7/presentation_transport.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace stellar7 {

PresentationTransport::PresentationTransport(PcmSink& audio, ScanSink& scans, const std::size_t capacity)
    : audio_(audio), scans_(scans), capacity_(capacity) {
    if (capacity_ == 0) {
        throw std::invalid_argument("presentation transport requires positive pending capacity");
    }
}

void PresentationTransport::requireReady() const {
    if (state_ != State::ready) {
        throw std::logic_error("presentation transport is unavailable");
    }
}

std::uint64_t PresentationTransport::validate(const ControlledSessionDelivery& delivery) const {
    if (delivery.firstSample != receivedEnd_) {
        throw std::invalid_argument("presentation transport requires contiguous sample batches");
    }
    if (delivery.samples.size() > std::numeric_limits<std::uint64_t>::max() - receivedEnd_) {
        throw std::invalid_argument("presentation transport sample range overflows");
    }
    const auto end = receivedEnd_ + static_cast<std::uint64_t>(delivery.samples.size());
    auto previous = lastScanPosition_;
    for (const auto& completed : delivery.scans) {
        if (completed.samplePosition < delivery.firstSample || completed.samplePosition > end ||
            (previous && completed.samplePosition < *previous)) {
            throw std::invalid_argument("presentation transport has unordered or out-of-range scan position");
        }
        if (completed.scan.rgb.size() != QuietScanout::frameBytes ||
            (completed.scan.page != 0xa000 && completed.scan.page != 0xa400)) {
            throw std::invalid_argument("presentation transport requires complete quiet-profile scan");
        }
        previous = completed.samplePosition;
    }
    return end;
}

void PresentationTransport::requireResult(const TransportResult result) {
    if (result != TransportResult::accepted && result != TransportResult::busy) {
        throw std::invalid_argument("presentation sink returned unsupported result");
    }
}

TransportResult PresentationTransport::tryAccept(ControlledSessionDelivery& delivery) {
    requireReady();
    if (pending_.size() >= capacity_) {
        return TransportResult::busy;
    }
    try {
        const auto end = validate(delivery);
        if (!delivery.scans.empty()) {
            lastScanPosition_ = delivery.scans.back().samplePosition;
        }
        pending_.push_back({std::move(delivery), 0, false});
        receivedEnd_ = end;
        return TransportResult::accepted;
    } catch (...) {
        state_ = State::rejected;
        throw;
    }
}

TransportResult PresentationTransport::pump() {
    requireReady();
    if (pending_.empty()) {
        return TransportResult::accepted;
    }
    state_ = State::pumping;
    try {
        for (auto& batch : pending_) {
            if (batch.audioSubmitted) {
                continue;
            }
            if (!batch.delivery.samples.empty()) {
                const auto result = audio_.trySubmit(batch.delivery.samples);
                requireResult(result);
                if (result == TransportResult::busy) {
                    break;
                }
            }
            batch.audioSubmitted = true;
            submittedEnd_ = batch.delivery.firstSample + static_cast<std::uint64_t>(batch.delivery.samples.size());
        }
        if (!pending_.front().audioSubmitted) {
            state_ = State::ready;
            return TransportResult::busy;
        }
        const auto played = audio_.playedSamples();
        if (played < played_ || played > submittedEnd_) {
            throw std::invalid_argument("presentation sink has invalid played-sample position");
        }
        played_ = played;
        while (!pending_.empty()) {
            auto& batch = pending_.front();
            if (!batch.audioSubmitted) {
                break;
            }
            while (batch.nextScan < batch.delivery.scans.size()) {
                const auto& completed = batch.delivery.scans[batch.nextScan];
                if (played_ < completed.samplePosition) {
                    state_ = State::ready;
                    return TransportResult::busy;
                }
                const auto result = scans_.tryPresent(completed.scan);
                requireResult(result);
                if (result == TransportResult::busy) {
                    state_ = State::ready;
                    return TransportResult::busy;
                }
                ++batch.nextScan;
            }
            pending_.pop_front();
        }
        state_ = State::ready;
        return pending_.empty() ? TransportResult::accepted : TransportResult::busy;
    } catch (...) {
        state_ = State::rejected;
        throw;
    }
}

bool PresentationTransport::hasPending() const noexcept {
    return !pending_.empty();
}

bool PresentationTransport::canAccept() const noexcept {
    return state_ == State::ready && pending_.size() < capacity_;
}

void PresentationTransport::cancel() {
    if (state_ == State::pumping) {
        throw std::logic_error("presentation transport cannot cancel during sink call");
    }
    pending_.clear();
    state_ = State::cancelled;
}

} // namespace stellar7
