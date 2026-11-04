#include "stellar7/ui_bitmap_work.hpp"

#include <algorithm>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr int screenWidth = 320;
constexpr int screenHeight = 200;
constexpr int planeCount = 4;
constexpr std::uint16_t firstPage = 0xa000;
constexpr std::uint16_t pageSpacing = 0x400;
constexpr std::uint64_t headerDispatches = 13;
constexpr std::uint64_t headerWords = 5;
constexpr std::uint64_t postHeaderDispatches = 31;
constexpr std::uint64_t returnDispatches = 7;
constexpr std::uint64_t rightClipDispatches = 8;
constexpr std::uint64_t bottomClipDispatches = 4;
constexpr std::uint64_t topClipDispatches = 20;
constexpr std::uint64_t leftClipDispatches = 10;
enum class SourceDirection { forward, mirroredTitleCap, effectSentinel };

void instructions(std::vector<NormalWorkOperation>& operations, const std::uint64_t count) {
    if (!operations.empty() && operations.back().kind == NormalWorkKind::instructions) {
        operations.back().count += count;
        return;
    }
    operations.push_back({NormalWorkKind::instructions, count});
}

void requireProfile(const IndexedBitmap& bitmap, const ProjectedPoint position,
    const Viewport viewport, const std::uint16_t page) {
    if (bitmap.width == 0 || bitmap.height == 0 || bitmap.width > screenWidth
        || bitmap.height > screenHeight || bitmap.width % planeCount != 0
        || bitmap.pixels.size() != static_cast<std::size_t>(bitmap.width) * bitmap.height
        || viewport.left < 0 || viewport.right >= screenWidth || viewport.left > viewport.right
        || viewport.top < 0 || viewport.bottom >= screenHeight || viewport.top > viewport.bottom
        || position.x < -screenWidth || position.x >= 2 * screenWidth
        || position.y < -screenHeight || position.y >= 2 * screenHeight
        || page < firstPage || page > firstPage + 2 * pageSpacing
        || (page - firstPage) % pageSpacing != 0) {
        throw std::invalid_argument("unsupported clipped UI bitmap profile");
    }
}
UiBitmapWork bitmapOperations(const IndexedBitmap& bitmap, const ProjectedPoint position,
    const Viewport viewport, const std::uint16_t page, const SourceDirection direction) {
    if (direction!=SourceDirection::effectSentinel) {
        requireProfile(bitmap, position, viewport, page);
    }
    const bool mirrored = direction == SourceDirection::mirroredTitleCap;
    if (mirrored && (bitmap.width != 24 || bitmap.height != 22)) {
        throw std::invalid_argument("mirrored UI bitmap requires the source24x22 title cap");
    }
    UiBitmapWork result{};
    auto& operations = result.operations;
    instructions(operations, 1 + headerDispatches); //2C1B JMPF and1280 header setup.
    operations.push_back({NormalWorkKind::repeatString, headerWords});
    instructions(operations, postHeaderDispatches + (mirrored ? 4 : 0));
    const auto finishCull = [&]() {
        instructions(operations, 1 + returnDispatches); //Cull JMP and1DCB..1DD3.
    };

    int rightTrim = 0;
    instructions(operations, 5); //130B..1315, including the equality trim branch.
    if (int{position.x} + bitmap.width - 1 >= viewport.right) {
        rightTrim = int{position.x} + bitmap.width - 1 - viewport.right;
        instructions(operations, rightClipDispatches);
        if (rightTrim >= bitmap.width) {
            finishCull();
            return result;
        }
    }

    int rows = bitmap.height;
    instructions(operations, 5); //132A..1337.
    if (int{position.y} + bitmap.height - 1 >= viewport.bottom) {
        rows -= int{position.y} + bitmap.height - 1 - viewport.bottom;
        instructions(operations, bottomClipDispatches);
        if (rows <= 0) {
            finishCull();
            return result;
        }
    }

    const int topTrim = std::max(0, int{viewport.top} - position.y);
    instructions(operations, 3); //1346..134C.
    if (topTrim != 0) {
        rows -= topTrim;
        //Zero exits at1381; negative exits at1386 after the extra JNS.
        instructions(operations, topClipDispatches);
        if (rows <= 0) {
            if (rows < 0) {
                instructions(operations, 1);
            }
            instructions(operations, returnDispatches);
            return result;
        }
    }

    const int leftTrim = std::max(0, int{viewport.left} - position.x);
    instructions(operations, 3); //1389..138F.
    if (leftTrim != 0) {
        instructions(operations, leftClipDispatches + (mirrored ? 2 : 0));
        if (leftTrim >= bitmap.width) {
            finishCull();
            return result;
        }
    }

    const int columns = bitmap.width - rightTrim - leftTrim;
    const int drawX = position.x + leftTrim;
    instructions(operations, 27 + (mirrored ? 2 : 0)); //13AF..1403, source phase selection.
    operations.push_back({NormalWorkKind::port8Write, 1}); //3C4 index2.
    instructions(operations, 1); //1406 POP DX.
    for (int plane = 0; plane < planeCount; ++plane) {
        const int sourcePlane = mirrored ? planeCount - 1 - (leftTrim + plane) % planeCount
            : (leftTrim + plane) % planeCount;
        const bool sourceCarry = plane != 0 && sourcePlane == (mirrored ? planeCount - 1 : 0);
        const bool destinationCarry = plane != 0 && drawX % planeCount + plane == planeCount;
        instructions(operations, plane == 0 ? 9 : 30 + sourcePlane
            + (sourceCarry ? 1 : 0) + (destinationCarry ? 4 : 0));
        operations.push_back({NormalWorkKind::port8Write, 1}); //3C5 map mask, even for empty planes.
        const int planeColumns = (columns + planeCount - 1 - plane) / planeCount;
        instructions(operations, (plane == 0 ? 32 + sourcePlane : 12) + (mirrored ? 4 : 0));
        if (planeColumns == 0) {
            continue;
        }
        instructions(operations, 2);
        for (int row = 0; row < rows; ++row) {
            instructions(operations, 1); //MOV CX,DX.
            for (int column = plane; column < columns; column += planeCount) {
                const int sourceColumn = mirrored ? bitmap.width - 1 - leftTrim - column
                    : leftTrim + column;
                const auto source = static_cast<std::size_t>(topTrim + row) * bitmap.width + sourceColumn;
                const auto color = bitmap.pixels[source];
                if (mirrored) {
                    instructions(operations, 2); //MOV AL,[SI];DEC SI, preserving reverse source pitch.
                } else {
                    operations.push_back({NormalWorkKind::string, 1}); //LODSB.
                }
                instructions(operations, 2);
                if (color != 0) {
                    operations.push_back({NormalWorkKind::string, 1}); //STOSB.
                    instructions(operations, 1);
                    result.effects.push_back(RadarPixelWrite{{static_cast<std::int16_t>(drawX + column),
                        static_cast<std::int16_t>(position.y + topTrim + row)}, color, page});
                }
                instructions(operations, 2); //INC DI/LOOP.
            }
            instructions(operations, 4); //Original source skip, destination pitch, DEC/JNZ.
        }
    }
    instructions(operations, 3 + returnDispatches); //Final plane restore, JMP and RETF epilogue.
    return result;
}
}

UiBitmapWork uiBitmapOperations(const IndexedBitmap& bitmap, const ProjectedPoint position,
    const Viewport viewport, const std::uint16_t page) {
    return bitmapOperations(bitmap, position, viewport, page, SourceDirection::forward);
}

UiBitmapWork uiEffectSentinelOperations(const ProjectedPoint anchor,
    const Viewport viewport, const std::uint16_t page) {
    constexpr std::uint16_t sourceAliasHeight=0x534D;
    constexpr std::int16_t sourceAliasHalfHeight=sourceAliasHeight/2;
    if (!(viewport==Viewport{24,294,24,145}) || anchor.x<viewport.left || anchor.x>viewport.right
        || anchor.y<viewport.top || anchor.y>viewport.bottom
        || (page!=0xa000 && page!=0xa800)) {
        throw std::invalid_argument("unsupported source effect sentinel bitmap");
    }
    //0AC8 subtracts half the aliased height from both screen coordinates.
    const IndexedBitmap sourceHeader{0,sourceAliasHeight,{}};
    return bitmapOperations(sourceHeader,{static_cast<std::int16_t>(anchor.x-sourceAliasHalfHeight),
        static_cast<std::int16_t>(anchor.y-sourceAliasHalfHeight)},viewport,page,SourceDirection::effectSentinel);
}

UiBitmapWork uiMirroredBitmapOperations(const IndexedBitmap& bitmap, const ProjectedPoint position,
    const Viewport viewport, const std::uint16_t page) {
    return bitmapOperations(bitmap, position, viewport, page, SourceDirection::mirroredTitleCap);
}

}
