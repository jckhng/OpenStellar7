#include "stellar7/level1.hpp"
#include "stellar7/level1_trace.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace stellar7 {

struct Level1ReloadTestAccess {
    static void retainedState(Level1State& state) {
        state.systemStatusLevels_ = {1, 2, 3, 1, 2, 3, 1};
        state.systemTimers_ = {11, 12, 13, 14, 15, 16, 17};
        state.selectedSystem_ = 2;
        state.lastDestroyedType_ = 11;
        state.consecutiveDestroyedTypeCount_ = 2;
    }

    static void simultaneousProjectileExpiry(Level1State& state) {
        state.hasAdvanced_ = true;
        state.activeListOrder_.clear();
        state.semanticActiveListOrder_.clear();
        state.allocationList_.clear();
        state.objectListOrder_.clear();
        for (auto& actor : state.entities_) {
            actor.flags = 0x80;
        }
        for (const auto slot : {0U, 1U, 3U, 4U, 5U}) {
            auto& projectile = state.projectiles_[slot];
            projectile.flags = 0;
            projectile.type = 23;
            projectile.position = {10000, 10000, 0};
            projectile.forwardVelocity = 0;
            projectile.ownerIsPlayer = true;
            projectile.stateBytes4dTo54 = {253, 20, 0, 0, 0, 0, 0, 0};
            state.allocationList_.push_back({Level1State::ObjectKind::projectile, slot});
        }
        state.projectiles_[0].stateBytes4dTo54[1] = 1;
        state.projectiles_[3].stateBytes4dTo54[1] = 1;
        for (const auto slot : {3U, 0U, 1U, 4U, 5U}) {
            state.objectListOrder_.push_back({Level1State::ObjectKind::projectile, slot});
        }
        state.pendingPostBoundaryInput_.fire = true;
    }

    static void assertRadarPresentationOrder() {
        Level1State state;
        using Kind = Level1State::ObjectKind;
        state.objectListOrder_ = {{Kind::projectile, 3}, {Kind::entity, 1}, {Kind::deathEffect, 2}};
        auto& projectile = state.projectiles_[3];
        projectile.type = 17;
        projectile.position = {111, 222, 333};
        projectile.collisionRadius = 500;
        projectile.stateBytes4dTo54[0] = 253;
        projectile.stateBytes4dTo54[1] = 20;
        auto& effect = state.deathEffects_[2];
        effect.entity.type = 21;
        effect.entity.position = {444, 555, 666};
        effect.entity.stateBytes4dTo52[0] = 252;
        effect.listed = false;
        const auto rngCalls = state.rng().calls();
        const auto objects = state.radarPresentationObjects();
        assert(objects.size() == 3);
        assert(objects[0].kind == Level1PresentationKind::projectile && objects[0].slot == 3);
        assert(objects[0].type == 17 && objects[0].category == -3 && objects[0].radius == 500);
        assert(objects[0].position.x == 111 && objects[0].position.z == 333);
        assert(objects[0].renderProfile == WorldActorRenderProfile::unknown);
        assert(objects[1].kind == Level1PresentationKind::entity && objects[1].slot == 1);
        assert(objects[1].type == state.entities_[1].type);
        // The draw list can still contain a terminal effect after allocation retirement.
        assert(objects[2].kind == Level1PresentationKind::deathEffect && objects[2].slot == 2);
        assert(objects[2].category == -4 && objects[2].position.y == 555);
        assert(state.rng().calls() == rngCalls);
        std::swap(state.objectListOrder_[0], state.objectListOrder_[2]);
        const auto reordered = state.radarPresentationObjects();
        assert(reordered[0].kind == Level1PresentationKind::deathEffect && reordered[0].slot == 2);
        assert(reordered[2].kind == Level1PresentationKind::projectile && reordered[2].slot == 3);
    }

    static void resetSharedTransientState(Level1State& state) {
        state.resetCampaignTransientState();
    }
};

}

void testLevel1Entry() {
    stellar7::Level1ReloadTestAccess::assertRadarPresentationOrder();
    stellar7::Level1State expiry;
    stellar7::Level1ReloadTestAccess::simultaneousProjectileExpiry(expiry);
    expiry.advanceIdleTick();
    assert((expiry.projectileOrder() == std::vector<std::size_t>{5, 1, 2, 4}));
    assert(!expiry.projectiles()[0].active() && !expiry.projectiles()[3].active());
    assert(expiry.projectiles()[2].active());

    stellar7::Level1State level;
    stellar7::Level1State retry;
    auto sharedReset = retry;
    stellar7::Level1ReloadTestAccess::retainedState(sharedReset);
    const auto resetRngCalls = sharedReset.rng().calls();
    const auto resetGround = sharedReset.ground().points();
    const auto resetScore = sharedReset.score();
    stellar7::Level1ReloadTestAccess::resetSharedTransientState(sharedReset);
    assert((sharedReset.systemTimers() == std::array<std::int16_t, 7>{0, 0, 0, 14, 0, 0, 0}));
    assert((sharedReset.systemStatusLevels() == std::array<std::int8_t, 7>{1, 2, 3, 1, 2, 3, 1}));
    assert(sharedReset.selectedSystem() == 2 && sharedReset.lastDestroyedType() == 11);
    assert(sharedReset.consecutiveDestroyedTypeCount() == 0);
    assert(sharedReset.rng().calls() == resetRngCalls);
    assert(sharedReset.ground().points() == resetGround && sharedReset.score() == resetScore);
    for (unsigned step = 0; step < 32; ++step) {
        retry.advanceTick({false, false, false, false, true});
    }
    auto failureRng = retry.rng();
    for (unsigned call = 0; call < 7; ++call) {
        failureRng.next();
    }
    auto expectedRng = failureRng;
    for (unsigned call = 0; call < 4; ++call) {
        expectedRng.next();
    }
    std::array<stellar7::GroundPoint, stellar7::Level1GroundState::pointCount> expectedPoints{};
    for (auto& point : expectedPoints) {
        point.x = static_cast<std::int16_t>(
            (static_cast<std::int16_t>(expectedRng.next()) % 295) * 32);
        point.y = static_cast<std::int16_t>(
            (static_cast<std::int16_t>(expectedRng.next()) % 56 + 90) * 32);
    }
    stellar7::Level1ReloadTestAccess::retainedState(retry);
    const auto retainedStatus = retry.systemStatusLevels();
    retry.reloadAfterFailure(failureRng);
    assert(retry.loopTick() == 0);
    assert(retry.playerArmor() == 680);
    assert(retry.score() == 0);
    assert(retry.missionActive());
    assert(!retry.failureState());
    assert(retry.playerWeaponCooldown() == 0);
    assert(retry.entities().size() == 6);
    assert(retry.systemStatusLevels() == retainedStatus);
    assert(retry.selectedSystem() == 2);
    assert((retry.systemTimers() == std::array<std::int16_t, 7>{0, 0, 0, 14, 0, 0, 0}));
    assert(retry.lastDestroyedType() == 11);
    assert(retry.consecutiveDestroyedTypeCount() == 0);
    assert(retry.rng().calls() == expectedRng.calls());
    assert(retry.rng().indexA() == expectedRng.indexA());
    assert(retry.rng().indexB() == expectedRng.indexB());
    for (std::size_t index = 0; index < expectedPoints.size(); ++index) {
        assert(retry.ground().points()[index].x == expectedPoints[index].x);
        assert(retry.ground().points()[index].y == expectedPoints[index].y);
    }
    assert(retry.entities()[0].type == 1);
    assert(retry.entities()[0].yaw == 0xb000);
    assert(retry.entities()[1].type == 11);
    assert(retry.entities()[1].position.z == 1000);
    assert(retry.entities()[1].stateBytes4dTo52[2] == 5);
    assert(retry.entities()[1].stateBytes4dTo52[3] == 0);
    assert(retry.entities()[1].modelAnimationVariant == 0);
    assert(retry.entities()[5].stateBytes4dTo52[1] == 16);
    assert(retry.entities()[5].stateBytes4dTo52[4] == 243);
    auto expectedContinuation = expectedRng;
    assert(retry.rng().last() == expectedContinuation.last());

    assert(level.selectedSystem() == -1);
    auto retainedSelection = level;
    for (const auto input : std::array<stellar7::Level1Input, 6>{{
             {}, {true, false, false, false, false},
             {false, true, false, false, false}, {false, false, true, false, false},
             {false, false, false, true, false}, {false, false, false, false, true}}}) {
        retainedSelection.advanceTick(input);
        assert(retainedSelection.selectedSystem() == -1);
    }
    const auto copiedSelection = retainedSelection;
    assert(copiedSelection.selectedSystem() == -1);

    assert(level.levelIndex() == 0);
    assert(level.loopTick() == 1);
    assert(level.entities().size() == 6);
    assert(level.player().allocationIndex == 6);
    assert(level.player().flags == 0);
    assert(level.player().type == 0);
    assert(level.player().collisionRadius == 723);
    assert((level.player().position == stellar7::FixedPosition{0, 0, 0}));
    assert(level.player().pitch == 0);
    assert(level.player().roll == 0);
    assert(level.player().yaw == 0);
    assert(level.player().yawVelocity == 0);
    assert(level.player().forwardVelocity == 0);
    assert((
        level.player().stateBytes4dTo52
        == std::array<std::uint8_t, 6>{0, 1, 0, 0, 0, 0}
    ));

    const std::array<std::int16_t, 6> types{1, 11, 29, 29, 29, 29};
    const std::array<stellar7::FixedPosition, 6> positions{{
        {-2048, 2048, 0},
        {8192, 8192, 1000},
        {16384, 16384, 0},
        {16384, -16384, 0},
        {-16384, 16384, 0},
        {-16384, -16384, 0},
    }};
    const std::array<std::uint16_t, 6> yaws{
        0xB000, 0x4800, 0, 0, 0, 0,
    };
    const std::array<std::int16_t, 6> speeds{225, 270, 0, 0, 0, 0};
    const std::array<std::array<std::uint8_t, 6>, 6> states{{
        {{255, 1, 0, 0, 1, 0}},
        {{255, 1, 5, 4, 3, 0}},
        {{254, 100, 0, 0, 0, 0}},
        {{254, 100, 0, 0, 0, 0}},
        {{254, 100, 0, 0, 0, 0}},
        {{254, 16, 0, 0, 243, 0}},
    }};
    for (std::size_t index = 0; index < level.entities().size(); ++index) {
        const auto& entity = level.entities()[index];
        assert(entity.allocationIndex == index);
        assert(entity.flags == 0);
        assert(entity.type == types[index]);
        assert(entity.position == positions[index]);
        assert(entity.pitch == 0);
        assert(entity.roll == 0);
        assert(entity.yaw == yaws[index]);
        assert(entity.pitchVelocity == 0);
        assert(entity.rollVelocity == 0);
        assert(entity.yawVelocity == 0);
        assert(entity.xVelocity == 0);
        assert(entity.yVelocity == 0);
        assert(entity.zVelocity == 0);
        assert(entity.forwardVelocity == speeds[index]);
        assert(entity.stateBytes4dTo52 == states[index]);
    }

    assert(level.rng().indexA() == 102);
    assert(level.rng().indexB() == 52);
    assert(level.rng().last() == 38480);
    assert(level.rng().calls() == 29);
    auto rng = level.rng();
    assert(rng.next() == 39518);
    assert(rng.indexA() == 100);
    assert(rng.indexB() == 50);
    assert(rng.calls() == 30);

    assert(
        stellar7::approximatePlanarDistance(
            {0, 0, 0},
            {-1841, 1961, 0}
        )
        == 2651
    );
    assert(
        stellar7::approximatePlanarDistance(
            {-45, -21, 0},
            {-32813, 8385, 0}
        )
        == 35919
    );
    assert(
        stellar7::fixedHeadingBetween(
            {-1841, 1961, 0},
            {0, 0, 0}
        )
        == 0x9EA0
    );
    assert(
        stellar7::fixedHeadingBetween(
            {7927, 8139, 0},
            {0, 0, 0}
        )
        == 0x60A0
    );
    assert(
        stellar7::fixedHeadingBetween(
            {0, 0, 0},
            {0, 1, 0}
        )
        == 0
    );
    assert(
        stellar7::fixedHeadingBetween(
            {0, 0, 0},
            {1, 0, 0}
        )
        == 0xC000
    );
    assert(
        stellar7::fixedHeadingBetween(
            {0, 0, 0},
            {2000, 15528, 0}
        )
        == 64224
    );

    // Natural-success-v4 target ticks 3774 -> 3775.  This exercises the
    // original's staged 32-bit multiply in the non-zero-pitch path.
    stellar7::Level1Entity pitchedProjectile{
        0, 0, 0, 0, {14195, -6210, 2000},
        0xFB20, 0, 12112,
        0, 0, 0,
        0, 0, 0,
        500,
        {},
    };
    stellar7::advanceEntityKinematics(pitchedProjectile);
    assert(pitchedProjectile.position.x == 13739);
    assert(pitchedProjectile.position.y == -6013);
    assert(pitchedProjectile.position.z == 1940);

    level.advanceEntityKinematics();
    assert((level.entities()[0].position == stellar7::FixedPosition{
        -1841, 1961, 0,
    }));
    assert((level.entities()[1].position == stellar7::FixedPosition{
        7927, 8139, 1000,
    }));
    for (std::size_t index = 2; index < level.entities().size(); ++index) {
        assert(level.entities()[index].position == positions[index]);
    }

    const auto mainView = stellar7::initialLevel1MainView();
    assert((mainView.origin == stellar7::FixedPosition{0, 0, 0}));
    assert((mainView.matrix.values == std::array<std::int16_t, 9>{
        16384, 0, 0,
        0, 16384, 0,
        0, 0, 16384,
    }));
    assert(mainView.matrix.mode == 0);
    auto failureViewMatrix = stellar7::makeFixedMatrixQ14(0, 0, 0x4000);
    stellar7::transpose(failureViewMatrix);
    const auto capturedFailureActorMatrix =
        stellar7::composeFixedMatricesQ14(
            stellar7::makeFixedMatrixQ14(0, 0, 44107),
            failureViewMatrix,
            0x5240
        );
    assert((
        capturedFailureActorMatrix.values
        == std::array<std::int16_t, 9>{
            -14496, 7633, 0,
            -7634, -14499, 0,
            0, 0, 16384,
        }
    ));
    assert(capturedFailureActorMatrix.mode == 1);
    assert((mainView.viewport == stellar7::Viewport{24, 294, 24, 145}));
    assert(mainView.perspectiveScale == 128);
    assert((
        stellar7::transformToView(
            level.entities()[0].position,
            mainView
        )
        == stellar7::FixedPosition{-1841, 1961, 0}
    ));

    const auto radarView = stellar7::initialLevel1RadarView();
    assert((radarView.origin == stellar7::FixedPosition{0, 0, 200000}));
    assert((radarView.matrix.values == std::array<std::int16_t, 9>{
        16384, 0, 0,
        0, 0, 16384,
        0, -16384, 0,
    }));
    auto generatedRadarMatrix = stellar7::makeFixedMatrixQ14(
        0xC000,
        0,
        0
    );
    stellar7::transpose(generatedRadarMatrix);
    assert(generatedRadarMatrix.values == radarView.matrix.values);
    assert(generatedRadarMatrix.mode == radarView.matrix.mode);
    const auto radarLocalMatrix = stellar7::makeFixedMatrixQ14(
        0xC000,
        0,
        0
    );
    const auto steeredTargetMatrix = stellar7::makeFixedMatrixQ14(
        0,
        0,
        0x17E0
    );
    auto steeredRadarMatrix = stellar7::composeFixedMatricesQ14(
        radarLocalMatrix,
        steeredTargetMatrix,
        0x4A00
    );
    stellar7::transpose(steeredRadarMatrix);
    assert((steeredRadarMatrix.values == std::array<std::int16_t, 9>{
        13649, 1, -9059,
        9061, 1, 13651,
        0, -16384, 0,
    }));
    assert(radarView.matrix.mode == 3);
    assert((radarView.viewport == stellar7::Viewport{112, 205, 146, 180}));
    assert(radarView.perspectiveScale == 128);
    assert((
        stellar7::transformToView(
            level.entities()[0].position,
            radarView
        )
        == stellar7::FixedPosition{-1841, 200000, 1961}
    ));

    const std::array<stellar7::FixedPosition, 6> radarPositions{{
        {16384, 16384, 0},
        {16384, -16384, 0},
        {-16384, 16384, 0},
        {-16384, -16384, 0},
        level.entities()[1].position,
        level.entities()[0].position,
    }};
    const std::array<stellar7::ProjectedPoint, 6> projected{{
        {168, 153},
        {168, 173},
        {148, 153},
        {148, 173},
        {163, 158},
        {157, 162},
    }};
    for (std::size_t index = 0; index < radarPositions.size(); ++index) {
        const auto viewPosition = stellar7::transformToView(
            radarPositions[index],
            radarView
        );
        assert(
            stellar7::projectToViewport(viewPosition, radarView)
            == projected[index]
        );
    }

    stellar7::Level1GroundState ground;
    assert((ground.points() == std::array<stellar7::GroundPoint, 6>{{
        {-6688, 1280},
        {2208, 1152},
        {-768, 4320},
        {-1216, 2016},
        {6240, 1248},
        {3776, 3616},
    }}));
    stellar7::StellarRng groundRng;
    for (std::uint32_t call = 0; call < 32; ++call) {
        groundRng.next();
    }
    ground.advance(0, 0, groundRng);
    assert((ground.points() == std::array<stellar7::GroundPoint, 6>{{
        {-8992, 3296},
        {4992, 2528},
        {2816, 1728},
        {1888, 2144},
        {-7328, 4384},
        {3776, 3616},
    }}));
    assert(groundRng.indexA() == 76);
    assert(groundRng.indexB() == 26);
    assert(groundRng.last() == 19647);
    assert(groundRng.calls() == 42);

    stellar7::Level1State firstTick;
    firstTick.advanceIdleTick();
    assert(firstTick.loopTick() == 1);
    assert(firstTick.rng().calls() == 42);
    assert(firstTick.rng().indexA() == 76);
    assert(firstTick.rng().indexB() == 26);
    assert(firstTick.rng().last() == 19647);
    assert((firstTick.ground().points() == ground.points()));
    const auto& type1 = firstTick.entities()[0];
    assert((type1.position == stellar7::FixedPosition{-1841, 1961, 0}));
    assert(type1.yawVelocity == -143);
    assert(type1.forwardVelocity == 0);
    assert((type1.stateBytes4dTo52 == std::array<std::uint8_t, 6>{
        255, 1, 0, 0, 1, 2,
    }));
    assert(type1.behaviorTimer == 0);
    assert(type1.presenterDistance == 2651);
    assert(type1.targetHeading == 0x9EA0);
    const auto& type11 = firstTick.entities()[1];
    assert((type11.position == stellar7::FixedPosition{7927, 8139, 1000}));
    assert(type11.yawVelocity == 0);
    assert(type11.forwardVelocity == 270);
    assert((type11.stateBytes4dTo52 == std::array<std::uint8_t, 6>{
        255, 1, 5, 3, 3, 0,
    }));
    assert(type11.behaviorTimer == 10);
    assert(type11.presenterDistance == 11111);
    assert(type11.targetHeading == 0x60A0);
    assert(firstTick.type11AnimationVariant() == 1);
    firstTick.advanceIdleTick();
    assert(firstTick.loopTick() == 2);
    assert(firstTick.entities()[1].stateBytes4dTo52[3] == 1);
    assert(firstTick.type11AnimationVariant() == 1);
    firstTick.advanceIdleTick();
    assert(firstTick.loopTick() == 3);
    assert(firstTick.entities()[1].stateBytes4dTo52[3] == 5);
    assert(firstTick.type11AnimationVariant() == 2);
    for (std::uint32_t tick = 3; tick < 20; ++tick) {
        firstTick.advanceIdleTick();
    }
    assert(firstTick.loopTick() == 20);
    assert(firstTick.type11AnimationVariant() == 7);
    assert(firstTick.rng().calls() == 102);
    assert(firstTick.rng().indexA() == 68);
    assert(firstTick.rng().indexB() == 18);
    assert(firstTick.rng().last() == 65248);
    assert(firstTick.entities()[0].stateBytes4dTo52[5] == 2);
    firstTick.advanceIdleTick();
    firstTick.advanceIdleTick();
    assert(firstTick.loopTick() == 22);
    assert(firstTick.entities()[1].stateBytes4dTo52[3] == 3);
    assert(firstTick.rng().calls() == 106);
    assert(firstTick.rng().indexA() == 60);
    assert(firstTick.rng().indexB() == 10);
    assert(firstTick.rng().last() == 64344);

    stellar7::Level1State bonusPickupContact;
    auto& bonusEntities = const_cast<std::vector<stellar7::Level1Entity>&>(
        bonusPickupContact.entities()
    );
    auto& bonus = bonusEntities[0];
    bonus.type = 42;
    bonus.collisionRadius = 330;
    bonus.position = {0, 0, 100};
    bonus.forwardVelocity = 0;
    bonus.stateBytes4dTo52 = {254, 1, 3, 0, 0, 3};
    bonusPickupContact.advanceIdleTick();
    assert(!bonusPickupContact.entities()[0].active());
    assert(bonusPickupContact.score() == 100);
    assert(bonusPickupContact.player().forwardVelocity == 0);
    assert(bonusPickupContact.audioEvents().size() == 1);
    assert(bonusPickupContact.audioEvents()[0].loopTick == 1);
    assert(bonusPickupContact.audioEvents()[0].eventId == 24);

    stellar7::Level1State playerControl;
    playerControl.advanceTick({true, false, false, false, false});
    assert(playerControl.player().yaw == 0);
    assert(playerControl.player().yawVelocity == 0);
    playerControl.advanceTick({true, false, false, false, false});
    assert(playerControl.player().yaw == 0x50);
    assert(playerControl.player().yawVelocity == 0x50);
    playerControl.advanceTick({true, false, false, false, false});
    assert(playerControl.player().yaw == 0xF0);
    assert(playerControl.player().yawVelocity == 0xA0);
    stellar7::Level1State playerThrottle;
    playerThrottle.advanceTick({false, false, true, false, false});
    assert(playerThrottle.player().forwardVelocity == 0);
    assert((
        playerThrottle.player().position
        == stellar7::FixedPosition{0, 0, 0}
    ));
    playerThrottle.advanceTick({false, false, true, false, false});
    assert(playerThrottle.player().forwardVelocity == 10);
    playerThrottle.advanceTick({false, false, true, false, false});
    assert(playerThrottle.player().forwardVelocity == 20);
    assert((
        playerThrottle.player().position
        == stellar7::FixedPosition{0, 20, 0}
    ));
    assert(playerThrottle.entities()[2].position.y == 16374);

    stellar7::Level1State firstNaturalAudio;
    firstNaturalAudio.advanceTick(
        {true, false, true, false, true}
    );
    assert(firstNaturalAudio.audioEvents().empty());
    firstNaturalAudio.advanceIdleTick();
    assert(firstNaturalAudio.audioEvents().size() == 1);
    assert(firstNaturalAudio.audioEvents()[0].loopTick == 1);
    assert(firstNaturalAudio.audioEvents()[0].eventId == 14);
    const auto firstNaturalAudioTrace = stellar7::level1StateTraceJson(
        firstNaturalAudio,
        "level1-audio-event"
    );
    assert(
        firstNaturalAudioTrace.find(
            "\"audio_events\":[{\"loop_tick\":1,\"event_id\":14}]"
        ) != std::string::npos
    );

    stellar7::Level1State deferredPlayerFire;
    for (std::uint32_t tick = 1; tick < 42; ++tick) {
        deferredPlayerFire.advanceIdleTick();
    }
    deferredPlayerFire.advanceTick({false, false, false, false, true});
    assert(deferredPlayerFire.loopTick() == 42);
    assert(!deferredPlayerFire.projectiles()[0].active());
    deferredPlayerFire.advanceIdleTick();
    assert(deferredPlayerFire.loopTick() == 43);
    assert(deferredPlayerFire.projectiles()[0].active());
    assert((
        deferredPlayerFire.projectiles()[0].position
        == stellar7::FixedPosition{0, 0, 0}
    ));
    assert(deferredPlayerFire.projectiles()[0].stateBytes4dTo54[1] == 64);
    assert((
        deferredPlayerFire.activeListOrder()
        == std::vector<std::size_t>{0, 1, 2, 3, 4, 5}
    ));

    stellar7::Level1State movingSecondShot;
    for (std::uint32_t tick = 1; tick <= 56; ++tick) {
        const bool steering = tick >= 42;
        const bool firstFire = tick >= 42 && tick < 50;
        const bool secondFire = tick >= 55;
        movingSecondShot.advanceTick({
            steering,
            false,
            steering,
            false,
            firstFire || secondFire,
        });
    }
    assert(movingSecondShot.loopTick() == 56);
    assert(movingSecondShot.projectiles()[1].active());
    assert((
        movingSecondShot.projectiles()[1].position
        == stellar7::FixedPosition{-63, 113, 0}
    ));
    assert(movingSecondShot.projectiles()[1].yaw == 5264);
    assert(movingSecondShot.projectiles()[1].stateBytes4dTo54[1] == 64);

    stellar7::Level1State scriptedType3RangeGate;
    for (std::uint32_t tick = 1; tick <= 126; ++tick) {
        scriptedType3RangeGate.advanceTick({
            tick >= 42 && tick < 60,
            false,
            tick >= 42 && tick < 70,
            false,
            (tick >= 42 && tick < 50)
                || (tick >= 55 && tick < 57),
        });
    }
    assert(scriptedType3RangeGate.loopTick() == 126);
    assert(scriptedType3RangeGate.entities().size() == 7);
    assert(scriptedType3RangeGate.entities()[6].type == 3);
    assert(scriptedType3RangeGate.entities()[6].presenterDistance == 38829);
    assert(scriptedType3RangeGate.entities()[6].behaviorTimer == 0);
    assert(scriptedType3RangeGate.entities()[6].weaponSlotTimers[0] == 0);
    for (const auto& candidate : scriptedType3RangeGate.projectiles()) {
        assert(!candidate.active());
    }

    stellar7::Level1State scriptedType3CruiseRangeGate;
    for (std::uint32_t tick = 1; tick <= 144; ++tick) {
        scriptedType3CruiseRangeGate.advanceTick({
            tick >= 42 && tick < 60,
            false,
            tick >= 42 && tick < 70,
            false,
            (tick >= 42 && tick < 50)
                || (tick >= 55 && tick < 57),
        });
    }
    const auto& cruiseType3 = scriptedType3CruiseRangeGate.entities()[6];
    assert(cruiseType3.type == 3);
    assert(cruiseType3.stateBytes4dTo52[5] == 1);
    assert(cruiseType3.presenterDistance == 38599);
    assert(cruiseType3.behaviorTimer == 12);
    assert(cruiseType3.weaponSlotTimers[0] == 0);
    for (const auto& candidate :
         scriptedType3CruiseRangeGate.projectiles()) {
        assert(!candidate.active());
    }

    stellar7::Level1State firstProjectile;
    for (std::uint32_t tick = 1; tick < 7; ++tick) {
        firstProjectile.advanceIdleTick();
    }
    const auto rngCallsBeforeFire = firstProjectile.rng().calls();
    assert(firstProjectile.firePrimary());
    assert(firstProjectile.rng().calls() == rngCallsBeforeFire);
    assert(firstProjectile.playerWeaponCooldown() == 9);
    assert((
        firstProjectile.playerProjectileTimers()
        == std::array<std::uint8_t, 2>{32, 0}
    ));
    assert(!firstProjectile.firePrimary());
    const auto& projectile = firstProjectile.projectiles()[0];
    assert(projectile.active());
    assert(projectile.poolSlot == 0);
    assert(projectile.flags == 0);
    assert(projectile.type == 23);
    assert(projectile.collisionRadius == 134);
    assert((projectile.position == stellar7::FixedPosition{0, 0, 0}));
    assert(projectile.pitch == 0);
    assert(projectile.roll == 0);
    assert(projectile.yaw == 0);
    assert(projectile.pitchVelocity == 0);
    assert(projectile.rollVelocity == 0);
    assert(projectile.yawVelocity == 0);
    assert(projectile.xVelocity == 0);
    assert(projectile.yVelocity == 0);
    assert(projectile.zVelocity == 0);
    assert(projectile.forwardVelocity == 500);
    assert((
        projectile.stateBytes4dTo54
        == std::array<std::uint8_t, 8>{253, 64, 0, 0, 0, 0, 0, 0}
    ));
    assert(projectile.ownerIsPlayer);
    firstProjectile.advanceIdleTick();
    assert(firstProjectile.playerWeaponCooldown() == 8);
    assert((
        firstProjectile.playerProjectileTimers()
        == std::array<std::uint8_t, 2>{31, 0}
    ));
    assert(firstProjectile.projectiles()[0].active());
    assert(firstProjectile.projectiles()[0].stateBytes4dTo54[1] == 63);
    assert((
        firstProjectile.projectiles()[0].position
        == stellar7::FixedPosition{0, 500, 0}
    ));
    assert(firstProjectile.score() == 0);

    stellar7::Level1State enemyAttack;
    for (std::uint32_t tick = 1; tick <= 29; ++tick) {
        enemyAttack.advanceIdleTick();
    }
    assert(enemyAttack.loopTick() == 29);
    assert(enemyAttack.playerArmor() == 680);
    assert(enemyAttack.missionActive());
    assert(!enemyAttack.failureState());
    assert(enemyAttack.entities()[0].yaw == 41052);
    assert(enemyAttack.entities()[0].behaviorTimer == 1);
    assert(enemyAttack.entities()[0].weaponCooldown == 8);
    assert((
        enemyAttack.entities()[0].weaponSlotTimers
        == std::array<std::uint8_t, 3>{54, 0, 0}
    ));
    const auto& enemyProjectile = enemyAttack.projectiles()[0];
    assert(enemyProjectile.active());
    assert(enemyProjectile.type == 17);
    assert(enemyProjectile.collisionRadius == 441);
    assert((
        enemyProjectile.position
        == stellar7::FixedPosition{-1841, 1961, 0}
    ));
    assert(enemyProjectile.yaw == 41052);
    assert(enemyProjectile.forwardVelocity == 500);
    assert((
        enemyProjectile.stateBytes4dTo54
        == std::array<std::uint8_t, 8>{253, 54, 0, 0, 0, 0, 0, 0}
    ));
    assert(!enemyProjectile.ownerIsPlayer);
    assert(enemyProjectile.ownerAllocationIndex == 0);
    assert(enemyAttack.audioEvents().size() == 1);
    assert(enemyAttack.audioEvents()[0].loopTick == 29);
    assert(enemyAttack.audioEvents()[0].eventId == 2);
    enemyAttack.advanceIdleTick();
    assert(enemyAttack.loopTick() == 30);
    assert(enemyAttack.entities()[0].yaw == 40909);
    assert(enemyAttack.entities()[0].weaponCooldown == 7);
    assert((
        enemyAttack.entities()[0].weaponSlotTimers
        == std::array<std::uint8_t, 3>{54, 0, 0}
    ));
    assert(enemyAttack.projectiles()[0].stateBytes4dTo54[1] == 53);
    assert((
        enemyAttack.projectiles()[0].position
        == stellar7::FixedPosition{-1485, 1610, 0}
    ));
    enemyAttack.advanceIdleTick();
    enemyAttack.advanceIdleTick();
    assert(enemyAttack.playerArmor() == 680);
    enemyAttack.advanceIdleTick();
    assert(enemyAttack.loopTick() == 33);
    assert(!enemyAttack.projectiles()[0].active());
    assert((
        enemyAttack.projectiles()[0].position
        == stellar7::FixedPosition{-417, 557, 0}
    ));
    assert(enemyAttack.playerArmor() == 640);
    assert(enemyAttack.missionActive());
    assert(!enemyAttack.failureState());
    assert(enemyAttack.audioEvents().size() == 2);
    assert(enemyAttack.audioEvents()[1].loopTick == 33);
    assert(enemyAttack.audioEvents()[1].eventId == 6);

    stellar7::Level1State firstRuntimeSpawn;
    for (std::uint32_t tick = 1; tick <= 49; ++tick) {
        firstRuntimeSpawn.advanceIdleTick();
    }
    assert(firstRuntimeSpawn.loopTick() == 49);
    assert(firstRuntimeSpawn.activeEnemyCount() == 3);
    assert(firstRuntimeSpawn.spawnedEnemyCount() == 3);
    assert(firstRuntimeSpawn.maximumEnemyCount() == 10);
    assert(firstRuntimeSpawn.entities().size() == 7);
    assert(firstRuntimeSpawn.lastSpawnTick() == 48);
    assert(firstRuntimeSpawn.lastSpawnRng().calls() == 157);
    assert(firstRuntimeSpawn.lastSpawnRng().indexA() == 70);
    assert(firstRuntimeSpawn.lastSpawnRng().indexB() == 20);
    assert(firstRuntimeSpawn.lastSpawnRng().last() == 59494);
    const auto* spawned = firstRuntimeSpawn.lastSpawnCheckpoint();
    assert(spawned != nullptr);
    assert(spawned->allocationIndex == 6);
    assert(spawned->type == 1);
    assert(spawned->collisionRadius == 1342);
    assert((
        spawned->position
        == stellar7::FixedPosition{-22529, -32769, 0}
    ));
    assert(spawned->yaw == 0);
    assert(spawned->forwardVelocity == 225);
    assert((
        spawned->stateBytes4dTo52
        == std::array<std::uint8_t, 6>{255, 1, 0, 0, 1, 0}
    ));
    assert(spawned->weaponSlotCount == 2);
    assert(spawned->weaponCooldown == 0);
    assert((
        spawned->weaponSlotTimers
        == std::array<std::uint8_t, 3>{0, 0, 0}
    ));

    stellar7::Level1State reusedSelectorSlot;
    auto& reusableEntities = const_cast<std::vector<stellar7::Level1Entity>&>(
        reusedSelectorSlot.entities()
    );
    reusableEntities[1].stateBytes4dTo52[1] = 0;
    for (
        std::uint32_t tick = 1;
        tick <= 2000 && !reusedSelectorSlot.entities()[1].active();
        ++tick
    ) {
        reusedSelectorSlot.advanceIdleTick();
    }
    assert(reusedSelectorSlot.entities()[1].active());
    assert(reusedSelectorSlot.entities()[1].allocationIndex == 1);
    assert(
        reusedSelectorSlot.entities()[1].primaryType11AnimationSelector
    );
    const auto& activeSpawn = firstRuntimeSpawn.entities()[6];
    assert((
        activeSpawn.position
        == stellar7::FixedPosition{-22529, -32769, 0}
    ));
    assert((
        activeSpawn.stateBytes4dTo52
        == std::array<std::uint8_t, 6>{255, 1, 0, 0, 1, 0}
    ));
    assert(activeSpawn.behaviorTimer == 0);
    assert(activeSpawn.presenterDistance == 0);
    assert(activeSpawn.targetHeading == 0);
    assert(firstRuntimeSpawn.entities()[0].yawVelocity == -256);
    assert(firstRuntimeSpawn.rng().calls() == 158);
    assert(firstRuntimeSpawn.rng().indexA() == 68);
    assert(firstRuntimeSpawn.rng().indexB() == 18);
    assert(firstRuntimeSpawn.rng().last() == 65032);
    firstRuntimeSpawn.advanceIdleTick();
    assert(firstRuntimeSpawn.loopTick() == 50);
    const auto& firstSpawnCallback = firstRuntimeSpawn.entities()[6];
    assert((
        firstSpawnCallback.position
        == stellar7::FixedPosition{-22529, -32544, 0}
    ));
    assert(firstSpawnCallback.yawVelocity == -143);
    assert(firstSpawnCallback.forwardVelocity == 0);
    assert(firstSpawnCallback.stateBytes4dTo52[5] == 0);
    assert(firstSpawnCallback.behaviorTimer == 0);
    assert(firstSpawnCallback.presenterDistance == 40992);
    assert(firstSpawnCallback.targetHeading == 59232);
    assert(firstRuntimeSpawn.rng().calls() == 161);
    assert(firstRuntimeSpawn.rng().indexA() == 62);
    assert(firstRuntimeSpawn.rng().indexB() == 12);
    assert(firstRuntimeSpawn.rng().last() == 35152);

    stellar7::Level1State firstEntityContact;
    for (std::uint32_t tick = 1; tick <= 43; ++tick) {
        firstEntityContact.advanceIdleTick();
    }
    assert(firstEntityContact.loopTick() == 43);
    const auto& contactType1 = firstEntityContact.entities()[0];
    assert(contactType1.stateBytes4dTo52[5] == 3);
    assert(contactType1.behaviorTimer == 1);
    assert(contactType1.forwardVelocity == 0);
    assert(contactType1.yawVelocity == 0);
    assert(contactType1.collisionContactAllocationIndex == 1);
    const auto& contactType11 = firstEntityContact.entities()[1];
    assert(contactType11.stateBytes4dTo52[5] == 2);
    assert(contactType11.position.z == 950);
    assert(
        contactType11.collisionContactAllocationIndex
        == static_cast<std::size_t>(-1)
    );
    firstEntityContact.advanceIdleTick();
    assert(firstEntityContact.loopTick() == 44);
    assert((
        firstEntityContact.entities()[0].position
        == stellar7::FixedPosition{-1841, 1961, 0}
    ));
    assert(firstEntityContact.entities()[0].yaw == 40608);
    assert(firstEntityContact.entities()[0].yawVelocity == 256);
    assert(firstEntityContact.entities()[0].forwardVelocity == 225);
    firstEntityContact.advanceIdleTick();
    assert(firstEntityContact.loopTick() == 45);
    assert((
        firstEntityContact.entities()[0].position
        == stellar7::FixedPosition{-1841, 1961, 0}
    ));
    assert(firstEntityContact.entities()[0].yaw == 40864);
    assert(firstEntityContact.entities()[0].yawVelocity == -256);
    assert(firstEntityContact.entities()[0].forwardVelocity == -45);

    stellar7::Level1State firstHeadingWordWrap;
    for (std::uint32_t tick = 1; tick <= 183; ++tick) {
        firstHeadingWordWrap.advanceIdleTick();
    }
    assert(firstHeadingWordWrap.loopTick() == 183);
    const auto& wrappedHeadingType11 =
        firstHeadingWordWrap.entities()[1];
    assert((
        wrappedHeadingType11.position
        == stellar7::FixedPosition{-9677, -32812, 1000}
    ));
    assert(wrappedHeadingType11.pitch == 0);
    assert(wrappedHeadingType11.roll == 62107);
    assert(wrappedHeadingType11.yaw == 39299);
    assert(wrappedHeadingType11.yawVelocity == -271);
    assert((
        wrappedHeadingType11.stateBytes4dTo52
        == std::array<std::uint8_t, 6>{255, 1, 5, 5, 3, 1}
    ));
    assert(wrappedHeadingType11.presenterDistance == 36440);
    assert(wrappedHeadingType11.targetHeading == 35744);
    assert(firstHeadingWordWrap.rng().calls() == 285);
    assert(firstHeadingWordWrap.rng().indexA() == 38);
    assert(firstHeadingWordWrap.rng().indexB() == 100);
    assert(firstHeadingWordWrap.rng().last() == 34849);
    for (std::uint32_t tick = 184; tick <= 193; ++tick) {
        firstHeadingWordWrap.advanceIdleTick();
    }
    assert(firstHeadingWordWrap.loopTick() == 193);
    const auto& unsignedTranslationType11 =
        firstHeadingWordWrap.entities()[1];
    assert((
        unsignedTranslationType11.position
        == stellar7::FixedPosition{-8433, 30335, 1000}
    ));
    assert(unsignedTranslationType11.pitch == 0);
    assert(unsignedTranslationType11.roll == 260);
    assert(unsignedTranslationType11.yaw == 36589);
    assert(unsignedTranslationType11.yawVelocity == -271);
    assert(unsignedTranslationType11.presenterDistance == 33497);
    assert(unsignedTranslationType11.targetHeading == 35584);
    assert(firstHeadingWordWrap.rng().calls() == 307);
    assert(firstHeadingWordWrap.rng().indexA() == 106);
    assert(firstHeadingWordWrap.rng().indexB() == 56);
    assert(firstHeadingWordWrap.rng().last() == 36211);

    // Pristine main-boundary captures show FUN_1000_3768 replacing the
    // depth-sorted active list with allocation order when the main-view
    // actor moves at least 0x400 world units from its stored projection
    // origin. The collision rebound crosses that threshold in both
    // directions on ticks 547 and 548.
    stellar7::Level1State firstProjectionThresholdRebuild;
    for (std::uint32_t tick = 1; tick <= 546; ++tick) {
        firstProjectionThresholdRebuild.advanceIdleTick();
    }
    assert((
        firstProjectionThresholdRebuild.activeListOrder()
        == std::vector<std::size_t>{0, 3, 5, 4, 2, 6, 1}
    ));
    firstProjectionThresholdRebuild.advanceIdleTick();
    assert(firstProjectionThresholdRebuild.loopTick() == 547);
    assert((
        firstProjectionThresholdRebuild.activeListOrder()
        == std::vector<std::size_t>{0, 1, 2, 3, 4, 5, 6}
    ));
    firstProjectionThresholdRebuild.advanceIdleTick();
    assert(firstProjectionThresholdRebuild.loopTick() == 548);
    assert((
        firstProjectionThresholdRebuild.activeListOrder()
        == std::vector<std::size_t>{0, 1, 2, 3, 4, 5, 6}
    ));
    firstProjectionThresholdRebuild.advanceIdleTick();
    assert(firstProjectionThresholdRebuild.loopTick() == 549);
    assert((
        firstProjectionThresholdRebuild.activeListOrder()
        == std::vector<std::size_t>{0, 3, 5, 4, 2, 6, 1}
    ));
    for (std::uint32_t tick = 550; tick <= 582; ++tick) {
        firstProjectionThresholdRebuild.advanceIdleTick();
    }
    assert(firstProjectionThresholdRebuild.loopTick() == 582);
    const auto& prePlayerContact =
        firstProjectionThresholdRebuild.entities()[6];
    assert((
        prePlayerContact.position
        == stellar7::FixedPosition{-1129, -831, 0}
    ));
    assert(prePlayerContact.yaw == 57520);
    assert(prePlayerContact.yawVelocity == 240);
    assert(prePlayerContact.forwardVelocity == 225);
    assert(prePlayerContact.stateBytes4dTo52[5] == 1);
    assert(prePlayerContact.behaviorTimer == 113);
    firstProjectionThresholdRebuild.advanceIdleTick();
    assert(firstProjectionThresholdRebuild.loopTick() == 583);
    const auto& firstPlayerContact =
        firstProjectionThresholdRebuild.entities()[6];
    assert((
        firstPlayerContact.position
        == stellar7::FixedPosition{-1129, -831, 0}
    ));
    assert(firstPlayerContact.yaw == 57760);
    assert(firstPlayerContact.yawVelocity == 256);
    assert(firstPlayerContact.forwardVelocity == -45);
    assert(firstPlayerContact.stateBytes4dTo52[5] == 4);
    assert(firstPlayerContact.behaviorTimer == 63);
    assert(firstProjectionThresholdRebuild.rng().calls() == 1024);
    for (std::uint32_t tick = 584; tick <= 747; ++tick) {
        firstProjectionThresholdRebuild.advanceIdleTick();
    }
    assert(firstProjectionThresholdRebuild.loopTick() == 747);
    const auto& preType11AbsoluteXResidue =
        firstProjectionThresholdRebuild.entities()[1];
    assert(preType11AbsoluteXResidue.stateBytes4dTo52[5] == 3);
    assert((preType11AbsoluteXResidue.position
        == stellar7::FixedPosition{13349, -18482, 150}));
    firstProjectionThresholdRebuild.advanceIdleTick();
    assert(firstProjectionThresholdRebuild.loopTick() == 748);
    const auto& firstType11AbsoluteXResidue =
        firstProjectionThresholdRebuild.entities()[1];
    assert(firstType11AbsoluteXResidue.stateBytes4dTo52[5] == 5);
    assert((firstType11AbsoluteXResidue.position
        == stellar7::FixedPosition{13313, -18215, 150}));
    assert(firstProjectionThresholdRebuild.rng().calls() == 1250);
    firstProjectionThresholdRebuild.advanceIdleTick();
    assert(firstProjectionThresholdRebuild.loopTick() == 749);
    assert((firstProjectionThresholdRebuild.entities()[1].position
        == stellar7::FixedPosition{13270, -17949, 250}));

    stellar7::Level1State objective;
    for (
        std::uint32_t step = 0;
        step < 10000 && objective.completionObject() == nullptr;
        ++step
    ) {
        // The target's top-level Type 11 selector lives in allocation slot
        // one. Reusing that slot must retain the selector identity rather
        // than treating it as state owned by the destroyed entity.
        const auto& selectorSlot = objective.entities()[1];
        if (selectorSlot.active() && selectorSlot.type == 11) {
            assert(
                objective.type11AnimationVariant()
                == selectorSlot.modelAnimationVariant
            );
        }
        const stellar7::Level1Entity* target = nullptr;
        for (const auto& candidate : objective.entities()) {
            if (
                !candidate.active()
                || static_cast<std::int8_t>(
                    candidate.stateBytes4dTo52[0]
                ) != -1
                || std::abs(candidate.position.z)
                    > static_cast<std::int32_t>(
                        (candidate.collisionRadius + 134U) >> 1U
                    )
            ) {
                continue;
            }
            if (
                target == nullptr
                || candidate.presenterDistance
                    < target->presenterDistance
            ) {
                target = &candidate;
            }
        }
        if (target == nullptr) {
            objective.advanceIdleTick();
            continue;
        }
        const auto difference = static_cast<std::int16_t>(
            stellar7::fixedHeadingBetween(
                objective.player().position,
                target->position
            ) - objective.player().yaw
        );
        const auto desiredVelocity = std::clamp(
            static_cast<int>(difference) / 4,
            -0x200,
            0x200
        );
        const auto currentVelocity = objective.player().yawVelocity;
        objective.advanceTick({
            currentVelocity < desiredVelocity - 0x28,
            currentVelocity > desiredVelocity + 0x28,
            true,
            false,
            true,
        });
        assert(objective.missionActive());
        assert(!objective.failureState());
    }
    const auto* completion = objective.completionObject();
    assert(completion != nullptr);
    assert(objective.spawnedEnemyCount() == 11);
    assert(objective.activeEnemyCount() == 1);
    assert(completion->type == 7);
    assert(completion->collisionRadius == 13052);
    assert(completion->position.z == 5000);
    assert(completion->forwardVelocity == 400);
    assert((
        completion->stateBytes4dTo52
        == std::array<std::uint8_t, 6>{255, 100, 0, 0, 5, 0}
    ));
    // Original-DOS checkpoint tick 3553 initializes the newly allocated
    // Type 7 completion object with three weapon slots and cleared timers.
    assert(completion->weaponSlotCount == 3);
    assert((
        completion->weaponSlotTimers
        == std::array<std::uint8_t, 3>{0, 0, 0}
    ));
    // FUN_1000_4370 consumes one RNG value in FUN_1000_4862 for the
    // allocator's default position and another for its default yaw. The
    // completion routine overwrites both values, but preserves this exact
    // tick-3553 RNG state before the ground presenter runs.
    assert(objective.rng().calls() == 16793);
    assert(objective.rng().indexA() == 62);
    assert(objective.rng().indexB() == 12);
    assert(objective.rng().last() == 45244);
    assert(
        std::any_of(
            objective.audioEvents().begin(),
            objective.audioEvents().end(),
            [](const stellar7::Level1AudioEvent& event) {
                return event.eventId == 17;
            }
        )
    );
    auto completionTick3554 = objective;
    completionTick3554.advanceIdleTick();
    completion = completionTick3554.completionObject();
    assert(completion != nullptr);
    // Original-DOS tick 3554 proves that FUN_1000_0c6a excludes Type 7
    // from player-origin recentering and FUN_1000_ac3a applies its bank.
    assert((
        completion->position
        == stellar7::FixedPosition{-25017, 13174, 5000}
    ));
    assert(completion->roll == 65520);
    assert(completion->yaw == 44096);
    assert(completion->yawVelocity == 16);
    assert(completion->targetHeading == 44112);
    assert(completion->presenterDistance == 30158);
    for (
        std::uint32_t step = 0;
        step < 1000 && objective.postWaveObject() == nullptr;
        ++step
    ) {
        objective.advanceIdleTick();
        assert(objective.missionActive());
    }
    const auto* postWave = objective.postWaveObject();
    assert(postWave != nullptr);
    assert(objective.spawnedEnemyCount() == 12);
    assert(objective.activeEnemyCount() == 2);
    assert(postWave->type == 44);
    assert(postWave->collisionRadius == 2792);
    assert(postWave->position.z == 0);
    assert(postWave->forwardVelocity == 0);
    assert((
        postWave->stateBytes4dTo52
        == std::array<std::uint8_t, 6>{255, 15, 2, 2, 5, 0}
    ));
    // Controlled original loop-tick capture 237 observes the newly deployed
    // Type 44 object's three model selectors at 1,1,1 after its first main
    // presenter call. The portable scalar mirrors selector zero.
    assert(postWave->modelAnimationVariant == 1);
    auto type44AnimationState = objective;
    const std::array<std::pair<std::uint8_t, std::uint8_t>, 4>
        type44AnimationSeries{{
            {1, 0},
            {2, 1},
            {3, 2},
            {3, 0},
        }};
    for (const auto [selector, countdown] : type44AnimationSeries) {
        type44AnimationState.advanceIdleTick();
        const auto* animatedType44 =
            type44AnimationState.postWaveObject();
        assert(animatedType44 != nullptr);
        assert(animatedType44->modelAnimationVariant == selector);
        assert(animatedType44->stateBytes4dTo52[3] == countdown);
    }
    const auto type44AudioCount = objective.audioEvents().size();
    auto type44ProjectileState = objective;
    type44ProjectileState.advanceIdleTick();
    const auto* type44AfterFire =
        type44ProjectileState.postWaveObject();
    assert(type44AfterFire != nullptr);
    assert(type44AfterFire->modelAnimationVariant == 1);
    assert(type44AfterFire->stateBytes4dTo52[3] == 0);
    assert(type44AfterFire->behaviorTimer == 20);
    const stellar7::Level1Projectile* type44Projectile = nullptr;
    for (const auto& candidate : type44ProjectileState.projectiles()) {
        if (candidate.active() && candidate.type == 22) {
            type44Projectile = &candidate;
            break;
        }
    }
    assert(type44Projectile != nullptr);
    assert(type44Projectile->poolSlot == 0);
    assert(type44Projectile->collisionRadius == 134);
    assert(type44Projectile->position.z == 2000);
    assert(
        type44Projectile->yaw
        == stellar7::fixedHeadingBetween(
            type44Projectile->position,
            type44ProjectileState.player().position
        )
    );
    assert(
        type44Projectile->pitch
        == stellar7::fixedHeadingBetween(
            {},
            {
                type44Projectile->position.z,
                static_cast<std::int32_t>(
                    type44AfterFire->presenterDistance
                ),
                0,
            }
        )
    );
    assert(type44Projectile->roll == 0);
    assert(type44Projectile->forwardVelocity == 500);
    assert((
        type44Projectile->stateBytes4dTo54
        == std::array<std::uint8_t, 8>{253, 80, 1, 0, 0, 0, 0, 0}
    ));
    assert(!type44Projectile->ownerIsPlayer);
    assert(
        type44Projectile->ownerAllocationIndex
        == type44AfterFire->allocationIndex
    );
    assert(
        type44ProjectileState.audioEvents().size()
        == type44AudioCount + 1
    );
    assert(
        type44ProjectileState.audioEvents().back().loopTick
        == type44ProjectileState.loopTick()
    );
    assert(type44ProjectileState.audioEvents().back().eventId == 6);

    auto projectileHit = type44ProjectileState;
    bool observedType44ProjectileDamage = false;
    for (
        std::uint32_t step = 0;
        step < 100 && projectileHit.missionActive();
        ++step
    ) {
        const auto armorBefore = projectileHit.playerArmor();
        projectileHit.advanceIdleTick();
        if (projectileHit.playerArmor() < armorBefore) {
            const auto damage = armorBefore - projectileHit.playerArmor();
            assert(damage == 104);
            observedType44ProjectileDamage = true;
            break;
        }
    }
    assert(observedType44ProjectileDamage);

    auto contactFailure = objective;
    bool observedType44ContactDamage = false;
    for (
        std::uint32_t step = 0;
        step < 1000 && contactFailure.missionActive();
        ++step
    ) {
        const auto* failureTarget = contactFailure.postWaveObject();
        assert(failureTarget != nullptr);
        const auto targetHeading = static_cast<std::uint16_t>(
            stellar7::fixedHeadingBetween(
                contactFailure.player().position,
                failureTarget->position
            ) + (
                contactFailure.player().forwardVelocity < 0
                    ? 0x8000U
                    : 0U
            )
        );
        const auto difference = static_cast<std::int16_t>(
            targetHeading - contactFailure.player().yaw
        );
        const auto desiredVelocity = std::clamp(
            static_cast<int>(difference) / 4,
            -0x200,
            0x200
        );
        const auto currentVelocity =
            contactFailure.player().yawVelocity;
        const auto armorBefore = contactFailure.playerArmor();
        contactFailure.advanceTick({
            currentVelocity < desiredVelocity - 0x28,
            currentVelocity > desiredVelocity + 0x28,
            contactFailure.player().forwardVelocity >= 0,
            contactFailure.player().forwardVelocity < 0,
            false,
        });
        if (contactFailure.playerArmor() < armorBefore) {
            const auto damage =
                armorBefore - contactFailure.playerArmor();
            assert(damage == 100 || damage == 104);
            observedType44ContactDamage =
                observedType44ContactDamage || damage == 100;
        }
    }
    assert(observedType44ContactDamage);
    assert(!contactFailure.missionActive());
    assert(contactFailure.failureState());
    assert(!contactFailure.successState());
    assert(!contactFailure.exitState());
    assert(contactFailure.playerArmor() < 0);

    stellar7::Level1State idleFailureBoundary;
    for (
        std::uint32_t step = 0;
        step < 2000 && idleFailureBoundary.playerArmor() >= 0;
        ++step
    ) {
        idleFailureBoundary.advanceIdleTick();
    }
    assert(idleFailureBoundary.playerArmor() < 0);
    assert(idleFailureBoundary.loopTick() == 1148);
    assert(idleFailureBoundary.missionActive());
    assert(!idleFailureBoundary.failureState());
    idleFailureBoundary.advanceIdleTick();
    assert(idleFailureBoundary.loopTick() == 1148);
    assert(!idleFailureBoundary.missionActive());
    assert(idleFailureBoundary.failureState());

    for (
        std::uint32_t step = 0;
        step < 1000 && objective.postWaveObject() != nullptr;
        ++step
    ) {
        postWave = objective.postWaveObject();
        const auto difference = static_cast<std::int16_t>(
            stellar7::fixedHeadingBetween(
                objective.player().position,
                postWave->position
            ) - objective.player().yaw
        );
        const auto desiredVelocity = std::clamp(
            static_cast<int>(difference) / 4,
            -0x200,
            0x200
        );
        const auto currentVelocity = objective.player().yawVelocity;
        const bool moveForward = postWave->presenterDistance > 26000U;
        const bool moveReverse = postWave->presenterDistance < 22000U;
        // FUN_1000_9e10 preserves throttle while any directional control is
        // active. Release steering while the synthetic pilot intends neutral
        // throttle damping inside its distance band.
        const bool dampThrottle = (
            !moveForward
            && !moveReverse
            && objective.player().forwardVelocity != 0
        );
        objective.advanceTick({
            !dampThrottle && currentVelocity < desiredVelocity - 0x28,
            !dampThrottle && currentVelocity > desiredVelocity + 0x28,
            moveForward,
            moveReverse,
            true,
        });
        assert(!objective.failureState());
    }
    assert(objective.postWaveObject() == nullptr);
    const stellar7::Level1Entity* pickup = nullptr;
    for (const auto& candidate : objective.entities()) {
        if (candidate.active() && candidate.type == 25) {
            pickup = &candidate;
            break;
        }
    }
    assert(pickup != nullptr);
    // Natural-success-v5 pristine ticks 4017 -> 4018 exercise the Type 44
    // special death branch.  It awards 1000 points and FUN_1000_4506 uses
    // the first free dynamic-pool record (stable entity slot 2).
    assert(objective.score() == 1260);
    assert(pickup->allocationIndex == 2);
    assert(pickup->type == 25);
    assert(pickup->collisionRadius == 897);
    assert((
        pickup->position
        == stellar7::FixedPosition{14995, -7097, 0}
    ));
    assert(pickup->pitch == 0);
    assert(pickup->roll == 0);
    assert(pickup->yaw == 44576);
    assert(pickup->forwardVelocity == 0);
    assert((
        pickup->stateBytes4dTo52
        == std::array<std::uint8_t, 6>{254, 100, 3, 0, 0, 1}
    ));
    assert(pickup->presenterDistance == 14262);
    assert(pickup->targetHeading == 11184);
    const auto pickupTurnMagnitude = static_cast<std::int16_t>(
        (
            static_cast<std::int16_t>(
                (objective.rng().last() >> 8U) & 0x03U
            ) + 10
        ) << 8
    );
    assert(
        pickup->yawVelocity
        == (
            (objective.rng().last() & 0x0100U) != 0
                ? -pickupTurnMagnitude
                : pickupTurnMagnitude
        )
    );
    for (
        std::uint32_t step = 0;
        step < 1000 && !objective.successState();
        ++step
    ) {
        pickup = nullptr;
        for (const auto& candidate : objective.entities()) {
            if (candidate.active() && candidate.type == 25) {
                pickup = &candidate;
                break;
            }
        }
        assert(pickup != nullptr);
        const auto pickupHeading = static_cast<std::uint16_t>(
            stellar7::fixedHeadingBetween(
                objective.player().position,
                pickup->position
            ) + (
                objective.player().forwardVelocity < 0
                    ? 0x8000U
                    : 0U
            )
        );
        const auto difference = static_cast<std::int16_t>(
            pickupHeading - objective.player().yaw
        );
        const auto desiredVelocity = std::clamp(
            static_cast<int>(difference) / 4,
            -0x200,
            0x200
        );
        const auto currentVelocity = objective.player().yawVelocity;
        objective.advanceTick({
            currentVelocity < desiredVelocity - 0x28,
            currentVelocity > desiredVelocity + 0x28,
            objective.player().forwardVelocity >= 0,
            objective.player().forwardVelocity < 0,
            false,
        });
        assert(!objective.failureState());
    }
    assert(objective.successState());
    assert(!objective.missionActive());
    assert(!objective.failureState());

    stellar7::Level1State explicitExit;
    explicitExit.advanceTick({
        false,
        false,
        false,
        false,
        false,
        true,
    });
    assert(explicitExit.missionActive());
    assert(!explicitExit.failureState());
    assert(!explicitExit.successState());
    assert(!explicitExit.exitState());
    explicitExit.advanceIdleTick();
    assert(!explicitExit.missionActive());
    assert(!explicitExit.failureState());
    assert(!explicitExit.successState());
    assert(explicitExit.exitState());
    const auto exitTick = explicitExit.loopTick();
    const auto exitRngCalls = explicitExit.rng().calls();
    explicitExit.advanceIdleTick();
    assert(explicitExit.loopTick() == exitTick);
    assert(explicitExit.rng().calls() == exitRngCalls);

    stellar7::Level1Entity pitched{
        0, 0, 0, 0, {0, 0, 0},
        0x4000, 0, 0,
        0, 0, 0,
        0, 0, 0,
        100,
        {},
    };
    stellar7::advanceEntityKinematics(pitched);
    assert((pitched.position == stellar7::FixedPosition{0, 0, 100}));

    const stellar7::Level1State traceState;
    const std::string trace = stellar7::level1StateTraceJson(
        traceState,
        "level1-entry-pre-integration"
    );
    assert(trace == stellar7::level1StateTraceJson(
        traceState,
        "level1-entry-pre-integration"
    ));
    assert(trace.find("\"format_version\":1") != std::string::npos);
    assert(trace.find(
        "\"boundary\":\"level1-entry-pre-integration\""
    ) != std::string::npos);
    assert(trace.find("\"loop_tick\":1") != std::string::npos);
    assert(trace.find("\"level_index\":0") != std::string::npos);
    assert(trace.find(
        "\"position\":[-2048,2048,0]"
    ) != std::string::npos);
    assert(trace.find(
        "\"rng\":{\"index_a\":102,\"index_b\":52,"
        "\"last\":38480,\"calls\":29}"
    ) != std::string::npos);
}
