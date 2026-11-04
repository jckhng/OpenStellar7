#include "stellar7/render_work.hpp"
#include "stellar7/ground_steering_work.hpp"
#include "stellar7/world_scene_work.hpp"

#include <cassert>
#include <stdexcept>

namespace {
std::uint64_t dispatches(const std::vector<stellar7::NormalWorkOperation>& operations) {
    std::uint64_t result = 0;
    for (const auto& operation : operations) {
        result += operation.kind == stellar7::NormalWorkKind::instructions ? operation.count : 1;
    }
    return result;
}
std::uint64_t operationHash(const std::vector<stellar7::NormalWorkOperation>& operations) {
    constexpr std::uint64_t offset = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t hash = offset;
    std::uint64_t pending = 0;
    const auto append = [&](const auto kind, const auto count) {
        hash = (hash ^ static_cast<std::uint64_t>(kind)) * prime;
        hash = (hash ^ count) * prime;
    };
    for (const auto& operation : operations) {
        if (operation.kind == stellar7::NormalWorkKind::instructions) {
            pending += operation.count;
            continue;
        }
        if (pending != 0) {
            append(stellar7::NormalWorkKind::instructions, pending);
            pending = 0;
        }
        append(operation.kind, operation.count);
    }
    if (pending != 0) {
        append(stellar7::NormalWorkKind::instructions, pending);
    }
    return hash;
}
template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
}

int main() {
    using namespace stellar7;
    WorldViewSetupInput input{{-1, 9, 0}, {}, {}, 0x88, 7, {24, 294, 24, 145}, {256, -256, 362}};
    assert(dispatches(worldViewSetupOperations(input).operations) == 223);
    input.actorAngles[2] = 80;
    const auto left = worldViewSetupOperations(input);
    assert(dispatches(left.operations) == 286);
    assert((left.transformedLight == FixedPosition{254, -258, 362}));
    assert((left.view.matrix.values == std::array<std::int16_t, 9>{16383, -125, 0, 125, 16383, 0, 0, 0, 16384}));
    input.actorAngles[2] = 0xffb0;
    const auto right = worldViewSetupOperations(input);
    assert(dispatches(right.operations) == 288);
    assert((right.transformedLight == FixedPosition{257, -255, 362}));
    const auto combined = composeFixedMatricesQ14(makeFixedMatrixQ14(0, 0, 0x4800), left.view.matrix, 0x5240);
    assert((combined.values == std::array<std::int16_t, 9>{-3077, 16093, 0, -16096, -3076, 0, 0, 0, 16384}));
    assert(combined.values != composeFixedMatricesQ14(makeFixedMatrixQ14(0, 0, 0x4800), left.view.matrix, 0).values);
    input.actorAngles[2] = 1;
    rejects([&] { worldViewSetupOperations(input); });
    input.actorAngles = {1, 0, 80};
    rejects([&] { worldViewSetupOperations(input); });

    std::array<IndexedBitmap, 8> bitmaps;
    bitmaps.fill({8, 1, std::vector<std::uint8_t>(8, 1)});
    GroundControlState ground{};
    ground.points.fill({320 * 32, 100 * 32});
    const StellarRng rng(std::array<std::uint16_t, StellarRng::wordCount>{}, 0, 0, 0, 0);
    const auto still = groundSteeringExecution(ground, rng, bitmaps, {24, 295, 24, 145});
    assert(dispatches(still.operations) == 581);
    assert(dispatches(groundClippedExecution(ground, rng, bitmaps, {24, 295, 24, 145}).operations) == 581);
    for (const auto turn : {80, -80}) {
        ground.turn = static_cast<std::int16_t>(turn);
        const auto moved = groundSteeringExecution(ground, rng, bitmaps, {24, 295, 24, 145});
        assert(dispatches(moved.operations) == 653 && moved.rng.calls() == 12);
        assert(moved.ground.points() == still.ground.points());
    }
    ground.turn = 80;
    ground.speed = 10;
    assert(dispatches(groundSteeringExecution(ground, rng, bitmaps, {24, 295,24,145}).operations) == 1091);
    ground.turn = 513;
    rejects([&] { groundSteeringExecution(ground, rng, bitmaps, {24,295,24,145}); });

    std::vector<IndexedBitmap> frames;
    for (std::uint8_t page = 0; page < 4; ++page) {
        frames.push_back({256, 16, std::vector<std::uint8_t>(256 * 16, page + 1)});
    }
    frames.push_back({32, 27, std::vector<std::uint8_t>(32 * 27)});
    const std::array<std::int16_t, 5> yaws{0, 80, -80, -240, 2048};
    const std::array<std::uint64_t, 5> hashes{14873861298023990709ULL, 16769265293480982980ULL,
        183095063055294222ULL, 12949281887807827097ULL, 11856660666789714059ULL};
    for (std::size_t index = 0; index < yaws.size(); ++index) {
        const HorizonWorkState state{24, 145, yaws[index], 90, 256, 16,
            HorizonBackground::gradient, HorizonOverlay::enabled, 233, 246};
        const auto work = horizonExecution(state, frames);
        assert(operationHash(work.operations) == hashes[index]);
        assert(work.pixels.size() == 272 * 122);
    }
}
