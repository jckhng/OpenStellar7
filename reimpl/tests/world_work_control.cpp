#include "stellar7/render_work.hpp"
#include "stellar7/world_traversal.hpp"

#include <cassert>
#include <limits>
#include <stdexcept>

int main() {
    using namespace stellar7;
    assert(solidSpanWork({{0, 8}}).portInstructions == 3);
    WorldViewSetupInput input{{0, 0, 0}, {}, {}, 0x88, 7, {24, 294, 24, 145}, {256, -256, 362}};
    const auto work = worldViewSetupOperations(input);
    const std::array<std::int16_t, 9> identity{16384, 0, 0, 0, 16384, 0, 0, 0, 16384};
    assert(work.view.matrix.values == identity && work.view.matrix.mode == 0);
    assert(work.view.perspectiveScale == 128);
    assert(work.view.viewport == input.viewport);
    assert(work.transformedLight == input.light);
    auto radarInput = input;
    radarInput.origin = {0, 0, 200000};
    radarInput.viewAngles = {0xc000, 0, 0};
    radarInput.viewport = {112, 205, 146, 180};
    const auto radar = radarViewSetupOperations(radarInput);
    const std::array<std::int16_t, 9> radarMatrix{16384, 0, 0, 0, 0, 16384, 0, -16384, 0};
    assert(radar.view.matrix.values == radarMatrix && radar.view.matrix.mode == 3);
    assert(radar.view.origin == radarInput.origin && radar.view.perspectiveScale == 128);
    assert((radar.transformedLight == FixedPosition{256, -362, -256}));
    assert(radar.operations.size() == 5 && radar.operations[0].count == 15);
    assert(radar.operations[1].kind == NormalWorkKind::repeatString && radar.operations[1].count == 6);
    assert(radar.operations[2].kind == NormalWorkKind::instructions && radar.operations[2].count == 431);
    assert(radar.operations[3].kind == NormalWorkKind::repeatString && radar.operations[3].count == 9);
    assert(radar.operations[4].kind == NormalWorkKind::instructions && radar.operations[4].count == 269);
    const auto radarRejected = [](const WorldViewSetupInput& state) {
        try {
            radarViewSetupOperations(state);
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };
    auto wrongRadar = radarInput;
    wrongRadar.actorAngles[2] = 1;
    assert(radarRejected(wrongRadar));
    wrongRadar = radarInput;
    wrongRadar.viewAngles[0] = 0;
    assert(radarRejected(wrongRadar));
    wrongRadar = radarInput;
    wrongRadar.viewport.right = 206;
    assert(radarRejected(wrongRadar));
    wrongRadar = radarInput;
    wrongRadar.projectionShift = 15;
    assert(radarRejected(wrongRadar));
    wrongRadar = radarInput;
    wrongRadar.origin.z = 199999;
    assert(radarRejected(wrongRadar));
    wrongRadar = radarInput;
    wrongRadar.flags = 0x80;
    assert(radarRejected(wrongRadar));
    wrongRadar = radarInput;
    wrongRadar.light.x = 255;
    assert(radarRejected(wrongRadar));
    assert(radarRejected(input));
    assert(work.operations.size() == 7);
    assert(work.operations[1].kind == NormalWorkKind::repeatString && work.operations[1].count == 6);
    std::uint64_t dispatches = 0, strings = 0;
    for (const auto& operation : work.operations) {
        if (operation.kind == NormalWorkKind::instructions) {
            dispatches += operation.count;
        } else {
            ++dispatches;
            strings += operation.count;
        }
    }
    assert(dispatches == 223 && strings == 9);
    const WorldActorWorkInput actor{0, {-24016, 28112, 0}};
    const auto center = worldFirstCenterOperations(work.view, actor);
    assert(center.center == actor.position && center.relativePosition == actor.position);
    assert(center.operations.size() == 2);
    assert(center.operations[0].count == 45 && center.operations[1].count == 27);
    auto translatedView = work.view;
    translatedView.origin = {16, -32, 64};
    assert((worldFirstCenterOperations(translatedView, actor).center == FixedPosition{-24032, 28144, -64}));
    bool unsupportedCenter = false;
    try {
        worldFirstCenterOperations(work.view, {0x40, actor.position});
    } catch (const std::invalid_argument&) {
        unsupportedCenter = true;
    }
    assert(unsupportedCenter);
    const auto caller = worldModelCallerOperations(actor.position, 559, 0, 0);
    assert(caller.minimumDepth == 27553 && caller.projectedRadius == 2);
    assert(caller.operations[0].count == 30 && caller.operations[1].count == 111
        && caller.operations[2].count == 17);
    const auto behind = worldModelCallerOperations({11728, -24016, 0}, 559, 0, 0);
    assert(behind.minimumDepth == 128 && behind.projectedRadius == 256);
    assert(behind.operations[0].count == 31 && behind.operations[2].count == 19);
    assert(worldModelReturnOperations(WorldActorContinuation::nextActor).front().count == 10);
    assert(worldModelReturnOperations(WorldActorContinuation::returnToCaller).front().count == 16);
    const auto nextCenter = worldNextCenterOperations(work.view, actor);
    assert(nextCenter.center == center.center && nextCenter.operations.front().count == 37);
    bool callbackRejected = false;
    try {
        worldModelCallerOperations(actor.position, 559, 0, 1);
    } catch (const std::invalid_argument&) {
        callbackRejected = true;
    }
    assert(callbackRejected);
    const auto cullDispatches = [](const WorldModelCullWork& result) {
        std::uint64_t count = 0;
        for (const auto& operation : result.operations) {
            assert(operation.kind == NormalWorkKind::instructions);
            count += operation.count;
        }
        return count;
    };
    const auto negativeVisible = worldModelCullOperations({-24016, 28112, 0}, 559);
    assert(!negativeVisible.culled && !negativeVisible.clipped && cullDispatches(negativeVisible) == 612);
    const auto positiveVisible = worldModelCullOperations({12288, 24576, 0}, 860);
    assert(!positiveVisible.culled && cullDispatches(positiveVisible) == 604);
    const std::array<FixedPosition, 5> wideCenters{{
        {8192, 31899, 0}, {8192, 31900, 0}, {8192, 32216, 0},
        {8192, 33635, 0}, {8192, 33636, 0}}};
    const std::array<std::uint64_t, 5> wideDispatches{604, 610, 610, 610, 616};
    for (std::size_t index = 0; index < wideCenters.size(); ++index) {
        const auto wide = worldModelCullOperations(wideCenters[index], 868);
        assert(!wide.culled && !wide.clipped && wide.projectedRadius == 3);
        assert(cullDispatches(wide) == wideDispatches[index]);
    }
    assert(cullDispatches(worldModelCullOperations({-8192, 32216, 0}, 868)) == 618);
    bool highDepthRejected = false;
    try {
        worldModelCullOperations({8192, 64668, 0}, 868);
    } catch (const std::invalid_argument&) {
        highDepthRejected = true;
    }
    assert(highDepthRejected);
    const auto lateralCull = worldModelCullOperations({-20480, 4096, 0}, 860);
    assert(lateralCull.culled && cullDispatches(lateralCull) == 321);
    const auto nearCull = worldModelCullOperations({11728, -24016, 0}, 559);
    assert(nearCull.culled && cullDispatches(nearCull) == 22);
    for (const auto depth : {0, 56, 127}) {
        const auto shallowCull = worldModelCullOperations({-20480, depth - 860, 0}, 860);
        assert(shallowCull.culled && !shallowCull.clipped && cullDispatches(shallowCull) == 25);
    }
    const auto nearEdge = worldModelCullOperations({-20480, 128 - 860, 0}, 860);
    assert(nearEdge.culled && cullDispatches(nearEdge) == 321);
    const auto liftedNearCull = worldModelCullOperations({8816, -21104, 45}, 559);
    assert(liftedNearCull.culled && cullDispatches(liftedNearCull) == 22);
    bool liftedVisibleRejected = false;
    try {
        worldModelCullOperations({12288, 24576, 45}, 860);
    } catch (const std::invalid_argument&) {
        liftedVisibleRejected = true;
    }
    assert(liftedVisibleRejected);
    for (const auto altitude : {-1, 1001}) {
        bool heightRejected = false;
        try {
            worldModelCullOperations({8816, -21104, altitude}, 559);
        } catch (const std::invalid_argument&) {
            heightRejected = true;
        }
        assert(heightRejected);
    }
    const auto clippedModel = worldModelCullOperations({12288, 10000, 0}, 860);
    assert(!clippedModel.culled && clippedModel.clipped);
    const auto rightClipped = worldModelCullOperations({-20768, 20564, 0}, 559);
    assert(!rightClipped.culled && rightClipped.clipped && rightClipped.projectedRadius == 3);
    std::uint64_t clippedDispatches = 0;
    for (const auto& operation : rightClipped.operations) {
        assert(operation.kind == NormalWorkKind::instructions);
        clippedDispatches += operation.count;
    }
    assert(clippedDispatches == 605);
    const auto clipEdge = worldModelCullOperations({-20880, 20876, 0}, 559);
    assert(!clipEdge.culled && !clipEdge.clipped);
    const auto upperClipped = worldModelCullOperations({1001, 3000, 0}, 1000);
    assert(!upperClipped.culled && upperClipped.clipped);
    Model3d model{};
    model.collisionRadius = 559;
    ModelGroup farther{};
    farther.vertices = {{{0, -100, 0}}};
    ModelGroup nearer{};
    nearer.vertices = {{{0, 100, 0}}};
    model.lods.push_back({20, 128, {farther, nearer}});
    const auto setup = worldModelSetupOperations({-24016, 28112, 0}, {}, 2, 13, model, 256, 256);
    assert(setup.matrix.mode == 0 && setup.selectedLodIndex == 0);
    assert((setup.translation == FixedPosition{-24016, 28112, 0}));
    assert((setup.groupOrder == std::vector<std::size_t>{1, 0}));
    assert((setup.groupDepths == std::vector<std::int32_t>{-2811200, 2811200}));
    assert(setup.vertexPrecision == 0 && setup.smallRadius == 1);
    auto multipleLods = model;
    multipleLods.lods.front().threshold = 2;
    multipleLods.lods.push_back({2, 128, {nearer, farther}});
    const auto equalLod = worldModelSetupOperations({-24016, 28112, 0}, {}, 2, 13, multipleLods, 256, 256);
    assert(equalLod.selectedLodIndex == 0 && equalLod.groupOrder == setup.groupOrder);
    multipleLods.lods.front().threshold = 1;
    const auto nextLod = worldModelSetupOperations({-24016, 28112, 0}, {}, 2, 13, multipleLods, 256, 256);
    assert(nextLod.selectedLodIndex == 1);
    assert((nextLod.groupOrder == std::vector<std::size_t>{0, 1}));
    assert((nextLod.groupDepths == std::vector<std::int32_t>{2811200, -2811200}));
    multipleLods.lods.back().threshold = 1;
    bool lodFallbackRejected = false;
    try {
        worldModelSetupOperations({-24016, 28112, 0}, {}, 2, 13, multipleLods, 256, 256);
    } catch (const std::invalid_argument&) {
        lodFallbackRejected = true;
    }
    assert(lodFallbackRejected);
    const auto equalAxes = worldModelSetupOperations({-20880, 20880, 0}, {0, 0, 4864}, 3, 13, model, 256, 256);
    const auto xDominant = worldModelSetupOperations({-20880, 20876, 0}, {0, 0, 4864}, 3, 13, model, 256, 256);
    assert((xDominant.translation == FixedPosition{-20880, 20876, 0}));
    assert(xDominant.matrix.values == equalAxes.matrix.values);
    assert(xDominant.vertexPrecision == 0 && xDominant.smallRadius == 1 && xDominant.selectedLodIndex == 0);
    const auto prefix = [](const WorldModelSetupWork& state) {
        std::uint64_t result = 0;
        for (const auto& operation : state.operations) {
            if (operation.kind == NormalWorkKind::string) {
                return result;
            }
            result += operation.kind == NormalWorkKind::instructions ? operation.count : 1;
        }
        return result;
    };
    assert(prefix(xDominant) == prefix(equalAxes) + 2);
    const auto yDominant = worldModelSetupOperations({-20880, 20881, 0}, {0, 0, 4864}, 3, 13, model, 256, 256);
    assert(prefix(yDominant) == prefix(equalAxes));
    const auto detailEdge = worldModelSetupOperations({-31441, 31440, 0}, {0, 0, 4864}, 2, 13, model, 256, 256);
    assert(detailEdge.vertexPrecision == 0);
    bool detailLimitRejected = false;
    try {
        worldModelSetupOperations({-31442, 31441, 0}, {0, 0, 4864}, 2, 13, model, 256, 256);
    } catch (const std::invalid_argument&) {
        detailLimitRejected = true;
    }
    assert(detailLimitRejected);
    bool wrongProfileRejected = false;
    try {
        worldModelSetupOperations({-24016, 28112, 0}, {}, 2, 13, model, 255, 256);
    } catch (const std::invalid_argument&) {
        wrongProfileRejected = true;
    }
    assert(wrongProfileRejected);
    bool wrongRadiusRejected = false;
    try {
        worldModelSetupOperations({-24016, 28112, 0}, {}, 3, 13, model, 256, 256);
    } catch (const std::invalid_argument&) {
        wrongRadiusRejected = true;
    }
    assert(wrongRadiusRejected);
    input.origin = {std::numeric_limits<std::int32_t>::min(), 0x12345678, std::numeric_limits<std::int32_t>::max()};
    assert(worldViewSetupOperations(input).view.origin == input.origin);
    const auto rejected = [](const WorldViewSetupInput& state) {
        try {
            worldViewSetupOperations(state);
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };
    auto invalid = input;
    invalid.flags = 0x89;
    assert(rejected(invalid));
    invalid = input;
    invalid.projectionShift = 6;
    assert(rejected(invalid));
    invalid = input;
    invalid.viewport.right = 295;
    assert(rejected(invalid));
    invalid = input;
    invalid.light.z = 0;
    assert(rejected(invalid));
    for (std::size_t axis = 0; axis < 3; ++axis) {
        invalid = input;
        invalid.viewAngles[axis] = 1;
        assert(rejected(invalid));
        invalid = input;
        invalid.actorAngles[axis] = 1;
        assert(rejected(invalid));
    }
}

