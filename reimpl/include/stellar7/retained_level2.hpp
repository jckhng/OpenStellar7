#pragma once

#include "stellar7/frame_controller.hpp"
#include "stellar7/retained_frame.hpp"

namespace stellar7 {

struct RetainedLevel2Owners {
    GameplayModel& model;
    const GameplayAssets& assets;
    RetainedFrameExecutor& executor;
    FrameController& controller;
    CockpitPageState& pages;
    RadarState& radar;
    IndexedPages& pixels;
    const HorizonWorkState& horizon;
    const CockpitSettings& settings;
    std::uint8_t& rasterStyle;
    std::optional<std::uint8_t>& enemyFlag;
};

// Callback-scoped references; observe/copy live synth before the next stage.
class RetainedLevel2Observer {
public:
    virtual ~RetainedLevel2Observer() = default;
    virtual void renderEntry(const Level2FramePlan&, const Level2RenderState&) {}
    virtual void body(const Level2FramePlan&, const ClockedPresentationWork&) {}
    virtual void display(const CockpitPageState&, const IndexedPages&, const ClockedPageFlipWork&) {}
    virtual void postcopy(const CockpitPageState&, const IndexedPages&, const ClockedPresentationWork&) {}
    virtual void renderComplete(const Level2FramePlan&) {}
    virtual void prefixEntry(const StationaryLevel2PrefixInput&) {}
    virtual void prefix(const RetainedPrefixExecution&) {}
    virtual void tailEntry(const FrameTailState&, const FrameTailGuards&) {}
    virtual void tail(const RetainedTailExecution&) {}
};

// Borrows canonical owners after an owned tail has been applied by the model.
// Bootstrap, frame-count policy and phase installation remain outside this class.
class RetainedLevel2Renderer final : public Level2FrameRenderer {
public:
    RetainedLevel2Renderer(RetainedLevel2Owners owners, FrameCopyProfile profile,
        RetainedLevel2Observer& observer);
    RetainedLevel2Renderer(const RetainedLevel2Renderer&) = delete;
    RetainedLevel2Renderer& operator=(const RetainedLevel2Renderer&) = delete;
    RetainedLevel2Renderer(RetainedLevel2Renderer&&) = delete;
    RetainedLevel2Renderer& operator=(RetainedLevel2Renderer&&) = delete;
    void advance(const GameplayInput& input);

private:
    enum class State { ready, advancing, rejected };
    enum class Hook { prefix, render, tail };
    void require(Hook hook) const;
    void render(Level2RenderState frame) override;
    Level2PrefixExecution prefixExecution() const noexcept override {
        return Level2PrefixExecution::source;
    }
    std::optional<StationaryLevel2PrefixWork> prepare(const StationaryLevel2PrefixInput& input) override;
    std::optional<FrameTailState> finish(const FrameTailState& state, const FrameTailGuards& guards) override;
    RetainedLevel2Owners owners_;
    FrameCopyProfile profile_;
    RetainedLevel2Observer& observer_;
    State state_ = State::ready;
    Hook hook_ = Hook::prefix;
};

} // namespace stellar7
