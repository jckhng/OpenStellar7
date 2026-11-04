#include "stellar7/cockpit_state.hpp"

#include <fstream>
#include <algorithm>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::size_t settingsBytes = 14;
constexpr std::int16_t maximumViewSize = 82;
constexpr char settingsFilename[] = "FAME.HAL";
constexpr std::uint32_t hallOfFameScoreMask = 0xa1b2c3d4;
constexpr std::size_t hallOfFameNameBytes = 20;
}

CockpitSettings decodeCockpitSettings(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() < settingsBytes) {
        throw std::runtime_error("truncated cockpit settings record");
    }
    const auto word = [&bytes](const std::size_t offset) {
        return static_cast<std::int16_t>(static_cast<std::uint16_t>(
            bytes[offset] | (static_cast<std::uint16_t>(bytes[offset + 1]) << 8U)));
    };
    CockpitSettings settings{};
    settings.viewSize = word(0);
    if (settings.viewSize < 0 || settings.viewSize > maximumViewSize) {
        throw std::runtime_error("unsupported cockpit view-size setting");
    }
    for (std::size_t index = 0; index < settings.options.size(); ++index) {
        settings.options[index] = word(2 * (index + 1));
    }
    return settings;
}

CockpitSettings loadCockpitSettings(const std::filesystem::path& dataDirectory) {
    const auto path = dataDirectory / settingsFilename;
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open cockpit settings: " + path.string());
    }
    std::vector<std::uint8_t> bytes(settingsBytes);
    if (!input.read(reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()))) {
        throw std::runtime_error("truncated cockpit settings: " + path.string());
    }
    return decodeCockpitSettings(bytes);
}

std::optional<HallOfFameScores> decodeHallOfFameScores(const std::vector<std::uint8_t>& bytes) {
    HallOfFameScores scores{};
    auto position = settingsBytes;
    for (auto& score : scores) {
        if (position > bytes.size() || bytes.size() - position < sizeof(score)) {
            return std::nullopt;
        }
        for (std::size_t index = 0; index < sizeof(score); ++index) {
            score |= static_cast<std::uint32_t>(bytes[position + index]) << (index * 8);
        }
        score ^= hallOfFameScoreMask;
        if (score > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
            return std::nullopt;
        }
        position += sizeof(score);
        const auto end = std::find(bytes.begin() + position, bytes.end(), 0);
        if (end == bytes.end() || static_cast<std::size_t>(end - bytes.begin() - position) >= hallOfFameNameBytes) {
            return std::nullopt;
        }
        position = static_cast<std::size_t>(end - bytes.begin()) + 1;
    }
    return position == bytes.size() ? std::optional<HallOfFameScores>{scores} : std::nullopt;
}

std::optional<HallOfFameScores> loadHallOfFameScores(const std::filesystem::path& dataDirectory) {
    std::ifstream input(dataDirectory / settingsFilename, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }
    return decodeHallOfFameScores({std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()});
}

void CockpitPageState::beginFrame() noexcept {
    drawPage_ = backPage_;
}

CockpitPageState CockpitPageState::fromRecoveredMenu() noexcept {
    CockpitPageState result;
    // The recovered allocation order is FRONT A000, CACHE A400, BACK A800.
    result.backPage_ = initialFrontPage + 2 * pageSpacing;
    result.cachePage_ = initialFrontPage + pageSpacing;
    result.drawPage_ = result.backPage_;
    result.copyPage_ = result.backPage_;
    return result;
}

void CockpitPageState::flip() noexcept {
    std::swap(frontPage_, backPage_);
}

void CockpitPageState::selectCockpitCopy() noexcept {
    copyPage_ = cachePage_;
}

void CockpitPageState::selectCockpitDraw() noexcept {
    drawPage_ = cachePage_;
}

void CockpitPageState::selectBackCopy() noexcept {
    copyPage_ = backPage_;
}

void CockpitPageState::selectScoreCopy() noexcept {
    drawPage_ = backPage_;
    copyPage_ = frontPage_;
}

void CockpitPageState::selectArmorCopy() noexcept {
    copyPage_ = backPage_;
    drawPage_ = frontPage_;
}

void CockpitPageState::selectFront() noexcept {
    drawPage_ = frontPage_;
}

void CockpitPageState::beginModal() noexcept {
    drawPage_ = backPage_;
    copyPage_ = backPage_;
}

bool CockpitPageState::cachedPageDiffersFromDraw() const noexcept {
    return cachePage_ != drawPage_;
}

}
