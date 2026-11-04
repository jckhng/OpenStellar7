#include "stellar7/retained_frame.hpp"
#include "keyboard_clock_access.hpp"

#include <stdexcept>
#include <utility>

namespace stellar7 {

RetainedFrameExecutor::RetainedFrameExecutor(PresentationClock& clock, SoundBlasterClockPhase initialPhase,
    const std::int64_t horizonCycles)
    : clock_(clock), phase_(std::move(initialPhase)), horizonCycles_(horizonCycles) {
    if (!phase_.bios || horizonCycles_ <= 0) {
        throw std::invalid_argument("retained frame requires owned returned-render BIOS phase and bounded horizon");
    }
}

RetainedFrameExecutor::RetainedFrameExecutor(PresentationClock& clock, SoundBlasterClockPhase initialPhase,
    const std::int64_t horizonCycles, FrameController& controller)
    : RetainedFrameExecutor(clock, std::move(initialPhase), horizonCycles) {
    if (!phase_.keyboard || !phase_.devices || phase_.devices->irq1InService || phase_.devices->irq1Requested
        || phase_.keyboard->used != 0 || phase_.keyboard->scheduled || phase_.keyboard->port60Pending
        || clock_.keyboardAccess_) {
        throw std::invalid_argument("physical keyboard arrival requires an idle owned transport");
    }
    keyboardAccess_ = std::make_unique<KeyboardClockAccess>(controller);
    keyboardAccess_->validate();
    phase_.interruptDelivery = AudioClockInterruptDelivery::residentTimerKeyboard;
    clock_.keyboardAccess_ = keyboardAccess_.get();
}

RetainedFrameExecutor::~RetainedFrameExecutor() {
    if (keyboardAccess_ && clock_.keyboardAccess_ == keyboardAccess_.get()) {
        clock_.keyboardAccess_ = nullptr;
    }
}

void RetainedFrameExecutor::queueForward(const KeyboardEdge edge) {
    require(Stage::display);
    if (!keyboardAccess_ || phase_.instructionKeyboardEdge || phase_.keyboard->used != 0
        || phase_.keyboard->scheduled || phase_.keyboard->port60Pending) {
        throw std::invalid_argument("physical edge requires drained bound render return");
    }
    keyboardAccess_->validateEdge(edge);
    phase_.instructionKeyboardEdge = edge;
}

void RetainedFrameExecutor::require(const Stage stage) const {
    if (stage_ != stage) {
        throw std::invalid_argument("retained frame execution boundary out of order");
    }
}

ClockedPresentationWork RetainedFrameExecutor::advance(const std::vector<NormalWorkOperation>& operations,
    const Stage next) {
    try {
        auto result = clock_.renderWork(phase_, operations, horizonCycles_);
        phase_ = result.timing.phase;
        stage_ = next;
        return result;
    } catch (...) {
        stage_ = Stage::rejected;
        throw;
    }
}

ClockedPresentationWork RetainedFrameExecutor::render(const Level2FramePlan& plan) {
    require(Stage::render);
    return advance(plan.operations, Stage::display);
}

ClockedPageFlipWork RetainedFrameExecutor::display(const CockpitPageState& pages) {
    require(Stage::display);
    constexpr std::uint16_t modeXCRTCPort = 0x3d4;
    try {
        auto result = clock_.renderPageFlip(phase_, {PageFlipSynchronization::enabled,
            pages.backPage(), pages.frontPage(), modeXCRTCPort}, horizonCycles_);
        phase_ = result.work.timing.phase;
        stage_ = Stage::postcopy;
        return result;
    } catch (...) {
        stage_ = Stage::rejected;
        throw;
    }
}

ClockedPresentationWork RetainedFrameExecutor::postcopy(const FrameCopyProfile profile) {
    require(Stage::postcopy);
    return advance(frameCopyOperations(profile).operations, Stage::tail);
}

RetainedTailExecution RetainedFrameExecutor::finish(const FrameTailState& state, const FrameTailGuards& guards) {
    require(Stage::tail);
    if (keyboardAccess_ && (phase_.keyboard->used != 0 || phase_.keyboard->scheduled
        || phase_.keyboard->port60Pending || phase_.devices->irq1Input
        || phase_.devices->irq1Requested || phase_.devices->irq1InService)) {
        throw std::invalid_argument("foreground tail requires drained physical packet");
    }
    auto plan = frameTailOperations(state, guards);
    auto work = advance(plan.operations, Stage::prefix);
    return {std::move(plan), std::move(work)};
}

RetainedPrefixExecution RetainedFrameExecutor::prepare(const StationaryLevel2PrefixInput& input) {
    require(Stage::prefix);
    auto plan = retainedLevel2PrefixOperations(input);
    auto work = advance(plan.operations, Stage::render);
    return {std::move(plan), std::move(work)};
}

}
