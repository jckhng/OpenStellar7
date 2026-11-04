#include "stellar7/controlled_session.hpp"
#include "gameplay_analysis.hpp"
#include "stellar7/indexed_render.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::size_t presentationPrerollCallbacks = 268;
constexpr std::uint32_t bootstrapTicks = 4369;
constexpr std::int16_t heldArmor = 680;
constexpr std::uint16_t returnedRenderLoopTick = 4;
constexpr std::size_t levelIntroductionOption = 5;
constexpr std::uint16_t restoredKeyboardHead = 0x36;
constexpr std::uint8_t solidRasterStyle = 1;
constexpr std::size_t keyCount = 5;

bool released(const GameplayInput& input) {
    return !input.left && !input.right && !input.forward && !input.reverse &&
        !input.fire && !input.exit && !input.confirm;
}
std::size_t keyIndex(const ControlledInputKey key) {
    const auto index = static_cast<std::size_t>(key);
    if (index >= keyCount) {
        throw std::invalid_argument("controlled bootstrap has unsupported movie key");
    }
    return index;
}
void validateRequest(const ControlledBootstrapRequest& request) {
    if (request.arrivalInput != ControlledArrivalInput::released
        && request.arrivalInput != ControlledArrivalInput::residentForward
        && request.arrivalInput != ControlledArrivalInput::physicalForwardPair
        && request.arrivalInput != ControlledArrivalInput::hostForward) {
        throw std::invalid_argument("controlled bootstrap has unsupported resident input");
    }
    if (request.delivery != ControlledInputDelivery::beforeTick) {
        throw std::invalid_argument("controlled bootstrap requires before-tick movie");
    }
    if (request.events.empty()) {
        throw std::invalid_argument("controlled bootstrap requires movie events");
    }
    std::array<bool, keyCount> held{};
    std::uint32_t previous = 0;
    for (const auto& event : request.events) {
        if (event.tick == 0 || event.tick < previous || event.keys.empty()) {
            throw std::invalid_argument("controlled bootstrap requires ordered nonempty movie events");
        }
        previous = event.tick;
        for (const auto key : event.keys) {
            auto& state = held[keyIndex(key)];
            if (state == event.pressed) {
                throw std::invalid_argument("controlled bootstrap movie key state is inconsistent");
            }
            state = event.pressed;
        }
        if ((held[0] && held[1]) || (held[2] && held[3])) {
            throw std::invalid_argument("controlled bootstrap movie holds opposing directions");
        }
    }
    if (std::any_of(held.begin(), held.end(), [](bool value) { return value; })) {
        throw std::invalid_argument("controlled bootstrap movie must release all keys");
    }
    if (request.copyProfile != FrameCopyProfile::modeXNoop) {
        throw std::invalid_argument("controlled bootstrap requires Mode-X no-op copy profile");
    }
    if (request.horizonCycles <= 0) {
        throw std::invalid_argument("controlled bootstrap requires positive work horizon");
    }
    (void)horizonOperations(request.horizon);
    const auto& phase = request.returnedRaw4Phase;
    if ((request.arrivalInput == ControlledArrivalInput::physicalForwardPair
            || request.arrivalInput == ControlledArrivalInput::hostForward) != phase.keyboard.has_value()
        || phase.instructionKeyboardEdge) {
        throw std::invalid_argument("physical input recipe requires its own initial keyboard profile");
    }
    const auto admittedStart = [](std::uint32_t start) { return start == 0 || start == 0x4000; };
    if (!phase.bios || !phase.devices || !phase.devices->frame || phase.linesTotal != QuietScanout::height ||
        phase.frameStart != AudioClockFrameStart::activeUnlockedVgaSurface ||
        phase.vgaInterrupt != AudioClockVgaInterrupt::fixedVgaNoDemand ||
        phase.devices->memoryMask != 0x1fffff || phase.devices->configuredBytesSkip != 0 ||
        phase.devices->drawnBytesSkip != 0 || phase.devices->pelPanning != 0 ||
        phase.devices->drawnPanning != 0 || !admittedStart(phase.devices->displayStart) ||
        !admittedStart(phase.devices->realStart)) {
        throw std::invalid_argument("controlled bootstrap has unsupported returned-phase addressing");
    }
}
void validateAssets(const GameplayAssets& assets) {
    if (assets.cockpitDacPalette.size() != vgaPaletteBytes ||
        std::any_of(assets.cockpitDacPalette.begin(), assets.cockpitDacPalette.end(),
            [](std::uint8_t component) { return component > vgaComponentMaximum; })) {
        throw std::invalid_argument("controlled bootstrap requires complete six-bit source palette");
    }
    if (assets.artGroups.size() <= 4) {
        throw std::invalid_argument("controlled bootstrap requires original gameplay bitmap groups");
    }
}
stellar7::RadarPages radarPageRoles(const stellar7::CockpitPageState& pages) {
    return {pages.cachePage(), pages.backPage(), pages.frontPage(), pages.drawPage(), pages.copyPage()};
}

stellar7::CockpitWorkBitmaps cockpitWorkBitmaps(const stellar7::GameplayAssets& assets) {
    constexpr std::size_t panelGroup = 3;
    constexpr std::size_t stripIndex = 1;
    constexpr std::size_t backgroundIndex = 2;
    constexpr std::size_t overlayGroup = 4;
    constexpr std::size_t indicatorIndex = 6;
    if (assets.artGroups.size() <= overlayGroup
        || assets.artGroups[panelGroup].size() <= stripIndex
        || assets.artGroups[overlayGroup].size() <= indicatorIndex) {
        throw std::invalid_argument("owned cockpit requires original bitmaps");
    }
    stellar7::CockpitWorkBitmaps bitmaps{};
    bitmaps.strip = assets.artGroups[panelGroup][stripIndex];
    bitmaps.indicator = assets.artGroups[overlayGroup][indicatorIndex];
    if (assets.artGroups[panelGroup].size() > backgroundIndex) {
        bitmaps.matchingPageBackground = assets.artGroups[panelGroup][backgroundIndex];
    }
    for (std::size_t index = 0; index < bitmaps.overlays.size(); ++index) {
        bitmaps.overlays[index] = assets.artGroups[overlayGroup][index];
    }
    return bitmaps;
}

std::vector<stellar7::RadarStateActor> radarActors(const std::vector<stellar7::Level1Entity>& entities,
    const std::vector<std::size_t>& order) {
    if (entities.size() != 7 || order.size() != 7) {
        throw std::invalid_argument("owned radar requires the retained seven-allocation Level2 path");
    }
    std::vector<stellar7::RadarStateActor> actors;
    for (const auto index : order) {
        const auto& actor = entities.at(index);
        actors.push_back({actor.allocationIndex, {actor.flags, actor.position}, actor.collisionRadius,
            actor.type, static_cast<std::int8_t>(actor.stateBytes4dTo52[0]), 0,
            actor.renderProfile});
    }
    return actors;
}

void applyRadarPages(const stellar7::RadarWork& work, stellar7::CockpitPageState& pages,
    stellar7::IndexedPages& pixels) {
    pixels.apply(work);
    if (work.pageCopy) {
        pages.selectArmorCopy();
    }
}


void renderReturnedRaw4(stellar7::Level2RenderState state, const stellar7::GameplayModel& model,
    const stellar7::GameplayAssets& assets, const stellar7::HorizonWorkState& horizon, const stellar7::CockpitSettings& settings,
    stellar7::CockpitPageState& pages, stellar7::IndexedPages& pixels) {
    const std::uint16_t priorLoopTick = returnedRenderLoopTick;
    constexpr std::size_t groundGroup = 4;
    constexpr std::size_t firstGroundBitmap = 15;
    constexpr std::uint8_t sightColor = 95;
    constexpr stellar7::HorizonColors level2Colors{64, 78};
    if (model.phase() != stellar7::GameplayPhase::level2 || model.level2LoopTick() != priorLoopTick
        || state.player.yaw != horizon.cameraYaw
        || state.player.pitch != 0 || state.player.roll != 0
        || !model.level2EntryPaletteActive() || model.damageFlashActive()
        || state.entities.size() != 7 || state.order.size() != 7) {
        throw std::invalid_argument("unsupported owned incoming-front producer");
    }
    std::array<stellar7::IndexedBitmap, 8> groundBitmaps;
    for (std::size_t index = 0; index < groundBitmaps.size(); ++index) {
        groundBitmaps[index] = assets.artGroups.at(groundGroup).at(firstGroundBitmap + index);
    }
    const auto ground = stellar7::groundClippedExecution({state.ground.points(), state.player.yawVelocity,
        state.player.forwardVelocity, state.player.position.z}, state.rng, groundBitmaps, {24, 295, 24, 145});
    const auto setup = stellar7::worldViewSetupOperations({state.player.position, {0, 0, 0},
        {state.player.pitch, state.player.roll, state.player.yaw}, 0x88, 7, {24, 294, 24, 145}, {256, -256, 362}});
    std::vector<stellar7::WorldSceneActor> actors;
    stellar7::SightRefreshInput refresh{{}, state.player.yaw};
    for (const auto index : state.order) {
        const auto& actor = state.entities.at(index);
        if (actor.renderProfile != stellar7::WorldActorRenderProfile::noHandles) {
            throw std::invalid_argument("incoming-front producer requires owned no-handle actors");
        }
        actors.push_back({{actor.flags, actor.position}, {actor.pitch, actor.roll, actor.yaw},
            actor.type, actor.collisionRadius, 0, 0});
        refresh.targets.push_back({actor.type, actor.targetHeading});
    }
    const auto world = stellar7::worldSceneOperations(assets, setup.view, actors,
        {assets.sharedModelRegistry[0], assets.sharedModelRegistry[1], 0, 0});
    const auto sight = stellar7::sightOperations(model.level2LoopTick(), state.alternateSightPhase,
        setup.view.viewport, sightColor, refresh);
    auto returnedPages = pages;
    returnedPages.beginFrame();
    returnedPages.selectCockpitCopy();
    const auto relation = returnedPages.cachedPageDiffersFromDraw()
        ? stellar7::CockpitPageRelation::differing : stellar7::CockpitPageRelation::matching;
    const auto bitmaps = cockpitWorkBitmaps(assets);
    const auto cockpit = stellar7::cockpitOperations({state.cockpitIndicatorX,
        state.cockpitIndicatorVelocity, settings.viewSize, relation}, bitmaps);
    const auto drawPage = returnedPages.drawPage();
    auto effects = stellar7::horizonPageEffects(horizon,
        assets.horizonFramesForLevel(1), level2Colors, drawPage);
    const auto appendPixels = [&](const auto& writes) {
        for (const auto& pixel : writes) {
            effects.emplace_back(stellar7::RadarPixelWrite{pixel.point, pixel.color, drawPage});
        }
    };
    appendPixels(ground.pixels);
    appendPixels(world.pixels);
    appendPixels(sight.pixels);
    const auto cockpitEffects = stellar7::cockpitPageEffects(cockpit, bitmaps,
        returnedPages.cachePage(), drawPage);
    effects.insert(effects.end(), cockpitEffects.begin(), cockpitEffects.end());
    auto returnedPixels = pixels;
    returnedPixels.apply(effects);
    // Completed pixel effects only; the earlier CPU/device/audio phase is unqualified.
    state.ground = ground.ground;
    state.rng = ground.rng;
    state.alternateSightPhase = sight.alternatePhase;
    state.sightExecution = stellar7::Level2SightExecution::rendered;
    state.cockpitIndicatorX = cockpit.state.indicatorX;
    state.cockpitIndicatorVelocity = cockpit.state.indicatorVelocity;
    state.cockpitExecution = stellar7::Level2CockpitExecution::rendered;
    pages = returnedPages;
    pixels = std::move(returnedPixels);
}

} // namespace

struct ControlledLevel2Session::Impl final : RetainedLevel2Observer {
    GameplayAssets assets;
    ControlledBootstrapRequest request;
    GameplayModel model;
    SoundBlasterSynth synth;
    PresentationClock clock;
    std::optional<FrameController> controller;
    CockpitPageState pages;
    RadarState radar;
    IndexedPages pixels;
    PaletteWorkState palette;
    std::optional<RetainedFrameExecutor> executor;
    QuietScanout scanout;
    ControlledSessionObserver* observer;
    ControlledKeyboardSource* keyboard;
    std::array<bool, keyCount> held{};
    std::size_t nextEvent = 0, level1Audio = 0, level2Audio = 0, failureAudio = 0, emptyTailReads = 0;
    std::vector<std::size_t> radarAllocations;
    std::uint8_t rasterStyle = solidRasterStyle;
    std::optional<std::uint8_t> enemyFlag;
    std::vector<std::int16_t> samples;
    std::vector<ControlledScan> scans;
    std::uint64_t firstSample = 0;
    std::uint64_t nextSample = 0;
    ControlledObservationScope scope = ControlledObservationScope::bootstrap;
    enum class Status { ready, advancing, rejected };
    Status status = Status::ready;
    bool arrivalExecuted = false;
    bool postCheckpointPending = false;
    bool physicalReleasePending = false;
    std::size_t priorRaw4Audio = 0;
    std::optional<RetainedLevel2Renderer> renderer;

    Impl(GameplayAssets initialAssets, ControlledBootstrapRequest initialRequest,
        ControlledSessionObserver* initialObserver, ControlledKeyboardSource* initialKeyboard)
        : assets(std::move(initialAssets)), request(std::move(initialRequest)),
          synth(assets.soundSequences, assets.soundBlasterInstrumentResource, assets.musicForLevel(0),
              assets.presentationMusicSequence, assets.musicForLevel(1), assets.musicForLevel(2), assets.logoMusicSequence),
          clock(synth), radar(assets.artGroups.at(3)), observer(initialObserver), keyboard(initialKeyboard) {
        std::copy(assets.cockpitDacPalette.begin(), assets.cockpitDacPalette.end(), palette.source.begin());
        // Model construction includes the first ordinary Level1 display.
        pages.flip();
        bootstrap();
    }
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    ControlledSessionView view(const CockpitPageState& selectedPages, const IndexedPages& selectedPixels) const {
        return {model, synth, selectedPages, radar, selectedPixels, palette,
            executor ? &executor->phase() : nullptr,
            {nextEvent, level1Audio, level2Audio, failureAudio, emptyTailReads}, clock.elapsedSamples(), scope,
            rasterStyle, enemyFlag, controller ? std::optional<std::uint16_t>{controller->expectedLoopTick()} : std::nullopt,
            controller && (request.arrivalInput == ControlledArrivalInput::physicalForwardPair
                || request.arrivalInput == ControlledArrivalInput::hostForward)
                ? std::optional<FrameControllerKeyboardState>{controller->keyboardState()} : std::nullopt};
    }
    ControlledSessionView view() const { return view(pages, pixels); }
    GameplayInput input() const { return {held[0], held[1], held[2], held[3], held[4], false, false}; }
    bool applyInput(std::uint32_t tick, ControlledInputDelivery delivery) {
        if (request.delivery != delivery) { return false; }
        const auto before = nextEvent;
        while (nextEvent < request.events.size() && request.events[nextEvent].tick == tick) {
            const auto& event = request.events[nextEvent];
            for (const auto key : event.keys) { held[keyIndex(key)] = event.pressed; }
            ++nextEvent;
        }
        if (nextEvent < request.events.size() && request.events[nextEvent].tick < tick) {
            throw std::invalid_argument("controlled bootstrap skipped a movie boundary");
        }
        return nextEvent != before;
    }
    void collect(const ClockedPresentationWork& work, const CockpitPageState&,
        const IndexedPages& selectedPixels, std::optional<std::uint16_t> writingPage = std::nullopt) {
        if (work.audio.firstSample != nextSample ||
            work.audio.vgaSamplePositions.size() != work.timing.vgaServices.size()) {
            throw std::logic_error("controlled delivery has noncontiguous work metadata");
        }
        samples.insert(samples.end(), work.audio.samples.begin(), work.audio.samples.end());
        nextSample += work.audio.samples.size();
        scanout.consume(work.timing.vgaServices, selectedPixels, palette, writingPage);
        std::size_t receiptIndex = 0;
        for (auto& completed : scanout.takeCompleted()) {
            while (receiptIndex < work.timing.vgaServices.size()) {
                const auto& receipt = work.timing.vgaServices[receiptIndex];
                if (receipt.kind == VgaServiceKind::scanLine && receipt.line == QuietScanout::height - 1 &&
                    receipt.frameStartMilliseconds == completed.originMilliseconds &&
                    receipt.scheduledMilliseconds == completed.scheduledCompletionMilliseconds &&
                    receipt.servicedMilliseconds == completed.completionMilliseconds) {
                    break;
                }
                ++receiptIndex;
            }
            if (receiptIndex == work.timing.vgaServices.size()) {
                throw std::logic_error("controlled scan has no completing VGA receipt");
            }
            scans.push_back({std::move(completed), work.audio.vgaSamplePositions[receiptIndex++]});
        }
    }
    void projectPalette() {
        if (model.phase() != GameplayPhase::level2 || !model.level2EntryPaletteActive() || model.damageFlashActive()) {
            throw std::invalid_argument("controlled bootstrap requires retained entry-palette history");
        }
        PaletteWorkState initial{};
        initial.source = palette.source;
        for (unsigned step = 0; step < levelEntryFadeSteps; ++step) {
            initial = completedFullPaletteUpload(initial, {0, vgaPaletteColors, 0,
                static_cast<std::uint8_t>(step * levelEntryFadeIntensityStep)});
        }
        palette = initial;
    }
    void initializePages() {
        const auto& initialization = model.lastCockpitInitialization();
        if (initialization.levelIndex != 1 || initialization.states.back().x != model.level2CockpitIndicatorX() ||
            initialization.states.back().velocity != model.level2CockpitIndicatorVelocity()) {
            throw std::invalid_argument("controlled bootstrap cockpit initialization mismatch");
        }
        const auto bitmaps = cockpitWorkBitmaps(assets);
        std::array<CockpitWork, 3> draws;
        for (std::size_t i = 0; i < draws.size(); ++i) {
            const auto& entry = initialization.states[i];
            draws[i] = cockpitOperations({entry.x, entry.velocity, request.settings.viewSize,
                i == 0 ? CockpitPageRelation::matching : CockpitPageRelation::differing}, bitmaps);
            const auto& returned = initialization.states[i + 1];
            if (draws[i].state.indicatorX != returned.x || draws[i].state.indicatorVelocity != returned.velocity) {
                throw std::invalid_argument("controlled bootstrap cockpit effects mismatch");
            }
        }
        constexpr std::uint8_t titleColor = 239;
        pixels.apply(cockpitResourcePageEffects(assets.cockpit, pages.cachePage()));
        const auto& font = request.settings.options[levelIntroductionOption] != 0 ? assets.titleFont : assets.scoreFont;
        pixels.apply(cockpitCachePageEffects(draws[0], draws[1], bitmaps, font, "ANTARES", titleColor,
            pages.cachePage(), pages.backPage()));
        radar.resetArmorStrip();
        for (const auto& actor : model.level2Entities()) {
            radar.initializeAllocation(actor.allocationIndex);
            radarAllocations.push_back(actor.allocationIndex);
        }
        pages.beginFrame();
        pages.selectCockpitCopy();
        const auto& player = model.level2Player();
        const auto startup = radar.initialize(model.playerArmor(), player.position, player.yaw, radarPageRoles(pages));
        applyRadarPages(startup, pages, pixels);
        pixels.apply(std::vector<IndexedPageEffect>{RadarPageCopy{pages.backPage(), pages.frontPage(), {0, 0}, 320, 200}});
        pixels.apply(scorePageEffects(model.score(), assets.scoreFont, pages.backPage(), pages.frontPage()));
        const auto& systems = model.level1State();
        pixels.apply(statusPageEffects(systems.systemStatusLevels(), systems.systemTimers(), systems.selectedSystem(),
            assets.artGroups.at(4), pages.backPage(), pages.frontPage()));
        pixels.apply(cockpitPageEffects(draws[2], bitmaps, pages.cachePage(), pages.backPage()));
    }
    RadarWork drawRadar() {
        pages.beginFrame();
        pages.selectCockpitCopy();
        const auto& player = model.level2Player();
        auto work = radar.draw(model.playerArmor(), player.position, player.yaw,
            radarActors(model.level2PresentationEntities(), model.level2PresentationActiveListOrder()),
            radarPageRoles(pages), {0, 0});
        applyRadarPages(work, pages, pixels);
        return work;
    }
    FrameTailState finishRaw4(const FrameTailState& state, const FrameTailGuards& guards) {
        if (executor || !controller || !released(input()) || state.loopTick != returnedRenderLoopTick ||
            model.level2AudioEvents().size() != priorRaw4Audio || priorRaw4Audio != level2Audio ||
            model.level1State().audioEvents().size() != level1Audio ||
            model.failureAudioEvents().size() != failureAudio || model.phase() != GameplayPhase::level2 ||
            static_cast<std::uint16_t>(controller->expectedLoopTick() + 1) != state.loopTick) {
            throw std::invalid_argument("controlled bootstrap has unsupported raw4 returned arrival");
        }
        clock.renderFrame();
        projectPalette();
        const auto radarWork = drawRadar();
        if (radarWork.pageCopy) {
            throw std::invalid_argument("controlled bootstrap arrival excludes front armor copy");
        }
        controller->consumeEmptyTail(controller->expectedLoopTick());
        ++emptyTailReads;
        if (request.arrivalInput == ControlledArrivalInput::residentForward) {
            controller->restoreForwardAtRenderReturn();
        }
        if (request.arrivalInput == ControlledArrivalInput::physicalForwardPair
            || request.arrivalInput == ControlledArrivalInput::hostForward) {
            executor.emplace(clock, std::move(request.returnedRaw4Phase), request.horizonCycles, *controller);
            if (request.arrivalInput == ControlledArrivalInput::physicalForwardPair) {
                executor->queueForward(KeyboardEdge::press);
                physicalReleasePending = true;
            } else {
                pollKeyboard();
            }
        } else {
            executor.emplace(clock, std::move(request.returnedRaw4Phase), request.horizonCycles);
        }
        if (observer) { observer->arrival(view()); }
        const auto flipped = executor->display(pages);
        collect(flipped.work, pages, pixels);
        if (observer) { observer->display(view(), flipped); }
        const auto copied = executor->postcopy(request.copyProfile);
        collect(copied, pages, pixels);
        if (observer) { observer->postcopy(view(), copied); }
        pages.flip();
        const auto ownedGuards = controller->augmentGuards(guards);
        if (observer) { observer->tailEntry(view(), state, ownedGuards); }
        const auto finished = executor->finish(state, ownedGuards);
        collect(finished.work, pages, pixels);
        if (observer) { observer->tail(view(), finished); }
        if (ownedGuards.sourceKeyboardEffects) {
            controller->completeTail(state.loopTick, finished.state);
        } else {
            controller->consumeEmptyTail(state.loopTick);
        }
        ++emptyTailReads;
        arrivalExecuted = true;
        return finished.state.state;
    }
    void bootstrap() {
        synth.startLevel1(presentationPrerollCallbacks);
        if (observer) { observer->startup(view()); }
        synth.clearRegisterWrites();
        const auto entrySamples = clock.renderEntry().size();
        if (observer) { observer->initialEntry(view(), entrySamples); }
        synth.clearRegisterWrites();
        for (std::uint32_t tick = 1; tick <= bootstrapTicks; ++tick) {
            arrivalExecuted = false;
            const auto beforeInput = nextEvent;
            applyInput(tick, ControlledInputDelivery::beforeTick);
            const auto previous = model.phase();
            const auto previousLoop = model.level2LoopTick();
            model.setSuccessAudioActive(synth.successActive());
            if (tick == bootstrapTicks) {
                if (previous != GameplayPhase::level2) {
                    throw std::invalid_argument("controlled bootstrap did not reach raw4 Level2");
                }
                priorRaw4Audio = model.level2AudioEvents().size();
                class Raw4Producer final : public Level2FrameRenderer {
                public:
                    explicit Raw4Producer(Impl& owner) : owner_(owner) {}
                    void render(Level2RenderState state) override {
                        renderReturnedRaw4(state, owner_.model, owner_.assets, owner_.request.horizon,
                            owner_.request.settings, owner_.pages, owner_.pixels);
                    }
                    std::optional<FrameTailState> finish(const FrameTailState& state, const FrameTailGuards& guards) override {
                        return owner_.finishRaw4(state, guards);
                    }
                private:
                    Impl& owner_;
                } producer(*this);
                model.tickWithRenderer(input(), producer);
            } else {
                model.tick(input());
            }
            if (!controller && previous == GameplayPhase::successPresentation && model.phase() == GameplayPhase::level2Handoff) {
                if (!released(input()) || model.level2LoopTick() != 0) {
                    throw std::invalid_argument("controlled bootstrap requires released controller loop0");
                }
                controller.emplace();
                controller->initialize({FrameControllerOrigin::restoredDiagnosticInitial, 0,
                    restoredKeyboardHead, restoredKeyboardHead, {}, 0, false, false, false, false, false, {},
                    request.initialKeyboardSlots});
            } else if (controller) {
                if (beforeInput != nextEvent || !released(input())) {
                    controller->delivery(FrameControllerDelivery::keyboard);
                }
                if ((previous != GameplayPhase::level2Handoff && previous != GameplayPhase::level2) ||
                    model.phase() != GameplayPhase::level2) {
                    throw std::invalid_argument("controlled bootstrap left ongoing Level2");
                }
                if (!arrivalExecuted) {
                    controller->consumeEmptyTail(previousLoop);
                    ++emptyTailReads;
                }
            }
            if (model.phase() == GameplayPhase::level1) { pages.flip(); }
            if (previous == GameplayPhase::successPresentation && model.phase() == GameplayPhase::level2Handoff) {
                initializePages();
            }
            if (model.phase() == GameplayPhase::level2Handoff || model.phase() == GameplayPhase::level2) {
                GameplayAnalysisAccess::applyLevel2Armor(model, heldArmor);
                if (!arrivalExecuted) {
                    drawRadar();
                    pages.flip();
                }
            }
            ControlledBootstrapTick report{tick, previous, 0, {}, std::nullopt, arrivalExecuted};
            if (previous != GameplayPhase::successPresentation && model.phase() == GameplayPhase::successPresentation) {
                report.successEntry = synth.driverState();
                synth.startSuccess();
            }
            if (previous == GameplayPhase::successPresentation && model.phase() == GameplayPhase::level2Handoff) {
                synth.stopSuccess();
                synth.switchLevel2();
            }
            const auto drain = [&](const auto& events, std::size_t& cursor) {
                while (cursor < events.size()) {
                    const auto event = events[cursor];
                    report.audioEvents.push_back(event);
                    synth.triggerLevel1Event(event.eventId);
                    ++cursor;
                }
            };
            drain(model.level1State().audioEvents(), level1Audio);
            drain(model.level2AudioEvents(), level2Audio);
            drain(model.failureAudioEvents(), failureAudio);
            report.entryFadeSamples = startsLevelEntry(previous, model.phase()) ? clock.renderEntry().size() : 0;
            if (!arrivalExecuted) { clock.renderFrame(); }
            if (observer) { observer->bootstrapTick(view(), report); }
            synth.clearRegisterWrites();
            const bool delivered = applyInput(tick, ControlledInputDelivery::afterCheckpoint);
            if (controller && delivered) { controller->delivery(FrameControllerDelivery::keyboard); }
        }
        if (!executor || !controller || !arrivalExecuted || !released(input()) ||
            model.level2LoopTick() != controller->expectedLoopTick() ||
            model.level1State().audioEvents().size() != level1Audio ||
            model.level2AudioEvents().size() != level2Audio || model.failureAudioEvents().size() != failureAudio) {
            throw std::invalid_argument("controlled bootstrap did not produce owned applied raw4 tail");
        }
        renderer.emplace(RetainedLevel2Owners{model, assets, *executor, *controller, pages, radar, pixels,
            request.horizon, request.settings, rasterStyle, enemyFlag}, request.copyProfile, *this);
        if (observer) { observer->ready(view()); }
        scope = ControlledObservationScope::continuation;
    }
    ControlledSessionDelivery drainDelivery() {
        ControlledSessionDelivery delivery{std::exchange(samples, {}), std::exchange(scans, {}), firstSample};
        firstSample = nextSample;
        return delivery;
    }
    ControlledSessionDelivery takeDelivery() {
        if (status != Status::ready) {
            throw std::invalid_argument("controlled session cannot drain outside ready boundary");
        }
        return drainDelivery();
    }
    ControlledSessionDelivery advance(const GameplayInput& externalInput) {
        if (status != Status::ready) { throw std::invalid_argument("controlled session is unavailable"); }
        if (!released(externalInput)) { throw std::invalid_argument("controlled session requires released gameplay input"); }
        status = Status::advancing;
        try {
            bool delivered = false;
            if (postCheckpointPending) {
                delivered = applyInput(static_cast<std::uint32_t>(model.ticks()), ControlledInputDelivery::afterCheckpoint);
            }
            delivered = applyInput(static_cast<std::uint32_t>(model.ticks() + 1), ControlledInputDelivery::beforeTick) || delivered;
            if (delivered || !released(input())) {
                controller->delivery(FrameControllerDelivery::keyboard);
                throw std::invalid_argument("controlled session excludes movie input delivery");
            }
            renderer->advance(externalInput);
            postCheckpointPending = true;
            if (observer) { observer->advanced(view()); }
            auto delivery = drainDelivery();
            status = Status::ready;
            return delivery;
        } catch (...) {
            status = Status::rejected;
            throw;
        }
    }
    void renderEntry(const Level2FramePlan& plan, const Level2RenderState& frame) override {
        if (model.level2AudioEvents().size() != level2Audio ||
            model.level1State().audioEvents().size() != level1Audio || model.failureAudioEvents().size() != failureAudio ||
            !model.level2EntryPaletteActive() || model.damageFlashActive()) {
            throw std::invalid_argument("controlled session excludes new simulation audio or palette history");
        }
        if (observer) { observer->renderEntry(view(), plan, frame); }
    }
    void body(const Level2FramePlan& plan, const ClockedPresentationWork& work) override {
        collect(work, pages, pixels, plan.pages.drawPage());
        if (observer) { observer->body(view(), plan, work); }
        if (keyboard) { pollKeyboard(); }
        if (physicalReleasePending) {
            if (model.level2LoopTick() != returnedRenderLoopTick + 1) {
                throw std::invalid_argument("physical release skipped the qualified raw5 return");
            }
            executor->queueForward(KeyboardEdge::release);
            physicalReleasePending = false;
        }
    }
    void pollKeyboard() {
        const auto state = controller->keyboardState();
        const ControlledKeyboardBoundary boundary{controller->expectedLoopTick(),
            (state.metadata48 & 1) != 0 ? ControlledForwardState::held : ControlledForwardState::released};
        const auto edge = keyboard->forwardAtRenderReturn(boundary);
        if (edge) { executor->queueForward(*edge); }
    }
    void display(const CockpitPageState& completedPages, const IndexedPages& completedPixels,
        const ClockedPageFlipWork& work) override {
        collect(work.work, completedPages, completedPixels);
        if (observer) { observer->display(view(completedPages, completedPixels), work); }
    }
    void postcopy(const CockpitPageState& completedPages, const IndexedPages& completedPixels,
        const ClockedPresentationWork& work) override {
        collect(work, completedPages, completedPixels);
        if (observer) { observer->postcopy(view(completedPages, completedPixels), work); }
    }
    void renderComplete(const Level2FramePlan& plan) override {
        if (observer) { observer->renderComplete(view(), plan); }
    }
    void prefixEntry(const StationaryLevel2PrefixInput& input) override {
        if (observer) { observer->prefixEntry(view(), input); }
    }
    void prefix(const RetainedPrefixExecution& result) override {
        collect(result.work, pages, pixels);
        if (observer) { observer->prefix(view(), result); }
    }
    void tailEntry(const FrameTailState& state, const FrameTailGuards& guards) override {
        if (observer) { observer->tailEntry(view(), state, guards); }
    }
    void tail(const RetainedTailExecution& result) override {
        collect(result.work, pages, pixels);
        if (observer) { observer->tail(view(), result); }
        ++emptyTailReads;
    }
};

ControlledLevel2Session::ControlledLevel2Session(GameplayAssets assets, ControlledBootstrapRequest request,
    ControlledSessionObserver* observer, ControlledKeyboardSource* keyboard) {
    validateRequest(request);
    if ((request.arrivalInput == ControlledArrivalInput::hostForward) != (keyboard != nullptr)) {
        throw std::invalid_argument("host-forward session requires its own keyboard source");
    }
    validateAssets(assets);
    impl_ = std::make_unique<Impl>(std::move(assets), std::move(request), observer, keyboard);
}
ControlledLevel2Session::~ControlledLevel2Session() = default;
ControlledLevel2Session::ControlledLevel2Session(ControlledLevel2Session&&) noexcept = default;
ControlledLevel2Session& ControlledLevel2Session::operator=(ControlledLevel2Session&&) noexcept = default;
ControlledSessionDelivery ControlledLevel2Session::advance(const GameplayInput& input) {
    if (!impl_) { throw std::invalid_argument("controlled session is moved from"); }
    return impl_->advance(input);
}
ControlledSessionDelivery ControlledLevel2Session::takeDelivery() {
    if (!impl_) { throw std::invalid_argument("controlled session is moved from"); }
    return impl_->takeDelivery();
}
} // namespace stellar7
