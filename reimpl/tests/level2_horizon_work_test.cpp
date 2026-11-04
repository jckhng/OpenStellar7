#include "stellar7/render_work.hpp"

#include <array>
#include <cassert>
#include <stdexcept>
#include <utility>

namespace {
using namespace stellar7;
template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
std::uint64_t operationHash(const std::vector<NormalWorkOperation>& operations) {
    std::uint64_t hash = 14695981039346656037ULL;
    std::uint64_t pending = 0;
    const auto append = [&](NormalWorkKind kind, std::uint64_t count) {
        hash = (hash ^ static_cast<std::uint64_t>(kind)) * 1099511628211ULL;
        hash = (hash ^ count) * 1099511628211ULL;
    };
    for (const auto& operation : operations) {
        if (operation.kind == NormalWorkKind::instructions) {
            pending += operation.count;
            continue;
        }
        if (pending != 0) { append(NormalWorkKind::instructions, pending); pending = 0; }
        append(operation.kind, operation.count);
    }
    if (pending != 0) { append(NormalWorkKind::instructions, pending); }
    return hash;
}
}
int main() {
    using namespace stellar7;
    std::vector<IndexedBitmap> frames;
    for (std::uint8_t page = 0; page < 4; ++page) {
        IndexedBitmap frame{256, 18, std::vector<std::uint8_t>(256 * 18)};
        for (std::size_t i = 0; i < frame.pixels.size(); ++i) {
            frame.pixels[i] = static_cast<std::uint8_t>(1 + page * 40 + i % 37);
        }
        frames.push_back(std::move(frame));
    }
    frames.push_back({72, 38, std::vector<std::uint8_t>(72 * 38)});
    for (std::size_t i = 0; i < frames.back().pixels.size(); ++i) {
        frames.back().pixels[i] = i % 5 == 0 ? 0 : static_cast<std::uint8_t>(170 + i % 43);
    }
    HorizonWorkState state{24, 145, 0, 90, 256, 18,
        HorizonBackground::solid, HorizonOverlay::enabled, 64, 78};
    //Saved original02A3..02AA P22 solid-sky rows7/8, unchanged at zero yaw.
    assert(operationHash(level2HorizonExecution(state, frames).operations) == 15652776375813924510ULL);
    state.overlay = HorizonOverlay::disabled;
    assert(operationHash(level2HorizonExecution(state, frames).operations) == 994466074957211026ULL);
    assert(operationHash(horizonExecution(state, frames).operations)
        == operationHash(level2HorizonExecution(state, frames).operations));
    for (const auto overlay : {HorizonOverlay::disabled, HorizonOverlay::enabled}) {
        state.overlay = overlay;
        for (const std::int16_t yaw : {0, 16, 80, -80, -240, 2048, 16384, 20480, -32768}) {
            state.cameraYaw = yaw;
            const auto work = level2HorizonExecution(state, frames);
            if (yaw != 0) {
                rejects([&] { horizonOperations(state); });
                rejects([&] { horizonExecution(state, frames); });
            }
            std::array<std::uint8_t, 320 * 200> actual{}, expected{};
            for (int y = 24; y <= 145; ++y) {
                for (int x = 24; x <= 295; ++x) {
                    expected[y * 320 + x] = y < 66 ? 64 : y >= 84 ? 78 : 0;
                }
            }
            const int angle = yaw >= 0 ? yaw / 64 : -((-int{yaw} + 63) / 64);
            if (overlay == HorizonOverlay::enabled && angle > 0 && angle < 320) {
                for (int y = 0; y < 38; ++y) {
                    for (int x = 0; x < 72; ++x) {
                        const int destination = angle - 40 + x;
                        const auto color = frames[4].pixels[y * 72 + x];
                        if (destination >= 24 && destination <= 295 && color != 0) {
                            expected[(y + 24) * 320 + destination] = color;
                        }
                    }
                }
            }
            const int primary = (-((static_cast<std::uint16_t>(angle) >> 8) & 255)) & 3;
            const int primaryX = 24 + (static_cast<std::uint16_t>(angle) & 255);
            for (int y = 0; y < 18; ++y) {
                for (int x = 24; x <= 295; ++x) {
                    const int relative = x - primaryX;
                    const int neighbor = relative < 0 ? -1 : relative >= 256 ? 1 : 0;
                    const int sourceX = relative - neighbor * 256;
                    expected[(y + 66) * 320 + x] = frames[(primary + neighbor) & 3].pixels[y * 256 + sourceX];
                }
            }
            for (const auto& pixel : work.pixels) {
                actual[pixel.point.y * 320 + pixel.point.x] = pixel.color;
            }
            assert(actual == expected);
            if (overlay == HorizonOverlay::enabled && yaw == 80) {
                assert(actual[24 * 320 + 24] == frames[4].pixels[63]);
            }
        }
    }
    state.cameraYaw = 80;
    auto wrongFrames = frames;
    wrongFrames[4] = {32, 27, std::vector<std::uint8_t>(32 * 27)};
    rejects([&] { level2HorizonExecution(state, wrongFrames); });
    wrongFrames = frames; wrongFrames[4].pixels.pop_back();
    rejects([&] { level2HorizonExecution(state, wrongFrames); });
    state.background = HorizonBackground::gradient;
    rejects([&] { level2HorizonExecution(state, frames); });
    state.background = HorizonBackground::solid; state.cameraYaw = 1;
    rejects([&] { level2HorizonExecution(state, frames); });
    state.cameraYaw = 80; state.bitmapHeight = 16;
    rejects([&] { level2HorizonExecution(state, frames); });
    state.bitmapHeight = 18; state.skyColor = 233;
    rejects([&] { level2HorizonExecution(state, frames); });
}
