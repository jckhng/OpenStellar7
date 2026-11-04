#include "stellar7/ui_pointer_work.hpp"

#include <array>
#include <cassert>
#include <stdexcept>

namespace {
template<class Function>
void rejects(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

std::uint64_t operationHash(const std::vector<stellar7::NormalWorkOperation>& operations) {
    constexpr std::uint64_t offset = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    auto hash = offset;
    for (const auto& operation : operations) {
        for (const auto value : {static_cast<std::uint64_t>(operation.kind), operation.count,
                 static_cast<std::uint64_t>(operation.value)}) {
            hash = (hash ^ value) * prime;
        }
    }
    return hash;
}

std::uint8_t sourceColor(const int x, const int y) {
    return static_cast<std::uint8_t>((x * 29 + y * 17) ^ (x >> 2));
}

std::uint64_t dispatches(const std::vector<stellar7::NormalWorkOperation>& operations) {
    std::uint64_t count = 0;
    for (const auto& operation : operations) {
        assert(operation.kind == stellar7::NormalWorkKind::instructions
            || operation.kind == stellar7::NormalWorkKind::port8Write
            || operation.kind == stellar7::NormalWorkKind::string);
        count += operation.count;
    }
    return count;
}
}

int main() {
    using namespace stellar7;
    struct Case {
        CockpitCopyRegion rectangle;
        std::uint64_t saveHash;
        std::uint64_t restoreHash;
    };
    //Original bounded CPU operation receipts, plus one resident JMPF per leaf.
    //projects/Stellar7/.work/analysis/pointer-work-p20-20261018/controlled-cpu.json.
    constexpr std::array<Case, 8> cases{{
        {{163, 69, 16, 21}, 0x0f5a7df66207ca6bULL, 0xe3f1d47c6b1263e6ULL},
        {{0, 0, 16, 10}, 0x609fd7f1b39aca91ULL, 0x52d2a46c69e15d54ULL},
        {{17, 25, 5, 2}, 0x64f54fa5a5bdd621ULL, 0xc3f8fe76fcabafa4ULL},
        {{2, 199, 3, 1}, 0x6014ab4546e501efULL, 0x9e6b234d98c035f2ULL},
        {{0, 0, 1, 1}, 0x0933c6d8fe2a00ceULL, 0x709a4c8b57319736ULL},
        {{1, 0, 1, 1}, 0xe365895f8d5b6019ULL, 0xc05d77fd42e9835bULL},
        {{2, 0, 1, 1}, 0x7e3d3fc45a3d0f40ULL, 0x609ded3b2e25c055ULL},
        {{3, 0, 1, 1}, 0x58f0264ae9dc26a1ULL, 0x20755ebf57f8588bULL}
    }};
    std::vector<std::uint8_t> transfer(1024);
    for (std::size_t index = 0; index < transfer.size(); ++index) {
        transfer[index] = static_cast<std::uint8_t>(index * 37 + 93);
    }
    const auto initialTransfer = transfer;
    for (const auto& test : cases) {
        const auto rectangle = test.rectangle;
        IndexedPages pages;
        for (int row = 0; row < rectangle.height; ++row) {
            for (int column = 0; column < rectangle.width; ++column) {
                const ProjectedPoint point{static_cast<std::int16_t>(rectangle.x + column),
                    static_cast<std::int16_t>(rectangle.y + row)};
                pages.write(0xa800, point, sourceColor(point.x, point.y));
                pages.write(0xa400, point, 0);
            }
        }
        const auto saved = uiPointerSaveOperations(pages, rectangle, 0xa800, transfer);
        assert(operationHash(saved.operations) == test.saveHash);
        assert(saved.transfer.size() == transfer.size() && transfer == initialTransfer);
        const auto pixels = static_cast<std::size_t>(rectangle.width) * rectangle.height;
        for (std::size_t index = 0; index < saved.transfer.size(); ++index) {
            if (index >= pixels) {
                assert(saved.transfer[index] == initialTransfer[index]);
                continue;
            }
            assert(saved.transfer[index] == sourceColor(rectangle.x + static_cast<int>(index % rectangle.width),
                rectangle.y + static_cast<int>(index / rectangle.width)));
        }

        const auto restored = uiPointerRestoreOperations(rectangle, 0xa000, transfer);
        assert(operationHash(restored.operations) == test.restoreHash);
        assert(restored.effects.size() == pixels);
        for (std::size_t index = 0; index < pixels; ++index) {
            const auto& pixel = std::get<RadarPixelWrite>(restored.effects[index]);
            assert(pixel.page == 0xa000 && pixel.color == transfer[index]);
            assert(pixel.point.x == rectangle.x + static_cast<int>(index % rectangle.width));
            assert(pixel.point.y == rectangle.y + static_cast<int>(index / rectangle.width));
        }
        pages.apply(restored.effects);
        for (std::size_t index = 0; index < pixels; ++index) {
            const ProjectedPoint point{static_cast<std::int16_t>(rectangle.x + index % rectangle.width),
                static_cast<std::int16_t>(rectangle.y + index / rectangle.width)};
            assert(pages.read(0xa000, point) == transfer[index]);
            assert(pages.read(0xa800, point) == sourceColor(point.x, point.y));
        }
    }

    const auto primary = uiPointerRestoreOperations({163, 69, 16, 21}, 0xa800, transfer);
    assert(dispatches(primary.operations) == 2684);
    assert(primary.operations.front().count == 19);
    std::uint64_t ports = 0;
    std::uint64_t strings = 0;
    for (std::size_t index = 0; index < primary.operations.size(); ++index) {
        const auto& operation = primary.operations[index];
        if (operation.kind == NormalWorkKind::port8Write) {
            ++ports;
        }
        if (operation.kind == NormalWorkKind::string) {
            ++strings;
            assert(operation.count == 1 && index != 0);
            assert(primary.operations[index - 1].kind == NormalWorkKind::port8Write);
        }
    }
    assert(ports == 337 && strings == 336);
    assert(primary.operations[primary.operations.size() - 3].value == 4);
    IndexedPages pages;
    pages.write(0xa000, {162, 69}, 19);
    pages.write(0xa000, {179, 69}, 20);
    pages.apply(uiPointerRestoreOperations({163, 69, 16, 21}, 0xa000, transfer).effects);
    assert(pages.read(0xa000, {162, 69}) == 19 && pages.read(0xa000, {179, 69}) == 20);
    rejects([&]() { uiPointerSaveOperations(IndexedPages{}, {163, 69, 16, 21}, 0xa800, transfer); });
    rejects([&]() { uiPointerRestoreOperations({163, 69, 16, 21}, 0xa100, transfer); });
    rejects([&]() { uiPointerRestoreOperations({-1, 0, 16, 21}, 0xa000, transfer); });
    rejects([&]() { uiPointerRestoreOperations({305, 0, 16, 21}, 0xa000, transfer); });
    rejects([&]() { uiPointerRestoreOperations({0, 180, 16, 21}, 0xa000, transfer); });
    rejects([&]() { uiPointerRestoreOperations({0, 0, 0, 21}, 0xa000, transfer); });
    rejects([&]() { uiPointerRestoreOperations({0, 0, 16, 0}, 0xa000, transfer); });
    rejects([&]() { uiPointerRestoreOperations({0, 0, 16, 21}, 0xa000, std::vector<std::uint8_t>(335)); });
}
