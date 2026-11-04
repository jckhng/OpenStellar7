#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>

#include "stellar7/gameplay.hpp"

namespace stellar7 {
enum class NativeDetailCapability { unavailable, bounded51To81, fullRange };
inline constexpr std::int16_t nativeDetailMinimum=51;
inline constexpr std::int16_t nativeDetailPanoramaThreshold=20;
inline constexpr std::int16_t nativeDetailIndicatorThreshold=50;
inline constexpr std::int16_t nativeDetailOverlayThreshold=70;
inline constexpr std::int16_t nativeDetailInteractiveMaximum=81;
inline constexpr std::int16_t nativeDetailLoadedMaximum=82;
constexpr bool nativeDetailSettingSupported(const std::int16_t value) noexcept {
    return value>=nativeDetailMinimum && value<=nativeDetailLoadedMaximum;
}
constexpr bool nativeFullDetailSettingSupported(const std::int16_t value) noexcept {
    return value>=0 && value<=nativeDetailLoadedMaximum;
}
//8AFC table uses byte offsets and the input before its lower scale clamp.
inline PlayableModelDetail nativeModelDetail(const std::int16_t detail) {
    if (!nativeFullDetailSettingSupported(detail)) {
        throw std::invalid_argument("unsupported native model detail");
    }
    constexpr std::array<std::uint16_t, 9> caps{9, 12, 17, 28, 32, 42, 52, 90, 128};
    constexpr std::uint16_t numerator = 256, divisor = 82, maximumScale = 255, minimumScale = 64;
    const auto input = std::min<std::uint16_t>(detail * numerator / divisor, maximumScale);
    const auto byteOffset = ((input + 1) & 0xffefU) >> 4;
    return {std::max(input, minimumScale), caps[byteOffset / sizeof(std::uint16_t)]};
}
} // namespace stellar7
