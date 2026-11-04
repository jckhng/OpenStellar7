#pragma once

#include "stellar7/normal_clock.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace stellar7 {

enum class FrameCopyProfile { unknown, modeXNoop };
enum class FrameCopyEffect { none };

struct FrameCopyRectangle {
    std::uint16_t x;
    std::uint16_t y;
    std::uint16_t width;
    std::uint16_t height;
};

struct FrameCopyWork {
    FrameCopyEffect effect;
    std::array<FrameCopyRectangle, 2> requestedRectangles;
    std::vector<NormalWorkOperation> operations;
};

// Decoded STELLAR.RES OVL/VGA driver; reject unsupported dispatcher bindings.
FrameCopyProfile decodeFrameCopyProfile(const std::vector<std::uint8_t>& driver);

//8522 through pre8552; original Mode-X22b5 is RETF, not22a1's planar copy.
FrameCopyWork frameCopyOperations(FrameCopyProfile profile);

struct FailureFrameCopyWork {
    FrameCopyEffect effect;
    FrameCopyRectangle requestedRectangle;
    std::vector<NormalWorkOperation> operations;
};

//74BA through74CE: flip cleanup and one full-screen22B5/index9 call.
FailureFrameCopyWork failureFrameCopyOperations(FrameCopyProfile profile);

}
