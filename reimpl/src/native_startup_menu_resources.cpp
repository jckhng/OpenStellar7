#include "stellar7/native_startup.hpp"

#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int64_t menuResourceHorizon = 120000;
constexpr std::size_t menuBitmapGroups = 3;
constexpr std::size_t firstMenuBitmapGroup = 3;
constexpr std::size_t menuColorTables = 2;
constexpr std::size_t paletteEntries = 256;

std::vector<std::vector<NormalWorkOperation>> beforeBlackout(const CockpitSettings& loaded,const StartupCapabilityProfile capability) {
    return {
        {{NormalWorkKind::instructions, 2}}, //40A5 PUSH/CALL4848: retained font0.
        {{NormalWorkKind::instructions, 3}}, //40AE cleanup/PUSH/CALL5012: STELART bitmap3.
        {{NormalWorkKind::instructions, 6}}, //40BA..40D2: retain/require/cache bitmap3.
        {{NormalWorkKind::instructions, 3}}, //40D7..40DE: bitmap4.
        {{NormalWorkKind::instructions, 6}}, //40E3..40F4: Mode-X1334=0 also loads bitmap5.
        {{NormalWorkKind::instructions, 8}}, //40F9..40FF plus complete B3D9's five instructions.
        {{NormalWorkKind::instructions, 31}}, //4102..4122 plus found-ARR3FFC's25 instructions.
        {{NormalWorkKind::instructions, 30}}, //4125..4130 plus second3FFC; providers untimed.
        {{NormalWorkKind::instructions, 2}}, //4133 cleanup/CALL30D6: native FILE/table reload.
        //Source3108/310B and absent-device helpers remain outside the FILE provider.
        {{NormalWorkKind::instructions, loaded.options[0] == 0 ? 10U : capability==StartupCapabilityProfile::portablePointer ? 2U : 14U}},
        {{NormalWorkKind::instructions, 1}}, //4139 CALL9D4C; complete child owns its work once.
    };
}
}

const NativeStartupMenuResourcesRun* NativeStartupSequence::recoveredMenuResources() const noexcept {
    return audio_ && audio_->menuResources_ ? &*audio_->menuResources_ : nullptr;
}

void NativeStartupSequence::initializeMenuResources() {
    if (phase_ != NativeStartupPhase::awaitingMenuResources || titleActive() || !audio_ || !audio_->workPhase_
        || !audio_->menuSong_ || !audio_->intro_ || !audio_->fonts_ || !audio_->context_) {
        throw std::logic_error("menu resources require the retained selected presentation song");
    }
    if (game_.artGroups.size() < firstMenuBitmapGroup + menuBitmapGroups || game_.modelColorTables.size() < menuColorTables
        || game_.uiPalette.size() != paletteEntries || !game_.cockpitSettings || !game_.highscoreTable
        || audio_->fonts_->firstFont.height == 0 || audio_->fonts_->firstFont.glyphs.empty()) {
        throw std::invalid_argument("menu resources require complete Mode-X bitmaps, arrays, fonts and settings");
    }
    NativeStartupMenuResourcesRun run{};
    for (std::size_t index = 0; index < menuBitmapGroups; ++index) {
        const auto& group = game_.artGroups[firstMenuBitmapGroup + index];
        if (group.empty()) {
            throw std::invalid_argument("menu bitmap provider returned an empty group");
        }
        for (const auto& bitmap : group) {
            if (bitmap.width == 0 || bitmap.height == 0
                || bitmap.pixels.size() != static_cast<std::size_t>(bitmap.width) * bitmap.height) {
                throw std::invalid_argument("menu bitmap provider returned incomplete pixels");
            }
        }
        run.bitmapCounts[index] = group.size();
    }
    run.cache = cacheBitmapGroup(game_.artGroups[firstMenuBitmapGroup], 0);
    // Reuse the same decoded FILE/header/table and absent-device contract.
    // No save occurred since the first load. The live view descriptor is retained.
    const auto capability=audio_->pointerCapability_==NativePointerCapability::portablePointer
        ? StartupCapabilityProfile::portablePointer : StartupCapabilityProfile::noMouseOrJoystick;
    const auto reloaded=audio_->joystick_ ? audio_->joystickSettings(*game_.cockpitSettings,*game_.highscoreTable,capability)
        : startupSettingsOperations(audio_->context_->context.state,*game_.cockpitSettings,*game_.highscoreTable,capability);
    run.configuration = reloaded.configuration;
    run.pointerCapability=reloaded.capability;
    run.randomSeed = {0x30, 0x6e}; //Actual B3D9 stores19D0/19D2; no host-time seed.
    auto prefix = beforeBlackout(*game_.cockpitSettings,reloaded.capability);
    if(audio_->joystick_&&game_.cockpitSettings->options[1]!=0) {++prefix[prefix.size()-2].front().count;}
    const std::vector<std::uint64_t> suffix{2, 3, 4, 3, 3, 2}; //413C..4178; native palette/close providers.
    BlackoutWorkState surface{};
    surface.palette = audio_->intro_->palette;
    surface.surface.clipping = BlackoutClip::disabled;
    const auto front = menuPageSegment(NativeMenuPage::front);
    const auto paletteHorizon = [&](const SoundBlasterClockPhase& phase) {
        return hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::storyPanels)
            &&phase.callback&&phase.callback->program ? 2*menuResourceHorizon : menuResourceHorizon;
    };
    auto planned = *audio_->workPhase_;
    for (const auto& operations : prefix) {
        planned = normalWorkTiming(planned, operations, menuResourceHorizon).phase;
    }
    planned = blackoutClockTiming(planned, surface, front, paletteHorizon(planned),storyRoles_?BlackoutPageLayout::recoveredStartup:BlackoutPageLayout::compatibility).clear.phase;
    for (const auto count : suffix) {
        planned = normalWorkTiming(planned, {{NormalWorkKind::instructions, count}}, menuResourceHorizon).phase;
    }

    run.firstSample = elapsedSamples();
    run.driverBefore = audio_->synth_.driverState();
    auto phase = *audio_->workPhase_;
    const auto append = [&](const ClockedPresentationWork& work) {
        run.samples.insert(run.samples.end(), work.audio.samples.begin(), work.audio.samples.end());
    };
    for (const auto& operations : prefix) {
        auto work = audio_->clock_.renderWork(phase, operations, menuResourceHorizon);
        phase = work.timing.phase;
        append(work);
        run.before.push_back(std::move(work));
    }
    run.blackout = audio_->clock_.renderBlackout(phase, surface, front, pages_, paletteHorizon(phase),storyRoles_?BlackoutPageLayout::recoveredStartup:BlackoutPageLayout::compatibility);
    for (const auto& step : run.blackout.steps) {
        append(step.before);
        append(step.palette.work);
    }
    append(run.blackout.clear);
    phase = run.blackout.clear.timing.phase;
    run.palette = run.blackout.state.palette;
    for (std::size_t index = 0; index < suffix.size(); ++index) {
        auto work = audio_->clock_.renderWork(phase,
            {{NormalWorkKind::instructions, suffix[index]}}, menuResourceHorizon);
        phase = work.timing.phase;
        append(work);
        run.after.push_back(std::move(work));
        if (index == 2) {
            //4158/1A22 completes the loaded PAL2 upload and both bank copies.
            for (std::size_t color = 0; color < paletteEntries; ++color) {
                run.palette.source[color * 3] = game_.uiPalette[color].red >> 2;
                run.palette.source[color * 3 + 1] = game_.uiPalette[color].green >> 2;
                run.palette.source[color * 3 + 2] = game_.uiPalette[color].blue >> 2;
            }
            run.palette = completedFullPaletteUpload(run.palette, {0, 256, 0, 63});
        }
    }
    run.streamsClosed = true;
    run.driverAfter = audio_->synth_.driverState();
    run.finalPhase = phase;
    frame_.pixels = menuPagePixels(NativeMenuPage::front);
    frame_.palette = expandVgaDacPalette(std::vector<std::uint8_t>(run.palette.dac.colors.begin(), run.palette.dac.colors.end()));
    audio_->workPhase_ = phase;
    audio_->menuResources_ = std::move(run);
    phase_ = NativeStartupPhase::awaitingMenuConstruction;
}

} // namespace stellar7
