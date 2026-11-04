#include "stellar7/recovered_frontend.hpp"
#include <stdexcept>

namespace stellar7 {
NativeSessionDelivery RecoveredFrontend::paceGameplay(const std::uint64_t minimumSampleEnd) {
    if(execution_!=CampaignExecution::playable||!session_) {
        throw std::invalid_argument("Native speed control requires an active playable session.");
    }
    return session_->pacePlayable(minimumSampleEnd);
}

NativeSessionDelivery NativeGameplaySession::pacePlayable(const std::uint64_t minimumSampleEnd) {
    const auto current=model_.phase();
    const bool ordinary=current==GameplayPhase::level1||current==GameplayPhase::level2
        ||current==GameplayPhase::level3Native||current==GameplayPhase::playable||current==GameplayPhase::level5;
    if(!ordinary||livePreferences_||story_||joystickCalibrationActive()||!audio_||!audio_->workPhase_) {
        throw std::invalid_argument("Native speed control cannot advance a modal or lifecycle boundary.");
    }
    NativeSessionDelivery delivery;delivery.firstSample=elapsedSamples();
    if(minimumSampleEnd<=delivery.firstSample) {return delivery;}
    constexpr std::uint64_t maximumPacingSamples=SoundBlasterSynth::sampleRate/10;
    if(minimumSampleEnd-delivery.firstSample>maximumPacingSamples) {
        throw std::invalid_argument("Native speed deadline exceeds one bounded frame wait.");
    }
    constexpr std::uint64_t idleDispatchBatch=256;
    constexpr std::int64_t workHorizon=120000;
    auto phase=*audio_->workPhase_;
    while(elapsedSamples()<minimumSampleEnd) {
        // Native idle CPU work advances the existing devices and synthesizer.
        // It is neither a fitted original frame cost nor inserted host silence.
        auto work=clock_.renderWork(phase,{{NormalWorkKind::instructions,idleDispatchBatch}},workHorizon);
        if(work.audio.firstSample!=delivery.firstSample+delivery.samples.size()) {
            throw std::logic_error("Native speed wait lost its retained audio epoch.");
        }
        phase=std::move(work.timing.phase);
        delivery.samples.insert(delivery.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
    }
    *audio_->workPhase_=std::move(phase);
    return delivery;
}
} // namespace stellar7
