#include "stellar7/quiet_scanout.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <vector>

namespace {
using namespace stellar7;

constexpr std::uint16_t firstPage = 0xa000;
constexpr std::uint16_t secondPage = 0xa400;
constexpr std::uint32_t secondStart = 0x4000;
constexpr std::uint32_t logicalHeight = QuietScanout::height / 2;
constexpr double linePeriodMilliseconds = 0.01;
constexpr double serviceDelayMilliseconds = 0.002;
constexpr std::uint32_t splitLine = 157;

std::uint8_t pixelIndex(const std::uint16_t page, const std::uint32_t x,
    const std::uint32_t y) {
    return static_cast<std::uint8_t>(1 + (x + 2 * y + (page == secondPage ? 1 : 0)) % 3);
}

struct Fixture {
    IndexedPages pages;
    PaletteWorkState palette;

    Fixture() {
        for (const auto page : {firstPage, secondPage}) {
            for (std::uint32_t y = 0; y < logicalHeight; ++y) {
                for (std::uint32_t x = 0; x < QuietScanout::width; ++x) {
                    pages.write(page, {static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)},
                        pixelIndex(page, x, y));
                }
            }
        }
        palette.dac.colors[3] = 63;
        palette.dac.colors[7] = 63;
        palette.dac.colors[9] = 1;
        palette.dac.colors[10] = 17;
        palette.dac.colors[11] = 32;
    }
};

VgaServiceReceipt start(const double origin, const std::uint16_t page) {
    return {VgaServiceKind::scanStart, 0, origin, origin + serviceDelayMilliseconds,
        origin, page == firstPage ? 0 : secondStart, 0, QuietScanout::height};
}

VgaServiceReceipt line(const double origin, const std::uint32_t row,
    const std::uint16_t page) {
    const auto scheduled = origin + (row + 1) * linePeriodMilliseconds;
    return {VgaServiceKind::scanLine, row + 1, scheduled, scheduled + serviceDelayMilliseconds,
        origin, page == firstPage ? 0 : secondStart, row, QuietScanout::height};
}

std::vector<VgaServiceReceipt> lines(const double origin, const std::uint16_t page,
    const std::uint32_t begin, const std::uint32_t end) {
    std::vector<VgaServiceReceipt> result;
    for (auto row = begin; row < end; ++row) {
        result.push_back(line(origin, row, page));
    }
    return result;
}

void complete(QuietScanout& collector, const Fixture& fixture, const double origin,
    const std::uint16_t page) {
    collector.consume({start(origin, page)}, fixture.pages, fixture.palette);
    collector.consume(lines(origin, page, 0, QuietScanout::height), fixture.pages, fixture.palette);
}

void checkScan(const QuietScanout::CompletedScan& scan, const double origin,
    const std::uint16_t page) {
    assert(scan.originMilliseconds == origin && scan.page == page);
    const auto finalLine = line(origin, QuietScanout::height - 1, page);
    assert(scan.scheduledCompletionMilliseconds == finalLine.scheduledMilliseconds);
    assert(scan.completionMilliseconds == finalLine.servicedMilliseconds);
    assert(scan.rgb.size() == QuietScanout::frameBytes);

    // Literal BGR0 expectations also check six-bit replication and doubled source rows.
    constexpr std::array<std::array<std::uint8_t, 4>, 3> colors{{
        {{0, 0, 255, 0}}, {{0, 255, 0, 0}}, {{130, 69, 4, 0}}
    }};
    for (std::uint32_t y = 0; y < QuietScanout::height; ++y) {
        for (std::uint32_t x = 0; x < QuietScanout::width; ++x) {
            const auto& expected = colors[pixelIndex(page, x, y / 2) - 1];
            const auto offset = (y * QuietScanout::width + x) * expected.size();
            for (std::size_t component = 0; component < expected.size(); ++component) {
                assert(scan.rgb[offset + component] == expected[component]);
            }
        }
    }
}

bool rejects(const std::function<void()>& operation) {
    try {
        operation();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void drainContinuity() {
    Fixture fixture;
    QuietScanout collector;
    collector.consume({line(0, 194, firstPage), line(0, 195, firstPage)},
        fixture.pages, fixture.palette);
    assert(collector.discardedInitialLines() == 2);
    assert(collector.pendingLines() == 0 && collector.takeCompleted().empty());

    collector.consume({start(10, firstPage)}, fixture.pages, fixture.palette, secondPage);
    collector.consume(lines(10, firstPage, 0, splitLine), fixture.pages, fixture.palette, secondPage);
    assert(collector.pendingLines() == splitLine && collector.takeCompleted().empty());
    assert(collector.pendingLines() == splitLine && collector.discardedInitialLines() == 2);
    collector.consume(lines(10, firstPage, splitLine, QuietScanout::height),
        fixture.pages, fixture.palette, secondPage);
    assert(collector.pendingLines() == 0 && collector.completed().size() == 1);
    checkScan(collector.completed().front(), 10, firstPage);

    collector.consume({start(30, secondPage)}, fixture.pages, fixture.palette, firstPage);
    collector.consume(lines(30, secondPage, 0, splitLine), fixture.pages, fixture.palette, firstPage);
    auto first = collector.takeCompleted();
    assert(first.size() == 1 && collector.completed().empty());
    checkScan(first.front(), 10, firstPage);
    assert(collector.pendingLines() == splitLine && collector.discardedInitialLines() == 2);
    assert(collector.takeCompleted().empty() && collector.pendingLines() == splitLine);

    // Changed line-receipt realStart must not redirect the cursor latched at scanStart.
    collector.consume(lines(30, firstPage, splitLine, QuietScanout::height),
        fixture.pages, fixture.palette, firstPage);
    auto second = collector.takeCompleted();
    assert(second.size() == 1 && collector.pendingLines() == 0);
    checkScan(second.front(), 30, secondPage);

    complete(collector, fixture, 50, firstPage);
    complete(collector, fixture, 70, secondPage);
    const auto later = collector.takeCompleted();
    assert(later.size() == 2 && collector.completed().empty());
    checkScan(later[0], 50, firstPage);
    checkScan(later[1], 70, secondPage);
    assert(collector.takeCompleted().empty() && collector.discardedInitialLines() == 2);
    checkScan(first.front(), 10, firstPage);
    checkScan(second.front(), 30, secondPage);
}

void completedOwnership() {
    Fixture fixture;
    QuietScanout collector;
    complete(collector, fixture, 10, firstPage);
    const auto saved = collector.completed().front().rgb;
    for (std::uint32_t y = 0; y < logicalHeight; ++y) {
        for (std::uint32_t x = 0; x < QuietScanout::width; ++x) {
            fixture.pages.write(firstPage,
                {static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)}, 2);
        }
    }
    fixture.palette.dac.colors[3] = 0;
    assert(collector.completed().front().rgb == saved);
    const auto drained = collector.takeCompleted();
    assert(drained.size() == 1 && collector.completed().empty());
    checkScan(drained.front(), 10, firstPage);
    fixture.pages.write(firstPage, {0, 0}, 3);
    fixture.palette.dac.colors.fill(63);
    assert(drained.front().rgb == saved && collector.takeCompleted().empty());
    assert(rejects([&]() {
        collector.consume({line(20, 0, firstPage)}, fixture.pages, fixture.palette);
    }));
}

void rejectionControls() {
    Fixture fixture;
    QuietScanout unknown;
    unknown.consume({start(10, firstPage)}, fixture.pages, fixture.palette);
    assert(rejects([&]() {
        unknown.consume({line(10, 0, firstPage)}, IndexedPages{}, fixture.palette);
    }));

    QuietScanout writer;
    writer.consume({start(10, firstPage), line(10, 0, firstPage)}, fixture.pages, fixture.palette);
    assert(writer.takeCompleted().empty() && writer.pendingLines() == 1);
    assert(rejects([&]() { writer.consume({}, fixture.pages, fixture.palette, firstPage); }));
    writer.consume({line(10, 1, firstPage)}, fixture.pages, fixture.palette, secondPage);
    assert(writer.pendingLines() == 2);

    QuietScanout changedPalette;
    changedPalette.consume({start(10, firstPage), line(10, 0, firstPage)},
        fixture.pages, fixture.palette);
    assert(changedPalette.takeCompleted().empty());
    auto changed = fixture.palette;
    changed.dac.colors[3] = 0;
    assert(rejects([&]() {
        changedPalette.consume({line(10, 1, firstPage)}, fixture.pages, changed);
    }));

    QuietScanout missingLine;
    missingLine.consume({start(10, firstPage), line(10, 0, firstPage)}, fixture.pages, fixture.palette);
    assert(rejects([&]() {
        missingLine.consume({line(10, 2, firstPage)}, fixture.pages, fixture.palette);
    }));

    QuietScanout backwards;
    auto delayed = line(10, 0, firstPage);
    delayed.servicedMilliseconds = 11;
    backwards.consume({start(10, firstPage), delayed}, fixture.pages, fixture.palette);
    assert(backwards.takeCompleted().empty());
    assert(rejects([&]() {
        backwards.consume({line(10, 1, firstPage)}, fixture.pages, fixture.palette);
    }));

    QuietScanout unsupported;
    auto badStart = start(10, firstPage);
    badStart.realStart = 0x8000;
    assert(rejects([&]() { unsupported.consume({badStart}, fixture.pages, fixture.palette); }));
}
}

int main() {
    // Synthetic projection/drain mechanics; no original CPU, IRQ or RGB qualification.
    drainContinuity();
    completedOwnership();
    rejectionControls();
}
