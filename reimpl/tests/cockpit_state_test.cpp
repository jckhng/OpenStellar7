#include "stellar7/cockpit_state.hpp"

#include <cassert>
#include <stdexcept>

namespace {
template<class Action>
void rejects(Action action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}
}

int main(int argc, char** argv) {
    using namespace stellar7;
    std::vector<std::uint8_t> bytes{
        81, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 255, 0,
    };
    const auto settings = decodeCockpitSettings(bytes);
    assert(settings.viewSize == 81);
    assert((settings.options == std::array<std::int16_t, 6>{1, 0, 1, 1, 0, 255}));
    bytes.push_back(0xff);
    assert(decodeCockpitSettings(bytes).viewSize == 81);
    bytes.resize(13);
    rejects([&] { decodeCockpitSettings(bytes); });
    bytes.resize(14);
    bytes[0] = 83;
    rejects([&] { decodeCockpitSettings(bytes); });
    bytes[0] = 0xff;
    bytes[1] = 0xff;
    rejects([&] { decodeCockpitSettings(bytes); });
    bytes[0] = 0;
    bytes[1] = 0;
    assert(decodeCockpitSettings(bytes).viewSize == 0);
    bytes[0] = 82;
    assert(decodeCockpitSettings(bytes).viewSize == 82);
    const auto missing = std::filesystem::temp_directory_path()
        / "stellar7-cockpit-missing-settings-test";
    assert(!std::filesystem::exists(missing));
    rejects([&] { loadCockpitSettings(missing); });
    assert(!loadHallOfFameScores(missing));
    std::vector<std::uint8_t> hall(14);
    HallOfFameScores expectedScores{};
    for (std::size_t index = 0; index < expectedScores.size(); ++index) {
        expectedScores[index] = static_cast<std::uint32_t>(2000 - index * 100);
        const auto encoded = expectedScores[index] ^ 0xa1b2c3d4;
        for (std::size_t byte = 0; byte < sizeof(encoded); ++byte) {
            hall.push_back(static_cast<std::uint8_t>(encoded >> (byte * 8)));
        }
        hall.insert(hall.end(), index + 1, 0x41);
        hall.push_back(0);
    }
    assert(decodeHallOfFameScores(hall) == expectedScores);
    auto truncated = hall;
    truncated.pop_back();
    assert(!decodeHallOfFameScores(truncated));
    hall.push_back(0);
    assert(!decodeHallOfFameScores(hall));
    assert(!decodeHallOfFameScores(std::vector<std::uint8_t>(14)));
    if (argc == 2) {
        assert(loadCockpitSettings(argv[1]).viewSize == 81);
        assert((loadHallOfFameScores(argv[1]) == HallOfFameScores{
            5155, 4815, 4470, 3960, 2770, 2100, 1920, 1870, 1820, 1800}));
    }

    CockpitPageState pages;
    assert(pages.cachePage() == 0xa800);
    assert(pages.frontPage() == 0xa000);
    assert(pages.backPage() == 0xa400);
    assert(pages.drawPage() == pages.backPage());
    assert(pages.copyPage() == pages.frontPage());
    assert(pages.cachedPageDiffersFromDraw());
    pages.selectCockpitDraw();
    assert(pages.drawPage() == pages.cachePage());
    assert(!pages.cachedPageDiffersFromDraw());
    pages.selectArmorCopy();
    assert(pages.drawPage() == pages.frontPage());
    assert(pages.copyPage() == pages.backPage());
    assert(pages.frontPage() == 0xa000 && pages.backPage() == 0xa400);
    pages.beginFrame();
    assert(pages.drawPage() == pages.backPage());
    pages.selectCockpitCopy();
    assert(pages.copyPage() == pages.cachePage());
    pages.flip();
    assert(pages.frontPage() == 0xa400);
    assert(pages.backPage() == 0xa000);
    assert(pages.drawPage() == 0xa400);
    assert(pages.copyPage() == pages.cachePage());
    pages.beginFrame();
    assert(pages.drawPage() == pages.backPage());
    assert(pages.cachedPageDiffersFromDraw());
    pages.flip();
    pages.beginFrame();
    assert(pages.frontPage() == 0xa000);
    assert(pages.backPage() == 0xa400);
    assert(pages.cachePage() == 0xa800);
    assert(pages.copyPage() == pages.cachePage());
    assert(pages.cachedPageDiffersFromDraw());
}
