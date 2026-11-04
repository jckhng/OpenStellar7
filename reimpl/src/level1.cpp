#include "stellar7/level1.hpp"
#include "stellar7/campaign_loader.hpp"

#include <stdexcept>

#include <algorithm>
#include <cmath>

namespace stellar7 {
namespace {

constexpr std::array<std::uint16_t, StellarRng::wordCount> rngSeed{{
    56611, 38195, 62408, 19394, 63458, 41951, 43632, 24746,
    5195, 35144, 6440, 27405, 40459, 64497, 34311, 18013,
    60148, 27133, 27716, 6951, 56177, 58255, 43956, 56391,
    4062, 19854, 64637, 36152, 20552, 58223, 43554, 12070,
    17402, 46501, 8427, 60805, 62915, 33183, 11676, 45677,
    59691, 38869, 15777, 35309, 7156, 29877, 44, 54141,
    15112, 3573, 62078, 49776, 9609, 11915, 25426, 55853,
}};

constexpr Level1Entity entity(
    const std::size_t allocationIndex,
    const std::int16_t type,
    const std::uint32_t collisionRadius,
    const FixedPosition position,
    const std::uint16_t yaw,
    const std::int16_t forwardVelocity,
    const std::array<std::uint8_t, 6> state
) noexcept {
    return {
        allocationIndex,
        0,
        type,
        collisionRadius,
        position,
        0,
        0,
        yaw,
        0,
        0,
        0,
        0,
        0,
        0,
        forwardVelocity,
        state,
    };
}

constexpr std::array<Level1Entity, Level1State::entityCount>
level1Entities{{
    entity(0, 1, 1342, {-2048, 2048, 0}, 0xB000, 225, {255, 1, 0, 0, 1, 0}),
    entity(1, 11, 748, {8192, 8192, 1000}, 0x4800, 270, {255, 1, 5, 4, 3, 0}),
    entity(2, 29, 862, {16384, 16384, 0}, 0, 0, {254, 100, 0, 0, 0, 0}),
    entity(3, 29, 862, {16384, -16384, 0}, 0, 0, {254, 100, 0, 0, 0, 0}),
    entity(4, 29, 862, {-16384, 16384, 0}, 0, 0, {254, 100, 0, 0, 0, 0}),
    entity(5, 29, 862, {-16384, -16384, 0}, 0, 0, {254, 16, 0, 0, 243, 0}),
}};

constexpr Level1Entity level1Player = entity(
    6,
    0,
    723,
    {0, 0, 0},
    0,
    0,
    {0, 1, 0, 0, 0, 0}
);

constexpr std::array<GroundPoint, Level1GroundState::pointCount>
level1EntryGroundPoints{{
    {-6688, 1280},
    {2208, 1152},
    {-768, 4320},
    {-1216, 2016},
    {6240, 1248},
    {3776, 3616},
}};

constexpr std::uint16_t decrementRngIndex(
    const std::uint16_t index
) noexcept {
    return index == 0 ? 0x6E : static_cast<std::uint16_t>(index - 2);
}

const std::array<std::int16_t, 2049>& cosineTable() noexcept {
    static const std::array<std::int16_t, 2049> table = [] {
        std::array<std::int16_t, 2049> result{};
        constexpr double pi = 3.14159265358979323846264338327950288;
        for (std::size_t index = 0; index < result.size(); ++index) {
            if (index == 0) {
                result[index] = 0x4000;
            } else if (index == 1024) {
                result[index] = 0;
            } else if (index == 2048) {
                result[index] = -0x4000;
            } else {
                result[index] = static_cast<std::int16_t>(std::floor(
                    16384.0 * std::cos(
                        pi * static_cast<double>(index) / 2048.0
                    )
                ));
            }
        }
        return result;
    }();
    return table;
}

const std::array<std::uint16_t, 512>& headingTable() noexcept {
    static const std::array<std::uint16_t, 512> table = [] {
        std::array<std::uint16_t, 512> result{};
        constexpr double pi = 3.14159265358979323846264338327950288;
        for (std::size_t index = 0; index < result.size(); ++index) {
            result[index] = static_cast<std::uint16_t>(std::floor(
                std::atan(static_cast<double>(index) / 512.0)
                * 2048.0 / pi
            ));
        }
        return result;
    }();
    return table;
}

std::int16_t fixedCosine(const std::uint16_t angle) noexcept {
    std::uint16_t index = static_cast<std::uint16_t>(angle >> 4);
    if ((index & 0x0800) != 0) {
        index = static_cast<std::uint16_t>(0x1000 - index);
    }
    return cosineTable()[index];
}

std::int16_t fixedSine(const std::uint16_t angle) noexcept {
    return fixedCosine(static_cast<std::uint16_t>(angle + 0xC000));
}

std::int32_t arithmeticShiftRight14(const std::int32_t value) noexcept {
    if (value >= 0) {
        return value / 0x4000;
    }
    return -static_cast<std::int32_t>(
        (static_cast<std::int64_t>(-value) + 0x3FFF) / 0x4000
    );
}

std::int32_t arithmeticShiftRight14(const std::int64_t value) noexcept {
    if (value >= 0) {
        return static_cast<std::int32_t>(value / 0x4000);
    }
    return -static_cast<std::int32_t>((-value + 0x3FFF) / 0x4000);
}

std::int32_t arithmeticShiftRight(
    const std::int64_t value,
    const unsigned bits
) noexcept {
    const auto divisor = static_cast<std::int64_t>(1) << bits;
    if (value >= 0) {
        return static_cast<std::int32_t>(value / divisor);
    }
    return -static_cast<std::int32_t>(
        (-value + divisor - 1) / divisor
    );
}

std::int16_t wrappedAdd(
    const std::int16_t value,
    const std::int32_t delta
) noexcept {
    return static_cast<std::int16_t>(
        static_cast<std::uint16_t>(value)
        + static_cast<std::uint16_t>(delta)
    );
}

void resetGroundPoint(GroundPoint& point, StellarRng& rng) noexcept {
    const auto xRandom = static_cast<std::int16_t>(rng.next());
    point.x = static_cast<std::int16_t>((xRandom % 0x127) * 0x20);
    const auto yRandom = static_cast<std::int16_t>(rng.next());
    point.y = static_cast<std::int16_t>(
        (yRandom % 0x38 + 0x5A) * 0x20
    );
}

template<typename Item>
void advanceKinematics(Item& item) noexcept {
    item.pitch = static_cast<std::uint16_t>(
        item.pitch + item.pitchVelocity
    );
    item.roll = static_cast<std::uint16_t>(
        item.roll + item.rollVelocity
    );
    item.yaw = static_cast<std::uint16_t>(
        item.yaw + item.yawVelocity
    );
    if (item.pitch == 0 && item.roll == 0) {
        item.position.x += arithmeticShiftRight14(
            -static_cast<std::int32_t>(fixedSine(item.yaw))
            * item.forwardVelocity
        );
        item.position.y += arithmeticShiftRight14(
            static_cast<std::int32_t>(fixedCosine(item.yaw))
            * item.forwardVelocity
        );
    } else {
        // FUN_1000_3b3a performs the x-axis negation before its first Q14
        // shift.  Preserve that staged rounding instead of negating the
        // already-truncated positive pitched-forward value.
        const auto negativePitchedForward = arithmeticShiftRight14(
            -static_cast<std::int32_t>(fixedCosine(item.pitch))
            * item.forwardVelocity
        );
        const auto pitchedForward = arithmeticShiftRight14(
            static_cast<std::int32_t>(fixedCosine(item.pitch))
            * item.forwardVelocity
        );
        item.position.x += arithmeticShiftRight14(
            static_cast<std::int32_t>(fixedSine(item.yaw))
            * negativePitchedForward
        );
        item.position.y += arithmeticShiftRight14(
            static_cast<std::int32_t>(fixedCosine(item.yaw))
            * pitchedForward
        );
        item.position.z += arithmeticShiftRight14(
            static_cast<std::int32_t>(fixedSine(item.pitch))
            * item.forwardVelocity
        );
    }
    item.position.x += item.xVelocity;
    item.position.y += item.yVelocity;
    item.position.z += item.zVelocity;
}

template<typename Item>
void reverseTranslation(Item& item) noexcept {
    item.position.x -= item.xVelocity;
    item.position.y -= item.yVelocity;
    item.position.z -= item.zVelocity;
    if (item.pitch == 0 && item.roll == 0) {
        item.position.x -= arithmeticShiftRight14(
            -static_cast<std::int32_t>(fixedSine(item.yaw))
            * item.forwardVelocity
        );
        item.position.y -= arithmeticShiftRight14(
            static_cast<std::int32_t>(fixedCosine(item.yaw))
            * item.forwardVelocity
        );
    } else {
        const auto pitchedForward = arithmeticShiftRight14(
            static_cast<std::int32_t>(fixedCosine(item.pitch))
            * item.forwardVelocity
        );
        item.position.x -= arithmeticShiftRight14(
            -static_cast<std::int32_t>(fixedSine(item.yaw))
            * pitchedForward
        );
        item.position.y -= arithmeticShiftRight14(
            static_cast<std::int32_t>(fixedCosine(item.yaw))
            * pitchedForward
        );
        item.position.z -= arithmeticShiftRight14(
            static_cast<std::int32_t>(fixedSine(item.pitch))
            * item.forwardVelocity
        );
    }
}

template<typename Left, typename Right>
bool collides(const Left& left, const Right& right) noexcept {
    if (left.type == 7 || right.type == 7) {
        return false;
    }
    const auto threshold = static_cast<std::int64_t>(
        (left.collisionRadius + right.collisionRadius) >> 1U
    );
    const auto magnitude = [](const std::int64_t value) noexcept {
        return value < 0 ? -value : value;
    };
    return (
        magnitude(
            static_cast<std::int64_t>(left.position.x)
            - right.position.x
        ) <= threshold
        && magnitude(
            static_cast<std::int64_t>(left.position.y)
            - right.position.y
        ) <= threshold
        && magnitude(
            static_cast<std::int64_t>(left.position.z)
            - right.position.z
        ) <= threshold
    );
}

constexpr std::array<std::uint16_t, 17> scoreByType{{
    0, 20, 35, 20, 30, 75, 90, 0, 25,
    30, 40, 50, 60, 10, 5, 5, 5,
}};

}  // namespace

FixedMatrixQ14 makeFixedMatrixQ14(
    const std::uint16_t pitch,
    const std::uint16_t roll,
    const std::uint16_t yaw
) noexcept {
    std::uint16_t mode = 3;
    if (pitch == 0) {
        mode = 2;
        if (roll == 0) {
            mode = 1;
            if (yaw == 0) {
                mode = 0;
            }
        }
    }
    FixedMatrixQ14 matrix{{}, mode};
    auto& value = matrix.values;
    if (mode == 0) {
        value = {
            0x4000, 0, 0,
            0, 0x4000, 0,
            0, 0, 0x4000,
        };
        return matrix;
    }
    if (mode == 1) {
        const auto cosine = fixedCosine(yaw);
        const auto sine = fixedSine(yaw);
        value = {
            cosine, sine, 0,
            static_cast<std::int16_t>(-sine), cosine, 0,
            0, 0, 0x4000,
        };
        return matrix;
    }

    const auto halfCosine = [](const std::uint16_t angle) noexcept {
        return arithmeticShiftRight(fixedCosine(angle), 1);
    };
    const auto q14Product = [](
        const std::int16_t left,
        const std::int32_t right
    ) noexcept {
        return arithmeticShiftRight14(
            static_cast<std::int64_t>(left) * right
        );
    };
    const auto angle = [](const std::uint32_t raw) noexcept {
        return static_cast<std::uint16_t>(raw);
    };
    const auto sinePitch = fixedSine(pitch);

    auto first = halfCosine(angle(roll - yaw));
    auto second = halfCosine(angle(roll + yaw));
    const auto a = first + second;
    const auto b = first - second;
    value[0] = static_cast<std::int16_t>(
        a - q14Product(sinePitch, b)
    );
    value[7] = static_cast<std::int16_t>(
        b - q14Product(sinePitch, a)
    );

    first = halfCosine(angle(yaw + roll + 0xC000U));
    second = halfCosine(angle(yaw - roll + 0xC000U));
    const auto c = first + second;
    first = halfCosine(angle(roll + yaw + 0xC000U));
    second = halfCosine(angle(roll - yaw + 0xC000U));
    const auto d = first + second;
    value[1] = static_cast<std::int16_t>(
        q14Product(sinePitch, d) + c
    );
    value[6] = static_cast<std::int16_t>(
        q14Product(sinePitch, c) + d
    );

    value[4] = static_cast<std::int16_t>(
        halfCosine(angle(pitch - yaw))
        + halfCosine(angle(pitch + yaw))
    );
    value[8] = static_cast<std::int16_t>(
        halfCosine(angle(pitch - roll))
        + halfCosine(angle(pitch + roll))
    );
    value[2] = static_cast<std::int16_t>(-(
        halfCosine(angle(roll + pitch + 0xC000U))
        + halfCosine(angle(roll - pitch + 0xC000U))
    ));
    value[3] = static_cast<std::int16_t>(-(
        halfCosine(angle(yaw + pitch + 0xC000U))
        + halfCosine(angle(yaw - pitch + 0xC000U))
    ));
    value[5] = sinePitch;
    return matrix;
}

FixedMatrixQ14 composeFixedMatricesQ14(
    const FixedMatrixQ14& first,
    const FixedMatrixQ14& second,
    const std::uint16_t lowWordSeed
) noexcept {
    if (first.mode == 0) {
        return second;
    }
    if (second.mode == 0) {
        return first;
    }
    FixedMatrixQ14 result{{
        0, 0, 0,
        0, 0, 0,
        0, 0, 0,
    }, static_cast<std::uint16_t>(
        first.mode == 1 && second.mode == 1 ? 1 : 3
    )};
    const auto ordinaryElement = [&](
        const std::size_t row,
        const std::size_t column
    ) noexcept {
        std::int64_t sum = 0;
        for (std::size_t inner = 0; inner < 3; ++inner) {
            sum += static_cast<std::int64_t>(
                second.values[row + inner * 3]
            ) * first.values[inner + column * 3];
        }
        return static_cast<std::int16_t>(arithmeticShiftRight14(sum));
    };
    const auto buggyTwoProduct = [&](
        const std::int16_t firstLeft,
        const std::int16_t firstRight,
        const std::int16_t secondLeft,
        const std::int16_t secondRight
    ) noexcept {
        const auto firstProduct = static_cast<std::int32_t>(firstLeft)
            * firstRight;
        const auto secondProduct = static_cast<std::int32_t>(secondLeft)
            * secondRight;
        const auto firstHighWord = arithmeticShiftRight(firstProduct, 16);
        const auto sum = static_cast<std::int64_t>(firstHighWord) * 0x10000
            + lowWordSeed
            + secondProduct;
        const auto shifted = static_cast<std::uint32_t>(sum) << 2;
        return static_cast<std::int16_t>(shifted >> 16);
    };

    if (first.mode == 1 && second.mode == 1) {
        for (std::size_t column = 0; column < 2; ++column) {
            for (std::size_t row = 0; row < 2; ++row) {
                result.values[row + column * 3] = buggyTwoProduct(
                    second.values[row],
                    first.values[column * 3],
                    second.values[row + 3],
                    first.values[1 + column * 3]
                );
            }
        }
        result.values[2] = 0;
        result.values[5] = 0;
        result.values[6] = 0;
        result.values[7] = 0;
        result.values[8] = 0x4000;
        return result;
    }
    if (first.mode == 1) {
        for (std::size_t column = 0; column < 2; ++column) {
            for (std::size_t row = 0; row < 3; ++row) {
                result.values[row + column * 3] = buggyTwoProduct(
                    second.values[row],
                    first.values[column * 3],
                    second.values[row + 3],
                    first.values[1 + column * 3]
                );
            }
        }
        result.values[6] = second.values[6];
        result.values[7] = second.values[7];
        result.values[8] = second.values[8];
        return result;
    }
    if (second.mode == 1) {
        for (std::size_t column = 0; column < 3; ++column) {
            for (std::size_t row = 0; row < 2; ++row) {
                result.values[row + column * 3] = buggyTwoProduct(
                    second.values[row],
                    first.values[column * 3],
                    second.values[row + 3],
                    first.values[1 + column * 3]
                );
            }
            result.values[2 + column * 3] = first.values[2 + column * 3];
        }
        return result;
    }
    for (std::size_t column = 0; column < 3; ++column) {
        for (std::size_t row = 0; row < 3; ++row) {
            result.values[row + column * 3] = ordinaryElement(row, column);
        }
    }
    return result;
}

void transpose(FixedMatrixQ14& matrix) noexcept {
    std::swap(matrix.values[1], matrix.values[3]);
    std::swap(matrix.values[2], matrix.values[6]);
    std::swap(matrix.values[5], matrix.values[7]);
}

ViewTransform initialLevel1MainView() noexcept {
    return {
        {0, 0, 0},
        {{
            16384, 0, 0,
            0, 16384, 0,
            0, 0, 16384,
        }, 0},
        {24, 294, 24, 145},
        128,
    };
}

ViewTransform initialLevel1RadarView() noexcept {
    return {
        {0, 0, 200000},
        {{
            16384, 0, 0,
            0, 0, 16384,
            0, -16384, 0,
        }, 3},
        {112, 205, 146, 180},
        128,
    };
}

FixedPosition transformToView(
    const FixedPosition worldPosition,
    const ViewTransform& view
) noexcept {
    const std::array<std::int32_t, 3> relative{{
        worldPosition.x - view.origin.x,
        worldPosition.y - view.origin.y,
        worldPosition.z - view.origin.z,
    }};
    if (view.matrix.mode == 0) {
        return {relative[0], relative[1], relative[2]};
    }
    std::array<std::int32_t, 3> transformed{};
    for (std::size_t output = 0; output < transformed.size(); ++output) {
        std::int64_t sum = 0;
        for (std::size_t input = 0; input < relative.size(); ++input) {
            sum += static_cast<std::int64_t>(relative[input])
                * view.matrix.values[output + input * 3];
        }
        transformed[output] = arithmeticShiftRight14(sum);
    }
    return {transformed[0], transformed[1], transformed[2]};
}

ProjectedPoint projectToViewport(
    const FixedPosition viewPosition,
    const ViewTransform& view
) noexcept {
    const auto centerX = static_cast<std::int32_t>(
        (view.viewport.right - view.viewport.left) / 2
        + view.viewport.left
    );
    const auto centerY = static_cast<std::int32_t>(
        (view.viewport.bottom - view.viewport.top) / 2
        + view.viewport.top
    );
    const auto projectedX = static_cast<std::int32_t>(
        static_cast<std::int64_t>(viewPosition.x)
        * view.perspectiveScale
        / viewPosition.y
    );
    const auto projectedY = static_cast<std::int32_t>(
        static_cast<std::int64_t>(viewPosition.z)
        * view.perspectiveScale
        / viewPosition.y
    );
    return {
        static_cast<std::int16_t>(centerX + projectedX),
        static_cast<std::int16_t>(centerY - projectedY),
    };
}

StellarRng::StellarRng() noexcept
    : table_(rngSeed) {
}

StellarRng::StellarRng(
    const std::array<std::uint16_t, wordCount>& table,
    const std::uint16_t indexA,
    const std::uint16_t indexB,
    const std::uint16_t last,
    const std::uint32_t calls
) noexcept
    : table_(table),
      indexA_(indexA),
      indexB_(indexB),
      last_(last),
      calls_(calls) {
}

std::uint16_t StellarRng::next() noexcept {
    const auto a = static_cast<std::size_t>(indexA_ / 2);
    const auto b = static_cast<std::size_t>(indexB_ / 2);
    table_[b] = static_cast<std::uint16_t>(table_[a] + table_[b]);
    last_ = table_[b];
    indexA_ = decrementRngIndex(indexA_);
    indexB_ = decrementRngIndex(indexB_);
    ++calls_;
    return last_;
}

std::uint16_t StellarRng::indexA() const noexcept {
    return indexA_;
}

std::uint16_t StellarRng::indexB() const noexcept {
    return indexB_;
}

std::uint16_t StellarRng::last() const noexcept {
    return last_;
}

std::uint32_t StellarRng::calls() const noexcept {
    return calls_;
}

Level1GroundState::Level1GroundState() noexcept
    : points_(level1EntryGroundPoints) {
}

Level1GroundState::Level1GroundState(StellarRng& rng) noexcept {
    for (auto& point : points_) {
        resetGroundPoint(point, rng);
    }
}

Level1GroundState::Level1GroundState(
    const std::array<GroundPoint, pointCount>& points
) noexcept
    : points_(points) {
}

GroundPoint advanceGroundPoint(GroundPoint point, const std::int16_t turn,
    const std::int16_t speed) noexcept {
    constexpr std::uint16_t horizontalCenter = 0x13e0;
    constexpr std::uint16_t verticalCenter = 0x0aa0;
    constexpr std::uint16_t turnCenter = 0x00a0;
    constexpr unsigned speedFractionBits = 11;
    if (speed != 0) {
        point.x = wrappedAdd(
            point.x,
            arithmeticShiftRight(
                static_cast<std::int16_t>(static_cast<std::uint16_t>(point.x) - horizontalCenter)
                * speed,
                speedFractionBits
            )
        );
        point.y = wrappedAdd(
            point.y,
            arithmeticShiftRight(
                static_cast<std::int16_t>(static_cast<std::uint16_t>(point.y) - verticalCenter)
                * speed,
                speedFractionBits
            )
        );
    }
    const auto coarseTurn = arithmeticShiftRight(turn, 4);
    if (coarseTurn != 0) {
        point.x = wrappedAdd(
            point.x,
            arithmeticShiftRight(
                static_cast<std::int16_t>(static_cast<std::uint16_t>(point.y) - turnCenter)
                * coarseTurn,
                9
            )
        );
    }
    return point;
}

void Level1GroundState::advance(
    const std::int16_t turn,
    const std::int16_t speed,
    StellarRng& rng
) noexcept {
    for (std::size_t index = 0; index < points_.size(); ++index) {
        auto& point = points_[index];
        visible_[index] = false;
        point = advanceGroundPoint(point, turn, speed);

        const auto screenX = arithmeticShiftRight(point.x, 5);
        const auto screenY = arithmeticShiftRight(point.y, 5);
        if (
            screenX <= 0x17
            || screenX >= 0x128
            || screenY <= 0x59
            || screenY >= 0x93
        ) {
            resetGroundPoint(point, rng);
        } else {
            visible_[index] = true;
        }
    }
}

const std::array<GroundPoint, Level1GroundState::pointCount>&
Level1GroundState::points() const noexcept {
    return points_;
}

bool Level1GroundState::pointVisible(const std::size_t index) const noexcept {
    return index < visible_.size() && visible_[index];
}

void advanceEntityKinematics(Level1Entity& item) noexcept {
    advanceKinematics(item);
}

void reverseEntityTranslation(Level1Entity& item) noexcept {
    reverseTranslation(item);
}

std::uint32_t approximatePlanarDistance(
    const FixedPosition source,
    const FixedPosition target
) noexcept {
    const auto magnitude = [](const std::int64_t value) noexcept {
        // FUN_1000_13de explicitly maps a signed 32-bit delta of -32768
        // to 32767 before comparing the two planar components.
        if (value == -0x8000LL) {
            return std::uint64_t{0x7FFF};
        }
        return static_cast<std::uint64_t>(value < 0 ? -value : value);
    };
    auto first = magnitude(
        static_cast<std::int64_t>(target.x) - source.x
    );
    auto second = magnitude(
        static_cast<std::int64_t>(target.y) - source.y
    );
    if (first < second) {
        std::swap(first, second);
    }
    return static_cast<std::uint32_t>(
        first + ((second * 3U) >> 3U)
    );
}

std::uint16_t fixedHeadingBetween(
    const FixedPosition source,
    const FixedPosition target
) noexcept {
    const auto deltaX = static_cast<std::int64_t>(target.x) - source.x;
    const auto deltaY = static_cast<std::int64_t>(target.y) - source.y;
    const auto magnitude = [](const std::int64_t value) noexcept {
        return static_cast<std::uint64_t>(value < 0 ? -value : value);
    };
    const auto x = magnitude(deltaX);
    const auto y = magnitude(deltaY);
    std::int32_t angle;
    if (x == 0) {
        angle = 0x400;
    } else if (y == 0) {
        angle = 0;
    } else if (x == y) {
        angle = 0x200;
    } else if (x > y) {
        const auto ratio = static_cast<std::size_t>((y << 9U) / x);
        angle = headingTable()[ratio];
    } else {
        const auto ratio = static_cast<std::size_t>((x << 9U) / y);
        angle = 0x400 - headingTable()[ratio];
    }
    if (deltaX < 0) {
        angle = 0x800 - angle;
    }
    if (deltaY < 0) {
        angle = 0x1000 - angle;
    }
    return static_cast<std::uint16_t>((angle - 0x400) * 0x10);
}

std::uint16_t presenterTargetHeading(
    const FixedPosition source,
    const FixedPosition target
) noexcept {
    // FUN_1000_9298 reads only the low position words, performs 16-bit
    // SUBs at 1000:92e2/92e5 and 1000:92eb/92ee, then CWD-sign-extends
    // each wrapped difference before calling the 32-bit heading routine
    // FUN_1000_3ed6.
    const auto deltaX = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(target.x)
        - static_cast<std::uint16_t>(source.x)
    );
    const auto deltaY = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(target.y)
        - static_cast<std::uint16_t>(source.y)
    );
    const auto signedWord = [](const std::uint16_t value) noexcept {
        return value <= 0x7FFFU
            ? static_cast<std::int32_t>(value)
            : static_cast<std::int32_t>(value) - 0x10000;
    };
    return fixedHeadingBetween(
        {},
        {signedWord(deltaX), signedWord(deltaY), 0}
    );
}

void normalizeWorldPosition(FixedPosition& position) noexcept {
    const auto normalize = [](std::int32_t& value) noexcept {
        // FUN_1000_0c6a adjusts exactly one 0x10000 page per pass.
        if (value > 0x8000) {
            value -= 0x10000;
        } else if (value < -0x8000) {
            value += 0x10000;
        }
    };
    normalize(position.x);
    normalize(position.y);
}

namespace {

bool projectionOriginMoved(
    const std::int32_t current,
    const std::int32_t stored
) noexcept {
    // FUN_1000_3768 computes the absolute signed 32-bit difference and
    // preserves the active list only while each planar component is below
    // 0x400. Using a wider subtraction avoids host signed overflow without
    // changing the threshold result.
    const auto difference =
        static_cast<std::int64_t>(current)
        - static_cast<std::int64_t>(stored);
    return difference <= -0x400 || difference >= 0x400;
}

std::uint8_t presenterAbsoluteXLowByte(
    const std::int32_t source,
    const std::int32_t target
) noexcept {
    // FUN_1000_9298 calls FUN_1000_13de immediately before dispatching an
    // actor callback. FUN_1000_13de leaves the absolute 32-bit X difference
    // in a stack slot later reused as FUN_1000_952c's uninitialized local.
    // The helper has an explicit -32768 clamp before the normal two-word
    // negation.
    const auto difference = static_cast<std::uint32_t>(
        static_cast<std::uint32_t>(target)
        - static_cast<std::uint32_t>(source)
    );
    const auto magnitude = difference == 0xFFFF8000U
        ? 0x00007FFFU
        : (difference & 0x80000000U) != 0
            ? static_cast<std::uint32_t>(0U - difference)
            : difference;
    return static_cast<std::uint8_t>(magnitude);
}

}  // namespace

std::uint8_t advanceType11Behavior(Level1Entity& item, const Level1Entity& player,
    StellarRng& rng, const std::function<bool(Level1Entity&)>& fire) {
    auto behavior = item.stateBytes4dTo52[5];
    if (
        item.collisionContactAllocationIndex
            != static_cast<std::size_t>(-1)
        || item.collisionContactIsPlayer
    ) {
        item.collisionContactAllocationIndex =
            static_cast<std::size_t>(-1);
        item.collisionContactIsPlayer = false;
        behavior = 5;
    }
    if (behavior == 0) {
        if (item.behaviorTimer == 0) {
            item.forwardVelocity = 270;
            const auto choice = rng.next();
            if (static_cast<std::uint8_t>(choice) < 90) {
                item.yawVelocity = 0;
            } else {
                auto turn = static_cast<std::int16_t>(
                    static_cast<std::int16_t>(choice) % 256
                    + 256
                );
                if ((choice & 0x0100U) != 0) {
                    turn = static_cast<std::int16_t>(-turn);
                }
                item.yawVelocity = turn;
            }
            item.behaviorTimer = static_cast<std::uint8_t>(
                (choice >> 8U) & 0x7FU
            );
        } else {
            --item.behaviorTimer;
            if (item.behaviorTimer == 0) {
                behavior = 1;
            }
        }
    } else if (behavior == 1) {
        const auto difference = static_cast<std::int16_t>(
            item.targetHeading - item.yaw
        );
        item.yawVelocity = static_cast<std::int16_t>(
            difference < -271 ? -271
            : difference > 271 ? 271
            : difference
        );
        const auto callbackScratch = rng.next();
        if (
            static_cast<std::uint8_t>(callbackScratch) < 8
        ) {
            behavior = 2;
        }
    } else if (behavior == 2) {
        item.position.z -= 50;
        if (item.position.z < 200) {
            behavior = 3;
        }
    } else if (behavior == 3) {
        const auto difference = static_cast<std::int16_t>(
            item.targetHeading - item.yaw
        );
        item.yawVelocity = static_cast<std::int16_t>(
            difference < -271 ? -271
            : difference > 271 ? 271
            : difference
        );
        if (
            item.presenterDistance <= 0x6000U
            && item.weaponSlotTimers[0] == 0
        ) {
            const auto magnitude = difference < 0
                ? -static_cast<std::int32_t>(difference)
                : static_cast<std::int32_t>(difference);
            if (magnitude < 0x100) {
                if (fire(item)) {
                    behavior = 4;
                    item.behaviorTimer = 0x3F;
                }
            } else if (
                presenterAbsoluteXLowByte(
                    player.position.x,
                    item.position.x
                ) < 2
            ) {
                behavior = 5;
            }
        }
    } else if (behavior == 4) {
        if (item.behaviorTimer == 0) {
            behavior = 5;
        } else {
            --item.behaviorTimer;
        }
    } else if (behavior == 5) {
        item.forwardVelocity = 270;
        if (item.position.z <= 1000) {
            item.position.z += 100;
            if (item.position.z >= 1000) {
                item.position.z = 1000;
                behavior = 0;
            }
        }
    }
    // FUN_1000_952c decrements the first weapon-slot timer at its
    // common callback tail, including the invocation that launched
    // the projectile through FUN_1000_98fa/FUN_1000_995e.
    if (item.weaponSlotTimers[0] != 0) {
        --item.weaponSlotTimers[0];
    }
    item.roll = static_cast<std::uint16_t>(
        item.roll + arithmeticShiftRight(
            -(
                static_cast<std::int32_t>(item.yawVelocity) * 0x10
                + static_cast<std::int16_t>(item.roll)
            ),
            4
        )
    );
    return behavior;
}

Level1State::Level1State() noexcept
    : player_(level1Player),
      entities_(level1Entities.begin(), level1Entities.end()) {
    entities_[0].weaponSlotCount = 2;
    entities_[1].weaponSlotCount = 1;
    entities_[1].modelAnimationVariant = 1;
    entities_[1].presentationModelAnimationVariant = 1;
    entities_[1].primaryType11AnimationSelector = true;
    allocationList_.reserve(
        entities_.size() + projectilePoolSize + deathEffectPoolSize
    );
    objectListOrder_.reserve(
        entities_.size() + projectilePoolSize + deathEffectPoolSize
    );
    activeListOrder_.reserve(entities_.size());
    semanticActiveListOrder_.reserve(entities_.size());
    for (std::size_t index = 0; index < entities_.size(); ++index) {
        allocationList_.push_back({ObjectKind::entity, index});
        objectListOrder_.push_back({ObjectKind::entity, index});
        activeListOrder_.push_back(index);
        semanticActiveListOrder_.push_back(index);
    }
    for (std::size_t slot = 0; slot < projectiles_.size(); ++slot) {
        projectiles_[slot].poolSlot = slot;
    }
    for (std::uint32_t call = 0; call < 29; ++call) {
        rng_.next();
    }
}

namespace {

class Level1ReloadLoader final : public CampaignActorLoader {
public:
    Level1ReloadLoader(std::vector<Level1Entity>& actors, StellarRng& rng)
        : actors_(actors), rng_(rng) {}

    Actor allocateOrdinary(const std::uint8_t type) override {
        if (type != 1 && type != 11) {
            throw std::invalid_argument("unsupported Level1 retry actor");
        }
        auto actor = create(type);
        auto& item = actors_[actor];
        item.stateBytes4dTo52[0] = 0xff;
        rng_.next(); //4862 position is subsequently replaced by the level table.
        if (type == 11) {
            item.position.z = 1000;
            item.stateBytes4dTo52[2] = 5;
            item.primaryType11AnimationSelector = true;
        }
        rng_.next(); //4370 heading is subsequently replaced by the level table.
        const auto defaults = campaignOrdinaryDefaults(type);
        item.forwardVelocity = *defaults.speed;
        item.stateBytes4dTo52[1] = defaults.health;
        item.stateBytes4dTo52[4] = defaults.behavior;
        item.weaponSlotCount = defaults.weaponSlots;
        return actor;
    }

    Actor allocateFixed(const std::uint8_t type) override {
        if (type != 29) {
            throw std::invalid_argument("unsupported Level1 retry fixed actor");
        }
        const auto actor = create(type);
        actors_[actor].stateBytes4dTo52[0] = 0xfe;
        actors_[actor].stateBytes4dTo52[1] = 100;
        return actor;
    }

    void write(const Actor actor, const CampaignActorField field, const std::int16_t value) override {
        auto& item = actors_.at(actor);
        switch (field) {
        case CampaignActorField::auxiliaryWord1a: break;
        case CampaignActorField::classification: item.stateBytes4dTo52[0] = static_cast<std::uint8_t>(value); break;
        case CampaignActorField::health: item.stateBytes4dTo52[1] = static_cast<std::uint8_t>(value); break;
        case CampaignActorField::animation: item.stateBytes4dTo52[2] = static_cast<std::uint8_t>(value); break;
        case CampaignActorField::behavior: item.stateBytes4dTo52[4] = static_cast<std::uint8_t>(value); break;
        case CampaignActorField::height: item.position.z = value; break;
        }
    }

    void setPosition(const Actor actor, const std::int16_t x, const std::int16_t y) override {
        actors_.at(actor).position.x = campaignCoordinate(x);
        actors_.at(actor).position.y = campaignCoordinate(y);
    }
    void setYaw(const Actor actor, const std::uint16_t yaw) override { actors_.at(actor).yaw = yaw; }
    void presentFixed(Actor) override { throw std::invalid_argument("unsupported Level1 retry fixed presenter"); }
    void aliasAnimation(Actor) override { throw std::invalid_argument("unsupported Level1 retry animation alias"); }

private:
    std::vector<Level1Entity>& actors_;
    StellarRng& rng_;

    Actor create(const std::uint8_t type) {
        // Bound immutable model radii for the original Level1 registry profile.
        constexpr std::uint32_t type1Radius = 1342;
        constexpr std::uint32_t type11Radius = 748;
        constexpr std::uint32_t type29Radius = 862;
        Level1Entity item{};
        item.allocationIndex = actors_.size();
        item.type = type;
        item.collisionRadius = type == 1 ? type1Radius : type == 11 ? type11Radius : type29Radius;
        actors_.push_back(item);
        return actors_.size() - 1;
    }
};

}

void Level1State::reloadAfterFailure(const StellarRng& retainedFailureRng) {
    rng_ = retainedFailureRng;
    entities_.clear();
    Level1ReloadLoader loader(entities_, rng_);
    initializeCampaignActors(campaignLevelDefinition(0, 0).table, loader);
    std::array<GroundPoint, Level1GroundState::pointCount> points{};
    for (auto& point : points) {
        resetGroundPoint(point, rng_);
    }
    ground_ = Level1GroundState(points);
    constexpr std::uint32_t playerRadius = 723;
    player_ = {};
    player_.allocationIndex = entities_.size();
    player_.collisionRadius = playerRadius;
    player_.stateBytes4dTo52[1] = 1;
    allocationList_.clear();
    objectListOrder_.clear();
    activeListOrder_.clear();
    semanticActiveListOrder_.clear();
    for (std::size_t index = 0; index < entities_.size(); ++index) {
        allocationList_.push_back({ObjectKind::entity, index});
        objectListOrder_.push_back({ObjectKind::entity, index});
        activeListOrder_.push_back(index);
        semanticActiveListOrder_.push_back(index);
    }
    projectiles_ = {};
    for (std::size_t slot = 0; slot < projectiles_.size(); ++slot) {
        projectiles_[slot].poolSlot = slot;
        projectiles_[slot].stateBytes4dTo54[0] = 0xfd;
    }
    deathEffects_ = {};
    for (auto& effect : deathEffects_) {
        effect.entity.flags = 0x88;
        effect.entity.stateBytes4dTo52[0] = 0xfc;
    }
    playerWeaponCooldown_ = 0;
    playerProjectileTimers_ = {};
    audioEvents_.clear();
    score_ = 0;
    playerArmor_ = 680;
    resetCampaignTransientState();
    levelIndex_ = 0;
    loopTick_ = 0;
    type11AnimationVariant_ = 0;
    projectionListOrigin_ = {};
    missionActive_ = true;
    failureState_ = false;
    failurePending_ = false;
    failureTailConsumed_ = false;
    failureTailProjectileActiveAtStart_.fill(false);
    successState_ = false;
    retainSuccessContact_ = false;
    successContactPending_ = false;
    successContactIndex_ = static_cast<std::size_t>(-1);
    successContactOrder_.clear();
    successContactProjectileActiveAtStart_.fill(false);
    exitState_ = false;
    activeEnemyCount_ = 2;
    spawnedEnemyCount_ = 2;
    lastSpawnCheckpoint_ = {};
    lastSpawnTick_ = 0;
    lastSpawnRng_ = rng_;
    hasLastSpawnCheckpoint_ = false;
    completionObjectAllocationIndex_ = static_cast<std::size_t>(-1);
    postWaveObjectAllocationIndex_ = static_cast<std::size_t>(-1);
    hasAdvanced_ = false;
    nativeInitialListPending_ = false;
    ordinaryTailPending_ = false;
    ordinaryTailConsumed_ = false;
    pendingPostBoundaryInput_ = {};
    playerCollisionMomentum_ = 0;
}

void Level1State::resetCampaignTransientState() noexcept {
    //48d4 omits12d4 (system3) and retains1620 last-destroyed type.
    constexpr std::size_t retainedTimerIndex = 3;
    for (std::size_t index = 0; index < systemTimers_.size(); ++index) {
        if (index != retainedTimerIndex) {
            systemTimers_[index] = 0;
        }
    }
    consecutiveDestroyedTypeCount_ = 0;
}

void Level1State::advanceEntityKinematics() noexcept {
    for (auto& item : entities_) {
        stellar7::advanceEntityKinematics(item);
    }
}

void Level1State::finishFrame() {
    const auto& input = pendingPostBoundaryInput_;
    if (!ordinaryTailPending_ || ordinaryTailConsumed_ || !hasAdvanced_
        || !missionActive_ || failurePending_ || successContactPending_ || playerArmor_ < 0
        || input.left || input.right || input.forward || input.reverse || input.fire || input.exit
        || selectedSystem_ != -1 || playerWeaponCooldown_ != 0
        || std::any_of(playerProjectileTimers_.begin(), playerProjectileTimers_.end(),
            [](const auto timer) { return timer != 0; })
        || std::any_of(systemTimers_.begin(), systemTimers_.end(),
            [](const auto timer) { return timer != 0; })) {
        throw std::invalid_argument("Level1 completion requires an unconsumed neutral ordinary display");
    }
    std::array<bool, projectilePoolSize> active{};
    for (std::size_t slot = 0; slot < active.size(); ++slot) {
        active[slot] = projectiles_[slot].active();
    }
    advancePostBoundaryTail(active);
    ordinaryTailPending_ = false;
    ordinaryTailConsumed_ = true;
}

bool Level1State::pendingFailureTail() const noexcept {
    constexpr std::int16_t minimumSupportedArmor = -128;
    return failurePending_ && !failureTailConsumed_ && missionActive_ && hasAdvanced_
        && playerArmor_ >= minimumSupportedArmor && playerArmor_ < 0
        && (loopTick_ & 0x000fU) != 0
        && selectedSystem_ == -1 && !pendingPostBoundaryInput_.exit
        && std::all_of(systemTimers_.begin(), systemTimers_.end(),
            [](const auto timer) { return timer == 0; });
}

void Level1State::finishFailureTail() {
    if (!pendingFailureTail()) {
        throw std::invalid_argument("Level1 failure tail requires an admitted unconsumed lethal display");
    }
    advancePostBoundaryTail(failureTailProjectileActiveAtStart_);
    failureTailConsumed_ = true;
}

bool Level1State::pendingSuccessContact() const noexcept {
    return successContactPending_ && !missionActive_ && successState_
        && successContactOrder_ == activeListOrder_
        && admitsSuccessContact(successContactIndex_, successContactOrder_)
        && std::none_of(successContactProjectileActiveAtStart_.begin(),
            successContactProjectileActiveAtStart_.end(), [](const auto active) { return active; });
}

bool Level1State::admitsSuccessContact(const std::size_t pickupIndex,
    const std::vector<std::size_t>& updateOrder) const noexcept {
    if (pickupIndex >= entities_.size() || updateOrder != activeListOrder_
        || spawnedEnemyCount_ < maximumEnemyCount_ || selectedSystem_ != -1
        || postWaveObjectAllocationIndex_ >= entities_.size()
        || failurePending_ || playerArmor_ < 0
        || std::any_of(systemStatusLevels_.begin(), systemStatusLevels_.end(),
            [](const auto status) { return status != 3; })
        || std::any_of(systemTimers_.begin(), systemTimers_.end(),
            [](const auto timer) { return timer != 0; })
        || std::any_of(projectiles_.begin(), projectiles_.end(),
            [](const auto& projectile) { return projectile.active(); })
        || std::any_of(deathEffects_.begin(), deathEffects_.end(),
            [](const auto& effect) { return effect.listed; })) {
        return false;
    }
    const auto liveCount = std::count_if(entities_.begin(), entities_.end(),
        [](const auto& actor) { return actor.active(); });
    if (static_cast<std::size_t>(liveCount) != updateOrder.size()) {
        return false;
    }
    std::size_t pickups = 0;
    for (std::size_t order = 0; order < updateOrder.size(); ++order) {
        const auto index = updateOrder[order];
        if (index >= entities_.size()
            || std::find(updateOrder.begin(), updateOrder.begin() + order, index) != updateOrder.begin() + order) {
            return false;
        }
        const auto& actor = entities_[index];
        if (!actor.active() || static_cast<std::int8_t>(actor.stateBytes4dTo52[0]) != -2) {
            return false;
        }
        if (actor.type == 25) {
            ++pickups;
            constexpr std::array<std::int16_t, 4> kPickupYawSpins{
                2560, -2816, 3072, -3328
            };
            if (index != pickupIndex || !actor.collisionContactIsPlayer || !collides(actor, player_)
                || actor.collisionRadius != 897 || actor.forwardVelocity != 0
                || actor.xVelocity != 0 || actor.yVelocity != 0 || actor.zVelocity != 0
                || actor.pitchVelocity != 0 || actor.rollVelocity != 0
                || std::find(kPickupYawSpins.begin(), kPickupYawSpins.end(), actor.yawVelocity)
                    == kPickupYawSpins.end()) {
                return false;
            }
            continue;
        }
        if (actor.type != 29 || collides(actor, player_) || actor.pitchVelocity != 0
            || actor.rollVelocity != 0 || actor.yawVelocity != 0 || actor.xVelocity != 0
            || actor.yVelocity != 0 || actor.zVelocity != 0 || actor.forwardVelocity != 0) {
            return false;
        }
    }
    return pickups == 1;
}

void Level1State::finishSuccessContact(const Level1Input& input) {
    if (!pendingSuccessContact() || input.fire || input.exit) {
        throw std::invalid_argument("Level1 success return requires its retained static contact");
    }
    // FE actors other than Type 42 skip the remaining 0784 integration.
    advancePickupOscillator(entities_[successContactIndex_]); // 0A9A, after 6F52 returns.
    pendingPostBoundaryInput_ = input;
    applyPostBoundaryInput(successContactProjectileActiveAtStart_);
    completePostBoundaryTail(); // 0312..03D1, including periodic 033C/033F.
    successContactPending_ = false;
    successContactIndex_ = static_cast<std::size_t>(-1);
    successContactOrder_.clear();
}

void Level1State::advancePickupOscillator(Level1Entity& item) noexcept {
    auto& presentation = item.stateBytes4dTo52[2];
    auto& direction = item.stateBytes4dTo52[5];
    if (static_cast<std::int8_t>(presentation) < 1) {
        direction = 1;
    } else if (static_cast<std::int8_t>(presentation) > 2) {
        direction = 0xFF;
    }
    presentation = static_cast<std::uint8_t>(
        presentation + static_cast<std::int8_t>(direction)
    );
}

void Level1State::advanceAnimatedEntityPresenters() noexcept {
    for (auto& item : entities_) {
        if (
            !item.active()
            || (item.type != 11 && item.type != 44)
            || !item.animatedModelPresenterEnabled
        ) {
            continue;
        }
        auto& countdown = item.stateBytes4dTo52[3];
        if (countdown == 0) {
            countdown = item.stateBytes4dTo52[2];
            item.modelAnimationVariant = static_cast<std::uint8_t>(
                (item.modelAnimationVariant + 1U) & 7U
            );
            if (
                item.type == 11
                && item.primaryType11AnimationSelector
            ) {
                type11AnimationVariant_ = item.modelAnimationVariant;
            }
        } else {
            --countdown;
        }
    }
}

void Level1State::applyPostBoundaryInput(
    const std::array<bool, projectilePoolSize>& projectileActiveAtStart) noexcept {
    const auto& deferred = pendingPostBoundaryInput_;
    const bool hasDirectionalInput = (
        deferred.left
        || deferred.right
        || deferred.forward
        || deferred.reverse
    );
    if (deferred.left || deferred.right) {
        const auto adjusted = static_cast<std::int32_t>(
            player_.yawVelocity
        ) + (deferred.left ? 0x50 : -0x50);
        player_.yawVelocity = static_cast<std::int16_t>(
            adjusted < -0x200 ? -0x200
            : adjusted > 0x200 ? 0x200
            : adjusted
        );
    } else if (player_.yawVelocity > 0) {
        player_.yawVelocity = static_cast<std::int16_t>(
            player_.yawVelocity < 0x50
                ? 0
                : player_.yawVelocity - 0x50
        );
    } else if (player_.yawVelocity < 0) {
        player_.yawVelocity = static_cast<std::int16_t>(
            player_.yawVelocity > -0x50
                ? 0
                : player_.yawVelocity + 0x50
        );
    }
    if (deferred.forward || deferred.reverse) {
        const auto adjusted = static_cast<std::int32_t>(
            player_.forwardVelocity
        ) + (deferred.reverse ? -10 : 10);
        player_.forwardVelocity = static_cast<std::int16_t>(
            adjusted < -200 ? -200
            : adjusted > 200 ? 200
            : adjusted
        );
    } else if (
        !hasDirectionalInput
        && player_.forwardVelocity > 0
    ) {
        player_.forwardVelocity = static_cast<std::int16_t>(
            player_.forwardVelocity < 10
                ? 0
                : player_.forwardVelocity - 10
        );
    } else if (
        !hasDirectionalInput
        && player_.forwardVelocity < 0
    ) {
        player_.forwardVelocity = static_cast<std::int16_t>(
            player_.forwardVelocity > -10
                ? 0
                : player_.forwardVelocity + 10
        );
    }
    if (deferred.fire) {
        firePrimary();
    }
    const auto playerDeltaX = player_.position.x;
    const auto playerDeltaY = player_.position.y;
    if (playerDeltaX != 0 || playerDeltaY != 0) {
        for (auto& item : entities_) {
            // FUN_1000_0c6a explicitly leaves the Type 7 completion
            // object in its world-relative coordinate frame.
            if (item.active() && item.type != 7) {
                item.position.x -= playerDeltaX;
                item.position.y -= playerDeltaY;
            }
        }
        // FUN_1000_0c6a walks the existing active-object pointer list.
        // A projectile allocated by Fire immediately before that call is
        // not included until the subsequent list rebuild.
        for (std::size_t slot = 0; slot < projectiles_.size(); ++slot) {
            auto& projectile = projectiles_[slot];
            if (projectileActiveAtStart[slot] && projectile.active()) {
                projectile.position.x -= playerDeltaX;
                projectile.position.y -= playerDeltaY;
            }
        }
        for (auto& effect : deathEffects_) {
            if (effect.listed) {
                effect.entity.position.x -= playerDeltaX;
                effect.entity.position.y -= playerDeltaY;
            }
        }
        player_.position.x = 0;
        player_.position.y = 0;
    }
    if (deferred.exit) {
        missionActive_ = false;
        exitState_ = true;
    }
}

void Level1State::completePostBoundaryTail() noexcept {
    if ((loopTick_ & 0x000FU) == 0) {
        // FUN_1000_0c6a runs after the captured main-view boundary.
        // Apply its periodic coordinate normalization here with the
        // other deferred post-boundary work, before newly spawned
        // objects enter the active list.
        for (auto& item : entities_) {
            if (item.active() && item.type != 7) {
                normalizeWorldPosition(item.position);
            }
        }
        for (auto& projectile : projectiles_) {
            if (projectile.active() && projectile.type != 7) {
                normalizeWorldPosition(projectile.position);
            }
        }
        for (auto& effect : deathEffects_) {
            if (effect.listed) {
                normalizeWorldPosition(effect.entity.position);
            }
        }
        runPeriodicSpawnCheck();
        runCompletionCheck();
    }
    // FUN_1000_0048 dispatches deferred Fire through FUN_1000_9e10
    // before decrementing the shared cooldown and both player slot
    // timers at 1000:0372..03cf. A newly launched 9/32 weapon state is
    // therefore already 8/31 at the next main-view boundary.
    if (playerWeaponCooldown_ != 0) {
        --playerWeaponCooldown_;
    }
    for (auto& timer : playerProjectileTimers_) {
        if (timer != 0) {
            --timer;
        }
    }
    ++loopTick_;
}

void Level1State::advancePostBoundaryTail(
    const std::array<bool, projectilePoolSize>& projectileActiveAtStart) noexcept {
    // The radar callback and queued input follow the completed main display.
    advanceAnimatedEntityPresenters();
    applyPostBoundaryInput(projectileActiveAtStart);
    if (!missionActive_) {
        return;
    }
    completePostBoundaryTail();
}

void Level1State::advanceTick(const Level1Input& input) noexcept {
    advanceTickImpl(input,nullptr);
}

void Level1State::advanceTickWithRenderer(const Level1Input& input, Level1FrameRenderer& renderer) {
    advanceTickImpl(input,&renderer);
}

void Level1State::advanceTickImpl(const Level1Input& input, Level1FrameRenderer* renderer) {
    if (failurePending_) {
        failurePending_ = false;
        failureTailConsumed_ = true;
        if (missionActive_) {
            missionActive_ = false;
            failureState_ = true;
        }
        return;
    }
    if (!missionActive_) {
        return;
    }
    const bool firstAdvance = !hasAdvanced_;
    std::array<bool, projectilePoolSize> projectileActiveAtStart{};
    for (std::size_t slot = 0; slot < projectiles_.size(); ++slot) {
        projectileActiveAtStart[slot] = projectiles_[slot].active();
    }
    std::array<bool, deathEffectPoolSize> deathEffectListedAtStart{};
    std::array<bool, deathEffectPoolSize>
        deathEffectPendingAllocationRemoval{};
    for (std::size_t slot = 0; slot < deathEffects_.size(); ++slot) {
        deathEffectListedAtStart[slot] = deathEffects_[slot].listed;
    }
    bool deathEffectMembershipChanged = false;
    bool deploySuccessPickup = false;
    Level1Entity successPickupSource{};
    std::vector<bool> entityActiveAtUpdateStart;
    entityActiveAtUpdateStart.reserve(entities_.size());
    for (const auto& item : entities_) {
        entityActiveAtUpdateStart.push_back(item.active());
    }
    if (!firstAdvance && !ordinaryTailConsumed_) {
        advancePostBoundaryTail(projectileActiveAtStart);
        if (!missionActive_) {
            return;
        }
    } else if (firstAdvance) {
        hasAdvanced_ = true;
    }
    ordinaryTailPending_ = false;
    ordinaryTailConsumed_ = false;

    // FUN_1000_0048 copies the rebound value at data offset 0x74 back into
    // the player immediately before FUN_1000_0784, then halves that stored
    // value toward zero. Post-boundary controls modify the player velocity
    // without erasing this collision momentum.
    if (playerCollisionMomentum_ != 0) {
        player_.forwardVelocity = playerCollisionMomentum_;
        playerCollisionMomentum_ = playerCollisionMomentum_ < 0
            ? static_cast<std::int16_t>(
                -((-playerCollisionMomentum_) >> 1)
            )
            : static_cast<std::int16_t>(
                playerCollisionMomentum_ >> 1
            );
    }

    std::vector<FixedPosition> entityPositionsAtUpdateStart;
    entityPositionsAtUpdateStart.reserve(entities_.size());
    std::vector<std::uint16_t> entityTargetHeadingsAtUpdateStart;
    entityTargetHeadingsAtUpdateStart.reserve(entities_.size());
    for (const auto& item : entities_) {
        entityPositionsAtUpdateStart.push_back(item.position);
        entityTargetHeadingsAtUpdateStart.push_back(item.targetHeading);
    }
    constexpr auto absentObjectOrder =
        static_cast<std::size_t>(-1);
    std::vector<std::size_t> entityObjectOrder(
        entities_.size(),
        absentObjectOrder
    );
    std::array<std::size_t, projectilePoolSize> projectileObjectOrder{};
    projectileObjectOrder.fill(absentObjectOrder);
    for (
        std::size_t order = 0;
        order < objectListOrder_.size();
        ++order
    ) {
        const auto& reference = objectListOrder_[order];
        if (
            reference.kind == ObjectKind::entity
            && reference.index < entityObjectOrder.size()
        ) {
            entityObjectOrder[reference.index] = order;
        } else if (
            reference.kind == ObjectKind::projectile
            && reference.index < projectileObjectOrder.size()
        ) {
            projectileObjectOrder[reference.index] = order;
        }
    }

    std::vector<std::size_t> updateOrder;
    updateOrder.reserve(activeListOrder_.size());
    for (const auto index : activeListOrder_) {
        if (
            index < entityActiveAtUpdateStart.size()
            && entityActiveAtUpdateStart[index]
            && entities_[index].active()
        ) {
            updateOrder.push_back(index);
        }
    }
    std::vector<std::size_t> callbackOrder;
    callbackOrder.reserve(activeListOrder_.size());
    for (const auto index : activeListOrder_) {
        if (
            index < entityActiveAtUpdateStart.size()
            && entityActiveAtUpdateStart[index]
            && entities_[index].active()
        ) {
            callbackOrder.push_back(index);
        }
    }

    advanceKinematics(player_);
    Level1Entity* playerCollision = nullptr;
    // FUN_1000_0c0a scans the active list after player integration, writes
    // the player pointer to every colliding actor's +0x5f contact field, and
    // returns the last collision for the player backout response.
    for (const auto index : activeListOrder_) {
        if (
            index >= entityActiveAtUpdateStart.size()
            || !entityActiveAtUpdateStart[index]
            || !entities_[index].active()
        ) {
            continue;
        }
        auto& item = entities_[index];
        const auto flags = static_cast<std::int8_t>(
            item.stateBytes4dTo52[0]
        );
        if (
            flags == -3
            || flags == -4
            || !collides(item, player_)
        ) {
            continue;
        }
        item.collisionContactAllocationIndex =
            static_cast<std::size_t>(-1);
        item.collisionContactIsPlayer = true;
        playerCollision = &item;
    }
    if (playerCollision != nullptr) {
        if (playerCollision->type == 25 || playerCollision->type == 26) {
            if (retainSuccessContact_ && playerCollision->type == 25) {
                const auto pickupIndex = static_cast<std::size_t>(playerCollision - entities_.data());
                if (admitsSuccessContact(pickupIndex, updateOrder)) {
                    successContactIndex_ = pickupIndex;
                    successContactOrder_ = updateOrder;
                    successContactProjectileActiveAtStart_ = projectileActiveAtStart;
                    successContactPending_ = true;
                    missionActive_ = false;
                    successState_ = true;
                    return;
                }
            }
            if (playerCollision->type == 26) {
                levelIndex_ = static_cast<std::int16_t>(levelIndex_ + 2);
            }
            missionActive_ = false;
            successState_ = true;
        } else if (playerCollision->type == 42) {
            // FUN_1000_0784 dispatches a Type 42 contact to FUN_1000_90f2
            // and removes the bonus object instead of applying the normal
            // player backout/rebound response. The payload byte selects the
            // repaired cockpit system; every pickup also awards 100 points
            // and emits sound event 0x18.
            const auto payload = playerCollision->stateBytes4dTo52[5];
            const auto allocationIndex = playerCollision->allocationIndex;
            playerCollision->stateBytes4dTo52[1] = 0;
            playerCollision->collisionContactIsPlayer = false;
            score_ += 100;
            audioEvents_.push_back({loopTick_, 24});
            repairBonusSystem(payload);
            removeAllocationObject(
                ObjectKind::entity,
                allocationIndex
            );
        } else {
            if (player_.forwardVelocity == 0) {
                player_.forwardVelocity = 10;
            }
            do {
                reverseTranslation(player_);
            } while (collides(*playerCollision, player_));
            player_.forwardVelocity = static_cast<std::int16_t>(
                -player_.forwardVelocity
            );
            playerCollisionMomentum_ = player_.forwardVelocity;
        }
    }
    for (const auto sourceIndex : updateOrder) {
        auto& source = entities_[sourceIndex];
        const auto sourceFlags = static_cast<std::int8_t>(
            source.stateBytes4dTo52[0]
        );
        // FUN_1000_0784 does not run the normal integration/collision-source
        // path for -2 objects. Type 42 is the sole exception: it receives
        // kinematics but still does not call FUN_1000_0c0a as a source.
        // These objects remain eligible targets in another source's scan.
        if (sourceFlags == -2) {
            if (source.type == 42) {
                advanceKinematics(source);
            }
            continue;
        }
        if (sourceFlags == -3 || sourceFlags == -4) {
            continue;
        }
        advanceKinematics(source);
        const bool sourceCollidedWithPlayer =
            collides(player_, source);
        if (sourceCollidedWithPlayer) {
            if (source.type == 25 || source.type == 26) {
                missionActive_ = false;
                successState_ = true;
            }
        }
        bool sourceCollided = sourceCollidedWithPlayer;
        bool lastTargetIsPlayer = sourceCollidedWithPlayer;
        std::size_t lastTarget = static_cast<std::size_t>(-1);
        std::int16_t lastTargetType = player_.type;
        // FUN_1000_0784 skips FUN_1000_0c0a when the newly integrated
        // source already intersects the player. Otherwise FUN_1000_0c0a
        // scans the active list, writes the source pointer to every target,
        // and returns the final collision.
        if (!sourceCollidedWithPlayer) {
            for (const auto targetIndex : updateOrder) {
                if (sourceIndex == targetIndex) {
                    continue;
                }
                auto& target = entities_[targetIndex];
                const auto targetFlags = static_cast<std::int8_t>(
                    target.stateBytes4dTo52[0]
                );
                if (
                    !target.active()
                    || targetFlags == -3
                    || targetFlags == -4
                    || !collides(source, target)
                ) {
                    continue;
                }
                target.collisionContactAllocationIndex =
                    source.allocationIndex;
                target.collisionContactIsPlayer = false;
                sourceCollided = true;
                lastTargetIsPlayer = false;
                lastTarget = target.allocationIndex;
                lastTargetType = target.type;
            }
        }
        if (
            sourceCollided
            && source.stateBytes4dTo52[4] != 3
        ) {
            source.collisionContactAllocationIndex =
                lastTargetIsPlayer
                    ? static_cast<std::size_t>(-1)
                    : lastTarget;
            source.collisionContactIsPlayer = lastTargetIsPlayer;
            if (source.stateBytes4dTo52[4] == 1) {
                source.stateBytes4dTo52[5] = 4;
                source.behaviorTimer = 0;
            }
            // FUN_1000_0e50 dispatches state-5 actors to FUN_1000_adcc.
            // In Level 1, a non-Type-7 actor damages its non-Type-7 contact
            // by 20 immediately, even while its behavior cooldown is live.
            if (
                levelIndex_ == 0
                && source.stateBytes4dTo52[4] == 5
                && source.type != 7
                && !lastTargetIsPlayer
                && lastTarget < entities_.size()
                && entities_[lastTarget].active()
                && entities_[lastTarget].type != 7
            ) {
                auto& target = entities_[lastTarget];
                auto& health = target.stateBytes4dTo52[1];
                if (health <= 20) {
                    const auto destroyedType = target.type;
                    const auto destroyed = target;
                    registerDestroyedTypeBonus(
                        destroyedType,
                        destroyed
                    );
                    deathEffectMembershipChanged =
                        spawnDeathEffect(destroyed)
                        || deathEffectMembershipChanged;
                    target.stateBytes4dTo52[1] = 0;
                    removeAllocationObject(
                        ObjectKind::entity,
                        lastTarget
                    );
                    if (
                        static_cast<std::int8_t>(
                            destroyed.stateBytes4dTo52[0]
                        ) == -1
                    ) {
                        --activeEnemyCount_;
                    }
                    if (
                        destroyedType >= 0
                        && static_cast<std::size_t>(destroyedType)
                            < scoreByType.size()
                    ) {
                        score_ += scoreByType[
                            static_cast<std::size_t>(destroyedType)
                        ];
                    }
                    if (destroyedType == 44) {
                        audioEvents_.push_back({loopTick_, 12});
                        score_ += 1000;
                        deploySuccessPickup = true;
                        successPickupSource = destroyed;
                        successPickupSource.targetHeading =
                            entityTargetHeadingsAtUpdateStart[lastTarget];
                    }
                } else {
                    health = static_cast<std::uint8_t>(health - 20U);
                    deathEffectMembershipChanged =
                        spawnTransientEffect(target, 0, 12)
                        || deathEffectMembershipChanged;
                    audioEvents_.push_back({loopTick_, 5});
                }
            }
            if (
                lastTargetType != 43
                && source.type != 13
                && source.type != 47
                && (lastTargetType < 46 || source.type < 44)
            ) {
                // FUN_1000_3d10 removes only the translation applied by
                // FUN_1000_3b3a. Angular integration remains in effect.
                reverseTranslation(source);
            }
        }
    }
    for (const auto index : callbackOrder) {
        auto& item = entities_[index];
        item.presenterDistance = approximatePlanarDistance(
            player_.position,
            item.position
        );
        item.targetHeading = presenterTargetHeading(
            item.position,
            player_.position
        );
    }
    for (std::size_t slot = 0; slot < deathEffects_.size(); ++slot) {
        auto& effect = deathEffects_[slot];
        if (!deathEffectListedAtStart[slot] || !effect.listed) {
            continue;
        }
        auto& item = effect.entity;
        if (!item.active()) {
            // FUN_1000_9298 refreshes every pointer still carried by
            // 0x45d2 before FUN_1000_3768 drops the zero-frame pool item.
            // Those presenter fields therefore survive into the next reuse.
            item.presenterDistance = approximatePlanarDistance(
                player_.position,
                item.position
            );
            item.targetHeading = presenterTargetHeading(
                item.position,
                player_.position
            );
            effect.listed = false;
            // The shared update pass removes expired projectiles before
            // FUN_1000_9298 retires zero-frame effect presenters.  Defer the
            // swap-back mutation so simultaneous projectile/effect expiry
            // preserves the original allocation-array order.
            deathEffectPendingAllocationRemoval[slot] = true;
            deathEffectMembershipChanged = true;
            continue;
        }
        item.presenterDistance = approximatePlanarDistance(
            player_.position,
            item.position
        );
        item.targetHeading = presenterTargetHeading(
            item.position,
            player_.position
        );
        const auto nextFrame = static_cast<std::uint8_t>(
            item.stateBytes4dTo52[1] + 1U
        );
        if (nextFrame >= item.stateBytes4dTo52[4]) {
            item.stateBytes4dTo52[1] = 0;
            item.stateBytes4dTo52[4] = 0;
            // The pool item leaves 0x49f8 as soon as its terminal frame
            // clears, but its pointer remains in the current 0x45d2 list
            // until the next rebuild.  Keep `listed` set for that stale
            // presenter boundary while retiring the allocation now.
            deathEffectPendingAllocationRemoval[slot] = true;
        } else {
            item.stateBytes4dTo52[1] = nextFrame;
        }
    }
    bool deployPostWaveObject = false;
    Level1Entity postWaveSource{};
    const auto advanceBehavior = [
        this,
        &deployPostWaveObject,
        &postWaveSource,
        &deathEffectMembershipChanged,
        &deploySuccessPickup,
        &successPickupSource,
        &entityTargetHeadingsAtUpdateStart
    ](const std::size_t index) noexcept {
        auto& item = entities_[index];
        auto& behavior = item.stateBytes4dTo52[5];
        if (item.type == 1 || item.type == 3) {
            const auto choice2Lower = static_cast<std::uint8_t>(
                item.type == 1 ? 50 : 48
            );
            const auto choice3Lower = static_cast<std::uint8_t>(
                item.type == 1 ? 102 : 128
            );
            const auto turnLimit = static_cast<std::int16_t>(
                item.type == 1 ? 143 : 399
            );
            const auto cruiseSpeed = static_cast<std::int16_t>(
                item.type == 1 ? 225 : 216
            );
            const auto behavior2Reset = static_cast<std::uint8_t>(
                item.type == 1 ? 12 : 10
            );
            const auto behavior3Reset = static_cast<std::uint8_t>(
                item.type == 1 ? 5 : 6
            );
            const bool insideAttackRange = (
                item.position.z < 0
                || item.presenterDistance <= 0x6000U
            );
            if (behavior == 0) {
                item.behaviorTimer = 0;
                const auto choice = static_cast<std::uint8_t>(rng_.next());
                behavior = choice < choice2Lower
                    ? 1
                    : (choice < choice3Lower ? 2 : 3);
            }
            if (behavior == 1) {
                if (item.behaviorTimer == 0) {
                    item.forwardVelocity = cruiseSpeed;
                    const auto choice = rng_.next();
                    if (static_cast<std::uint8_t>(choice) < 90) {
                        item.yawVelocity = 0;
                    } else {
                        const auto turnFactor = static_cast<std::int16_t>(
                            item.type == 1 ? 1 : 3
                        );
                        auto turn = static_cast<std::int16_t>(
                            static_cast<std::int16_t>(choice)
                                % (turnFactor << 7)
                            + 256
                        );
                        if ((choice & 0x0100U) != 0) {
                            turn = static_cast<std::int16_t>(-turn);
                        }
                        item.yawVelocity = turn;
                    }
                    item.behaviorTimer = static_cast<std::uint8_t>(
                        (choice >> 8U) & 0x7FU
                    );
                } else {
                    --item.behaviorTimer;
                    if (item.behaviorTimer == 0) {
                        behavior = 0;
                    }
                    if (insideAttackRange) {
                        fireEnemy(index);
                    }
                }
            } else if (behavior == 2) {
                item.forwardVelocity = 0;
                const auto difference = static_cast<std::int16_t>(
                    item.targetHeading - item.yaw
                );
                item.yawVelocity = static_cast<std::int16_t>(
                    difference < -turnLimit ? -turnLimit
                    : difference > turnLimit ? turnLimit
                    : difference
                );
                if (!insideAttackRange || fireEnemy(index)) {
                    behavior = 0;
                }
                const auto keepChoice =
                    static_cast<std::uint8_t>(rng_.next());
                if (keepChoice < behavior2Reset) {
                    behavior = 0;
                }
            } else if (behavior == 3) {
                const auto difference = static_cast<std::int16_t>(
                    item.targetHeading - item.yaw
                );
                item.yawVelocity = static_cast<std::int16_t>(
                    difference < -turnLimit ? -turnLimit
                    : difference > turnLimit ? turnLimit
                    : difference
                );
                if (insideAttackRange) {
                    fireEnemy(index);
                }
                if (
                    static_cast<std::uint8_t>(rng_.next())
                    < behavior3Reset
                ) {
                    behavior = 0;
                }
            } else if (behavior == 4) {
                item.collisionContactAllocationIndex =
                    static_cast<std::size_t>(-1);
                item.collisionContactIsPlayer = false;
                if (item.behaviorTimer == 0) {
                    const auto choice = rng_.next();
                    const auto reverseSpeed = static_cast<std::int16_t>(
                        item.type == 1 ? 45 : 72
                    );
                    item.forwardVelocity = item.forwardVelocity < 1
                        ? cruiseSpeed
                        : static_cast<std::int16_t>(-reverseSpeed);
                    const auto turn = static_cast<std::int16_t>(
                        (item.type == 1 ? 1 : 3) << 8
                    );
                    item.yawVelocity = (choice & 0x0200U) != 0
                        ? static_cast<std::int16_t>(-turn)
                        : turn;
                    item.behaviorTimer = 0x3F;
                } else {
                    --item.behaviorTimer;
                    if (item.behaviorTimer == 0) {
                        behavior = 0;
                        item.forwardVelocity = cruiseSpeed;
                    }
                    if (insideAttackRange) {
                        fireEnemy(index);
                    }
                }
            } else if (behavior == 5) {
                item.forwardVelocity = cruiseSpeed;
                const auto choice = rng_.next();
                if (static_cast<std::uint8_t>(choice) < 90) {
                    item.yawVelocity = 0;
                } else {
                    const auto turnFactor = static_cast<std::int16_t>(
                        item.type == 1 ? 1 : 3
                    );
                    auto turn = static_cast<std::int16_t>(
                        static_cast<std::int16_t>(choice)
                            % (turnFactor << 7)
                        + 256
                    );
                    if ((choice & 0x0100U) != 0) {
                        turn = static_cast<std::int16_t>(-turn);
                    }
                    item.yawVelocity = turn;
                }
                item.behaviorTimer = static_cast<std::uint8_t>(
                    (choice >> 8U) & 0x7FU
                );
            }
        } else if (item.type == 11) {
            behavior = advanceType11Behavior(item, player_, rng_,
                [this, index](Level1Entity&) { return fireEnemy(index); });
        } else if (item.type == 7) {
            if (behavior == 0) {
                const auto difference = static_cast<std::int16_t>(
                    item.targetHeading - item.yaw
                );
                item.yawVelocity = static_cast<std::int16_t>(
                    difference < -0x500 ? -0x500
                    : difference > 0x500 ? 0x500
                    : difference
                );
                if (item.position.z >= 500) {
                    item.roll = static_cast<std::uint16_t>(
                        item.roll + arithmeticShiftRight(
                            -(
                                static_cast<std::int32_t>(
                                    item.yawVelocity
                                ) * 0x10
                                + static_cast<std::int16_t>(item.roll)
                            ),
                            4
                        )
                    );
                }
                if (item.presenterDistance < 1000U) {
                    behavior = 1;
                    item.behaviorTimer = 0x50;
                }
            } else if (behavior == 1) {
                item.forwardVelocity = static_cast<std::int16_t>(
                    item.forwardVelocity - 5
                );
                --item.behaviorTimer;
                if (item.forwardVelocity == 0) {
                    behavior = 2;
                    item.behaviorTimer = 0x1D;
                }
            } else if (behavior == 2) {
                --item.behaviorTimer;
                item.stateBytes4dTo52[2] =
                    static_cast<std::uint8_t>(
                        static_cast<std::int8_t>(item.behaviorTimer)
                            / -10
                        + 3
                    );
                if (
                    postWaveObjectAllocationIndex_
                        == static_cast<std::size_t>(-1)
                    && item.behaviorTimer == 0
                ) {
                    behavior = 3;
                    item.behaviorTimer = 0x46;
                    item.flags = static_cast<std::uint8_t>(
                        item.flags | 0x08U
                    );
                }
            } else if (behavior == 3) {
                --item.behaviorTimer;
                if (
                    static_cast<std::int8_t>(item.behaviorTimer) < 0x1E
                ) {
                    if (item.behaviorTimer == 0x1D) {
                        item.flags = static_cast<std::uint8_t>(
                            item.flags
                            & static_cast<std::uint8_t>(~0x08U)
                        );
                        deployPostWaveObject = true;
                        postWaveSource = item;
                    } else {
                        item.stateBytes4dTo52[2] =
                            static_cast<std::uint8_t>(
                                static_cast<std::int8_t>(
                                    item.behaviorTimer
                                ) / 10
                            );
                        if (item.behaviorTimer == 0) {
                            behavior = 4;
                        }
                    }
                }
            } else if (behavior == 4) {
                item.pitch = static_cast<std::uint16_t>(
                    item.pitch + 0x90
                );
                item.forwardVelocity = static_cast<std::int16_t>(
                    item.forwardVelocity + 0x32
                );
                item.yawVelocity = 0x100;
                if (item.position.z >= 500) {
                    item.roll = static_cast<std::uint16_t>(
                        item.roll + arithmeticShiftRight(
                            -(
                                static_cast<std::int32_t>(
                                    item.yawVelocity
                                ) * 0x10
                                + static_cast<std::int16_t>(item.roll)
                            ),
                            4
                        )
                    );
                }
                if (item.presenterDistance > 60000U) {
                    item.stateBytes4dTo52[1] = 0;
                    removeAllocationObject(
                        ObjectKind::entity,
                        index
                    );
                    completionObjectAllocationIndex_ =
                        static_cast<std::size_t>(-1);
                }
            }
        } else if (item.type == 44) {
            if (item.behaviorTimer == 0) {
                if (
                    item.collisionContactAllocationIndex
                        != static_cast<std::size_t>(-1)
                ) {
                    const auto contactIndex =
                        item.collisionContactAllocationIndex;
                    if (
                        contactIndex < entities_.size()
                        && entities_[contactIndex].type != 7
                    ) {
                        auto& target = entities_[contactIndex];
                        auto& health = target.stateBytes4dTo52[1];
                        if (health <= 20) {
                            const auto destroyedType = target.type;
                            const auto destroyed = target;
                            registerDestroyedTypeBonus(
                                destroyedType,
                                destroyed
                            );
                            deathEffectMembershipChanged =
                                spawnDeathEffect(destroyed)
                                || deathEffectMembershipChanged;
                            health = 0;
                            removeAllocationObject(
                                ObjectKind::entity,
                                contactIndex
                            );
                            if (
                                destroyed.active()
                                && static_cast<std::int8_t>(
                                    destroyed.stateBytes4dTo52[0]
                                ) == -1
                            ) {
                                --activeEnemyCount_;
                            }
                            if (
                                destroyed.active()
                                && destroyedType >= 0
                                && static_cast<std::size_t>(destroyedType)
                                    < scoreByType.size()
                            ) {
                                score_ += scoreByType[
                                    static_cast<std::size_t>(destroyedType)
                                ];
                            }
                            if (destroyed.active() && destroyedType == 44) {
                                audioEvents_.push_back({loopTick_, 12});
                                score_ += 1000;
                                deploySuccessPickup = true;
                                successPickupSource = destroyed;
                                successPickupSource.targetHeading =
                                    entityTargetHeadingsAtUpdateStart[
                                        contactIndex
                                    ];
                            }
                        } else {
                            health = static_cast<std::uint8_t>(
                                health - 20U
                            );
                            deathEffectMembershipChanged =
                                spawnTransientEffect(target, 0, 12)
                                || deathEffectMembershipChanged;
                            audioEvents_.push_back({loopTick_, 5});
                        }
                    }
                    item.collisionContactAllocationIndex =
                        static_cast<std::size_t>(-1);
                }
                if (item.presenterDistance > 0x2000U) {
                    item.forwardVelocity = 200;
                    item.animatedModelPresenterEnabled = true;
                } else {
                    item.forwardVelocity = 0;
                    item.animatedModelPresenterEnabled = false;
                }
                const auto difference = static_cast<std::int16_t>(
                    item.targetHeading - item.yaw
                );
                const auto magnitude = difference < 0
                    ? -static_cast<std::int32_t>(difference)
                    : static_cast<std::int32_t>(difference);
                if (magnitude < 200) {
                    item.behaviorTimer = 20;
                    item.yawVelocity = 0;
                    if (item.presenterDistance < 0x5000U) {
                        fireType44(index);
                    }
                } else {
                    item.yawVelocity = static_cast<std::int16_t>(
                        difference < -0x200 ? -0x200
                        : difference > 0x200 ? 0x200
                        : difference
                    );
                }
            } else {
                --item.behaviorTimer;
            }
            if (item.collisionContactIsPlayer) {
                item.collisionContactIsPlayer = false;
                audioEvents_.push_back({loopTick_, 6});
                playerArmor_ = static_cast<std::int16_t>(
                    playerArmor_ - 100
                );
                if (playerArmor_ < 0) {
                    failurePending_ = true;
                }
            }
        } else if (item.type == 25 || item.type == 26) {
            advancePickupOscillator(item);
        }
    };
    bool projectileMembershipChanged = false;
    // FUN_1000_0048 completes the shared FUN_1000_0784 update pass,
    // including projectile callback FUN_1000_04d8, before FUN_1000_9298
    // dispatches entity behavior callbacks. An expired projectile can
    // therefore return its pool slot before a later actor fires.
    // Follow the shared list: simultaneous swap-back removals depend on order.
    std::array<std::size_t, projectilePoolSize> projectileUpdateOrder{};
    for (std::size_t slot = 0; slot < projectileUpdateOrder.size(); ++slot) {
        projectileUpdateOrder[slot] = slot;
    }
    std::stable_sort(projectileUpdateOrder.begin(), projectileUpdateOrder.end(),
        [&projectileObjectOrder](const auto left, const auto right) {
            return projectileObjectOrder[left] < projectileObjectOrder[right];
        });
    for (const auto slot : projectileUpdateOrder) {
        auto& projectile = projectiles_[slot];
        if (!projectileActiveAtStart[slot] || !projectile.active()) {
            continue;
        }
        --projectile.stateBytes4dTo54[1];
        // FUN_1000_04d8 checks the previous boundary's altitude before
        // FUN_1000_3b3a.  A projectile can therefore cross below -300 and
        // remain observable for one boundary before its ground impact.
        if (projectile.position.z < -300) {
            Level1Entity impactSource{};
            impactSource.position = projectile.position;
            impactSource.presenterDistance =
                projectile.presenterDistance;
            deathEffectMembershipChanged =
                spawnTransientEffect(impactSource, 0, 12)
                || deathEffectMembershipChanged;
            projectile.stateBytes4dTo54[1] = 0;
            removeAllocationObject(ObjectKind::projectile, slot);
            projectileMembershipChanged = true;
            continue;
        }
        advanceKinematics(projectile);
        if (projectile.type != 23) {
            if (collides(projectile, player_)) {
                projectile.stateBytes4dTo54[1] = 0;
                const auto armorDamage = projectile.type == 20
                    ? 56
                    : projectile.type == 22 ? 104 : 40;
                audioEvents_.push_back({loopTick_, 6});
                playerArmor_ = static_cast<std::int16_t>(
                    playerArmor_ - armorDamage
                );
                if (playerArmor_ < 0) {
                    failurePending_ = true;
                }
                removeAllocationObject(ObjectKind::projectile, slot);
                projectileMembershipChanged = true;
                continue;
            }
            for (const auto entityIndex : activeListOrder_) {
                if (
                    entityIndex == projectile.ownerAllocationIndex
                    || !entities_[entityIndex].active()
                ) {
                    continue;
                }
                auto collisionTarget = entities_[entityIndex];
                if (
                    projectileObjectOrder[slot]
                        < entityObjectOrder[entityIndex]
                    && entityIndex
                        < entityPositionsAtUpdateStart.size()
                ) {
                    collisionTarget.position =
                        entityPositionsAtUpdateStart[entityIndex];
                }
                if (collides(projectile, collisionTarget)) {
                    projectile.stateBytes4dTo54[1] = 0;
                    break;
                }
            }
            if (!projectile.active()) {
                removeAllocationObject(ObjectKind::projectile, slot);
                projectileMembershipChanged = true;
            }
            continue;
        }
        for (const auto entityIndex : activeListOrder_) {
            auto& target = entities_[entityIndex];
            if (!target.active()) {
                continue;
            }
            auto collisionTarget = target;
            if (
                projectileObjectOrder[slot]
                    < entityObjectOrder[entityIndex]
                && entityIndex < entityPositionsAtUpdateStart.size()
            ) {
                collisionTarget.position =
                    entityPositionsAtUpdateStart[entityIndex];
            }
            const auto targetFlags = static_cast<std::int8_t>(
                target.stateBytes4dTo52[0]
            );
            if (
                targetFlags == -4
                || targetFlags == -3
                || target.type == 46
                || !collides(projectile, collisionTarget)
            ) {
                continue;
            }
            const auto damage = static_cast<std::uint8_t>(
                projectile.stateBytes4dTo54[2] == 0 ? 1 : 5
            );
            auto& health = target.stateBytes4dTo52[1];
            const bool resetsAfterLethalDamage =
                target.type == 25 || target.type == 26;
            if (health <= damage && !resetsAfterLethalDamage) {
                const auto destroyedType = target.type;
                const auto destroyed = target;
                registerDestroyedTypeBonus(
                    destroyedType,
                    destroyed
                );
                deathEffectMembershipChanged =
                    spawnDeathEffect(destroyed)
                    || deathEffectMembershipChanged;
                entities_[entityIndex].stateBytes4dTo52[1] = 0;
                removeAllocationObject(
                    ObjectKind::entity,
                    entityIndex
                );
                if (
                    static_cast<std::int8_t>(
                        destroyed.stateBytes4dTo52[0]
                    ) == -1
                ) {
                    --activeEnemyCount_;
                }
                if (
                    destroyedType >= 0
                    && static_cast<std::size_t>(destroyedType)
                        < scoreByType.size()
                ) {
                    score_ += scoreByType[
                        static_cast<std::size_t>(destroyedType)
                    ];
                }
                if (destroyedType == 44) {
                    // FUN_1000_1176 recognizes state class 5 as the special
                    // post-wave target: event 12, a fixed 1000 points, and
                    // FUN_1000_45c6 pickup deployment.  The global Type 44
                    // handle at 0x1292 deliberately remains stale.
                    audioEvents_.push_back({loopTick_, 12});
                    score_ += 1000;
                    deploySuccessPickup = true;
                    successPickupSource = destroyed;
                    successPickupSource.targetHeading =
                        entityTargetHeadingsAtUpdateStart[entityIndex];
                }
            } else {
                health = health <= damage
                    ? 100
                    : static_cast<std::uint8_t>(health - damage);
                // FUN_1000_1176 emits the 0..12 impact animation and event 5
                // for every surviving player-projectile hit. Tick 614 is the
                // first natural-success boundary that exposes this path.
                deathEffectMembershipChanged =
                    spawnTransientEffect(target, 0, 12)
                    || deathEffectMembershipChanged;
                audioEvents_.push_back({loopTick_, 5});
            }
            projectile.stateBytes4dTo54[1] = 0;
            break;
        }
        if (!projectile.active()) {
            removeAllocationObject(ObjectKind::projectile, slot);
            projectileMembershipChanged = true;
        }
    }
    // FUN_1000_9298 computes presentation state and dispatches callbacks in
    // the active-list order produced by the preceding frame.
    for (const auto index : callbackOrder) {
        if (
            entities_[index].type == 1
            || entities_[index].type == 3
            || entities_[index].type == 11
            || entities_[index].type == 7
            || entities_[index].type == 44
            || entities_[index].type == 25
            || entities_[index].type == 26
        ) {
            advanceBehavior(index);
        }
    }
    if (deployPostWaveObject) {
        // Type 44 uses the same FUN_1000_4370 allocator as ordinary actors.
        // Its default position and yaw are overwritten below, but their two
        // FUN_1000_b39e calls remain part of the observable RNG sequence.
        rng_.next();
        rng_.next();
        auto deployed = entity(
            0,
            44,
            2792,
            {
                postWaveSource.position.x,
                postWaveSource.position.y,
                0,
            },
            postWaveSource.targetHeading,
            0,
            {255, 15, 2, 0, 5, 0}
        );
        auto& stored = allocateEntity(deployed);
        postWaveObjectAllocationIndex_ = stored.allocationIndex;
        ++activeEnemyCount_;
        ++spawnedEnemyCount_;
    }
    if (deploySuccessPickup) {
        const auto choice = rng_.next();
        const auto pickupYaw = static_cast<std::uint16_t>(
            successPickupSource.targetHeading - 0x8000U
        );
        auto pickup = entity(
            0,
            25,
            897,
            {
                successPickupSource.position.x,
                successPickupSource.position.y,
                0,
            },
            pickupYaw,
            100,
            {254, 100, 2, 0, 0, 1}
        );
        // FUN_1000_45c6 repeatedly advances the new pickup at speed 100
        // until FUN_1000_0c0a no longer reports the destroyed Type 44 actor
        // still carried by the current active list.
        while (collides(pickup, successPickupSource)) {
            advanceKinematics(pickup);
        }
        pickup.forwardVelocity = 0;
        pickup.yawVelocity = static_cast<std::int16_t>(
            (
                static_cast<std::int16_t>(
                    (choice >> 8U) & 0x03U
                ) + 10
            ) << 8
        );
        if ((choice & 0x0100U) != 0) {
            pickup.yawVelocity = static_cast<std::int16_t>(
                -pickup.yawVelocity
            );
        }
        pickup.collisionContactAllocationIndex =
            successPickupSource.allocationIndex;
        pickup.presenterDistance = approximatePlanarDistance(
            player_.position,
            pickup.position
        );
        pickup.targetHeading = presenterTargetHeading(
            pickup.position,
            player_.position
        );
        // The newly appended pickup participates in the same FUN_1000_9298
        // pass, advancing selector 2 to 3 before the captured boundary.
        pickup.stateBytes4dTo52[2] = 3;
        // FUN_1000_4506 scans only the dynamic 16-record pool.  Portable
        // entity slots 0 and 1 model the separate Type 7 and Type 44 actors.
        allocateEntity(pickup, 2, true);
    }
    const bool activeListMembershipChanged = [this,
        &entityActiveAtUpdateStart,
        &projectileActiveAtStart]() noexcept {
        if (entities_.size() != entityActiveAtUpdateStart.size()) {
            return true;
        }
        for (std::size_t index = 0; index < entities_.size(); ++index) {
            if (
                entities_[index].active()
                != entityActiveAtUpdateStart[index]
            ) {
                return true;
            }
        }
        for (std::size_t slot = 0; slot < projectiles_.size(); ++slot) {
            if (
                projectiles_[slot].active()
                != projectileActiveAtStart[slot]
            ) {
                return true;
            }
        }
        return false;
    }();
    const bool projectionThresholdCrossed =
        projectionOriginMoved(
            player_.position.x,
            projectionListOrigin_.x
        )
        || projectionOriginMoved(
            player_.position.y,
            projectionListOrigin_.y
        );
    const bool rebuildActiveList =
        nativeInitialListPending_
        || activeListMembershipChanged
        || projectileMembershipChanged
        || deathEffectMembershipChanged
        || projectionThresholdCrossed;
    for (auto& projectile : projectiles_) {
        if (!projectile.active()) {
            continue;
        }
        projectile.presenterDistance = approximatePlanarDistance(
            player_.position,
            projectile.position
        );
        projectile.targetHeading = presenterTargetHeading(
            projectile.position,
            player_.position
        );
    }
    const auto referenceIsListed = [this](
        const ObjectReference& reference
    ) noexcept {
        if (reference.kind == ObjectKind::entity) {
            return (
                reference.index < entities_.size()
                && entities_[reference.index].active()
            );
        }
        if (reference.kind == ObjectKind::projectile) {
            return (
                reference.index < projectiles_.size()
                && projectiles_[reference.index].active()
            );
        }
        return (
            reference.index < deathEffects_.size()
            && deathEffects_[reference.index].listed
        );
    };
    if (rebuildActiveList) {
        // Allocation/removal sets the flag at 0x1296. Independently,
        // FUN_1000_3768 rebuilds when the main-view actor's X or Y has
        // moved at least 0x400 from the origin last copied to 0x417c.
        // Both paths copy the current origin and replace 0x45d2 from the
        // allocation array at 0x49f8.
        projectionListOrigin_ = player_.position;
        objectListOrder_.clear();
        for (const auto& reference : allocationList_) {
            if (referenceIsListed(reference)) {
                objectListOrder_.push_back(reference);
            }
        }
        nativeInitialListPending_ = false;
    } else {
        // FUN_1000_14b0 sorts the complete shared object list. Projectile
        // positions in that list determine which actor state is visible
        // when a later projectile callback performs its collision scan.
        for (
            std::size_t destination = 0;
            destination < objectListOrder_.size();
            ++destination
        ) {
            std::uint32_t maximumDistance = 0;
            auto maximumIndex = destination;
            for (
                std::size_t candidate = destination;
                candidate < objectListOrder_.size();
                ++candidate
            ) {
                const auto distance = objectPresenterDistance(
                    objectListOrder_[candidate]
                );
                if (maximumDistance < distance) {
                    maximumDistance = distance;
                    maximumIndex = candidate;
                }
            }
            if (maximumIndex != destination) {
                std::swap(
                    objectListOrder_[destination],
                    objectListOrder_[maximumIndex]
                );
            }
        }
    }
    // FUN_1000_b996 clears the projected-visible bit on transformable
    // objects before testing the main-view near plane and viewport. It sets
    // the bit again only when the projected center is visible. Transient
    // effects retain this presenter-owned flag at the captured main-view
    // boundary.
    auto mainView = initialLevel1MainView();
    mainView.origin = player_.position;
    mainView.matrix = makeFixedMatrixQ14(
        player_.pitch,
        player_.roll,
        player_.yaw
    );
    transpose(mainView.matrix);
    for (const auto& reference : objectListOrder_) {
        if (
            reference.kind != ObjectKind::entity
            || reference.index >= entities_.size()
        ) {
            continue;
        }
        auto& item = entities_[reference.index];
        if (!item.active() || (item.flags & 0x08U) == 0) {
            continue;
        }
        item.flags = static_cast<std::uint8_t>(
            item.flags & static_cast<std::uint8_t>(~0x04U)
        );
        const auto local = transformToView(item.position, mainView);
        if (local.y > 0) {
            const auto projected = projectToViewport(local, mainView);
            if (
                projected.x >= mainView.viewport.left
                && projected.x <= mainView.viewport.right
                && projected.y >= mainView.viewport.top
                && projected.y <= mainView.viewport.bottom
            ) {
                item.flags = static_cast<std::uint8_t>(
                    item.flags | 0x04U
                );
            }
        }
        if (item.type == 7 && (item.flags & 0x04U) != 0) {
            // FUN_1000_b0b4 draws ten completion streaks (-5 through 4).
            // Each line selects its color with one FUN_1000_b39e call.
            for (std::uint8_t streak = 0; streak < 10; ++streak) {
                rng_.next();
            }
        }
    }
    for (const auto& reference : objectListOrder_) {
        if (
            reference.kind != ObjectKind::deathEffect
            || reference.index >= deathEffects_.size()
            || !deathEffects_[reference.index].listed
        ) {
            continue;
        }
        auto& effect = deathEffects_[reference.index].entity;
        if ((effect.flags & 0x08U) == 0) {
            continue;
        }
        effect.flags = static_cast<std::uint8_t>(
            effect.flags & static_cast<std::uint8_t>(~0x04U)
        );
        const auto local = transformToView(effect.position, mainView);
        if (local.y <= 0) {
            continue;
        }
        const auto projected = projectToViewport(local, mainView);
        if (
            projected.x >= mainView.viewport.left
            && projected.x <= mainView.viewport.right
            && projected.y >= mainView.viewport.top
            && projected.y <= mainView.viewport.bottom
        ) {
            effect.flags = static_cast<std::uint8_t>(
                effect.flags | 0x04U
            );
        }
    }
    refreshDeathEffectAllocationIndices();
    activeListOrder_.clear();
    semanticActiveListOrder_.clear();
    for (const auto& reference : objectListOrder_) {
        if (
            reference.kind == ObjectKind::entity
            && reference.index < entities_.size()
            && entities_[reference.index].active()
        ) {
            activeListOrder_.push_back(reference.index);
            semanticActiveListOrder_.push_back(
                entities_[reference.index].allocationIndex
            );
        } else if (
            reference.kind == ObjectKind::deathEffect
            && reference.index < deathEffects_.size()
            && deathEffects_[reference.index].listed
        ) {
            semanticActiveListOrder_.push_back(
                deathEffects_[reference.index].entity.allocationIndex
            );
        }
    }
    // FUN_1000_9298 can retire an effect allocation after the current
    // presenter list has already consumed its pointer.  Apply those
    // swap-back removals only after preserving that one stale boundary.
    for (std::size_t slot = 0; slot < deathEffects_.size(); ++slot) {
        if (deathEffectPendingAllocationRemoval[slot]) {
            removeAllocationObject(ObjectKind::deathEffect, slot);
        }
    }
    if (renderer) {
        renderer->render({ground_,rng_,player_,entities_,activeListOrder_,loopTick_});
    } else {
        ground_.advance(player_.yawVelocity,player_.forwardVelocity,rng_);
    }
    // FUN_1000_130c runs once for the main view and once for the radar.
    // Traces are emitted at the main-view boundary, before the radar call.
    for (auto& item : entities_) {
        if (
            item.active()
            && (item.type == 11 || item.type == 44)
        ) {
            item.presentationModelAnimationVariant =
                item.modelAnimationVariant;
        }
    }
    advanceAnimatedEntityPresenters();
    pendingPostBoundaryInput_ = input;
    ordinaryTailPending_ = true;
    if (failurePending_) {
        failureTailProjectileActiveAtStart_ = projectileActiveAtStart;
        failureTailConsumed_ = false;
    }
}

void Level1State::advanceIdleTick() noexcept {
    advanceTick({});
}

bool Level1State::firePrimary() noexcept {
    if (playerWeaponCooldown_ != 0) {
        return false;
    }
    auto timer = playerProjectileTimers_.end();
    for (auto item = playerProjectileTimers_.begin();
         item != playerProjectileTimers_.end();
         ++item) {
        if (*item == 0) {
            timer = item;
            break;
        }
    }
    if (timer == playerProjectileTimers_.end()) {
        return false;
    }
    auto projectile = projectiles_.end();
    for (auto item = projectiles_.begin();
         item != projectiles_.end();
         ++item) {
        if (!item->active()) {
            projectile = item;
            break;
        }
    }
    if (projectile == projectiles_.end()) {
        return false;
    }

    const auto poolSlot = projectile->poolSlot;
    *projectile = {};
    projectile->poolSlot = poolSlot;
    projectile->type = 23;
    projectile->collisionRadius = 134;
    projectile->position = player_.position;
    projectile->pitch = player_.pitch;
    projectile->roll = player_.roll;
    projectile->yaw = player_.yaw;
    projectile->forwardVelocity = 500;
    projectile->stateBytes4dTo54 = {
        253, 64, 0, 0, 0, 0, 0, 0,
    };
    projectile->ownerIsPlayer = true;
    appendAllocationObject(ObjectKind::projectile, poolSlot);
    audioEvents_.push_back({loopTick_, 14});
    *timer = 32;
    playerWeaponCooldown_ = 9;
    return true;
}

bool Level1State::fireEnemy(const std::size_t entityIndex) noexcept {
    auto& owner = entities_[entityIndex];
    if (owner.weaponCooldown != 0) {
        --owner.weaponCooldown;
        return false;
    }
    const auto difference = static_cast<std::int16_t>(
        owner.targetHeading - owner.yaw
    );
    const auto magnitude = difference < 0
        ? -static_cast<std::int32_t>(difference)
        : static_cast<std::int32_t>(difference);
    if (magnitude >= 0x200 || owner.weaponSlotCount == 0) {
        return false;
    }
    bool launched = false;
    for (std::size_t slot = 0; slot < owner.weaponSlotCount; ++slot) {
        auto& timer = owner.weaponSlotTimers[slot];
        if (timer != 0) {
            --timer;
            continue;
        }
        if (launched) {
            continue;
        }
        auto projectile = projectiles_.end();
        for (auto item = projectiles_.begin();
             item != projectiles_.end();
             ++item) {
            if (!item->active()) {
                projectile = item;
                break;
            }
        }
        if (projectile == projectiles_.end()) {
            continue;
        }
        const auto poolSlot = projectile->poolSlot;
        *projectile = {};
        projectile->poolSlot = poolSlot;
        const auto projectileType = owner.type == 1 ? 17 : 20;
        const auto lifetime = static_cast<std::uint8_t>(
            projectileType == 17 ? 54 : 60
        );
        projectile->type = projectileType;
        projectile->collisionRadius = (
            projectileType == 17 ? 441 : 134
        );
        projectile->position = owner.position;
        projectile->pitch = owner.pitch;
        projectile->roll = owner.roll;
        projectile->yaw = owner.yaw;
        projectile->forwardVelocity = 500;
        projectile->stateBytes4dTo54 = {
            253, lifetime, 0, 0, 0, 0, 0, 0,
        };
        projectile->ownerAllocationIndex = owner.allocationIndex;
        appendAllocationObject(ObjectKind::projectile, poolSlot);
        audioEvents_.push_back({
            loopTick_,
            static_cast<std::uint16_t>(
                projectileType < 20 ? 2 : 15
            ),
        });
        if (owner.stateBytes4dTo52[4] != 5) {
            owner.behaviorTimer = 1;
        }
        owner.weaponCooldown = owner.type == 1 ? 8 : 0;
        timer = lifetime;
        launched = true;
    }
    return launched;
}

bool Level1State::fireType44(const std::size_t entityIndex) noexcept {
    auto projectile = projectiles_.end();
    for (auto item = projectiles_.begin();
         item != projectiles_.end();
         ++item) {
        if (!item->active()) {
            projectile = item;
            break;
        }
    }
    if (projectile == projectiles_.end()) {
        return false;
    }

    const auto& owner = entities_[entityIndex];
    const auto poolSlot = projectile->poolSlot;
    const auto inheritedPitch = projectile->pitch;
    const auto inheritedRoll = projectile->roll;
    const auto inheritedPitchVelocity = projectile->pitchVelocity;
    const auto inheritedRollVelocity = projectile->rollVelocity;
    const auto inheritedYawVelocity = projectile->yawVelocity;
    const auto inheritedXVelocity = projectile->xVelocity;
    const auto inheritedYVelocity = projectile->yVelocity;
    const auto inheritedZVelocity = projectile->zVelocity;
    const auto inheritedForwardVelocity = projectile->forwardVelocity;
    *projectile = {};
    projectile->poolSlot = poolSlot;
    projectile->type = 22;
    projectile->collisionRadius = 134;
    projectile->position = owner.position;
    projectile->position.z += 2000;
    // FUN_1000_acec copies only the owner's three 32-bit coordinates.
    // FUN_1000_44ba leaves the reused slot's remaining motion state intact.
    projectile->pitch = inheritedPitch;
    projectile->roll = inheritedRoll;
    projectile->yaw = static_cast<std::uint16_t>(
        owner.yaw - 0x4000U
    );
    // FUN_1000_acec integrates the copied pose before writing the Type 22
    // speed. FUN_1000_44ba leaves the reused pool slot's old speed intact,
    // so a previously fired slot contributes one inherited launch step.
    projectile->pitchVelocity = inheritedPitchVelocity;
    projectile->rollVelocity = inheritedRollVelocity;
    projectile->yawVelocity = inheritedYawVelocity;
    projectile->xVelocity = inheritedXVelocity;
    projectile->yVelocity = inheritedYVelocity;
    projectile->zVelocity = inheritedZVelocity;
    projectile->forwardVelocity = inheritedForwardVelocity;
    advanceKinematics(*projectile);
    projectile->forwardVelocity = 500;
    projectile->yaw = fixedHeadingBetween(
        projectile->position,
        player_.position
    );
    projectile->pitch = fixedHeadingBetween(
        {},
        {
            projectile->position.z,
            static_cast<std::int32_t>(owner.presenterDistance),
            0,
        }
    );
    projectile->stateBytes4dTo54 = {
        253, 80, 1, 0, 0, 0, 0, 0,
    };
    projectile->ownerAllocationIndex = owner.allocationIndex;
    appendAllocationObject(ObjectKind::projectile, poolSlot);
    audioEvents_.push_back({loopTick_, 6});
    return true;
}

void Level1State::runPeriodicSpawnCheck() noexcept {
    if (spawnedEnemyCount_ >= maximumEnemyCount_) {
        return;
    }
    constexpr std::array<std::uint8_t, 11> thresholdByActiveCount{{
        130, 60, 10, 0, 0, 0, 0, 0, 0, 0, 140,
    }};
    const auto choice = rng_.next();
    const auto low = static_cast<std::uint8_t>(choice);
    const auto high = static_cast<std::uint8_t>(choice >> 8U);
    const auto activeIndex = activeEnemyCount_ < 0
        ? std::size_t{0}
        : std::min<std::size_t>(
            static_cast<std::size_t>(activeEnemyCount_),
            thresholdByActiveCount.size() - 1
        );
    if (low >= thresholdByActiveCount[activeIndex] || high == 0) {
        return;
    }
    constexpr std::array<std::uint8_t, 14> typeWeights{{
        100, 0, 170, 0, 0, 0, 0, 0, 0, 0, 255, 0, 0, 0,
    }};
    std::uint8_t selectedType = 0;
    std::uint8_t selectedWeight = 0;
    for (std::size_t index = 0; index < typeWeights.size(); ++index) {
        if (
            selectedWeight < high
            && high < typeWeights[index]
        ) {
            selectedType = static_cast<std::uint8_t>(index + 1);
            selectedWeight = typeWeights[index];
        }
    }
    if (selectedType == 0) {
        return;
    }

    const auto positionChoice = rng_.next();
    auto firstCode = static_cast<std::int16_t>(
        static_cast<std::uint8_t>(positionChoice >> 8U) & 0x0FU
    );
    auto secondCode = static_cast<std::int16_t>(0x10);
    if ((positionChoice & 1U) != 0) {
        std::swap(firstCode, secondCode);
    }
    if ((positionChoice & 2U) != 0) {
        firstCode = static_cast<std::int16_t>(-firstCode);
    }
    if ((positionChoice & 4U) != 0) {
        secondCode = static_cast<std::int16_t>(-secondCode);
    }
    const auto coordinate = [](const std::int16_t code) noexcept {
        const auto lowByte = static_cast<std::uint8_t>(code);
        return static_cast<std::int32_t>(
            static_cast<std::int16_t>(
                static_cast<std::uint16_t>(lowByte) << 11U
            )
        ) - 1;
    };
    const auto yawChoice = rng_.next();
    const auto yaw = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(
            static_cast<std::uint8_t>(yawChoice) & 0xF0U
        ) << 11U
    );

    std::uint32_t radius = 0;
    std::int16_t speed = 0;
    std::uint8_t presenterState = 0;
    std::uint8_t weaponSlots = 0;
    if (selectedType == 1) {
        radius = 1342;
        speed = 225;
        presenterState = 1;
        weaponSlots = 2;
    } else if (selectedType == 3) {
        radius = 770;
        speed = 216;
        presenterState = 1;
        weaponSlots = 1;
    } else if (selectedType == 11) {
        radius = 748;
        speed = 270;
        presenterState = 3;
        weaponSlots = 1;
    } else {
        return;
    }
    auto spawned = entity(
        0,
        selectedType,
        radius,
        {
            coordinate(firstCode),
            coordinate(secondCode),
            selectedType == 11 ? 1000 : 0,
        },
        yaw,
        speed,
        {
            255,
            1,
            static_cast<std::uint8_t>(selectedType == 11 ? 5 : 0),
            0,
            presenterState,
            0,
        }
    );
    spawned.weaponSlotCount = weaponSlots;
    auto& stored = allocateEntity(spawned);
    spawned = stored;
    ++activeEnemyCount_;
    ++spawnedEnemyCount_;
    lastSpawnCheckpoint_ = spawned;
    lastSpawnTick_ = loopTick_;
    lastSpawnRng_ = rng_;
    hasLastSpawnCheckpoint_ = true;
}

Level1Entity& Level1State::allocateEntity(
    Level1Entity value,
    const std::size_t firstReusableIndex,
    const bool preserveDynamicScratch
) noexcept {
    for (
        std::size_t index = firstReusableIndex;
        index < entities_.size();
        ++index
    ) {
        if (!entities_[index].active()) {
            const bool primaryType11AnimationSelector =
                entities_[index].primaryType11AnimationSelector;
            const auto presenterDistance =
                entities_[index].presenterDistance;
            const auto targetHeading = entities_[index].targetHeading;
            const auto collisionContactAllocationIndex =
                entities_[index].collisionContactAllocationIndex;
            const auto collisionContactIsPlayer =
                entities_[index].collisionContactIsPlayer;
            value.allocationIndex = index;
            entities_[index] = value;
            entities_[index].primaryType11AnimationSelector =
                primaryType11AnimationSelector;
            if (preserveDynamicScratch) {
                // FUN_1000_4506 reinitializes the dynamic pool record but
                // leaves its presenter distance/heading and +0x5f contact
                // storage untouched for the Type 25 caller and same-tick
                // presenter.  Type 42 also starts at dynamic slot 2 but its
                // target-backed initialization clears these fields.
                entities_[index].presenterDistance = presenterDistance;
                entities_[index].targetHeading = targetHeading;
                entities_[index].collisionContactAllocationIndex =
                    collisionContactAllocationIndex;
                entities_[index].collisionContactIsPlayer =
                    collisionContactIsPlayer;
            }
            appendAllocationObject(ObjectKind::entity, index);
            return entities_[index];
        }
    }
    value.allocationIndex = entities_.size();
    entities_.push_back(value);
    appendAllocationObject(
        ObjectKind::entity,
        entities_.size() - 1
    );
    return entities_.back();
}

std::uint8_t Level1State::beginDestroyedTypeBonus(const std::int16_t destroyedType) noexcept {
    constexpr std::array<std::uint8_t, 14> bonusByType{{
        0, 7, 0, 0, 2, 6, 5, 0, 0, 0, 0, 3, 0, 4,
    }};
    if (
        destroyedType >= 0
        && static_cast<std::size_t>(destroyedType)
            < bonusByType.size()
    ) {
        if (lastDestroyedType_ == destroyedType) {
            ++consecutiveDestroyedTypeCount_;
        } else {
            consecutiveDestroyedTypeCount_ = 1;
        }
        const auto bonus = bonusByType[
            static_cast<std::size_t>(destroyedType)
        ];
        if (
            consecutiveDestroyedTypeCount_ == 3
            && bonus != 0
        ) {
            lastDestroyedType_ = 0;
            consecutiveDestroyedTypeCount_ = 0;
            return bonus;
        }
    }
    return 0;
}

void Level1State::repairBonusSystem(const std::uint8_t payload) noexcept {
    if (payload >= 1 && payload <= systemCount) {
        systemStatusLevels_[payload - 1] = 3;
    }
}

bool Level1State::registerDestroyedTypeBonus(
    const std::int16_t destroyedType,
    const Level1Entity& source
) noexcept {
    const auto bonus = beginDestroyedTypeBonus(destroyedType);
    if (bonus != 0) {
        auto pickup = entity(
            0,
            42,
            330,
            {
                source.position.x,
                source.position.y,
                100,
            },
            0,
            0,
            {254, 1, bonus, 0, 0, bonus}
        );
        pickup.rollVelocity = 0x0800;
        pickup.yawVelocity = 0x0800;
        allocateEntity(pickup, 2);
    }
    lastDestroyedType_ = destroyedType;
    return bonus != 0;
}

void Level1State::appendAllocationObject(
    const ObjectKind kind,
    const std::size_t index
) noexcept {
    const auto duplicate = std::find_if(
        allocationList_.begin(),
        allocationList_.end(),
        [kind, index](const ObjectReference& reference) noexcept {
            return reference.kind == kind && reference.index == index;
        }
    );
    if (duplicate == allocationList_.end()) {
        allocationList_.push_back({kind, index});
    }
}

void Level1State::removeAllocationObject(
    const ObjectKind kind,
    const std::size_t index
) noexcept {
    const auto found = std::find_if(
        allocationList_.begin(),
        allocationList_.end(),
        [kind, index](const ObjectReference& reference) noexcept {
            return reference.kind == kind && reference.index == index;
        }
    );
    if (found == allocationList_.end()) {
        return;
    }
    if (found != allocationList_.end() - 1) {
        *found = allocationList_.back();
    }
    allocationList_.pop_back();
}

bool Level1State::spawnDeathEffect(
    const Level1Entity& source
) noexcept {
    const auto distanceBucket = std::min<std::uint32_t>(
        source.presenterDistance / 0x1964U,
        5U
    );
    const auto firstFrame = static_cast<std::uint8_t>(
        distanceBucket * 3U + 13U
    );
    const auto lastFrame = static_cast<std::uint8_t>(
        distanceBucket * 3U + 17U
    );
    return spawnTransientEffect(source, firstFrame, lastFrame);
}

bool Level1State::spawnTransientEffect(
    const Level1Entity& source,
    const std::uint8_t firstFrame,
    const std::uint8_t lastFrame
) noexcept {
    auto available = std::find_if(
        deathEffects_.begin(),
        deathEffects_.end(),
        [](const Level1DeathEffect& effect) noexcept {
            return !effect.listed;
        }
    );
    if (available == deathEffects_.end()) {
        return false;
    }
    const auto poolSlot = static_cast<std::size_t>(
        std::distance(deathEffects_.begin(), available)
    );
    available->listed = true;
    available->entity.flags = 0x8C;
    available->entity.type = -1;
    available->entity.collisionRadius = 0;
    available->entity.position = source.position;
    available->entity.stateBytes4dTo52[0] = 252;
    available->entity.stateBytes4dTo52[1] =
        static_cast<std::uint8_t>(firstFrame + 2U);
    available->entity.stateBytes4dTo52[4] = lastFrame;
    appendAllocationObject(ObjectKind::deathEffect, poolSlot);
    return true;
}

void Level1State::refreshDeathEffectAllocationIndices() noexcept {
    const auto activeDynamicEntityCount = static_cast<std::size_t>(
        std::count_if(
            entities_.begin() + std::min(
                entityCount,
                entities_.size()
            ),
            entities_.end(),
            [](const Level1Entity& item) noexcept {
                return item.active();
            }
        )
    );
    const auto activeProjectileCount = static_cast<std::size_t>(
        std::count_if(
            projectiles_.begin(),
            projectiles_.end(),
            [](const Level1Projectile& projectile) noexcept {
                return projectile.active();
            }
        )
    );
    std::size_t activeEffectIndex = 0;
    for (auto& effect : deathEffects_) {
        if (!effect.listed) {
            continue;
        }
        // The pristine trace analyzer numbers pointers not present at the
        // entry boundary in address order. The projectile pool precedes the
        // FUN_1000_4580 transient pool in the original data segment.
        effect.entity.allocationIndex =
            entityCount
            + activeDynamicEntityCount
            + activeProjectileCount
            + activeEffectIndex;
        ++activeEffectIndex;
    }
}

std::uint32_t Level1State::objectPresenterDistance(
    const ObjectReference& reference
) const noexcept {
    if (
        reference.kind == ObjectKind::entity
        && reference.index < entities_.size()
    ) {
        return entities_[reference.index].presenterDistance;
    }
    if (
        reference.kind == ObjectKind::projectile
        && reference.index < projectiles_.size()
    ) {
        return projectiles_[reference.index].presenterDistance;
    }
    if (
        reference.kind == ObjectKind::deathEffect
        && reference.index < deathEffects_.size()
    ) {
        return deathEffects_[reference.index]
            .entity.presenterDistance;
    }
    return 0;
}

void Level1State::runCompletionCheck() noexcept {
    if (
        successState_
        || activeEnemyCount_ != 0
        || spawnedEnemyCount_ < maximumEnemyCount_
        || completionObjectAllocationIndex_
            != static_cast<std::size_t>(-1)
        || postWaveObjectAllocationIndex_
            != static_cast<std::size_t>(-1)
    ) {
        return;
    }
    // FUN_1000_4370 initializes every allocated actor through
    // FUN_1000_4862, consuming a default-position RNG value, then consumes
    // another for default yaw. FUN_1000_46c8 overwrites both fields for the
    // Type 7 completion object, but the RNG side effects remain observable.
    rng_.next();
    rng_.next();
    auto completion = entity(
        0,
        7,
        13052,
        player_.position,
        player_.yaw,
        static_cast<std::int16_t>(0x9000),
        {255, 100, 0, 0, 5, 0}
    );
    completion.pitch = player_.pitch;
    completion.roll = player_.roll;
    completion.position.z = 5000;
    advanceKinematics(completion);
    completion.forwardVelocity = 400;
    completion.weaponSlotCount = 3;
    auto& stored = allocateEntity(completion);
    completionObjectAllocationIndex_ = stored.allocationIndex;
    audioEvents_.push_back({loopTick_, 17});
    ++activeEnemyCount_;
    ++spawnedEnemyCount_;
}

std::int16_t Level1State::levelIndex() const noexcept {
    return levelIndex_;
}

std::uint16_t Level1State::loopTick() const noexcept {
    return loopTick_;
}

std::uint8_t Level1State::type11AnimationVariant() const noexcept {
    return type11AnimationVariant_;
}

const Level1Entity& Level1State::player() const noexcept {
    return player_;
}

const std::vector<Level1Entity>&
Level1State::entities() const noexcept {
    return entities_;
}

const std::vector<std::size_t>&
Level1State::activeListOrder() const noexcept {
    return activeListOrder_;
}

const std::array<Level1Projectile, Level1State::projectilePoolSize>&
Level1State::projectiles() const noexcept {
    return projectiles_;
}

const std::array<Level1DeathEffect, Level1State::deathEffectPoolSize>&
Level1State::deathEffects() const noexcept {
    return deathEffects_;
}

const std::vector<std::size_t>&
Level1State::semanticActiveListOrder() const noexcept {
    return semanticActiveListOrder_;
}

std::vector<Level1PresentationObject> Level1State::radarPresentationObjects() const {
    std::vector<Level1PresentationObject> result;
    result.reserve(objectListOrder_.size());
    for (const auto& reference : objectListOrder_) {
        if (reference.kind == ObjectKind::projectile) {
            const auto& projectile = projectiles_.at(reference.index);
            result.push_back({Level1PresentationKind::projectile, reference.index,
                projectile.type, projectile.flags, projectile.collisionRadius, projectile.position,
                static_cast<std::int8_t>(projectile.stateBytes4dTo54[0]), WorldActorRenderProfile::unknown});
            continue;
        }
        const auto effect = reference.kind == ObjectKind::deathEffect;
        const auto& actor = effect ? deathEffects_.at(reference.index).entity : entities_.at(reference.index);
        // Source45d2 can retain a terminal record after its allocation retires.
        result.push_back({effect ? Level1PresentationKind::deathEffect : Level1PresentationKind::entity,
            reference.index, actor.type, actor.flags, actor.collisionRadius, actor.position,
            static_cast<std::int8_t>(actor.stateBytes4dTo52[0]), actor.renderProfile});
    }
    return result;
}

std::vector<std::size_t> Level1State::projectileOrder() const {
    std::vector<std::size_t> order;
    for (const auto& reference : objectListOrder_) {
        if (reference.kind == ObjectKind::projectile && reference.index < projectiles_.size()
            && projectiles_[reference.index].active()) {
            order.push_back(projectiles_[reference.index].poolSlot);
        }
    }
    return order;
}

std::uint8_t Level1State::playerWeaponCooldown() const noexcept {
    return playerWeaponCooldown_;
}

const std::array<
    std::uint8_t,
    Level1State::playerProjectileSlots
>& Level1State::playerProjectileTimers() const noexcept {
    return playerProjectileTimers_;
}

const std::vector<Level1AudioEvent>&
Level1State::audioEvents() const noexcept {
    return audioEvents_;
}

const StellarRng& Level1State::rng() const noexcept {
    return rng_;
}

const Level1GroundState& Level1State::ground() const noexcept {
    return ground_;
}

std::uint32_t Level1State::score() const noexcept {
    return score_;
}

std::int16_t Level1State::lastDestroyedType() const noexcept {
    return lastDestroyedType_;
}

std::uint16_t Level1State::consecutiveDestroyedTypeCount() const noexcept {
    return consecutiveDestroyedTypeCount_;
}

std::int16_t Level1State::playerArmor() const noexcept {
    return playerArmor_;
}

const std::array<std::int8_t, Level1State::systemCount>&
Level1State::systemStatusLevels() const noexcept {
    return systemStatusLevels_;
}

const std::array<std::int16_t, Level1State::systemCount>&
Level1State::systemTimers() const noexcept {
    return systemTimers_;
}

std::int16_t Level1State::selectedSystem() const noexcept {
    return selectedSystem_;
}

bool Level1State::missionActive() const noexcept {
    return missionActive_;
}

bool Level1State::failureState() const noexcept {
    return failureState_;
}

bool Level1State::successState() const noexcept {
    return successState_;
}

bool Level1State::exitState() const noexcept {
    return exitState_;
}

std::int16_t Level1State::activeEnemyCount() const noexcept {
    return activeEnemyCount_;
}

std::int16_t Level1State::spawnedEnemyCount() const noexcept {
    return spawnedEnemyCount_;
}

std::int16_t Level1State::maximumEnemyCount() const noexcept {
    return maximumEnemyCount_;
}

const Level1Entity* Level1State::lastSpawnCheckpoint() const noexcept {
    return hasLastSpawnCheckpoint_ ? &lastSpawnCheckpoint_ : nullptr;
}

std::uint16_t Level1State::lastSpawnTick() const noexcept {
    return lastSpawnTick_;
}

const StellarRng& Level1State::lastSpawnRng() const noexcept {
    return lastSpawnRng_;
}

const Level1Entity* Level1State::completionObject() const noexcept {
    if (
        completionObjectAllocationIndex_
        >= entities_.size()
    ) {
        return nullptr;
    }
    const auto& item = entities_[completionObjectAllocationIndex_];
    return item.active() ? &item : nullptr;
}

const Level1Entity* Level1State::postWaveObject() const noexcept {
    if (postWaveObjectAllocationIndex_ >= entities_.size()) {
        return nullptr;
    }
    const auto& item = entities_[postWaveObjectAllocationIndex_];
    return item.active() ? &item : nullptr;
}

std::size_t Level1State::postWaveObjectAllocationIndex() const noexcept {
    return postWaveObjectAllocationIndex_;
}

}  // namespace stellar7
