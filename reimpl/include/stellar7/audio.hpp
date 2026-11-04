#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "stellar7/audio_work.hpp"
#include "stellar7/sampled_voice.hpp"

namespace stellar7 {

// Current interactive cadence; not a claim about the original's variable frame timing.
inline constexpr std::size_t gameplayAudioSamplesPerFrame = 1455;

struct OplRegisterWrite {
    std::uint8_t registerNumber;
    std::uint8_t value;
};

struct NormalWorkTiming;

struct ClockedOplRegisterWrite {
    std::int64_t cycleOffset;
    OplRegisterWrite write;
};

struct ClockedSoundBlasterAudio {
    std::vector<std::int16_t> samples;
    std::vector<ClockedOplRegisterWrite> writes;
    std::uint64_t firstSample = 0;
    std::vector<std::uint64_t> vgaSamplePositions;
};

enum class SoundBlasterClockSource { samples, normalWork };
enum class SoundBlasterInitialization { compatibility, recoveredWork };
enum class PlayableSequenceMode { once, repeat };
enum class PlayableMusicPublication { current, suspended };

struct SoundBlasterDriverState {
    std::array<std::uint8_t, 9> notes{};
    std::array<std::uint8_t, 9> velocities{};
    std::array<std::uint8_t, 16> channelVoiceCursors{};
    std::array<std::uint16_t, 3> directTimers{};
    bool directLevelLatch = false;
    SoundBlasterClockSource clockSource = SoundBlasterClockSource::samples;
    std::uint64_t normalClockCycles = 0;
    // Sample-driven phase is retained as history after normal work activation.
    std::uint64_t clockPhase = 0;
    std::uint64_t clockPeriod = 0;
    std::size_t sequencePosition = 0;
    std::uint16_t sequenceWait = 0;
    std::uint8_t runningStatus = 0;
    std::uint16_t fade = 0;
    std::uint16_t fadeDivider = 0;
    bool sequenceActive = false;
    std::array<std::uint8_t, 9> carrierKsl{}, carrierFallback{}, operatorKsl{}, operatorFallback{};
    std::array<bool, 9> operatorEnabled{};
    bool driverEnabled = true;
    std::uint16_t initializationOplWrites = 0;
    bool sequenceAllocated = false;
    bool sequenceSelected = false;
    std::array<std::size_t, 4> sequencePositions{};
    std::array<bool, 4> sequenceAllocations{}, sequenceSelections{}, sequenceActivity{};
};

std::vector<std::uint8_t> decodeSoundBlasterVolumeTable(const std::vector<std::uint8_t>& driver);

std::uint16_t pcSpeakerDivisor(
    std::uint8_t note,
    std::uint16_t pitchBend = 0x2000
);

int level1EventSequenceIndex(int eventId) noexcept;

class Opl2Chip {
public:
    static constexpr unsigned sampleRate = 44100;
    static constexpr unsigned inputClock = 3579545;

    Opl2Chip();
    ~Opl2Chip();
    Opl2Chip(Opl2Chip&&) noexcept;
    Opl2Chip& operator=(Opl2Chip&&) noexcept;
    Opl2Chip(const Opl2Chip&) = delete;
    Opl2Chip& operator=(const Opl2Chip&) = delete;

    void reset();
    void writeRegister(std::uint8_t registerNumber, std::uint8_t value);
    std::vector<std::int16_t> render(std::size_t sampleCount);
    unsigned nativeSampleRate() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

enum class SoundBlasterStartupPhase { idle, logoDelay, logoPlaying, logoEnding, presentation, gameplay, gameplayWaiting, logoStopped };
enum class LogoCompletion { sequenceEnd, countdown };

class SoundBlasterSynth {
public:
    static constexpr unsigned sampleRate = Opl2Chip::sampleRate;
    // Legacy nominal diagnostic rate; playback uses the original PIT ratio.
    static constexpr unsigned sequenceRate = 60;

    SoundBlasterSynth(
        std::vector<std::vector<std::uint8_t>> sequences = {},
        std::vector<std::uint8_t> instrumentResource = {},
        std::vector<std::uint8_t> musicSequence = {},
        std::vector<std::uint8_t> presentationSequence = {},
        std::vector<std::uint8_t> level2MusicSequence = {},
        std::vector<std::uint8_t> level3MusicSequence = {},
        std::vector<std::uint8_t> logoSequence = {},
        std::vector<std::uint8_t> volumeTable = {},
        SoundBlasterInitialization initialization = SoundBlasterInitialization::compatibility
    );
    ~SoundBlasterSynth();
    SoundBlasterSynth(SoundBlasterSynth&&) noexcept;
    SoundBlasterSynth& operator=(SoundBlasterSynth&&) noexcept;
    SoundBlasterSynth(const SoundBlasterSynth&) = delete;
    SoundBlasterSynth& operator=(const SoundBlasterSynth&) = delete;

    void beginLogo();
    void advanceStartupIrq();
    void triggerLogoEvent();
    void beginLogoFrame();
    void finishLogo();
    void finishLogo(LogoCompletion completion);
    bool logoFinished() const noexcept;
    void stopLogo();
    void validateGameplayRequest() const;
    void requestGameplay();
    void fadePresentation();
    void beginPresentation();
    void validateMenuAudioReturn() const;
    //04BF repeats retained SNG1 without the startup2000-IRQ countdown.
    void restartMenuAudio();
    void beginMenu();
    void beginGameplay();
    SoundBlasterStartupPhase startupPhase() const noexcept;
    std::uint16_t startupCountdown() const noexcept;
    bool logoActive() const noexcept;
    void startMusic();
    void stopMusic();
    void startLevel1();
    // Replay a measured first-pass sequence history; not a sample-clock restore.
    void startLevel1(std::size_t presentationCallbacks);
    void startLevel2();
    void switchLevel2();
    void startLevel3();
    // Switch the resident music sequencer to LEVEL3.RES without releasing
    // physical voices. Native campaign handoff preserves the live driver
    // state instead of performing a fresh SNG reset.
    void switchLevel3();
    void loadCampaignMusic(const std::vector<std::uint8_t>& sequence);
    // Untimed native providers on the retained normal-work synth and IRQ epoch.
    SoundBlasterCallbackState selectPlayableSequence(const std::vector<std::uint8_t>& sequence,
        PlayableSequenceMode mode, PlayableMusicPublication publication = PlayableMusicPublication::current);
    //84CE/51C4/033E: publish or suspend the existing descriptor, preserving cursor/allocation.
    SoundBlasterCallbackState setPlayableMusicPublication(PlayableMusicPublication publication);
    SoundBlasterCallbackState triggerPlayableEvent(std::uint16_t eventId);
    SoundBlasterCallbackState resetPlayableDirectEffects();
    SoundBlasterCallbackState stopPlayableSequence();
    SoundBlasterCallbackState deselectPlayableSequence();
    SoundBlasterCallbackState releaseSuspendedPlayableSequence();
    SoundBlasterCallbackState releasePlayableStoryLogo();
    SoundBlasterCallbackState selectPlayableAttractLogo(PlayableMusicPublication publication);
    SoundBlasterCallbackState triggerPlayableAttractLogoEvent();
    SoundBlasterCallbackState selectPlayableAttractMenu(PlayableMusicPublication publication);
    void loadPlayableSampledVoice(const std::vector<std::uint8_t>& sequence);
    void validateCampaignMusicLoad(const std::vector<std::uint8_t>& sequence) const;
    void validateCampaignMusicRelease() const;
    // 018C frees payload/descriptor storage; the caller separately owns51A6.
    void releaseCampaignMusic();
    void validateSampledVoiceLoad(const std::vector<std::uint8_t>& sequence) const;
    void loadSampledVoice(const std::vector<std::uint8_t>& sequence);
    void validateSampledVoiceStart() const;
    void startSampledVoice();
    // Explicit semantic completion service; caller must own its IRQ boundary.
    void serviceSampledVoiceIrq();
    void stopSampledVoice();
    void releaseSampledVoice();
    SampledVoiceObservation sampledVoiceObservation() const noexcept;
    void resetDirectEffects();
    void startSuccess();
    void stopSuccess();
    bool successActive() const noexcept;
    void triggerLevel1Event(int eventId);
    std::vector<std::int16_t> render(std::size_t sampleCount);
    // Preserve OPL output but defer callbacks; consecutive intervals must be shorter than one PIT IRQ period.
    std::vector<std::int16_t> renderInterruptMasked(std::size_t sampleCount);
    // Complete a source STI shadow without consuming an output sample.
    void resumeSampleInterrupts();
    bool hasInstrumentBank() const noexcept;
    const std::vector<OplRegisterWrite>& registerWrites() const noexcept;
    void clearRegisterWrites() noexcept;
    SoundBlasterDriverState driverState() const noexcept;
    SoundBlasterCallbackState callbackWorkState() const;
    // Source op16/op6 have no OPL effects in the fresh no-handle, zero-timer domain.
    void completeRecoveredDirectReset();
    SoundBlasterCallbackState prepareRecoveredLogo(const SoundBlasterProgramState& program, PlayableMusicPublication publication = PlayableMusicPublication::current);
    SoundBlasterCallbackState previewRecoveredPresentation(PlayableMusicPublication publication = PlayableMusicPublication::current) const;
    SoundBlasterCallbackState prepareRecoveredPresentation(PlayableMusicPublication publication = PlayableMusicPublication::current);
    SoundBlasterCallbackState preparePlayablePresentationAfterStory(PlayableMusicPublication publication);
    // Independent Level1 allocation after the retained Begin stop; publication is separate.
    SoundBlasterCallbackState previewRecoveredLevel1(PlayableMusicPublication publication = PlayableMusicPublication::current) const;
    SoundBlasterCallbackState prepareRecoveredLevel1(PlayableMusicPublication publication = PlayableMusicPublication::current);
    SoundBlasterCallbackState previewRecoveredLevel1Retry() const;
    SoundBlasterCallbackState previewRecoveredLevel1Retry(const SoundBlasterCallbackState& noHandle,
        const SoundBlasterCallbackState& retainedHandle) const;
    SoundBlasterCallbackState prepareRecoveredLevel1Retry();
    SoundBlasterCallbackState previewRecoveredLevel1Reset() const;
    SoundBlasterCallbackState completeRecoveredLevel1Reset();
    // Untimed immutable resource binding after the selected Level1 direct reset.
    void configurePlayerHitVolumeRow(const SoundBlasterVolumeRow& row);
    SoundBlasterEnemy20CallerWindow configureEnemy20CallerWindow(const SoundBlasterCallbackState& retained);
    void configureEnemy20McbByte(const SoundBlasterCallbackState& retained,const SoundBlasterEnemy20McbByte& byte);
    void configurePlayableEnemy20Process(const SoundBlasterCallbackState& retained,const SoundBlasterEnemy20McbByte& byte);
    // The work clock owns callback delivery. OPL writes take effect at its data-OUT boundaries.
    ClockedSoundBlasterAudio renderClocked(const NormalWorkTiming& work, std::uint64_t originCycles);

private:
    friend class NativeGameplaySession;
    void validateBomb7CallerRow(const SoundBlasterCallbackState& retained, const SoundBlasterBomb7CallerRow& row) const;
    void configureBomb7CallerRow(const SoundBlasterCallbackState& retained, const SoundBlasterBomb7CallerRow& row);
    void validatePlayableBombEvent(const SoundBlasterBomb7CallerRow& row) const;
    class Impl;
    std::unique_ptr<Impl> impl_;
};

class PcSpeakerSynth {
public:
    static constexpr unsigned sampleRate = 44100;
    static constexpr unsigned sequenceRate = 15;

    explicit PcSpeakerSynth(
        std::vector<std::vector<std::uint8_t>> sequences = {}
    );

    void startMusic();
    void triggerLevel1Event(int eventId);
    std::vector<std::int16_t> render(std::size_t sampleCount);

private:
    void startSequence(std::size_t index);
    void stopSequence();
    void advanceSequenceTick();
    void applyMessage(
        std::uint8_t status,
        std::uint8_t first,
        std::uint8_t second
    );
    bool readMessage(
        std::uint8_t& status,
        std::uint8_t& first,
        std::uint8_t& second,
        std::uint16_t& delay
    );

    std::vector<std::vector<std::uint8_t>> sequences_;
    const std::vector<std::uint8_t>* active_ = nullptr;
    std::size_t position_ = 0;
    std::uint8_t runningStatus_ = 0;
    std::uint8_t selectedChannel_ = 0xff;
    std::uint16_t delay_ = 0;
    std::uint16_t pitchBend_ = 0x2000;
    int note_ = -1;
    std::uint16_t divisor_ = 0;
    std::uint64_t sequenceSamplePhase_ = 0;
    std::uint64_t squarePhase_ = 0;
};

}  // namespace stellar7
