#include "stellar7/native_startup.hpp"

#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int64_t startupWorkHorizon = 120000;
constexpr std::size_t maximumCountdownPolls = 10000;

void requireFont(const BitmapFont& font) {
    if (font.glyphs.empty() || font.height == 0) {
        throw std::invalid_argument("native startup requires both decoded font resources");
    }
}
}

const NativeStartupFontsRun* NativeStartupSequence::recoveredFonts() const noexcept {
    return audio_ && audio_->fonts_ ? &*audio_->fonts_ : nullptr;
}

void NativeStartupSequence::initializeFonts(const StartupFontsProfile profile) {
    if (phase_ != NativeStartupPhase::awaitingFontResources || !audio_ || !audio_->presentation_ || !audio_->workPhase_) {
        throw std::logic_error("font initialization requires retained selected-song startup");
    }
    NativeStartupFontsRun run{};
    run.fonts = startupFontsOperations(profile);
    requireFont(game_.scoreFont);
    requireFont(game_.titleFont);
    run.firstFont = game_.titleFont; //12CA is FNT0; existing asset names describe later use.
    run.secondFont = game_.scoreFont; //12CC is FNT1.
    // Explicit native successful-allocation policy; no DOS allocator duration.
    std::size_t probeBytes = 0;
    for (const auto& segment : run.fonts.segments) {
        if (segment.kind == StartupFontsKind::allocateProbe) { probeBytes = segment.value; }
    }
    std::vector<std::uint8_t> probe(probeBytes);
    auto planned = *audio_->workPhase_;
    for (const auto& segment : run.fonts.segments) {
        planned = normalWorkTiming(planned, segment.operations, startupWorkHorizon).phase;
    }
    run.firstSample = elapsedSamples();
    for (const auto& segment : run.fonts.segments) {
        auto work = audio_->clock_.renderWork(*audio_->workPhase_, segment.operations, startupWorkHorizon);
        audio_->workPhase_ = work.timing.phase;
        run.samples.insert(run.samples.end(), work.audio.samples.begin(), work.audio.samples.end());
        run.work.push_back(std::move(work));
        if (segment.kind == StartupFontsKind::allocateProbe) { run.memoryProbeBytes = segment.value; }
        if (segment.kind == StartupFontsKind::releaseProbe) {
            std::vector<std::uint8_t>{}.swap(probe);
            run.probeReleased = true;
        }
    }
    audio_->fonts_ = std::move(run);
    phase_ = NativeStartupPhase::logoCountdownWork;
}

ClockedPresentationWork NativeStartupSequence::pollLogoCountdown(const std::size_t guestPolls) {
    if (phase_ != NativeStartupPhase::logoCountdownWork || !audio_ || !audio_->workPhase_) {
        throw std::logic_error("logo countdown polling requires the retained source wait");
    }
    if (guestPolls == 0 || guestPolls > maximumCountdownPolls) {
        throw std::invalid_argument("startup countdown packet exceeds guest-poll bound");
    }
    // The operation executes actual five-instruction polls. Arrival of each IRQ
    // and callback is computed from the current phase, never a requested ordinal.
    auto work = audio_->clock_.renderWork(*audio_->workPhase_,
        {{NormalWorkKind::startupCountdownPoll, guestPolls}}, startupWorkHorizon);
    audio_->workPhase_ = work.timing.phase;
    if (work.timing.countdownReturned) { phase_ = NativeStartupPhase::awaitingLogoFrames; }
    return work;
}

ClockedPresentationWork NativeStartupSequence::playLogoEvent(const StartupLogoEventProfile profile) {
    if (phase_ != NativeStartupPhase::awaitingLogoFrames || !audio_ || !audio_->workPhase_
        || !audio_->settings_ || !audio_->resources_) {
        throw std::logic_error("logo event requires the completed retained startup countdown");
    }
    const auto& settings = audio_->settings_->settings.configuration;
    const auto suppressed = profile == StartupLogoEventProfile::playableSoundsDisabled;
    if ((profile != StartupLogoEventProfile::sourceSound && !suppressed)
        || audio_->resources_->resources.state.driverSelector != 4
        || (suppressed ? settings.options[3] != 0
            || (settings.options[2] != 1 && audio_->startupMusicProfile_ != StartupMusicProfile::playableUnpublished)
            : settings.options[3] == 0)) {
        throw std::invalid_argument("unsupported logo event sound option or driver");
    }
    if (!suppressed && audio_->startupMusicProfile_ == StartupMusicProfile::playableUnpublished) {
        auto& phase = *audio_->workPhase_;
        if (!phase.playableStartup || !phase.playableMusicOff || !phase.sequenceSelection
            || !phase.sequenceSelection->countdownReturned || phase.sequenceSelection->logoEventPlayed) {
            throw std::invalid_argument("cold logo event requires its real completed unpublished allocation");
        }
        //Explicit native foreground provider; no new source CPU timing claim.
        phase.callback = audio_->synth_.triggerPlayableEvent(26);
        phase.sequenceSelection->logoEventPlayed = true;
        ClockedPresentationWork work;
        work.audio.firstSample = elapsedSamples();
        work.timing.phase = phase;
        phase_ = NativeStartupPhase::awaitingLogoAnimation;
        return work;
    }
    const auto kind = suppressed ? NormalWorkKind::startupLogoEventSuppressed : NormalWorkKind::startupLogoEvent;
    auto work = audio_->clock_.renderWork(*audio_->workPhase_, {{kind, 1, 26}}, startupWorkHorizon);
    audio_->workPhase_ = work.timing.phase;
    phase_ = NativeStartupPhase::awaitingLogoAnimation;
    return work;
}

}  // namespace stellar7
