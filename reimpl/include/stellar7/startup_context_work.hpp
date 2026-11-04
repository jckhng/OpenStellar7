#pragma once

#include "stellar7/level1.hpp"
#include "stellar7/normal_clock.hpp"

namespace stellar7 {

enum class StartupContextOutput { allocationFlag, matrixMode, matrixWord, lightWord };

struct StartupContextEffect {
    StartupContextOutput output;
    std::uint16_t index;
    std::uint16_t value;
    std::uint32_t sourcePc;
    std::uint64_t afterDispatch;
};

struct StartupViewDescriptor {
    std::array<std::int16_t, 6> words{};
};

struct StartupContextState {
    std::array<std::uint8_t, 5> allocationFlags{};
    FixedMatrixQ14 matrix{};
    FixedPosition light{};
    StartupViewDescriptor viewDescriptor;
};

struct StartupContextWork {
    StartupContextState state;
    std::vector<NormalWorkOperation> operations;
    std::vector<StartupContextEffect> effects;
};

// Complete41B3..41BB caller, including its fixed descriptor/matrix/light program.
// Native fresh storage; no guest registers, source phase or later state input.
StartupContextWork startupContextOperations();

}  // namespace stellar7
