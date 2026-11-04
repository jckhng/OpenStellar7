#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace stellar7 {

struct CockpitSettings {
    std::int16_t viewSize;
    std::array<std::int16_t, 6> options;
};

CockpitSettings decodeCockpitSettings(const std::vector<std::uint8_t>& bytes);
CockpitSettings loadCockpitSettings(const std::filesystem::path& dataDirectory);
inline constexpr std::size_t hallOfFameSize = 10;
using HallOfFameScores = std::array<std::uint32_t, hallOfFameSize>;
std::optional<HallOfFameScores> decodeHallOfFameScores(const std::vector<std::uint8_t>& bytes);
std::optional<HallOfFameScores> loadHallOfFameScores(const std::filesystem::path& dataDirectory);

// Native Mode-X initialization and retained roles; not a startup phase seed.
class CockpitPageState {
public:
    static CockpitPageState fromRecoveredMenu() noexcept;
    void beginFrame() noexcept;
    void flip() noexcept;
    void selectCockpitCopy() noexcept;
    void selectCockpitDraw() noexcept;
    void selectArmorCopy() noexcept;
    void selectScoreCopy() noexcept;
    void selectBackCopy() noexcept;
    void selectFront() noexcept;
    void beginModal() noexcept;
    bool cachedPageDiffersFromDraw() const noexcept;
    std::uint16_t cachePage() const noexcept { return cachePage_; }
    std::uint16_t frontPage() const noexcept { return frontPage_; }
    std::uint16_t backPage() const noexcept { return backPage_; }
    std::uint16_t drawPage() const noexcept { return drawPage_; }
    std::uint16_t copyPage() const noexcept { return copyPage_; }

private:
    static constexpr std::uint16_t initialFrontPage = 0xa000;
    static constexpr std::uint16_t pageSpacing = 0x400;
    std::uint16_t frontPage_ = initialFrontPage;
    std::uint16_t backPage_ = initialFrontPage + pageSpacing;
    std::uint16_t cachePage_ = initialFrontPage + 2 * pageSpacing;
    std::uint16_t drawPage_ = backPage_;
    std::uint16_t copyPage_ = frontPage_;
};

}
