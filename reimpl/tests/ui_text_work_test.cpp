#include "stellar7/ui_text_work.hpp"

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
}

int main() {
    using namespace stellar7;
    constexpr Viewport display{0, 319, 0, 199};
    //Relocated original4C0E/4A4C/0FEB control: source receipt text-regression-source.json.
    BitmapFont font{65, 3, 0, {{9, 3, std::vector<std::uint8_t>(27, 1)}, {0, 3, {}}}};
    const auto work = uiTextOperations(font, UiFontLayout::proportional, "AB", {10, 10}, 128, 0xa000, display);
    assert(work.width == 9 && work.effects.size() == 27 && work.pixelDispatches.size() == 27);
    std::uint64_t dispatches = 0;
    std::vector<std::uint64_t> ports;
    for (const auto& operation : work.operations) {
        dispatches += operation.count;
        if (operation.kind == NormalWorkKind::port16Write) {
            ports.push_back(dispatches);
        } else {
            assert(operation.kind == NormalWorkKind::instructions);
        }
    }
    assert(dispatches == 1569);
    assert((ports == std::vector<std::uint64_t>{109,157,205,253,301,349,397,445,495,
        560,608,656,704,752,800,848,896,946,1011,1059,1107,1155,1203,1251,1299,1347,1397}));
    const auto first = std::get<RadarPixelWrite>(work.effects.front());
    const auto last = std::get<RadarPixelWrite>(work.effects.back());
    assert(first.point.x == 10 && first.point.y == 10 && first.color == 128 && first.page == 0xa000);
    assert(last.point.x == 18 && last.point.y == 12 && work.pixelDispatches.front() == 117
        && work.pixelDispatches.back() == 1405);
    const auto width = uiTextWidthOperations(font, UiFontLayout::proportional, "AB");
    assert(width.width == 9 && width.operations.size() == 1 && width.operations.front().count == 50);
    const auto empty = uiTextOperations(font, UiFontLayout::proportional, "", {10, 10}, 128, 0xa000, display);
    assert(empty.width == 0 && empty.effects.empty() && empty.operations.size() == 1 && empty.operations[0].count == 12);
    rejects([&]() { uiTextWidthOperations(font, UiFontLayout::fixed, "AB"); });
    rejects([&]() { uiTextWidthOperations(font, UiFontLayout::proportional, "C"); });
    rejects([&]() { uiTextWidthOperations(font, UiFontLayout::proportional, std::string_view("A\0B", 3)); });
    rejects([&]() { uiTextWidthOperations(font, static_cast<UiFontLayout>(2), "A"); });
    rejects([&]() { uiTextOperations(font, UiFontLayout::proportional, "A", {10, 10}, 128, 0xa100, display); });
    rejects([&]() { uiTextOperations(font, UiFontLayout::proportional, "A", {10, 10}, 128, 0xa000, {0,18,0,199}); });
    rejects([&]() { uiTextOperations(font, UiFontLayout::proportional, "A", {10, 10}, 128, 0xa000, {0,319,0,12}); });
    font.glyphs[0].pixels[0] = 2;
    rejects([&]() { uiTextWidthOperations(font, UiFontLayout::proportional, "A"); });
}
