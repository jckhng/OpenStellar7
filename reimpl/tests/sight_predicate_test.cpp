#include "stellar7/gameplay.hpp"

#include <cassert>

namespace stellar7 {
struct GameplayModelTestAccess {
    static void configure(GameplayModel& model, std::uint16_t heading,
        std::uint16_t yaw, std::uint16_t tick, std::int16_t type = 8) {
        model.startLevel2();
        model.phase_ = GameplayPhase::level2;
        model.level2LoopTick_ = tick;
        model.level2Player_.position = {};
        model.level2Player_.yaw = yaw;
        model.level2Entities_.assign(1, Level1Entity{});
        auto& entity = model.level2Entities_.front();
        entity.type = type;
        entity.position = {0, 1000, 0};
        entity.targetHeading = heading;
        model.level2ActiveListOrder_ = {0};
    }
    static void refresh(GameplayModel& model) {
        model.refreshLevel2SightPhase();
    }
};
}

class SightRenderer final : public stellar7::Level2FrameRenderer {
public:
    explicit SightRenderer(stellar7::Level2SightExecution execution)
        : execution_(execution) {}
    void render(stellar7::Level2RenderState state) override {
        assert(state.sightExecution == stellar7::Level2SightExecution::pending);
        state.alternateSightPhase = true;
        state.sightExecution = execution_;
    }
private:
    stellar7::Level2SightExecution execution_;
};

int main() {
    using namespace stellar7;
    GameplayModel model;
    // Position is aligned, but the retained presenter heading is not.
    GameplayModelTestAccess::configure(model, 0x9000, 0, 8);
    GameplayModelTestAccess::refresh(model);
    assert(!model.alternateSightPhase());
    GameplayModelTestAccess::configure(model, 0x83ff, 0, 8);
    GameplayModelTestAccess::refresh(model);
    assert(model.alternateSightPhase());
    GameplayModelTestAccess::configure(model, 0x8400, 0, 8);
    GameplayModelTestAccess::refresh(model);
    assert(!model.alternateSightPhase());
    // Native NEG-equivalent absolute value preserves signed8000 overflow.
    GameplayModelTestAccess::configure(model, 0, 0, 8);
    GameplayModelTestAccess::refresh(model);
    assert(model.alternateSightPhase());
    GameplayModelTestAccess::configure(model, 0x9000, 0, 7);
    GameplayModelTestAccess::refresh(model);
    assert(model.alternateSightPhase());
    GameplayModelTestAccess::configure(model, 0x8000, 0, 8, 14);
    GameplayModelTestAccess::refresh(model);
    assert(!model.alternateSightPhase());
    GameplayModelTestAccess::configure(model, 0x8000, 0, 8, -1);
    GameplayModelTestAccess::refresh(model);
    assert(model.alternateSightPhase());
    GameplayModelTestAccess::configure(model, 0x8000, 0, 7, 14);
    SightRenderer pending(Level2SightExecution::pending);
    model.tickWithRenderer({}, pending);
    assert(!model.alternateSightPhase());
    GameplayModelTestAccess::configure(model, 0x8000, 0, 7, 14);
    SightRenderer rendered(Level2SightExecution::rendered);
    model.tickWithRenderer({}, rendered);
    assert(model.alternateSightPhase());
}
