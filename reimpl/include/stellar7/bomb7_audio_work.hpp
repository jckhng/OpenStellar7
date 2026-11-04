#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace stellar7 {
inline constexpr std::size_t bomb7CallerRowBytes = 64;
inline constexpr std::size_t bomb7MenuPayloadOffset = 0x2033;
inline constexpr std::uint16_t bomb7CallerRowAddress = 0x9203;

class SoundBlasterBomb7CallerRow {
public:
    const std::array<std::uint8_t, bomb7CallerRowBytes>& values() const noexcept { return *storage_; }
    friend bool operator==(const SoundBlasterBomb7CallerRow& a, const SoundBlasterBomb7CallerRow& b) noexcept {
        return a.storage_ == b.storage_;
    }
    friend bool operator!=(const SoundBlasterBomb7CallerRow& a, const SoundBlasterBomb7CallerRow& b) noexcept {
        return !(a == b);
    }

private:
    friend class NativeAudioOwner;
    friend struct Bomb7AudioTestAccess;
    explicit SoundBlasterBomb7CallerRow(std::array<std::uint8_t, bomb7CallerRowBytes> values)
        : storage_(std::make_shared<const std::array<std::uint8_t, bomb7CallerRowBytes>>(values)) {}
    static std::optional<SoundBlasterBomb7CallerRow> fromMenuSong(const std::vector<std::uint8_t>& payload);
    std::shared_ptr<const std::array<std::uint8_t, bomb7CallerRowBytes>> storage_;
};
} // namespace stellar7
