#pragma once
#include "stellar7/retained_level2.hpp"
#include "stellar7/quiet_scanout.hpp"
#include "stellar7/controlled_keyboard.hpp"
#include <memory>

namespace stellar7 {
enum class ControlledInputDelivery { beforeTick, afterCheckpoint };
enum class ControlledArrivalInput { released, residentForward, physicalForwardPair, hostForward };
enum class ControlledInputKey { left, right, forward, reverse, fire };
struct ControlledInputEvent {
    std::uint32_t tick;
    bool pressed;
    std::vector<ControlledInputKey> keys;
};
struct ControlledBootstrapRequest {
    std::vector<ControlledInputEvent> events;
    ControlledInputDelivery delivery;
    SoundBlasterClockPhase returnedRaw4Phase;
    HorizonWorkState horizon;
    CockpitSettings settings;
    FrameCopyProfile copyProfile;
    std::int64_t horizonCycles;
    ControlledArrivalInput arrivalInput = ControlledArrivalInput::released;
    std::array<std::uint16_t, frameKeyboardSlotCount> initialKeyboardSlots{};
};
struct ControlledSessionCursors {
    std::size_t nextInput;
    std::size_t level1Audio;
    std::size_t level2Audio;
    std::size_t failureAudio;
    std::size_t emptyTailReads;
};
enum class ControlledObservationScope { bootstrap, continuation };
struct ControlledSessionView {
    const GameplayModel& model;
    const SoundBlasterSynth& synth;
    const CockpitPageState& pages;
    const RadarState& radar;
    const IndexedPages& pixels;
    const PaletteWorkState& palette;
    const SoundBlasterClockPhase* phase;
    ControlledSessionCursors cursors;
    std::uint64_t elapsedSamples;
    ControlledObservationScope scope;
    std::uint8_t rasterStyle;
    std::optional<std::uint8_t> enemyFlag;
    std::optional<std::uint16_t> controllerTick;
    std::optional<FrameControllerKeyboardState> keyboardState;
};
struct ControlledBootstrapTick {
    std::uint32_t tick;
    GameplayPhase previous;
    std::size_t entryFadeSamples;
    std::vector<Level1AudioEvent> audioEvents;
    std::optional<SoundBlasterDriverState> successEntry;
    bool arrivalExecuted;
};
class ControlledSessionObserver {
public:
    virtual ~ControlledSessionObserver() = default;
    virtual void startup(const ControlledSessionView&) {}
    virtual void initialEntry(const ControlledSessionView&, std::size_t) {}
    virtual void bootstrapTick(const ControlledSessionView&, const ControlledBootstrapTick&) {}
    virtual void arrival(const ControlledSessionView&) {}
    virtual void ready(const ControlledSessionView&) {}
    virtual void advanced(const ControlledSessionView&) {}
    virtual void renderEntry(const ControlledSessionView&, const Level2FramePlan&, const Level2RenderState&) {}
    virtual void body(const ControlledSessionView&, const Level2FramePlan&, const ClockedPresentationWork&) {}
    virtual void display(const ControlledSessionView&, const ClockedPageFlipWork&) {}
    virtual void postcopy(const ControlledSessionView&, const ClockedPresentationWork&) {}
    virtual void renderComplete(const ControlledSessionView&, const Level2FramePlan&) {}
    virtual void prefixEntry(const ControlledSessionView&, const StationaryLevel2PrefixInput&) {}
    virtual void prefix(const ControlledSessionView&, const RetainedPrefixExecution&) {}
    virtual void tailEntry(const ControlledSessionView&, const FrameTailState&, const FrameTailGuards&) {}
    virtual void tail(const ControlledSessionView&, const RetainedTailExecution&) {}
};
struct ControlledScan {
    QuietScanout::CompletedScan scan;
    std::uint64_t samplePosition;
};
struct ControlledSessionDelivery {
    std::vector<std::int16_t> samples;
    std::vector<ControlledScan> scans;
    std::uint64_t firstSample = 0;
};
// Fixed existing268/4369/armor680-held recipe; not a cold/startLevel2 factory.
class ControlledLevel2Session {
public:
    ControlledLevel2Session(GameplayAssets assets, ControlledBootstrapRequest request,
        ControlledSessionObserver* observer = nullptr, ControlledKeyboardSource* keyboard = nullptr);
    ~ControlledLevel2Session();
    ControlledLevel2Session(const ControlledLevel2Session&) = delete;
    ControlledLevel2Session& operator=(const ControlledLevel2Session&) = delete;
    ControlledLevel2Session(ControlledLevel2Session&&) noexcept;
    ControlledLevel2Session& operator=(ControlledLevel2Session&&) noexcept;
    ControlledSessionDelivery advance(const GameplayInput& releasedInput);
    ControlledSessionDelivery takeDelivery();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace stellar7
