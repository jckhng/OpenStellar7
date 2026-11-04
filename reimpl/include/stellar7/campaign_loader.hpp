#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <functional>
#include <array>
#include <optional>

namespace stellar7 {

struct Model3d;
struct Level1Entity;
class StellarRng;

enum class CampaignDifficulty : std::uint8_t { easy=0, standard=1, hard=2 };

std::uint8_t campaignDifficultyIndex(CampaignDifficulty difficulty);
std::array<std::int8_t,7> campaignInitialCharges(CampaignDifficulty difficulty);
unsigned campaignContinueBudget(CampaignDifficulty difficulty);
std::uint16_t campaignScoreAward(std::uint16_t award, CampaignDifficulty difficulty);
std::uint8_t campaignGuardianInitialHealth(std::uint8_t level, CampaignDifficulty difficulty);

struct CampaignInitialActor {
    std::uint8_t type = 0;
    std::int16_t x = 0;
    std::int16_t y = 0;
    std::uint16_t yaw = 0;
};

struct CampaignLevelTable {
    std::uint8_t level = 0;
    std::uint8_t enemyLimit = 0;
    std::uint8_t sky = 0;
    std::uint16_t schedulerPointer = 0;
    std::uint16_t weightsPointer = 0;
    std::vector<CampaignInitialActor> actors;
};

CampaignLevelTable decodeCampaignLevel(
    const std::vector<std::uint8_t>& originalDs,
    std::uint8_t level,
    std::uint8_t difficulty
);

struct CampaignLevelDefinition {
    static constexpr std::size_t schedulerSize = 10;
    static constexpr std::size_t weightsSize = 14;
    CampaignLevelTable table;
    std::array<std::uint8_t, schedulerSize> scheduler;
    std::array<std::uint8_t, weightsSize> weights;
};

// Recovered immutable semantics; level is zero-based. No runtime DS pointers.
CampaignLevelDefinition campaignLevelDefinition(std::uint8_t level, std::uint8_t difficulty);
bool campaignHorizonOverlayEnabled(std::uint8_t level);

enum class CampaignExit : std::uint8_t { greenReward = 25, blueWarp = 26 };

//07ED..0812 plus parent0475. Destination >=7 enters ending; retain overshoot.
//Warp availability and resource teardown remain caller-owned.
std::uint8_t campaignExitDestination(std::uint8_t level, CampaignExit exit);

struct CampaignOrdinaryDefaults {
    std::uint8_t health;
    std::uint8_t behavior;
    std::uint8_t weaponSlots;
    std::optional<std::int16_t> speed;
};

CampaignOrdinaryDefaults campaignOrdinaryDefaults(std::uint8_t type);

// Compatibility source window0..15; includes bytes following the logical10-byte row.
std::uint8_t campaignSpawnThreshold(std::uint8_t level, std::uint8_t index);

// Caller owns index clamping and RNG consumption; no allocation or actor RNG draws.
std::optional<std::uint8_t> campaignSpawnType(const CampaignLevelDefinition& definition,
    std::uint8_t thresholdIndex, std::int16_t spawnedCount, std::uint16_t randomWord);

enum class CampaignActorField {
    auxiliaryWord1a, classification, health, animation, behavior, height
};

// Allocators own pool selection, model binding, initialization and RNG calls.
class CampaignActorLoader {
public:
    using Actor = std::size_t;
    virtual ~CampaignActorLoader() = default;
    virtual Actor allocateOrdinary(std::uint8_t type) = 0;
    virtual Actor allocateFixed(std::uint8_t type) = 0;
    virtual void write(Actor actor, CampaignActorField field, std::int16_t value) = 0;
    virtual void setPosition(Actor actor, std::int16_t x, std::int16_t y) = 0;
    virtual void setYaw(Actor actor, std::uint16_t yaw) = 0;
    virtual void presentFixed(Actor actor) = 0;
    virtual void aliasAnimation(Actor actor) = 0;
};

void initializeCampaignActors(
    const CampaignLevelTable& table,
    CampaignActorLoader& loader
);

class CampaignOrdinaryConstructor {
public:
    CampaignOrdinaryConstructor(std::uint8_t level, const std::vector<std::uint8_t>& types,
        const std::vector<Model3d>& shared, const std::vector<Model3d>& local);
    Level1Entity construct(std::uint8_t resolvedType, StellarRng& rng) const;

private:
    struct ModelMetadata {
        std::uint8_t type;
        std::uint32_t radius;
    };
    std::uint8_t level_;
    std::vector<ModelMetadata> models_;
};

// Reset-owned ordinary records; table placement still consumes constructor RNG.
// Fixed/special table entries and multi-selector heap models reject before RNG.
std::vector<Level1Entity> constructCampaignOrdinaryActors(const CampaignLevelTable& table,
    StellarRng& rng, const std::vector<Model3d>& shared, const std::vector<Model3d>& local);

void validateLevel3LoaderActors(const std::vector<Model3d>& shared,
    const std::vector<Model3d>& local);
std::vector<Level1Entity> constructLevel3LoaderActors(StellarRng& rng,
    const std::vector<Model3d>& shared, const std::vector<Model3d>& local);

// Fresh campaign pools, including fixed29+level and the24/31 companion pair.
// Validation completes before constructor RNG is consumed.
void validateCampaignLoaderActors(const CampaignLevelTable& table,
    const std::vector<Model3d>& shared, const std::vector<Model3d>& local);
std::vector<Level1Entity> constructCampaignLoaderActors(const CampaignLevelTable& table,
    StellarRng& rng, const std::vector<Model3d>& shared, const std::vector<Model3d>& local);

// External effects unavailable from DS alone remain explicit caller contracts.
class CampaignAllocationServices {
public:
    virtual ~CampaignAllocationServices() = default;
    virtual std::uint16_t nextRandom() = 0;
    virtual std::uint32_t modelRadius(std::uint8_t type) = 0;
    virtual void releaseAnimation(std::uint16_t pointer) = 0;
    virtual void presentFixed(std::uint16_t actor) = 0;
};

// Operates on one canonical DS image, preserving actor near-pointer aliases.
class CampaignMemoryLoader final : public CampaignActorLoader {
public:
    CampaignMemoryLoader(std::vector<std::uint8_t>& ds, CampaignAllocationServices& services);
    void resetLevel();
    void constructPools(const std::function<std::uint16_t(std::size_t)>& allocateRecord);
    Actor allocateOrdinary(std::uint8_t type) override;
    Actor allocateFixed(std::uint8_t type) override;
    void write(Actor actor, CampaignActorField field, std::int16_t value) override;
    void setPosition(Actor actor, std::int16_t x, std::int16_t y) override;
    void setYaw(Actor actor, std::uint16_t yaw) override;
    void presentFixed(Actor actor) override;
    void aliasAnimation(Actor actor) override;

private:
    std::vector<std::uint8_t>& ds_;
    CampaignAllocationServices& services_;
    void putByte(std::size_t offset, std::uint8_t value);
    void putWord(std::size_t offset, std::uint16_t value);
    void putDword(std::size_t offset, std::uint32_t value);
    Actor freeActor(std::size_t pool, std::size_t end) const;
    void appendAllocation(Actor actor);
    void clearActor(Actor actor);
};

std::int32_t campaignCoordinate(std::int16_t value) noexcept;

void validateCampaignOrdinaryActors(const CampaignLevelTable& table,
    const std::vector<Model3d>& shared, const std::vector<Model3d>& local);

std::uint32_t campaignModelRadius(
    const std::vector<Model3d>& shared,
    const std::vector<Model3d>& local,
    std::uint16_t type
) noexcept;

Level1Entity decodeCampaignActor(
    const std::vector<std::uint8_t>& ds,
    std::uint16_t actor,
    std::size_t allocationIndex
);

}
