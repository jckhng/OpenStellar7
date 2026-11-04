#include "stellar7/ui_fill_work.hpp"

#include <stdexcept>

namespace stellar7 {

UiFillWork uiFillOperations(const CockpitCopyRegion rectangle, const std::uint8_t color,
    const std::uint16_t page) {
    constexpr int screenWidth = 320;
    constexpr int screenHeight = 200;
    constexpr int pixelsPerByte = 4;
    if (rectangle.x < 0 || rectangle.y < 0 || rectangle.width <= 0 || rectangle.height <= 0
        || rectangle.x + rectangle.width > screenWidth || rectangle.y + rectangle.height > screenHeight
        || (page != 0xa000 && page != 0xa400 && page != 0xa800)) {
        throw std::invalid_argument("UI fill requires an inside rectangle and physical page");
    }
    UiFillWork result{};
    using Kind = NormalWorkKind;
    const auto instructions = [&](const std::uint64_t count) {
        result.operations.push_back({Kind::instructions, count});
    };
    const auto string = [&]() { result.operations.push_back({Kind::string, 1}); };
    instructions(83); //2E25..300E, including2BE4 and all four inside clip tests.
    for (int row = 0; row < rectangle.height; ++row) {
        instructions(6 + 9); //300E..3018 CALL and0205..0215.
        const auto left = rectangle.x;
        const auto right = left + rectangle.width - 1;
        if (((left ^ right) & ~3) == 0) {
            instructions(14);
            result.operations.push_back({Kind::port16Write, 1});
            instructions(1);
            string();
        } else {
            instructions(11);
            result.operations.push_back({Kind::port16Write, 1});
            instructions(2);
            string();
            instructions(1);
            result.operations.push_back({Kind::port8Write, 1});
            instructions(6);
            const auto bulk = rectangle.width - pixelsPerByte + (left & 3);
            if ((bulk >> 2) & 1) {
                string();
            }
            result.operations.push_back({Kind::repeatString, static_cast<std::uint64_t>(bulk >> 3)});
            instructions(2);
            result.operations.push_back({Kind::port8Write, 1});
            instructions(1);
            string();
        }
        instructions(1 + 6); //0205 RETF and301C..3026.
        for (int column = 0; column < rectangle.width; ++column) {
            result.effects.push_back(RadarPixelWrite{{static_cast<std::int16_t>(rectangle.x + column),
                static_cast<std::int16_t>(rectangle.y + row)}, color, page});
        }
    }
    instructions(5); //3028..302C.
    return result;
}

} // namespace stellar7
