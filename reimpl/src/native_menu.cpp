#include "stellar7/native_session.hpp"
#include "stellar7/blackout_work.hpp"
#include "playable_detail_render.hpp"

#include <algorithm>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::uint16_t width = 320;
constexpr std::uint16_t height = 200;
constexpr std::uint16_t pointerWidth = 16;
constexpr std::uint16_t pointerHeight = 21;
constexpr std::uint64_t pitInputClock = 1'193'182;
constexpr std::uint64_t rawIrqsPerCallback = 4;
constexpr std::array<std::int16_t, MenuModel::itemCount> widgetRows{58, 90, 115, 140, 165};

std::vector<IndexedPageEffect> pointerEffects(const IndexedBitmap& pointer, const ProjectedPoint origin,
    const std::uint16_t page) {
    if (pointer.width != pointerWidth || pointer.height + 1 != pointerHeight
        || pointer.pixels.size() != pointer.width * pointer.height) {
        throw std::invalid_argument("menu pointer requires the source16x20 bitmap");
    }
    std::vector<IndexedPageEffect> result;
    for (std::uint16_t y = 0; y < pointer.height; ++y) {
        for (std::uint16_t x = 0; x < pointer.width; ++x) {
            const auto color = pointer.pixels[y * pointer.width + x];
            if (color != 0) {
                result.emplace_back(RadarPixelWrite{
                    {static_cast<std::int16_t>(origin.x + x), static_cast<std::int16_t>(origin.y + y)}, color, page});
            }
        }
    }
    return result;
}
} //namespace

void NativeGameplaySession::beginMenuGame(NativeSessionDelivery& delivery) {
    constexpr std::size_t difficultyOption = 4; //DS:12B8 in the FAME settings record.
    constexpr std::uint16_t beginRawIrqs = 480;
    constexpr std::size_t panelGroup = 3;
    constexpr std::size_t overlayGroup = 4;
    constexpr std::uint8_t titleColor = 239;
    constexpr std::uint64_t nanosecondsPerSecond = 1'000'000'000;
    if (!assets_.cockpitSettings || assets_.cockpitSettings->options[difficultyOption] != 0
        || assets_.artGroups.size() <= overlayGroup || assets_.artGroups[panelGroup].size() < 3
        || assets_.artGroups[overlayGroup].size() < 7 || assets_.scoreFont.height != 8
        || assets_.scoreFont.glyphs.size() != 96 || assets_.cockpit.pixels.size() != width * height
        || assets_.cockpitDacPalette.size() != vgaPaletteBytes || !radar_ || level1Pages_
        || campaignResources_ || ending_ || endingAssets_ || successPalette_
        || assets_.horizonFramesForLevel(0).empty()) {
        throw std::invalid_argument("menu Begin requires the retained difficulty0 Level1 loader resources");
    }
    model_.validateMenuBegin();
    synth_.validateGameplayRequest();
    clock_.validateNativeIrqWait();
    completedFullPaletteUpload(*endingPalette_, {0, vgaPaletteColors, 0, vgaComponentMaximum});

    //Validate the complete pure loader/display path before changing live owners.
    auto stagedModel = model_;
    stagedModel.prepareMenuBegin();
    stagedModel.finishMenuBegin();
    CockpitWorkBitmaps bitmaps;
    bitmaps.strip = assets_.artGroups[panelGroup][1];
    bitmaps.matchingPageBackground = assets_.artGroups[panelGroup][2];
    bitmaps.indicator = assets_.artGroups[overlayGroup][6];
    for (std::size_t index = 0; index < bitmaps.overlays.size(); ++index) {
        bitmaps.overlays[index] = assets_.artGroups[overlayGroup][index];
    }
    const auto& record = stagedModel.lastCockpitInitialization();
    const auto settings = renderSettings();
    const auto cockpit = audio_->playablePreferences_ ? playableCockpitOperations : cockpitOperations;
    const auto matching = cockpit({record.states[0].x, record.states[0].velocity,
        settings.viewSize, CockpitPageRelation::matching}, bitmaps);
    const auto ordinary = cockpit({record.states[1].x, record.states[1].velocity,
        settings.viewSize, CockpitPageRelation::differing}, bitmaps);
    auto stagedPixels = *pixels_;
    stagedPixels.apply(cockpitResourcePageEffects(assets_.cockpit, pages_.cachePage()));
    stagedPixels.apply(cockpitCachePageEffects(matching, ordinary, bitmaps, assets_.scoreFont,
        "SOL", titleColor, pages_.cachePage(), pages_.backPage()));
    RadarState stagedRadar(assets_.artGroups[panelGroup]);
    const auto& player = stagedModel.level1State().player();
    auto stagedRoles = pages_;
    stagedRoles.beginFrame();
    stagedRoles.selectCockpitCopy();
    stagedPixels.apply(stagedRadar.initialize(stagedModel.playerArmor(), player.position, player.yaw,
        {stagedRoles.cachePage(), stagedRoles.backPage(), stagedRoles.frontPage(),
            stagedRoles.drawPage(), stagedRoles.copyPage()}));
    stagedPixels.apply(scorePageEffects(0, assets_.scoreFont, pages_.backPage(), pages_.frontPage()));
    const auto& state = stagedModel.level1State();
    stagedPixels.apply(statusPageEffects(state.systemStatusLevels(), state.systemTimers(), state.selectedSystem(),
        assets_.artGroups[overlayGroup], pages_.backPage(), pages_.frontPage()));
    stagedModel.tick({});
    if (!supportsLevel1Pages(stagedModel)) {
        throw std::invalid_argument("menu Begin reaches an unsupported retained Level1 display");
    }
    renderLevel1ViewportIndexed(assets_, stagedModel);

    renderMenuWidget(0, MenuWidgetState::released);
    renderMenuCursor();
    synth_.fadePresentation(); //165E; the second request follows9D4C, retaining AA.
    const auto fade = clock_.renderNativeBlackout({*endingPalette_, {}}, pages_, *pixels_);
    for (const auto& step : fade.steps) {
        publishMenuPalette(step.palette);
        delivery.entry.push_back({frame_, step.samples, step.elapsedNanoseconds});
    }
    frame_.pixels = *level2PagePixels(pages_.frontPage());
    publishMenuPalette(fade.state.palette);
    delivery.entry.push_back({frame_, fade.clearSamples, fade.elapsedNanoseconds});
    synth_.requestGameplay(); //0152 and015A, before score/profile writes and6638.
    model_.prepareMenuBegin();
    const auto waitSamples = clock_.nativeIrqWaitSamples(beginRawIrqs);
    std::vector<std::int16_t> waiting;
    waiting.reserve(waitSamples);
    const auto irqPeriod = synth_.driverState().clockPeriod / rawIrqsPerCallback;
    for (std::size_t index = 0; index < waitSamples; ++index) {
        const auto before = synth_.driverState().clockPhase % irqPeriod;
        const auto sample = clock_.renderSamples(1);
        waiting.push_back(sample.front());
        if (synth_.driverState().clockPhase % irqPeriod < before) {
            synth_.advanceStartupIrq();
        }
    }
    if (synth_.startupCountdown() != 0) {
        throw std::logic_error("retained Begin missed its480 rawIRQ endpoint");
    }
    const auto waitEnd = fade.elapsedNanoseconds + waitSamples * nanosecondsPerSecond / SoundBlasterSynth::sampleRate;
    delivery.entry.push_back({frame_, std::move(waiting), waitEnd});
    synth_.stopMusic(); //017B, before4E06. The later no-handle stop emits nothing.
    model_.finishMenuBegin();
    synth_.resetDirectEffects(); //17E5/206B precedes the new Level1 type0 sequence.
    synth_.beginGameplay();
    level1Cursor_ = level2Cursor_ = failureCursor_ = 0;
    initializePages(NativeSessionStart::level1); //Only the two loader50C4 draws.
    for (std::int16_t y = 0; y < height; ++y) {
        for (std::int16_t x = 0; x < width; ++x) {
            frame_.pixels[y * width + x] = pixels_->read(pages_.frontPage(), {x, y});
        }
    }
    NativeSessionDelivery entry;
    enter(entry);
    for (auto& step : entry.entry) {
        step.elapsedNanoseconds += waitEnd;
        delivery.entry.push_back(std::move(step));
    }
    synth_.resetDirectEffects(); //01AC's separate op16 follows the loader's music start/fade.
    pixels_->apply(scorePageEffects(model_.score(), assets_.scoreFont, pages_.backPage(), pages_.frontPage()));
    const auto& systems = model_.level1State();
    pixels_->apply(statusPageEffects(systems.systemStatusLevels(), systems.systemTimers(), systems.selectedSystem(),
        assets_.artGroups[overlayGroup], pages_.backPage(), pages_.frontPage())); //7970/8F1C after4E06.
    menu_.reset();
    menuPressed_.reset();
    menuCursor_.reset();
    endingPalette_.reset(); //The SCENED/menu borrow ends at the Level1 palette handoff.
    model_.tick({}); //Post-8F1C ordinary body: source tick0 and the third50C4 draw.
    refreshFrame();
    dispatch(model_.level1State().audioEvents(), level1Cursor_, delivery);
    delivery.samples = clock_.renderFrame();
}

void NativeGameplaySession::publishMenuPalette(const PaletteWorkState& state) {
    *endingPalette_ = state;
    for (std::size_t color = 0; color < vgaPaletteColors; ++color) {
        const auto expand = [&state, color](const std::size_t component) {
            const auto value = state.dac.colors[color * vgaPaletteComponents + component];
            return static_cast<std::uint8_t>((value << 2) | (value >> 4));
        };
        frame_.palette[color] = {expand(0), expand(1), expand(2)};
    }
    endingPaletteRequest_ = {0, vgaPaletteColors, 0, vgaComponentMaximum};
}

void NativeGameplaySession::renderMenuWidget(const std::size_t selection, const MenuWidgetState state) {
    pixels_->apply(mainMenuWidgetEffects(assets_.artGroups.at(0), assets_.titleFont, selection,
        state, pages_.backPage()));
    pixels_->apply(std::vector<IndexedPageEffect>{RadarPageCopy{pages_.backPage(), pages_.frontPage(),
        {102, widgetRows.at(selection)}, 116, 30}});
}

void NativeGameplaySession::renderMenuCursor() {
    if (menuCursor_) {
        pixels_->apply(std::vector<IndexedPageEffect>{RadarPageCopy{pages_.backPage(), pages_.frontPage(),
            *menuCursor_, pointerWidth, pointerHeight}});
    }
    const ProjectedPoint origin{static_cast<std::int16_t>(menu_->cursorX()),
        static_cast<std::int16_t>(menu_->cursorY())};
    renderHighscorePointer(origin, pointerEffects(assets_.artGroups.at(0).at(0), origin, pages_.backPage()));
    menuCursor_ = origin;
}

void NativeGameplaySession::returnToMenu(NativeSessionDelivery& delivery,
    const GameplayInput& input, const NameEntryInput& text) {
    if (input.left || input.right || input.forward || input.reverse || input.fire || input.exit || input.confirm
        || !text.characters.empty() || !pixels_ || !radar_ || !endingPalette_ || ending_ || endingAssets_
        || campaignResources_ || successPalette_ || menu_ || level1Pages_ || !highscoreTable_
        || highscoreTable_->rank(model_.score()) || !audio_->adopted_
        || synth_.startupCountdown() != 0
        || !assets_.cockpitSettings || assets_.cockpitSettings->options[2] == 0
        || assets_.cockpitDacPalette.size() != vgaPaletteBytes
        || !std::equal(assets_.cockpitDacPalette.begin(), assets_.cockpitDacPalette.end(), endingPalette_->source.begin())
        || frame_.pixels.size() != width * height || frame_.palette.size() != vgaPaletteColors
        || assets_.artGroups.empty() || assets_.artGroups[0].empty()) {
        throw std::invalid_argument("title return requires retained normal ending, original menu owners and a highscore rank miss");
    }
    model_.validateTitleReturn();
    clock_.validateNativeIrqWait();
    synth_.validateMenuAudioReturn();
    completedFullPaletteUpload(*endingPalette_, {0, vgaPaletteColors, 0, vgaComponentMaximum});
    if (endingPalette_->dac.mode != PaletteDacMode::write || endingPalette_->dac.pelIndex != 0) {
        throw std::invalid_argument("title return requires completed shared DAC state");
    }
    const auto composition = mainMenuPageEffects(assets_.artGroups[0], assets_.titleFont, pages_.backPage());
    const MenuModel initialMenu;
    const ProjectedPoint origin{static_cast<std::int16_t>(initialMenu.cursorX()),
        static_cast<std::int16_t>(initialMenu.cursorY())};
    const auto pointer = pointerEffects(assets_.artGroups[0][0], origin, pages_.backPage());
    for (const auto physical : {pages_.cachePage(), pages_.backPage(), pages_.frontPage()}) {
        for (std::int16_t y = 0; y < height; ++y) {
            for (std::int16_t x = 0; x < width; ++x) {
                if (!pixels_->known(physical, {x, y})
                    || (physical != pages_.cachePage() && pixels_->read(physical, {x, y}) != 0)) {
                    throw std::invalid_argument("title return requires the retained cleared front/back and scene cache");
                }
            }
        }
    }
    auto staged = *pixels_;
    staged.apply(composition);
    staged.apply(pointer); //Validate pixel lowering before changing the live MIDI owner.

    //B94 is already clear:04BA has no stop dispatch.04BF precedes330A/0147.
    synth_.restartMenuAudio();
    auto first = clock_.renderMenuPalette(*endingPalette_); //1578's3546, with02C2 already zero.
    publishMenuPalette(first.palette);
    delivery.entry.push_back({frame_, std::move(first.samples), first.elapsedNanoseconds});
    menu_.emplace(initialMenu); //5230(0);16B0 retains both physical page identities.
    pages_.beginModal();
    pixels_->apply(composition);
    pages_.selectFront();
    pixels_->apply(std::vector<IndexedPageEffect>{RadarPageCopy{pages_.backPage(), pages_.frontPage(),
        {0, 0}, width, height}});
    pages_.beginModal();
    renderMenuCursor();
    auto second = clock_.renderMenuPalette(*endingPalette_); //16B0's second3546, after composition.
    publishMenuPalette(second.palette);
    delivery.entry.push_back({frame_, std::move(second.samples),
        first.elapsedNanoseconds + second.elapsedNanoseconds});
    synth_.beginMenu(); //1804 writes30000 after rendering; it does not restart SNG1.
    model_.finishTitleReturn();
    pageProjection_ = NativePageProjection::retainedMenu;
}

NativeSessionDelivery NativeGameplaySession::advanceMenu(const NativeMenuInput input, const std::size_t sampleBudget) {
    if (!initialized_ || model_.phase() != GameplayPhase::nativeMenu || !menu_ || !pixels_ || !endingPalette_
        || synth_.startupPhase() != SoundBlasterStartupPhase::presentation
        || synth_.sampledVoiceObservation().phase != SampledVoicePhase::unloaded) {
        throw std::invalid_argument("menu polling requires its retained presentation owner");
    }
    const auto state = synth_.driverState();
    const auto irqPeriod = state.clockPeriod / rawIrqsPerCallback;
    const auto countdown = synth_.startupCountdown();
    if (state.clockSource != SoundBlasterClockSource::samples || irqPeriod == 0
        || sampleBudget > countdown * irqPeriod / pitInputClock || state.clockPhase >= state.clockPeriod * 2
        || (state.clockPhase % irqPeriod + sampleBudget * pitInputClock) / irqPeriod >= countdown) {
        throw std::invalid_argument("menu sample budget reaches an unowned idle/demo or clock boundary");
    }
    if (!input.confirm && menuPressed_ && !((input.up || input.down) && menuArmed_)) {
        if (*menuPressed_ != 0) {
            throw std::invalid_argument("menu selection requires its source dialog/return operation");
        }
        NativeSessionDelivery delivery;
        delivery.firstSample = clock_.elapsedSamples();
        beginMenuGame(delivery);
        return delivery;
    }
    NativeSessionDelivery delivery;
    delivery.firstSample = clock_.elapsedSamples();
    if ((input.up || input.down) && menuArmed_) {
        if (menuPressed_) {
            renderMenuWidget(*menuPressed_, MenuWidgetState::released);
            menuPressed_.reset();
        }
        if (input.up) {
            menu_->moveUp();
        } else {
            menu_->moveDown();
        }
        menuArmed_ = false;
        renderMenuCursor();
    } else {
        if (!input.up && !input.down) {
            menuArmed_ = true;
        }
        if (input.confirm && !menuPressed_) {
            menuPressed_ = menu_->selection();
            renderMenuWidget(*menuPressed_, MenuWidgetState::pressed);
            renderMenuCursor();
        }
    }
    delivery.samples.reserve(sampleBudget);
    for (std::size_t index = 0; index < sampleBudget; ++index) {
        const auto before = synth_.driverState().clockPhase % irqPeriod;
        const auto sample = clock_.renderSamples(1);
        delivery.samples.push_back(sample.front());
        if (synth_.driverState().clockPhase % irqPeriod < before) {
            synth_.advanceStartupIrq();
        }
    }
    model_.finishMenuPoll();
    return delivery;
}
} //namespace stellar7
