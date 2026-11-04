#pragma once
#include "stellar7/level1.hpp"
#include "stellar7/normal_clock.hpp"
#include <optional>
namespace stellar7 {
bool isLevel1Type7ApproachState(const Level1Entity& actor) noexcept;
struct Level1Type7Work { Level1Entity actor; std::vector<NormalWorkOperation> operations; };
//A418 entry..A43E RET includingAF8C, excluding9298 caller. Presenter fields must
//already be refreshed. States0..2 only; beam installation rejects atomically.
Level1Type7Work level1Type7Operations(const Level1Entity& actor,
    const std::optional<std::size_t>& guardianEntityIndex = {});
} // namespace stellar7
