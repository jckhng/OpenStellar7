#include "playable_system_pages.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
using namespace stellar7;
constexpr std::uint16_t back = 0xa800, front = 0xa000, cache = 0xa400;
void require(const bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
struct ExpectedPages {
    std::vector<std::uint8_t> backPixels = std::vector<std::uint8_t>(64000, 40);
    std::vector<std::uint8_t> frontPixels = std::vector<std::uint8_t>(64000, 30);
    void fill(const int x, const int y, const int width, const int height, const std::uint8_t color) {
        for (int row = y; row < y + height; ++row) {
            for (int column = x; column < x + width; ++column) { backPixels[row * 320 + column] = color; }
        }
    }
    void bitmap(const IndexedBitmap& image, const int x, const int y) {
        for (int row = 0; row < image.height; ++row) {
            for (int column = 0; column < image.width; ++column) {
                const auto color = image.pixels[row * image.width + column];
                if (color != 0) { backPixels[(y + row) * 320 + x + column] = color; }
            }
        }
    }
    void jumpBorder(const std::uint8_t color) {
        fill(35, 151, 26, 1, color); fill(35, 183, 26, 1, color);
        fill(35, 151, 1, 33, color); fill(60, 151, 1, 33, color);
    }
    void copyJump() {
        //106A includes alignment twice; x35,width26 covers x32..63.
        for (int y = 151; y <= 183; ++y) {
            for (int x = 32; x <= 63; ++x) { frontPixels[y * 320 + x] = backPixels[y * 320 + x]; }
        }
    }
    void passiveBorder() {
        fill(60, 151, 30, 1, 234); fill(60, 167, 30, 1, 234);
        fill(60, 151, 1, 17, 234); fill(89, 151, 1, 17, 234);
    }
    void copyPassive() {
        for (int y = 151; y <= 167; ++y) {
            for (int x = 60; x <= 91; ++x) { frontPixels[y * 320 + x] = backPixels[y * 320 + x]; }
        }
    }
    void jumpPanel(const std::vector<IndexedBitmap>& art, const SystemPanelStyle style, const unsigned charges) {
        const auto color = style == SystemPanelStyle::active ? 176 : style == SystemPanelStyle::ready ? 137 : 146;
        fill(37, 153, 22, 29, static_cast<std::uint8_t>(color));
        bitmap(art.at(style == SystemPanelStyle::unavailable && charges == 0 ? 52 : 43), 42, 155);
        for (unsigned pip = 0; pip < 3; ++pip) { bitmap(art.at(charges > pip ? 49 : 48), 42 + 4 * pip, 178); }
        copyJump();
    }
};

void test(const std::vector<IndexedBitmap>& art) {
    IndexedPages pages;
    for (std::int16_t y = 0; y < 200; ++y) {
        for (std::int16_t x = 0; x < 320; ++x) {
            pages.write(back, {x, y}, 40); pages.write(front, {x, y}, 30); pages.write(cache, {x, y}, 20);
        }
    }
    ExpectedPages expected;
    const auto apply = [&](const PlayableSystemEffect& effect) {
        pages.apply(playableSystemPageEffects(effect, art, back, front, CockpitPageLayout::recoveredStartup));
    };
    const auto compare = [&]() {
        require(pages.snapshot(back) == expected.backPixels && pages.snapshot(front) == expected.frontPixels,
            "Jump panel differs from independent source-coordinate pixels/copies");
        require(pages.snapshot(cache) == std::vector<std::uint8_t>(64000, 20), "Jump HUD changed retained cache");
    };
    apply(SystemBorder{2, true}); expected.jumpBorder(234);
    apply(SystemCopy{2}); expected.copyJump(); compare();
    for (const auto charges : {2U, 0U}) {
        apply(SystemPanel{2, SystemPanelStyle::active, static_cast<std::uint8_t>(charges)});
        expected.jumpPanel(art, SystemPanelStyle::active, charges); compare();
    }
    apply(SystemPanel{2, SystemPanelStyle::unavailable, 0});
    expected.jumpPanel(art, SystemPanelStyle::unavailable, 0); compare();
    apply(SystemBorder{2, false}); expected.jumpBorder(148);
    apply(SystemAudio{16});
    apply(SystemBorder{3, true}); expected.passiveBorder();
    apply(SystemCopy{3}); expected.copyPassive();
    apply(SystemCopy{2}); expected.copyJump(); compare();
    PlayableJumpState systems{{3, 3, 0, 3, 3, 3, 3}, {}, 2, 0};
    bool rejected = false;
    try { (void)statusPageEffects(systems.statuses, systems.timers, systems.selectedSystem, art,
        back, front, CockpitPageLayout::recoveredStartup); }
    catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "strict selected-panel guard changed");
    std::cout << "Declared Jump panels: selected234/active176/zeroactive/landing146/passive selection/physical copies/cache PASS\n";
}
} //namespace
int main(int argc, char** argv) {
    if (argc != 2) { return 2; }
    try {
        std::ifstream file(std::filesystem::path(argv[1]) / "STELART.RES", std::ios::binary);
        require(static_cast<bool>(file), "missing actual cockpit art");
        const auto art = decodeBitmapGroups({std::istreambuf_iterator<char>(file), {}});
        test(art.at(4));
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
