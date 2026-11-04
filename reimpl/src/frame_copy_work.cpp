#include "stellar7/frame_copy_work.hpp"

#include <stdexcept>

namespace stellar7 {

FrameCopyProfile decodeFrameCopyProfile(const std::vector<std::uint8_t>& driver) {
    constexpr std::size_t modeXDriverSize = 8900;
    constexpr std::size_t dispatchTable = 0x102;
    constexpr std::size_t rectangleDispatchIndex = 9;
    constexpr std::size_t farPointerBytes = 4;
    constexpr std::uint16_t noOpOffset = 0x1ee;
    constexpr std::uint8_t farReturnOpcode = 0xcb;
    constexpr auto entry = dispatchTable + rectangleDispatchIndex * farPointerBytes;
    if (driver.size() != modeXDriverSize || driver[entry] != (noOpOffset & 0xff)
        || driver[entry + 1] != (noOpOffset >> 8) || driver[entry + 2] != 0
        || driver[entry + 3] != 0 || driver[noOpOffset] != farReturnOpcode) {
        throw std::invalid_argument("unsupported original Mode-X rectangle dispatcher");
    }
    return FrameCopyProfile::modeXNoop;
}

FrameCopyWork frameCopyOperations(const FrameCopyProfile profile) {
    if (profile != FrameCopyProfile::modeXNoop) {
        throw std::invalid_argument("post-flip rectangles require owned Mode-X dispatcher profile");
    }
    constexpr std::uint64_t firstCallerAndStub = 10;
    constexpr std::uint64_t secondCallerAndStub = 11;
    constexpr std::uint64_t finalCleanup = 1;
    constexpr std::uint64_t twoFarReturns = 2;
    return {FrameCopyEffect::none, {{{24, 24, 271, 124}, {112, 146, 94, 44}}},
        {{NormalWorkKind::instructions, firstCallerAndStub + secondCallerAndStub
            + finalCleanup + twoFarReturns}}};
}

FailureFrameCopyWork failureFrameCopyOperations(const FrameCopyProfile profile) {
    if (profile != FrameCopyProfile::modeXNoop) {
        throw std::invalid_argument("failure display copy requires owned Mode-X dispatcher profile");
    }
    constexpr std::uint64_t flipCleanup = 1;
    constexpr std::uint64_t rectangleArguments = 7;
    constexpr std::uint64_t rectangleCall = 1;
    constexpr std::uint64_t dispatcherAndReturn = 2;
    constexpr std::uint64_t rectangleCleanup = 1;
    return {FrameCopyEffect::none,{0,0,320,200},
        {{NormalWorkKind::instructions,flipCleanup + rectangleArguments + rectangleCall
            + dispatcherAndReturn + rectangleCleanup}}};
}

}
