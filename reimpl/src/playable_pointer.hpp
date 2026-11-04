#pragma once

#include "stellar7/native_pointer.hpp"

#include <array>
#include <optional>
#include <vector>

namespace stellar7 {
//Extents are source5A7C inclusive offsets, not pixel counts.
struct PointerHitBox { std::int16_t x, y, extentX, extentY; };
enum class PointerButton { released, held, cancel };
struct PointerWidgetPoll {
    std::optional<std::size_t> pressed;
    std::optional<std::size_t> released;
    std::optional<std::size_t> activated;
};
ProjectedPoint sourcePointerPosition(ProjectedPoint position);
std::optional<std::size_t> pointerWidgetHit(const std::vector<PointerHitBox>& boxes, ProjectedPoint position);
PointerWidgetPoll pointerWidgetPoll(std::optional<std::size_t> pressed,
    const std::vector<PointerHitBox>& boxes, ProjectedPoint position, PointerButton button);
std::vector<PointerHitBox> sourceWidgetHitBoxes(const std::vector<std::uint8_t>& scratch, std::size_t count);
const std::vector<PointerHitBox>& mainPointerHitBoxes();
const std::vector<PointerHitBox>& preferencePointerHitBoxes();
const std::vector<PointerHitBox>& briefingPointerHitBoxes();
const std::vector<PointerHitBox>& scoresPointerHitBoxes();
const std::vector<PointerHitBox>& continuePointerHitBoxes();
const std::vector<PointerHitBox>& namePointerHitBoxes();
} // namespace stellar7
