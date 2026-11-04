#include "stellar7/gameplay.hpp"

#include <cassert>

namespace stellar7 {
struct GameplayModelTestAccess {
    static void initializeCampaign(GameplayModel& model) {
        model.initializeLevel2({}, GameplayModel::Level2EntryOrigin::campaign);
    }
    static bool fire(GameplayModel& model) {
        auto& owner = model.level2Entities_.front();
        owner.targetHeading = owner.yaw;
        owner.weaponCooldown = 0;
        owner.weaponSlotTimers = {};
        return model.fireLevel2EnemyProjectile(0);
    }
    static std::size_t allocateActor(GameplayModel& model, Level1Entity entity) {
        return model.allocateLevel2Actor(entity);
    }
    static std::size_t allocateFixedObject(GameplayModel& model, Level1Entity entity) {
        return model.allocateLevel2FixedObject(entity);
    }
    static void assertOrdinaryIncarnationCarry() {
        GameplayModel model;
        initializeCampaign(model);
        model.phase_ = GameplayPhase::level2Handoff;
        assert(model.level2PresentationActorIncarnation(0) == 1);
        assert(model.level2PresentationActorIncarnation(4) == 1);
        assert(!model.level2PresentationActorIncarnation(5));
        auto replacement = model.level2Entities_[0];
        replacement.type = 1;
        auto& retiring = model.level2Entities_[4];
        retiring.position = {};
        retiring.xVelocity = 0;
        retiring.yVelocity = 0;
        model.tick({});
        assert(!model.level2ActorPoolActive_[4]);
        assert(allocateActor(model, replacement) == 4);
        assert(!model.level2PresentationActorIncarnation(0));
        snapshot(model);
        assert(model.level2PresentationPoolSlot(4) == 4);
        assert(model.level2PresentationActorIncarnation(4) == 2);
        assert(model.level2PresentationActorIncarnation(0) == 1);
        //A later same-type allocation must not change an already delivered snapshot.
        ++model.level2ActorIncarnations_[4];
        assert(model.level2PresentationActorIncarnation(4) == 2);
        snapshot(model);
        assert(model.level2PresentationActorIncarnation(4) == 3);
        assert(model.level2PresentationActorIncarnation(0) == 1);
    }
    static void snapshot(GameplayModel& model) {
        model.snapshotLevel2Presentation();
    }
};
}

int main() {
    using namespace stellar7;
    assert(Level1Entity{}.renderProfile == WorldActorRenderProfile::unknown);
    GameplayModel model;
    model.startLevel2();
    assert(model.level2Entities().size() == 7);
    for (const auto& entity : model.level2Entities()) {
        assert(entity.renderProfile == WorldActorRenderProfile::noHandles);
    }
    model.tick({});
    for (const auto& entity : model.level2Entities()) {
        if (entity.type == 8 || entity.type == 13 || entity.type == 30) {
            assert(entity.renderProfile == WorldActorRenderProfile::noHandles);
        }
    }
    auto copied = model.level2Entities().front();
    copied.type = 11;
    const auto actor = GameplayModelTestAccess::allocateActor(model, copied);
    assert(actor != static_cast<std::size_t>(-1));
    assert(model.level2Entities()[actor].renderProfile == WorldActorRenderProfile::unknown);
    auto fixed = copied;
    fixed.type = 30;
    const auto fixedIndex = GameplayModelTestAccess::allocateFixedObject(model, fixed);
    assert(fixedIndex != static_cast<std::size_t>(-1));
    assert(model.level2Entities()[fixedIndex].renderProfile == WorldActorRenderProfile::unknown);
    model.startLevel3();
    for (const auto& entity : model.level2Entities()) {
        assert(entity.renderProfile == WorldActorRenderProfile::unknown);
    }
    GameplayModel campaign;
    GameplayModelTestAccess::initializeCampaign(campaign);
    assert(campaign.level2PresentationPoolSlot(5) == 16);
    assert(GameplayModelTestAccess::fire(campaign));
    const auto& projectile = campaign.level2Entities().back();
    assert(projectile.type == 19);
    assert(projectile.renderProfile == WorldActorRenderProfile::inlineSelectorNoCallback);
    assert(campaign.level2PresentationPoolSlot(5) == 16);
    assert(campaign.level2PresentationPoolSlot(6) == 17);
    GameplayModelTestAccess::allocateActor(campaign, copied);
    assert(!campaign.level2PresentationPoolSlot(6));

    for (const auto type : {1, 4, 8, 13}) {
        auto ordinary = copied;
        ordinary.type = type;
        const auto index = GameplayModelTestAccess::allocateActor(campaign, ordinary);
        assert(index != static_cast<std::size_t>(-1));
        assert(campaign.level2Entities()[index].renderProfile == WorldActorRenderProfile::noHandles);
    }
    GameplayModelTestAccess::assertOrdinaryIncarnationCarry();

}
