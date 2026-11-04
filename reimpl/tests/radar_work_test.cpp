#include "stellar7/radar_work.hpp"
#include "stellar7/radar_state.hpp"
#include <cassert>
#include <stdexcept>

int main() {
    using namespace stellar7;
    const WorldViewSetupInput view{{0, 0, 200000}, {0xc000, 0, 0}, {0, 0, 0},
        0x88, 7, {112, 205, 146, 180}, {256, -256, 362}};
    auto turningView = view;
    turningView.actorAngles[2] = 0xffb0;
    const auto turning = radarViewSetupOperations(turningView);
    const std::array<std::int16_t, 9> sourceTurningMatrix{16381, 1, 127, -127, 1, 16384, 0, -16384, 0};
    assert(turning.view.matrix.values == sourceTurningMatrix);
    assert((turning.transformedLight == FixedPosition{257, -362, -255}));
    assert(turning.operations.size() == 3);
    assert(turning.operations[0].kind == NormalWorkKind::instructions && turning.operations[0].count == 15);
    assert(turning.operations[1].kind == NormalWorkKind::repeatString && turning.operations[1].count == 6);
    assert(turning.operations[2].kind == NormalWorkKind::instructions && turning.operations[2].count == 859);
    RadarActorInput actor{{0x88, {0, 0, 0}}, 1024, 13, -1, 0, WorldActorRenderProfile::noHandles, 17, 18, 19};
    auto work = radarTraversalOperations(view, {actor}, {0, 0});
    assert(work.actors.size() == 1);
    assert(work.actors[0].center.y == 200000);
    assert(work.actors[0].projectedX == 158 && work.actors[0].projectedY == 163);
    assert(work.actors[0].flags == 0x8c);
    assert(work.actors[0].projectedRadius == 0);
    auto elevated = actor;
    elevated.actor.position = {1562, 0, 90};
    const auto lifted = radarTraversalOperations(view, {elevated}, {0, 0});
    assert(lifted.actors[0].center.y == 199910);
    assert(lifted.actors[0].cachedCenter.y == 199910);
    assert(lifted.actors[0].projectedX == 159 && lifted.actors[0].projectedY == 163);
    std::vector<NormalWorkOperation> compact;
    for (const auto& operation : work.operations) {
        if (!compact.empty() && compact.back().kind == operation.kind) {
            compact.back().count += operation.count;
        } else {
            compact.push_back(operation);
        }
    }
    const std::vector<NormalWorkOperation> expected{{NormalWorkKind::instructions, 23},
        {NormalWorkKind::repeatString, 6}, {NormalWorkKind::instructions, 431},
        {NormalWorkKind::repeatString, 9}, {NormalWorkKind::instructions, 739},
        {NormalWorkKind::repeatString, 6}, {NormalWorkKind::instructions, 329}};
    assert(compact.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        assert(compact[i].kind == expected[i].kind && compact[i].count == expected[i].count);
    }
    actor.radius = 20979;
    // Normalizing numerator and denominator loses bits: exact host division gives14.
    assert(radarTraversalOperations(view, {actor}, {0, 0}).actors[0].projectedRadius == 15);
    actor.radius = 1024;
    actor.actor.position.y = 28224;
    work = radarTraversalOperations(view, {actor}, {0, 0});
    assert(work.actors[0].flags == 0x88);
    assert(work.actors[0].projectedX == 17 && work.actors[0].projectedY == 18);
    assert(work.actors[0].projectedRadius == 19);
    assert(work.actors[0].cachedCenter.z == 28224);
    for (const auto position : {FixedPosition{-100000, 0, 0}, FixedPosition{100000, 0, 0},
            FixedPosition{0, 100000, 0}, FixedPosition{0, -100000, 0}}) {
        actor.actor.position = position;
        assert(!radarTraversalOperations(view, {actor}, {0, 0}).actors[0].projected);
    }
    actor.callback = 1;
    bool rejected = false;
    try { radarTraversalOperations(view, {actor}, {0, 0}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    actor.callback = 0;
    rejected = false;
    try { radarTraversalOperations(view, {actor}, {1, 0}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    assert(radarTraversalOperations(view, {}, {0, 0}).actors.empty());
    const RadarRetainedState state{680, 42, {0xa800, 0xa000, 0xa400, 0xa000, 0xa800}};
    const IndexedBitmap panel{120, 56, std::vector<std::uint8_t>(120 * 56, 7)};
    const RadarRetainedState level1State{680, 42, {0xa400, 0xa800, 0xa000, 0xa800, 0xa400}};
    std::vector<RadarActorInput> level1Actors{
        {{0, {-2048, 2048, 0}}, 1342, 1, -1, 0, WorldActorRenderProfile::noHandles, 17, 18, 19},
        {{0, {8192, 8192, 1000}}, 748, 11, -1, 0x130c,
            WorldActorRenderProfile::heapSelectorDeferredAnimation, 17, 18, 19},
        {{0, {16384, 16384, 0}}, 862, 29, -2, 0, WorldActorRenderProfile::noHandles, 17, 18, 19},
        {{0, {16384, -16384, 0}}, 862, 29, -2, 0, WorldActorRenderProfile::noHandles, 17, 18, 19},
        {{0, {-16384, 16384, 0}}, 862, 29, -2, 0, WorldActorRenderProfile::noHandles, 17, 18, 19},
        {{0, {-16384, -16384, 0}}, 862, 29, -2, 0, WorldActorRenderProfile::noHandles, 17, 18, 19}};
    Model3d type11Model{};
    type11Model.flags = 1;
    type11Model.animationLimits = {8};
    const auto initializedLevel1 = level1RadarInitializationOperations(level1State, view, {}, 0, panel, {228, 138});
    assert(initializedLevel1.cpuWorkQualified && initializedLevel1.actors.empty());
    assert(initializedLevel1.state.pages.cached == 0xa400 && initializedLevel1.state.pages.back == 0xa800);
    RadarState level1Owner({panel});
    const auto loaderRadar = level1Owner.initializeLevel1(680, {}, 0, level1State.pages);
    assert(loaderRadar.pageCopy && loaderRadar.pageCopy->sourcePage == 0xa800);
    assert(loaderRadar.pageCopy->destinationPage == 0xa000 && level1Owner.armorStrip() == 42);
    std::vector<RadarStateActor> ownedLevel1Actors;
    for (std::size_t allocation = 0; allocation < level1Actors.size(); ++allocation) {
        const auto& input = level1Actors[allocation];
        level1Owner.initializeAllocation(allocation);
        ownedLevel1Actors.push_back({allocation, input.actor, input.radius, input.type,
            input.category, input.callback, input.renderProfile});
    }
    const auto firstOwnedRadar = level1Owner.drawLevel1(680, {}, 0, ownedLevel1Actors,
        level1State.pages, {}, type11Model, {5, 3, 1});
    assert(firstOwnedRadar.animation.countdown == 2 && level1Owner.projection(1).cachedCenter.y == 199000);
    const auto adjacentOwnedRadar = level1Owner.drawLevel1(680, {}, 0, ownedLevel1Actors,
        {0xa400, 0xa000, 0xa800, 0xa000, 0xa400}, {}, type11Model, {5, 1, 1});
    assert(adjacentOwnedRadar.animation.countdown == 0 && !adjacentOwnedRadar.radar.pageCopy);
    bool invalidOwnedRadar = false;
    try {
        level1Owner.drawLevel1(680, {}, 0, ownedLevel1Actors, level1State.pages,
            {}, type11Model, {5, 6, 1});
    } catch (const std::invalid_argument&) { invalidOwnedRadar = true; }
    assert(invalidOwnedRadar && level1Owner.projection(1).cachedCenter.y == 199000);
    for (const auto countdown : {3, 1}) {
        const auto level1 = level1RadarOperations(level1State, view, {}, 0, level1Actors,
            {}, panel, {228, 138}, type11Model, {5, static_cast<std::uint8_t>(countdown), 1});
        assert(level1.radar.cpuWorkQualified && !level1.radar.operations.empty());
        assert(level1.animation.countdown == countdown - 1 && level1.animation.variant == 1);
        assert(level1.radar.actors.size() == level1Actors.size());
        assert(level1.radar.pixels.size() == 120 * 56 + 6);
        assert(level1.radar.actors[1].cachedCenter.y == 199000);
        std::vector<NormalWorkOperation> ordered;
        for (const auto& operation : level1.radar.operations) {
            if (!ordered.empty() && ordered.back().kind == operation.kind) {
                ordered.back().count += operation.count;
            } else {
                ordered.push_back(operation);
            }
        }
        // Complete synthetic original8515->8518 stream, including all56 panel rows.
        std::vector<NormalWorkOperation> original{{NormalWorkKind::instructions, 111},
            {NormalWorkKind::port16Write, 1}, {NormalWorkKind::instructions, 2},
            {NormalWorkKind::port16Write, 1}, {NormalWorkKind::instructions, 9}};
        for (unsigned row = 0; row < 56; ++row) {
            original.push_back({NormalWorkKind::repeatString, 30});
            original.push_back({NormalWorkKind::instructions, row == 55 ? 5U : 4U});
        }
        original.push_back({NormalWorkKind::port16Write, 1});
        original.push_back({NormalWorkKind::instructions, 35});
        original.push_back({NormalWorkKind::repeatString, 6});
        original.push_back({NormalWorkKind::instructions, 431});
        original.push_back({NormalWorkKind::repeatString, 9});
        for (const auto dispatches : {769, 803, 808, 815, 833, 863}) {
            original.push_back({NormalWorkKind::instructions, static_cast<std::uint64_t>(dispatches)});
            original.push_back({NormalWorkKind::repeatString, 6});
        }
        original.push_back({NormalWorkKind::instructions, 428});
        for (unsigned marker = 0; marker < 6; ++marker) {
            original.push_back({NormalWorkKind::port16Write, 1});
            original.push_back({NormalWorkKind::instructions, marker == 0 ? 69U : marker == 5 ? 35U : 70U});
        }
        assert(ordered.size() == original.size());
        for (std::size_t index = 0; index < original.size(); ++index) {
            assert(ordered[index].kind == original[index].kind && ordered[index].count == original[index].count);
        }
        for (const auto& output : level1.radar.actors) {
            assert(output.flags == 0 && output.projectedY == 0);
        }
        for (const auto& pixel : level1.radar.pixels) {
            assert(pixel.page == 0xa800);
        }
        auto flatReference = level1Actors;
        for (auto& reference : flatReference) {
            reference.type = reference.category == -2 ? 30 : 13;
            reference.callback = 0;
            reference.renderProfile = WorldActorRenderProfile::noHandles;
        }
        const auto reference = radarOperations(state, view, {}, 0, flatReference, {}, panel, {228, 138});
        std::uint64_t referenceDispatches = 0;
        std::uint64_t level1Dispatches = 0;
        for (const auto& operation : reference.operations) {
            if (operation.kind == NormalWorkKind::instructions) { referenceDispatches += operation.count; }
        }
        for (const auto& operation : level1.radar.operations) {
            if (operation.kind == NormalWorkKind::instructions) { level1Dispatches += operation.count; }
        }
        // BBCE/BBF4 guards add8 dispatches; actual130C contributes15 once.
        assert(level1Dispatches == referenceDispatches + 8 + 15);
    }
    for (unsigned mutation = 0; mutation < 8; ++mutation) {
        auto invalidActors = level1Actors;
        auto invalidState = level1State;
        Level1AnimationState invalidAnimation{5, 3, 1};
        if (mutation == 0) { invalidActors[0].type = 4; }
        if (mutation == 1) { invalidActors[1].callback = 0; }
        if (mutation == 2) { invalidActors[1].actor.position.z = 950; }
        if (mutation == 3) { invalidActors[2].category = -1; }
        if (mutation == 4) { invalidState.pages.cached = 0xa800; }
        if (mutation == 5) { invalidAnimation.countdown = 6; }
        if (mutation == 6) { invalidActors.pop_back(); }
        if (mutation == 7) { invalidActors[0].actor.flags = 4; }
        rejected = false;
        try {
            level1RadarOperations(invalidState, view, {}, 0, invalidActors, {}, panel,
                {228, 138}, type11Model, invalidAnimation);
        } catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    auto lastCallbackActors = level1Actors;
    const auto secondActor = lastCallbackActors[1];
    lastCallbackActors[1] = lastCallbackActors.back();
    lastCallbackActors.back() = secondActor;
    const auto lastCallback = level1RadarOperations(level1State, view, {}, 0, lastCallbackActors,
        {}, panel, {228, 138}, type11Model, {5, 1, 1});
    assert(lastCallback.animation.countdown == 0);
    assert(lastCallback.radar.actors.back().cachedCenter.y == 199000);
    for (const std::uint16_t yaw : {1, 15, 30116, 65535}) {
        const auto pixels = radarPresentationPixels(state, view, {}, yaw, {}, {}, panel, {228, 138});
        auto expectedMatrix = composeFixedMatricesQ14(makeFixedMatrixQ14(0xc000, 0, 0),
            makeFixedMatrixQ14(0, 0, yaw), 0x5240);
        transpose(expectedMatrix);
        assert(pixels.setup.view.matrix.values == expectedMatrix.values);
        assert(pixels.setup.view.matrix.mode == expectedMatrix.mode);
        assert(!pixels.cpuWorkQualified && pixels.operations.empty() && pixels.setup.operations.empty());
        rejected = false;
        try { radarOperations(state, view, {}, yaw, {}, {}, panel, {228, 138}); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    actor.actor.flags = 0;
    actor.actor.position = {0, 0, 0};
    const auto outer = radarOperations(state, view, {0, 0, 0}, 0, {actor}, {0, 0}, panel, {228, 138});
    assert(outer.operations.front().kind == NormalWorkKind::instructions);
    assert(outer.operations.front().count == 44); // Caller/save loop/CALLF/stub, excludes opaque body.
    assert(outer.operations.back().count == 16); // Unchanged armor and698a RET once.
    assert(outer.actors[0].flags == 0 && outer.actors[0].projectedY == 0);
    assert(outer.actors[0].projectedX == 158);
    assert(outer.enemyFlag == 0 && outer.rasterStyle == 0 && outer.radarPass == 0);
    assert(outer.drawColor == 0xe8 && outer.fillColor == 0xe8);
    assert(outer.viewport == Viewport({112, 206, 146, 179}));
    assert(outer.state.armorStrip == 42 && outer.state.pages.draw == state.pages.draw);
    assert(outer.pixels.size() == 120 * 56 + 1);
    assert(outer.pixels.back().point.x == 154 && outer.pixels.back().point.y == 163);
    assert(outer.pixels.back().color == 228 && outer.pixels.back().page == 0xa000);
    actor.category = -2;
    assert(radarOperations(state, view, {0, 0, 0}, 0, {actor}, {0, 0}, panel, {228, 138}).enemyFlag == 1);
    actor.category = -1;
    actor.actor.position = {-71875, 0, 0}; // Projector admitsX112; markerX108 clips.
    assert(radarOperations(state, view, {0, 0, 0}, 0, {actor}, {0, 0}, panel, {228, 138}).pixels.size() == 120 * 56);
    auto bottom = actor;
    bottom.actor.position = {0, -28000, 0};
    const auto bottomWork = radarOperations(state, view, {0, 0, 0}, 0, {bottom}, {0, 0}, panel, {228, 138});
    assert(bottomWork.actors[0].projected);
    assert(bottomWork.pixels.size() == 120 * 56); // ProjectorY180 passes; markerY180 rejects.
    auto zeroPanel = panel;
    zeroPanel.pixels[0] = 0;
    const auto zeroWork = radarOperations(state, view, {0, 0, 0}, 0, {}, {0, 0}, zeroPanel, {228, 138});
    assert(zeroWork.pixels.size() == 120 * 56 && zeroWork.pixels[0].color == 0);
    auto startup = state;
    startup.armor = 488;
    startup.armorStrip = 0;
    const auto initialized = radarOperations(startup, view, {0, 0, 0}, 0, {}, {0, 0}, panel, {228, 138});
    assert(initialized.state.armorStrip == 30 && initialized.drawColor == 239);
    assert(initialized.pageCopy && initialized.pageCopy->sourcePage == startup.pages.back);
    assert(initialized.pageCopy->destinationPage == startup.pages.front);
    assert(initialized.pageCopy->position.x == 112 && initialized.pageCopy->position.y == 189);
    assert(initialized.pageCopy->coveredWidth == 68 && initialized.pageCopy->height == 7);
    assert(initialized.pixels.size() == 120 * 56 + 31 * 7);
    auto held = initialized.state;
    held.armor = 680;
    const auto grown = radarOperations(held, view, {0, 0, 0}, 0, {}, {0, 0}, panel, {228, 138});
    assert(grown.state.armorStrip == 42 && grown.pageCopy && grown.pageCopy->coveredWidth == 32);
    assert(grown.state.pages.back == held.pages.back && grown.state.pages.front == held.pages.front);
    assert(grown.state.pages.copy == held.pages.back && grown.state.pages.draw == held.pages.front);
    const auto unchanged = radarOperations(grown.state, view, {0, 0, 0}, 0, {}, {0, 0}, panel, {228, 138});
    assert(!unchanged.pageCopy && unchanged.drawColor == 0xe8);
    auto injured = grown.state;
    injured.armor = 488;
    const auto erased = radarOperations(injured, view, {0, 0, 0}, 0, {}, {0, 0}, panel, {228, 138});
    assert(erased.drawColor == 0 && erased.state.armorStrip == 30 && erased.pageCopy);
    assert(erased.pixels.back().color == 0);
    for (unsigned mutation = 0; mutation < 3; ++mutation) {
        auto invalid = state;
        if (mutation == 0) { invalid.armorStrip.reset(); }
        if (mutation == 1) { invalid.armorStrip = 43; }
        if (mutation == 2) { invalid.pages.draw = 0xa800; }
        rejected = false;
        try { radarOperations(invalid, view, {0, 0, 0}, 0, {actor}, {0, 0}, panel, {228, 138}); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    rejected = false;
    try { radarOperations(state, view, {0, 0, 0}, 1, {actor}, {0, 0}, panel, {228, 138}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    rejected = false;
    try { radarOperations(state, view, {0, 0, 0}, 0, {actor}, {0, 0}, panel, {95, 138}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    assert(radarOperations(outer.state, view, {0, 0, 0}, 0, {}, {0, 0}, panel, {228, 138}).enemyFlag == 1);
    actor.actor.flags = 0x88;
    for (unsigned mutation = 0; mutation < 7; ++mutation) {
        auto unsupported = actor;
        if (mutation == 0) { unsupported.renderProfile = WorldActorRenderProfile::unknown; }
        if (mutation == 1) { unsupported.actor.flags = 0x80; }
        if (mutation == 2) { unsupported.actor.position.z = 1001; }
        if (mutation == 3) { unsupported.radius = 32768; }
        if (mutation == 4) { unsupported.actor.position.z = -1; }
        if (mutation == 5) { unsupported.actor.position.z = 70000; }
        if (mutation == 6) { unsupported.type = 8; unsupported.actor.position.z = 45; }
        rejected = false;
        try { radarTraversalOperations(view, {unsupported}, {0, 0}); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    for (const auto type : {1, 4}) {
        RadarActorInput ordinary{{0, {}}, 1342, static_cast<std::int16_t>(type), -1, 0,
            WorldActorRenderProfile::noHandles, 0, 0, 0};
        const auto pixels = radarPresentationPixels(state, view, {}, 0, {ordinary}, {}, panel, {228, 138});
        assert(pixels.actors[0].projected && pixels.pixels.back().color == 228);
        assert(!pixels.cpuWorkQualified && pixels.operations.empty() && pixels.setup.operations.empty());
        rejected = false;
        try { radarOperations(state, view, {}, 0, {ordinary}, {}, panel, {228, 138}); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
        for (unsigned mutation = 0; mutation < 4; ++mutation) {
            auto invalid = ordinary;
            if (mutation == 0) { invalid.renderProfile = WorldActorRenderProfile::unknown; }
            if (mutation == 1) { invalid.callback = 0x130c; }
            if (mutation == 2) { invalid.category = -3; }
            if (mutation == 3) { invalid.actor.position.z = 1; }
            rejected = false;
            try { radarPresentationPixels(state, view, {}, 0, {invalid}, {}, panel, {228, 138}); }
            catch (const std::invalid_argument&) { rejected = true; }
            assert(rejected);
        }
    }
    for (const auto type : {17, 20}) {
        RadarActorInput hostile{{0, {}}, 441, static_cast<std::int16_t>(type), -3, 0,
            WorldActorRenderProfile::inlineSelectorNoCallback, 0, 0, 0};
        const auto pixels = radarPresentationPixels(state, view, {}, 0, {hostile}, {}, panel, {228, 138});
        assert(pixels.actors[0].projected && pixels.pixels.back().color == 238);
        assert(!pixels.cpuWorkQualified && pixels.operations.empty() && pixels.setup.operations.empty());
        rejected = false;
        try { radarOperations(state, view, {}, 0, {hostile}, {}, panel, {228, 138}); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    for (const auto type : {5, 22}) {
        RadarActorInput actor{{0, {0, 10000, 0}}, 134, static_cast<std::int16_t>(type),
            static_cast<std::int8_t>(type == 5 ? -1 : -3), 0,
            type == 5 ? WorldActorRenderProfile::noHandles : WorldActorRenderProfile::inlineSelectorNoCallback};
        const auto pixels = radarPresentationPixels(state, view, {}, 0, {actor}, {}, panel, {228, 138});
        assert(pixels.actors[0].projected && !pixels.cpuWorkQualified && pixels.operations.empty());
        rejected = false;
        try { radarOperations(state, view, {}, 0, {actor}, {}, panel, {228, 138}); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
        actor.actor.position.z = 1;
        rejected = false;
        try { radarPresentationPixels(state, view, {}, 0, {actor}, {}, panel, {228, 138}); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    RadarActorInput airborneShot{{0, {9899, 4007, 150}}, 134, 20, -3, 0,
        WorldActorRenderProfile::inlineSelectorNoCallback, 0, 0, 0};
    const auto airborne = radarPresentationPixels(state, view, {}, 0, {airborneShot}, {}, panel, {228, 138});
    assert(airborne.actors[0].projected && airborne.actors[0].center.y == 199850);
    assert(!airborne.cpuWorkQualified && airborne.operations.empty());
    rejected = false;
    try { radarOperations(state, view, {}, 0, {airborneShot}, {}, panel, {228, 138}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    for (const auto altitude : {-1, 1001}) {
        airborneShot.actor.position.z = altitude;
        rejected = false;
        try { radarPresentationPixels(state, view, {}, 0, {airborneShot}, {}, panel, {228, 138}); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    RadarActorInput projectile{{0, {}}, 441, 19, -3, 0,
        WorldActorRenderProfile::inlineSelectorNoCallback, 0, 0, 0};
    const auto shot = radarOperations(state, view, {}, 0, {projectile}, {}, panel, {228, 138});
    assert(shot.enemyFlag == 1 && shot.pixels.back().color == 238);
    assert(shot.actors[0].flags == 0 && shot.actors[0].projectedY == 0);
    auto playerShot = projectile;
    playerShot.type = 23;
    playerShot.radius = 134;
    const auto playerWork = radarOperations(state, view, {}, 0, {playerShot}, {}, panel, {228, 138});
    assert(playerWork.enemyFlag == 1 && playerWork.pixels.back().color == 238);
    assert(playerWork.actors[0].flags == 0 && playerWork.actors[0].projectedY == 0);
    for (const auto armor : {-1, -4, -16}) {
        auto lethal = state;
        lethal.armor = static_cast<std::int16_t>(armor);
        lethal.armorStrip = 0;
        const auto pixels = radarPresentationPixels(lethal, view, {}, 0, {}, {}, panel, {228, 138});
        assert(pixels.state.armorStrip == -1 && pixels.drawColor == 0);
        assert(pixels.pageCopy && pixels.pageCopy->position.x == 108);
        assert(pixels.pageCopy->coveredWidth == 12 && pixels.pageCopy->height == 7);
        assert(pixels.pixels.size() == panel.pixels.size() + 14);
        assert(pixels.pixels[panel.pixels.size()].point.x == 110);
        assert(!pixels.cpuWorkQualified && pixels.operations.empty());
        rejected = false;
        try { radarOperations(lethal, view, {}, 0, {}, {}, panel, {228, 138}); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    auto lethal = state;
    lethal.armor = -128;
    lethal.armorStrip = 0;
    const auto lethalPixels = radarPresentationPixels(lethal, view, {}, 0, {}, {}, panel, {228, 138});
    assert(lethalPixels.state.armorStrip == -8 && lethalPixels.pageCopy);
    assert(lethalPixels.pixels[panel.pixels.size()].point.x == 96);
    assert(lethalPixels.pageCopy->position.x == 96 && lethalPixels.pageCopy->coveredWidth == 24);
    lethal.armor = -129;
    rejected = false;
    try { radarPresentationPixels(lethal, view, {}, 0, {}, {}, panel, {228, 138}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    for (unsigned mutation = 0; mutation < 4; ++mutation) {
        auto invalid = playerShot;
        if (mutation == 0) { invalid.renderProfile = WorldActorRenderProfile::noHandles; }
        if (mutation == 1) { invalid.callback = 0x130c; }
        if (mutation == 2) { invalid.category = -2; }
        if (mutation == 3) { invalid.type = 22; }
        rejected = false;
        try { radarOperations(state, view, {}, 0, {invalid}, {}, panel, {228, 138}); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }

    RadarActorInput craft{{0, {0, 0, 5000}}, 13052, 7, -1, 0,
        WorldActorRenderProfile::inlineSelectorNoCallback, 17, 18, 19};
    const auto craftPixels = radarPresentationPixels(state, view, {}, 0, {craft}, {}, panel, {228, 138});
    assert(craftPixels.actors[0].projected && craftPixels.pixels.back().color == 228);
    assert(craftPixels.actors[0].flags == 0 && craftPixels.actors[0].projectedY == 0);
    assert(craftPixels.actors[0].cachedCenter.y == 195000);
    assert(!craftPixels.cpuWorkQualified && craftPixels.operations.empty() && craftPixels.setup.operations.empty());
    rejected = false;
    try { radarOperations(state, view, {}, 0, {craft}, {}, panel, {228, 138}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    for (unsigned mutation = 0; mutation < 8; ++mutation) {
        auto invalid = craft;
        if (mutation == 0) { invalid.renderProfile = WorldActorRenderProfile::unknown; }
        if (mutation == 1) { invalid.renderProfile = WorldActorRenderProfile::noHandles; }
        if (mutation == 2) { invalid.callback = 0xb0b4; }
        if (mutation == 3) { invalid.actor.flags = 8; }
        if (mutation == 4) { invalid.category = -3; }
        if (mutation == 5) { invalid.actor.position.z = 5001; }
        if (mutation == 6) { invalid.actor.position.z = 4999; }
        if (mutation == 7) { invalid.radius = 32768; }
        rejected = false;
        try { radarPresentationPixels(state, view, {}, 0, {invalid}, {}, panel, {228, 138}); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    craft.renderProfile = WorldActorRenderProfile::inlineSelectorBeamCallback;
    craft.callback = 0xb0b4;
    craft.actor.flags = 12;
    const auto beamRadar = radarPresentationPixels(state, view, {}, 0, {craft}, {}, panel, {228, 138});
    assert(beamRadar.actors[0].projected && beamRadar.actors[0].flags == 12);
    assert(!beamRadar.cpuWorkQualified && beamRadar.operations.empty());
    rejected = false;
    try { radarOperations(state, view, {}, 0, {craft}, {}, panel, {228, 138}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    craft.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
    craft.callback = 0;
    craft.actor.flags = 4;
    const auto postBeamRadar = radarPresentationPixels(state, view, {}, 0, {craft}, {}, panel, {228, 138});
    assert(postBeamRadar.actors[0].flags == 4 && postBeamRadar.actors[0].projectedY == 0);
    craft.renderProfile = WorldActorRenderProfile::inlineSelectorDeparture;
    craft.actor.position.z = 5200;
    const auto departureRadar = radarPresentationPixels(state, view, {}, 0, {craft}, {}, panel, {228, 138});
    assert(departureRadar.actors[0].cachedCenter.y == 194800);
    assert(departureRadar.actors[0].flags == 4 && departureRadar.actors[0].projectedY == 0);
    assert(!departureRadar.cpuWorkQualified && departureRadar.operations.empty());
    for (unsigned mutation = 0; mutation < 6; ++mutation) {
        auto invalid = craft;
        if (mutation == 0) { invalid.actor.position.z = -1; }
        if (mutation == 1) { invalid.actor.position.z = 1000001; }
        if (mutation == 2) { invalid.actor.position.z = 60000; }
        if (mutation == 3) { invalid.callback = 0xb0b4; }
        if (mutation == 4) { invalid.actor.flags = 8; }
        if (mutation == 5) { invalid.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback; }
        rejected = false;
        try { radarPresentationPixels(state, view, {}, 0, {invalid}, {}, panel, {228, 138}); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    rejected = false;
    try { radarOperations(state, view, {}, 0, {craft}, {}, panel, {228, 138}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    RadarActorInput guardian{{0, {0, 10000, 0}}, 1147, 44, -1, 0x130c,
        WorldActorRenderProfile::heapSelectorDeferredAnimation, 17, 18, 19};
    const auto guardianRadar = radarPresentationPixels(state, view, {}, 0, {guardian}, {}, panel, {228, 138});
    assert(guardianRadar.actors[0].projected && guardianRadar.pixels.back().color == 228);
    assert(!guardianRadar.cpuWorkQualified && guardianRadar.operations.empty());
    for (unsigned mutation = 0; mutation < 7; ++mutation) {
        auto invalid = guardian;
        if (mutation == 0) { invalid.renderProfile = WorldActorRenderProfile::unknown; }
        if (mutation == 1) { invalid.renderProfile = WorldActorRenderProfile::noHandles; }
        if (mutation == 2) { invalid.type = 47; }
        if (mutation == 3) { invalid.radius = 0; }
        if (mutation == 4) { invalid.callback = 0; }
        if (mutation == 5) { invalid.actor.position.z = 1; }
        if (mutation == 6) { invalid.category = -2; }
        rejected = false;
        try { radarPresentationPixels(state, view, {}, 0, {invalid}, {}, panel, {228, 138}); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    rejected = false;
    try { radarOperations(state, view, {}, 0, {guardian}, {}, panel, {228, 138}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    guardian.type = 45;
    const auto openGuardianRadar = radarPresentationPixels(state, view, {}, 0, {guardian}, {}, panel, {228, 138});
    assert(openGuardianRadar.actors[0].flags == 0 && openGuardianRadar.actors[0].projectedY == 0);
    assert(!openGuardianRadar.cpuWorkQualified && openGuardianRadar.operations.empty());
    RadarActorInput emitted{{0, {0, 10000, 0}}, 2260, 46, -2, 0,
        WorldActorRenderProfile::noHandles, 17, 18, 19};
    const auto fixedRadar = radarPresentationPixels(state, view, {}, 0, {emitted}, {}, panel, {228, 138});
    assert(fixedRadar.actors[0].projected && fixedRadar.actors[0].projectedY == 0);
    assert(!fixedRadar.cpuWorkQualified && fixedRadar.operations.empty());
    for (unsigned mutation = 0; mutation < 6; ++mutation) {
        auto invalid = emitted;
        if (mutation == 0) { invalid.callback = 0x130c; }
        if (mutation == 1) { invalid.radius = 1147; }
        if (mutation == 2) { invalid.actor.position.z = 1; }
        if (mutation == 3) { invalid.type = 47; }
        if (mutation == 4) { invalid.renderProfile = WorldActorRenderProfile::unknown; }
        if (mutation == 5) { invalid.category = -1; }
        rejected = false;
        try { radarPresentationPixels(state, view, {}, 0, {invalid}, {}, panel, {228, 138}); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    rejected = false;
    try { radarOperations(state, view, {}, 0, {emitted}, {}, panel, {228, 138}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    for (const auto type : {47, 48}) {
        for (const auto altitude : {0, 250, 500}) {
            RadarActorInput inlineActor{{0, {0, 10000, altitude}}, 559, static_cast<std::int16_t>(type), -1, 0,
                WorldActorRenderProfile::inlineSelectorNoCallback, 17, 18, 19};
            const auto inlineRadar = radarPresentationPixels(state, view, {}, 0, {inlineActor}, {}, panel, {228, 138});
            assert(inlineRadar.actors[0].flags == 0 && inlineRadar.actors[0].projectedY == 0);
            assert(!inlineRadar.cpuWorkQualified && inlineRadar.operations.empty());
            rejected = false;
            try { radarOperations(state, view, {}, 0, {inlineActor}, {}, panel, {228, 138}); }
            catch (const std::invalid_argument&) { rejected = true; }
            assert(rejected);
            for (unsigned mutation = 0; mutation < 8; ++mutation) {
                auto invalid = inlineActor;
                if (mutation == 0) { invalid.actor.position.z = 501; }
                if (mutation == 1) { invalid.actor.position.z = -1; }
                if (mutation == 2) { invalid.actor.position.z = 499; }
                if (mutation == 3) { invalid.radius = 291; }
                if (mutation == 4) { invalid.callback = 0x130c; }
                if (mutation == 5) { invalid.category = -3; }
                if (mutation == 6) { invalid.renderProfile = WorldActorRenderProfile::noHandles; }
                if (mutation == 7) { invalid.type = 49; }
                rejected = false;
                try { radarPresentationPixels(state, view, {}, 0, {invalid}, {}, panel, {228, 138}); }
                catch (const std::invalid_argument&) { rejected = true; }
                assert(rejected);
            }
        }
    }
    RadarActorInput pickup{{0, {0, 10000, 0}}, 897, 25, -2, 0,
        WorldActorRenderProfile::inlineSelectorNoCallback, 17, 18, 19};
    const auto pickupRadar = radarPresentationPixels(state, view, {}, 0, {pickup}, {}, panel, {228, 138});
    assert(pickupRadar.actors[0].projected && pickupRadar.pixels.back().color == 234);
    assert(pickupRadar.actors[0].flags == 0 && pickupRadar.actors[0].projectedY == 0);
    assert(!pickupRadar.cpuWorkQualified && pickupRadar.operations.empty());
    for (unsigned mutation = 0; mutation < 6; ++mutation) {
        auto invalid = pickup;
        if (mutation == 0) { invalid.type = 26; }
        if (mutation == 1) { invalid.category = -1; }
        if (mutation == 2) { invalid.radius = 0; }
        if (mutation == 3) { invalid.callback = 0x130c; }
        if (mutation == 4) { invalid.actor.position.z = 1; }
        if (mutation == 5) { invalid.renderProfile = WorldActorRenderProfile::unknown; }
        rejected = false;
        try { radarPresentationPixels(state, view, {}, 0, {invalid}, {}, panel, {228, 138}); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
    }
    rejected = false;
    try { radarOperations(state, view, {}, 0, {pickup}, {}, panel, {228, 138}); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    for (const auto altitude : {150, 500, 1000}) {
        RadarActorInput animated{{0, {0, 10000, altitude}}, 748, 11, -1, 0x130c,
            WorldActorRenderProfile::heapSelectorDeferredAnimation, 17, 18, 19};
        const auto pixels = radarPresentationPixels(state, view, {}, 0, {animated}, {}, panel, {228, 138});
        assert(!pixels.cpuWorkQualified && pixels.operations.empty());
        assert(pixels.actors[0].flags == 0 && pixels.actors[0].projectedY == 0);
        rejected = false;
        try { radarOperations(state, view, {}, 0, {animated}, {}, panel, {228, 138}); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
        for (unsigned mutation = 0; mutation < 5; ++mutation) {
            auto invalid = animated;
            if (mutation == 0) { invalid.actor.position.z = 149; }
            if (mutation == 1) { invalid.actor.position.z = 1001; }
            if (mutation == 2) { invalid.radius = 1147; }
            if (mutation == 3) { invalid.callback = 0; }
            if (mutation == 4) { invalid.renderProfile = WorldActorRenderProfile::unknown; }
            rejected = false;
            try { radarPresentationPixels(state, view, {}, 0, {invalid}, {}, panel, {228, 138}); }
            catch (const std::invalid_argument&) { rejected = true; }
            assert(rejected);
        }
    }

    const std::array<RadarActorInput, 4> level3Actors{{
        {{0, {0, 10000, 0}}, 701, 10, -1, 0, WorldActorRenderProfile::noHandles, 17, 18, 19},
        {{0, {0, 10000, 0}}, 829, 2, -1, 0, WorldActorRenderProfile::noHandles, 17, 18, 19},
        {{0, {0, 10000, 0}}, 997, 24, -4, 0x130c,
            WorldActorRenderProfile::heapSelectorDeferredAnimation, 17, 18, 19},
        {{0, {0, 10000, 0}}, 1103, 31, -2, 0,
            WorldActorRenderProfile::inlineSelectorNoCallback, 17, 18, 19}
    }};
    for (const auto& level3Actor : level3Actors) {
        const auto projected = radarPresentationPixels(state, view, {}, 0,
            {level3Actor}, {}, panel, {228, 138});
        assert(projected.actors.size() == 1 && projected.actors[0].projected);
        assert(projected.actors[0].flags == 0 && projected.actors[0].projectedY == 0);
        assert(projected.actors[0].cachedCenter.y == 200000);
        assert(projected.actors[0].projectedX != level3Actor.projectedX);
        const auto markerColor = level3Actor.type == 24 ? 78 : level3Actor.type == 31 ? 138 : 228;
        assert(projected.pixels.back().color == markerColor);
        assert(!projected.cpuWorkQualified && projected.operations.empty() && projected.setup.operations.empty());
        rejected = false;
        try { radarOperations(state, view, {}, 0, {level3Actor}, {}, panel, {228, 138}); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
        for (unsigned mutation = 0; mutation < 7; ++mutation) {
            auto invalid = level3Actor;
            if (mutation == 0) { invalid.actor.flags = 4; }
            if (mutation == 1) { invalid.category = -3; }
            if (mutation == 2) { invalid.callback = invalid.callback == 0 ? 0x130c : 0; }
            if (mutation == 3) { invalid.renderProfile = WorldActorRenderProfile::unknown; }
            if (mutation == 4) { invalid.actor.position.z = 1; }
            if (mutation == 5) { invalid.radius = 0; }
            if (mutation == 6) { invalid.radius = 32768; }
            rejected = false;
            try { radarPresentationPixels(state, view, {}, 0, {invalid}, {}, panel, {228, 138}); }
            catch (const std::invalid_argument&) { rejected = true; }
            assert(rejected);
        }
    }

    auto playableActor=level3Actors.front();
    playableActor.type=6;
    rejected=false;
    try {radarPresentationPixels(state,view,{},0,{playableActor},{},panel,{228,138});}
    catch (const std::invalid_argument&) {rejected=true;}
    assert(rejected);
    const auto playable=radarPresentationPixels(state,view,{},0,{playableActor},{},panel,{228,138},
        RadarPageLayout::compatibility,RadarPixelDomain::playableCampaign);
    const auto retained=radarPresentationPixels(state,view,{},0,{level3Actors.front()},{},panel,{228,138});
    assert(playable.actors.front().projectedX==retained.actors.front().projectedX);
    assert(playable.actors.front().projectedRadius==retained.actors.front().projectedRadius);
    assert(playable.operations.empty() && playable.setup.operations.empty() && !playable.cpuWorkQualified);
    for (unsigned mutation=0;mutation<5;++mutation) {
        auto invalid=playableActor;
        if (mutation==0) {invalid.renderProfile=WorldActorRenderProfile::unknown;}
        if (mutation==1) {invalid.radius=0;}
        if (mutation==2) {invalid.callback=0x130c;}
        if (mutation==3) {invalid.category=0;}
        if (mutation==4) {invalid.type=49;}
        rejected=false;
        try {radarPresentationPixels(state,view,{},0,{invalid},{},panel,{228,138},
            RadarPageLayout::compatibility,RadarPixelDomain::playableCampaign);}
        catch (const std::invalid_argument&) {rejected=true;}
        assert(rejected);
    }
}
