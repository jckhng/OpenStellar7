#include "stellar7/presentation_transport.hpp"

#include <cassert>
#include <functional>
#include <stdexcept>
#include <type_traits>

namespace {
using namespace stellar7;

static_assert(!std::is_copy_constructible_v<PresentationTransport>);
static_assert(!std::is_move_constructible_v<PresentationTransport>);

bool rejects(const std::function<void()>& operation) {
    try {
        operation();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

struct FakePcm final : PcmSink {
    unsigned busyCalls = 0;
    unsigned calls = 0;
    unsigned positionCalls = 0;
    std::uint64_t played = 0;
    bool failSubmit = false;
    bool failPosition = false;
    bool invalidResult = false;
    unsigned busyOnCall = 0;
    std::function<void()> duringSubmit;
    std::vector<std::vector<std::int16_t>> attempts;
    std::vector<std::vector<std::int16_t>> accepted;

    TransportResult trySubmit(const std::vector<std::int16_t>& samples) override {
        ++calls;
        attempts.push_back(samples);
        if (duringSubmit) {
            duringSubmit();
        }
        if (failSubmit) {
            throw std::runtime_error("synthetic PCM error");
        }
        if (invalidResult) {
            return static_cast<TransportResult>(-1);
        }
        if (calls == busyOnCall) {
            return TransportResult::busy;
        }
        if (busyCalls != 0) {
            --busyCalls;
            return TransportResult::busy;
        }
        accepted.push_back(samples);
        return TransportResult::accepted;
    }
    std::uint64_t playedSamples() override {
        ++positionCalls;
        if (failPosition) {
            throw std::runtime_error("synthetic position error");
        }
        return played;
    }
};

struct FakeScans final : ScanSink {
    unsigned busyCalls = 0;
    unsigned calls = 0;
    bool fail = false;
    bool invalidResult = false;
    std::function<void()> duringPresent;
    std::vector<QuietScanout::CompletedScan> accepted;

    TransportResult tryPresent(const QuietScanout::CompletedScan& scan) override {
        ++calls;
        if (duringPresent) {
            duringPresent();
        }
        if (fail) {
            throw std::runtime_error("synthetic scan error");
        }
        if (invalidResult) {
            return static_cast<TransportResult>(-1);
        }
        if (busyCalls != 0) {
            --busyCalls;
            return TransportResult::busy;
        }
        accepted.push_back(scan);
        return TransportResult::accepted;
    }
};

ControlledScan scan(std::uint64_t position, std::uint8_t marker) {
    ControlledScan result{};
    result.samplePosition = position;
    result.scan.page = 0xa000;
    result.scan.rgb.assign(QuietScanout::frameBytes, marker);
    return result;
}

ControlledSessionDelivery batch(std::uint64_t first, std::vector<std::int16_t> samples,
    std::vector<ControlledScan> scans = {}) {
    ControlledSessionDelivery result{};
    result.firstSample = first;
    result.samples = std::move(samples);
    result.scans = std::move(scans);
    return result;
}

void backpressureAndOrder() {
    FakePcm audio;
    FakeScans images;
    PresentationTransport queue(audio, images);
    auto delivery = batch(0, {11, 22, 33, 44}, {scan(0, 1), scan(2, 2), scan(4, 3)});
    assert(queue.tryAccept(delivery) == TransportResult::accepted);
    auto next = batch(4, {55});
    assert(queue.tryAccept(next) == TransportResult::busy);
    assert(next.firstSample == 4 && next.samples == std::vector<std::int16_t>{55});

    audio.busyCalls = 1;
    assert(queue.pump() == TransportResult::busy);
    assert(audio.calls == 1 && audio.positionCalls == 0 && images.calls == 0);
    assert(queue.hasPending());
    assert(queue.pump() == TransportResult::busy);
    assert(audio.calls == 2 && audio.accepted.size() == 1);
    assert(audio.attempts[0] == audio.attempts[1]);
    assert(images.accepted.size() == 1 && images.accepted[0].rgb.front() == 1);

    audio.played = 2;
    images.busyCalls = 1;
    assert(queue.pump() == TransportResult::busy);
    assert(audio.calls == 2 && images.accepted.size() == 1);
    assert(queue.pump() == TransportResult::busy);
    assert(audio.calls == 2 && images.accepted.size() == 2);
    assert(images.accepted[1].rgb.front() == 2);
    audio.played = 4;
    assert(queue.pump() == TransportResult::accepted);
    assert(!queue.hasPending() && audio.calls == 2 && images.calls == 4);
    assert(images.accepted.size() == 3 && images.accepted[2].rgb.front() == 3);
    for (std::size_t index = 0; index < images.accepted.size(); ++index) {
        assert(images.accepted[index].rgb == std::vector<std::uint8_t>(QuietScanout::frameBytes,
            static_cast<std::uint8_t>(index + 1)));
    }
    assert(queue.pump() == TransportResult::accepted && images.calls == 4);

    assert(queue.tryAccept(next) == TransportResult::accepted);
    assert(queue.pump() == TransportResult::accepted);
    assert(audio.accepted.size() == 2 && audio.accepted[1] == std::vector<std::int16_t>{55});
}

void malformedBatches() {
    const std::vector<std::function<void(ControlledSessionDelivery&)>> malformed{
        [](auto& delivery) { delivery.firstSample = 1; },
        [](auto& delivery) { delivery.scans[0].samplePosition = 3; },
        [](auto& delivery) { delivery.scans.push_back(scan(0, 2)); },
        [](auto& delivery) { delivery.scans[0].scan.rgb.pop_back(); },
        [](auto& delivery) { delivery.scans[0].scan.page = 0; },
    };
    for (const auto& mutate : malformed) {
        FakePcm audio;
        FakeScans images;
        PresentationTransport queue(audio, images);
        auto delivery = batch(0, {1, 2}, {scan(1, 1)});
        mutate(delivery);
        assert(rejects([&] { queue.tryAccept(delivery); }));
        assert(audio.calls == 0 && images.calls == 0);
        assert(rejects([&] { queue.pump(); }));
        queue.cancel();
        assert(!queue.hasPending());
        assert(rejects([&] { queue.tryAccept(delivery); }));
    }
}

void emptyAndCrossBatchRanges() {
    FakePcm audio;
    FakeScans images;
    PresentationTransport queue(audio, images);
    auto empty = batch(0, {}, {scan(0, 1)});
    assert(queue.tryAccept(empty) == TransportResult::accepted);
    assert(queue.pump() == TransportResult::accepted);
    assert(audio.calls == 0 && images.accepted.size() == 1);
    auto first = batch(0, {1, 2});
    assert(queue.tryAccept(first) == TransportResult::accepted);
    assert(queue.pump() == TransportResult::accepted);
    auto skipped = batch(3, {3});
    assert(rejects([&] { queue.tryAccept(skipped); }));

    FakePcm anotherAudio;
    FakeScans anotherImages;
    PresentationTransport another(anotherAudio, anotherImages);
    auto one = batch(0, {1});
    assert(another.tryAccept(one) == TransportResult::accepted);
    assert(another.pump() == TransportResult::accepted);
    auto stale = batch(1, {2}, {scan(0, 3)});
    assert(rejects([&] { another.tryAccept(stale); }));
}

void sinkErrorsAndPlaybackGuard() {
    for (unsigned control = 0; control != 7; ++control) {
        FakePcm audio;
        FakeScans images;
        PresentationTransport queue(audio, images);
        auto delivery = batch(0, {1, 2}, {scan(2, 1)});
        assert(queue.tryAccept(delivery) == TransportResult::accepted);
        if (control == 0) {
            audio.failSubmit = true;
        } else if (control == 1) {
            audio.failPosition = true;
        } else if (control == 2) {
            audio.played = 2;
            images.fail = true;
        } else if (control == 3) {
            audio.played = 3;
        } else if (control == 4) {
            audio.played = 1;
            assert(queue.pump() == TransportResult::busy);
            audio.played = 0;
        } else if (control == 5) {
            audio.invalidResult = true;
        } else {
            audio.played = 2;
            images.invalidResult = true;
        }
        assert(rejects([&] { queue.pump(); }));
        const auto calls = audio.calls;
        assert(rejects([&] { queue.pump(); }));
        assert(audio.calls == calls);
        queue.cancel();
        assert(!queue.hasPending());
    }
}

void cancellationAndReentry() {
    FakePcm audio;
    FakeScans images;
    PresentationTransport queue(audio, images);
    auto delivery = batch(0, {1, 2}, {scan(2, 1)});
    assert(queue.tryAccept(delivery) == TransportResult::accepted);
    auto next = batch(2, {3});
    const auto reenter = [&] {
        assert(rejects([&] { queue.pump(); }));
        assert(rejects([&] { queue.tryAccept(next); }));
        assert(rejects([&] { queue.cancel(); }));
        assert(next.samples == std::vector<std::int16_t>{3});
    };
    audio.duringSubmit = reenter;
    images.duringPresent = reenter;
    assert(queue.pump() == TransportResult::busy);
    assert(audio.accepted.size() == 1 && queue.hasPending());
    audio.played = 2;
    assert(queue.pump() == TransportResult::accepted);
    assert(images.accepted.size() == 1);
    queue.cancel();
    queue.cancel();
    assert(!queue.hasPending());
    assert(rejects([&] { queue.tryAccept(next); }));
    assert(rejects([&] { queue.pump(); }));

    FakePcm busyAudio;
    FakeScans busyImages;
    PresentationTransport busyQueue(busyAudio, busyImages);
    auto waiting = batch(0, {9}, {scan(1, 1)});
    assert(busyQueue.tryAccept(waiting) == TransportResult::accepted);
    busyAudio.busyCalls = 1;
    assert(busyQueue.pump() == TransportResult::busy);
    busyQueue.cancel();
    assert(busyAudio.accepted.empty() && busyImages.accepted.empty());
}

void lookahead() {
    FakePcm audio;
    FakeScans images;
    assert(rejects([&] { PresentationTransport invalid(audio, images, 0); }));
    PresentationTransport queue(audio, images, 2);
    auto first = batch(0, {1, 2}, {scan(2, 1)});
    auto second = batch(2, {3, 4}, {scan(4, 2)});
    auto third = batch(4, {5}, {scan(5, 3)});
    assert(queue.canAccept());
    assert(queue.tryAccept(first) == TransportResult::accepted);
    assert(queue.canAccept());
    assert(queue.tryAccept(second) == TransportResult::accepted);
    assert(!queue.canAccept());
    assert(queue.tryAccept(third) == TransportResult::busy);
    assert(third.firstSample == 4 && third.samples == std::vector<std::int16_t>{5});
    assert(queue.pump() == TransportResult::busy);
    assert(audio.accepted.size() == 2 && audio.calls == 2 && images.calls == 0);
    assert(audio.accepted[0] == std::vector<std::int16_t>({1, 2}));
    assert(audio.accepted[1] == std::vector<std::int16_t>({3, 4}));

    audio.played = 4;
    images.busyCalls = 1;
    assert(queue.pump() == TransportResult::busy);
    assert(audio.calls == 2 && images.accepted.empty());
    assert(queue.pump() == TransportResult::accepted);
    assert(images.accepted.size() == 2);
    assert(images.accepted[0].rgb.front() == 1 && images.accepted[1].rgb.front() == 2);
    assert(queue.canAccept() && !queue.hasPending());
    assert(queue.tryAccept(third) == TransportResult::accepted);
    assert(queue.pump() == TransportResult::busy);
    assert(audio.accepted.size() == 3 && audio.accepted[2] == std::vector<std::int16_t>{5});
    audio.played = 5;
    assert(queue.pump() == TransportResult::accepted);
}

void audioBusyStillPresentsOlderScans() {
    FakePcm audio;
    FakeScans images;
    PresentationTransport queue(audio, images, 2);
    auto first = batch(0, {1, 2}, {scan(2, 1)});
    auto second = batch(2, {3, 4}, {scan(4, 2)});
    assert(queue.tryAccept(first) == TransportResult::accepted);
    assert(queue.tryAccept(second) == TransportResult::accepted);
    audio.busyOnCall = 2;
    audio.played = 2;
    assert(queue.pump() == TransportResult::busy);
    assert(audio.accepted.size() == 1 && images.accepted.size() == 1);
    assert(images.accepted[0].rgb.front() == 1 && queue.canAccept());
    assert(queue.pump() == TransportResult::busy);
    assert(audio.accepted.size() == 2 && audio.calls == 3);
    assert(audio.attempts[1] == audio.attempts[2]);
    audio.played = 4;
    assert(queue.pump() == TransportResult::accepted);
    assert(images.accepted.size() == 2 && images.accepted[1].rgb.front() == 2);
}

void lookaheadSubmissionRangeAndPoison() {
    FakePcm audio;
    FakeScans images;
    PresentationTransport queue(audio, images, 2);
    auto first = batch(0, {1, 2}, {scan(2, 1)});
    auto second = batch(2, {3, 4}, {scan(4, 2)});
    assert(queue.tryAccept(first) == TransportResult::accepted);
    assert(queue.tryAccept(second) == TransportResult::accepted);
    audio.busyOnCall = 2;
    audio.played = 3;
    assert(rejects([&] { queue.pump(); }));
    assert(audio.accepted.size() == 1 && images.accepted.empty());
    assert(!queue.canAccept() && queue.hasPending());
    assert(rejects([&] { queue.pump(); }));
    queue.cancel();
    assert(!queue.hasPending() && !queue.canAccept());
}

} // namespace

int main() {
    backpressureAndOrder();
    malformedBatches();
    emptyAndCrossBatchRanges();
    sinkErrorsAndPlaybackGuard();
    cancellationAndReentry();
    lookahead();
    audioBusyStillPresentsOlderScans();
    lookaheadSubmissionRangeAndPoison();
}
