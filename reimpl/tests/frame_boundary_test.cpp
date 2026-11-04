#include "stellar7/gameplay.hpp"
#include "stellar7/frame_controller.hpp"

#include <cassert>
#include <stdexcept>

namespace stellar7 {
struct GameplayModelTestAccess {
    static void configureTail(GameplayModel& model) {
        model.level2Player_.weaponCooldown = 8;
        model.level2Player_.weaponSlotTimers = {10, 12, 0};
    }

    static const VisibilityState& visibility(const GameplayModel& model) {
        return model.level2Visibility_;
    }

    static void configureTailMotion(GameplayModel& model) {
        model.level2Player_.forwardVelocity = 30;
        model.level2Player_.yawVelocity = 80;
    }

    static void configureUnsupportedVisibilityActor(GameplayModel& model) {
        model.level2Entities_.front().type = 7;
    }

    static void configureCompletionContact(GameplayModel& model) {
        for (auto& entity : model.level2Entities_) {
            entity.position = {20000, -20000, 0};
        }
        auto& pickup = model.level2Entities_.front();
        pickup.type = 25;
        pickup.flags = 0;
        pickup.collisionRadius = 897;
        pickup.position = model.level2Player_.position;
    }
};
}

namespace {

class GroundRenderer final : public stellar7::Level2FrameRenderer {
public:
    unsigned calls = 0;
    stellar7::FixedPosition firstPosition{};

    void render(stellar7::Level2RenderState state) override {
        ++calls;
        firstPosition = state.entities.at(state.order.at(0)).position;
        state.ground.advance(state.player.yawVelocity, state.player.forwardVelocity, state.rng);
    }
};

class TailRenderer final : public stellar7::Level2FrameRenderer {
public:
    unsigned tails = 0;
    stellar7::FrameController controller;
    stellar7::FrameTailState incoming{};
    stellar7::FrameTailState returned{};

    void render(stellar7::Level2RenderState state) override {
        state.ground.advance(state.player.yawVelocity, state.player.forwardVelocity, state.rng);
    }

    std::optional<stellar7::FrameTailState> finish(
        const stellar7::FrameTailState& state,
        const stellar7::FrameTailGuards& guards) override {
        assert(!guards.firstFrame);
        if (tails == 0) {
            controller.initialize({stellar7::FrameControllerOrigin::sourceColdDefaults,
                state.loopTick, 0x1e, 0x1e, {}, 0, false, false, false, false, false, {}});
        }
        assert(state.loopTick == controller.expectedLoopTick());
        incoming = state;
        returned = stellar7::frameTailOperations(state, controller.augmentGuards(guards)).state;
        controller.consumeEmptyTail(state.loopTick);
        ++tails;
        return returned;
    }
};

class RetainingRenderer final : public stellar7::Level2FrameRenderer {
public:
    unsigned calls = 0;

    void render(stellar7::Level2RenderState state) override {
        if (calls != 0) {
            assert(state.ground.points().front() == (stellar7::GroundPoint{-32, -32}));
        }
        std::array<stellar7::GroundPoint, stellar7::Level1GroundState::pointCount> points{};
        points.fill({-32, -32});
        state.ground = stellar7::Level1GroundState(points);
        state.rng.next();
        ++calls;
    }
};

class PrefixRenderer final : public stellar7::Level2FrameRenderer {
public:
    TailRenderer tail;
    unsigned preparations = 0;
    std::optional<stellar7::StationaryLevel2PrefixWork> prepared;

    stellar7::Level2PrefixExecution prefixExecution() const noexcept override {
        return stellar7::Level2PrefixExecution::source;
    }

    std::optional<stellar7::StationaryLevel2PrefixWork> prepare(
        const stellar7::StationaryLevel2PrefixInput& input) override {
        assert(input.player.position == tail.returned.playerPosition);
        assert(!input.refreshRequested && input.lastListCamera == input.player.position);
        for (std::size_t position = 0; position < input.activeOrder.size(); ++position) {
            assert(input.actors.at(input.activeOrder[position]).position
                == tail.returned.actors.at(position).position);
        }
        prepared = stellar7::stationaryLevel2PrefixOperations(input);
        ++preparations;
        return prepared;
    }

    void render(stellar7::Level2RenderState state) override {
        if (prepared) {
            assert(state.order == prepared->activeOrder);
            for (std::size_t index = 0; index < state.entities.size(); ++index) {
                assert(state.entities[index].position == prepared->actors[index].position);
                assert(state.entities[index].yaw == prepared->actors[index].yaw);
                assert(state.entities[index].presenterDistance == prepared->actors[index].presenterDistance);
            }
        }
        tail.render(state);
    }

    std::optional<stellar7::FrameTailState> finish(const stellar7::FrameTailState& state,
        const stellar7::FrameTailGuards& guards) override {
        return tail.finish(state, guards);
    }
};

class CockpitRenderer final : public stellar7::Level2FrameRenderer {
public:
    std::int16_t incomingX = 0;
    std::int16_t incomingVelocity = 0;
    std::int16_t returnedX = 0;
    std::int16_t returnedVelocity = 0;
    stellar7::Level2CockpitExecution execution = stellar7::Level2CockpitExecution::pending;

    void render(stellar7::Level2RenderState state) override {
        assert(state.cockpitExecution == stellar7::Level2CockpitExecution::pending);
        incomingX = state.cockpitIndicatorX;
        incomingVelocity = state.cockpitIndicatorVelocity;
        state.ground.advance(state.player.yawVelocity, state.player.forwardVelocity, state.rng);
        if (execution == stellar7::Level2CockpitExecution::rendered) {
            state.cockpitIndicatorX = returnedX;
            state.cockpitIndicatorVelocity = returnedVelocity;
            state.cockpitExecution = execution;
        }
    }
};

void sameGround(const stellar7::GameplayModel& first, const stellar7::GameplayModel& second) {
    for (std::size_t index = 0; index < first.level2Ground().points().size(); ++index) {
        assert(first.level2Ground().points()[index].x == second.level2Ground().points()[index].x);
        assert(first.level2Ground().points()[index].y == second.level2Ground().points()[index].y);
        assert(first.level2Ground().pointVisible(index) == second.level2Ground().pointVisible(index));
    }
    auto firstRng = first.level2Rng();
    auto secondRng = second.level2Rng();
    assert(firstRng.calls() == secondRng.calls());
    for (std::size_t index = 0; index < stellar7::StellarRng::wordCount; ++index) {
        assert(firstRng.next() == secondRng.next());
    }
}

}

int main() {
    stellar7::GameplayModel prefixModel;
    prefixModel.startLevel2();
    assert(prefixModel.level2Player().weaponSlotCount == 0);
    prefixModel.tick({});
    PrefixRenderer prefixRenderer;
    prefixModel.tickWithRenderer({}, prefixRenderer);
    assert(prefixRenderer.preparations == 0 && prefixRenderer.tail.tails == 1);
    const auto prefixLoop = prefixModel.level2LoopTick();
    prefixModel.tickWithRenderer({}, prefixRenderer);
    assert(prefixRenderer.preparations == 1 && prefixRenderer.tail.tails == 2);
    assert(prefixModel.level2LoopTick() == prefixLoop + 1);
    auto unadmittedPrefix = prefixModel;
    stellar7::GameplayInput newInput{};
    newInput.fire = true;
    for (const auto& input : {newInput, stellar7::GameplayInput{}}) {
        bool rejected = false;
        try {
            unadmittedPrefix.tickWithRenderer(input, prefixRenderer);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected && prefixRenderer.preparations == 1);
    }

    stellar7::GameplayModel tailModel;
    tailModel.startLevel2();
    assert(stellar7::GameplayModelTestAccess::visibility(tailModel).lastCamera()
        == tailModel.level2Player().position);
    assert(!stellar7::GameplayModelTestAccess::visibility(tailModel).refreshRequested());
    tailModel.tick({});
    stellar7::GameplayModelTestAccess::configureTail(tailModel);
    auto tailLegacy = tailModel;
    TailRenderer tailRenderer;
    for (unsigned frame = 0; frame < 3; ++frame) {
        tailLegacy.tick({});
        tailModel.tickWithRenderer({}, tailRenderer);
        assert(tailRenderer.tails == frame + 1);
        assert(stellar7::GameplayModelTestAccess::visibility(tailModel).tracking()
            == stellar7::VisibilityTracking::sourceInitialized);
        assert(tailModel.level2LoopTick() == tailLegacy.level2LoopTick() + 1);
        assert(tailModel.level2Player().weaponCooldown == tailRenderer.returned.weaponTimers[0]);
        assert(tailModel.level2Player().weaponSlotTimers[0] == tailRenderer.returned.weaponTimers[1]);
        assert(tailModel.level2Player().weaponSlotTimers[1] == tailRenderer.returned.weaponTimers[2]);
        assert(tailModel.level2Player().weaponCooldown
            == (tailLegacy.level2Player().weaponCooldown == 0 ? 0
                : tailLegacy.level2Player().weaponCooldown - 1));
        for (std::size_t index = 0; index < tailModel.level2Entities().size(); ++index) {
            assert(tailModel.level2Entities()[index].position == tailLegacy.level2Entities()[index].position);
        }
    }
    GroundRenderer resumedRenderer;
    tailLegacy.tick({});
    tailModel.tickWithRenderer({}, resumedRenderer);
    assert(tailModel.level2LoopTick() == tailLegacy.level2LoopTick());
    assert(tailModel.level2Player().weaponCooldown == tailLegacy.level2Player().weaponCooldown);
    assert(tailModel.level2Player().weaponSlotTimers == tailLegacy.level2Player().weaponSlotTimers);
    stellar7::GameplayInput moving{};
    moving.forward = true;
    const auto lastListCamera = stellar7::GameplayModelTestAccess::visibility(tailModel).lastCamera();
    tailModel.tick(moving);
    tailModel.tick(moving);
    assert(stellar7::GameplayModelTestAccess::visibility(tailModel).tracking()
        == stellar7::VisibilityTracking::sourceInitialized);
    assert(stellar7::GameplayModelTestAccess::visibility(tailModel).lastCamera() == lastListCamera);
    stellar7::GameplayModelTestAccess::configureUnsupportedVisibilityActor(tailModel);
    tailModel.tick({});
    assert(stellar7::GameplayModelTestAccess::visibility(tailModel).tracking()
        == stellar7::VisibilityTracking::unknown);
    tailModel.startLevel2();
    assert(stellar7::GameplayModelTestAccess::visibility(tailModel).tracking()
        == stellar7::VisibilityTracking::unknown);

    stellar7::GameplayModel movingTail;
    movingTail.startLevel2();
    movingTail.tick({});
    stellar7::GameplayModelTestAccess::configureTailMotion(movingTail);
    auto movingLegacy = movingTail;
    TailRenderer movingRenderer;
    for (unsigned frame = 0; frame < 2; ++frame) {
        movingLegacy.tick({});
        movingTail.tickWithRenderer({}, movingRenderer);
        assert(movingTail.level2Player().position == stellar7::FixedPosition{});
        assert(movingTail.level2Player().forwardVelocity == movingRenderer.returned.forwardVelocity);
        for (std::size_t position = 0; position < movingRenderer.incoming.actors.size(); ++position) {
            const auto index = movingTail.level2ActiveListOrder().at(position);
            assert(movingTail.level2PresentationEntities().at(index).position
                == movingRenderer.incoming.actors[position].position);
            assert(movingTail.level2PresentationEntities().at(index).position
                == movingLegacy.level2Entities().at(index).position);
            assert(movingTail.level2Entities().at(index).position
                == movingRenderer.returned.actors[position].position);
        }
    }
    movingLegacy.tick({});
    movingTail.tickWithRenderer({}, resumedRenderer);
    assert(movingTail.level2LoopTick() == movingLegacy.level2LoopTick());
    assert(movingTail.level2Player().position == movingLegacy.level2Player().position);
    for (std::size_t index = 0; index < movingTail.level2Entities().size(); ++index) {
        assert(movingTail.level2Entities()[index].position == movingLegacy.level2Entities()[index].position);
    }

    stellar7::GameplayModel retained;
    GroundRenderer renderer;
    bool rejected = false;
    try {
        retained.tickWithRenderer({}, renderer);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected && retained.ticks() == 0 && renderer.calls == 0);
    retained.startLevel2();
    retained.tick({});
    auto ordinary = retained;
    for (unsigned frame = 0; frame < 16; ++frame) {
        ordinary.tick({});
        retained.tickWithRenderer({}, renderer);
        assert(renderer.calls == frame + 1);
        assert(retained.ticks() == ordinary.ticks());
        assert(retained.level2LoopTick() == ordinary.level2LoopTick());
        assert(retained.level2ActiveListOrder() == ordinary.level2ActiveListOrder());
        for (std::size_t index = 0; index < retained.level2Entities().size(); ++index) {
            assert(retained.level2Entities()[index].position == ordinary.level2Entities()[index].position);
            assert(retained.level2Entities()[index].yaw == ordinary.level2Entities()[index].yaw);
        }
        assert(renderer.firstPosition == retained.level2Entities().at(retained.level2ActiveListOrder().at(0)).position);
        sameGround(retained, ordinary);
        assert(retained.level2CockpitIndicatorX() == ordinary.level2CockpitIndicatorX());
        assert(retained.level2CockpitIndicatorVelocity() == ordinary.level2CockpitIndicatorVelocity());
    }
    RetainingRenderer retainingRenderer;
    const auto previousDraws = retained.level2Rng().calls();
    for (unsigned frame = 0; frame < 2; ++frame) {
        retained.tickWithRenderer({}, retainingRenderer);
        assert(retained.level2Ground().points().front() == (stellar7::GroundPoint{-32, -32}));
        assert(retained.level2Rng().calls() == previousDraws + frame + 1);
    }
    auto cockpitModel = ordinary;
    CockpitRenderer cockpit;
    cockpit.execution = stellar7::Level2CockpitExecution::rendered;
    cockpit.returnedX = 130;
    cockpit.returnedVelocity = -4;
    const auto precedingX = cockpitModel.level2CockpitIndicatorX();
    cockpitModel.tickWithRenderer({}, cockpit);
    assert(cockpit.incomingX == precedingX && cockpitModel.level2CockpitIndicatorX() == 130);
    cockpit.returnedX = 178;
    cockpit.returnedVelocity = 4;
    cockpitModel.tickWithRenderer({}, cockpit);
    assert(cockpit.incomingX == 130 && cockpit.incomingVelocity == -4);
    assert(cockpitModel.level2CockpitIndicatorX() == 178);
    assert(cockpitModel.level2CockpitIndicatorVelocity() == 4);
    cockpit.execution = stellar7::Level2CockpitExecution::pending;
    auto pendingLegacy = cockpitModel;
    pendingLegacy.tick({});
    cockpitModel.tickWithRenderer({}, cockpit);
    assert(cockpit.incomingX == 178 && cockpit.incomingVelocity == 4);
    assert(cockpitModel.level2CockpitIndicatorX() == 174);
    assert(cockpitModel.level2CockpitIndicatorVelocity() == -4);
    assert(cockpitModel.level2CockpitIndicatorX() == pendingLegacy.level2CockpitIndicatorX());
    cockpit.execution = stellar7::Level2CockpitExecution::rendered;
    cockpit.returnedX = 130;
    cockpit.returnedVelocity = -4;
    cockpitModel.tickWithRenderer({}, cockpit);
    cockpit.execution = stellar7::Level2CockpitExecution::pending;
    pendingLegacy = cockpitModel;
    pendingLegacy.tick({});
    cockpitModel.tickWithRenderer({}, cockpit);
    assert(cockpit.incomingX == 130 && cockpit.incomingVelocity == -4);
    assert(cockpitModel.level2CockpitIndicatorX() == 134);
    assert(cockpitModel.level2CockpitIndicatorVelocity() == 4);
    assert(cockpitModel.level2CockpitIndicatorX() == pendingLegacy.level2CockpitIndicatorX());
    stellar7::GameplayModel earlyExit;
    earlyExit.startLevel2();
    earlyExit.tick({});
    stellar7::GameplayModelTestAccess::configureCompletionContact(earlyExit);
    auto earlyLegacy = earlyExit;
    GroundRenderer earlyRenderer;
    earlyLegacy.tick({});
    earlyExit.tickWithRenderer({}, earlyRenderer);
    assert(earlyRenderer.calls == 0);
    assert(earlyExit.phase() == earlyLegacy.phase());
    assert(earlyExit.level2CockpitIndicatorX() == earlyLegacy.level2CockpitIndicatorX());
    assert(earlyExit.level2CockpitIndicatorVelocity() == earlyLegacy.level2CockpitIndicatorVelocity());
}
