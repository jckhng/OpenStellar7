#include "stellar7/level1_frame.hpp"

#include <cassert>
#include <memory>
#include <stdexcept>
#include <iostream>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace stellar7 {
struct GameplayModelTestAccess {
    static Level1State& level(GameplayModel& model) { return model.level1_; }
};
struct Level1ReloadTestAccess {
    static void prepare(Level1State& state) {
        state.entities_[0].position = {-8000,-16000,0};
        state.entities_[0].forwardVelocity = 0;
        state.entities_[0].weaponCooldown = 255;
        state.entities_[1].position = {8000,-16000,1000};
        state.entities_[1].behaviorTimer = 20;
        for (std::size_t index = 2; index < state.entities_.size(); ++index) {
            state.entities_[index].position.y = -16000;
        }
    }
    static void hit(Level1State& state, const std::int16_t armor) {
        state.playerArmor_ = armor;
        auto& shot = state.projectiles_[0];
        shot.type = 17;
        shot.collisionRadius = 441;
        shot.position = {};
        shot.forwardVelocity = 500;
        shot.stateBytes4dTo54 = {0xfd,2,0,0,0,0,0,0};
        shot.ownerAllocationIndex = 0;
        state.allocationList_.push_back({Level1State::ObjectKind::projectile,0});
        state.objectListOrder_.push_back({Level1State::ObjectKind::projectile,0});
        state.nativeInitialListPending_ = false;
    }
};
}

namespace {
using namespace stellar7;
GameplayAssets assets() {
    GameplayAssets result;
    result.horizonFrames.assign(4,{256,16,std::vector<std::uint8_t>(256*16,7)});
    result.artGroups.resize(5);
    result.artGroups[3] = {{120,56,std::vector<std::uint8_t>(120*56,7)},
        {4,1,{0,0,0,0}},{120,18,std::vector<std::uint8_t>(120*18,3)}};
    result.artGroups[4].assign(15,{4,1,{0,0,0,0}});
    for (std::size_t index = 7; index < 15; ++index) {
        result.artGroups[4][index] = {8,1,std::vector<std::uint8_t>(8,13)};
    }
    result.models.resize(30);
    result.models[1].collisionRadius = 1342;
    result.models[11].collisionRadius = 748;
    result.models[11].flags = 1;
    result.models[11].animationLimits = {8};
    result.models[17].collisionRadius = 441;
    result.models[20].collisionRadius = 134;
    result.models[23].collisionRadius = 134;
    result.models[29].collisionRadius = 862;
    result.sharedModelRegistry[0] = result.sharedModelRegistry[1] = 256;
    return result;
}

void control(const std::int16_t armor) {
    auto model = std::make_unique<GameplayModel>(GameplayModel::fromNativeStartup());
    auto& level = GameplayModelTestAccess::level(*model);
    Level1ReloadTestAccess::prepare(level);
    const auto data = assets();
    const CockpitSettings settings{90,{}};
    auto pages = CockpitPageState::fromRecoveredMenu();
    auto pixels = std::make_unique<IndexedPages>();
    RadarState radar(data.artGroups[3]);
    for (std::size_t index = 0; index < level.entities().size(); ++index) { radar.initializeAllocation(index); }
    pixels->apply(radar.initializeLevel1(680,{},0,
        {pages.cachePage(),pages.backPage(),pages.frontPage(),pages.backPage(),pages.cachePage()}));
    FrameTailGuards guards{};
    guards.missionActive = true;
    guards.keyboardHead = guards.keyboardTail = 0x1e;
    const auto first = model->advanceRecoveredLevel1Frame(data,settings,pages,radar,*pixels,guards);
    pages = first.render.pages;
    pages.flip();
    *pixels = first.render.pixels;
    radar = first.render.radarState;
    assert(model->ticks() == 1 && level.loopTick() == 1);

    // Controlled component arrival, not a naturally reached gameplay fixture.
    Level1ReloadTestAccess::hit(level,armor);
    radar.initializeAllocation(16);
    const auto events = level.audioEvents().size();
    const auto hit = model->advanceRecoveredLevel1Frame(data,settings,pages,radar,*pixels,guards);
    assert(level.playerArmor() == armor-40 && !level.projectiles()[0].active());
    assert(model->ticks() == 2 && level.loopTick() == 2);
    assert(level.audioEvents().size() == events+1 && level.audioEvents().back().eventId == 6);
    assert(!hit.tail.lowArmorWarningRequested && hit.tail.audioEvents.empty());
    assert(hit.tail.failureRequested == (armor < 40));
    assert(level.missionActive() == (armor >= 40));
    if (armor < 40) {
        assert(model->phase() == GameplayPhase::failurePreExplosionBlackout);
        assert(!model->pendingLevel1FailureTail());
        bool rejected = false;
        try { model->advanceRecoveredLevel1Frame(data,settings,hit.render.pages,hit.render.radarState,hit.render.pixels,guards); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected && model->ticks() == 2 && level.loopTick() == 2);
        return;
    }
    assert(model->phase() == GameplayPhase::level1 && level.playerArmor() == 0);
    pages = hit.render.pages;
    pages.flip();
    const auto adjacent = model->advanceRecoveredLevel1Frame(data,settings,pages,hit.render.radarState,hit.render.pixels,guards);
    assert(model->ticks() == 3 && level.loopTick() == 3 && level.playerArmor() == 0);
    assert(!adjacent.tail.failureRequested && level.audioEvents().size() == events+1);
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _set_error_mode(_OUT_TO_STDERR);
#endif
    try {
        control(40);
        control(0);
    } catch (const std::exception& error) {
        std::cerr << "player combat frame: " << error.what() << '\n';
        return 1;
    }
}
