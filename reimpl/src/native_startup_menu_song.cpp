#include "stellar7/native_startup.hpp"
#include "playable_startup_provider.hpp"

#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int64_t menuSongHorizon = 120000;
constexpr std::uint16_t menuSongOrdinal = 1;

template<class Work, class Prepare>
SoundBlasterClockPhase executeMenuSong(SoundBlasterClockPhase phase, const DriverSequenceWork& activation,
    const SoundBlasterCallbackState& prepared, Work work, Prepare prepare) {
    const auto run = [&](const std::vector<NormalWorkOperation>& operations) { phase = work(phase, operations); };
    const bool native = phase.playableStartup;
    run({{native ? NormalWorkKind::playableStartupMenuCountdown : NormalWorkKind::startupMenuCountdown, 1, 2000},
        {NormalWorkKind::instructions, 4}}); //637F..638F; native0086 loads the next SNG chunk.
    //6394 cleanup/store and CALL51A6; empty current takes four instructions.
    //639F..63A6 calls2E6, whose index0/loaded-driver prefix has19 instructions.
    run({{NormalWorkKind::instructions, 3 + 4 + 4 + 19 + 11 + 12}});
    run(activation.operations);
    run({{NormalWorkKind::instructions, 9 + 5}}); //Driver/wrapper return.
    prepare();
    phase.callback = native ? playableStartupUnpublishedCallback(prepared)
        : applyDriverSequenceCallbackState(*phase.callback, activation);
    phase.sequenceSelection = AudioClockSequenceSelectionPhase{prepared};
    phase.sequenceSelection->songOrdinal = menuSongOrdinal;
    phase.sequenceSelection->menuCountdownWritten = true;
    if (native) {
        run({{NormalWorkKind::instructions,3 + 5 + 5 + 6 + 6 + 6}});
        return phase;
    }
    run({{NormalWorkKind::instructions, 3}, //031B..0323; publication is its actual source store.
        {NormalWorkKind::soundBlasterSequencePublish, 1, menuSongOrdinal},
        {NormalWorkKind::instructions, 5 + 5 + 6 + 6}}); //Return/caller/flag0 epilogue/CALL062A(BMP:,4).
    // Whole062A cursor-reset provider is native/untimed; caller cleanup and RET remain source work.
    run({{NormalWorkKind::instructions, 6}}); //6489..6491 returns40A5.
    return phase;
}
}

const NativeStartupMenuSongRun* NativeStartupSequence::recoveredMenuSong() const noexcept {
    return audio_ && audio_->menuSong_ ? &*audio_->menuSong_ : nullptr;
}

void NativeStartupSequence::beginMenuSong() {
    if (phase_ != NativeStartupPhase::awaitingMenuMusic || !audio_ || !audio_->workPhase_
        || !audio_->resources_ || !audio_->intro_) {
        throw std::logic_error("menu song requires the retained accepted intro skip");
    }
    if (audio_->intro_->mode!=NativeIntroPresentation::earlyEscape) {
        if (!hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::storyPanels)
            || audio_->synth_.sampledVoiceObservation().phase!=SampledVoicePhase::unloaded) {
            throw std::invalid_argument("native menu song requires its returned Story owner");
        }
        NativeStartupMenuSongRun run{};run.audioProfile=audio_->startupMusicProfile_;
        run.song={game_.presentationMusicSequence,std::nullopt,0xffff};
        run.activation=soundBlasterSequenceOperations(run.song.payload,DriverSequenceAdmission::inheritedChannels);
        run.firstSample=elapsedSamples();run.before=audio_->synth_.driverState();
        auto phase=*audio_->workPhase_;phase.playableCampaign=true;
        auto count=audio_->clock_.renderWork(phase,{{NormalWorkKind::playableStoryWaitStore,1,2000}},menuSongHorizon);
        phase=count.timing.phase;run.samples=std::move(count.audio.samples);run.work.push_back(std::move(count));
        const auto prepared=audio_->synth_.preparePlayablePresentationAfterStory(audio_->playableMusicPublication());
        phase.callback=audio_->synth_.callbackWorkState();phase.playableStartup=true;
        phase.playableMusicOff=!audio_->playableMusicEnabled();phase.playableCampaign=false;
        phase.sequenceSelection=AudioClockSequenceSelectionPhase{prepared};
        phase.sequenceSelection->songOrdinal=menuSongOrdinal;phase.sequenceSelection->menuCountdownWritten=true;
        run.finalPhase=phase;run.after=audio_->synth_.driverState();
        audio_->workPhase_=phase;audio_->menuSong_=std::move(run);
        phase_=NativeStartupPhase::awaitingMenuResources;return;
    }
    if (audio_->resources_->resources.state.openedStartupResource) {
        throw std::invalid_argument("newly opened startup resource epilogue is not admitted");
    }
    NativeStartupMenuSongRun run{};
    run.audioProfile = audio_->startupMusicProfile_;
    run.song = {game_.presentationMusicSequence, std::nullopt, 0xffff}; //Independent source0086 allocation.
    run.activation = soundBlasterSequenceOperations(run.song.payload, DriverSequenceAdmission::inheritedChannels);
    const auto prepared = audio_->synth_.previewRecoveredPresentation(audio_->playableMusicPublication());
    executeMenuSong(*audio_->workPhase_, run.activation, prepared, [](const auto& phase, const auto& operations) {
        return normalWorkTiming(phase, operations, menuSongHorizon).phase;
    }, []() {});
    run.firstSample = elapsedSamples();
    run.before = audio_->synth_.driverState();
    run.finalPhase = executeMenuSong(*audio_->workPhase_, run.activation, prepared,
        [&](const auto& phase, const auto& operations) {
            auto work = audio_->clock_.renderWork(phase, operations, menuSongHorizon);
            run.samples.insert(run.samples.end(), work.audio.samples.begin(), work.audio.samples.end());
            auto next = work.timing.phase;
            run.work.push_back(std::move(work));
            return next;
        }, [&]() { audio_->synth_.prepareRecoveredPresentation(audio_->playableMusicPublication()); });
    run.after = audio_->synth_.driverState();
    audio_->workPhase_ = run.finalPhase;
    audio_->menuSong_ = std::move(run);
    phase_ = NativeStartupPhase::awaitingMenuResources;
}

} // namespace stellar7
