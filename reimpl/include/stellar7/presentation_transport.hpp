#pragma once

#include "stellar7/controlled_session.hpp"

#include <deque>
#include <optional>

namespace stellar7 {

enum class TransportResult { accepted, busy };

// Accepted calls retain/copy all data before returning; busy calls consume none.
class PcmSink {
public:
    virtual ~PcmSink() = default;
    virtual TransportResult trySubmit(const std::vector<std::int16_t>& samples) = 0;
    virtual std::uint64_t playedSamples() = 0;
};

class ScanSink {
public:
    virtual ~ScanSink() = default;
    virtual TransportResult tryPresent(const QuietScanout::CompletedScan& scan) = 0;
};

// Borrows sinks; neither sink may retain a reference to a callback argument.
class PresentationTransport {
public:
    PresentationTransport(PcmSink& audio, ScanSink& scans, std::size_t capacity = 1);
    PresentationTransport(const PresentationTransport&) = delete;
    PresentationTransport& operator=(const PresentationTransport&) = delete;
    PresentationTransport(PresentationTransport&&) = delete;
    PresentationTransport& operator=(PresentationTransport&&) = delete;

    // Moves delivery only on acceptance. A full queue leaves it untouched.
    TransportResult tryAccept(ControlledSessionDelivery& delivery);
    TransportResult pump();
    bool hasPending() const noexcept;
    bool canAccept() const noexcept;
    // Closes the transport and discards only data not yet accepted by sinks.
    void cancel();

private:
    enum class State { ready, pumping, rejected, cancelled };
    void requireReady() const;
    std::uint64_t validate(const ControlledSessionDelivery& delivery) const;
    static void requireResult(TransportResult result);

    PcmSink& audio_;
    ScanSink& scans_;
    struct Pending {
        ControlledSessionDelivery delivery;
        std::size_t nextScan = 0;
        bool audioSubmitted = false;
    };
    const std::size_t capacity_;
    std::deque<Pending> pending_;
    std::uint64_t receivedEnd_ = 0;
    std::uint64_t submittedEnd_ = 0;
    std::uint64_t played_ = 0;
    std::optional<std::uint64_t> lastScanPosition_;
    State state_ = State::ready;
};

} // namespace stellar7
