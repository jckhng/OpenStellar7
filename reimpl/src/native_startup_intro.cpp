#include "stellar7/native_startup.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int64_t introWorkHorizon = 120000;
}

const NativeStartupIntroRun* NativeStartupSequence::recoveredIntro() const noexcept {
    return audio_ && audio_->intro_ ? &*audio_->intro_ : nullptr;
}

void NativeStartupSequence::skipIntro(const NativeIntroKey key) {
    if (phase_ != NativeStartupPhase::awaitingIntroInput || !audio_ || !audio_->workPhase_
        || !audio_->presentation_ || !audio_->settings_) {
        throw std::logic_error("native intro input requires the retained logo release");
    }
    const auto& settings = audio_->settings_->settings.configuration;
    if ((settings.options[0] != 0 && (!audio_->playablePreferences_
        || audio_->pointerCapability_!=NativePointerCapability::portablePointer)) || (settings.options[1]!=0&&!audio_->joystick_)) {
        throw std::invalid_argument("native intro input requires absent mouse/joystick");
    }
    NativeStartupIntroRun run{};
    run.input = startupIntroSkipOperations(key,settings.options[0]!=0
        ? StartupCapabilityProfile::portablePointer : StartupCapabilityProfile::noMouseOrJoystick);
    std::vector<NormalWorkOperation> query{{NormalWorkKind::instructions, 3}}; //626D/626F/CALL5F4C.
    query.insert(query.end(), run.input.query.begin(), run.input.query.end());
    query.push_back({NormalWorkKind::instructions, 5}); //6273..627C, including CALL9D4C.
    BlackoutWorkState surface{};
    surface.palette = audio_->presentation_->palette;
    surface.surface.clipping = BlackoutClip::disabled; // Retained native startup profile.
    const auto front = menuPageSegment(NativeMenuPage::front);
    auto checked = normalWorkTiming(*audio_->workPhase_, query, introWorkHorizon).phase;
    checked = blackoutClockTiming(checked, surface, front, introWorkHorizon).clear.phase;
    normalWorkTiming(checked, {{NormalWorkKind::instructions, 4}}, introWorkHorizon);
    const auto append = [&](const ClockedPresentationWork& work) {
        run.samples.insert(run.samples.end(), work.audio.samples.begin(), work.audio.samples.end());
    };
    run.firstSample = elapsedSamples();
    run.query = audio_->clock_.renderWork(*audio_->workPhase_, query, introWorkHorizon);
    append(run.query);
    run.blackout = audio_->clock_.renderBlackout(run.query.timing.phase, surface, front, pages_, introWorkHorizon);
    for (const auto& step : run.blackout.steps) { append(step.before); append(step.palette.work); }
    append(run.blackout.clear);
    auto phase = run.blackout.clear.timing.phase;
    run.palette = run.blackout.state.palette;
    const auto tail = [&]() {
        auto work = audio_->clock_.renderWork(phase, {{NormalWorkKind::instructions, 2}}, introWorkHorizon);
        phase = work.timing.phase;
        append(work);
        run.paletteSelection.push_back(std::move(work));
    };
    tail(); //627F PUSH/CALL1A22.
    // Whole1A22 reuses the native provider:0736 uploads cached PAL1, then copies
    // it to output and source. The preceding blackout record remains immutable.
    for (std::size_t index = 0; index < resources_.menuScreen.palette.size(); ++index) {
        const auto& color = resources_.menuScreen.palette[index];
        run.palette.source[index * 3] = color.red >> 2;
        run.palette.source[index * 3 + 1] = color.green >> 2;
        run.palette.source[index * 3 + 2] = color.blue >> 2;
    }
    run.palette = completedFullPaletteUpload(run.palette, {0, 256, 0, 63}); //1A22/0736 uploads and copies both banks.
    tail(); //6288 cleanup/628B JMP, returning637F.
    run.finalPhase = phase;
    std::fill(frame_.pixels.begin(), frame_.pixels.end(), 0);
    frame_.palette = expandVgaDacPalette(std::vector<std::uint8_t>(run.palette.dac.colors.begin(), run.palette.dac.colors.end()));
    audio_->workPhase_ = phase;
    audio_->intro_ = std::move(run);
    phase_ = NativeStartupPhase::awaitingMenuMusic;
}

} // namespace stellar7
