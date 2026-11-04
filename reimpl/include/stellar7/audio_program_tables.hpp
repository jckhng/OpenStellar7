#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace stellar7 {
namespace audio_program_detail {
inline constexpr std::size_t frequencyEntryCount = 509;
inline constexpr std::size_t quarterStepsPerOctave = 48;
inline constexpr std::size_t zeroBlockEntries = 96;
// Original driver0509: all509 words repeat this exact48-entry period.
inline constexpr std::array<std::uint16_t, quarterStepsPerOctave> frequencyPeriod{{
    343, 348, 353, 358, 363, 369, 374, 379,
    385, 390, 396, 402, 408, 414, 420, 426,
    432, 438, 445, 451, 458, 464, 471, 478,
    485, 492, 499, 506, 514, 521, 529, 536,
    544, 552, 560, 568, 577, 585, 594, 602,
    611, 620, 629, 638, 647, 656, 666, 676,
}};
}

inline std::uint16_t soundBlasterProgramFrequency(const std::size_t index) {
    if (index >= audio_program_detail::frequencyEntryCount) {
        throw std::invalid_argument("Sound Blaster program frequency index outside source table");
    }
    return audio_program_detail::frequencyPeriod[index % audio_program_detail::quarterStepsPerOctave];
}

inline std::uint8_t soundBlasterProgramBlock(const std::size_t index) {
    if (index >= audio_program_detail::frequencyEntryCount) {
        throw std::invalid_argument("Sound Blaster program block index outside source table");
    }
    // Original030C remains zero for two periods, then increments every48 entries.
    return index < audio_program_detail::zeroBlockEntries ? 0 : static_cast<std::uint8_t>(
        index / audio_program_detail::quarterStepsPerOctave - 1);
}

}  // namespace stellar7
