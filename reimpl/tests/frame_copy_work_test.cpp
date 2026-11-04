#include "stellar7/frame_copy_work.hpp"

#include <cassert>
#include <stdexcept>

template<class Function>
void reject(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

int main() {
    using namespace stellar7;
    const auto work = frameCopyOperations(FrameCopyProfile::modeXNoop);
    assert(work.effect == FrameCopyEffect::none);
    assert(work.requestedRectangles[0].x == 24 && work.requestedRectangles[0].y == 24);
    assert(work.requestedRectangles[0].width == 271 && work.requestedRectangles[0].height == 124);
    assert(work.requestedRectangles[1].x == 112 && work.requestedRectangles[1].y == 146);
    assert(work.requestedRectangles[1].width == 94 && work.requestedRectangles[1].height == 44);
    assert(work.operations.size() == 1);
    assert(work.operations[0].kind == NormalWorkKind::instructions);
    assert(work.operations[0].count == 24);
    const auto failure = failureFrameCopyOperations(FrameCopyProfile::modeXNoop);
    assert(failure.effect == FrameCopyEffect::none);
    assert(failure.requestedRectangle.x == 0 && failure.requestedRectangle.y == 0);
    assert(failure.requestedRectangle.width == 320 && failure.requestedRectangle.height == 200);
    assert(failure.operations.size() == 1 && failure.operations[0].kind == NormalWorkKind::instructions);
    assert(failure.operations[0].count == 12);
    reject([] { failureFrameCopyOperations(FrameCopyProfile::unknown); });
    reject([] { frameCopyOperations(FrameCopyProfile::unknown); });
    reject([] { decodeFrameCopyProfile({}); });
    std::vector<std::uint8_t> binding(8900);
    binding[0x126] = 0xee;
    binding[0x127] = 1;
    binding[0x1ee] = 0xcb;
    assert(decodeFrameCopyProfile(binding) == FrameCopyProfile::modeXNoop);
    binding[0x126] = 0x6a;
    binding[0x127] = 0x10;
    reject([&] { decodeFrameCopyProfile(binding); });
    binding[0x126] = 0xee;
    binding[0x127] = 1;
    binding[0x1ee] = 0x90;
    reject([&] { decodeFrameCopyProfile(binding); });
    binding[0x1ee] = 0xcb;
    binding[0x128] = 1;
    reject([&] { decodeFrameCopyProfile(binding); });
    binding.resize(0x1ef);
    reject([&] { decodeFrameCopyProfile(binding); });
}
