#include "stellar7/startup_context_work.hpp"

namespace stellar7 {
namespace {
constexpr std::uint32_t gameBase = 0x08240;
constexpr std::uint32_t driverBase = 0x1f0a0;
constexpr std::uint16_t pitch = 0x2000;
constexpr std::uint16_t roll = 0;
constexpr std::uint16_t yaw = 0xa000;
constexpr std::uint16_t sineOffset = 0xc000;
constexpr std::uint64_t lookupBaseDispatches = 10;
constexpr std::uint64_t negativeQuadrantDispatches = 2;
}

StartupContextWork startupContextOperations() {
    StartupContextWork result{};
    std::uint64_t dispatches = 0;
    const auto plain = [&](const std::uint64_t count) {
        result.operations.push_back({NormalWorkKind::instructions, count});
        dispatches += count;
    };
    const auto effect = [&](const StartupContextOutput output, const std::uint16_t index,
        const std::uint16_t value, const std::uint32_t sourcePc) {
        result.effects.push_back({output, index, value, sourcePc, dispatches});
    };
    const auto lookup = [&](const std::int32_t angle) {
        const auto wrapped = static_cast<std::uint16_t>(angle);
        const bool negativeQuadrant = ((wrapped >> 4) & 0x0800) != 0;
        plain(lookupBaseDispatches + (negativeQuadrant ? negativeQuadrantDispatches : 0));
    };
    const auto matrix = makeFixedMatrixQ14(pitch, roll, yaw);
    const auto storeMatrix = [&](const std::uint64_t count, const std::uint16_t index,
        const std::uint16_t offset) {
        plain(count);
        result.state.matrix.values[index] = matrix.values[index];
        effect(StartupContextOutput::matrixWord, index,
            static_cast<std::uint16_t>(matrix.values[index]), driverBase + offset);
    };

    plain(4); //41B3,41B6,BC7E,BC81.
    for (std::uint16_t index = 0; index < result.state.allocationFlags.size(); ++index) {
        plain(1);
        result.state.allocationFlags[index] = 0;
        effect(StartupContextOutput::allocationFlag, index, 0, gameBase + 0xbc84);
        plain(3);
    }
    plain(23); //BC8D..BC60, including descriptor construction and matrix CALLF.

    plain(12); //34AE..34D7 selects mode3 and writes only its low byte.
    result.state.matrix.mode = matrix.mode;
    effect(StartupContextOutput::matrixMode, 0, matrix.mode, driverBase + 0x34d7);
    plain(4);
    plain(3); lookup(pitch + sineOffset);
    storeMatrix(1, 5, 0x358d);
    plain(4); lookup(roll - yaw);
    plain(5); lookup(roll + yaw);
    plain(5); lookup(roll - yaw);
    plain(5); lookup(roll + yaw);
    storeMatrix(11, 0, 0x363e);
    storeMatrix(9, 7, 0x3650);
    plain(4); lookup(yaw + roll + sineOffset);
    plain(6); lookup(yaw - roll + sineOffset);
    plain(6); lookup(roll + yaw + sineOffset);
    plain(6); lookup(roll - yaw + sineOffset);
    storeMatrix(10, 1, 0x3709);
    storeMatrix(8, 6, 0x371a);
    plain(3); lookup(pitch + yaw);
    plain(5); lookup(pitch - yaw);
    storeMatrix(3, 4, 0x3769);
    plain(3); lookup(pitch + roll);
    plain(5); lookup(pitch - roll);
    storeMatrix(3, 8, 0x37b8);
    plain(4); lookup(roll - pitch + sineOffset);
    plain(6); lookup(roll + pitch + sineOffset);
    storeMatrix(4, 2, 0x380f);
    plain(4); lookup(yaw - pitch + sineOffset);
    plain(6); lookup(yaw + pitch + sineOffset);
    storeMatrix(4, 3, 0x3866);
    plain(4); //3869..386C already includes RETF.

    plain(8); //BC65..BC74, including vector-transform CALLF.
    const auto light = transformToView({0, 0x0200, 0}, {{}, result.state.matrix, {}, 0});
    const std::array<std::int32_t, 3> values{light.x, light.y, light.z};
    const std::array<std::int32_t*, 3> outputs{&result.state.light.x, &result.state.light.y, &result.state.light.z};
    constexpr std::array<std::uint16_t, 3> stores{0x3ec7, 0x3efc, 0x3f32};
    plain(12);
    for (std::uint16_t index = 0; index < values.size(); ++index) {
        plain(24);
        *outputs[index] = static_cast<std::int16_t>(values[index]);
        effect(StartupContextOutput::lightWord, index,
            static_cast<std::uint16_t>(values[index]), driverBase + stores[index]);
    }
    plain(4); //3F35..3F38.
    plain(5); //BC79..BC9C returns to41BB.
    return result;
}

}  // namespace stellar7
