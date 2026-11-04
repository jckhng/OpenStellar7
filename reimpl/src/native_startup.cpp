#include "stellar7/native_startup.hpp"
#include "stellar7/timer_install_work.hpp"
#include "playable_joystick.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::uint16_t sourcePage = 0xa800;
constexpr std::uint16_t workPage = 0xa400;
constexpr std::uint16_t displayPage = 0xa000;
constexpr std::size_t width = 320;
constexpr std::size_t height = 200;
constexpr std::size_t logoStepCount = 17;
constexpr std::uint64_t irqsPerAudioCallback = 4;
constexpr std::array<int, 8> logoX{237, 236, 233, 229, 225, 219, 210, 198};
constexpr std::array<int, 8> logoY{65, 63, 61, 57, 52, 46, 38, 25};
constexpr std::int16_t copyX = 196; //22a1 aligns198 down to a plane boundary.
constexpr std::int16_t copyY = 25;
constexpr std::uint16_t copyWidth = 92;
constexpr std::uint16_t copyHeight = 90;
constexpr std::uint16_t menuPointerWidth = 16;
constexpr std::uint16_t menuPointerHeight = 21;
constexpr std::int64_t startupWorkHorizonCycles = 1000;
constexpr std::int64_t startupResourceWorkHorizonCycles = 120000;
}

NativeStartupAssets decodeNativeStartupAssets(const std::vector<std::uint8_t>& resource) {
    const auto palettes = decodePalettes(resource);
    const auto groups = decodeBitmapGroups(resource);
    if (palettes.size() < 2 || groups.size() < 2 || groups[1].size() != 8) {
        throw std::invalid_argument("startup requires STELART palettes and eight logo frames");
    }
    // Explicit6070 palette selection; the screen decoder never selects a palette.
    return {{decodeIndexedScreenResource(resource, 0), palettes[0]},
        {decodeIndexedScreenResource(resource, 1), palettes[1]}, groups[1],palettes.size()>2?palettes[2]:std::vector<Rgb>{}};
}

NativeAudioOwner::NativeAudioOwner(const GameplayAssets& assets, const SoundBlasterInitialization initialization)
    : synth_(assets.soundSequences, assets.soundBlasterInstrumentResource,
          assets.musicForLevel(0), assets.presentationMusicSequence,
          assets.musicForLevel(1), assets.musicForLevel(2), assets.logoMusicSequence,
          assets.soundBlasterVolumeTable, initialization), currentScores_(assets.highscoreTable) {}

NativeStartupSequence::NativeStartupSequence(const GameplayAssets& game,
    const NativeStartupAssets& resources, const LogoCompletion completion,
    std::shared_ptr<HighscoreStore> highscoreStore)
    : game_(game), resources_(resources), completion_(completion), highscoreStore_(std::move(highscoreStore)) {
    if (resources.logoScreen.pixels.size() != width * height || resources.logoScreen.palette.size() != 256
        || resources.menuScreen.palette.size() != 256 || game.uiPalette.size() != 256 || resources.logoFrames.size() != 8) {
        throw std::invalid_argument("unsupported startup resource dimensions");
    }
    for (const auto& bitmap : resources.logoFrames) {
        if (bitmap.width == 0 || bitmap.height == 0 || bitmap.width > 90 || bitmap.height > 90
            || bitmap.pixels.size() != static_cast<std::size_t>(bitmap.width) * bitmap.height) {
            throw std::invalid_argument("unsupported startup logo bitmap");
        }
    }
}

const NativeStartupContextRun* NativeStartupSequence::recoveredContext() const noexcept {
    return audio_ && audio_->context_ ? &*audio_->context_ : nullptr;
}

const NativeStartupSettingsRun* NativeStartupSequence::recoveredSettings() const noexcept {
    return audio_ && audio_->settings_ ? &*audio_->settings_ : nullptr;
}

const NativeStartupResourcesRun* NativeStartupSequence::recoveredResources() const noexcept {
    return audio_ && audio_->resources_ ? &*audio_->resources_ : nullptr;
}

void NativeStartupSequence::initializeResources(const NativeStartupDeviceProfile profile) {
    if (phase_ != NativeStartupPhase::awaitingResourceInitialization || !audio_ || !audio_->settings_ || !audio_->workPhase_) {
        throw std::logic_error("resource initialization requires the retained native settings return");
    }
    if (profile != NativeStartupDeviceProfile::portableOplNoDspProbe
        || game_.soundBlasterVolumeTable.size() != 16 * 64 || !audio_->synth_.hasInstrumentBank()
        || resources_.menuScreen.pixels.size() != width * height) {
        throw std::invalid_argument("unsupported native startup resources or device capability");
    }
    const auto music = audio_->startupMusicProfile_ == StartupMusicProfile::playableUnpublished
        ? StartupResourceMusicProfile::playableUnpublished : StartupResourceMusicProfile::sourceSelected;
    auto resources = startupResourcesOperations(audio_->settings_->settings, game_.soundBlasterInstrumentResource, music);
    // Native providers allocate before clock/OPL mutation; no DOS heap timing claim.
    std::vector<std::uint8_t> transfer(static_cast<std::size_t>(resources.state.transferWidth) * resources.state.transferHeight);
    std::vector<std::uint8_t> scratch(resources.state.scratchBytes);
    auto work = audio_->clock_.renderWork(*audio_->workPhase_, resources.operations, startupResourceWorkHorizonCycles);
    audio_->workPhase_ = work.timing.phase;
    audio_->resources_.emplace(NativeStartupResourcesRun{std::move(resources), std::move(work),
        std::move(transfer), std::move(scratch)});
    phase_ = NativeStartupPhase::awaitingPresentationResources;
}

void NativeStartupSequence::initializeSettings(const StartupCapabilityProfile profile) {
    if (phase_ != NativeStartupPhase::awaitingSettings || !audio_ || !audio_->context_ || !audio_->workPhase_) {
        throw std::logic_error("settings require the retained native context return");
    }
    if (!game_.cockpitSettings || !game_.highscoreTable) {
        throw std::invalid_argument("native settings require a decoded configuration and highscore table");
    }
    if (profile == StartupCapabilityProfile::portablePointer && (!audio_->playablePreferences_
        || audio_->pointerCapability_ != NativePointerCapability::portablePointer)) {
        throw std::invalid_argument("portable settings require the actual declared pointer capability");
    }
    auto settings=audio_->joystick_ ? audio_->joystickSettings(*game_.cockpitSettings,*game_.highscoreTable,profile)
        : startupSettingsOperations(audio_->context_->context.state,*game_.cockpitSettings,*game_.highscoreTable,profile);
    const bool unpublished = audio_->startupMusicProfile_ == StartupMusicProfile::playableUnpublished;
    if (unpublished && settings.configuration.options[2] != 0) {
        throw std::invalid_argument("native startup music profile differs from its decoded setting");
    }
    auto work = audio_->clock_.renderWork(*audio_->workPhase_, settings.operations, startupWorkHorizonCycles);
    audio_->context_->context.state.viewDescriptor = settings.viewDescriptor;
    audio_->workPhase_ = work.timing.phase;
    audio_->settings_.emplace(NativeStartupSettingsRun{std::move(settings), std::move(work)});
    phase_ = NativeStartupPhase::awaitingResourceInitialization;
}

void NativeStartupSequence::start(const NativeStartupExecution execution, const NativePointerCapability pointerCapability, const NativeMenuFeatures menuFeatures,const NativeJoystickSample joystick) {
    if (phase_ != NativeStartupPhase::idle) {
        throw std::logic_error("startup already started");
    }
    if (execution != NativeStartupExecution::recoveredContext && execution != NativeStartupExecution::sampleCompatibility
        && execution != NativeStartupExecution::playableMusicOff && execution != NativeStartupExecution::playableContext) {
        throw std::invalid_argument("unsupported native startup execution domain");
    }
    const bool playable = execution == NativeStartupExecution::playableContext || execution == NativeStartupExecution::playableMusicOff;
    if (!validNativeMenuFeatures(menuFeatures) || (!playable && menuFeatures!=NativeMenuFeatures::legacy)) {
        throw std::invalid_argument("menu features require their explicit playable startup");
    }
    if ((pointerCapability != NativePointerCapability::unavailable && pointerCapability != NativePointerCapability::portablePointer)
        || (pointerCapability != NativePointerCapability::unavailable && !playable)) {
        throw std::invalid_argument("portable pointer requires an explicit playable startup");
    }
    if (hasNativeMenuFeature(menuFeatures,NativeMenuFeatures::storyPanels)
        && (!game_.storyScenes || !game_.cockpitSettings
            || (game_.cockpitSettings->options[5]!=0 && game_.cockpitSettings->options[5]!=255))) {
        throw std::invalid_argument("Story startup requires complete preflighted scenes and option0/255");
    }
    audio_ = std::make_unique<NativeAudioOwner>(game_, execution != NativeStartupExecution::sampleCompatibility
        ? SoundBlasterInitialization::recoveredWork : SoundBlasterInitialization::compatibility);
    audio_->pointerCapability_ = pointerCapability;
    audio_->menuFeatures_ = menuFeatures;
    if(hasNativeMenuFeature(menuFeatures,NativeMenuFeatures::joystickControls)) {
        audio_->joystick_=std::make_unique<PlayableJoystick>();audio_->acceptJoystickSample(joystick);
    } else if(joystick.connected) {throw std::invalid_argument("joystick requires its explicit native capability");}
    if(hasNativeMenuFeature(menuFeatures,NativeMenuFeatures::automaticAttract)) {audio_->startupAssets_=std::make_shared<const NativeStartupAssets>(resources_);}
    if(hasNativeMenuFeature(menuFeatures,NativeMenuFeatures::automaticAttract) && game_.attractVoiceAvailable
        && (resources_.reentryPalette.size()!=256 || !game_.cockpitSettings
            || (game_.cockpitSettings->options[5]!=0 && (game_.cockpitSettings->options[5]!=255 || !game_.storyScenes)))) {
        throw std::invalid_argument("automatic attract requires complete palettes and actual enabled Story resources");
    }
    if (execution != NativeStartupExecution::sampleCompatibility) {
        audio_->playablePreferences_ = execution == NativeStartupExecution::playableContext
            || execution == NativeStartupExecution::playableMusicOff;
        audio_->startupMusicProfile_ = execution == NativeStartupExecution::playableMusicOff
            ? StartupMusicProfile::playableUnpublished : StartupMusicProfile::sourceSelected;
        auto context = startupContextOperations();
        auto phase = nativeTimerInstallPhase();
        phase.soundBlasterInitialization.emplace();
        auto installer = audio_->clock_.renderWork(phase, timerInstallOperations(), startupWorkHorizonCycles);
        auto work = audio_->clock_.renderWork(installer.timing.phase, context.operations, startupWorkHorizonCycles);
        audio_->workPhase_ = work.timing.phase;
        audio_->workPhase_->playableStartup = execution == NativeStartupExecution::playableMusicOff;
        audio_->workPhase_->playableMusicOff = execution == NativeStartupExecution::playableMusicOff;
        audio_->workPhase_->playableJoystickControls=hasNativeMenuFeature(menuFeatures,NativeMenuFeatures::joystickControls);
        audio_->workPhase_->playableAutomaticAttract=hasNativeMenuFeature(menuFeatures,NativeMenuFeatures::automaticAttract);
        audio_->context_.emplace(NativeStartupContextRun{std::move(context), std::move(installer), std::move(work)});
        phase_ = NativeStartupPhase::awaitingSettings;
        return;
    }
    audio_->synth_.beginLogo();
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            pages_.write(workPage, {static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)},
                resources_.logoScreen.pixels[y * width + x]);
        }
    }
    pages_.apply(std::vector<IndexedPageEffect>{
        RadarPageCopy{workPage, displayPage, {0, 0}, width, height},
        RadarPageCopy{workPage, sourcePage, {0, 0}, width, height}});
    frame_ = resources_.logoScreen;
    for (auto& color : frame_.palette) {
        for (auto* component : {&color.red, &color.green, &color.blue}) {
            const auto dac = ((*component >> 2) * 60) / 63;
            *component = static_cast<std::uint8_t>((dac << 2) | (dac >> 4));
        }
    }
    //9d9e endpoint; intermediate palette uploads and CPU/VGA timing remain unqualified.
    phase_ = NativeStartupPhase::logoDelay;
}

void NativeStartupSequence::acceptSkipInput() {
    if (phase_ == NativeStartupPhase::idle || phase_ >= NativeStartupPhase::menu) {
        throw std::logic_error("intro skip input is outside its supported gate");
    }
    skipAccepted_ = true;
}

void NativeStartupSequence::drawLogoStep() {
    audio_->synth_.beginLogoFrame(); //5ffa writes the countdown before drawing.
    pages_.apply(std::vector<IndexedPageEffect>{
        RadarPageCopy{sourcePage, workPage, {copyX, copyY}, copyWidth, copyHeight}});
    const int index = logoSteps_ < 8 ? static_cast<int>(logoSteps_) : 15 - static_cast<int>(logoSteps_);
    if (index > 0) {
        const auto& bitmap = resources_.logoFrames[index];
        for (std::uint16_t y = 0; y < bitmap.height; ++y) {
            for (std::uint16_t x = 0; x < bitmap.width; ++x) {
                const auto color = bitmap.pixels[y * bitmap.width + x];
                if (color != 0) {
                    pages_.write(workPage, {static_cast<std::int16_t>(logoX[index] + x),
                        static_cast<std::int16_t>(logoY[index] + y)}, color);
                }
            }
        }
    }
    pages_.apply(std::vector<IndexedPageEffect>{
        RadarPageCopy{workPage, displayPage, {copyX, copyY}, copyWidth, copyHeight}});
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            frame_.pixels[y * width + x] = pages_.read(displayPage,
                {static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)});
        }
    }
    ++logoSteps_;
}

void NativeStartupSequence::transition() {
    auto& synth = audio_->synth_;
    if (phase_ == NativeStartupPhase::logoDelay && countdown() == 0) {
        synth.triggerLogoEvent();
        phase_ = NativeStartupPhase::logoFrames;
        drawLogoStep();
    } else if (phase_ == NativeStartupPhase::logoFrames && countdown() == 0) {
        if (logoSteps_ < logoStepCount) {
            drawLogoStep();
        } else {
            synth.finishLogo(completion_);
            phase_ = NativeStartupPhase::logoEnd;
        }
    } else if (phase_ == NativeStartupPhase::logoEnd && synth.logoFinished()) {
        synth.stopLogo();
        if (!skipAccepted_) {
            phase_ = NativeStartupPhase::rejected;
            throw std::logic_error("optional intro scenes and input-device gates are not implemented");
        }
        //9d4c selects/clears the display via6622; SCR1/decorations are bypassed.
        for (std::size_t y = 0; y < height; ++y) {
            for (std::size_t x = 0; x < width; ++x) {
                pages_.write(displayPage, {static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)}, 0);
            }
        }
        std::fill(frame_.pixels.begin(), frame_.pixels.end(), 0);
        //641d/643b releases PAL1 and loads PAL2 before16b0 restores the menu DAC.
        frame_.palette = game_.uiPalette;
        synth.beginPresentation();
        pages_.apply(mainMenuPageEffects(game_.artGroups.at(0), game_.titleFont, workPage));
        pages_.apply(std::vector<IndexedPageEffect>{RadarPageCopy{workPage, displayPage, {0, 0}, width, height}});
        synth.beginMenu();
        phase_ = NativeStartupPhase::menu;
        drawMenuCursor();
    } else if (phase_ == NativeStartupPhase::beginWait && countdown() == 0) {
        synth.beginGameplay();
        audio_->ready_ = true;
        phase_ = NativeStartupPhase::ready;
    }
}

std::vector<std::int16_t> NativeStartupSequence::advance(const std::size_t sampleBudget) {
    if (phase_ == NativeStartupPhase::idle || phase_ == NativeStartupPhase::adopted
        || phase_ == NativeStartupPhase::rejected || phase_ == NativeStartupPhase::awaitingSettings
        || phase_ == NativeStartupPhase::awaitingResourceInitialization
        || phase_ == NativeStartupPhase::awaitingPresentationResources
        || phase_ == NativeStartupPhase::awaitingFontResources
        || phase_ == NativeStartupPhase::logoCountdownWork
        || phase_ == NativeStartupPhase::awaitingLogoFrames
        || phase_ == NativeStartupPhase::awaitingLogoAnimation
        || phase_ == NativeStartupPhase::awaitingLogoSoundEnd
        || phase_ == NativeStartupPhase::awaitingLogoRelease
        || phase_ == NativeStartupPhase::awaitingIntroInput
        || phase_ == NativeStartupPhase::awaitingMenuMusic
        || phase_ == NativeStartupPhase::awaitingMenuResources
        || phase_ == NativeStartupPhase::awaitingMenuConstruction
        || phase_ == NativeStartupPhase::awaitingMenuEntry
        || phase_ == NativeStartupPhase::awaitingMenuDecision
        || phase_ == NativeStartupPhase::awaitingGameplayLoader) {
        throw std::logic_error("startup cannot advance in this phase");
    }
    std::vector<std::int16_t> result;
    result.reserve(sampleBudget);
    for (std::size_t sample = 0; sample < sampleBudget && phase_ != NativeStartupPhase::ready; ++sample) {
        const auto before = audio_->synth_.driverState();
        const auto irqPeriod = before.clockPeriod / irqsPerAudioCallback;
        const bool initialEdge = elapsedSamples() == 0 && before.clockPhase >= before.clockPeriod;
        const auto pcm = audio_->clock_.renderSamples(1);
        result.push_back(pcm.front());
        const auto after = audio_->synth_.driverState();
        // Derive native IRQ edges from the existing driver phase, never a second clock.
        if (initialEdge || after.clockPhase % irqPeriod < before.clockPhase % irqPeriod) {
            audio_->synth_.advanceStartupIrq();
        }
        const auto previous = phase_;
        transition();
        // Expose menu/Begin boundaries exactly, independent of caller chunk size.
        if (previous != phase_ && (phase_ == NativeStartupPhase::menu || phase_ == NativeStartupPhase::ready)) {
            break;
        }
    }
    return result;
}

void NativeStartupSequence::requestBegin() {
    if (phase_ != NativeStartupPhase::menu || menu_.selection() != 0) {
        throw std::logic_error("Begin requires the menu owner");
    }
    audio_->synth_.fadePresentation(); //165e precedes the display fade/clear.
    //166b/9d4c clears the displayed menu before the caller's Begin wait.
    for (std::int16_t y = 0; y < height; ++y) {
        for (std::int16_t x = 0; x < width; ++x) {
            pages_.write(displayPage, {x, y}, 0);
        }
    }
    std::fill(frame_.pixels.begin(), frame_.pixels.end(), 0);
    for (auto& color : frame_.palette) {
        for (auto* component : {&color.red, &color.green, &color.blue}) {
            const auto dac = ((*component >> 2) * 3) / 63;
            *component = static_cast<std::uint8_t>((dac << 2) | (dac >> 4));
        }
    }
    audio_->synth_.requestGameplay(); //015a precedes8cee/113a's seven-byte copy.
    difficulty_.fill(3); //Immutable1587, difficulty0; other difficulties are not admitted.
    phase_ = NativeStartupPhase::beginWait;
}

void NativeStartupSequence::drawMenuWidget(const std::size_t selection, const MenuWidgetState state) {
    constexpr std::array<std::int16_t, 5> rows{58, 90, 115, 140, 165};
    pages_.apply(mainMenuWidgetEffects(game_.artGroups.at(0), game_.titleFont, selection, state, workPage));
    pages_.apply(std::vector<IndexedPageEffect>{RadarPageCopy{workPage, displayPage,
        {102, rows.at(selection)}, 116, 30}});
}

void NativeStartupSequence::drawMenuCursor() {
    const auto& pointer = game_.artGroups.at(0).at(0);
    if (pointer.width != menuPointerWidth || pointer.height + 1 != menuPointerHeight) {
        throw std::invalid_argument("menu requires source pointer bitmap");
    }
    if (menuCursor_) {
        pages_.apply(std::vector<IndexedPageEffect>{RadarPageCopy{workPage, displayPage,
            *menuCursor_, menuPointerWidth, menuPointerHeight}});
    }
    const ProjectedPoint origin{static_cast<std::int16_t>(menu_.cursorX()),
        static_cast<std::int16_t>(menu_.cursorY())};
    std::array<std::uint8_t, menuPointerWidth * menuPointerHeight> background{};
    for (std::uint16_t y = 0; y < menuPointerHeight; ++y) {
        for (std::uint16_t x = 0; x < menuPointerWidth; ++x) {
            const ProjectedPoint point{static_cast<std::int16_t>(origin.x + x), static_cast<std::int16_t>(origin.y + y)};
            background[y * menuPointerWidth + x] = pages_.read(workPage, point);
            if (y < pointer.height && pointer.pixels[y * pointer.width + x] != 0) {
                pages_.write(workPage, point, pointer.pixels[y * pointer.width + x]);
            }
        }
    }
    pages_.apply(std::vector<IndexedPageEffect>{RadarPageCopy{workPage, displayPage,
        origin, menuPointerWidth, menuPointerHeight}});
    for (std::uint16_t y = 0; y < menuPointerHeight; ++y) {
        for (std::uint16_t x = 0; x < menuPointerWidth; ++x) {
            pages_.write(workPage, {static_cast<std::int16_t>(origin.x + x), static_cast<std::int16_t>(origin.y + y)},
                background[y * menuPointerWidth + x]);
        }
    }
    menuCursor_ = origin;
    frame_.pixels = menuPagePixels(NativeMenuPage::front);
}

NativeMenuAction NativeStartupSequence::pollMenu(const NativeMenuInput input) {
    if (phase_ != NativeStartupPhase::menu) {
        throw std::logic_error("menu input requires the current menu owner");
    }
    if ((input.up || input.down) && menuArmed_) {
        if (pressedMenu_) {
            drawMenuWidget(*pressedMenu_, MenuWidgetState::released);
            pressedMenu_.reset();
        }
        if (input.up) { menu_.moveUp(); } else { menu_.moveDown(); }
        menuArmed_ = false;
        drawMenuCursor();
        return NativeMenuAction::none;
    }
    if (!input.up && !input.down) {
        menuArmed_ = true;
    }
    if (input.confirm && !pressedMenu_) {
        pressedMenu_ = menu_.selection();
        drawMenuWidget(*pressedMenu_, MenuWidgetState::pressed);
        drawMenuCursor();
        return NativeMenuAction::none;
    }
    if (input.confirm || !pressedMenu_) {
        return NativeMenuAction::none;
    }
    const auto selected = *pressedMenu_;
    pressedMenu_.reset();
    drawMenuWidget(selected, MenuWidgetState::released);
    drawMenuCursor();
    if (selected == 0) {
        requestBegin();
        return NativeMenuAction::begin;
    }
    return selected + 1 == MenuModel::itemCount ? NativeMenuAction::exit : NativeMenuAction::unsupported;
}

std::uint16_t NativeStartupSequence::menuPageSegment(const NativeMenuPage page) const noexcept {
    if(storyRoles_) {
        return page==NativeMenuPage::front ? storyRoles_->frontPage()
            : page==NativeMenuPage::back ? storyRoles_->backPage() : storyRoles_->cachePage();
    }
    if (audio_ && audio_->context_) {
        return page == NativeMenuPage::cache ? workPage : page == NativeMenuPage::back ? sourcePage : displayPage;
    }
    return page == NativeMenuPage::cache ? sourcePage : page == NativeMenuPage::back ? workPage : displayPage;
}

std::vector<std::uint8_t> NativeStartupSequence::menuPagePixels(const NativeMenuPage page) const {
    if (phase_ != NativeStartupPhase::menu && phase_ != NativeStartupPhase::beginWait
        && phase_ != NativeStartupPhase::awaitingFontResources
        && phase_ != NativeStartupPhase::logoCountdownWork && phase_ != NativeStartupPhase::awaitingLogoFrames
        && phase_ != NativeStartupPhase::awaitingLogoAnimation
        && phase_ != NativeStartupPhase::awaitingLogoSoundEnd
        && phase_ != NativeStartupPhase::awaitingLogoRelease
        && phase_ != NativeStartupPhase::awaitingIntroInput
        && phase_ != NativeStartupPhase::awaitingMenuMusic
        && phase_ != NativeStartupPhase::awaitingMenuResources
        && phase_ != NativeStartupPhase::awaitingMenuConstruction
        && phase_ != NativeStartupPhase::awaitingMenuEntry
        && phase_ != NativeStartupPhase::awaitingMenuDecision
        && phase_ != NativeStartupPhase::awaitingGameplayLoader) {
        throw std::logic_error("menu observation requires its retained page owner");
    }
    return pages_.snapshot(menuPageSegment(page));
}

std::unique_ptr<NativeAudioOwner> NativeStartupSequence::releaseAudio() {
    if (phase_ != NativeStartupPhase::ready || !audio_) {
        throw std::logic_error("startup audio can be adopted exactly once after Begin");
    }
    phase_ = NativeStartupPhase::adopted;
    return std::move(audio_);
}
std::uint64_t NativeStartupSequence::elapsedSamples() const noexcept {
    return audio_ ? audio_->elapsedSamples() : 0;
}
std::uint16_t NativeStartupSequence::countdown() const noexcept {
    if (audio_ && audio_->workPhase_ && audio_->workPhase_->timer) {
        return audio_->workPhase_->timer->globalCount;
    }
    return audio_ ? audio_->synth_.startupCountdown() : 0;
}

}  // namespace stellar7
