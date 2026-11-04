#include "../src/quiet_scanout.hpp"

#include <cassert>
#include <functional>

namespace {
stellar7::SoundBlasterClockPhase phase() {
    using namespace stellar7;
    SoundBlasterClockPhase p{};
    p.remainingCycles = 5000;
    p.interrupts = AudioClockInterrupts::enabled;
    p.isa8DelayNanoseconds = 1020;
    p.dmaDacRate = 44100;
    p.pitPeriodMilliseconds = 55;
    p.singleLinePeriodMilliseconds = 0.03;
    p.horizontalPeriodMilliseconds = 0.03;
    p.linesTotal = 400;
    p.linesDone = 400;
    p.minimumLatchDelayMilliseconds = 0.001;
    p.frameStart = AudioClockFrameStart::activeUnlockedVgaSurface;
    p.vgaInterrupt = AudioClockVgaInterrupt::fixedVgaNoDemand;
    p.devices = AudioClockDevicePhase{};
    p.devices->fixedPitAndMaskedDma = true;
    p.devices->picTicks = 100;
    p.devices->framePeriodMilliseconds = 14;
    p.devices->displayEndMilliseconds = 12;
    p.devices->retraceStartMilliseconds = 13;
    p.devices->retraceEndMilliseconds = 13.1;
    p.devices->frame = AudioClockVgaFramePhase{1000.0 / 14, 100, 0};
    p.events = {{0.1, AudioClockEventKind::frameStart, 0}};
    return p;
}

bool rejects(const std::function<void()>& operation) {
    try {
        operation();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}
}

int main() {
    using namespace stellar7;
    const auto joined = normalWorkTiming(phase(), {{NormalWorkKind::instructions, 80000}}, 100000);
    const auto first = normalWorkTiming(phase(), {{NormalWorkKind::instructions, 40000}}, 100000);
    const auto second = normalWorkTiming(first.phase, {{NormalWorkKind::instructions, 40000}}, 100000);
    auto split = first.vgaServices;
    for (auto receipt : second.vgaServices) {
        receipt.cycleOffset += first.total.elapsedCycles;
        split.push_back(receipt);
    }
    assert(joined.total.elapsedCycles == first.total.elapsedCycles + second.total.elapsedCycles);
    assert(joined.vgaServices.size() == split.size());
    for (std::size_t i = 0; i < split.size(); ++i) {
        const auto& a = joined.vgaServices[i];
        const auto& b = split[i];
        assert(a.kind == b.kind && a.cycleOffset == b.cycleOffset);
        assert(a.scheduledMilliseconds == b.scheduledMilliseconds && a.servicedMilliseconds == b.servicedMilliseconds);
        assert(a.frameStartMilliseconds == b.frameStartMilliseconds && a.realStart == b.realStart);
        assert(a.line == b.line && a.linesTotal == b.linesTotal);
    }

    auto nestedPhase = phase();
    nestedPhase.interruptDelivery = AudioClockInterruptDelivery::residentTimerIrq0;
    nestedPhase.devices->irq0Requested = true;
    nestedPhase.irq0ActivationPossible = true;
    nestedPhase.irqDelayMilliseconds = 0.002;
    nestedPhase.timer = AudioClockTimerPhase{1, 0, 5, 1, 4};
    nestedPhase.callback = SoundBlasterCallbackState{};
    nestedPhase.callback->wait = 2;
    nestedPhase.remainingCycles = 1;
    nestedPhase.deferredCycles = 4999;
    nestedPhase.horizontalPeriodMilliseconds = 0.001;
    nestedPhase.singleLinePeriodMilliseconds = 0.001;
    nestedPhase.events = {{0.0002, AudioClockEventKind::irqCheck, 0}, {0.008, AudioClockEventKind::frameStart, 0}};
    const auto nested = normalWorkTiming(nestedPhase, {{NormalWorkKind::instructions, 150}}, 1000);
    assert(nested.callbacks.size() == 1 && nested.phase.callback->wait == 1);
    const auto callbackStart = nested.callbacks[0].cycleOffset;
    const auto callbackEnd = nested.callbacks[0].timing.total.elapsedCycles;
    std::size_t servicesWithinCallback = 0;
    std::uint32_t expectedLine = 0;
    for (const auto& receipt : nested.vgaServices) {
        if (receipt.cycleOffset >= callbackStart && receipt.cycleOffset < callbackEnd) {
            ++servicesWithinCallback;
        }
        if (receipt.kind == VgaServiceKind::scanLine) {
            assert(receipt.line == expectedLine++);
        }
    }
    assert(servicesWithinCallback > 0 && expectedLine > 0);

    IndexedPages pages;
    for (std::int16_t y = 0; y < 200; ++y) {
        for (std::int16_t x = 0; x < 320; ++x) {
            pages.write(0xa000, {x, y}, 1);
            pages.write(0xa400, {x, y}, 2);
        }
    }
    PaletteWorkState palette{};
    palette.dac.colors[3] = 63;
    palette.dac.colors[7] = 63;
    QuietScanoutCollector collector;
    collector.consume(first.vgaServices, pages, palette, 0xa400);
    assert(collector.completed().empty() && collector.pendingLines() > 0);
    collector.consume(second.vgaServices, pages, palette, 0xa400);
    assert(collector.completed().size() == 1);
    const auto& scan = collector.completed()[0];
    assert(scan.page == 0xa000 && scan.rgb.size() == QuietScanoutCollector::frameBytes);
    assert(scan.rgb[0] == 0 && scan.rgb[1] == 0 && scan.rgb[2] == 255 && scan.rgb[3] == 0);

    // A later hardware latch must not change the already-started physical cursor.
    auto latch = second.vgaServices;
    for (auto& receipt : latch) {
        if (receipt.kind == VgaServiceKind::scanLine) {
            receipt.realStart = 0x4000;
        }
    }
    QuietScanoutCollector latched;
    latched.consume(first.vgaServices, pages, palette);
    latched.consume(latch, pages, palette);
    assert(latched.completed()[0].rgb == scan.rgb);
    auto otherPhase = phase();
    otherPhase.devices->realStart = 0x4000;
    QuietScanoutCollector other;
    other.consume(normalWorkTiming(otherPhase, {{NormalWorkKind::instructions, 65000}}, 100000).vgaServices,
        pages, palette);
    assert(other.completed()[0].page == 0xa400 && other.completed()[0].rgb[1] == 255);

    QuietScanoutCollector unknown;
    assert(rejects([&]() { unknown.consume(joined.vgaServices, IndexedPages{}, palette); }));
    QuietScanoutCollector writer;
    assert(rejects([&]() { writer.consume(first.vgaServices, pages, palette, 0xa000); }));
    QuietScanoutCollector changedPalette;
    changedPalette.consume(first.vgaServices, pages, palette);
    auto changed = palette;
    changed.dac.colors[3] = 0;
    assert(rejects([&]() { changedPalette.consume(second.vgaServices, pages, changed); }));
    auto missing = joined.vgaServices;
    missing.erase(missing.begin() + 2);
    QuietScanoutCollector missingLine;
    assert(rejects([&]() { missingLine.consume(missing, pages, palette); }));
    auto unsupported = joined.vgaServices;
    unsupported[0].realStart = 0x8000;
    QuietScanoutCollector badPage;
    assert(rejects([&]() { badPage.consume(unsupported, pages, palette); }));
    QuietScanoutCollector incoming;
    incoming.consume({joined.vgaServices[1]}, pages, palette);
    assert(incoming.completed().empty() && incoming.discardedInitialLines() == 1);
}
