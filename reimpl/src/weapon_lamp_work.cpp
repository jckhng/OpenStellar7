#include "stellar7/weapon_lamp_work.hpp"
#include "stellar7/render_work.hpp"

#include <stdexcept>

namespace stellar7 {

WeaponLampWork weaponLampOperations(const std::uint16_t dirty, const WeaponLampState requested,
    const IndexedBitmap& bitmap, const CockpitPageState& pages) {
    constexpr std::uint16_t initialDirty = 0xffff;
    constexpr ProjectedPoint position{243, 170};
    constexpr std::uint16_t width = 24;
    constexpr std::uint16_t bitmapHeight = 5;
    constexpr std::uint16_t copyHeight = 10;
    constexpr std::uint16_t planeCount = 4;
    const auto value = static_cast<std::uint16_t>(requested);
    if (value > static_cast<std::uint16_t>(WeaponLampState::ready)
        || (dirty != initialDirty && dirty > static_cast<std::uint16_t>(WeaponLampState::ready))) {
        throw std::invalid_argument("unsupported weapon lamp dirty state");
    }
    WeaponLampWork result{dirty,pages,{{NormalWorkKind::instructions,6}},{}};
    if (dirty == value) {
        result.operations.push_back({NormalWorkKind::instructions,4}); //7C03..7C07.
        return result;
    }
    if (bitmap.width != width || bitmap.height != bitmapHeight || bitmap.pixels.size() != width * bitmapHeight
        || pages.cachePage() != 0xa400
        || !((pages.frontPage() == 0xa000 && pages.backPage() == 0xa800)
            || (pages.frontPage() == 0xa800 && pages.backPage() == 0xa000))) {
        throw std::invalid_argument("weapon lamp requires its original bitmap and recovered display roles");
    }
    result.pages.beginModal(); //7BBB..7BC1: draw/copy both select BACK.
    result.operations.push_back({NormalWorkKind::instructions,13 + 1}); //7BBB..7BD9 and2C17 JMPF.
    const auto bitmapWork = planarBitmapOperations(bitmap,position.x,PlanarBlitMode::transparent);
    result.operations.insert(result.operations.end(),bitmapWork.begin(),bitmapWork.end());
    for (std::uint16_t plane = 0; plane < planeCount; ++plane) {
        for (std::uint16_t y = 0; y < bitmapHeight; ++y) {
            for (std::uint16_t x = plane; x < width; x += planeCount) {
                const auto color = bitmap.pixels[y * width + x];
                if (color == 0) { continue; }
                result.effects.emplace_back(RadarPixelWrite{{static_cast<std::int16_t>(position.x + x),
                    static_cast<std::int16_t>(position.y + y)},color,pages.backPage()});
            }
        }
    }
    result.operations.push_back({NormalWorkKind::instructions,12 + 1}); //7BDE..7BF7 and22A1 JMPF.
    const auto copy = planarCopyOperations(position.x,width,copyHeight);
    result.operations.insert(result.operations.end(),copy.begin(),copy.end());
    const auto groups = planarCopyWork(position.x,width,copyHeight).stringIterations / copyHeight;
    result.effects.emplace_back(RadarPageCopy{pages.backPage(),pages.frontPage(),
        {static_cast<std::int16_t>(position.x - position.x % planeCount),position.y},
        static_cast<std::uint16_t>(groups * planeCount),copyHeight});
    result.pages.selectArmorCopy();
    result.dirty = value;
    result.operations.push_back({NormalWorkKind::instructions,6}); //7BFC..7C07.
    return result;
}

} // namespace stellar7
