#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace stellar7 {
struct BitmapCache;
struct IndexedBitmap;
inline constexpr std::uint16_t enemy20McbIndex=35;
inline constexpr std::uint16_t enemy20McbAddress=0x5426;
class SoundBlasterEnemy20McbByte {
public:
    std::uint8_t value() const noexcept;
    std::optional<std::uint8_t> nativeProcessByte(std::uint16_t index) const noexcept;
    bool isNativeIdentityOf(const SoundBlasterEnemy20McbByte& reserved) const noexcept;
    friend bool operator==(const SoundBlasterEnemy20McbByte& a,const SoundBlasterEnemy20McbByte& b) noexcept {
        return a.storage_==b.storage_;
    }
    friend bool operator!=(const SoundBlasterEnemy20McbByte& a,const SoundBlasterEnemy20McbByte& b) noexcept {
        return !(a==b);
    }
private:
    friend class NativeGameplaySession;
    friend struct Enemy20McbTestAccess;
    enum class DosLoadPlacement { afterPsp, high };
    struct NativeDosInitialLoadProfile {
        std::uint16_t imageSegment;
        DosLoadPlacement placement;
    };
    static constexpr std::size_t reservedByteCount = 3;
    static constexpr std::size_t retiredTailByteCount = 8;
    struct Storage;
    SoundBlasterEnemy20McbByte(std::array<std::uint8_t,reservedByteCount> values,
        std::array<std::uint8_t,retiredTailByteCount> retiredTail);
    SoundBlasterEnemy20McbByte withNativeProcessIdentity(std::string_view executable,
        const std::vector<std::uint8_t>& ownedSsm0,const IndexedBitmap& recentBitmap,
        NativeDosInitialLoadProfile initialLoad) const;
    // Only the retained startup owner can mint a production allocation generation.
    static std::optional<SoundBlasterEnemy20McbByte> fromCache(const BitmapCache& cache);
    std::shared_ptr<const Storage> storage_;
};
inline constexpr std::uint16_t enemy20CallerIndexBegin=45;
inline constexpr std::size_t enemy20CallerWindowBytes=19;
struct SoundBlasterEnemy20CallerWindow {
    std::array<std::uint8_t,enemy20CallerWindowBytes> payloadPrefix;
};
inline bool operator==(const SoundBlasterEnemy20CallerWindow& a,const SoundBlasterEnemy20CallerWindow& b) noexcept {
    return a.payloadPrefix==b.payloadPrefix;
}
inline bool operator!=(const SoundBlasterEnemy20CallerWindow& a,const SoundBlasterEnemy20CallerWindow& b) noexcept {
    return !(a==b);
}
//Readonly provider projection of the retained COCKPIT SSM0 payload, not a song.
SoundBlasterEnemy20CallerWindow level1Enemy20CallerWindow(const std::vector<std::uint8_t>& ownedSsm0);
class SoundBlasterSynth;
struct SoundBlasterClockPhase;
//After the actual Level1 music/reset/resource provider; no dispatch or time.
SoundBlasterClockPhase bindLevel1Enemy20Audio(SoundBlasterSynth& synth,const SoundBlasterClockPhase& phase);
SoundBlasterClockPhase bindLevel1Enemy20McbAudio(SoundBlasterSynth& synth,const SoundBlasterClockPhase& phase,
    const SoundBlasterEnemy20McbByte& byte);
}
