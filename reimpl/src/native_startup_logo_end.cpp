#include "stellar7/native_startup.hpp"

#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int64_t soundEndHorizon = 120000;
constexpr std::uint64_t maximumSoundEndPolls = 100000000;

template<class Work>
SoundBlasterClockPhase executeLogoEnd(SoundBlasterClockPhase phase, const std::size_t packetPolls, Work work) {
    if (phase.playableStartup && phase.playableMusicOff) {
        phase = work(phase,std::vector<NormalWorkOperation>{{NormalWorkKind::playableStartupLogoWaitStore,1,480}});
        std::uint64_t polls=0;
        while (!phase.sequenceSelection->countdownReturned) {
            if (polls >= maximumSoundEndPolls) { throw std::invalid_argument("cold logo timer exceeded its poll bound"); }
            phase = work(phase,std::vector<NormalWorkOperation>{{NormalWorkKind::startupCountdownPoll,packetPolls}});
            polls += packetPolls;
        }
        return phase;
    }
    phase = work(phase, std::vector<NormalWorkOperation>{
        {NormalWorkKind::instructions, 7}, {NormalWorkKind::startupLogoOnce, 1, 1}});
    std::uint64_t polls = 0;
    while (!phase.sequenceSelection->logoSoundReturned) {
        if (polls >= maximumSoundEndPolls) {
            throw std::invalid_argument("logo sound wait exceeds admitted source poll bound");
        }
        phase = work(phase, std::vector<NormalWorkOperation>{{NormalWorkKind::startupLogoEndPoll, packetPolls}});
        polls += packetPolls;
    }
    return phase;
}
}

const NativeStartupLogoEndRun* NativeStartupSequence::recoveredLogoEnd() const noexcept {
    return audio_ && audio_->logoEnd_ ? &*audio_->logoEnd_ : nullptr;
}

void NativeStartupSequence::finishLogoSound(const std::size_t pollsPerPacket, const NativeDeliverySink& sink) {
    if (phase_ != NativeStartupPhase::awaitingLogoSoundEnd || !audio_ || !audio_->workPhase_) {
        throw std::logic_error("logo sound-end requires the complete retained animation");
    }
    if (pollsPerPacket == 0 || pollsPerPacket > 10000) {
        throw std::invalid_argument("logo sound-end packet exceeds source poll bound");
    }
    // B96 is the immutable loaded-image1 for the admitted native sound4 profile.
    executeLogoEnd(*audio_->workPhase_, pollsPerPacket, [](const auto& phase, const auto& operations) {
        return normalWorkTiming(phase, operations, soundEndHorizon).phase;
    });
    NativeStartupLogoEndRun run{};
    run.firstSample = elapsedSamples();
    run.finalPhase = executeLogoEnd(*audio_->workPhase_, pollsPerPacket,
        [&](const auto& phase, const auto& operations) {
            auto work = audio_->clock_.renderWork(phase, operations, soundEndHorizon);
            run.samples.insert(run.samples.end(), work.audio.samples.begin(), work.audio.samples.end());
            run.guestPolls += work.timing.sequenceEndPolls + work.timing.countdownPolls;
            if (sink) {
                NativeSessionDelivery delivery;
                delivery.firstSample = work.audio.firstSample;
                delivery.samples = work.audio.samples;
                sink(delivery);
            }
            auto next = work.timing.phase;
            run.work.push_back(std::move(work));
            return next;
        });
    audio_->workPhase_ = run.finalPhase;
    audio_->logoEnd_ = std::move(run);
    phase_ = NativeStartupPhase::awaitingLogoRelease;
}

const NativeStartupLogoReleaseRun* NativeStartupSequence::recoveredLogoRelease() const noexcept {
    return audio_ && audio_->logoRelease_ ? &*audio_->logoRelease_ : nullptr;
}

void NativeStartupSequence::releaseLogo() {
    if (phase_ != NativeStartupPhase::awaitingLogoRelease || !audio_ || !audio_->workPhase_) {
        throw std::logic_error("logo release requires the retained completed sound wait");
    }
    const auto cold = audio_->startupMusicProfile_ == StartupMusicProfile::playableUnpublished;
    const std::vector<NormalWorkOperation> stop = cold
        ? std::vector<NormalWorkOperation>{{NormalWorkKind::instructions,1},
            {NormalWorkKind::playableStartupLogoEmptyReturn,1}}
        : std::vector<NormalWorkOperation>{{NormalWorkKind::startupLogoStop,1}};
    // Whole54DC uses the existing untimed native metadata provider. Historical
    // bitmap/cache snapshots remain immutable; release does not alter physical VRAM.
    const std::vector<NormalWorkOperation> release{{NormalWorkKind::instructions, 2},
        {NormalWorkKind::instructions, 1}}; //6262 PUSH/CALL54DC,626A cleanup.
    auto checked = normalWorkTiming(*audio_->workPhase_, stop, soundEndHorizon).phase;
    normalWorkTiming(checked, release, soundEndHorizon);
    NativeStartupLogoReleaseRun run{};
    run.firstSample = elapsedSamples();
    for (const auto& operations : {stop, release}) {
        auto work = audio_->clock_.renderWork(*audio_->workPhase_, operations, soundEndHorizon);
        run.samples.insert(run.samples.end(), work.audio.samples.begin(), work.audio.samples.end());
        audio_->workPhase_ = work.timing.phase;
        run.work.push_back(std::move(work));
    }
    run.metadataReleased = true;
    run.finalPhase = *audio_->workPhase_;
    audio_->logoRelease_ = std::move(run);
    phase_ = NativeStartupPhase::awaitingIntroInput;
}

}  // namespace stellar7
