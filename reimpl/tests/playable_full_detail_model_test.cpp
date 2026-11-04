#include "playable_detail.hpp"
#include "stellar7/gameplay.hpp"

#include <array>
#include <cassert>
#include <stdexcept>

//Controlled owner/charge setup tests one source branch; retained native integration is separate.
namespace stellar7 {
struct Level1ReloadTestAccess {
    static void charges(Level1State& state) {
        state.systemStatusLevels_ = {0, -2, 0, 1, -3, 0, 0};
        state.selectedSystem_ = 0;
        state.systemTimers_ = {0, 400, 0, 17, 300, 0, 0};
    }
};
struct GameplayModelTestAccess {
    static void ordinary(GameplayModel& model) {
        model.playableCampaignFrames_ = true;
        model.phase_ = GameplayPhase::playable;
    }
    static void actor(GameplayModel& model, const std::int16_t type) {
        model.phase_ = GameplayPhase::level2;
        model.activeCampaignLevel_ = 1;
        model.level2Player_ = {};
        Level1Entity actor{};
        actor.type = type;
        actor.collisionRadius = 100;
        actor.position = {0, 500, 0};
        actor.renderProfile = WorldActorRenderProfile::noHandles;
        model.level2PresentationEntities_ = {actor};
        model.level2PresentationActiveListOrder_ = {0};
    }
    static void callbacks(GameplayModel& model) {
        model.level2HasAdvanced_ = true;
        model.campaignModelsBound_ = true;
        model.campaignModels_[11].animationLimit = 8;
        model.campaignModels_[24].animationLimit = 6;
        Level1Entity heap{};
        heap.type = 11;
        heap.animatedModelPresenterEnabled = true;
        heap.renderProfile = WorldActorRenderProfile::heapSelectorDeferredAnimation;
        heap.stateBytes4dTo52[2] = 2;
        heap.stateBytes4dTo52[3] = 2;
        auto fixed = heap; fixed.type = 24;
        model.level2Entities_ = {heap, fixed};
        model.level2ActiveListOrder_ = {0, 1};
        model.level2EntityActorPoolSlots_ = {0, model.level2ActorPoolSize_};
        model.level2EntityFixedPoolSlots_ = {model.level2FixedObjectPoolSize_, 0};
        model.level2EntityProjectilePoolSlots_ = {model.level2ProjectilePoolSize_, model.level2ProjectilePoolSize_};
        model.level2EntityDeathEffectPoolSlots_ = {model.level2DeathEffectPoolSize_, model.level2DeathEffectPoolSize_};
    }
    static bool pendingSettled(const GameplayModel& model) { return model.playableRadarAnimationSettled_; }
    static void charges(GameplayModel& model) {
        Level1ReloadTestAccess::charges(model.level1_);
    }
};
}

int main() {
    using namespace stellar7;
    constexpr std::array<std::int16_t, 9> settings{0,20,21,50,51,70,71,81,82};
    constexpr std::array<std::uint16_t, 9> scales{64,64,65,156,159,218,221,252,255};
    constexpr std::array<std::uint16_t, 9> caps{9,12,17,32,42,52,52,90,128};
    const auto strict = GameplayModel::fromNativeStartup();
    auto legacy = strict;
    GameplayModelTestAccess::ordinary(legacy);
    bool rejected = false;
    try { legacy.synchronizePlayableDetail(0); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    rejected = false;
    try { legacy.resumePlayablePreferences(81); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    for (std::size_t index=0; index<settings.size(); ++index) {
        auto model = GameplayModel::fromNativeStartup(settings[index]);
        const auto& record = model.lastCockpitInitialization();
        assert(record.displaySteps==2);
        assert(model.level2CockpitIndicatorX()==(settings[index]<=50 ? 130 : strict.level2CockpitIndicatorX()));
        //Initial shared-bank attachment resets its descriptor; main preferences do not call8AFC.
        assert(model.playableModelDetail(0).scale==256 && model.playableModelDetail(0).cap==256);
        GameplayModelTestAccess::ordinary(model);
        const auto incoming = model.level2CockpitIndicatorX();
        const auto tick = model.level2LoopTick();
        const auto states = model.resumePlayablePreferences(settings[index]);
        assert(states[0].x==incoming);
        assert(states[2].x==(settings[index]<=50 ? incoming : incoming+8));
        assert(model.level2LoopTick()==tick);
        assert(model.playableModelDetail(0).scale==scales[index] && model.playableModelDetail(43).cap==caps[index]);
        assert(model.playableModelDetail(44).scale==256 && model.playableModelDetail(48).cap==256);
        model.synchronizePlayableDetail(81); //Title Begin synchronizes only the6D85 predicate.
        assert(model.playableModelDetail(0).scale==scales[index] && model.playableModelDetail(0).cap==caps[index]);
        model.resumePlayablePreferences(50);
        const auto frozen = model.level2CockpitIndicatorX();
        model.resumePlayablePreferences(51);
        assert(model.level2CockpitIndicatorX()==frozen+8);
    }
    //A controlled two-LOD mesh proves that descriptor0 changes the actual common renderer.
    ModelGroup group{-1, 0, {{-128,0,-128},{128,0,-128},{0,0,128}},
        {{0,0,{},{{0x80,{7,7,7,7},0,{0,1,2}}}}}};
    Model3d mesh{};
    mesh.collisionRadius=100;
    mesh.lods={{9,0,{group}},{256,0,{group}}};
    GameplayAssets art{};
    art.models.resize(44);
    art.models[1]=mesh;
    art.level2Models={mesh};
    auto renderModel=GameplayModel::fromNativeStartup(81);
    GameplayModelTestAccess::ordinary(renderModel);
    GameplayModelTestAccess::actor(renderModel,1);
    const auto fullTrace=level2GameplayModelDrawTraceJson(art,renderModel);
    assert(fullTrace.find("\"apparent_size\":32,\"lod_threshold\":256")!=std::string::npos);
    renderModel.resumePlayablePreferences(0);
    const auto reducedTrace=level2GameplayModelDrawTraceJson(art,renderModel);
    assert(reducedTrace.find("\"apparent_size\":8,\"lod_threshold\":9")!=std::string::npos);
    GameplayModelTestAccess::actor(renderModel,44);
    const auto localTrace=level2GameplayModelDrawTraceJson(art,renderModel);
    assert(localTrace.find("\"apparent_size\":32,\"lod_threshold\":256")!=std::string::npos);

    //One pending ordinary130C plus one extra modal698A; Type24's ordinaryradar was already delivered.
    auto callbacks = GameplayModel::fromNativeStartup(0);
    GameplayModelTestAccess::ordinary(callbacks);
    GameplayModelTestAccess::callbacks(callbacks);
    const auto world = callbacks.level2LoopTick();
    const auto rng = callbacks.level2Rng().calls();
    callbacks.resumePlayablePreferences(0);
    assert(callbacks.level2Entities()[0].stateBytes4dTo52[3]==0);
    assert(callbacks.level2Entities()[1].stateBytes4dTo52[3]==1);
    assert(GameplayModelTestAccess::pendingSettled(callbacks));
    callbacks.resumePlayablePreferences(0);
    assert(callbacks.level2Entities()[0].stateBytes4dTo52[3]==2
        &&callbacks.level2Entities()[0].modelAnimationVariant==1);
    assert(callbacks.level2Entities()[1].stateBytes4dTo52[3]==0);
    assert(callbacks.level2LoopTick()==world && callbacks.level2Rng().calls()==rng);

    auto charges = GameplayModel::fromNativeStartup(0);
    GameplayModelTestAccess::ordinary(charges);
    GameplayModelTestAccess::charges(charges);
    const auto timers = charges.level1State().systemTimers();
    const auto events = charges.level2AudioEvents().size();
    charges.resumePlayablePreferences(0);
    assert((charges.level1State().systemStatusLevels()==std::array<std::int8_t,7>{0,2,0,1,3,0,0}));
    //System1 is still negative when source8ED2(0) cycles, so source chooses system3.
    assert(charges.level1State().selectedSystem()==3 && charges.level1State().systemTimers()==timers);
    assert(charges.level2AudioEvents().size()==events+1 && charges.level2AudioEvents().back().eventId==16);
}
