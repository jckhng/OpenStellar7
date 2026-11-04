#pragma once

#include "stellar7/native_session.hpp"

#include <memory>

namespace stellar7 {

enum class CampaignExecution { sourceWork, playable };

// Device-free orchestration of the same recovered startup/session owners.
class RecoveredFrontend {
public:
    RecoveredFrontend(const GameplayAssets& assets, const NativeStartupAssets& startup,
        FrameCopyProfile copyProfile, std::size_t workPacketBudget = 10000,
        std::shared_ptr<HighscoreStore> highscoreStore = {},
        CampaignExecution execution = CampaignExecution::sourceWork,
        NativePointerCapability pointerCapability = NativePointerCapability::unavailable,
        NativeMenuFeatures menuFeatures = NativeMenuFeatures::legacy,NativeJoystickSample joystick = {});
    RecoveredFrontend(const RecoveredFrontend&) = delete;
    RecoveredFrontend& operator=(const RecoveredFrontend&) = delete;

    // Long Begin execution may send a contiguous prefix to sink. The return retains the whole receipt.
    NativeSessionDelivery advanceStartup(NativeMenuInput input = {}, const NativeDeliverySink& sink = {});
    NativeSessionDelivery advanceGameplay(const GameplayInput& input,
        const ContinueKeyboardPacket& continueInput = {},const NameEntryInput& text = {},
        const NativeDeliverySink& sink = {});
    NativeSessionDelivery advanceContinue(const ContinueKeyboardPacket& input = {});
    NativeSessionDelivery paceGameplay(std::uint64_t minimumSampleEnd);
    NativeSessionDelivery advanceRetry();
    const IndexedScreen& frame() const noexcept;
    std::uint64_t elapsedSamples() const noexcept;
    const NativeStartupSequence& startup() const noexcept { return *startup_; }
    const NativeGameplaySession* session() const noexcept { return session_.get(); }
    CampaignExecution execution() const noexcept { return execution_; }
    bool presentationInputActive() const noexcept {
        return attractProgress().has_value() || (session_ ? session_->storyActive()
            : startup_->storyActive() || startup_->titleActive() || startup_->creditsActive());
    }
    std::optional<NativeAttractProgress> attractProgress() const noexcept {
        return session_ ? session_->attractProgress() : startup_->attractProgress();
    }
    bool attractAcceptsSourceKey() const noexcept {
        return session_ ? session_->attractAcceptsSourceKey() : startup_->creditsActive() || startup_->attractAcceptsSourceKey()
            || (startup_->titleActive() && startup_->phase()==NativeStartupPhase::awaitingMenuResources);
    }
    bool joystickCalibrationActive() const noexcept {return session_?session_->joystickCalibrationActive():startup_->joystickCalibrationActive();}
    bool menuBackAvailable() const noexcept {
        return session_?session_->menuBackAvailable():startup_->menuBackAvailable();
    }

private:
    const GameplayAssets& assets_;
    std::shared_ptr<HighscoreStore> highscoreStore_;
    FrameCopyProfile copyProfile_;
    std::size_t workPacketBudget_;
    CampaignExecution execution_;
    NativePointerCapability pointerCapability_;
    NativeMenuFeatures menuFeatures_;
    NativeJoystickSample initialJoystick_{};
    std::unique_ptr<NativeStartupSequence> startup_;
    std::unique_ptr<NativeGameplaySession> session_;
};

} // namespace stellar7
