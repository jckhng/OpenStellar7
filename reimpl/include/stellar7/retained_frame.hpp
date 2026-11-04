#pragma once

#include "stellar7/level2_frame.hpp"
#include "stellar7/frame_copy_work.hpp"
#include "stellar7/presentation_clock.hpp"
#include <memory>

namespace stellar7 {

class FrameController;

struct RetainedPrefixExecution {
    StationaryLevel2PrefixWork state;
    ClockedPresentationWork work;
};

struct RetainedTailExecution {
    FrameTailWork state;
    ClockedPresentationWork work;
};

// Begins at an owned render return; no subsequent phase installation is exposed.
class RetainedFrameExecutor {
public:
    RetainedFrameExecutor(PresentationClock& clock, SoundBlasterClockPhase initialPhase, std::int64_t horizonCycles);
    RetainedFrameExecutor(PresentationClock& clock, SoundBlasterClockPhase initialPhase,
        std::int64_t horizonCycles, FrameController& controller);
    ~RetainedFrameExecutor();
    RetainedFrameExecutor(const RetainedFrameExecutor&) = delete;
    RetainedFrameExecutor& operator=(const RetainedFrameExecutor&) = delete;
    RetainedFrameExecutor(RetainedFrameExecutor&&) = delete;
    RetainedFrameExecutor& operator=(RetainedFrameExecutor&&) = delete;
    ClockedPresentationWork render(const Level2FramePlan& plan);
    ClockedPageFlipWork display(const CockpitPageState& pages);
    ClockedPresentationWork postcopy(FrameCopyProfile profile);
    RetainedTailExecution finish(const FrameTailState& state, const FrameTailGuards& guards);
    RetainedPrefixExecution prepare(const StationaryLevel2PrefixInput& input);
    const SoundBlasterClockPhase& phase() const noexcept { return phase_; }
    void queueForward(KeyboardEdge edge);

private:
    enum class Stage { render, display, postcopy, tail, prefix, rejected };
    void require(Stage stage) const;
    ClockedPresentationWork advance(const std::vector<NormalWorkOperation>& operations, Stage next);
    PresentationClock& clock_;
    SoundBlasterClockPhase phase_;
    std::int64_t horizonCycles_;
    Stage stage_ = Stage::display;
    std::unique_ptr<KeyboardClockAccess> keyboardAccess_;
};

}
