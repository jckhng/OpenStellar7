#pragma once

#include "stellar7/indexed_pages.hpp"
#include "stellar7/normal_clock.hpp"
#include "stellar7/palette_clock.hpp"

#include <cmath>
#include <optional>
#include <stdexcept>
#include <vector>
#include <utility>

namespace stellar7 {

// Quiet-profile projection only; consumes owned service receipts without advancing time or writing VRAM.
class QuietScanout {
public:
    static constexpr std::uint32_t width = 320;
    static constexpr std::uint32_t height = 400;
    static constexpr std::size_t frameBytes = width * height * 4;

    struct CompletedScan {
        double originMilliseconds;
        double completionMilliseconds;
        double scheduledCompletionMilliseconds;
        std::uint16_t page;
        std::vector<std::uint8_t> rgb;
    };

    void consume(const std::vector<stellar7::VgaServiceReceipt>& receipts,
        const stellar7::IndexedPages& pages, const stellar7::PaletteWorkState& palette,
        const std::optional<std::uint16_t> writingPage = std::nullopt) {
        rejectWriter(writingPage);
        for (const auto& receipt : receipts) {
            if (!std::isfinite(receipt.scheduledMilliseconds) || !std::isfinite(receipt.servicedMilliseconds)
                || receipt.servicedMilliseconds < receipt.scheduledMilliseconds
                || receipt.servicedMilliseconds < lastServiceMilliseconds_ || receipt.linesTotal != height) {
                throw std::invalid_argument("unsupported quiet scan service order/profile");
            }
            lastServiceMilliseconds_ = receipt.servicedMilliseconds;
            if (receipt.kind == stellar7::VgaServiceKind::scanStart) {
                if (active_ || receipt.line != 0 || receipt.frameStartMilliseconds != receipt.scheduledMilliseconds
                    || (receipt.realStart != 0 && receipt.realStart != 0x4000)) {
                    throw std::invalid_argument("unsupported quiet scan start");
                }
                page_ = receipt.realStart == 0 ? 0xa000 : 0xa400;
                originMilliseconds_ = receipt.frameStartMilliseconds;
                colors_ = palette.dac.colors;
                line_ = 0;
                rgb_.clear();
                rgb_.reserve(frameBytes);
                active_ = true;
                started_ = true;
                rejectWriter(writingPage);
                continue;
            }
            if (receipt.kind != stellar7::VgaServiceKind::scanLine) {
                throw std::invalid_argument("unsupported quiet scan service kind");
            }
            if (!active_) {
                if (started_) {
                    throw std::invalid_argument("quiet scan line without retained start");
                }
                ++discardedInitialLines_;
                continue;
            }
            if (receipt.line != line_ || receipt.frameStartMilliseconds != originMilliseconds_
                || palette.dac.colors != colors_) {
                throw std::invalid_argument("quiet scan discontinuity or palette writer");
            }
            for (std::int16_t x = 0; x < width; ++x) {
                const stellar7::ProjectedPoint point{x, static_cast<std::int16_t>(line_ / 2)};
                if (!pages.known(page_, point)) {
                    throw std::invalid_argument("quiet scan reads unknown physical pixel");
                }
                const auto index = pages.read(page_, point) * stellar7::vgaPaletteComponents;
                const auto expand = [](const std::uint8_t component) {
                    const auto six = component & 63;
                    return static_cast<std::uint8_t>((six << 2) | (six >> 4));
                };
                rgb_.push_back(expand(colors_[index + 2]));
                rgb_.push_back(expand(colors_[index + 1]));
                rgb_.push_back(expand(colors_[index]));
                rgb_.push_back(0);
            }
            if (++line_ == height) {
                completed_.push_back({originMilliseconds_, receipt.servicedMilliseconds,
                    receipt.scheduledMilliseconds, page_, std::move(rgb_)});
                active_ = false;
            }
        }
    }

    const std::vector<CompletedScan>& completed() const { return completed_; }
    std::vector<CompletedScan> takeCompleted() { return std::exchange(completed_, {}); }
    std::uint32_t pendingLines() const { return active_ ? line_ : 0; }
    std::uint64_t discardedInitialLines() const { return discardedInitialLines_; }

private:
    void rejectWriter(const std::optional<std::uint16_t> writingPage) const {
        if (active_ && writingPage == page_) {
            throw std::invalid_argument("quiet scan overlaps displayed-page writer");
        }
    }

    bool active_ = false;
    bool started_ = false;
    std::uint16_t page_ = 0;
    std::uint32_t line_ = 0;
    double originMilliseconds_ = 0;
    double lastServiceMilliseconds_ = 0;
    std::uint64_t discardedInitialLines_ = 0;
    std::array<std::uint8_t, stellar7::vgaPaletteBytes> colors_{};
    std::vector<std::uint8_t> rgb_;
    std::vector<CompletedScan> completed_;
};

}
