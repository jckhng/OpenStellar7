#include "stellar7/weapon_lamp_work.hpp"
#include "stellar7/render_work.hpp"

#include <cassert>
#include <stdexcept>

namespace {
template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
}

int main() {
    using namespace stellar7;
    IndexedBitmap bitmap{24,5,std::vector<std::uint8_t>(120)};
    for (std::size_t index = 0; index < bitmap.pixels.size(); ++index) {
        bitmap.pixels[index] = index % 3 == 0 ? 7 : 0;
    }
    auto pages = CockpitPageState::fromRecoveredMenu();
    for (unsigned display = 0; display < 2; ++display) {
        const auto work = weaponLampOperations(0xffff,WeaponLampState::unavailable,bitmap,pages);
        assert(work.dirty == 0 && work.pages.drawPage() == pages.frontPage());
        assert(work.pages.copyPage() == pages.backPage() && work.pages.cachePage() == pages.cachePage());
        const auto& copy = std::get<RadarPageCopy>(work.effects.back());
        assert(copy.sourcePage == pages.backPage() && copy.destinationPage == pages.frontPage());
        assert(copy.position.x == 240 && copy.position.y == 170 && copy.coveredWidth == 32 && copy.height == 10);
        IndexedPages pixels;
        for (std::int16_t y = 170; y < 180; ++y) {
            for (std::int16_t x = 240; x < 272; ++x) {
                pixels.write(pages.backPage(),{x,y},3);
                pixels.write(pages.frontPage(),{x,y},5);
                pixels.write(pages.cachePage(),{x,y},11);
            }
        }
        pixels.apply(work.effects);
        for (std::int16_t y = 170; y < 180; ++y) {
            for (std::int16_t x = 240; x < 272; ++x) {
                const auto drawn = x >= 243 && x < 267 && y < 175 && bitmap.pixels[(y-170)*24+x-243] != 0;
                const auto expected = drawn ? 7 : 3;
                assert(pixels.read(pages.backPage(),{x,y}) == expected);
                assert(pixels.read(pages.frontPage(),{x,y}) == expected);
                assert(pixels.read(pages.cachePage(),{x,y}) == 11);
            }
        }
        const auto repeated = weaponLampOperations(work.dirty,WeaponLampState::unavailable,{},work.pages);
        assert(repeated.effects.empty() && repeated.pages.drawPage() == work.pages.drawPage());
        assert(repeated.operations.size() == 2 && repeated.operations[0].count + repeated.operations[1].count == 10);
        const auto ready = weaponLampOperations(work.dirty,WeaponLampState::ready,bitmap,work.pages);
        assert(ready.dirty == 1 && ready.effects.size() == work.effects.size());
        pages.flip();
    }
    rejects([&] { weaponLampOperations(2,WeaponLampState::ready,bitmap,pages); });
    rejects([&] { weaponLampOperations(0xffff,static_cast<WeaponLampState>(2),bitmap,pages); });
    rejects([&] { weaponLampOperations(0xffff,WeaponLampState::ready,{},pages); });
    rejects([&] { weaponLampOperations(0xffff,WeaponLampState::ready,bitmap,CockpitPageState{}); });
}
