#include "stellar7/level2_music_work.hpp"
#include "stellar7/audio_repeat_work.hpp"
#include "stellar7/audio.hpp"
#include "stellar7/normal_clock.hpp"
#include "stellar7/audio_direct_work.hpp"
#include "stellar7/audio_program_work.hpp"
#include "retained_menu_clock_domain.hpp"
#include "playable_startup_clock_domain.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace stellar7 {
namespace {

constexpr std::size_t canonicalPresentationCallbacks = 730;
constexpr std::uint16_t logoDelayIrqs = 240;
constexpr std::uint16_t logoFrameIrqs = 10;
constexpr std::uint16_t beginWaitIrqs = 480;
constexpr std::uint16_t presentationWaitIrqs = 2000;
constexpr std::uint16_t menuIdleIrqs = 30000;
constexpr std::uint16_t musicVolume = 15;
constexpr std::uint16_t fadeDividerReload = 20;
constexpr std::uint16_t volumeTableAddress = 0x0903;
constexpr std::uint16_t instrumentBankAddress = 0x0d28;
constexpr std::size_t recoveredLogoSlot = 0;
constexpr std::size_t recoveredPresentationSlot = 1;
constexpr std::size_t recoveredLevel1Slot = 2;
constexpr std::size_t recoveredSuccessSlot = 3;
constexpr std::size_t sequenceBodyOffset = 36;
constexpr std::size_t volumeTableSize = 16 * 64;
constexpr int logoEvent = 26;
constexpr std::uint64_t normalCyclesPerSecond = 5'000'000;
constexpr std::uint8_t channelEnableController = 0x4e;
constexpr std::uint8_t channelDeferralController = 0x40;
constexpr std::uint8_t deferralThreshold = 64;
constexpr std::size_t firstDirectVoice = 6;
constexpr std::uint8_t firstDirectChannel = 13;
constexpr std::array<std::uint8_t, 3> retainedSequenceChannels{6, 7, 15};
static_assert(SampledVoiceStream::outputRate == SoundBlasterSynth::sampleRate);

std::uint64_t clockedSamplesAt(const std::uint64_t cycles) {
    if (cycles > std::numeric_limits<std::uint64_t>::max() / SoundBlasterSynth::sampleRate) {
        throw std::invalid_argument("normal work PCM timeline exceeds sample conversion range");
    }
    const auto scaled = cycles * SoundBlasterSynth::sampleRate;
    return scaled / normalCyclesPerSecond + (scaled % normalCyclesPerSecond != 0);
}

bool sameCallbackState(const SoundBlasterCallbackState& first, const SoundBlasterCallbackState& second) {
    const auto sameProgram = [](const auto& left, const auto& right) {
        if (left.has_value() != right.has_value()) { return false; }
        if (!left) { return true; }
        if (left->instrumentBank != right->instrumentBank || left->volumeTable != right->volumeTable
            || left->playerHitVolumeRow != right->playerHitVolumeRow
            || left->enemy20CallerWindow != right->enemy20CallerWindow
            || left->enemy20McbByte != right->enemy20McbByte
            || left->bomb7CallerRow != right->bomb7CallerRow
            || left->channelPrograms != right->channelPrograms || left->operators != right->operators
            || left->handleStatus != right->handleStatus || left->repeatCount != right->repeatCount
            || left->handleVolume != right->handleVolume || left->waveformMask != right->waveformMask
            || left->accumulator != right->accumulator || left->fadeDivider != right->fadeDivider
            || left->amplitudeDepth != right->amplitudeDepth || left->vibratoDepth != right->vibratoDepth
            || left->noteSelect != right->noteSelect || left->loopMarker != right->loopMarker) { return false; }
        for (std::size_t voice = 0; voice < left->voices.size(); ++voice) {
            const auto& a = left->voices[voice];
            const auto& b = right->voices[voice];
            if (a.program != b.program || a.tuningMagnitude != b.tuningMagnitude
                || a.operatorScale != b.operatorScale || a.operatorFallback != b.operatorFallback
                || a.carrierScale != b.carrierScale || a.carrierFallback != b.carrierFallback
                || a.operatorDisabled != b.operatorDisabled) { return false; }
        }
        return true;
    };
    return first.voices.channels == second.voices.channels && first.voices.notes == second.voices.notes
        && first.voices.tuning == second.voices.tuning && first.voices.channelDeferrals == second.voices.channelDeferrals
        && first.voices.directTimers == second.voices.directTimers && first.velocities == second.velocities
        && first.sounding == second.sounding && first.channelEnabled == second.channelEnabled
        && first.channelVoiceCursors == second.channelVoiceCursors && first.sequence == second.sequence
        && first.position == second.position && first.wait == second.wait && first.fade == second.fade
        && first.mode == second.mode && first.operation == second.operation && first.runningStatus == second.runningStatus
        && first.active == second.active && first.gate == second.gate && first.latch == second.latch
        && first.domain == second.domain && first.driverEnabled == second.driverEnabled
        && sameProgram(first.program, second.program);
}

constexpr std::array<std::uint8_t, 18> operatorOffsets{
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
};
constexpr std::array<std::uint8_t, 18> voiceOperators{
    0, 3, 1, 4, 2, 5, 6, 9, 7, 10, 8, 11, 12, 15, 13, 16, 14, 17,
};
constexpr std::array<std::array<std::uint8_t, 8>, 28> level1EventRecords{{
    {{0x31, 0x00, 0x78, 0x00, 0x51, 0x00, 0x03, 0x00}},
    {{0x1d, 0x00, 0x50, 0x00, 0x19, 0x01, 0x05, 0x00}},
    {{0x34, 0x00, 0x3c, 0x00, 0x1a, 0x02, 0x0d, 0x00}},
    {{0x1f, 0x00, 0x3c, 0x00, 0x1b, 0x03, 0x07, 0x00}},
    {{0x20, 0x00, 0x3c, 0x00, 0x1c, 0x04, 0x0b, 0x00}},
    {{0x21, 0x00, 0x3c, 0x00, 0x1d, 0x05, 0x0b, 0x00}},
    {{0x2c, 0x00, 0x50, 0x00, 0x1e, 0x06, 0x0c, 0x00}},
    {{0x22, 0x00, 0x3c, 0x00, 0x1f, 0x07, 0x08, 0x00}},
    {{0x23, 0x18, 0x0f, 0x00, 0x20, 0x08, 0x08, 0x00}},
    {{0x24, 0x27, 0x1e, 0x00, 0x21, 0x09, 0x09, 0x00}},
    {{0x24, 0x1a, 0x1e, 0x00, 0x22, 0x0a, 0x09, 0x00}},
    {{0x26, 0x00, 0x78, 0x00, 0x34, 0x0b, 0xff, 0xff}},
    {{0x27, 0x00, 0x78, 0x00, 0x35, 0x0c, 0x04, 0x00}},
    {{0x28, 0x00, 0x78, 0x00, 0x37, 0x0d, 0xff, 0xff}},
    {{0x36, 0x00, 0x0c, 0x00, 0x39, 0x0e, 0x02, 0x00}},
    {{0x2a, 0x00, 0x28, 0x00, 0x3a, 0x0f, 0x01, 0x00}},
    {{0x2e, 0x24, 0x0f, 0x00, 0x3b, 0x10, 0x00, 0x00}},
    {{0x28, 0x00, 0x68, 0x01, 0x49, 0x11, 0xff, 0xff}},
    {{0x2c, 0x01, 0x50, 0x00, 0x4a, 0x12, 0x0c, 0x00}},
    {{0x21, 0x00, 0x3c, 0x00, 0x4d, 0x13, 0x0c, 0x00}},
    {{0x2b, 0x00, 0x2c, 0x01, 0x4e, 0x14, 0xff, 0xff}},
    {{0x2f, 0x00, 0x3c, 0x00, 0x4f, 0x15, 0x0c, 0x00}},
    {{0x30, 0x01, 0x3c, 0x00, 0x50, 0x16, 0xff, 0xff}},
    {{0x00, 0x00, 0x00, 0x00, 0x18, 0x17, 0xff, 0xff}},
    {{0x32, 0x18, 0x50, 0x00, 0x52, 0x18, 0x06, 0x00}},
    {{0x35, 0x00, 0x1e, 0x00, 0x53, 0x19, 0xff, 0xff}},
    {{0x02, 0x50, 0x1e, 0x00, 0x46, 0x1a, 0xff, 0xff}},
    {{0x13, 0x0c, 0x1e, 0x00, 0x23, 0x1b, 0xff, 0xff}},
}};

std::vector<std::uint8_t> decodeInstrumentBank(
    const std::vector<std::uint8_t>& resource
) {
    constexpr std::size_t headerSize = 19;
    constexpr std::size_t bankHalfSize = 0x540;
    if (
        resource.size() != headerSize + bankHalfSize * 2 + 2
        || resource[0] != 0x89
        || resource[1] != 0x11
        || resource[headerSize + bankHalfSize] != 0xab
        || resource[headerSize + bankHalfSize + 1] != 0xcd
    ) {
        return {};
    }
    std::vector<std::uint8_t> bank;
    bank.reserve(bankHalfSize * 2);
    bank.insert(
        bank.end(),
        resource.begin() + headerSize,
        resource.begin() + headerSize + bankHalfSize
    );
    bank.insert(
        bank.end(),
        resource.begin() + headerSize + bankHalfSize + 2,
        resource.end()
    );
    return bank;
}

}  // namespace

std::vector<std::uint8_t> decodeSoundBlasterVolumeTable(const std::vector<std::uint8_t>& driver) {
    constexpr std::array<std::uint8_t, 8> signature{'b', 'l', 'a', 's', 't', 'd', 'r', 'v'};
    constexpr std::size_t signatureOffset = 10;
    if (driver.size() < volumeTableAddress + volumeTableSize
        || !std::equal(signature.begin(), signature.end(), driver.begin() + signatureOffset)) {
        throw std::invalid_argument("unsupported Sound Blaster driver level table");
    }
    return {driver.begin() + volumeTableAddress, driver.begin() + volumeTableAddress + volumeTableSize};
}

class SoundBlasterSynth::Impl {
public:
    Impl(
        std::vector<std::vector<std::uint8_t>> sequences,
        std::vector<std::uint8_t> instrumentResource,
        std::vector<std::uint8_t> musicSequence,
        std::vector<std::uint8_t> presentationSequence,
        std::vector<std::uint8_t> level2MusicSequence,
        std::vector<std::uint8_t> level3MusicSequence,
        std::vector<std::uint8_t> logoSequence,
        std::vector<std::uint8_t> volumeTable,
        const SoundBlasterInitialization initialization
    )
        : sequences_(std::move(sequences)),
          instrumentBank_(decodeInstrumentBank(instrumentResource)),
          musicSequence_(std::move(musicSequence)),
          presentationSequence_(std::move(presentationSequence)),
          level2MusicSequence_(std::move(level2MusicSequence)),
          level3MusicSequence_(std::move(level3MusicSequence)),
          logoSequence_(std::move(logoSequence)), volumeTable_(std::move(volumeTable)) {
        if (!volumeTable_.empty() && volumeTable_.size() != volumeTableSize) {
            throw std::invalid_argument("unsupported Sound Blaster volume table dimensions");
        }
        if (initialization != SoundBlasterInitialization::compatibility
            && initialization != SoundBlasterInitialization::recoveredWork) {
            throw std::invalid_argument("unsupported Sound Blaster initialization policy");
        }
        recoveredInitialization_ = initialization == SoundBlasterInitialization::recoveredWork;
        driverEnabled_ = !recoveredInitialization_;
        if (driverEnabled_) {
            voiceOwners_.fill(0xff);
            // Compatibility206b state; recovered startup reaches this writer later.
            for (std::size_t slot = 0; slot < directDurations_.size(); ++slot) {
                voiceOwners_[firstDirectVoice + slot] = static_cast<std::uint8_t>(firstDirectChannel + slot);
            }
            resetOpl();
        } else {
            // Immutable loaded SBL tables precede op2 and the later206b reset.
            voiceOwners_.fill(0);
            for (auto& voice : voices_) {
                voice.program = 0;
            }
        }
        channelVoiceCursor_.fill(0);
    }

    void beginLogo() {
        if (startupPhase_ != SoundBlasterStartupPhase::idle || logoSequence_.size() < 36) {
            throw std::logic_error("logo startup requires an idle driver and decoded logo sequence");
        }
        startSequenceData(0, &logoSequence_, Playback::repeat);
        startupCountdown_ = logoDelayIrqs;
        startupPhase_ = SoundBlasterStartupPhase::logoDelay;
    }

    void advanceStartupIrq() {
        // Original IRQ0 saturates DS1dc1 at zero; audio runs every fourth IRQ.
        if (startupCountdown_ != 0) {
            --startupCountdown_;
        }
    }

    void triggerLogoEvent() {
        if (startupPhase_ != SoundBlasterStartupPhase::logoDelay || startupCountdown_ != 0) {
            throw std::logic_error("logo event requires the startup IRQ countdown to expire");
        }
        triggerLevel1Event(logoEvent);
        startupPhase_ = SoundBlasterStartupPhase::logoPlaying;
    }

    void beginLogoFrame() {
        if (startupPhase_ != SoundBlasterStartupPhase::logoPlaying || startupCountdown_ != 0) {
            throw std::logic_error("logo frame requires the preceding countdown to expire");
        }
        startupCountdown_ = logoFrameIrqs;
    }

    void finishLogo(const LogoCompletion completion) {
        if (startupPhase_ != SoundBlasterStartupPhase::logoPlaying) {
            throw std::logic_error("logo completion requires the animation event phase");
        }
        logoCompletion_ = completion;
        if (completion == LogoCompletion::sequenceEnd) {
            sequencers_[0].playback = Playback::once;
        } else {
            startupCountdown_ = beginWaitIrqs;
        }
        startupPhase_ = SoundBlasterStartupPhase::logoEnding;
    }

    bool logoFinished() const noexcept {
        return startupPhase_ == SoundBlasterStartupPhase::logoEnding
            && (logoCompletion_ == LogoCompletion::sequenceEnd ? !sequencers_[0].active : startupCountdown_ == 0);
    }

    void stopLogo() {
        if (!logoFinished()) {
            throw std::logic_error("logo stop requires its selected completion boundary");
        }
        stopMusic();
        startupPhase_ = SoundBlasterStartupPhase::logoStopped;
    }

    void validateGameplayRequest() const {
        const auto base = !musicSequence_.empty() && musicSequence_[0] == 0x84 ? 2U : 0U;
        if (workClockActive_ || startupPhase_ != SoundBlasterStartupPhase::presentation
            || !sequencers_[0].allocated || volumeTable_.size() != volumeTableSize
            || instrumentBank_.size() != 96 * 28
            || musicSequence_.size() < 36 || musicSequence_[base] != 0
            || sampledVoice_.observation().phase != SampledVoicePhase::unloaded) {
            throw std::invalid_argument("Begin requires retained native presentation and decoded Level1 MIDI");
        }
    }

    void requestGameplay() {
        if (startupPhase_ != SoundBlasterStartupPhase::presentation) {
            throw std::logic_error("Begin requires the live presentation owner");
        }
        fadePresentation(); //0152 repeats165e's request without resetting the divider.
        startupCountdown_ = beginWaitIrqs;
        startupPhase_ = SoundBlasterStartupPhase::gameplayWaiting;
    }

    void fadePresentation() {
        if (startupPhase_ != SoundBlasterStartupPhase::presentation || !sequencers_[0].allocated
            || volumeTable_.size() != volumeTableSize) {
            throw std::logic_error("presentation fade requires its live owner and decoded level table");
        }
        musicFade_ = musicVolume; //2d60 changes AC only; AA retains its phase.
    }

    void beginPresentation() {
        if ((startupPhase_ != SoundBlasterStartupPhase::logoStopped && !logoFinished())
            || presentationSequence_.size() < 36) {
            throw std::logic_error("presentation requires completed logo startup and decoded sequence");
        }
        startupCountdown_ = presentationWaitIrqs; //637f writes this before starting SNG1.
        startSequenceData(0, &presentationSequence_, Playback::repeat);
        startupPhase_ = SoundBlasterStartupPhase::presentation;
    }

    void beginMenu() {
        if (startupPhase_ != SoundBlasterStartupPhase::presentation) {
            throw std::logic_error("menu requires the live presentation owner");
        }
        startupCountdown_ = menuIdleIrqs; //1804 leaves the existing SNG1 sequence live.
    }

    void validateMenuAudioReturn() const {
        const auto base = !presentationSequence_.empty() && presentationSequence_[0] == 0x84 ? 2U : 0U;
        if (workClockActive_ || startupPhase_ != SoundBlasterStartupPhase::gameplay
            || presentationSequence_.size() < 36 || presentationSequence_[base] != 0
            || sampledVoice_.observation().phase != SampledVoicePhase::unloaded
            || std::any_of(sequencers_.begin(), sequencers_.end(),
                [](const auto& sequence) { return sequence.allocated; })) {
            throw std::invalid_argument("menu audio return requires released native gameplay audio and retained type0 SNG1");
        }
    }

    void restartMenuAudio() {
        validateMenuAudioReturn();
        resetDirectEffects(); //04BF/02E6/op6:206B precedes1821, without another51A6 sweep.
        startSequenceData(0, &presentationSequence_, Playback::repeat);
        startupPhase_ = SoundBlasterStartupPhase::presentation;
    }

    void beginGameplay() {
        if ((startupPhase_ != SoundBlasterStartupPhase::presentation
            && startupPhase_ != SoundBlasterStartupPhase::gameplayWaiting)
            || (startupPhase_ == SoundBlasterStartupPhase::gameplayWaiting && startupCountdown_ != 0)
            || musicSequence_.size() < 36) {
            throw std::logic_error("gameplay requires presentation startup and decoded music");
        }
        if (startupPhase_ == SoundBlasterStartupPhase::gameplayWaiting) {
            stopMusic(); //017b; legacy direct-presentation API retains its old behavior.
        }
        startMusic();
        startupPhase_ = SoundBlasterStartupPhase::gameplay;
    }

    SoundBlasterStartupPhase startupPhase() const noexcept { return startupPhase_; }
    std::uint16_t startupCountdown() const noexcept { return startupCountdown_; }
    bool logoActive() const noexcept {
        return (startupPhase_ == SoundBlasterStartupPhase::logoDelay
            || startupPhase_ == SoundBlasterStartupPhase::logoPlaying
            || startupPhase_ == SoundBlasterStartupPhase::logoEnding)
            && (sequencers_[0].active || sequencers_[0].playback == Playback::repeat);
    }

    //SBL206b: bookkeeping only. Keep direct timers, fallback levels and OPL live.
    void resetDirectEffects() {
        requireInitialized();
        if (workClockActive_) {
            throw std::invalid_argument("direct-effect reset requires recovered work on the strict clock");
        }
        applyDirectReset();
    }

    void completeRecoveredDirectReset() {
        validateRecoveredPreparation();
        applyDirectReset();
    }

    SoundBlasterCallbackState prepareRecoveredLogo(const SoundBlasterProgramState& program, const PlayableMusicPublication publication) {
        if (publication != PlayableMusicPublication::current && publication != PlayableMusicPublication::suspended) {
            throw std::invalid_argument("unsupported recovered music publication");
        }
        validateRecoveredPreparation();
        const auto base = logoSequence_.size() > 2 && logoSequence_[0] == 0x84 ? 2U : 0U;
        if (logoSequence_.size() < base + 35 || logoSequence_[base] != 0) {
            throw std::invalid_argument("recovered logo requires complete type0 sequence data");
        }
        if (program.instrumentBank != instrumentBank_ || program.volumeTable != volumeTable_
            || program.handleStatus != 0 || program.repeatCount != 0xffff || program.handleVolume != musicVolume) {
            throw std::invalid_argument("recovered logo program state differs from owned source resources");
        }
        applyDirectReset();
        startSequenceData(0, &logoSequence_, Playback::repeat);
        sequencers_[0].selected = false;
        programState_ = program;
        if (publication == PlayableMusicPublication::suspended) {playableSequenceOwned_ = true;}
        return buildCallbackWorkState(SoundBlasterCallbackDomain::sequence);
    }

    SoundBlasterCallbackState preparePlayablePresentationAfterStory(const PlayableMusicPublication publication) {
        if (!recoveredInitialization_ || !workClockActive_ || !driverEnabled_ || !programState_
            || workSequenceSlot_!=recoveredLogoSlot || sequencers_[recoveredPresentationSlot].allocated
            || sequencers_[recoveredLevel1Slot].allocated
            || sampledVoice_.observation().phase!=SampledVoicePhase::unloaded
            || (publication!=PlayableMusicPublication::current && publication!=PlayableMusicPublication::suspended)) {
            throw std::invalid_argument("native Story menu requires released cold presentation owners");
        }
        for (std::size_t index=0;index<sequencers_.size();++index) {
            const auto& slot=sequencers_[index];
            const bool frozenLogo=playableSequenceOwned_ && index==recoveredLogoSlot
                && !slot.selected && slot.data==&logoSequence_;
            if (slot.selected || (slot.active && !frozenLogo)) {
                throw std::invalid_argument("native Story menu cannot replace live sequence work");
            }
        }
        auto incoming=callbackWorkState();
        const auto work=soundBlasterSequenceOperations(presentationSequence_,DriverSequenceAdmission::inheritedChannels);
        //17E5 writes the new descriptor and its specified physical fields;
        //Story's still-live direct timers are not part of those reset stores.
        auto selected=applyDriverSequenceCallbackState(incoming,work);
        selected.domain=SoundBlasterCallbackDomain::sequence;selected.sequence=presentationSequence_;
        selected.position=sequenceBodyOffset;selected.wait=0;selected.mode=0;selected.operation=2;
        selected.active=true;selected.fade=0;
        selected.program=soundBlasterProgramSequenceState(*programState_,work.state);
        selected.runningStatus=recoveredParserStatus_;
        workSequenceSlot_=recoveredPresentationSlot;
        auto& menu=sequencers_[workSequenceSlot_];
        menu.data=&presentationSequence_;menu.allocated=true;
        playableSequenceOwned_=true;applyProgramState(selected);
        menu.selected=publication==PlayableMusicPublication::current;
        startupPhase_=SoundBlasterStartupPhase::presentation;
        return buildCallbackWorkState(SoundBlasterCallbackDomain::sequence);
    }

    SoundBlasterCallbackState previewRecoveredPresentation(const PlayableMusicPublication publication) const {
        if (publication != PlayableMusicPublication::current && publication != PlayableMusicPublication::suspended) {
            throw std::invalid_argument("unsupported recovered music publication");
        }
        const bool suspended = publication == PlayableMusicPublication::suspended && playableSequenceOwned_
            && programState_ && programState_->handleStatus == 0 && programState_->repeatCount == 0xffff
            && sequencers_[0].active;
        if (!recoveredInitialization_ || !workClockActive_ || !driverEnabled_ || workSequenceSlot_ != 0
            || !programState_ || (!suspended && (programState_->handleStatus != 0xffff || programState_->repeatCount != 1))
            || !sequencers_[0].allocated || (!suspended && sequencers_[0].active) || sequencers_[0].selected
            || sequencers_[0].data != &logoSequence_ || sequencers_[1].allocated) {
            throw std::invalid_argument("presentation activation requires the retained deselected logo allocation");
        }
        const auto incoming = callbackWorkState();
        soundBlasterCallbackAdvance(incoming);
        const auto work = soundBlasterSequenceOperations(presentationSequence_, DriverSequenceAdmission::inheritedChannels);
        auto builderInput=incoming;
        if (playableSequenceOwned_ && builderInput.domain==SoundBlasterCallbackDomain::level2RetryNoHandle) {
            //17E5 initializes a new descriptor from the physical driver fields.
            builderInput.domain=SoundBlasterCallbackDomain::noHandle;
            builderInput.program.reset();
        }
        auto selected = soundBlasterSequenceCallbackState(builderInput, presentationSequence_, work);
        selected.program = soundBlasterProgramSequenceState(*programState_, work.state);
        selected.runningStatus = recoveredParserStatus_; //17E5 does not overwrite driver A2/A3.
        return selected;
    }

    SoundBlasterCallbackState prepareRecoveredPresentation(const PlayableMusicPublication publication) {
        if (publication != PlayableMusicPublication::current && publication != PlayableMusicPublication::suspended) {
            throw std::invalid_argument("unsupported recovered music publication");
        }
        const auto selected = previewRecoveredPresentation(publication);
        workSequenceSlot_ = 1;
        auto& sequence = sequencers_[workSequenceSlot_];
        sequence.data = &presentationSequence_;
        sequence.allocated = true;
        applyProgramState(selected);
        sequence.selected = false; //0325 remains a separate source store.
        if (publication == PlayableMusicPublication::suspended) {
            playableSequenceOwned_ = true;
            startupPhase_ = SoundBlasterStartupPhase::presentation;
        }
        return buildCallbackWorkState(SoundBlasterCallbackDomain::sequence);
    }

    SoundBlasterCallbackState previewRecoveredLevel1(const PlayableMusicPublication publication) const {
        if (publication != PlayableMusicPublication::current && publication != PlayableMusicPublication::suspended) {
            throw std::invalid_argument("unsupported recovered music publication");
        }
        const auto& menu = sequencers_[recoveredPresentationSlot];
        const bool suspended = publication == PlayableMusicPublication::suspended && playableSequenceOwned_
            && programState_ && (programState_->handleStatus == 0 || programState_->handleStatus == 0xffff);
        if (!recoveredInitialization_ || !workClockActive_ || !driverEnabled_
            || workSequenceSlot_ != recoveredPresentationSlot
            || startupPhase_ != SoundBlasterStartupPhase::presentation
            || !programState_ || (!suspended && programState_->handleStatus != 0xffff)
            || programState_->repeatCount != 0xffff || !menu.allocated || (!suspended && menu.active) || menu.selected
            || menu.data != &presentationSequence_ || sequencers_[recoveredLevel1Slot].allocated) {
            throw std::invalid_argument("Level1 activation requires the retained stopped menu allocation");
        }
        const auto incoming = callbackWorkState();
        soundBlasterCallbackAdvance(incoming);
        const auto work = soundBlasterSequenceOperations(musicSequence_, DriverSequenceAdmission::inheritedChannels);
        auto builderInput=incoming;
        if (playableSequenceOwned_ && builderInput.domain==SoundBlasterCallbackDomain::level2RetryNoHandle) {
            //17E5 initializes a new descriptor from the physical driver fields.
            builderInput.domain=SoundBlasterCallbackDomain::noHandle;
            builderInput.program.reset();
        }
        auto selected = soundBlasterSequenceCallbackState(builderInput, musicSequence_, work);
        selected.program = soundBlasterProgramSequenceState(*programState_, work.state);
        selected.runningStatus = recoveredParserStatus_;
        return selected;
    }

    SoundBlasterCallbackState prepareRecoveredLevel1(const PlayableMusicPublication publication) {
        if (publication != PlayableMusicPublication::current && publication != PlayableMusicPublication::suspended) {
            throw std::invalid_argument("unsupported recovered music publication");
        }
        const auto selected = previewRecoveredLevel1(publication);
        workSequenceSlot_ = recoveredLevel1Slot;
        auto& sequence = sequencers_[workSequenceSlot_];
        sequence.data = &musicSequence_;
        sequence.allocated = true;
        applyProgramState(selected);
        sequence.selected = false; //0325 publishes only after the activation body returns.
        if (publication == PlayableMusicPublication::suspended) {playableSequenceOwned_ = true;}
        return buildCallbackWorkState(SoundBlasterCallbackDomain::sequence);
    }

    SoundBlasterCallbackState previewRecoveredLevel1Retry() const {
        if (sequencers_[recoveredLevel1Slot].selected) {
            throw std::invalid_argument("Level1 retry activation requires its retained deselected gameplay allocation");
        }
        return previewRecoveredLevel1Retry(callbackWorkState(),
            buildCallbackWorkState(SoundBlasterCallbackDomain::sequence));
    }

    SoundBlasterCallbackState previewRecoveredLevel1Retry(const SoundBlasterCallbackState& noHandle,
        const SoundBlasterCallbackState& retainedHandle) const {
        const auto& sequence = sequencers_[recoveredLevel1Slot];
        if (!recoveredInitialization_ || !workClockActive_ || !driverEnabled_
            || workSequenceSlot_ != recoveredLevel1Slot || startupPhase_ != SoundBlasterStartupPhase::gameplay
            || !level1ResetCompleted_ || !programState_ || !sequence.allocated || sequence.data != &musicSequence_
            || !retainedHandle.program || retainedHandle.sequence != musicSequence_
            || retainedHandle.program->instrumentBank != instrumentBank_
            || retainedHandle.program->volumeTable != volumeTable_
            || retainedHandle.program->playerHitVolumeRow != programState_->playerHitVolumeRow
            || retainedHandle.program->enemy20CallerWindow != programState_->enemy20CallerWindow
            || retainedHandle.program->enemy20McbByte != programState_->enemy20McbByte
            || retainedHandle.program->bomb7CallerRow != programState_->bomb7CallerRow) {
            throw std::invalid_argument("Level1 retry preflight requires retained driver resources and stopped handle storage");
        }
        if (!sameCallbackState(soundBlasterDeselectState(retainedHandle),noHandle)) {
            throw std::invalid_argument("Level1 retry preflight current and retained handle disagree");
        }
        const auto work = soundBlasterSequenceOperations(musicSequence_,DriverSequenceAdmission::inheritedChannels);
        auto selected = soundBlasterSequenceCallbackState(noHandle,musicSequence_,work);
        selected.program = soundBlasterProgramSequenceState(*retainedHandle.program,work.state);
        selected.runningStatus = retainedHandle.runningStatus;
        return selected;
    }

    SoundBlasterCallbackState prepareRecoveredLevel1Retry() {
        const auto selected = previewRecoveredLevel1Retry();
        applyProgramState(selected);
        sequencers_[workSequenceSlot_].selected = false; //0325 publishes after17E5 returns.
        level1ResetCompleted_ = false; //A new activation requires its distinct01A8 reset receipt.
        return buildCallbackWorkState(SoundBlasterCallbackDomain::sequence);
    }

    SoundBlasterCallbackState previewRecoveredLevel1Reset() const {
        if (!recoveredInitialization_ || !workClockActive_ || workSequenceSlot_ != recoveredLevel1Slot
            || startupPhase_ != SoundBlasterStartupPhase::gameplay || level1ResetCompleted_
            || !sequencers_[workSequenceSlot_].selected
            || std::any_of(directDurations_.begin(),directDurations_.end(),[](const auto timer) { return timer != 0; })) {
            throw std::invalid_argument("Level1 direct reset requires its selected retained owner once");
        }
        return soundBlasterDirectResetState(callbackWorkState());
    }

    SoundBlasterCallbackState completeRecoveredLevel1Reset() {
        const auto state = previewRecoveredLevel1Reset();
        const auto reset = soundBlasterDirectResetOperations();
        applyProgramState(state);
        for (std::size_t slot = 0; slot < directDurations_.size(); ++slot) {
            directResetWordsD5_[slot] = *reset.state.directResetWords[slot][0];
            directResetWordsEb_[slot] = *reset.state.directResetWords[slot][1];
        }
        level1ResetCompleted_ = true;
        return state;
    }

private:
    void validateRecoveredPreparation() const {
        if (!recoveredInitialization_ || !workClockActive_ || !driverEnabled_
            || initializationOplWrites_ != soundBlasterInitializationWriteCount
            || startupPhase_ != SoundBlasterStartupPhase::idle
            || std::any_of(sequencers_.begin(), sequencers_.end(), [](const auto& sequence) { return sequence.allocated; })
            || std::any_of(directDurations_.begin(), directDurations_.end(), [](const auto timer) { return timer != 0; })) {
            throw std::invalid_argument("recovered sequence initialization requires fresh no-handle driver ownership");
        }
    }

    void applyDirectReset() {
        constexpr std::uint8_t resetProgram = 13;
        constexpr std::uint16_t resetWord = 4;
        for (std::size_t slot = 0; slot < directDurations_.size(); ++slot) {
            const auto index = firstDirectVoice + slot;
            auto& voice = voices_[index];
            voiceOwners_[index] = static_cast<std::uint8_t>(firstDirectChannel + slot);
            voice.active = false;
            voice.program = resetProgram;
            voice.note = 0;
            voice.frequencyPending = false;
            voiceLevels_[index].carrierKsl = 0;
            voiceLevels_[index].operatorKsl = 0;
            voiceLevels_[index].operatorEnabled = true;
            directResetWordsD5_[slot] = resetWord;
            directResetWordsEb_[slot] = resetWord;
        }
    }

public:

    void startMusic() {
        startSequenceData(0, &musicSequence_, Playback::repeat);
    }

    void stopMusic() {
        releaseSequence(0);
    }

    void startLevel1(const std::size_t presentationCallbacks) {
        // The natural startup caller waits for STELLAR SNG0 to end before
        // starting SNG1. This replays resource history, not elapsed device time.
        if (logoSequence_.size() >= 36) {
            startSequenceData(0, &logoSequence_, Playback::once);
            while (sequencers_[0].active) {
                advanceSequenceTick(0);
            }
        }

        // The original keeps the BLASTDRV instance alive across the long
        // STELLAR.RES presentation sequence.  The canonical OPL-logged Begin
        // route stops that sequence after inclusive sequencer tick 729;
        // starting LEVEL1.RES clears voices 0-5 but retains velocities on the
        // reserved physical voices 6-8.  Replaying the resource data preserves
        // that driver contract without making presentation audio part of the
        // gameplay-only frontend.
        if (presentationSequence_.size() >= 36) {
            startSequenceData(0, &presentationSequence_, Playback::once);
            for (
                std::size_t tick = 0;
                tick < presentationCallbacks;
                ++tick
            ) {
                advanceSequenceTick(0);
            }
        }
        startMusic();
        // Gameplay traces historically begin with the stable 158-write OPL
        // reset prefix.  Keep that scoped provenance while retaining the
        // hardware and voice state produced by the discarded pre-roll.
        if (registerWrites_.size() > 158) {
            registerWrites_.erase(registerWrites_.begin() + 158, registerWrites_.end());
        }
    }

    void startLevel2() {
        // A direct launch has no campaign history. Execute the success cue
        // without forcing captured velocities onto the reserved voices.
        const auto prefix = registerWrites_.size();
        startSuccess();
        for (unsigned tick = 0; successActive() && tick < sequenceRate * 10; ++tick) {
            advanceSequenceTick(0);
        }
        stopSuccess();
        registerWrites_.resize(prefix);
        switchLevel2();
    }

    void switchLevel2() {
        // A campaign handoff keeps the live driver's reserved velocities
        // and independent SFX timers; only the music resource is replaced.
        startSequenceData(0, &level2MusicSequence_, Playback::repeat);
    }

    void startLevel3() {
        for (std::size_t voice = 6; voice < voices_.size(); ++voice) {
            voices_[voice].velocity = 1;
        }
        startSequenceData(0, &level3MusicSequence_, Playback::repeat);
    }

    void switchLevel3() {
        if (level3MusicSequence_.size() < 36) {
            return;
        }
        // The native Level 2-to-Level 3 resource switch changes the active
        // SNG descriptor and cursor while retaining OPL voices, notes, and
        // the sample clock. Do not call startSequenceData(): it releases the
        // resident sequence and emits a false all-voice key-off sweep.
        const auto& sequence = level3MusicSequence_;
        const std::size_t base = sequence[0] == 0x84 ? 2 : 0;
        voiceOwners_.fill(0xff);
        channelVoiceCursor_.fill(0);
        std::size_t voice = 0;
        for (std::size_t channel = 0; channel < 16; ++channel) {
            const std::size_t descriptor = base + 1 + channel * 2;
            if (descriptor + 1 >= sequence.size()) {
                break;
            }
            const std::uint8_t count =
                static_cast<std::uint8_t>(sequence[descriptor] & 0x7f);
            const std::uint8_t flags = sequence[descriptor + 1];
            if ((flags & 0x04) == 0) {
                continue;
            }
            for (
                std::uint8_t assigned = 0;
                assigned < count && voice < voiceOwners_.size();
                ++assigned
            ) {
                voiceOwners_[voice++] = static_cast<std::uint8_t>(channel);
            }
        }
        Sequencer& sequencer = sequencers_[0];
        sequencer.data = &sequence;
        sequencer.position = base + 34;
        sequencer.loopPosition = sequencer.position;
        sequencer.playback = Playback::repeat;
        sequencer.runningStatus = 0;
        sequencer.delay = 0;
        sequencer.allocated = true;
        sequencer.active = true;
        sequencer.selected = true;
    }

    void startSuccess() {
        startSequence(0, 0);
    }

    void validateCampaignMusicLoad(const std::vector<std::uint8_t>& sequence) const {
        if (sequence.size() < 36) {
            throw std::invalid_argument("campaign music resource is incomplete");
        }
        const auto base = sequence[0] == 0x84 ? 2U : 0U;
        if (sequence[base] != 0 || sampledVoice_.resident()) {
            throw std::invalid_argument("campaign MIDI load excludes sampled or unsupported sequence types");
        }
    }

    void loadCampaignMusic(const std::vector<std::uint8_t>& sequence) {
        validateCampaignMusicLoad(sequence);
        releaseSequence(0);
        campaignMusicSequence_ = sequence;
        startSequenceData(0, &campaignMusicSequence_, Playback::repeat);
    }

    SoundBlasterCallbackState selectPlayableSequence(const std::vector<std::uint8_t>& sequence,
        const PlayableSequenceMode mode, const PlayableMusicPublication publication) {
        if (!recoveredInitialization_ || !workClockActive_ || !programState_
            || (mode != PlayableSequenceMode::once && mode != PlayableSequenceMode::repeat)
            || (publication != PlayableMusicPublication::current && publication != PlayableMusicPublication::suspended)) {
            throw std::invalid_argument("playable sequence requires its retained work-clock driver");
        }
        validateCampaignMusicLoad(sequence);
        const auto admission=mode==PlayableSequenceMode::once
            ? DriverSequenceAdmission::successInheritedChannels : DriverSequenceAdmission::inheritedChannels;
        const auto activation=soundBlasterSequenceOperations(sequence,admission);
        const auto incoming=callbackWorkState();
        auto selected=applyDriverSequenceCallbackState(incoming,activation);
        selected.program=soundBlasterProgramSequenceState(*incoming.program,activation.state);
        selected.program->repeatCount=mode==PlayableSequenceMode::repeat ? 0xffff : 1;
        selected.sequence=sequence;selected.position=sequenceBodyOffset;selected.wait=0;selected.operation=2;
        selected.domain=SoundBlasterCallbackDomain::sequence;selected.active=true;selected.fade=0;
        for (std::size_t index=0;index<sequencers_.size();++index) {
            const auto& old=sequencers_[index];
            const bool frozenStartup=playableSequenceOwned_ && index<workSequenceSlot_ && !old.selected
                && ((index==recoveredLogoSlot && old.data==&logoSequence_)
                    || (index==recoveredPresentationSlot && old.data==&presentationSequence_));
            if (index!=workSequenceSlot_ && (old.selected || (old.active && !frozenStartup))) {
                throw std::invalid_argument("playable sequence cannot replace another live allocation");
            }
        }
        if (incoming.domain==SoundBlasterCallbackDomain::sequence) {
            const auto stopped=soundBlasterStopAdvance(incoming,SoundBlasterStopProfile::retainedDirectTimers);
            for (const auto& block:stopped.blocks) {
                if (block.kind==SoundBlasterWorkKind::registerWrite) {applyProgramWrite(block.registerNumber,block.value);}
            }
        }
        campaignMusicSequence_=sequence;
        auto& slot=sequencers_[workSequenceSlot_];
        slot.data=&campaignMusicSequence_;slot.allocated=true;
        slot.selected=publication==PlayableMusicPublication::current;
        playableSequenceOwned_=true;
        applyProgramState(selected);
        return callbackWorkState();
    }

    SoundBlasterCallbackState selectPlayableAttractLogo(const PlayableMusicPublication publication) {
        playableAttractInheritedOwner_=playableSequenceOwned_;
        const auto state=selectPlayableSequence(logoSequence_,PlayableSequenceMode::repeat,publication);
        playableAttractLogo_=true;
        return state;
    }

    SoundBlasterCallbackState triggerPlayableAttractLogoEvent() {
        const auto& slot=sequencers_[workSequenceSlot_];
        if(!playableAttractLogo_ || !playableSequenceOwned_ || !programState_ || !slot.allocated
            || !slot.active || slot.data!=&campaignMusicSequence_ || campaignMusicSequence_!=logoSequence_) {
            throw std::invalid_argument("attract event26 requires its retained native SNG0 descriptor");
        }
        const auto incoming=slot.selected ? callbackWorkState() : buildCallbackWorkState(SoundBlasterCallbackDomain::sequence);
        const auto direct=soundBlasterPlayableDirectAdvance(incoming,startupLogoDirectRecord,SoundBlasterDirectCaller::unownedChannel);
        for(const auto& block:direct.blocks) {
            if(block.kind==SoundBlasterWorkKind::registerWrite) {applyProgramWrite(block.registerNumber,block.value);}
        }
        if(slot.selected) {applyProgramState(direct.state);} else {applySuccessPhysicalState(direct.state);}
        return callbackWorkState();
    }

    SoundBlasterCallbackState selectPlayableAttractMenu(const PlayableMusicPublication publication) {
        selectPlayableSequence(presentationSequence_,PlayableSequenceMode::repeat,publication);
        //First-open6070 reactivates the existing4C12 allocation; keep its identity.
        auto& slot=sequencers_[workSequenceSlot_];slot.data=&presentationSequence_;
        playableAttractLogo_=false;
        if(workSequenceSlot_==recoveredPresentationSlot) {startupPhase_=SoundBlasterStartupPhase::presentation;}
        playableSequenceOwned_=playableAttractInheritedOwner_;
        return callbackWorkState();
    }

    SoundBlasterCallbackState setPlayableMusicPublication(const PlayableMusicPublication publication) {
        if (!recoveredInitialization_ || !workClockActive_ || !programState_
            || (publication != PlayableMusicPublication::current && publication != PlayableMusicPublication::suspended)) {
            throw std::invalid_argument("music publication requires its retained playable driver");
        }
        auto& slot=sequencers_[workSequenceSlot_];
        if (!slot.allocated || !slot.data) {
            throw std::invalid_argument("music publication requires its retained descriptor");
        }
        const auto publish=publication==PlayableMusicPublication::current;
        if (slot.selected==publish) {return callbackWorkState();}
        const auto full=buildCallbackWorkState(SoundBlasterCallbackDomain::sequence);
        if (publish) {
            slot.selected=true; //51C4 publishes B94/B96 only; no rewind/driver initialization.
            return full;
        }
        const auto off=soundBlasterStopAdvance(full,SoundBlasterStopProfile::retainedDirectTimers);
        for (const auto& block:off.blocks) {
            if (block.kind==SoundBlasterWorkKind::registerWrite) {applyProgramWrite(block.registerNumber,block.value);}
        }
        //033E/opcode10 is2DA0 only: no2D9A/statusFFFF store or descriptor release.
        applyProgramState(off.state);
        slot.selected=false;playableSequenceOwned_=true;
        return callbackWorkState();
    }

    SoundBlasterCallbackState resetPlayableDirectEffects() {
        if (!recoveredInitialization_ || !workClockActive_ || !programState_) {
            throw std::invalid_argument("playable reset requires its retained work-clock driver");
        }
        const auto state=soundBlasterPlayableDirectResetState(callbackWorkState());
        if (sequencers_[workSequenceSlot_].selected) {applyProgramState(state);}
        else {applySuccessPhysicalState(state);}
        const auto reset=soundBlasterDirectResetOperations();
        for (std::size_t slot=0;slot<directDurations_.size();++slot) {
            directResetWordsD5_[slot]=*reset.state.directResetWords[slot][0];
            directResetWordsEb_[slot]=*reset.state.directResetWords[slot][1];
        }
        const auto& sequence=sequencers_[workSequenceSlot_];
        if (playableSequenceOwned_ && workSequenceSlot_==recoveredLevel1Slot
            && sequence.allocated && sequence.active && sequence.data==&musicSequence_) {
            level1ResetCompleted_=true;
            startupPhase_=SoundBlasterStartupPhase::gameplay;
        }
        return callbackWorkState();
    }

    SoundBlasterCallbackState stopPlayableSequence() {
        if (!recoveredInitialization_ || !workClockActive_ || !programState_) {
            throw std::invalid_argument("playable stop requires its retained work-clock driver");
        }
        auto stopped=soundBlasterStopAdvance(buildCallbackWorkState(SoundBlasterCallbackDomain::sequence),
            SoundBlasterStopProfile::retainedDirectTimers);
        for (const auto& block:stopped.blocks) {
            if (block.kind==SoundBlasterWorkKind::registerWrite) {applyProgramWrite(block.registerNumber,block.value);}
        }
        //2D9A follows the key-off body before51BD clears the current handle.
        stopped.state.program->handleStatus=0xffff;
        stopped.state.active=false;
        applyProgramState(stopped.state);
        playableSequenceOwned_=true;
        return callbackWorkState();
    }

    SoundBlasterCallbackState deselectPlayableSequence() {
        if (!recoveredInitialization_ || !workClockActive_ || !playableSequenceOwned_ || !programState_) {
            throw std::invalid_argument("playable deselection requires its retained stopped owner");
        }
        const auto state=soundBlasterLevel2RetryDeselectState(buildCallbackWorkState(SoundBlasterCallbackDomain::sequence));
        auto& slot=sequencers_[workSequenceSlot_];
        slot.selected=false;slot.active=false;slot.allocated=false;slot.data=nullptr;
        applyProgramState(state);
        return callbackWorkState();
    }

    SoundBlasterCallbackState releasePlayableStoryLogo() {
        auto& logo=sequencers_[recoveredLogoSlot];
        if (!recoveredInitialization_ || !workClockActive_ || !driverEnabled_ || !programState_
            || playableSequenceOwned_ || workSequenceSlot_!=recoveredLogoSlot
            || !logo.allocated || logo.selected || logo.active || logo.data!=&logoSequence_
            || programState_->handleStatus!=0xffff || programState_->repeatCount!=1
            || sampledVoice_.observation().phase!=SampledVoicePhase::unloaded
            || std::any_of(sequencers_.begin()+1,sequencers_.end(),[](const auto& slot) {return slot.allocated;})) {
            throw std::invalid_argument("Story logo release requires its completed cold source stop");
        }
        //625D/51A6 already stopped and deselected the logo. Adopt its native
        //borrow without repeating op0E or replacing any physical driver fields.
        playableSequenceOwned_=true;
        return releaseSuspendedPlayableSequence();
    }

    SoundBlasterCallbackState releaseSuspendedPlayableSequence() {
        auto& slot=sequencers_[workSequenceSlot_];
        if (!recoveredInitialization_ || !workClockActive_ || !playableSequenceOwned_ || !programState_
            || !slot.allocated || !slot.data || slot.selected) {
            throw std::invalid_argument("suspended resource release requires its unpublished native descriptor");
        }
        soundBlasterLevel2RetryNoHandleAdvance(callbackWorkState());
        //51A6 has no current handle: release the native borrow without op0E.
        slot.active=false;slot.allocated=false;slot.data=nullptr;
        return callbackWorkState();
    }

    void loadPlayableSampledVoice(const std::vector<std::uint8_t>& sequence) {
        if (!recoveredInitialization_ || !workClockActive_ || !playableSequenceOwned_
            || std::any_of(sequencers_.begin(),sequencers_.end(),[this](const auto& slot) {
                const bool frozenStartup=playableSequenceOwned_ && !slot.selected
                    && ((&slot==&sequencers_[recoveredLogoSlot] && slot.data==&logoSequence_)
                        || (&slot==&sequencers_[recoveredPresentationSlot] && slot.data==&presentationSequence_));
                return slot.selected || (slot.active && !frozenStartup);
            })
            || sampledVoice_.observation().phase!=SampledVoicePhase::unloaded) {
            throw std::invalid_argument("playable voice requires its released MIDI owner");
        }
        decodeSampledVoice(sequence);
        playableSampledVoice_=true;
        loadSampledVoice(sequence);
    }

    SoundBlasterCallbackState triggerPlayableEvent(const std::uint16_t eventId) {
        if (!recoveredInitialization_ || !workClockActive_ || !programState_) {
            throw std::invalid_argument("playable event requires its retained work-clock driver");
        }
        const std::array<std::uint8_t,8>* record=nullptr;
        auto caller=SoundBlasterDirectCaller::unownedChannel;
        switch(eventId) {
        case logoEvent:
            if (!playableSequenceOwned_ || workSequenceSlot_!=recoveredLogoSlot
                || !sequencers_[recoveredLogoSlot].allocated || !sequencers_[recoveredLogoSlot].active
                || sequencers_[recoveredLogoSlot].selected || sequencers_[recoveredLogoSlot].data!=&logoSequence_) {
                throw std::invalid_argument("native event26 requires its allocated unpublished logo descriptor");
            }
            record=&startupLogoDirectRecord;break;
        case storyFirstCueEvent:record=&storyFirstCueDirectRecord;caller=SoundBlasterDirectCaller::storyFirstCue;break;
        case storySecondCueEvent:record=&storySecondCueDirectRecord;caller=SoundBlasterDirectCaller::storySecondCue;break;
        case gameplaySuperPrimaryFireEvent:record=&gameplaySuperPrimaryFireDirectRecord;caller=SoundBlasterDirectCaller::gameplaySuperPrimaryFire;break;
        case gameplayTurboActivateEvent:record=&gameplayTurboActivateDirectRecord;caller=SoundBlasterDirectCaller::gameplayTurboActivate;break;
        case gameplayShieldDefeatEvent:record=&gameplayShieldDefeatDirectRecord;caller=SoundBlasterDirectCaller::gameplayShieldDefeat;break;
        case gameplayScannerPulseEvent:record=&gameplayScannerPulseDirectRecord;caller=SoundBlasterDirectCaller::gameplayScannerPulse;break;
        case gameplayDecoyEvent:record=&gameplayDecoyDirectRecord;caller=SoundBlasterDirectCaller::gameplayDecoy;break;
        case gameplayBombEvent:record=&gameplayBombDirectRecord;caller=SoundBlasterDirectCaller::gameplayBomb;break;
        case gameplaySystemActivateEvent:record=&gameplaySystemActivateDirectRecord;caller=SoundBlasterDirectCaller::gameplaySystemActivate;break;
        case gameplaySystemCycleEvent:record=&gameplaySystemCycleDirectRecord;caller=SoundBlasterDirectCaller::gameplaySystemCycle;break;
        case gameplayPrimaryFireEvent:record=&gameplayPrimaryFireDirectRecord;caller=SoundBlasterDirectCaller::gameplayPrimaryFire;break;
        case gameplayEnemyType17Event:record=&gameplayEnemyType17DirectRecord;caller=SoundBlasterDirectCaller::gameplayEnemyFire;break;
        case gameplayFuelBayEvent:record=&gameplayFuelBayDirectRecord;caller=SoundBlasterDirectCaller::gameplayFuelBay;break;
        case gameplayEnemyType20Event:record=&gameplayEnemyType20DirectRecord;caller=SoundBlasterDirectCaller::gameplayEnemyType20;break;
        case gameplayPlayerHitEvent:record=&gameplayPlayerHitDirectRecord;caller=SoundBlasterDirectCaller::gameplayPlayerHit;break;
        case gameplayPlayerCombatEvent:record=&gameplayPlayerCombatDirectRecord;caller=SoundBlasterDirectCaller::gameplayPlayerCombat;break;
        case gameplayTargetImpactEvent:record=&gameplayTargetImpactDirectRecord;caller=SoundBlasterDirectCaller::gameplayTargetImpact;break;
        case gameplaySystemBonusEvent:record=&gameplaySystemBonusDirectRecord;caller=SoundBlasterDirectCaller::gameplaySystemBonus;break;
        case gameplayArrivalEvent:record=&gameplayArrivalDirectRecord;caller=SoundBlasterDirectCaller::gameplayArrival;break;
        case gameplayGuardianDefeatEvent:record=&gameplayGuardianDefeatDirectRecord;caller=SoundBlasterDirectCaller::gameplayGuardianDefeat;break;
        case gameplaySolidContactEvent:record=&gameplaySolidContactDirectRecord;caller=SoundBlasterDirectCaller::gameplaySolidContact;break;
        case gameplayOrdinaryDeathEvent:record=&gameplayOrdinaryDeathDirectRecord;caller=SoundBlasterDirectCaller::gameplayOrdinaryDeath;break;
        default:throw std::invalid_argument("unsupported playable direct event");
        }
        const auto noCurrent=!sequencers_[workSequenceSlot_].selected;
        const bool storyCue=caller==SoundBlasterDirectCaller::storyFirstCue||caller==SoundBlasterDirectCaller::storySecondCue;
        const auto incoming=noCurrent&&!storyCue ? buildCallbackWorkState(SoundBlasterCallbackDomain::sequence) : callbackWorkState();
        const auto direct=soundBlasterPlayableDirectAdvance(incoming,*record,caller);
        for(const auto& block:direct.blocks) {
            if(block.kind==SoundBlasterWorkKind::registerWrite) {applyProgramWrite(block.registerNumber,block.value);}
        }
        if (noCurrent) {applySuccessPhysicalState(direct.state);}
        else {applyProgramState(direct.state);}
        return callbackWorkState();
    }

    void validateCampaignMusicRelease() const {
        if (campaignMusicSequence_.empty()
            || std::any_of(sequencers_.begin(), sequencers_.end(), [this](const auto& sequence) {
                return sequence.data == &campaignMusicSequence_;
            })) {
            throw std::invalid_argument("campaign descriptor release requires resident unborrowed storage");
        }
    }

    void releaseCampaignMusic() {
        validateCampaignMusicRelease();
        std::vector<std::uint8_t>{}.swap(campaignMusicSequence_);
    }

    void validateSampledVoiceLoad(const std::vector<std::uint8_t>& sequence) const {
        requireNativeSampledVoice();
        if (sampledVoice_.observation().phase != SampledVoicePhase::unloaded) {
            throw std::invalid_argument("sampled voice load requires released prior storage");
        }
        decodeSampledVoice(sequence);
    }

    void loadSampledVoice(const std::vector<std::uint8_t>& sequence) {
        requireInitialized();
        requireNativeSampledVoice();
        sampledVoice_.load(sequence);
    }

    void validateSampledVoiceStart() const {
        requireNativeSampledVoice();
        if (std::any_of(sequencers_.begin(), sequencers_.end(),
            [this](const auto& sequence) { return playableSampledVoice_ ? sequence.active || sequence.selected : sequence.allocated; })) {
            throw std::invalid_argument("sampled voice start requires the prior resident sequence to be stopped");
        }
        sampledVoice_.validateArm();
    }

    void startSampledVoice() {
        validateSampledVoiceStart();
        if (playableSampledVoice_) {resetPlayableDirectEffects();}
        else {resetDirectEffects();} //17E5's206B precedes19A4; not1821's MIDI voice-owner reset.
        sampledVoice_.arm();
    }

    void serviceSampledVoiceIrq() {
        requireNativeSampledVoice();
        sampledVoice_.serviceTransferIrq();
    }

    void stopSampledVoice() {
        requireNativeSampledVoice();
        const auto phase = sampledVoice_.observation().phase;
        const auto busy = sampledVoice_.busy();
        sampledVoice_.stop(); //Reject the pending-IRQ race before any OPL change.
        if (!busy && phase != SampledVoicePhase::loaded && phase != SampledVoicePhase::unloaded
            && phase != SampledVoicePhase::stopped) {
            silencePhysicalVoices(); //0Eh's not-busy2DA0 branch.
        }
    }

    void releaseSampledVoice() {
        requireNativeSampledVoice();
        sampledVoice_.release();
    }

    SampledVoiceObservation sampledVoiceObservation() const noexcept {
        return sampledVoice_.observation();
    }

    void stopSuccess() {
        releaseSequence(0);
    }

    bool successActive() const noexcept {
        return !sequences_.empty() && sequencers_[0].active
            && sequencers_[0].data == &sequences_[0];
    }

    void triggerLevel1Event(const int eventId) {
        requireInitialized();
        if (
            eventId < 0
            || static_cast<std::size_t>(eventId) >= level1EventRecords.size()
        ) {
            return;
        }
        triggerDirectEvent(
            static_cast<std::size_t>(eventId),
            level1EventRecords[static_cast<std::size_t>(eventId)]
        );
    }

    std::vector<std::int16_t> render(const std::size_t sampleCount) {
        requireInitialized();
        if (workClockActive_) {
            throw std::logic_error("sample-driven callbacks are disabled after normal work clock activation");
        }
        if (sampleCount != 0) {
            interruptMaskedSamples_ = 0;
        }
        std::vector<std::int16_t> result;
        result.reserve(sampleCount);
        while (result.size() < sampleCount) {
            if (sequenceSamplePhase_ >= sequencePhasePeriod) {
                serviceDueSampleInterrupt();
            }
            const std::uint64_t phaseRemaining =
                sequencePhasePeriod - sequenceSamplePhase_;
            std::size_t untilTick = static_cast<std::size_t>(
                (phaseRemaining + pitInputClock - 1) / pitInputClock
            );
            untilTick = std::max<std::size_t>(untilTick, 1);
            const std::size_t count = std::min(
                untilTick,
                sampleCount - result.size()
            );
            auto samples = renderOutputSamples(count);
            result.insert(result.end(), samples.begin(), samples.end());
            sequenceSamplePhase_ += count * pitInputClock;
        }
        return result;
    }

    std::vector<std::int16_t> renderInterruptMasked(const std::size_t sampleCount) {
        requireInitialized();
        if (workClockActive_) {
            throw std::logic_error("masked samples require recovered normal work after clock activation");
        }
        // Longer masks can collapse PIT edges in the PIC and need a separate model.
        constexpr std::size_t maximumSamples = sampleRate * pitDivisor / pitInputClock;
        if (sampleCount > maximumSamples - interruptMaskedSamples_) {
            throw std::runtime_error("Interrupt-masked audio exceeds one PIT IRQ period");
        }
        auto samples = renderOutputSamples(sampleCount);
        sequenceSamplePhase_ += sampleCount * pitInputClock;
        interruptMaskedSamples_ += sampleCount;
        return samples;
    }

    void resumeSampleInterrupts() {
        if (workClockActive_ || sequenceSamplePhase_ >= 2 * sequencePhasePeriod) {
            throw std::invalid_argument("unsupported source STI sample-clock phase");
        }
        interruptMaskedSamples_ = 0;
        if (sequenceSamplePhase_ >= sequencePhasePeriod) {
            serviceDueSampleInterrupt();
        }
    }

    bool hasInstrumentBank() const noexcept {
        return instrumentBank_.size() == 96 * 28;
    }

    const std::vector<OplRegisterWrite>& registerWrites() const noexcept {
        return registerWrites_;
    }

    void clearRegisterWrites() noexcept {
        registerWrites_.clear();
    }

    SoundBlasterDriverState driverState() const noexcept {
        SoundBlasterDriverState state;
        state.driverEnabled = driverEnabled_;
        state.initializationOplWrites = initializationOplWrites_;
        for (std::size_t voice = 0; voice < voices_.size(); ++voice) {
            state.notes[voice] = voices_[voice].note;
            state.velocities[voice] = voices_[voice].velocity;
        }
        state.channelVoiceCursors = channelVoiceCursor_;
        state.directTimers = directDurations_;
        state.directLevelLatch = directLevelLatch_;
        state.clockSource = workClockActive_ ? SoundBlasterClockSource::normalWork : SoundBlasterClockSource::samples;
        state.normalClockCycles = workCycles_;
        state.clockPhase = sequenceSamplePhase_;
        state.clockPeriod = sequencePhasePeriod;
        const auto slot = recoveredInitialization_ ? workSequenceSlot_ : 0;
        state.sequencePosition = sequencers_[slot].position;
        state.sequenceWait = sequencers_[slot].delay;
        state.runningStatus = recoveredInitialization_ ? recoveredParserStatus_ : sequencers_[slot].runningStatus;
        state.fade = musicFade_;
        state.fadeDivider = fadeDivider_;
        state.sequenceActive = sequencers_[slot].active;
        state.sequenceAllocated = sequencers_[slot].allocated;
        state.sequenceSelected = sequencers_[slot].selected;
        for (std::size_t index = 0; index < sequencers_.size(); ++index) {
            state.sequencePositions[index] = sequencers_[index].position;
            state.sequenceAllocations[index] = sequencers_[index].allocated;
            state.sequenceSelections[index] = sequencers_[index].selected;
            state.sequenceActivity[index] = sequencers_[index].active;
        }
        for (std::size_t voice = 0; voice < voiceLevels_.size(); ++voice) {
            const auto& level = voiceLevels_[voice];
            state.carrierKsl[voice] = level.carrierKsl;
            state.carrierFallback[voice] = level.carrierFallback;
            state.operatorKsl[voice] = level.operatorKsl;
            state.operatorFallback[voice] = level.operatorFallback;
            state.operatorEnabled[voice] = level.operatorEnabled;
        }
        return state;
    }

    SoundBlasterCallbackState callbackWorkState() const {
        const bool level2Retry = recoveredInitialization_ && workSequenceSlot_ == recoveredLevel1Slot
            && sequencers_[workSequenceSlot_].allocated && sequencers_[workSequenceSlot_].data == &level2MusicSequence_;
        return buildCallbackWorkState(sequencers_[recoveredInitialization_ ? workSequenceSlot_ : 0].selected
            ? SoundBlasterCallbackDomain::sequence : (level2Retry || playableSequenceOwned_) ? SoundBlasterCallbackDomain::level2RetryNoHandle
                : successAudioRetained_ ? SoundBlasterCallbackDomain::successNoHandle : SoundBlasterCallbackDomain::noHandle);
    }

private:
    bool nativeUnpublishedLevel1() const {
        if (!playableSequenceOwned_ || !recoveredInitialization_ || !workClockActive_ || !driverEnabled_
            || !level1ResetCompleted_ || !programState_ || workSequenceSlot_!=recoveredLevel1Slot
            || startupPhase_!=SoundBlasterStartupPhase::gameplay) {return false;}
        const auto& sequence=sequencers_[workSequenceSlot_];
        return sequence.allocated && sequence.active && !sequence.selected && sequence.data==&musicSequence_;
    }

public:
    void configurePlayerHitVolumeRow(const SoundBlasterVolumeRow& row) {
        if (row.volume != gameplayPlayerHitCallerVolume || !recoveredInitialization_ || !driverEnabled_
            || !level1ResetCompleted_ || workSequenceSlot_ != recoveredLevel1Slot
            || startupPhase_ != SoundBlasterStartupPhase::gameplay || !programState_
            || programState_->playerHitVolumeRow || !sequencers_[workSequenceSlot_].allocated
            || (!sequencers_[workSequenceSlot_].selected && !nativeUnpublishedLevel1()) || !sequencers_[workSequenceSlot_].active) {
            throw std::invalid_argument("player hit volume row requires one immutable binding after Level1 reset");
        }
        programState_->playerHitVolumeRow = row;
    }

    SoundBlasterEnemy20CallerWindow configureEnemy20CallerWindow(const SoundBlasterCallbackState& retained) {
        if (!recoveredInitialization_ || !driverEnabled_ || !level1ResetCompleted_
            || workSequenceSlot_ != recoveredLevel1Slot || startupPhase_ != SoundBlasterStartupPhase::gameplay
            || !programState_ || programState_->enemy20CallerWindow || sequences_.empty()
            || sequencers_[workSequenceSlot_].data != &musicSequence_
            || !sequencers_[workSequenceSlot_].allocated || (!sequencers_[workSequenceSlot_].selected && !nativeUnpublishedLevel1())
            || !sequencers_[workSequenceSlot_].active || !sameCallbackState(callbackWorkState(),retained)) {
            throw std::invalid_argument("enemy20 caller window requires one binding on the actual Level1 reset owner");
        }
        const auto window = level1Enemy20CallerWindow(sequences_[0]);
        programState_->enemy20CallerWindow = window;
        return window;
    }

    void validateBomb7CallerRow(const SoundBlasterCallbackState& retained,
        const SoundBlasterBomb7CallerRow&) const {
        if (!recoveredInitialization_ || !workClockActive_ || !driverEnabled_ || !level1ResetCompleted_
            || workSequenceSlot_ != recoveredLevel1Slot || startupPhase_ != SoundBlasterStartupPhase::gameplay
            || !programState_ || programState_->bomb7CallerRow || !sameCallbackState(callbackWorkState(), retained)) {
            throw std::invalid_argument("Bomb audio row requires one binding on the retained Level1 owner");
        }
    }

    void configureBomb7CallerRow(const SoundBlasterCallbackState& retained, const SoundBlasterBomb7CallerRow& row) {
        validateBomb7CallerRow(retained, row);
        programState_->bomb7CallerRow = row;
    }

    void validatePlayableBombEvent(const SoundBlasterBomb7CallerRow& row) const {
        if (!recoveredInitialization_ || !workClockActive_ || !programState_
            || (programState_->bomb7CallerRow && *programState_->bomb7CallerRow != row)) {
            throw std::invalid_argument("Bomb audio preview lost its retained owner generation");
        }
        auto incoming = sequencers_[workSequenceSlot_].selected ? callbackWorkState()
            : buildCallbackWorkState(SoundBlasterCallbackDomain::sequence);
        incoming.program->bomb7CallerRow = row;
        //8E94's activation8 precedes8E20's event7. Neither preview publishes state.
        const auto activation = soundBlasterPlayableDirectAdvance(incoming, gameplaySystemActivateDirectRecord,
            SoundBlasterDirectCaller::gameplaySystemActivate);
        soundBlasterPlayableDirectAdvance(activation.state, gameplayBombDirectRecord, SoundBlasterDirectCaller::gameplayBomb);
    }

    void configureEnemy20McbByte(const SoundBlasterCallbackState& retained,const SoundBlasterEnemy20McbByte& byte) {
        if (!recoveredInitialization_ || !driverEnabled_ || !level1ResetCompleted_
            || workSequenceSlot_ != recoveredLevel1Slot || startupPhase_ != SoundBlasterStartupPhase::gameplay
            || !programState_ || !programState_->enemy20CallerWindow || programState_->enemy20McbByte
            || sequencers_[workSequenceSlot_].data != &musicSequence_
            || !sequencers_[workSequenceSlot_].allocated || (!sequencers_[workSequenceSlot_].selected && !nativeUnpublishedLevel1())
            || !sequencers_[workSequenceSlot_].active || !sameCallbackState(callbackWorkState(),retained)) {
            throw std::invalid_argument("enemy20 MCB byte requires one binding on the actual Level1 reset owner");
        }
        programState_->enemy20McbByte = byte;
    }

    void configurePlayableEnemy20Process(const SoundBlasterCallbackState& retained,const SoundBlasterEnemy20McbByte& byte) {
        if (!recoveredInitialization_ || !driverEnabled_ || !level1ResetCompleted_
            || workSequenceSlot_ != recoveredLevel1Slot || startupPhase_ != SoundBlasterStartupPhase::gameplay
            || !programState_ || !programState_->enemy20McbByte
            || !byte.isNativeIdentityOf(*programState_->enemy20McbByte)
            || !sameCallbackState(callbackWorkState(),retained)) {
            throw std::invalid_argument("native DOS process identity requires its retained Level1 allocation generation");
        }
        programState_->enemy20McbByte = byte;
    }

private:
    SoundBlasterCallbackState buildCallbackWorkState(const SoundBlasterCallbackDomain domain) const {
        const auto slot = recoveredInitialization_ ? workSequenceSlot_ : 0;
        if ((!playableSampledVoice_ && (sampledVoice_.resident() || sampledVoice_.observation().speakerEnabled))
            || !workStateSupported_
            || (domain == SoundBlasterCallbackDomain::sequence
                && (!sequencers_[slot].allocated || sequencers_[slot].data == nullptr
                    || (!sequencers_[slot].active && (!programState_ || programState_->handleStatus != 0xffff
                        || (programState_->repeatCount != 0 && programState_->repeatCount != 1
                            && programState_->repeatCount != 0xffff)))))) {
            throw std::invalid_argument("unsupported live callback state for normal work clock");
        }
        for (std::size_t index = 0; index < sequencers_.size(); ++index) {
            const auto& other = sequencers_[index];
            const bool oldLogo = recoveredInitialization_ && slot >= recoveredPresentationSlot && index == recoveredLogoSlot
                && other.data == &logoSequence_ && !other.active && !other.selected && other.playback == Playback::once;
            const bool oldMenu = recoveredInitialization_ && (slot == recoveredLevel1Slot || (successAudioRetained_ && slot == recoveredSuccessSlot)) && index == recoveredPresentationSlot
                && other.data == &presentationSequence_ && !other.active && !other.selected && other.playback == Playback::repeat;
            const bool oldLevel1 = successAudioRetained_ && slot == recoveredSuccessSlot
                && index == recoveredLevel1Slot && other.data == &musicSequence_
                && !other.active && !other.selected && other.playback == Playback::repeat;
            const bool oldSuccess = successAudioRetained_ && slot==recoveredLevel1Slot
                && sequencers_[slot].data==&level2MusicSequence_ && index==recoveredSuccessSlot
                && !sequences_.empty() && other.data==&sequences_[0] && !other.active && !other.selected
                && other.playback==Playback::once;
            const bool retiredPlayable=playableSequenceOwned_ && !other.active && !other.selected;
            const bool frozenStartup=playableSequenceOwned_ && index<slot && !other.selected
                && ((index==recoveredLogoSlot && other.data==&logoSequence_)
                    || (index==recoveredPresentationSlot && other.data==&presentationSequence_));
            if (index != slot && other.allocated && !oldLogo && !oldMenu && !oldLevel1 && !oldSuccess
                && !retiredPlayable && !frozenStartup) {
                throw std::invalid_argument("multiple resident sequences excluded from normal work clock");
            }
        }
        SoundBlasterCallbackState state{};
        state.driverEnabled = driverEnabled_;
        state.voices.channels = voiceOwners_;
        state.voices.directTimers = directDurations_;
        state.voices.channelDeferrals = channelDeferrals_;
        state.channelEnabled = channelEnabled_;
        state.channelVoiceCursors = channelVoiceCursor_;
        if (domain == SoundBlasterCallbackDomain::sequence) {
            state.sequence = *sequencers_[slot].data;
            state.position = sequencers_[slot].position;
            state.wait = sequencers_[slot].delay;
            state.runningStatus = recoveredInitialization_ ? recoveredParserStatus_ : sequencers_[slot].runningStatus;
            state.program = programState_;
            state.active = sequencers_[slot].active;
        } else {
            state.domain = domain;
            state.active = false;
            state.operation = 0x1e;
            if (domain == SoundBlasterCallbackDomain::successNoHandle
                || domain == SoundBlasterCallbackDomain::level2RetryNoHandle) {
                state.program = programState_;
                state.runningStatus = recoveredParserStatus_;
            }
        }
        state.fade = musicFade_;
        state.latch = directLevelLatch_ ? SoundBlasterLevelLatch::Latched : SoundBlasterLevelLatch::Unlatched;
        for (std::size_t voice = 0; voice < voices_.size(); ++voice) {
            if (voices_[voice].frequencyPending) {
                throw std::invalid_argument("pending live frequency excluded from normal work clock");
            }
            state.voices.notes[voice] = voices_[voice].note;
            state.velocities[voice] = voices_[voice].velocity;
            state.sounding[voice] = static_cast<std::uint8_t>(voices_[voice].active);
            if (programState_) {
                state.voices.tuning[voice] = programTuning_[voice];
            }
        }
        return state;
    }

public:

    ClockedSoundBlasterAudio renderClocked(const NormalWorkTiming& work, const std::uint64_t originCycles) {
        const auto voice=sampledVoice_.observation();
        //Released Story speech retains the physical speaker/DAC hold through
        //native menu startup; it has no active DMA or pending speech IRQ.
        const bool nativeVoice=playableSampledVoice_&&(work.phase.playableCampaign
            ||(work.phase.playableStartup&&voice.phase==SampledVoicePhase::unloaded));
        if (!nativeVoice && (sampledVoice_.resident() || voice.speakerEnabled)) {
            throw std::invalid_argument("sampled DSP/DMA/IRQ work is not qualified for the strict clock");
        }
        const bool callbackInactive = work.phase.timer && work.phase.timer->activeMask == 0;
        if (recoveredInitialization_ != work.phase.soundBlasterInitialization.has_value()) {
            throw std::invalid_argument("normal work differs from owned Sound Blaster initialization policy");
        }
        if (callbackInactive && !work.callbacks.empty()) {
            throw std::invalid_argument("inactive timer cannot deliver Sound Blaster callbacks");
        }
        if (originCycles != workCycles_ || work.total.elapsedCycles < 0
            || static_cast<std::uint64_t>(work.total.elapsedCycles) > std::numeric_limits<std::uint64_t>::max() - originCycles) {
            throw std::invalid_argument("noncontiguous normal work PCM timeline");
        }
        const auto endCycles = originCycles + static_cast<std::uint64_t>(work.total.elapsedCycles);
        const auto originSample = clockedSamplesAt(originCycles);
        const auto sampleCount = clockedSamplesAt(endCycles) - originSample;
        ClockedSoundBlasterAudio output;
        output.firstSample = originSample;
        std::int64_t previousVgaCycle = -1;
        for (const auto& receipt : work.vgaServices) {
            if (receipt.cycleOffset < 0 || receipt.cycleOffset < previousVgaCycle ||
                receipt.cycleOffset > work.total.elapsedCycles) {
                throw std::invalid_argument("invalid VGA order in normal work PCM timeline");
            }
            output.vgaSamplePositions.push_back(clockedSamplesAt(
                originCycles + static_cast<std::uint64_t>(receipt.cycleOffset)));
            previousVgaCycle = receipt.cycleOffset;
        }
        const auto savedSequencers = sequencers_;
        const auto savedWorkSequenceSlot = workSequenceSlot_;
        const auto savedSuccessRetained = successAudioRetained_;
        const auto savedLevelReset = level1ResetCompleted_;
        const auto savedResetD5 = directResetWordsD5_;
        const auto savedResetEb = directResetWordsEb_;
        const auto savedVoices = voices_;
        const auto savedOwners = voiceOwners_;
        const auto savedCursors = channelVoiceCursor_;
        const auto savedTimers = directDurations_;
        const auto savedLatch = directLevelLatch_;
        const auto savedDriverEnabled = driverEnabled_;
        const auto savedInitializationWrites = initializationOplWrites_;
        const auto savedStartupPhase = startupPhase_;
        const auto savedProgram = programState_;
        const auto savedParserStatus = recoveredParserStatus_;
        const auto savedFade = musicFade_;
        const auto savedFadeDivider = fadeDivider_;
        const auto savedProgramTuning = programTuning_;
        const auto savedLevels = voiceLevels_;
        const auto savedEnabled = channelEnabled_;
        const auto savedDeferrals = channelDeferrals_;
        const auto writeStart = registerWrites_.size();
        deferOplWrites_ = true;
        try {
            std::int64_t previousCycle = -1;
            for (const auto& effect : work.soundBlasterEffects) {
                if (effect.cycleOffset < 0 || effect.cycleOffset < previousCycle
                    || effect.cycleOffset > work.total.elapsedCycles) {
                    throw std::invalid_argument("invalid foreground Sound Blaster effect order");
                }
                previousCycle = effect.cycleOffset;
            }
            previousCycle = -1;
            std::size_t foreground = 0;
            std::vector<std::uint16_t> directWrites;
            std::size_t successActivationEffect = 0;
            bool successActivationStarted = false;
            bool level2ActivationStarted = false;
            std::size_t level2ActivationEffect = 0;
            std::size_t level2ResetEffect = 0;
            const auto applyForeground = [&](const std::int64_t through) {
                while (foreground < work.soundBlasterEffects.size()
                    && work.soundBlasterEffects[foreground].cycleOffset <= through) {
                    const auto& effect = work.soundBlasterEffects[foreground++];
                    if (effect.kind == SoundBlasterForegroundKind::fadeRequest) {
                        const bool suspendedMenu=work.phase.playableMusicOff && playableStartupProgram(work.phase)
                            && playableSequenceOwned_
                            && workSequenceSlot_==recoveredPresentationSlot
                            && sequencers_[workSequenceSlot_].data==&presentationSequence_
                            && !sequencers_[workSequenceSlot_].selected;
                        auto state = suspendedMenu ? buildCallbackWorkState(SoundBlasterCallbackDomain::sequence)
                            : callbackWorkState();
                        const auto menuEof=workSequenceSlot_==recoveredPresentationSlot
                            && sequencers_[workSequenceSlot_].data==&presentationSequence_
                            && retainedMenuPrediction(work.phase,state) && retainedMenuEof(state);
                        if (effect.value != 0x0c || !state.program
                            || ((!state.active || state.program->handleStatus != 0) && !menuEof
                                && !(suspendedMenu && state.program->handleStatus==0xffff))
                            || state.program->handleVolume == 0
                            || state.program->handleVolume > musicVolume) {
                            throw std::invalid_argument("source fade requires the live positive-volume handle");
                        }
                        state.fade = state.program->handleVolume;
                        applyProgramState(state);
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::sequenceEnd) {
                        auto state = callbackWorkState();
                        if (effect.value != 0xffff || !state.program) {
                            throw std::invalid_argument("source stop status requires the selected handle");
                        }
                        state.program->handleStatus = effect.value;
                        state.active = false;
                        applyProgramState(state);
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::stopComplete
                        || effect.kind == SoundBlasterForegroundKind::successStopComplete
                        || effect.kind == SoundBlasterForegroundKind::level2RetryStopComplete) {
                        if (effect.value != 0x0e) {
                            throw std::invalid_argument("unsupported foreground stop operation");
                        }
                        if (effect.kind == SoundBlasterForegroundKind::level2RetryStopComplete
                            && (!work.phase.level2RetryMusic || work.phase.level2RetryMusic->stage != AudioClockLevel2RetryMusicStage::stopped
                                || !work.phase.sequenceSelection || work.phase.sequenceSelection->songOrdinal != level2MusicIdentity
                                || work.phase.sequenceSelection->prepared.sequence != level2MusicSequence_
                                || workSequenceSlot_ != recoveredLevel1Slot || sequencers_[workSequenceSlot_].data != &level2MusicSequence_
                                || !sequencers_[workSequenceSlot_].selected)) {
                            throw std::invalid_argument("Level2 retry stop lost its canonical selected owner");
                        }
                        const auto stopped = soundBlasterStopAdvance(callbackWorkState(),
                            effect.kind != SoundBlasterForegroundKind::stopComplete
                                ? SoundBlasterStopProfile::retainedDirectTimers : SoundBlasterStopProfile::zeroDirectTimers);
                        std::vector<std::uint16_t> expected;
                        for (const auto& block : stopped.blocks) {
                            if (block.kind == SoundBlasterWorkKind::registerWrite) {
                                expected.push_back(static_cast<std::uint16_t>((block.registerNumber << 8) | block.value));
                            }
                        }
                        if (directWrites != expected) {
                            throw std::invalid_argument("stop OPL work differs from retained live driver state");
                        }
                        applyProgramState(stopped.state);
                        directWrites.clear();
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::level2RetryCurrentClear) {
                        if (!effect.directState || !work.phase.level2RetryMusic
                            || work.phase.level2RetryMusic->stage != AudioClockLevel2RetryMusicStage::stopped
                            || workSequenceSlot_ != recoveredLevel1Slot || !sequencers_[workSequenceSlot_].selected
                            || sequencers_[workSequenceSlot_].data != &level2MusicSequence_) {
                            throw std::invalid_argument("Level2 retry current clear lost its selected physical owner");
                        }
                        const auto cleared=soundBlasterLevel2RetryDeselectState(callbackWorkState());
                        if (!sameCallbackState(cleared,*effect.directState)) {
                            throw std::invalid_argument("Level2 retry current clear differs from retained driver state");
                        }
                        sequencers_[workSequenceSlot_].selected=false;
                        if (!sameCallbackState(callbackWorkState(),cleared)) {
                            throw std::invalid_argument("Level2 retry current clear changed physical storage");
                        }
                        previousCycle=effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::successCurrentClear) {
                        if (!effect.directState || !sequencers_[workSequenceSlot_].selected
                            || (workSequenceSlot_ != recoveredLevel1Slot && workSequenceSlot_ != recoveredSuccessSlot)) {
                            throw std::invalid_argument("success current clear lost its selected source owner");
                        }
                        const auto cleared = soundBlasterSuccessDeselectState(callbackWorkState());
                        if (!sameCallbackState(cleared,*effect.directState)) {
                            throw std::invalid_argument("success current clear receipt differs from live driver");
                        }
                        sequencers_[workSequenceSlot_].selected = false;
                        successAudioRetained_ = true;
                        if (!sameCallbackState(callbackWorkState(),cleared)) {
                            throw std::invalid_argument("success current clear changed physical storage");
                        }
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::level2ActivationState
                        || effect.kind == SoundBlasterForegroundKind::level2SequencePublish
                        || effect.kind == SoundBlasterForegroundKind::level2ResetState
                        || effect.kind == SoundBlasterForegroundKind::level2RetryActivationState
                        || effect.kind == SoundBlasterForegroundKind::level2RetrySequencePublish
                        || effect.kind == SoundBlasterForegroundKind::level2RetryResetState) {
                        const bool retry=effect.kind == SoundBlasterForegroundKind::level2RetryActivationState
                            || effect.kind == SoundBlasterForegroundKind::level2RetrySequencePublish
                            || effect.kind == SoundBlasterForegroundKind::level2RetryResetState;
                        if (!effect.directState || !work.phase.sequenceSelection
                            || work.phase.sequenceSelection->songOrdinal != level2MusicIdentity
                            || work.phase.sequenceSelection->prepared.sequence != level2MusicSequence_
                            || (!retry && (!work.phase.successAudio || work.phase.successAudio->stage != SuccessAudioStage::returned
                                || !successAudioRetained_))
                            || (retry && (!work.phase.level2RetryMusic
                                || (work.phase.level2RetryMusic->stage != AudioClockLevel2RetryMusicStage::playing
                                    && work.phase.level2RetryMusic->stage != AudioClockLevel2RetryMusicStage::reset)))
                            || !recoveredInitialization_ || !driverEnabled_) {
                            throw std::invalid_argument("Level2 receipt lacks the owned destination resource and completed success");
                        }
                        const bool reset=effect.kind==SoundBlasterForegroundKind::level2ResetState
                            || effect.kind==SoundBlasterForegroundKind::level2RetryResetState;
                        const auto body=reset ? soundBlasterDirectResetOperations()
                            : soundBlasterSequenceOperations(level2MusicSequence_,DriverSequenceAdmission::inheritedChannels);
                        if (effect.kind==SoundBlasterForegroundKind::level2SequencePublish
                            || effect.kind==SoundBlasterForegroundKind::level2RetrySequencePublish) {
                            if (effect.value!=level2MusicIdentity || !level2ActivationStarted
                                || level2ActivationEffect!=body.effects.size()
                                || workSequenceSlot_!=(retry ? recoveredLevel1Slot : recoveredSuccessSlot)) {
                                throw std::invalid_argument("Level2 publication precedes its complete source activation");
                            }
                            auto expected=callbackWorkState();
                            expected.domain=SoundBlasterCallbackDomain::sequence;
                            expected.sequence=level2MusicSequence_;expected.position=level2MusicBodyOffset;
                            expected.wait=0;expected.operation=2;expected.active=true;
                            if (!sameCallbackState(expected,*effect.directState)) {
                                throw std::invalid_argument("Level2 publication differs from retained physical state");
                            }
                            workSequenceSlot_=recoveredLevel1Slot; //Reuse the existing campaign allocation slot.
                            auto& selected=sequencers_[workSequenceSlot_];
                            selected.data=&level2MusicSequence_;selected.allocated=true;
                            applyProgramState(expected);selected.selected=true;
                            level1ResetCompleted_=false;
                        } else {
                            if ((!reset && (retry
                                    ? (workSequenceSlot_!=recoveredLevel1Slot || sequencers_[workSequenceSlot_].selected
                                        || sequencers_[workSequenceSlot_].active || sequencers_[workSequenceSlot_].data!=&level2MusicSequence_)
                                    : (workSequenceSlot_!=recoveredSuccessSlot || sequencers_[workSequenceSlot_].selected
                                        || sequencers_[recoveredLevel1Slot].selected || sequencers_[recoveredLevel1Slot].active
                                        || sequencers_[recoveredLevel1Slot].data!=&musicSequence_)))
                                || (reset && (workSequenceSlot_!=recoveredLevel1Slot || !sequencers_[workSequenceSlot_].selected
                                    || sequencers_[workSequenceSlot_].data!=&level2MusicSequence_ || level1ResetCompleted_))) {
                                throw std::invalid_argument("Level2 driver stores lost their retained source allocation");
                            }
                            auto expected=callbackWorkState();
                            auto& index=reset ? level2ResetEffect : level2ActivationEffect;
                            if (effect.value==0) {
                                if (reset) {
                                    if (index!=body.effects.size()) {
                                        throw std::invalid_argument("Level2 reset completion precedes a source store");
                                    }
                                } else {
                                    if (level2ActivationStarted) {
                                        throw std::invalid_argument("Level2 repeat initialization repeats");
                                    }
                                    expected.program->repeatCount=0xffff;level2ActivationStarted=true;
                                }
                            } else {
                                if ((!reset && !level2ActivationStarted) || effect.value!=index+1 || index>=body.effects.size()) {
                                    throw std::invalid_argument("Level2 source stores are out of order");
                                }
                                const auto& store=body.effects[index++];
                                expected=applyDriverSequenceEffect(expected,store);
                                if (store.kind==DriverSequenceEffectKind::directResetWord) {
                                    auto& words=store.index%2==0 ? directResetWordsD5_ : directResetWordsEb_;
                                    words[store.index/2]=store.value;
                                }
                            }
                            if (!sameCallbackState(expected,*effect.directState)) {
                                throw std::invalid_argument("Level2 source store differs from live driver storage");
                            }
                            if (reset) {
                                applyProgramState(expected);
                                if (effect.value==0) {level1ResetCompleted_=true;}
                            } else {applySuccessPhysicalState(expected);}
                        }
                        previousCycle=effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::successActivationState) {
                        if (!effect.directState || !successAudioRetained_ || workSequenceSlot_ != recoveredLevel1Slot
                            || !work.phase.successAudio || sequences_.empty()
                            || work.phase.successAudio->sequence != sequences_[0]
                            || sequencers_[recoveredSuccessSlot].allocated) {
                            throw std::invalid_argument("success activation requires the already-owned SSM0 resource");
                        }
                        const auto activation = soundBlasterSequenceOperations(sequences_[0],DriverSequenceAdmission::successInheritedChannels);
                        auto expected = callbackWorkState();
                        if (effect.value == 0) {
                            if (successActivationStarted) {
                                throw std::invalid_argument("success030C repeat initialization repeats");
                            }
                            expected.program->repeatCount = 0xffff;
                            successActivationStarted = true;
                        } else {
                            if (!successActivationStarted || effect.value != successActivationEffect+1
                                || successActivationEffect >= activation.effects.size()) {
                                throw std::invalid_argument("success activation source stores are out of order");
                            }
                            const auto& store = activation.effects[successActivationEffect++];
                            expected = applyDriverSequenceEffect(expected,store);
                            if (store.kind == DriverSequenceEffectKind::directResetWord) {
                                auto& words = store.index % 2 == 0 ? directResetWordsD5_ : directResetWordsEb_;
                                words[store.index/2] = store.value;
                            }
                        }
                        if (!sameCallbackState(expected,*effect.directState)) {
                            throw std::invalid_argument("success partial activation receipt differs from live source state");
                        }
                        applySuccessPhysicalState(expected);
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::successSequencePublish) {
                        if (effect.value != recoveredSuccessSlot || !effect.directState || !successActivationStarted
                            || workSequenceSlot_ != recoveredLevel1Slot || sequences_.empty()
                            || sequencers_[recoveredSuccessSlot].allocated || !work.phase.successAudio
                            || work.phase.successAudio->sequence != sequences_[0]) {
                            throw std::invalid_argument("success0325 publication lacks its complete source activation");
                        }
                        const auto activation = soundBlasterSequenceOperations(sequences_[0],DriverSequenceAdmission::successInheritedChannels);
                        if (successActivationEffect != activation.effects.size()) {
                            throw std::invalid_argument("success publication precedes a source driver write");
                        }
                        auto expected = callbackWorkState();
                        expected.domain = SoundBlasterCallbackDomain::sequence;
                        expected.sequence = sequences_[0];
                        expected.position = 36;
                        expected.wait = 0;
                        expected.operation = 2;
                        expected.active = true;
                        if (!sameCallbackState(expected,*effect.directState)) {
                            throw std::invalid_argument("success publication receipt differs from live source state");
                        }
                        workSequenceSlot_ = recoveredSuccessSlot;
                        auto& selected = sequencers_[workSequenceSlot_];
                        selected.data = &sequences_[0];
                        selected.allocated = true;
                        applyProgramState(expected);
                        selected.selected = true;
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::sequenceClear) {
                        if (effect.value != 0 || !sequencers_[workSequenceSlot_].selected) {
                            throw std::invalid_argument("source current clear requires the selected handle");
                        }
                        const bool nativeMenu = work.phase.playableStartup && playableSequenceOwned_
                            && workSequenceSlot_ == recoveredPresentationSlot
                            && sequencers_[workSequenceSlot_].data == &presentationSequence_;
                        const auto cleared = nativeMenu ? soundBlasterLevel2RetryDeselectState(callbackWorkState())
                            : soundBlasterDeselectState(callbackWorkState());
                        sequencers_[workSequenceSlot_].selected = false;
                        if (!sameCallbackState(callbackWorkState(), cleared)) {
                            throw std::invalid_argument("source current clear changed physical driver storage");
                        }
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::repeatOnce) {
                        if (effect.value != 1 || !programState_ || programState_->repeatCount != 0xffff) {
                            throw std::invalid_argument("source logo once store requires the retained repeating handle");
                        }
                        auto state = callbackWorkState();
                        state.program->repeatCount = 1;
                        applyProgramState(state);
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::runtimeOplWrite) {
                        if (!recoveredInitialization_ || !driverEnabled_ || !programState_) {
                            throw std::invalid_argument("direct OPL work requires the retained driver");
                        }
                        applyProgramWrite(static_cast<std::uint8_t>(effect.value >> 8),
                            static_cast<std::uint8_t>(effect.value));
                        directWrites.push_back(effect.value);
                        output.writes.push_back({effect.cycleOffset,
                            {static_cast<std::uint8_t>(effect.value >> 8), static_cast<std::uint8_t>(effect.value)}});
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::directState) {
                        if (effect.value != 26 || !effect.directState) {
                            throw std::invalid_argument("direct event requires a derived state receipt");
                        }
                        const auto direct = soundBlasterDirectAdvance(callbackWorkState(), startupLogoDirectRecord);
                        std::vector<std::uint16_t> expected;
                        for (const auto& block : direct.blocks) {
                            if (block.kind == SoundBlasterWorkKind::registerWrite) {
                                expected.push_back(static_cast<std::uint16_t>((block.registerNumber << 8) | block.value));
                            }
                        }
                        if (!sameCallbackState(direct.state, *effect.directState) || directWrites != expected) {
                            throw std::invalid_argument("direct event differs from retained live driver state");
                        }
                        applyProgramState(direct.state);
                        directWrites.clear();
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::gameplayDirectState) {
                        if (effect.value != gameplayPrimaryFireEvent || !effect.directState
                            || !work.phase.sequenceSelection || work.phase.sequenceSelection->songOrdinal != 2
                            || work.phase.sequenceSelection->prepared.sequence != musicSequence_
                            || !recoveredInitialization_ || !driverEnabled_ || !level1ResetCompleted_
                            || workSequenceSlot_ != recoveredLevel1Slot
                            || sequencers_[workSequenceSlot_].data != &musicSequence_
                            || startupPhase_ != SoundBlasterStartupPhase::gameplay
                            || !sequencers_[workSequenceSlot_].allocated || !sequencers_[workSequenceSlot_].selected) {
                            throw std::invalid_argument("primary fire receipt requires the retained Level1 owner");
                        }
                        const auto direct = soundBlasterDirectAdvance(callbackWorkState(), gameplayPrimaryFireDirectRecord,
                            SoundBlasterDirectCaller::gameplayPrimaryFire);
                        std::vector<std::uint16_t> expected;
                        for (const auto& block : direct.blocks) {
                            if (block.kind == SoundBlasterWorkKind::registerWrite) {
                                expected.push_back(static_cast<std::uint16_t>((block.registerNumber << 8) | block.value));
                            }
                        }
                        if (!sameCallbackState(direct.state, *effect.directState) || directWrites != expected) {
                            throw std::invalid_argument("primary fire receipt differs from live driver state or ordered OPL");
                        }
                        applyProgramState(direct.state);
                        directWrites.clear();
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::gameplayLevel2DirectState
                        || effect.kind == SoundBlasterForegroundKind::gameplayLevel2EnemyDirectState) {
                        const auto enemy=effect.kind==SoundBlasterForegroundKind::gameplayLevel2EnemyDirectState;
                        if (effect.value != (enemy ? gameplayEnemyType17Event : gameplayPrimaryFireEvent) || !effect.directState
                            || !work.phase.sequenceSelection || !work.phase.sequenceSelection->published
                            || work.phase.sequenceSelection->songOrdinal != level2MusicIdentity
                            || !work.phase.sequenceSelection->level1DirectResetCompleted
                            || work.phase.sequenceSelection->prepared.sequence != level2MusicSequence_
                            || !work.phase.successAudio || work.phase.successAudio->stage != SuccessAudioStage::returned
                            || !successAudioRetained_ || !recoveredInitialization_ || !driverEnabled_ || !level1ResetCompleted_
                            || workSequenceSlot_ != recoveredLevel1Slot
                            || sequencers_[workSequenceSlot_].data != &level2MusicSequence_
                            || startupPhase_ != SoundBlasterStartupPhase::gameplay
                            || !sequencers_[workSequenceSlot_].allocated || !sequencers_[workSequenceSlot_].selected) {
                            throw std::invalid_argument("primary fire receipt requires the retained Level2 owner");
                        }
                        const auto direct = soundBlasterDirectAdvance(callbackWorkState(),
                            enemy ? gameplayEnemyType17DirectRecord : gameplayPrimaryFireDirectRecord,
                            enemy ? SoundBlasterDirectCaller::gameplayEnemyFire : SoundBlasterDirectCaller::gameplayPrimaryFire);
                        std::vector<std::uint16_t> expected;
                        for (const auto& block : direct.blocks) {
                            if (block.kind == SoundBlasterWorkKind::registerWrite) {
                                expected.push_back(static_cast<std::uint16_t>((block.registerNumber << 8) | block.value));
                            }
                        }
                        if (!sameCallbackState(direct.state, *effect.directState) || directWrites != expected) {
                            throw std::invalid_argument("primary fire receipt differs from live driver state or ordered OPL");
                        }
                        applyProgramState(direct.state);
                        directWrites.clear();
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::gameplayLevel2TargetHitDirectState
                        || effect.kind == SoundBlasterForegroundKind::failureLevel2PlayerHitDirectState) {
                        const auto failure=effect.kind==SoundBlasterForegroundKind::failureLevel2PlayerHitDirectState;
                        if ((effect.value != gameplayTargetImpactEvent && effect.value != gameplayPlayerHitEvent) || !effect.directState
                            || (failure && effect.value!=gameplayPlayerHitEvent)
                            || !work.phase.sequenceSelection || !work.phase.sequenceSelection->published
                            || work.phase.sequenceSelection->songOrdinal != level2MusicIdentity
                            || !work.phase.sequenceSelection->level1DirectResetCompleted
                            || work.phase.sequenceSelection->prepared.sequence != level2MusicSequence_
                            || !work.phase.successAudio || work.phase.successAudio->stage != SuccessAudioStage::returned
                            || !successAudioRetained_ || !recoveredInitialization_ || !driverEnabled_ || !level1ResetCompleted_
                            || workSequenceSlot_ != recoveredLevel1Slot
                            || sequencers_[workSequenceSlot_].data != &level2MusicSequence_
                            || startupPhase_ != SoundBlasterStartupPhase::gameplay
                            || !sequencers_[workSequenceSlot_].allocated || !sequencers_[workSequenceSlot_].selected) {
                            throw std::invalid_argument("target hit receipt requires the retained Level2 owner");
                        }
                        const bool playerHit = effect.value == gameplayPlayerHitEvent;
                        const auto direct = soundBlasterDirectAdvance(callbackWorkState(),
                            playerHit ? gameplayPlayerHitDirectRecord : gameplayTargetImpactDirectRecord,
                            playerHit ? SoundBlasterDirectCaller::gameplayPlayerHit : SoundBlasterDirectCaller::gameplayTargetImpact);
                        std::vector<std::uint16_t> expected;
                        for (const auto& block : direct.blocks) {
                            if (block.kind == SoundBlasterWorkKind::registerWrite) {
                                expected.push_back(static_cast<std::uint16_t>((block.registerNumber << 8) | block.value));
                            }
                        }
                        if (!sameCallbackState(direct.state, *effect.directState) || directWrites != expected) {
                            throw std::invalid_argument("target hit receipt differs from live driver state or ordered OPL");
                        }
                        applyProgramState(direct.state);
                        directWrites.clear();
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::gameplayEnemyDirectState) {
                        if (effect.value != gameplayEnemyType17Event || !effect.directState
                            || !work.phase.sequenceSelection || !work.phase.sequenceSelection->published
                            || work.phase.sequenceSelection->songOrdinal != 2
                            || work.phase.sequenceSelection->prepared.sequence != musicSequence_
                            || !recoveredInitialization_ || !driverEnabled_ || !level1ResetCompleted_
                            || workSequenceSlot_ != recoveredLevel1Slot
                            || sequencers_[workSequenceSlot_].data != &musicSequence_
                            || startupPhase_ != SoundBlasterStartupPhase::gameplay
                            || !sequencers_[workSequenceSlot_].allocated || !sequencers_[workSequenceSlot_].selected) {
                            throw std::invalid_argument("enemy fire receipt requires the retained Level1 owner");
                        }
                        const auto direct = soundBlasterDirectAdvance(callbackWorkState(), gameplayEnemyType17DirectRecord,
                            SoundBlasterDirectCaller::gameplayEnemyFire);
                        std::vector<std::uint16_t> expected;
                        for (const auto& block : direct.blocks) {
                            if (block.kind == SoundBlasterWorkKind::registerWrite) {
                                expected.push_back(static_cast<std::uint16_t>((block.registerNumber << 8) | block.value));
                            }
                        }
                        if (!sameCallbackState(direct.state, *effect.directState) || directWrites != expected) {
                            throw std::invalid_argument("enemy fire receipt differs from live driver state or ordered OPL");
                        }
                        applyProgramState(direct.state);
                        directWrites.clear();
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::gameplayEnemyType20DirectState
                        || effect.kind == SoundBlasterForegroundKind::gameplayLevel2EnemyType20DirectState) {
                        const auto level2=effect.kind==SoundBlasterForegroundKind::gameplayLevel2EnemyType20DirectState;
                        if (effect.value != gameplayEnemyType20Event || !effect.directState
                            || !work.phase.sequenceSelection || !work.phase.sequenceSelection->published
                            || work.phase.sequenceSelection->songOrdinal != (level2 ? level2MusicIdentity : 2) || !work.phase.sequenceSelection->level1DirectResetCompleted
                            || work.phase.sequenceSelection->prepared.sequence != (level2 ? level2MusicSequence_ : musicSequence_)
                            || (level2 && (!work.phase.successAudio || work.phase.successAudio->stage!=SuccessAudioStage::returned
                                || !successAudioRetained_))
                            || !programState_ || !programState_->enemy20CallerWindow || sequences_.empty()
                            || *programState_->enemy20CallerWindow != level1Enemy20CallerWindow(sequences_[0])
                            || !recoveredInitialization_ || !driverEnabled_ || !level1ResetCompleted_
                            || workSequenceSlot_ != recoveredLevel1Slot
                            || sequencers_[workSequenceSlot_].data != (level2 ? &level2MusicSequence_ : &musicSequence_)
                            || startupPhase_ != SoundBlasterStartupPhase::gameplay
                            || !sequencers_[workSequenceSlot_].allocated || !sequencers_[workSequenceSlot_].selected) {
                            throw std::invalid_argument("enemy fire receipt requires the retained Level1 owner");
                        }
                        const auto direct = soundBlasterDirectAdvance(callbackWorkState(), gameplayEnemyType20DirectRecord,
                            SoundBlasterDirectCaller::gameplayEnemyType20);
                        std::vector<std::uint16_t> expected;
                        for (const auto& block : direct.blocks) {
                            if (block.kind == SoundBlasterWorkKind::registerWrite) {
                                expected.push_back(static_cast<std::uint16_t>((block.registerNumber << 8) | block.value));
                            }
                        }
                        if (!sameCallbackState(direct.state, *effect.directState) || directWrites != expected) {
                            throw std::invalid_argument("enemy fire receipt differs from live driver state or ordered OPL");
                        }
                        applyProgramState(direct.state);
                        directWrites.clear();
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::gameplayPlayerHitDirectState) {
                        if (effect.value != gameplayPlayerHitEvent || !effect.directState
                            || !work.phase.sequenceSelection || work.phase.sequenceSelection->songOrdinal != 2
                            || work.phase.sequenceSelection->prepared.sequence != musicSequence_
                            || !recoveredInitialization_ || !driverEnabled_ || !level1ResetCompleted_
                            || workSequenceSlot_ != recoveredLevel1Slot
                            || sequencers_[workSequenceSlot_].data != &musicSequence_
                            || startupPhase_ != SoundBlasterStartupPhase::gameplay
                            || !sequencers_[workSequenceSlot_].allocated || !sequencers_[workSequenceSlot_].selected) {
                            throw std::invalid_argument("player hit receipt requires the retained Level1 owner");
                        }
                        const auto direct = soundBlasterDirectAdvance(callbackWorkState(), gameplayPlayerHitDirectRecord,
                            SoundBlasterDirectCaller::gameplayPlayerHit);
                        std::vector<std::uint16_t> expected;
                        for (const auto& block : direct.blocks) {
                            if (block.kind == SoundBlasterWorkKind::registerWrite) {
                                expected.push_back(static_cast<std::uint16_t>((block.registerNumber << 8) | block.value));
                            }
                        }
                        if (!sameCallbackState(direct.state, *effect.directState) || directWrites != expected) {
                            throw std::invalid_argument("player hit receipt differs from live driver state, volume row or ordered OPL");
                        }
                        applyProgramState(direct.state);
                        directWrites.clear();
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::gameplayTargetImpactDirectState) {
                        if (effect.value != gameplayTargetImpactEvent || !effect.directState
                            || !work.phase.sequenceSelection || work.phase.sequenceSelection->songOrdinal != 2
                            || work.phase.sequenceSelection->prepared.sequence != musicSequence_
                            || !recoveredInitialization_ || !driverEnabled_ || !level1ResetCompleted_
                            || workSequenceSlot_ != recoveredLevel1Slot
                            || sequencers_[workSequenceSlot_].data != &musicSequence_
                            || startupPhase_ != SoundBlasterStartupPhase::gameplay
                            || !sequencers_[workSequenceSlot_].allocated || !sequencers_[workSequenceSlot_].selected) {
                            throw std::invalid_argument("target impact receipt requires the retained Level1 owner");
                        }
                        const auto direct = soundBlasterDirectAdvance(callbackWorkState(), gameplayTargetImpactDirectRecord,
                            SoundBlasterDirectCaller::gameplayTargetImpact);
                        std::vector<std::uint16_t> expected;
                        for (const auto& block : direct.blocks) {
                            if (block.kind == SoundBlasterWorkKind::registerWrite) {
                                expected.push_back(static_cast<std::uint16_t>((block.registerNumber << 8) | block.value));
                            }
                        }
                        if (!sameCallbackState(direct.state, *effect.directState) || directWrites != expected) {
                            throw std::invalid_argument("target impact receipt differs from live driver state or ordered OPL");
                        }
                        applyProgramState(direct.state);
                        directWrites.clear();
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::gameplaySystemBonusDirectState) {
                        if (effect.value != gameplaySystemBonusEvent || !effect.directState
                            || !recoveredInitialization_ || !driverEnabled_ || !level1ResetCompleted_
                            || workSequenceSlot_ != recoveredLevel1Slot
                            || startupPhase_ != SoundBlasterStartupPhase::gameplay
                            || !sequencers_[workSequenceSlot_].allocated || !sequencers_[workSequenceSlot_].selected) {
                            throw std::invalid_argument("system bonus receipt requires the retained Level1 owner");
                        }
                        const auto direct = soundBlasterDirectAdvance(callbackWorkState(), gameplaySystemBonusDirectRecord,
                            SoundBlasterDirectCaller::gameplaySystemBonus);
                        std::vector<std::uint16_t> expected;
                        for (const auto& block : direct.blocks) {
                            if (block.kind == SoundBlasterWorkKind::registerWrite) {
                                expected.push_back(static_cast<std::uint16_t>((block.registerNumber << 8) | block.value));
                            }
                        }
                        if (!sameCallbackState(direct.state, *effect.directState) || directWrites != expected) {
                            throw std::invalid_argument("system bonus receipt differs from live driver state or ordered OPL");
                        }
                        applyProgramState(direct.state);
                        directWrites.clear();
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::gameplayLevel2ArrivalStopState) {
                        if (effect.value != gameplayArrivalEvent || !effect.directState || !directWrites.empty()
                            || !recoveredInitialization_ || !driverEnabled_ || !level1ResetCompleted_
                            || !work.phase.sequenceSelection || !work.phase.sequenceSelection->published
                            || work.phase.sequenceSelection->songOrdinal != level2MusicIdentity
                            || !work.phase.sequenceSelection->level1DirectResetCompleted
                            || work.phase.sequenceSelection->prepared.sequence != level2MusicSequence_
                            || !work.phase.successAudio || work.phase.successAudio->stage != SuccessAudioStage::returned
                            || !successAudioRetained_ || workSequenceSlot_ != recoveredLevel1Slot
                            || sequencers_[workSequenceSlot_].data != &level2MusicSequence_
                            || startupPhase_ != SoundBlasterStartupPhase::gameplay
                            || !sequencers_[workSequenceSlot_].allocated || !sequencers_[workSequenceSlot_].selected
                            || !sameCallbackState(callbackWorkState(),*effect.directState)) {
                            throw std::invalid_argument("arrival stop receipt differs from its retained Level2 owner");
                        }
                        previousCycle=effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::gameplayArrivalDirectState
                        || effect.kind == SoundBlasterForegroundKind::gameplayLevel2ArrivalDirectState) {
                        const auto level2=effect.kind==SoundBlasterForegroundKind::gameplayLevel2ArrivalDirectState;
                        if (effect.value != gameplayArrivalEvent || !effect.directState
                            || !recoveredInitialization_ || !driverEnabled_ || !level1ResetCompleted_
                            || (level2 ? (!work.phase.sequenceSelection || !work.phase.sequenceSelection->published
                                || work.phase.sequenceSelection->songOrdinal != level2MusicIdentity
                                || !work.phase.sequenceSelection->level1DirectResetCompleted
                                || work.phase.sequenceSelection->prepared.sequence != level2MusicSequence_
                                || sequencers_[recoveredLevel1Slot].data != &level2MusicSequence_
                                || !work.phase.successAudio || work.phase.successAudio->stage!=SuccessAudioStage::returned
                                || !successAudioRetained_) : sequencers_[recoveredLevel1Slot].data != &musicSequence_)
                            || workSequenceSlot_ != recoveredLevel1Slot
                            || startupPhase_ != SoundBlasterStartupPhase::gameplay
                            || !sequencers_[workSequenceSlot_].allocated || !sequencers_[workSequenceSlot_].selected) {
                            throw std::invalid_argument("arrival receipt requires its retained campaign owner");
                        }
                        const auto direct = soundBlasterDirectAdvance(callbackWorkState(), gameplayArrivalDirectRecord,
                            SoundBlasterDirectCaller::gameplayArrival);
                        std::vector<std::uint16_t> expected;
                        for (const auto& block : direct.blocks) {
                            if (block.kind == SoundBlasterWorkKind::registerWrite) {
                                expected.push_back(static_cast<std::uint16_t>((block.registerNumber << 8) | block.value));
                            }
                        }
                        if (!sameCallbackState(direct.state, *effect.directState) || directWrites != expected) {
                            throw std::invalid_argument("arrival receipt differs from live driver state or ordered OPL");
                        }
                        applyProgramState(direct.state);
                        directWrites.clear();
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::gameplayGuardianDefeatDirectState
                        || effect.kind == SoundBlasterForegroundKind::gameplayLevel2GuardianDefeatDirectState) {
                        const auto level2=effect.kind==SoundBlasterForegroundKind::gameplayLevel2GuardianDefeatDirectState;
                        if (effect.value != gameplayGuardianDefeatEvent || !effect.directState
                            || !recoveredInitialization_ || !driverEnabled_ || !level1ResetCompleted_
                            || (level2 ? (!work.phase.sequenceSelection || !work.phase.sequenceSelection->published
                                || work.phase.sequenceSelection->songOrdinal != level2MusicIdentity
                                || !work.phase.sequenceSelection->level1DirectResetCompleted
                                || work.phase.sequenceSelection->prepared.sequence != level2MusicSequence_
                                || sequencers_[recoveredLevel1Slot].data != &level2MusicSequence_
                                || !work.phase.successAudio || work.phase.successAudio->stage!=SuccessAudioStage::returned
                                || !successAudioRetained_) : sequencers_[recoveredLevel1Slot].data != &musicSequence_)
                            || workSequenceSlot_ != recoveredLevel1Slot
                            || startupPhase_ != SoundBlasterStartupPhase::gameplay
                            || !sequencers_[workSequenceSlot_].allocated || !sequencers_[workSequenceSlot_].selected) {
                            throw std::invalid_argument("guardian defeat receipt requires its retained campaign owner");
                        }
                        const auto direct = soundBlasterDirectAdvance(callbackWorkState(), gameplayGuardianDefeatDirectRecord,
                            SoundBlasterDirectCaller::gameplayGuardianDefeat);
                        std::vector<std::uint16_t> expected;
                        for (const auto& block : direct.blocks) {
                            if (block.kind == SoundBlasterWorkKind::registerWrite) {
                                expected.push_back(static_cast<std::uint16_t>((block.registerNumber << 8) | block.value));
                            }
                        }
                        if (!sameCallbackState(direct.state, *effect.directState) || directWrites != expected) {
                            throw std::invalid_argument("guardian defeat receipt differs from live driver state or ordered OPL");
                        }
                        applyProgramState(direct.state);
                        directWrites.clear();
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::gameplaySolidContactDirectState
                        || effect.kind == SoundBlasterForegroundKind::gameplayLevel2SolidContactDirectState) {
                        const auto level2=effect.kind==SoundBlasterForegroundKind::gameplayLevel2SolidContactDirectState;
                        if (effect.value != gameplaySolidContactEvent || !effect.directState
                            || !recoveredInitialization_ || !driverEnabled_ || !level1ResetCompleted_
                            || !work.phase.sequenceSelection || !work.phase.sequenceSelection->published
                            || work.phase.sequenceSelection->songOrdinal != (level2 ? level2MusicIdentity : 2)
                            || !work.phase.sequenceSelection->level1DirectResetCompleted
                            || work.phase.sequenceSelection->prepared.sequence != (level2 ? level2MusicSequence_ : musicSequence_)
                            || sequencers_[recoveredLevel1Slot].data != (level2 ? &level2MusicSequence_ : &musicSequence_)
                            || (level2 && (!work.phase.successAudio || work.phase.successAudio->stage!=SuccessAudioStage::returned
                                || !successAudioRetained_))
                            || workSequenceSlot_ != recoveredLevel1Slot
                            || startupPhase_ != SoundBlasterStartupPhase::gameplay
                            || !sequencers_[workSequenceSlot_].allocated || !sequencers_[workSequenceSlot_].selected) {
                            throw std::invalid_argument("solid contact receipt requires the retained Level1 owner");
                        }
                        const auto direct = soundBlasterDirectAdvance(callbackWorkState(), gameplaySolidContactDirectRecord,
                            SoundBlasterDirectCaller::gameplaySolidContact);
                        std::vector<std::uint16_t> expected;
                        for (const auto& block : direct.blocks) {
                            if (block.kind == SoundBlasterWorkKind::registerWrite) {
                                expected.push_back(static_cast<std::uint16_t>((block.registerNumber << 8) | block.value));
                            }
                        }
                        if (!sameCallbackState(direct.state, *effect.directState) || directWrites != expected) {
                            throw std::invalid_argument("solid contact receipt differs from live driver state or ordered OPL");
                        }
                        applyProgramState(direct.state);
                        directWrites.clear();
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::gameplayPlayerCombatDirectState) {
                        if (effect.value != gameplayPlayerCombatEvent || !effect.directState
                            || !work.phase.sequenceSelection || !work.phase.sequenceSelection->published
                            || work.phase.sequenceSelection->songOrdinal != 2
                            || !work.phase.sequenceSelection->level1DirectResetCompleted
                            || work.phase.sequenceSelection->prepared.sequence != musicSequence_
                            || sequencers_[recoveredLevel1Slot].data != &musicSequence_
                            || !recoveredInitialization_ || !driverEnabled_ || !level1ResetCompleted_
                            || workSequenceSlot_ != recoveredLevel1Slot
                            || startupPhase_ != SoundBlasterStartupPhase::gameplay
                            || !sequencers_[workSequenceSlot_].allocated || !sequencers_[workSequenceSlot_].selected) {
                            throw std::invalid_argument("player combat receipt requires the retained Level1 owner");
                        }
                        const auto direct = soundBlasterDirectAdvance(callbackWorkState(), gameplayPlayerCombatDirectRecord,
                            SoundBlasterDirectCaller::gameplayPlayerCombat);
                        std::vector<std::uint16_t> expected;
                        for (const auto& block : direct.blocks) {
                            if (block.kind == SoundBlasterWorkKind::registerWrite) {
                                expected.push_back(static_cast<std::uint16_t>((block.registerNumber << 8) | block.value));
                            }
                        }
                        if (!sameCallbackState(direct.state, *effect.directState) || directWrites != expected) {
                            throw std::invalid_argument("player combat receipt differs from live driver state or ordered OPL");
                        }
                        applyProgramState(direct.state);
                        directWrites.clear();
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::gameplayLevel2PlayerCombatDirectState) {
                        if (effect.value != gameplayPlayerCombatEvent || !effect.directState
                            || !work.phase.sequenceSelection || !work.phase.sequenceSelection->published
                            || work.phase.sequenceSelection->songOrdinal != level2MusicIdentity
                            || !work.phase.sequenceSelection->level1DirectResetCompleted
                            || work.phase.sequenceSelection->prepared.sequence != level2MusicSequence_
                            || !work.phase.successAudio || work.phase.successAudio->stage != SuccessAudioStage::returned
                            || !successAudioRetained_ || !recoveredInitialization_ || !driverEnabled_ || !level1ResetCompleted_
                            || sequencers_[recoveredLevel1Slot].data != &level2MusicSequence_
                            || workSequenceSlot_ != recoveredLevel1Slot
                            || startupPhase_ != SoundBlasterStartupPhase::gameplay
                            || !sequencers_[workSequenceSlot_].allocated || !sequencers_[workSequenceSlot_].selected) {
                            throw std::invalid_argument("player combat receipt requires the retained Level2 owner");
                        }
                        const auto direct = soundBlasterDirectAdvance(callbackWorkState(), gameplayPlayerCombatDirectRecord,
                            SoundBlasterDirectCaller::gameplayPlayerCombat);
                        std::vector<std::uint16_t> expected;
                        for (const auto& block : direct.blocks) {
                            if (block.kind == SoundBlasterWorkKind::registerWrite) {
                                expected.push_back(static_cast<std::uint16_t>((block.registerNumber << 8) | block.value));
                            }
                        }
                        if (!sameCallbackState(direct.state, *effect.directState) || directWrites != expected) {
                            throw std::invalid_argument("player combat receipt differs from live driver state or ordered OPL");
                        }
                        applyProgramState(direct.state);
                        directWrites.clear();
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (effect.kind == SoundBlasterForegroundKind::sequencePublish) {
                        if (!recoveredInitialization_ || !driverEnabled_ || effect.value != workSequenceSlot_
                            || !sequencers_[workSequenceSlot_].allocated || sequencers_[workSequenceSlot_].selected || !work.phase.sequenceSelection
                            || !sameCallbackState(buildCallbackWorkState(SoundBlasterCallbackDomain::sequence),
                                work.phase.sequenceSelection->prepared)) {
                            throw std::invalid_argument("source current publication requires the prepared logo handle");
                        }
                        sequencers_[workSequenceSlot_].selected = true;
                        startupPhase_ = workSequenceSlot_ == recoveredLogoSlot ? SoundBlasterStartupPhase::logoDelay
                            : workSequenceSlot_ == recoveredPresentationSlot ? SoundBlasterStartupPhase::presentation
                            : SoundBlasterStartupPhase::gameplay;
                        previousCycle = effect.cycleOffset;
                        continue;
                    }
                    if (!recoveredInitialization_ || driverEnabled_) {
                        throw std::invalid_argument("foreground initialization requires the staged driver");
                    }
                    if (effect.kind == SoundBlasterForegroundKind::oplWrite) {
                        if (initializationOplWrites_ >= soundBlasterInitializationWriteCount) {
                            throw std::invalid_argument("foreground OPL initialization repeats");
                        }
                        const OplRegisterWrite physical{static_cast<std::uint8_t>(effect.value >> 8),
                            static_cast<std::uint8_t>(effect.value)};
                        write(physical.registerNumber, physical.value);
                        output.writes.push_back({effect.cycleOffset, physical});
                        ++initializationOplWrites_;
                    } else if (effect.kind == SoundBlasterForegroundKind::driverEnable
                        && effect.value == 1 && initializationOplWrites_ == soundBlasterInitializationWriteCount) {
                        driverEnabled_ = true;
                    } else {
                        throw std::invalid_argument("invalid source driver enable effect");
                    }
                    previousCycle = effect.cycleOffset;
                }
            };
            for (const auto& callback : work.callbacks) {
                applyForeground(callback.cycleOffset);
                if (!directWrites.empty()) {
                    throw std::invalid_argument("callback interrupts an unpublished masked direct event");
                }
                if (callback.cycleOffset < 0 || callback.cycleOffset < previousCycle
                    || callback.cycleOffset > work.total.elapsedCycles) {
                    throw std::invalid_argument("invalid callback order in normal work PCM timeline");
                }
                const auto callbackState = callbackWorkState();
                if (!work.phase.playableCampaign && !playableStartupProgram(work.phase)
                    && callbackState.domain == SoundBlasterCallbackDomain::level2RetryNoHandle
                    && (!work.phase.level2RetryMusic || !work.phase.sequenceSelection
                        || work.phase.sequenceSelection->songOrdinal != level2MusicIdentity
                        || work.phase.sequenceSelection->prepared.sequence != level2MusicSequence_
                        || (work.phase.level2RetryMusic->stage != AudioClockLevel2RetryMusicStage::stopped
                            && work.phase.level2RetryMusic->stage != AudioClockLevel2RetryMusicStage::emptyStopped
                            && work.phase.level2RetryMusic->stage != AudioClockLevel2RetryMusicStage::resourceBound
                            && work.phase.level2RetryMusic->stage != AudioClockLevel2RetryMusicStage::playing))) {
                    throw std::invalid_argument("Level2 retry callback lost its explicit retained source stage");
                }
                const bool retainedMenu = workSequenceSlot_ == recoveredPresentationSlot
                    && sequencers_[workSequenceSlot_].data == &presentationSequence_
                    && retainedMenuPrediction(work.phase,callbackState);
                const bool retainedLevel1 = workSequenceSlot_ == recoveredLevel1Slot && level1ResetCompleted_
                    && sequencers_[workSequenceSlot_].data == &musicSequence_
                    && callbackState.domain == SoundBlasterCallbackDomain::sequence && callbackState.program;
                const bool retainedLevel2 = workSequenceSlot_ == recoveredLevel1Slot && level1ResetCompleted_
                    && sequencers_[workSequenceSlot_].data == &level2MusicSequence_
                    && callbackState.domain == SoundBlasterCallbackDomain::sequence && callbackState.program
                    && work.phase.sequenceSelection && work.phase.sequenceSelection->published
                    && work.phase.sequenceSelection->songOrdinal == level2MusicIdentity
                    && work.phase.sequenceSelection->level1DirectResetCompleted
                    && work.phase.sequenceSelection->prepared.sequence == level2MusicSequence_
                    && work.phase.successAudio && work.phase.successAudio->stage == SuccessAudioStage::returned
                    && successAudioRetained_;
                const auto predicted = (work.phase.playableCampaign || playableStartupProgram(work.phase))
                    ? soundBlasterPlayableCallbackAdvance(callbackState)
                    : retainedMenu ? soundBlasterMenuCallbackAdvance(callbackState)
                    : retainedLevel1 ? soundBlasterLevel1CallbackAdvance(callbackState)
                    : retainedLevel2 ? soundBlasterLevel2CallbackAdvance(callbackState)
                    : soundBlasterCallbackAdvance(callbackState);
                if (!sameCallbackState(predicted.state, callback.advance.state) || predicted.kind != callback.advance.kind) {
                    throw std::invalid_argument("work callback differs from owned live sequence state");
                }
                const auto firstWrite = registerWrites_.size();
                const bool program = predicted.kind == SoundBlasterCallbackKind::programControl
                    || predicted.kind == SoundBlasterCallbackKind::programWait
                    || predicted.kind == SoundBlasterCallbackKind::successNoHandle
                    || predicted.kind == SoundBlasterCallbackKind::level2RetryNoHandle;
                if (program) {
                    applyProgramCallback(predicted);
                } else {
                    advanceDirectEvents();
                }
                if (predicted.kind != SoundBlasterCallbackKind::noHandle
                    && !program) {
                    advanceSequenceTick(0);
                }
                if (!sameCallbackState(callbackWorkState(), predicted.state)
                    || registerWrites_.size() - firstWrite != callback.timing.opl.size()
                    || callback.timing.oplEffects.size() != callback.timing.opl.size()) {
                    throw std::invalid_argument("live decoder output outside recovered callback work");
                }
                std::size_t writer = 0;
                for (const auto& block : predicted.blocks) {
                    if (block.kind == SoundBlasterWorkKind::plain) {
                        continue;
                    }
                    const auto& write = registerWrites_[firstWrite + writer];
                    const auto cycle = callback.timing.oplEffects[writer].elapsedCycles;
                    const auto voice = static_cast<std::size_t>(block.voice);
                    const auto carrier = block.kind == SoundBlasterWorkKind::registerWrite ? 0
                        : operatorOffsets[voiceOperators[voice * 2 + 1]];
                    const auto expectedRegister = block.kind == SoundBlasterWorkKind::registerWrite ? block.registerNumber
                        : block.kind == SoundBlasterWorkKind::carrierLevel ? 0x40 + carrier
                        : (block.kind == SoundBlasterWorkKind::frequencyLow ? 0xa0 : 0xb0) + voice;
                    if (write.registerNumber != expectedRegister || cycle < callback.cycleOffset
                        || cycle < previousCycle || cycle > work.total.elapsedCycles
                        || (block.kind == SoundBlasterWorkKind::registerWrite && write.value != block.value)) {
                        throw std::invalid_argument("live OPL order outside recovered work boundaries");
                    }
                    output.writes.push_back({cycle, write});
                    previousCycle = cycle;
                    ++writer;
                }
            }
            applyForeground(work.total.elapsedCycles);
            if (!directWrites.empty()) {
                throw std::invalid_argument("direct event did not publish its retained state");
            }
            if (work.phase.soundBlasterInitialization
                && (work.phase.soundBlasterInitialization->driverEnabled != driverEnabled_
                    || work.phase.soundBlasterInitialization->oplWrites != initializationOplWrites_)) {
                throw std::invalid_argument("initialization return differs from the owned driver");
            }
            if (!callbackInactive
                && (!work.phase.callback || !sameCallbackState(callbackWorkState(), *work.phase.callback))) {
                throw std::invalid_argument("work return differs from owned live sequence state");
            }
        } catch (...) {
            sequencers_ = savedSequencers;
            workSequenceSlot_ = savedWorkSequenceSlot;
            successAudioRetained_ = savedSuccessRetained;
            level1ResetCompleted_ = savedLevelReset;
            directResetWordsD5_ = savedResetD5;
            directResetWordsEb_ = savedResetEb;
            voices_ = savedVoices;
            voiceOwners_ = savedOwners;
            channelVoiceCursor_ = savedCursors;
            directDurations_ = savedTimers;
            directLevelLatch_ = savedLatch;
            driverEnabled_ = savedDriverEnabled;
            initializationOplWrites_ = savedInitializationWrites;
            startupPhase_ = savedStartupPhase;
            programState_ = savedProgram;
            recoveredParserStatus_ = savedParserStatus;
            musicFade_ = savedFade;
            fadeDivider_ = savedFadeDivider;
            programTuning_ = savedProgramTuning;
            voiceLevels_ = savedLevels;
            channelEnabled_ = savedEnabled;
            channelDeferrals_ = savedDeferrals;
            registerWrites_.resize(writeStart);
            deferOplWrites_ = false;
            throw;
        }
        deferOplWrites_ = false;
        output.samples.reserve(static_cast<std::size_t>(sampleCount));
        const auto renderUntil = [&](const std::uint64_t sample) {
            const auto part = renderOutputSamples(static_cast<std::size_t>(sample) - output.samples.size());
            output.samples.insert(output.samples.end(), part.begin(), part.end());
        };
        std::size_t voiceCallback=0;
        const auto serviceVoiceUntil=[&](const std::int64_t cycle) {
            if (!work.phase.playableCampaign || !playableSampledVoice_) {return;}
            while (voiceCallback<work.callbacks.size() && work.callbacks[voiceCallback].cycleOffset<=cycle) {
                const auto offset=static_cast<std::uint64_t>(work.callbacks[voiceCallback].cycleOffset);
                renderUntil(clockedSamplesAt(originCycles+offset)-originSample);
                // Native DSP service is placed at the retained driver callback;
                // its instruction and DMA interrupt latency remain unmeasured.
                sampledVoice_.service();
                ++voiceCallback;
            }
        };
        for (const auto& write : output.writes) {
            serviceVoiceUntil(write.cycleOffset);
            renderUntil(clockedSamplesAt(originCycles + static_cast<std::uint64_t>(write.cycleOffset)) - originSample);
            opl2_.writeRegister(write.write.registerNumber, write.write.value);
        }
        serviceVoiceUntil(work.total.elapsedCycles);
        renderUntil(sampleCount);
        workCycles_ = endCycles;
        workClockActive_ = true;
        return output;
    }

private:
    void applyProgramWrite(const std::uint8_t reg, const std::uint8_t value) {
        write(reg, value);
        if (reg >= 0xa0 && reg < 0xa0 + voices_.size()) {
            voices_[reg - 0xa0].a0 = value;
        }
        if (reg >= 0xb0 && reg < 0xb0 + voices_.size()) {
            voices_[reg - 0xb0].b0 = value;
        }
    }

    void applyProgramCallback(const SoundBlasterCallbackAdvance& advance) {
        for (const auto& block : advance.blocks) {
            if (block.kind == SoundBlasterWorkKind::registerWrite) {
                applyProgramWrite(block.registerNumber, block.value);
            } else if (block.kind != SoundBlasterWorkKind::plain) {
                throw std::invalid_argument("unexpected program callback device effect");
            }
        }
        if (advance.kind == SoundBlasterCallbackKind::successNoHandle
            || advance.kind == SoundBlasterCallbackKind::level2RetryNoHandle) {
            applySuccessPhysicalState(advance.state);
        } else {
            applyProgramState(advance.state);
        }
    }

    void applySuccessPhysicalState(const SoundBlasterCallbackState& state) {
        const auto retained = sequencers_[workSequenceSlot_];
        applyProgramState(state);
        // No current handle:1C6B/17E5 alter physical driver state, not old handle cursor.
        sequencers_[workSequenceSlot_] = retained;
    }

    void applyProgramState(const SoundBlasterCallbackState& state) {
        if (!state.program || !programState_) {
            throw std::invalid_argument("program callback requires owned recovered driver storage");
        }
        if (state.program->enemy20CallerWindow != programState_->enemy20CallerWindow) {
            throw std::invalid_argument("program callback changed the readonly enemy20 caller resource window");
        }
        if (state.program->enemy20McbByte != programState_->enemy20McbByte) {
            throw std::invalid_argument("program callback changed the immutable enemy20 MCB byte");
        }
        if (state.program->bomb7CallerRow != programState_->bomb7CallerRow) {
            throw std::invalid_argument("program callback changed the retained Bomb menu-song generation");
        }
        if (state.program->playerHitVolumeRow != programState_->playerHitVolumeRow) {
            throw std::invalid_argument("program callback changed the immutable player hit volume row");
        }
        programState_ = state.program;
        voiceOwners_ = state.voices.channels;
        channelEnabled_ = state.channelEnabled;
        channelVoiceCursor_ = state.channelVoiceCursors;
        channelDeferrals_ = state.voices.channelDeferrals;
        directDurations_ = state.voices.directTimers;
        programTuning_ = state.voices.tuning;
        directLevelLatch_ = state.latch == SoundBlasterLevelLatch::Latched;
        musicFade_ = state.fade;
        fadeDivider_ = state.program->fadeDivider;
        auto& sequence = sequencers_[workSequenceSlot_];
        sequence.position = state.position;
        sequence.delay = state.wait;
        recoveredParserStatus_ = state.runningStatus;
        sequence.active = state.active;
        sequence.playback = state.program->repeatCount == 0xffff ? Playback::repeat : Playback::once;
        sequence.loopPosition = state.program->loopMarker + 2; // Source offsets exclude the84 wrapper.
        for (std::size_t voice = 0; voice < voices_.size(); ++voice) {
            auto& physical = voices_[voice];
            physical.note = state.voices.notes[voice];
            physical.velocity = state.velocities[voice];
            physical.active = state.sounding[voice] != 0;
            physical.sequencer = workSequenceSlot_;
            physical.program = state.program->voices[voice].program;
            auto& levels = voiceLevels_[voice];
            const auto& source = state.program->voices[voice];
            levels.carrierKsl = static_cast<std::uint8_t>(source.carrierScale << 6);
            levels.carrierFallback = static_cast<std::uint8_t>(source.carrierFallback);
            levels.operatorKsl = static_cast<std::uint8_t>(source.operatorScale << 6);
            levels.operatorFallback = static_cast<std::uint8_t>(source.operatorFallback);
            levels.operatorEnabled = source.operatorDisabled == 0;
        }
    }

    void requireInitialized() const {
        if (!driverEnabled_) {
            throw std::invalid_argument("Sound Blaster driver has not completed source initialization");
        }
    }

    void serviceDueSampleInterrupt() {
        sequenceSamplePhase_ -= sequencePhasePeriod;
        // Native0494 skips the driver while an ended one-shot handle
        // remains allocated; a released handle still ticks SFX.
        const auto& resident = sequencers_[0];
        if (sampledVoice_.resident()) {
            if (sampledVoice_.observation().phase != SampledVoicePhase::complete) {
                advanceDirectEvents();
                sampledVoice_.service();
            }
        } else if (!resident.allocated || resident.active
            || resident.playback == Playback::repeat) {
            advanceDirectEvents();
            for (std::size_t slot = 0; slot < sequencers_.size(); ++slot) {
                advanceSequenceTick(slot);
            }
        }
    }

    void requireNativeSampledVoice() const {
        if (workClockActive_ && !playableSampledVoice_) {
            throw std::invalid_argument("sampled voice requires native sample execution; strict DSP work is unqualified");
        }
    }

    std::vector<std::int16_t> renderOutputSamples(const std::size_t count) {
        auto samples = opl2_.render(count);
        if (sampledVoice_.observation().speakerEnabled) {
            sampledVoice_.mix(samples);
        }
        return samples;
    }

    // Original 271e PIT setup and captured IRQ callback reload, checkpoint208.
    static constexpr std::uint64_t pitInputClock = 1193182;
    static constexpr std::uint64_t pitDivisor = 0xffff / 13;
    static constexpr std::uint64_t audioCallbackReload = 4;
    static constexpr std::uint64_t sequencePhasePeriod =
        sampleRate * pitDivisor * audioCallbackReload;

    enum class Playback { once, repeat };

    struct Sequencer {
        const std::vector<std::uint8_t>* data = nullptr;
        std::size_t position = 0;
        std::size_t loopPosition = 0;
        Playback playback = Playback::once;
        std::uint8_t runningStatus = 0;
        std::uint16_t delay = 0;
        bool allocated = false;
        bool active = false;
        bool selected = false;
    };

    struct Voice {
        bool active = false;
        bool frequencyPending = false;
        std::size_t sequencer = 0;
        std::uint8_t channel = 0;
        std::uint8_t note = 0;
        std::uint8_t velocity = 0;
        std::uint8_t program = 0xff;
        std::uint8_t a0 = 0;
        std::uint8_t b0 = 0;
    };

    struct VoiceLevel {
        std::uint8_t carrierKsl = 0;
        std::uint8_t carrierFallback = 0;
        std::uint8_t operatorKsl = 0;
        std::uint8_t operatorFallback = 0;
        bool operatorEnabled = true;
    };

    void write(const std::uint8_t reg, const std::uint8_t value) {
        registerWrites_.push_back({reg, value});
        if (!deferOplWrites_) {
            opl2_.writeRegister(reg, value);
        }
    }

    void resetOpl() {
        opl2_.reset();
        // blastdrv first disables waveform select, initializes all 18
        // operators, clears the nine channels and waveform registers, then
        // enables waveform select.  This 158-write prefix is stable across
        // the independent target OPL captures and is part of the device
        // contract, rather than a generic OPL example reset.
        write(0x01, 0x00);

        static constexpr std::array<std::uint8_t, 18> initOffsets{
            0, 1, 2, 3, 4, 5,
            8, 9, 10, 11, 12, 13,
            16, 17, 18, 19, 20, 21,
        };
        for (std::size_t index = 0; index < initOffsets.size(); ++index) {
            const std::uint8_t offset = initOffsets[index];
            const bool modulator = (index % 6) < 3;
            write(0xbd, 0x00);
            write(0x08, 0x00);
            write(
                static_cast<std::uint8_t>(0x40 + offset),
                modulator ? 0x4f : 0x00
            );
            if (modulator) {
                write(
                    static_cast<std::uint8_t>(
                        0xc0 + (index / 6) * 3 + (index % 3)
                    ),
                    0x06
                );
            }
            write(
                static_cast<std::uint8_t>(0x60 + offset),
                modulator ? 0xf1 : 0xf2
            );
            write(
                static_cast<std::uint8_t>(0x80 + offset),
                modulator ? 0x53 : 0x74
            );
            write(
                static_cast<std::uint8_t>(0x20 + offset),
                modulator ? 0x01 : 0x11
            );
        }
        write(0xbd, 0x00);
        write(0xbd, 0x00);
        write(0x08, 0x00);
        for (std::uint8_t channel = 0; channel < 9; ++channel) {
            write(static_cast<std::uint8_t>(0xa0 + channel), 0);
            write(static_cast<std::uint8_t>(0xb0 + channel), 0);
        }
        for (const std::uint8_t offset : initOffsets) {
            write(static_cast<std::uint8_t>(0xe0 + offset), 0);
        }
        write(0x01, 0x20);
    }

    void startSequence(
        const std::size_t slot,
        const std::size_t sequenceIndex
    ) {
        if (
            slot >= sequencers_.size()
            || sequenceIndex >= sequences_.size()
            || sequences_[sequenceIndex].size() < 36
        ) {
            return;
        }
        startSequenceData(slot, &sequences_[sequenceIndex], Playback::once);
    }

    void startSequenceData(
        const std::size_t slot,
        const std::vector<std::uint8_t>* sequence,
        const Playback playback
    ) {
        requireInitialized();
        if (
            slot >= sequencers_.size()
            || sequence == nullptr
            || sequence->size() < 36
        ) {
            return;
        }
        const auto base = (*sequence)[0] == 0x84 ? 2U : 0U;
        if ((*sequence)[base] != 0 || sampledVoice_.resident()) {
            throw std::invalid_argument("OPL sequencer requires admitted type0 data without resident PCM");
        }
        releaseSequence(slot);
        configureVoiceOwners(*sequence);
        musicFade_ = 0;
        fadeDivider_ = fadeDividerReload; //197d/198c initializes driver globals.
        Sequencer& sequencer = sequencers_[slot];
        sequencer.data = sequence;
        sequencer.position = base + 34;
        sequencer.loopPosition = sequencer.position;
        sequencer.playback = playback;
        sequencer.runningStatus = 0;
        sequencer.delay = 0;
        sequencer.allocated = true;
        sequencer.active = true;
        sequencer.selected = true;
        // The resident PIT callback continues across sequence changes.
        // Native17e5 preserves its counter and the independent SFX timers.
    }

    void configureVoiceOwners(
        const std::vector<std::uint8_t>& sequence
    ) {
        // blastdrv 17e5:1821 clears the channel/voice state, then
        // 192d:1968 assigns physical voices sequentially from each SNG
        // descriptor whose device-selection flags contain bit 2.
        const std::size_t base = sequence[0] == 0x84 ? 2 : 0;
        const std::array<std::uint8_t, 3> retainedSpecialVelocities{
            voices_[6].velocity,
            voices_[7].velocity,
            voices_[8].velocity,
        };
        voices_ = {};
        for (auto& level : voiceLevels_) {
            //1821 resets KSL/enable but retains the independent fallback levels.
            level.carrierKsl = 0;
            level.operatorKsl = 0;
            level.operatorEnabled = true;
        }
        // blastdrv 17e5:184c initializes every physical voice's current
        // program to 0dh. A note using the default channel program therefore
        // reuses the resident patch instead of emitting a redundant refresh.
        for (Voice& voice : voices_) {
            voice.program = 0x0d;
        }
        for (std::size_t special = 0; special < 3; ++special) {
            voices_[6 + special].velocity =
                retainedSpecialVelocities[special];
        }
        voiceOwners_.fill(0xff);
        // Direct mode's1821/18af/1904 loops skip channels6,7 and15.
        for (std::size_t channel = 0; channel < channelVoiceCursor_.size(); ++channel) {
            if (std::find(retainedSequenceChannels.begin(), retainedSequenceChannels.end(), channel) != retainedSequenceChannels.end()) {
                continue;
            }
            channelVoiceCursor_[channel] = 0;
            channelEnabled_[channel] = 0;
            channelDeferrals_[channel] = 0;
        }
        workStateSupported_ = true;
        // Original206b arms three direct voices;1821 preserves their owners.
        for (std::size_t slot = 0; slot < directDurations_.size(); ++slot) {
            voiceOwners_[firstDirectVoice + slot] = static_cast<std::uint8_t>(firstDirectChannel + slot);
        }
        // 1821..1928 clears voice/sequence fields, but not the independent
        // CS:bf/c1/c3 direct-event timers. They can expire inside success.
        directLevelLatch_ = false;
        std::size_t voice = 0;
        for (std::size_t channel = 0; channel < 16; ++channel) {
            const std::size_t descriptor = base + 1 + channel * 2;
            if (descriptor + 1 >= sequence.size()) {
                break;
            }
            const std::uint8_t count =
                static_cast<std::uint8_t>(sequence[descriptor] & 0x7f);
            const std::uint8_t flags = sequence[descriptor + 1];
            if ((flags & 0x04) == 0) {
                continue;
            }
            for (
                std::uint8_t assigned = 0;
                assigned < count && voice < voiceOwners_.size();
                ++assigned
            ) {
                voiceOwners_[voice++] =
                    static_cast<std::uint8_t>(channel);
            }
        }
    }

    void silencePhysicalVoices() {
        // blastdrv 2da0:2dd9 calls its A0/B0 key-off helper for all nine
        // physical OPL voices.  Helper 2331 passes each retained note through
        // the 2605 frequency tables with velocity zero, including reset note
        // zero (57:01); it does not replay a cached hardware frequency.  The
        // sweep occurs at the FC end marker and again when operation 14
        // releases the ended handle; sounding state is not a loop condition.
        for (std::size_t voice = 0; voice < voices_.size(); ++voice) {
            keyOff(voice);
        }
    }

    void endSequence(const std::size_t slot) {
        if (slot >= sequencers_.size()) {
            return;
        }
        Sequencer& sequencer = sequencers_[slot];
        if (!sequencer.allocated || !sequencer.active) {
            return;
        }
        silencePhysicalVoices();
        sequencer.active = false;
        sequencer.position = sequencer.loopPosition;
        sequencer.delay = 0;
    }

    void releaseSequence(const std::size_t slot) {
        requireInitialized();
        if (slot >= sequencers_.size()) {
            return;
        }
        if (sequencers_[slot].allocated) {
            silencePhysicalVoices();
        }
        sequencers_[slot] = {};
    }

    bool readMessage(
        Sequencer& sequencer,
        std::uint8_t& status,
        std::uint8_t& first,
        std::uint8_t& second,
        std::uint16_t& delay
    ) {
        if (
            !sequencer.active
            || sequencer.data == nullptr
            || sequencer.position >= sequencer.data->size()
        ) {
            return false;
        }
        const auto& data = *sequencer.data;
        std::uint8_t value = data[sequencer.position++];
        if (value == 0xfc) {
            return false;
        }
        if ((value & 0x80) != 0) {
            sequencer.runningStatus = value;
            status = value;
            if ((status & 0xf0) == 0xf0) {
                while (
                    sequencer.position < data.size()
                    && data[sequencer.position++] != 0xf7
                ) {
                }
                first = 0;
                second = 0;
            } else {
                if (sequencer.position >= data.size()) {
                    return false;
                }
                first = data[sequencer.position++];
                second = 0;
                if (
                    (status & 0xf0) != 0xc0
                    && (status & 0xf0) != 0xd0
                ) {
                    if (sequencer.position >= data.size()) {
                        return false;
                    }
                    second = data[sequencer.position++];
                }
            }
        } else {
            if (sequencer.runningStatus == 0) {
                return false;
            }
            status = sequencer.runningStatus;
            first = value;
            second = 0;
            if (
                (status & 0xf0) != 0xc0
                && (status & 0xf0) != 0xd0
            ) {
                if (sequencer.position >= data.size()) {
                    return false;
                }
                second = data[sequencer.position++];
            }
        }
        delay = 0;
        while (
            sequencer.position < data.size()
            && data[sequencer.position] == 0xf8
        ) {
            delay = static_cast<std::uint16_t>(delay + 240);
            ++sequencer.position;
        }
        if (
            sequencer.position >= data.size()
            || data[sequencer.position] == 0xfc
        ) {
            return false;
        }
        delay = static_cast<std::uint16_t>(
            delay + data[sequencer.position++]
        );
        return true;
    }

    void advanceSequenceTick(const std::size_t slot) {
        Sequencer& sequencer = sequencers_[slot];
        if (!sequencer.active) {
            if (!sequencer.allocated || sequencer.playback != Playback::repeat) {
                return;
            }
            sequencer.active = true;
        }
        if (sequencer.delay > 0) {
            --sequencer.delay;
            advanceMusicFade(slot);
            return;
        }
        while (sequencer.active) {
            std::uint8_t status = 0;
            std::uint8_t first = 0;
            std::uint8_t second = 0;
            std::uint16_t delay = 0;
            const auto messagePosition = sequencer.position;
            if (!readMessage(sequencer, status, first, second, delay)) {
                endSequence(slot);
                return;
            }
            if (status == 0xcf && first == 0x7f) {
                // Native1d2a stores the data-byte position minus one, even
                // with running status, and keeps voice state on restart.
                const bool explicitStatus = ((*sequencer.data)[messagePosition] & 0x80) != 0;
                sequencer.loopPosition = messagePosition - (explicitStatus ? 0 : 1);
            }
            applyMessage(slot, status, first, second);
            if (delay > 0) {
                sequencer.delay = static_cast<std::uint16_t>(delay - 1);
                flushPendingFrequencies(slot);
                advanceMusicFade(slot);
                return;
            }
        }
    }

    void flushPendingFrequencies(const std::size_t slot) {
        for (std::size_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
            Voice& voice = voices_[voiceIndex];
            if (!voice.active || !voice.frequencyPending
                || voice.sequencer != slot) {
                continue;
            }
            write(
                static_cast<std::uint8_t>(0xa0 + voiceIndex),
                voice.a0
            );
            write(
                static_cast<std::uint8_t>(0xb0 + voiceIndex),
                voice.b0
            );
            voice.frequencyPending = false;
        }
    }

    void applyMessage(
        const std::size_t slot,
        const std::uint8_t status,
        const std::uint8_t first,
        const std::uint8_t second
    ) {
        const std::uint8_t channel = status & 0x0f;
        switch (status & 0xf0) {
        case 0x80:
            noteOff(slot, channel, first);
            break;
        case 0x90:
            if (second == 0) {
                noteOff(slot, channel, first);
            } else {
                noteOn(slot, channel, first, second);
            }
            break;
        case 0xb0:
            if (first == channelEnableController) {
                channelEnabled_[channel] = second;
                if (second == 0) {
                    workStateSupported_ = false;
                }
            } else if (first == channelDeferralController) {
                channelDeferrals_[channel] = static_cast<std::uint8_t>(second >= deferralThreshold);
                workStateSupported_ = false;
            } else if (first == 0x4c || first == 0x60 || (first == 0x4b && second != 0)) {
                workStateSupported_ = false;
            }
            if (first == 0x4b && second == 0) {
                for (std::size_t voice = 0; voice < voices_.size(); ++voice) {
                    if (
                        voices_[voice].active
                        && voices_[voice].sequencer == slot
                        && voices_[voice].channel == channel
                    ) {
                        keyOff(voice);
                    }
                }
            }
            break;
        case 0xc0:
            if (channel != 0x0f && first < 96) {
                for (
                    std::size_t voice = 0;
                    voice < voices_.size();
                    ++voice
                ) {
                    if (voiceOwners_[voice] != channel) {
                        continue;
                    }
                    applyPatch(voice, first);
                    if (musicFade_ != 0) {
                        refreshVoiceLevels(voice);
                    } else {
                        setVoiceLevel(voice, first, voices_[voice].velocity);
                    }
                    voices_[voice].program = first;
                }
            }
            break;
        default:
            workStateSupported_ = false;
            break;
        }
    }

    std::size_t allocateVoice(const std::uint8_t channel) {
        // blastdrv 1dac:1e57 first takes an inactive voice owned by the
        // channel. If all of that channel's voices are active, it replaces
        // the next owned voice using the per-channel circular cursor.
        for (std::size_t voice = 0; voice < voices_.size(); ++voice) {
            if (
                voiceOwners_[voice] == channel
                && !voices_[voice].active
            ) {
                channelVoiceCursor_[channel] = static_cast<std::uint8_t>(
                    (voice + 1) % voices_.size()
                );
                return voice;
            }
        }
        const std::size_t cursor = channelVoiceCursor_[channel] % voices_.size();
        for (std::size_t distance = 0; distance < voices_.size(); ++distance) {
            const std::size_t voice = (cursor + distance) % voices_.size();
            if (voiceOwners_[voice] == channel) {
                keyOff(voice);
                channelVoiceCursor_[channel] = static_cast<std::uint8_t>(
                    (voice + 1) % voices_.size()
                );
                return voice;
            }
        }
        // Original 1e2c wrap scan visits voice0 even at cursor0; 1e4e then
        // advances once more. An unowned channel still retains that cursor.
        constexpr std::size_t unownedZeroCursorNext = 2;
        channelVoiceCursor_[channel] = static_cast<std::uint8_t>(
            cursor == 0 ? unownedZeroCursorNext : (cursor + 1) % voices_.size()
        );
        return voices_.size();
    }

    void applyPatch(
        const std::size_t voiceIndex,
        const std::uint8_t program
    ) {
        if (instrumentBank_.size() != 96 * 28 || program >= 96) {
            return;
        }
        const std::uint8_t* patch =
            instrumentBank_.data() + static_cast<std::size_t>(program) * 28;
        auto& level = voiceLevels_[voiceIndex];
        if (patch[12] != 0) {
            level.operatorEnabled = false;
        } else {
            level.operatorKsl = static_cast<std::uint8_t>(patch[0] << 6);
            level.operatorFallback = static_cast<std::uint8_t>(63 - patch[8]);
        }
        level.carrierKsl = static_cast<std::uint8_t>(patch[13] << 6);
        level.carrierFallback = static_cast<std::uint8_t>(63 - patch[21]);
        for (std::size_t part = 0; part < 2; ++part) {
            const std::uint8_t* op = patch + part * 13;
            const std::uint8_t operatorIndex =
                voiceOperators[voiceIndex * 2 + part];
            const std::uint8_t offset = operatorOffsets[operatorIndex];
            const std::uint8_t characteristics = static_cast<std::uint8_t>(
                (op[9] != 0 ? 0x80 : 0)
                | (op[10] != 0 ? 0x40 : 0)
                | (op[5] != 0 ? 0x20 : 0)
                | (op[11] != 0 ? 0x10 : 0)
                | (op[1] & 0x0f)
            );
            write(0xbd, 0x00);
            write(0x08, 0x00);
            write(
                static_cast<std::uint8_t>(0x40 + offset),
                static_cast<std::uint8_t>(
                    ((op[0] & 0x03) << 6) | (op[8] & 0x3f)
                )
            );
            if (part == 0) {
                write(
                    static_cast<std::uint8_t>(0xc0 + voiceIndex),
                    static_cast<std::uint8_t>(
                        ((patch[2] & 0x07) << 1)
                        | (patch[12] == 0 ? 1 : 0)
                    )
                );
            }
            write(
                static_cast<std::uint8_t>(0x60 + offset),
                static_cast<std::uint8_t>(
                    ((op[3] & 0x0f) << 4) | (op[6] & 0x0f)
                )
            );
            write(
                static_cast<std::uint8_t>(0x80 + offset),
                static_cast<std::uint8_t>(
                    ((op[4] & 0x0f) << 4) | (op[7] & 0x0f)
                )
            );
            write(static_cast<std::uint8_t>(0x20 + offset), characteristics);
            write(
                static_cast<std::uint8_t>(0xe0 + offset),
                static_cast<std::uint8_t>(patch[26 + part] & 0x03)
            );
        }
    }

    static std::uint8_t velocityAttenuation(const std::uint8_t velocity) {
        // Level 1's active handle selects row 12 of blastdrv's recovered
        // 16-by-64 volume table. That row is the identity mapping.
        return static_cast<std::uint8_t>(63 - (velocity >> 1));
    }

    std::uint8_t fadedLevel(const std::uint8_t ksl, const std::uint8_t fallback,
        const std::uint8_t velocity) const {
        const auto index = velocity != 0 ? velocity >> 1 : fallback;
        const auto address = static_cast<std::uint16_t>(volumeTableAddress + (musicFade_ << 6) + index);
        std::uint8_t scaled;
        if (address >= volumeTableAddress && address - volumeTableAddress < volumeTable_.size()) {
            scaled = volumeTable_[address - volumeTableAddress];
        } else if (address >= instrumentBankAddress && address - instrumentBankAddress < instrumentBank_.size()) {
            //The voice0/direct FF alias can index beyond the table into the bank.
            scaled = instrumentBank_[address - instrumentBankAddress];
        } else {
            throw std::invalid_argument("unsupported faded driver level address");
        }
        return static_cast<std::uint8_t>(ksl | static_cast<std::uint8_t>(63 - scaled));
    }

    void refreshVoiceLevels(const std::size_t voice) {
        const auto& level = voiceLevels_[voice];
        const auto velocity = voice == 0 && directLevelLatch_ ? std::uint8_t{0xff} : voices_[voice].velocity;
        if (level.operatorEnabled) {
            const auto offset = operatorOffsets[voiceOperators[voice * 2]];
            write(static_cast<std::uint8_t>(0x40 + offset), fadedLevel(level.operatorKsl, level.operatorFallback, velocity));
        }
        const auto offset = operatorOffsets[voiceOperators[voice * 2 + 1]];
        write(static_cast<std::uint8_t>(0x40 + offset), fadedLevel(level.carrierKsl, level.carrierFallback, velocity));
    }

    void advanceMusicFade(const std::size_t slot) {
        if (musicFade_ == 0) {
            return;
        }
        if (slot != 0 || std::any_of(sequencers_.begin() + 1, sequencers_.end(),
            [](const Sequencer& sequence) { return sequence.allocated; })) {
            throw std::invalid_argument("multiple resident sequences excluded from music fade");
        }
        if (--fadeDivider_ != 0) {
            return;
        }
        if (--musicFade_ == 0) {
            silencePhysicalVoices(); //2d83 stops without EOF's cursor/wait reset.
            sequencers_[slot].active = false;
            sequencers_[slot].playback = Playback::once;
            return;
        }
        for (std::size_t voice = 0; voice < voices_.size(); ++voice) {
            refreshVoiceLevels(voice);
        }
        fadeDivider_ = fadeDividerReload;
    }

    void setVoiceLevel(
        const std::size_t voiceIndex,
        const std::uint8_t program,
        const std::uint8_t velocity
    ) {
        if (instrumentBank_.size() != 96 * 28 || program >= 96) {
            return;
        }
        if (musicFade_ != 0) {
            const auto& level = voiceLevels_[voiceIndex];
            const auto offset = operatorOffsets[voiceOperators[voiceIndex * 2 + 1]];
            write(static_cast<std::uint8_t>(0x40 + offset), fadedLevel(level.carrierKsl, level.carrierFallback, velocity));
            return;
        }
        const std::uint8_t* patch =
            instrumentBank_.data() + static_cast<std::size_t>(program) * 28;
        const std::uint8_t operatorIndex =
            voiceOperators[voiceIndex * 2 + 1];
        const std::uint8_t offset = operatorOffsets[operatorIndex];
        const std::uint8_t level = velocity == 0
            ? static_cast<std::uint8_t>(patch[21] & 0x3f)
            : velocityAttenuation(velocity);
        write(
            static_cast<std::uint8_t>(0x40 + offset),
            static_cast<std::uint8_t>(
                ((patch[13] & 0x03) << 6)
                | level
            )
        );
    }

    void setDirectProgramLevel(
        const std::size_t eventId,
        const std::size_t voiceIndex,
        const std::uint8_t program,
        const std::uint8_t velocity
    ) {
        if (velocity == 0 || eventId + 3 >= level1EventRecords.size()) {
            setVoiceLevel(voiceIndex, program, velocity);
            return;
        }
        const auto& futureRecord = level1EventRecords[eventId + 3];
        const std::uint16_t volumeWord = static_cast<std::uint16_t>(
            futureRecord[0]
            | (static_cast<std::uint16_t>(futureRecord[1]) << 8)
        );
        const std::uint16_t tableAddress = static_cast<std::uint16_t>(
            0x0903
            + static_cast<std::uint16_t>(volumeWord << 6)
            + (velocity >> 1)
        );
        std::uint8_t scaledVelocity = velocity >> 1;
        if (tableAddress >= 0x0d28) {
            const std::size_t bankOffset = tableAddress - 0x0d28;
            if (bankOffset < instrumentBank_.size()) {
                scaledVelocity = instrumentBank_[bankOffset];
            }
        }
        const std::uint8_t* patch =
            instrumentBank_.data() + static_cast<std::size_t>(program) * 28;
        const std::uint8_t carrier =
            operatorOffsets[voiceOperators[voiceIndex * 2 + 1]];
        write(
            static_cast<std::uint8_t>(0x40 + carrier),
            static_cast<std::uint8_t>(
                ((patch[13] & 0x03) << 6)
                | static_cast<std::uint8_t>(63 - scaledVelocity)
            )
        );
    }

    void noteOn(
        const std::size_t slot,
        const std::uint8_t channel,
        const std::uint8_t note,
        const std::uint8_t velocity
    ) {
        if (note < 12 || note > 107) {
            return;
        }
        const std::size_t voiceIndex = allocateVoice(channel);
        if (voiceIndex == voices_.size()) {
            return;
        }
        Voice& voice = voices_[voiceIndex];
        if (voiceIndex == 0) {
            // 1e61 rewrites CS:95 for physical voice0; the direct-event
            // FF override survives other voices' note updates.
            directLevelLatch_ = false;
        }
        // blastdrv's 2605 note-on core never selects or loads a patch. The
        // current physical-voice patch is established by program changes at
        // 1d57 and reused here, including on descriptors with bit 10h set.
        const std::uint8_t program = voice.program;

        const unsigned semitone = note % 12;
        static constexpr std::array<std::uint16_t, 12> fNumbers{
            0x157, 0x16b, 0x181, 0x198, 0x1b0, 0x1ca,
            0x1e5, 0x202, 0x220, 0x241, 0x263, 0x287,
        };
        const unsigned block = (note - 12) / 12;
        const std::uint16_t fNumber = fNumbers[semitone];
        voice.a0 = static_cast<std::uint8_t>(fNumber);
        voice.b0 = static_cast<std::uint8_t>(
            0x20 | ((block & 0x07) << 2) | ((fNumber >> 8) & 0x03)
        );
        setVoiceLevel(voiceIndex, program, velocity);
        voice.active = true;
        voice.frequencyPending = false;
        voice.sequencer = slot;
        voice.channel = channel;
        voice.note = note;
        voice.velocity = velocity;
        voice.program = program;
        // blastdrv 2605 writes A0 and B0 inside each note-on call.  This is
        // observable when several zero-delay notes share a sequencer tick:
        // each voice's level/frequency triplet is complete before allocation
        // advances to the next voice.
        write(static_cast<std::uint8_t>(0xa0 + voiceIndex), voice.a0);
        write(static_cast<std::uint8_t>(0xb0 + voiceIndex), voice.b0);
    }

    void noteOff(
        const std::size_t slot,
        const std::uint8_t channel,
        const std::uint8_t note
    ) {
        for (std::size_t voice = 0; voice < voices_.size(); ++voice) {
            if (
                voiceOwners_[voice] == channel
                && voices_[voice].note == note
            ) {
                // blastdrv 1e90 matches owner and retained note only. 2605
                // clears the sounding byte but keeps the note, so a repeated
                // note-off deliberately emits the same A0/B0 key-off again.
                keyOff(voice);
                return;
            }
        }
    }

    void keyOff(const std::size_t voiceIndex) {
        Voice& voice = voices_[voiceIndex];
        // All 2331 callers derive the off frequency from the retained note,
        // including a surviving SFX timer on a freshly reset music voice.
        static constexpr std::array<std::uint16_t, 12> fNumbers{
            0x157, 0x16b, 0x181, 0x198, 0x1b0, 0x1ca,
            0x1e5, 0x202, 0x220, 0x241, 0x263, 0x287,
        };
        const auto fNumber = fNumbers[voice.note % 12];
        const unsigned block = voice.note < 12 ? 0 : (voice.note - 12) / 12;
        voice.a0 = static_cast<std::uint8_t>(fNumber);
        voice.b0 = static_cast<std::uint8_t>(((block & 7) << 2) | ((fNumber >> 8) & 3));
        write(
            static_cast<std::uint8_t>(0xa0 + voiceIndex),
            voice.a0
        );
        write(
            static_cast<std::uint8_t>(0xb0 + voiceIndex),
            static_cast<std::uint8_t>(voice.b0 & ~0x20)
        );
        writeLatchedDirectLevel(voiceIndex);
        voice.active = false;
        voice.frequencyPending = false;
    }

    void triggerDirectEvent(
        const std::size_t eventId,
        const std::array<std::uint8_t, 8>& record
    ) {
        std::size_t slot = 0;
        if (directDurations_[0] != 0) {
            if (directDurations_[1] == 0) {
                slot = 1;
            } else if (directDurations_[2] == 0) {
                slot = 2;
            } else {
                // Native 21a5/21b3 use JL: equal busy timers prefer later slots.
                slot = directDurations_[1] <= directDurations_[0] ? 1 : 0;
                if (directDurations_[2] <= directDurations_[slot]) {
                    slot = 2;
                }
                keyOff(6 + slot);
                directDurations_[slot] = 0;
            }
        }
        const std::size_t voiceIndex = 6 + slot;
        const std::uint8_t program = record[0];
        const std::uint8_t note = record[1];
        const std::uint16_t duration = static_cast<std::uint16_t>(
            record[2] | (static_cast<std::uint16_t>(record[3]) << 8)
        );
        const auto directChannel = static_cast<std::uint8_t>(firstDirectChannel + slot);
        // Native21da -> 1d57 changes the logical direct channel's program.
        // Music may own the fixed note voice, or own another matching voice.
        for (std::size_t ownedVoice = 0; ownedVoice < voices_.size(); ++ownedVoice) {
            if (voiceOwners_[ownedVoice] != directChannel) {
                continue;
            }
            applyPatch(ownedVoice, program);
            setDirectProgramLevel(eventId, ownedVoice, program, voices_[ownedVoice].velocity);
            voices_[ownedVoice].program = program;
        }

        static constexpr std::array<std::uint16_t, 12> fNumbers{
            0x157, 0x16b, 0x181, 0x198, 0x1b0, 0x1ca,
            0x1e5, 0x202, 0x220, 0x241, 0x263, 0x287,
        };
        const std::uint16_t fNumber = fNumbers[note % 12];
        Voice& voice = voices_[voiceIndex];
        const std::uint8_t carrier =
            operatorOffsets[voiceOperators[voiceIndex * 2 + 1]];
        voice.a0 = static_cast<std::uint8_t>(fNumber);
        voice.b0 = static_cast<std::uint8_t>(
            0x20
            | (((note < 12 ? 0 : (note - 12) / 12) & 0x07) << 2)
            | ((fNumber >> 8) & 0x03)
        );
        write(static_cast<std::uint8_t>(0xa0 + voiceIndex), voice.a0);
        write(static_cast<std::uint8_t>(0xb0 + voiceIndex), voice.b0);
        write(static_cast<std::uint8_t>(0x40 + carrier), 0x40);
        directLevelLatch_ = true;
        voice.active = true;
        voice.channel = directChannel;
        voice.note = note;
        directDurations_[slot] = duration;
    }

    void advanceDirectEvents() {
        for (std::size_t slot = 0; slot < directDurations_.size(); ++slot) {
            if (directDurations_[slot] == 0) {
                continue;
            }
            --directDurations_[slot];
            if (directDurations_[slot] == 0) {
                keyOff(6 + slot);
            }
        }
    }

    void writeLatchedDirectLevel(const std::size_t voiceIndex) {
        if (directLevelLatch_) {
            // 2605:2698..26c1 follows A0/B0 with 3f-ff (byte40h) while
            // CS:95 retains the direct dispatcher21f8's global FF latch.
            const auto carrier = operatorOffsets[voiceOperators[voiceIndex * 2 + 1]];
            write(static_cast<std::uint8_t>(0x40 + carrier), 0x40);
        }
    }

    std::vector<std::vector<std::uint8_t>> sequences_;
    std::vector<std::uint8_t> instrumentBank_;
    std::vector<std::uint8_t> musicSequence_;
    std::vector<std::uint8_t> presentationSequence_;
    std::vector<std::uint8_t> level2MusicSequence_;
    std::vector<std::uint8_t> level3MusicSequence_;
    std::vector<std::uint8_t> campaignMusicSequence_;
    bool playableSequenceOwned_ = false;
    bool playableAttractLogo_ = false;
    bool playableAttractInheritedOwner_ = false;
    bool playableSampledVoice_ = false;
    std::vector<std::uint8_t> logoSequence_;
    std::vector<std::uint8_t> volumeTable_;
    std::uint16_t musicFade_ = 0;
    std::uint16_t fadeDivider_ = fadeDividerReload;
    SoundBlasterStartupPhase startupPhase_ = SoundBlasterStartupPhase::idle;
    std::uint16_t startupCountdown_ = 0;
    LogoCompletion logoCompletion_ = LogoCompletion::sequenceEnd;
    Opl2Chip opl2_;
    SampledVoiceStream sampledVoice_;
    std::array<Sequencer, 4> sequencers_{};
    std::array<Voice, 9> voices_{};
    std::array<VoiceLevel, 9> voiceLevels_{};
    bool directLevelLatch_ = false;
    std::array<std::uint8_t, 9> voiceOwners_{};
    std::array<std::uint8_t, 16> channelVoiceCursor_{};
    std::array<std::uint8_t, 16> channelEnabled_{};
    std::array<std::uint8_t, 16> channelDeferrals_{};
    std::array<std::uint16_t, 3> directDurations_{};
    //206b's six literal stores; no reader is admitted or aliased to BF/C1/C3.
    std::array<std::uint16_t, 3> directResetWordsD5_{}, directResetWordsEb_{};
    std::vector<OplRegisterWrite> registerWrites_;
    std::uint64_t sequenceSamplePhase_ = sequencePhasePeriod;
    std::size_t interruptMaskedSamples_ = 0;
    std::uint64_t workCycles_ = 0;
    bool workClockActive_ = false;
    bool deferOplWrites_ = false;
    bool workStateSupported_ = true;
    bool recoveredInitialization_ = false;
    bool driverEnabled_ = true;
    std::uint16_t initializationOplWrites_ = 0;
    std::optional<SoundBlasterProgramState> programState_;
    std::size_t workSequenceSlot_ = 0;
    bool level1ResetCompleted_ = false;
    bool successAudioRetained_ = false;
    std::uint8_t recoveredParserStatus_ = 0;
    std::array<std::uint8_t, 9> programTuning_{};
};

SoundBlasterSynth::SoundBlasterSynth(
    std::vector<std::vector<std::uint8_t>> sequences,
    std::vector<std::uint8_t> instrumentResource,
    std::vector<std::uint8_t> musicSequence,
    std::vector<std::uint8_t> presentationSequence,
    std::vector<std::uint8_t> level2MusicSequence,
    std::vector<std::uint8_t> level3MusicSequence,
    std::vector<std::uint8_t> logoSequence,
    std::vector<std::uint8_t> volumeTable,
    const SoundBlasterInitialization initialization
)
    : impl_(std::make_unique<Impl>(
          std::move(sequences),
          std::move(instrumentResource),
          std::move(musicSequence),
          std::move(presentationSequence),
          std::move(level2MusicSequence),
          std::move(level3MusicSequence),
          std::move(logoSequence),
          std::move(volumeTable), initialization
      )) {}

SoundBlasterSynth::~SoundBlasterSynth() = default;
SoundBlasterSynth::SoundBlasterSynth(SoundBlasterSynth&&) noexcept = default;
SoundBlasterSynth& SoundBlasterSynth::operator=(
    SoundBlasterSynth&&
) noexcept = default;

void SoundBlasterSynth::beginLogo() {
    impl_->beginLogo();
}

void SoundBlasterSynth::advanceStartupIrq() {
    impl_->advanceStartupIrq();
}

void SoundBlasterSynth::triggerLogoEvent() {
    impl_->triggerLogoEvent();
}

void SoundBlasterSynth::finishLogo() {
    impl_->finishLogo(LogoCompletion::sequenceEnd);
}

void SoundBlasterSynth::beginLogoFrame() { impl_->beginLogoFrame(); }
void SoundBlasterSynth::finishLogo(const LogoCompletion completion) { impl_->finishLogo(completion); }
bool SoundBlasterSynth::logoFinished() const noexcept { return impl_->logoFinished(); }
void SoundBlasterSynth::stopLogo() { impl_->stopLogo(); }
void SoundBlasterSynth::requestGameplay() { impl_->requestGameplay(); }
void SoundBlasterSynth::validateGameplayRequest() const { impl_->validateGameplayRequest(); }
void SoundBlasterSynth::fadePresentation() { impl_->fadePresentation(); }

void SoundBlasterSynth::beginPresentation() {
    impl_->beginPresentation();
}

void SoundBlasterSynth::validateMenuAudioReturn() const {
    impl_->validateMenuAudioReturn();
}

void SoundBlasterSynth::restartMenuAudio() {
    impl_->restartMenuAudio();
}

void SoundBlasterSynth::beginMenu() { impl_->beginMenu(); }

void SoundBlasterSynth::beginGameplay() {
    impl_->beginGameplay();
}

SoundBlasterStartupPhase SoundBlasterSynth::startupPhase() const noexcept {
    return impl_->startupPhase();
}

std::uint16_t SoundBlasterSynth::startupCountdown() const noexcept {
    return impl_->startupCountdown();
}

bool SoundBlasterSynth::logoActive() const noexcept {
    return impl_->logoActive();
}

void SoundBlasterSynth::startMusic() {
    impl_->startMusic();
}

void SoundBlasterSynth::stopMusic() {
    impl_->stopMusic();
}

void SoundBlasterSynth::startLevel1() {
    impl_->startLevel1(canonicalPresentationCallbacks);
}

void SoundBlasterSynth::startLevel1(const std::size_t presentationCallbacks) {
    impl_->startLevel1(presentationCallbacks);
}

void SoundBlasterSynth::startLevel2() {
    impl_->startLevel2();
}

void SoundBlasterSynth::switchLevel2() {
    impl_->switchLevel2();
}

void SoundBlasterSynth::startLevel3() {
    impl_->startLevel3();
}

void SoundBlasterSynth::switchLevel3() {
    impl_->switchLevel3();
}

void SoundBlasterSynth::resetDirectEffects() {
    impl_->resetDirectEffects();
}

void SoundBlasterSynth::loadCampaignMusic(const std::vector<std::uint8_t>& sequence) {
    impl_->loadCampaignMusic(sequence);
}

SoundBlasterCallbackState SoundBlasterSynth::selectPlayableSequence(const std::vector<std::uint8_t>& sequence,
    const PlayableSequenceMode mode, const PlayableMusicPublication publication) {
    return impl_->selectPlayableSequence(sequence,mode,publication);
}

SoundBlasterCallbackState SoundBlasterSynth::setPlayableMusicPublication(const PlayableMusicPublication publication) {
    return impl_->setPlayableMusicPublication(publication);
}

void SoundBlasterSynth::validateBomb7CallerRow(const SoundBlasterCallbackState& retained,
    const SoundBlasterBomb7CallerRow& row) const { impl_->validateBomb7CallerRow(retained, row); }
void SoundBlasterSynth::configureBomb7CallerRow(const SoundBlasterCallbackState& retained,
    const SoundBlasterBomb7CallerRow& row) { impl_->configureBomb7CallerRow(retained, row); }
void SoundBlasterSynth::validatePlayableBombEvent(const SoundBlasterBomb7CallerRow& row) const {
    impl_->validatePlayableBombEvent(row);
}

SoundBlasterCallbackState SoundBlasterSynth::selectPlayableAttractLogo(const PlayableMusicPublication publication) {return impl_->selectPlayableAttractLogo(publication);}
SoundBlasterCallbackState SoundBlasterSynth::triggerPlayableAttractLogoEvent() {return impl_->triggerPlayableAttractLogoEvent();}
SoundBlasterCallbackState SoundBlasterSynth::selectPlayableAttractMenu(const PlayableMusicPublication publication) {return impl_->selectPlayableAttractMenu(publication);}

SoundBlasterCallbackState SoundBlasterSynth::triggerPlayableEvent(const std::uint16_t eventId) {
    return impl_->triggerPlayableEvent(eventId);
}

SoundBlasterCallbackState SoundBlasterSynth::resetPlayableDirectEffects() {
    return impl_->resetPlayableDirectEffects();
}

SoundBlasterCallbackState SoundBlasterSynth::stopPlayableSequence() {
    return impl_->stopPlayableSequence();
}

SoundBlasterCallbackState SoundBlasterSynth::deselectPlayableSequence() {
    return impl_->deselectPlayableSequence();
}

SoundBlasterCallbackState SoundBlasterSynth::releasePlayableStoryLogo() {
    return impl_->releasePlayableStoryLogo();
}

SoundBlasterCallbackState SoundBlasterSynth::releaseSuspendedPlayableSequence() {
    return impl_->releaseSuspendedPlayableSequence();
}

void SoundBlasterSynth::loadPlayableSampledVoice(const std::vector<std::uint8_t>& sequence) {
    impl_->loadPlayableSampledVoice(sequence);
}

void SoundBlasterSynth::validateCampaignMusicLoad(const std::vector<std::uint8_t>& sequence) const {
    impl_->validateCampaignMusicLoad(sequence);
}

void SoundBlasterSynth::validateSampledVoiceLoad(const std::vector<std::uint8_t>& sequence) const {
    impl_->validateSampledVoiceLoad(sequence);
}

void SoundBlasterSynth::loadSampledVoice(const std::vector<std::uint8_t>& sequence) {
    impl_->loadSampledVoice(sequence);
}

void SoundBlasterSynth::validateSampledVoiceStart() const {
    impl_->validateSampledVoiceStart();
}

void SoundBlasterSynth::startSampledVoice() {
    impl_->startSampledVoice();
}

void SoundBlasterSynth::serviceSampledVoiceIrq() {
    impl_->serviceSampledVoiceIrq();
}

void SoundBlasterSynth::stopSampledVoice() {
    impl_->stopSampledVoice();
}

void SoundBlasterSynth::releaseSampledVoice() {
    impl_->releaseSampledVoice();
}

SampledVoiceObservation SoundBlasterSynth::sampledVoiceObservation() const noexcept {
    return impl_->sampledVoiceObservation();
}

void SoundBlasterSynth::startSuccess() {
    impl_->startSuccess();
}

void SoundBlasterSynth::stopSuccess() {
    impl_->stopSuccess();
}

void SoundBlasterSynth::validateCampaignMusicRelease() const {
    impl_->validateCampaignMusicRelease();
}

void SoundBlasterSynth::releaseCampaignMusic() {
    impl_->releaseCampaignMusic();
}

bool SoundBlasterSynth::successActive() const noexcept {
    return impl_->successActive();
}

void SoundBlasterSynth::triggerLevel1Event(const int eventId) {
    impl_->triggerLevel1Event(eventId);
}

std::vector<std::int16_t> SoundBlasterSynth::render(
    const std::size_t sampleCount
) {
    return impl_->render(sampleCount);
}

std::vector<std::int16_t> SoundBlasterSynth::renderInterruptMasked(
    const std::size_t sampleCount
) {
    return impl_->renderInterruptMasked(sampleCount);
}

void SoundBlasterSynth::resumeSampleInterrupts() {
    impl_->resumeSampleInterrupts();
}

bool SoundBlasterSynth::hasInstrumentBank() const noexcept {
    return impl_->hasInstrumentBank();
}

const std::vector<OplRegisterWrite>&
SoundBlasterSynth::registerWrites() const noexcept {
    return impl_->registerWrites();
}

void SoundBlasterSynth::clearRegisterWrites() noexcept {
    impl_->clearRegisterWrites();
}

SoundBlasterDriverState SoundBlasterSynth::driverState() const noexcept {
    return impl_->driverState();
}

SoundBlasterCallbackState SoundBlasterSynth::callbackWorkState() const {
    return impl_->callbackWorkState();
}

void SoundBlasterSynth::completeRecoveredDirectReset() { impl_->completeRecoveredDirectReset(); }
SoundBlasterCallbackState SoundBlasterSynth::prepareRecoveredLogo(const SoundBlasterProgramState& program,
    const PlayableMusicPublication publication) {
    return impl_->prepareRecoveredLogo(program,publication);
}
SoundBlasterCallbackState SoundBlasterSynth::previewRecoveredPresentation(const PlayableMusicPublication publication) const { return impl_->previewRecoveredPresentation(publication); }
SoundBlasterCallbackState SoundBlasterSynth::preparePlayablePresentationAfterStory(const PlayableMusicPublication publication) {
    return impl_->preparePlayablePresentationAfterStory(publication);
}

SoundBlasterCallbackState SoundBlasterSynth::prepareRecoveredPresentation(const PlayableMusicPublication publication) { return impl_->prepareRecoveredPresentation(publication); }
SoundBlasterCallbackState SoundBlasterSynth::previewRecoveredLevel1(const PlayableMusicPublication publication) const { return impl_->previewRecoveredLevel1(publication); }
SoundBlasterCallbackState SoundBlasterSynth::prepareRecoveredLevel1(const PlayableMusicPublication publication) { return impl_->prepareRecoveredLevel1(publication); }
SoundBlasterCallbackState SoundBlasterSynth::previewRecoveredLevel1Retry() const { return impl_->previewRecoveredLevel1Retry(); }
SoundBlasterCallbackState SoundBlasterSynth::previewRecoveredLevel1Retry(const SoundBlasterCallbackState& noHandle,
    const SoundBlasterCallbackState& retainedHandle) const { return impl_->previewRecoveredLevel1Retry(noHandle,retainedHandle); }
SoundBlasterCallbackState SoundBlasterSynth::prepareRecoveredLevel1Retry() { return impl_->prepareRecoveredLevel1Retry(); }
SoundBlasterCallbackState SoundBlasterSynth::previewRecoveredLevel1Reset() const { return impl_->previewRecoveredLevel1Reset(); }
SoundBlasterCallbackState SoundBlasterSynth::completeRecoveredLevel1Reset() { return impl_->completeRecoveredLevel1Reset(); }
void SoundBlasterSynth::configurePlayerHitVolumeRow(const SoundBlasterVolumeRow& row) { impl_->configurePlayerHitVolumeRow(row); }
SoundBlasterEnemy20CallerWindow SoundBlasterSynth::configureEnemy20CallerWindow(const SoundBlasterCallbackState& retained) {
    return impl_->configureEnemy20CallerWindow(retained);
}
void SoundBlasterSynth::configureEnemy20McbByte(const SoundBlasterCallbackState& retained,const SoundBlasterEnemy20McbByte& byte) {
    impl_->configureEnemy20McbByte(retained,byte);
}

void SoundBlasterSynth::configurePlayableEnemy20Process(const SoundBlasterCallbackState& retained,const SoundBlasterEnemy20McbByte& byte) {
    impl_->configurePlayableEnemy20Process(retained,byte);
}

ClockedSoundBlasterAudio SoundBlasterSynth::renderClocked(const NormalWorkTiming& work,
    const std::uint64_t originCycles) {
    return impl_->renderClocked(work, originCycles);
}

}  // namespace stellar7
