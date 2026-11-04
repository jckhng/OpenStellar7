#include "stellar7/palette_clock.hpp"

#include <algorithm>
#include <stdexcept>

namespace stellar7 {

PaletteWorkState transformPalette(const PaletteWorkState& state, const PaletteRequest& request) {
    const auto validBank = [](const auto& bank) {
        return std::all_of(bank.begin(), bank.end(), [](const std::uint8_t value) {
            return value <= vgaComponentMaximum;
        });
    };
    if (request.count == 0 || request.start >= vgaPaletteColors
        || request.count > vgaPaletteColors - request.start
        || request.targetColor >= vgaPaletteColors || request.intensity > vgaComponentMaximum
        || !validBank(state.source) || !validBank(state.output) || !validBank(state.dac.colors)
        || (state.dac.mode != PaletteDacMode::write && state.dac.mode != PaletteDacMode::read)
        || state.dac.profile != PaletteDacProfile::sixBitNonParadiseFullTriplet
        || state.dac.writeIndex >= vgaPaletteColors || state.dac.readIndex >= vgaPaletteColors
        || state.dac.pelIndex >= vgaPaletteComponents
        || (state.dac.latch && !validBank(*state.dac.latch))) {
        throw std::invalid_argument("unsupported source palette bank/request/DAC state");
    }
    auto result = state;
    const auto begin = request.start * vgaPaletteComponents;
    const auto end = begin + request.count * vgaPaletteComponents;
    for (std::size_t i = begin; i < end; ++i) {
        const auto target = request.targetColor * vgaPaletteComponents + i % vgaPaletteComponents;
        result.output[i] = static_cast<std::uint8_t>(
            state.source[i] * request.intensity / vgaComponentMaximum
            + state.source[target] * (vgaComponentMaximum - request.intensity) / vgaComponentMaximum);
    }
    return result;
}

PaletteWorkState completedFullPaletteUpload(const PaletteWorkState& state, const PaletteRequest& request) {
    if (request.start != 0 || request.count != vgaPaletteColors) {
        throw std::invalid_argument("completed palette projection requires a full upload");
    }
    auto result = transformPalette(state, request);
    result.dac.colors = result.output;
    result.dac.mode = PaletteDacMode::write;
    result.dac.writeIndex = 0;
    result.dac.readIndex = vgaPaletteColors - 1;
    result.dac.pelIndex = 0;
    std::array<std::uint8_t, vgaPaletteComponents> latch{};
    std::copy(result.output.end() - vgaPaletteComponents, result.output.end(), latch.begin());
    result.dac.latch = latch;
    return result;
}

}
