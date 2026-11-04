#include "stellar7/render_work.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <utility>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

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

std::vector<stellar7::IndexedBitmap> frames(const std::uint16_t height) {
    std::vector<stellar7::IndexedBitmap> result;
    for (int page = 0; page < 4; ++page) {
        stellar7::IndexedBitmap bitmap{256, height, {}};
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < 256; ++x) {
                bitmap.pixels.push_back(static_cast<std::uint8_t>(page * 37 + y * 19 + x));
            }
        }
        result.push_back(std::move(bitmap));
    }
    return result;
}

void sameOperations(const std::vector<stellar7::NormalWorkOperation>& first,
    const std::vector<stellar7::NormalWorkOperation>& second) {
    assert(first.size() == second.size());
    for (std::size_t index = 0; index < first.size(); ++index) {
        assert(first[index].kind == second[index].kind && first[index].count == second[index].count);
        assert(first[index].value == second[index].value);
    }
}

std::uint64_t operationHash(const std::vector<stellar7::NormalWorkOperation>& operations) {
    std::vector<stellar7::NormalWorkOperation> canonical;
    for (const auto& operation : operations) {
        if (operation.kind == stellar7::NormalWorkKind::instructions && !canonical.empty()
            && canonical.back().kind == stellar7::NormalWorkKind::instructions) {
            canonical.back().count += operation.count;
        } else {
            canonical.push_back(operation);
        }
    }
    auto result = 14695981039346656037ULL;
    for (const auto& operation : canonical) {
        for (const auto value : {static_cast<std::uint64_t>(operation.kind), operation.count,
                 static_cast<std::uint64_t>(operation.value)}) {
            result = (result ^ value) * 1099511628211ULL;
        }
    }
    return result;
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace stellar7;
    const HorizonWorkState state{24, 145, 0, 90, 256, 16,
        HorizonBackground::gradient, HorizonOverlay::enabled, 233, 246};
    const auto bitmaps = frames(16);
    const auto first = horizonExecution(state, bitmaps);
    //Original CPU receipts: level1-horizon-p22-20261018/source.json.
    assert(operationHash(first.operations) == 0x0cc868deb5b64c35ULL);
    assert(first.pixels.size() == 272 * 122);
    assert(first.viewport.left == 24 && first.viewport.right == 295);
    assert(first.viewport.top == 24 && first.viewport.bottom == 145);
    assert(first.lineColor == 246 && first.fillColor == 246);
    assert(first.pixels.front().point.x == 24 && first.pixels.front().point.y == 67);
    assert(first.pixels.front().color == 233);
    assert(first.pixels[272].point.y == 66 && first.pixels[272].color == 234);
    assert(first.pixels[43 * 272].point.y == 24 && first.pixels[43 * 272].color == 254);
    std::vector<std::uint8_t> retained(320 * 200, 93);
    for (const auto& pixel : first.pixels) {
        retained[pixel.point.y * 320 + pixel.point.x] = pixel.color;
    }
    for (int y = 0; y < 200; ++y) {
        for (int x = 0; x < 320; ++x) {
            auto expected = std::uint8_t{93};
            if (x >= 24 && x <= 295 && y >= 24 && y < 68) {
                expected = static_cast<std::uint8_t>(std::min(254, 233 + 67 - y));
            } else if (x >= 24 && x <= 295 && y >= 68 && y < 84) {
                const auto source = x - 24;
                expected = bitmaps[source / 256].pixels[(y - 68) * 256 + source % 256];
            } else if (x >= 24 && x <= 295 && y >= 84 && y <= 145) {
                expected = 246;
            }
            assert(retained[y * 320 + x] == expected);
        }
    }
    //Both adjacent neutral horizon inputs retain zero yaw and the same source bank.
    const auto second = horizonExecution(state, bitmaps);
    sameOperations(first.operations, second.operations);
    for (const auto& pixel : second.pixels) {
        assert(retained[pixel.point.y * 320 + pixel.point.x] == pixel.color);
    }
    auto saturated = state;
    saturated.overlay = HorizonOverlay::disabled;
    saturated.skyColor = 255;
    const auto high = horizonExecution(saturated, bitmaps);
    assert(operationHash(high.operations) == 0xedf3727b3ef8ce6eULL);
    assert(high.pixels.front().color == 255 && high.pixels[43 * 272].color == 255);
    auto solid = state;
    solid.overlay = HorizonOverlay::disabled;
    solid.background = HorizonBackground::solid;
    const auto ordinary = horizonExecution(solid, bitmaps);
    assert(operationHash(ordinary.operations) == 0x986b5fd0df715f4eULL);
    sameOperations(ordinary.operations, horizonOperations(solid));
    assert(ordinary.pixels.front().point.y == 24 && ordinary.pixels.front().color == 233);
    auto disabled = state;
    disabled.overlay = HorizonOverlay::disabled;
    assert(operationHash(horizonOperations(disabled)) == 0xf299de8a79bd0a79ULL);
    const HorizonWorkState legacy{24, 145, 0, 90, 256, 18,
        HorizonBackground::solid, HorizonOverlay::disabled};
    assert(operationHash(horizonOperations(legacy)) == 0x38d0691ae1f7107cULL);
    auto changed = state;
    changed.cameraYaw = 1;
    rejects([&]() { horizonExecution(changed, bitmaps); });
    changed = state;
    changed.viewTop = 0;
    rejects([&]() { horizonOperations(changed); });
    changed = state;
    changed.bitmapHeight = 60;
    rejects([&]() { horizonOperations(changed); });
    changed = state;
    changed.bitmapWidth = 255;
    rejects([&]() { horizonOperations(changed); });
    changed = state;
    changed.stage = 70;
    rejects([&]() { horizonOperations(changed); });
    changed = state;
    changed.background = static_cast<HorizonBackground>(2);
    rejects([&]() { horizonOperations(changed); });
    changed = state;
    changed.overlay = static_cast<HorizonOverlay>(2);
    rejects([&]() { horizonOperations(changed); });
    rejects([&]() { horizonExecution(state, std::vector<IndexedBitmap>(3, bitmaps[0])); });
    auto malformed = bitmaps;
    malformed[3].pixels.pop_back();
    rejects([&]() { horizonExecution(state, malformed); });
}
