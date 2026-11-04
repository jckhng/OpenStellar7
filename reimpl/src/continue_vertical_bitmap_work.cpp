#include "stellar7/continue_work.hpp"

#include <stdexcept>

namespace stellar7 {
UiBitmapWork continueVerticalBitmapOperations(const IndexedBitmap& bitmap, const ProjectedPoint position,
    const Viewport viewport, const std::uint16_t page) {
    constexpr int planes = 4;
    if (bitmap.width != 88 || bitmap.height != 11 || bitmap.pixels.size() != 88*11
        || viewport.left < 0 || viewport.right > 319 || viewport.top < 0 || viewport.bottom > 199
        || viewport.left > viewport.right || viewport.top > viewport.bottom
        || position.x < viewport.left || position.y < viewport.top
        || position.x+bitmap.width-1 >= viewport.right || position.y+bitmap.height-1 >= viewport.bottom
        || (page != 0xa000 && page != 0xa400 && page != 0xa800)) {
        throw std::invalid_argument("Continue vertical overlay requires the inside88x11 source bitmap and a physical page");
    }
    UiBitmapWork work{};
    const auto plain = [&](const std::uint64_t count) {
        if (!work.operations.empty() && work.operations.back().kind == NormalWorkKind::instructions) {
            work.operations.back().count += count;
        } else { work.operations.push_back({NormalWorkKind::instructions,count}); }
    };
    plain(1+13); //2C1B JMPF and1280 header setup.
    work.operations.push_back({NormalWorkKind::repeatString,5});
    plain(31+5); //Post-header; flags1 executes12D2..12DF last-row source setup.
    plain(5+5+3+3+27); //Inside clip tests130B..138F and phase/flags dispatch13AF..1403.
    work.operations.push_back({NormalWorkKind::port8Write,1});
    plain(1); //1406 POP DX.
    for (int plane = 0; plane < planes; ++plane) {
        const bool destinationCarry = plane != 0 && position.x%planes+plane == planes;
        plain(plane == 0 ? 9 : 30+plane+(destinationCarry ? 4 : 0));
        work.operations.push_back({NormalWorkKind::port8Write,1});
        //Flags1 selects16D7: SHR BX/SUB BX,[1274]/SAR BX instead of two SHR.
        plain((plane == 0 ? 32 : 12)+1);
        plain(2); //Nonempty plane width; all source planes contain22 columns.
        for (int row = 0; row < bitmap.height; ++row) {
            plain(1); //MOV CX,DX.
            for (int column = plane; column < bitmap.width; column += planes) {
                const auto color = bitmap.pixels[(bitmap.height-1-row)*bitmap.width+column];
                work.operations.push_back({NormalWorkKind::string,1}); //LODSB.
                plain(2); //OR/JZ.
                if (color != 0) {
                    work.operations.push_back({NormalWorkKind::string,1}); //STOSB.
                    plain(1); //DEC DI.
                    work.effects.push_back(RadarPixelWrite{{static_cast<std::int16_t>(position.x+column),
                        static_cast<std::int16_t>(position.y+row)},color,page});
                }
                plain(2); //INC DI/LOOP.
            }
            plain(4); //Negative source pitch, destination pitch, DEC/JNZ.
        }
    }
    plain(3+7); //1919 final restore/JMP and1DCB..1DD3 RETF epilogue.
    return work;
}
} // namespace stellar7
