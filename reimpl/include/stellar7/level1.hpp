#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace stellar7 {

struct FixedPosition {
    std::int32_t x;
    std::int32_t y;
    std::int32_t z;
};

constexpr bool operator==(
    const FixedPosition& left,
    const FixedPosition& right
) noexcept {
    return left.x == right.x
        && left.y == right.y
        && left.z == right.z;
}

struct FixedMatrixQ14 {
    std::array<std::int16_t, 9> values;
    std::uint16_t mode;
};

FixedMatrixQ14 makeFixedMatrixQ14(
    std::uint16_t pitch,
    std::uint16_t roll,
    std::uint16_t yaw
) noexcept;
FixedMatrixQ14 composeFixedMatricesQ14(
    const FixedMatrixQ14& first,
    const FixedMatrixQ14& second,
    std::uint16_t lowWordSeed
) noexcept;
void transpose(FixedMatrixQ14& matrix) noexcept;

struct Viewport {
    std::int16_t left;
    std::int16_t right;
    std::int16_t top;
    std::int16_t bottom;
};

constexpr bool operator==(
    const Viewport& left,
    const Viewport& right
) noexcept {
    return left.left == right.left
        && left.right == right.right
        && left.top == right.top
        && left.bottom == right.bottom;
}

struct ViewTransform {
    FixedPosition origin;
    FixedMatrixQ14 matrix;
    Viewport viewport;
    std::int32_t perspectiveScale;
};

struct ProjectedPoint {
    std::int16_t x;
    std::int16_t y;
};

constexpr bool operator==(
    const ProjectedPoint& left,
    const ProjectedPoint& right
) noexcept {
    return left.x == right.x && left.y == right.y;
}

ViewTransform initialLevel1MainView() noexcept;
ViewTransform initialLevel1RadarView() noexcept;
FixedPosition transformToView(
    FixedPosition worldPosition,
    const ViewTransform& view
) noexcept;
ProjectedPoint projectToViewport(
    FixedPosition viewPosition,
    const ViewTransform& view
) noexcept;

enum class WorldActorRenderProfile {
    unknown,
    noHandles,
    inlineSelectorNoCallback,
    bitmapEffectCallback,
    inlineSelectorBeamCallback,
    heapSelectorDeferredAnimation,
    inlineSelectorDeparture,
    heapSelectorNoCallback,
};

struct CompletionBeamPresentation {
    FixedPosition center{};
    bool projected = false;
    ProjectedPoint anchor{};
    std::uint16_t radius = 0;
    std::uint8_t timer = 0;
    std::array<std::uint8_t, 10> colors{};
};

inline constexpr std::size_t retainedAnimationSelectorCapacity=3;
struct Level1Entity {
    std::size_t allocationIndex;
    std::uint8_t flags;
    std::int16_t type;
    std::uint32_t collisionRadius;
    FixedPosition position;
    std::uint16_t pitch;
    std::uint16_t roll;
    std::uint16_t yaw;
    std::int16_t pitchVelocity;
    std::int16_t rollVelocity;
    std::int16_t yawVelocity;
    std::int16_t xVelocity;
    std::int16_t yVelocity;
    std::int16_t zVelocity;
    std::int16_t forwardVelocity;
    std::array<std::uint8_t, 6> stateBytes4dTo52;
    std::uint8_t modelAnimationVariant = 0;
    bool primaryType11AnimationSelector = false;
    std::uint8_t behaviorTimer = 0;
    // Target-local probe state for FUN_1000:952c's stack-local miss gate at
    // 1000:9616.  The neutral value avoids inventing stack lifetime between
    // callbacks; controlled regressions may set it when that lifetime is
    // explicitly part of the fixture.
    std::uint8_t type12MissGate = 2;
    std::uint16_t type6CloakTimer = 0;
    std::uint32_t presenterDistance = 0;
    std::uint16_t targetHeading = 0;
    std::uint8_t weaponSlotCount = 0;
    std::uint8_t weaponCooldown = 0;
    std::array<std::uint8_t, 3> weaponSlotTimers{};
    std::size_t collisionContactAllocationIndex =
        static_cast<std::size_t>(-1);
    bool collisionContactIsPlayer = false;
    std::uint8_t presentationModelAnimationVariant = 0;
    bool animatedModelPresenterEnabled = true;
    bool ownerIsPlayer = false;
    std::size_t ownerAllocationIndex = static_cast<std::size_t>(-1);
    WorldActorRenderProfile renderProfile = WorldActorRenderProfile::unknown;
    // Heap indices 1/2 are independent of the existing index-0 selector.
    std::array<std::uint8_t,retainedAnimationSelectorCapacity-1> modelAnimationTail{};
    std::array<std::uint8_t,retainedAnimationSelectorCapacity-1> presentationModelAnimationTail{};

    bool active() const noexcept {
        return stateBytes4dTo52[1] != 0;
    }
};

struct Level1Projectile {
    std::size_t poolSlot = 0;
    std::uint8_t flags = 0;
    std::int16_t type = 0;
    std::uint32_t collisionRadius = 0;
    FixedPosition position{};
    std::uint16_t pitch = 0;
    std::uint16_t roll = 0;
    std::uint16_t yaw = 0;
    std::int16_t pitchVelocity = 0;
    std::int16_t rollVelocity = 0;
    std::int16_t yawVelocity = 0;
    std::int16_t xVelocity = 0;
    std::int16_t yVelocity = 0;
    std::int16_t zVelocity = 0;
    std::int16_t forwardVelocity = 0;
    std::array<std::uint8_t, 8> stateBytes4dTo54{};
    bool ownerIsPlayer = false;
    std::size_t ownerAllocationIndex = static_cast<std::size_t>(-1);
    std::uint32_t presenterDistance = 0;
    std::uint16_t targetHeading = 0;

    bool active() const noexcept {
        return stateBytes4dTo54[1] != 0;
    }
};

struct Level1DeathEffect {
    Level1Entity entity{};
    bool listed = false;
    ProjectedPoint projected{};
};

struct Level1Input {
    bool left = false;
    bool right = false;
    bool forward = false;
    bool reverse = false;
    bool fire = false;
    bool exit = false;
};

struct Level1AudioEvent {
    std::uint16_t loopTick = 0;
    std::uint16_t eventId = 0;
};

void advanceEntityKinematics(Level1Entity& entity) noexcept;
void reverseEntityTranslation(Level1Entity& entity) noexcept;
std::uint32_t approximatePlanarDistance(
    FixedPosition source,
    FixedPosition target
) noexcept;
std::uint16_t fixedHeadingBetween(
    FixedPosition source,
    FixedPosition target
) noexcept;
std::uint16_t presenterTargetHeading(
    FixedPosition source,
    FixedPosition target
) noexcept;
void normalizeWorldPosition(FixedPosition& position) noexcept;

class StellarRng {
public:
    static constexpr std::size_t wordCount = 56;

    StellarRng() noexcept;
    StellarRng(
        const std::array<std::uint16_t, wordCount>& table,
        std::uint16_t indexA,
        std::uint16_t indexB,
        std::uint16_t last,
        std::uint32_t calls
    ) noexcept;

    std::uint16_t next() noexcept;
    std::uint16_t indexA() const noexcept;
    std::uint16_t indexB() const noexcept;
    std::uint16_t last() const noexcept;
    std::uint32_t calls() const noexcept;

private:
    std::array<std::uint16_t, wordCount> table_;
    std::uint16_t indexA_ = 0x30;
    std::uint16_t indexB_ = 0x6E;
    std::uint16_t last_ = 0;
    std::uint32_t calls_ = 0;
};

struct GroundPoint {
    std::int16_t x;
    std::int16_t y;
};

constexpr bool operator==(
    const GroundPoint& left,
    const GroundPoint& right
) noexcept {
    return left.x == right.x && left.y == right.y;
}

// Source4c66 movement before viewport classification and reset; word subtraction wraps.
GroundPoint advanceGroundPoint(GroundPoint point, std::int16_t turn,
    std::int16_t speed) noexcept;

// Source952c; caller owns projectile allocation and the actor record lifetime.
std::uint8_t advanceType11Behavior(Level1Entity& actor, const Level1Entity& player,
    StellarRng& rng, const std::function<bool(Level1Entity&)>& fire);

class Level1GroundState {
public:
    static constexpr std::size_t pointCount = 6;

    Level1GroundState() noexcept;
    explicit Level1GroundState(StellarRng& rng) noexcept;
    explicit Level1GroundState(
        const std::array<GroundPoint, pointCount>& points
    ) noexcept;

    void advance(
        std::int16_t turn,
        std::int16_t speed,
        StellarRng& rng
    ) noexcept;
    const std::array<GroundPoint, pointCount>& points() const noexcept;
    bool pointVisible(std::size_t index) const noexcept;

private:
    std::array<GroundPoint, pointCount> points_;
    std::array<bool, pointCount> visible_{};
};

enum class Level1PresentationKind { entity, projectile, deathEffect };

struct Level1RenderState {
    Level1GroundState& ground;
    StellarRng& rng;
    const Level1Entity& player;
    const std::vector<Level1Entity>& entities;
    const std::vector<std::size_t>& order;
    std::uint16_t loopTick;
};

class Level1FrameRenderer {
public:
    virtual ~Level1FrameRenderer() = default;
    // Owns ground/RNG advance before the existing main-view animation callback.
    virtual void render(Level1RenderState state) = 0;
};

struct Level1PresentationObject {
    Level1PresentationKind kind;
    std::size_t slot;
    std::int16_t type;
    std::uint8_t flags;
    std::uint32_t radius;
    FixedPosition position;
    std::int8_t category;
    WorldActorRenderProfile renderProfile;
};

class Level1State {
    friend struct Level1ReloadTestAccess;
    friend class GameplayModel;
public:
    static constexpr std::size_t entityCount = 6;
    static constexpr std::size_t projectilePoolSize = 8;
    static constexpr std::size_t deathEffectPoolSize = 8;
    static constexpr std::size_t playerProjectileSlots = 2;
    static constexpr std::size_t systemCount = 7;

    Level1State() noexcept;

    // Difficulty0 same-level retry, before the first ordinary presentation.
    void reloadAfterFailure(const StellarRng& retainedFailureRng);
    void advanceEntityKinematics() noexcept;
    void advanceTick(const Level1Input& input) noexcept;
    void advanceTickWithRenderer(const Level1Input& input, Level1FrameRenderer& renderer);
    void advanceIdleTick() noexcept;
    bool firePrimary() noexcept;
    std::int16_t levelIndex() const noexcept;
    std::uint16_t loopTick() const noexcept;
    std::uint8_t type11AnimationVariant() const noexcept;
    const Level1Entity& player() const noexcept;
    const std::vector<Level1Entity>& entities() const noexcept;
    const std::vector<std::size_t>& activeListOrder() const noexcept;
    const std::array<Level1Projectile, projectilePoolSize>&
    projectiles() const noexcept;
    const std::array<Level1DeathEffect, deathEffectPoolSize>&
    deathEffects() const noexcept;
    const std::vector<std::size_t>&
    semanticActiveListOrder() const noexcept;
    std::vector<std::size_t> projectileOrder() const;
    std::vector<Level1PresentationObject> radarPresentationObjects() const;
    std::uint8_t playerWeaponCooldown() const noexcept;
    const std::array<std::uint8_t, playerProjectileSlots>&
    playerProjectileTimers() const noexcept;
    const std::vector<Level1AudioEvent>& audioEvents() const noexcept;
    const StellarRng& rng() const noexcept;
    const Level1GroundState& ground() const noexcept;
    std::uint32_t score() const noexcept;
    std::int16_t lastDestroyedType() const noexcept;
    std::uint16_t consecutiveDestroyedTypeCount() const noexcept;
    std::int16_t playerArmor() const noexcept;
    const std::array<std::int8_t, systemCount>&
    systemStatusLevels() const noexcept;
    const std::array<std::int16_t, systemCount>&
    systemTimers() const noexcept;
    std::int16_t selectedSystem() const noexcept;
    bool missionActive() const noexcept;
    bool failureState() const noexcept;
    bool successState() const noexcept;
    bool exitState() const noexcept;
    std::int16_t activeEnemyCount() const noexcept;
    std::int16_t spawnedEnemyCount() const noexcept;
    std::int16_t maximumEnemyCount() const noexcept;
    const Level1Entity* lastSpawnCheckpoint() const noexcept;
    std::uint16_t lastSpawnTick() const noexcept;
    const StellarRng& lastSpawnRng() const noexcept;
    const Level1Entity* completionObject() const noexcept;
    const Level1Entity* postWaveObject() const noexcept;
    std::size_t postWaveObjectAllocationIndex() const noexcept;

private:
    void resetCampaignTransientState() noexcept;
    void finishFrame();
    bool pendingFailureTail() const noexcept;
    void finishFailureTail();
    bool pendingSuccessContact() const noexcept;
    void finishSuccessContact(const Level1Input& input);
    bool admitsSuccessContact(std::size_t pickupIndex,
        const std::vector<std::size_t>& updateOrder) const noexcept;
    void advancePickupOscillator(Level1Entity& item) noexcept;
    void advanceAnimatedEntityPresenters() noexcept;
    void applyPostBoundaryInput(const std::array<bool, projectilePoolSize>& projectileActiveAtStart) noexcept;
    void completePostBoundaryTail() noexcept;
    void advancePostBoundaryTail(const std::array<bool, projectilePoolSize>& projectileActiveAtStart) noexcept;

    enum class ObjectKind : std::uint8_t {
        entity,
        projectile,
        deathEffect,
    };
    struct ObjectReference {
        ObjectKind kind;
        std::size_t index;
    };

    void advanceTickImpl(const Level1Input& input, Level1FrameRenderer* renderer);
    bool fireEnemy(std::size_t entityIndex) noexcept;
    bool fireType44(std::size_t entityIndex) noexcept;
    void runPeriodicSpawnCheck() noexcept;
    void runCompletionCheck() noexcept;
    Level1Entity& allocateEntity(
        Level1Entity value,
        std::size_t firstReusableIndex = 0,
        bool preserveDynamicScratch = false
    ) noexcept;
    bool registerDestroyedTypeBonus(
        std::int16_t destroyedType,
        const Level1Entity& source
    ) noexcept;
    std::uint8_t beginDestroyedTypeBonus(std::int16_t destroyedType) noexcept;
    void repairBonusSystem(std::uint8_t payload) noexcept;
    bool spawnTransientEffect(
        const Level1Entity& source,
        std::uint8_t firstFrame,
        std::uint8_t lastFrame
    ) noexcept;
    bool spawnDeathEffect(const Level1Entity& source) noexcept;
    void refreshDeathEffectAllocationIndices() noexcept;
    std::uint32_t objectPresenterDistance(
        const ObjectReference& reference
    ) const noexcept;
    void appendAllocationObject(
        ObjectKind kind,
        std::size_t index
    ) noexcept;
    void removeAllocationObject(
        ObjectKind kind,
        std::size_t index
    ) noexcept;

    std::int16_t levelIndex_ = 0;
    std::uint16_t loopTick_ = 1;
    std::uint8_t type11AnimationVariant_ = 1;
    Level1Entity player_;
    std::vector<Level1Entity> entities_;
    std::vector<ObjectReference> allocationList_;
    std::vector<ObjectReference> objectListOrder_;
    std::vector<std::size_t> activeListOrder_;
    std::vector<std::size_t> semanticActiveListOrder_;
    FixedPosition projectionListOrigin_{};
    std::array<Level1Projectile, projectilePoolSize> projectiles_{};
    std::array<Level1DeathEffect, deathEffectPoolSize> deathEffects_{};
    std::uint8_t playerWeaponCooldown_ = 0;
    std::array<std::uint8_t, playerProjectileSlots>
        playerProjectileTimers_{};
    std::vector<Level1AudioEvent> audioEvents_;
    std::uint32_t score_ = 0;
    std::int16_t playerArmor_ = 680;
    std::array<std::int8_t, systemCount> systemStatusLevels_{
        {3, 3, 3, 3, 3, 3, 3}
    };
    std::array<std::int16_t, systemCount> systemTimers_{};
    std::int16_t selectedSystem_ = -1;
    bool missionActive_ = true;
    bool failureState_ = false;
    bool failurePending_ = false;
    bool failureTailConsumed_ = false;
    std::array<bool, projectilePoolSize> failureTailProjectileActiveAtStart_{};
    bool successState_ = false;
    bool retainSuccessContact_ = false;
    bool successContactPending_ = false;
    std::size_t successContactIndex_ = static_cast<std::size_t>(-1);
    std::vector<std::size_t> successContactOrder_;
    std::array<bool, projectilePoolSize> successContactProjectileActiveAtStart_{};
    bool exitState_ = false;
    std::int16_t activeEnemyCount_ = 2;
    std::int16_t spawnedEnemyCount_ = 2;
    static constexpr std::int16_t maximumEnemyCount_ = 10;
    Level1Entity lastSpawnCheckpoint_{};
    std::uint16_t lastSpawnTick_ = 0;
    StellarRng lastSpawnRng_;
    bool hasLastSpawnCheckpoint_ = false;
    std::size_t completionObjectAllocationIndex_ =
        static_cast<std::size_t>(-1);
    std::size_t postWaveObjectAllocationIndex_ =
        static_cast<std::size_t>(-1);
    std::int16_t lastDestroyedType_ = 0;
    std::uint16_t consecutiveDestroyedTypeCount_ = 0;
    StellarRng rng_;
    Level1GroundState ground_;
    bool hasAdvanced_ = false;
    bool nativeInitialListPending_ = false;
    bool ordinaryTailPending_ = false;
    bool ordinaryTailConsumed_ = false;
    Level1Input pendingPostBoundaryInput_{};
    std::int16_t playerCollisionMomentum_ = 0;
};

}  // namespace stellar7
