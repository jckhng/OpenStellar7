#pragma once

#include "stellar7/normal_clock.hpp"

#include <array>
#include <cstddef>
#include <optional>

namespace stellar7 {

inline constexpr std::size_t vgaPaletteColors = 256;
inline constexpr std::size_t vgaPaletteComponents = 3;
inline constexpr std::size_t vgaPaletteBytes = vgaPaletteColors * vgaPaletteComponents;
inline constexpr std::uint8_t vgaComponentMaximum = 63;

enum class PaletteDacMode : std::uint8_t { write = 0, read = 3 };
enum class PaletteDacProfile { sixBitNonParadiseFullTriplet };

struct PaletteDacState {
    std::array<std::uint8_t, vgaPaletteBytes> colors{};
    PaletteDacMode mode = PaletteDacMode::write;
    PaletteDacProfile profile = PaletteDacProfile::sixBitNonParadiseFullTriplet;
    std::uint16_t writeIndex = 0;
    std::uint16_t readIndex = 0;
    std::uint8_t pelIndex = 0;
    std::optional<std::array<std::uint8_t, vgaPaletteComponents>> latch;
};

struct PaletteWorkState {
    std::array<std::uint8_t, vgaPaletteBytes> source{};
    std::array<std::uint8_t, vgaPaletteBytes> output{};
    PaletteDacState dac;
};

struct PaletteRequest {
    std::uint16_t start;
    std::uint16_t count;
    std::uint16_t targetColor;
    std::uint8_t intensity;
};

struct PalettePortWrite {
    std::int64_t cycleOffset;
    std::uint16_t port;
    std::uint8_t value;
};

struct PaletteColorCommit {
    std::int64_t cycleOffset;
    std::uint16_t index;
    std::array<std::uint8_t, vgaPaletteComponents> color;
};

struct PaletteClockTiming {
    NormalWorkTiming timing;
    PaletteWorkState state;
    std::uint64_t highReads = 0;
    std::int64_t waitEntryCycles = 0;
    std::int64_t uploadStartCycles = 0;
    std::int64_t uploadEndCycles = 0;
    std::vector<PalettePortWrite> writes;
    std::vector<PaletteColorCommit> commits;
};

// Loaded3568:077a, including its068a upload, through RETF. Resident1b38 excluded.
// Normal Mode-X clock; high-only retrace wait, six-bit triplet DAC/access history.
// Unknown initial latch is allowed only when overwritten before a color commit.
PaletteClockTiming paletteClockTiming(const SoundBlasterClockPhase& phase,
    const PaletteWorkState& state, const PaletteRequest& request, std::int64_t horizonCycles);

// The same immutable source-bank arithmetic used by the playable endpoint.
PaletteWorkState transformPalette(const PaletteWorkState& state, const PaletteRequest& request);

// Completed full-upload effects only; CPU/port timing and entry ancestry are separate.
PaletteWorkState completedFullPaletteUpload(const PaletteWorkState& state, const PaletteRequest& request);

}
