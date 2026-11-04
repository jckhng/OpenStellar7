#include "stellar7/ui_pointer_work.hpp"

#include <stdexcept>

namespace stellar7 {
namespace {
constexpr int screenWidth = 320;
constexpr int screenHeight = 200;
constexpr int planeCount = 4;
constexpr std::uint16_t firstPage = 0xa000;
constexpr std::uint16_t pageSpacing = 0x400;
constexpr std::uint64_t residentJumpDispatches = 1;
constexpr std::uint64_t returnDispatches = 5;
enum class TransferDirection { save, restore };

void instructions(std::vector<NormalWorkOperation>& operations, const std::uint64_t count) {
    if (!operations.empty() && operations.back().kind == NormalWorkKind::instructions) {
        operations.back().count += count;
        return;
    }
    operations.push_back({NormalWorkKind::instructions, count});
}

void requireProfile(const CockpitCopyRegion rectangle, const std::uint16_t page,
    const std::vector<std::uint8_t>& transfer) {
    if (rectangle.x < 0 || rectangle.y < 0 || rectangle.width <= 0 || rectangle.height <= 0
        || int{rectangle.x} + rectangle.width > screenWidth
        || int{rectangle.y} + rectangle.height > screenHeight
        || page < firstPage || page > firstPage + 2 * pageSpacing
        || (page - firstPage) % pageSpacing != 0
        || transfer.size() < static_cast<std::size_t>(rectangle.width) * rectangle.height) {
        throw std::invalid_argument("pointer transfer requires an inside rectangle, physical page and owned buffer");
    }
}

std::vector<NormalWorkOperation> pointerOperations(const CockpitCopyRegion rectangle,
    const TransferDirection direction) {
    const bool saving = direction == TransferDirection::save;
    std::vector<NormalWorkOperation> operations;
    instructions(operations, residentJumpDispatches + (saving ? 17 : 18));
    operations.push_back({NormalWorkKind::port8Write, 1, static_cast<std::uint16_t>(saving ? 4 : 2)});
    instructions(operations, saving ? 11 : 13);
    const int phase = rectangle.x % planeCount;
    for (int row = 0; row < rectangle.height; ++row) {
        instructions(operations, 3); //Row count, initial plane/mask and byte cursor.
        for (int column = 0; column < rectangle.width; ++column) {
            const int plane = (phase + column) % planeCount;
            const auto output = static_cast<std::uint16_t>(saving ? plane : 1 << plane);
            operations.push_back({NormalWorkKind::port8Write, 1, output});
            operations.push_back({NormalWorkKind::string, 1}); //One MOVSB, never REP.
            instructions(operations, 3); //Advance plane/mask and test its wrap.
            if (plane == planeCount - 1) {
                instructions(operations, 2); //Advance planar byte and reset plane/mask.
            }
            instructions(operations, 2); //Undo MOVSB's planar cursor increment; LOOP.
        }
        instructions(operations, 3); //Next80-byte VGA row; DEC/JNZ.
    }
    instructions(operations, returnDispatches);
    return operations;
}
}

UiPointerSaveWork uiPointerSaveOperations(const IndexedPages& pages, const CockpitCopyRegion rectangle,
    const std::uint16_t copyPage, const std::vector<std::uint8_t>& transfer) {
    requireProfile(rectangle, copyPage, transfer);
    UiPointerSaveWork result{pointerOperations(rectangle, TransferDirection::save), transfer};
    std::size_t cursor = 0;
    for (int row = 0; row < rectangle.height; ++row) {
        for (int column = 0; column < rectangle.width; ++column) {
            result.transfer[cursor++] = pages.read(copyPage,
                {static_cast<std::int16_t>(rectangle.x + column), static_cast<std::int16_t>(rectangle.y + row)});
        }
    }
    return result;
}

UiPointerRestoreWork uiPointerRestoreOperations(const CockpitCopyRegion rectangle,
    const std::uint16_t drawPage, const std::vector<std::uint8_t>& transfer) {
    requireProfile(rectangle, drawPage, transfer);
    UiPointerRestoreWork result{pointerOperations(rectangle, TransferDirection::restore), {}};
    result.effects.reserve(static_cast<std::size_t>(rectangle.width) * rectangle.height);
    std::size_t cursor = 0;
    for (int row = 0; row < rectangle.height; ++row) {
        for (int column = 0; column < rectangle.width; ++column) {
            result.effects.push_back(RadarPixelWrite{{static_cast<std::int16_t>(rectangle.x + column),
                static_cast<std::int16_t>(rectangle.y + row)}, transfer[cursor++], drawPage});
        }
    }
    return result;
}

}
