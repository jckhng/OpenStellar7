#include "stellar7/campaign_loader.hpp"
#include "stellar7/resource_codec.hpp"
#include "stellar7/level1.hpp"

#include <stdexcept>
#include <algorithm>

namespace stellar7 {
namespace {
constexpr std::size_t levelCount = 7;
constexpr std::size_t difficultyCount = 3;
constexpr std::size_t countTable = 0x962;
constexpr std::size_t typePointers = 0x96a;
constexpr std::size_t schedulerPointers = 0x978;
constexpr std::size_t weightsPointers = 0x986;
constexpr std::size_t difficultyTable = 0x994;
constexpr std::size_t skyTable = 0x9aa;
//6e6c selects the overlay byte from immutable DS13a2+level.
constexpr std::array<bool, levelCount> horizonOverlays{true, true, false, true, false, false, false};
constexpr std::size_t xPointers = 0x9b2;
constexpr std::size_t yPointers = 0x9c0;
constexpr std::size_t yawPointers = 0x9ce;
constexpr std::uint8_t levelFixedType = 29;
constexpr std::uint8_t animatedFixedType = 24;
constexpr std::uint8_t companionType = 31;
constexpr std::size_t actorBytes = 99;
constexpr std::size_t healthOffset = 0x4e;
constexpr std::size_t animationPointerOffset = 0x1c;
constexpr std::size_t animationOffset = 0x4f;
constexpr std::size_t supportedAllocationLimit = 32;
constexpr std::uint8_t level3Index = 2;
constexpr std::uint8_t level3YawActorType = 10;
constexpr std::uint8_t level3GroundActorType = 2;
constexpr std::uint8_t fixedActorInitialHealth = 100;
constexpr std::uint8_t type6AnimationLimit = 2;

// Semantic translation of pinned unpacked DS campaign tables0x768..0x9dc.
// Provenance: campaign_loader_tables.py, image SHA256 d7ed113243f5fe91adfdf66a89c6d4824a44a03de8141ad5e9a3926685a99851.
const std::array<CampaignLevelDefinition, levelCount> campaignDefinitions{{
    {{0, 0, 5, 0, 0, {
        {1, -1, 1, 0xb000},
        {11, 4, 4, 0x4800},
        {29, 8, 8, 0x0000},
        {29, 8, -8, 0x0000},
        {29, -8, 8, 0x0000},
        {29, -8, -8, 0x0000}
    }},
     {130, 60, 10, 0, 0, 0, 0, 0, 0, 0},
     {100, 0, 170, 0, 0, 0, 0, 0, 0, 0, 255, 0, 0, 0}},
    {{1, 0, 9, 0, 0, {
        {8, 6, 12, 0x0000},
        {8, -10, 2, 0x4000},
        {8, 12, -4, 0xc000},
        {13, -12, 14, 0x0300},
        {13, 6, -12, 0x0700},
        {29, -4, 10, 0x0000},
        {29, 4, -10, 0xab00}
    }},
     {140, 80, 20, 0, 0, 0, 0, 0, 0, 0},
     {65, 0, 0, 165, 0, 0, 0, 205, 0, 0, 0, 0, 255, 0}},
    {{2, 0, 11, 0, 0, {
        {10, -8, -4, 0x0000},
        {10, -14, -2, 0x2000},
        {10, -10, -10, 0x0000},
        {2, 8, 12, 0x4000},
        {24, -10, -6, 0x0f00}
    }},
     {150, 90, 30, 0, 0, 0, 0, 0, 0, 0},
     {0, 102, 153, 0, 0, 0, 0, 0, 0, 204, 0, 255, 0, 0}},
    {{3, 0, 13, 0, 0, {
        {9, -6, 14, 0xa000},
        {5, 12, -12, 0x7000},
        {14, -6, 2, 0x2000},
        {14, 0, -4, 0x2000},
        {14, 10, 8, 0x2000},
        {29, -10, 10, 0x2000},
        {29, -10, -10, 0x2000},
        {29, 8, -8, 0x2000}
    }},
     {160, 100, 50, 10, 0, 0, 0, 0, 0, 0},
     {0, 0, 0, 110, 185, 0, 0, 0, 255, 0, 0, 0, 0, 0}},
    {{4, 0, 15, 0, 0, {
        {11, -8, 12, 0x8000},
        {11, 8, 12, 0x8000},
        {11, 0, 14, 0x8000},
        {13, -8, -6, 0x0000},
        {13, 8, -6, 0x0000},
        {13, 0, -10, 0x0000}
    }},
     {170, 120, 70, 30, 0, 0, 0, 0, 0, 0},
     {0, 0, 0, 0, 80, 0, 0, 140, 0, 0, 205, 0, 255, 0}},
    {{5, 0, 17, 0, 0, {
        {9, -8, 8, 0xa000},
        {13, -14, 2, 0xe000},
        {13, 6, 10, 0x0700},
        {14, -2, 12, 0x0300},
        {14, -4, 4, 0x0c00},
        {6, 2, -4, 0x2400},
        {24, -10, -6, 0x3000}
    }},
     {180, 140, 90, 50, 10, 0, 0, 0, 0, 0},
     {0, 0, 0, 0, 0, 120, 0, 0, 185, 0, 0, 0, 255, 0}},
    {{6, 0, 21, 0, 0, {
        {10, -12, 6, 0x0000},
        {12, 12, -6, 0x0000},
        {6, -10, -14, 0xe000},
        {6, -4, 12, 0x7400},
        {14, 4, 12, 0x8c00},
        {14, -8, 8, 0x0b00},
        {29, 8, -8, 0x0b00}
    }},
     {190, 150, 110, 70, 30, 0, 0, 0, 0, 0},
     {0, 0, 0, 40, 0, 120, 0, 0, 0, 160, 0, 200, 255, 0}}
}};

constexpr std::array<std::array<std::uint8_t, difficultyCount>, levelCount> enemyLimits{{
    {10, 13, 15},
    {12, 15, 18},
    {12, 15, 18},
    {14, 18, 21},
    {16, 20, 24},
    {18, 23, 27},
    {20, 25, 30}
}};

// Source-indexed compatibility windows include six bytes after each logical row.
constexpr std::array<std::array<std::uint8_t, 16>, levelCount> spawnThresholds{{
    {130, 60, 10, 0, 0, 0, 0, 0, 0, 0, 140, 80, 20, 0, 0, 0},
    {140, 80, 20, 0, 0, 0, 0, 0, 0, 0, 150, 90, 30, 0, 0, 0},
    {150, 90, 30, 0, 0, 0, 0, 0, 0, 0, 160, 100, 50, 10, 0, 0},
    {160, 100, 50, 10, 0, 0, 0, 0, 0, 0, 170, 120, 70, 30, 0, 0},
    {170, 120, 70, 30, 0, 0, 0, 0, 0, 0, 180, 140, 90, 50, 10, 0},
    {180, 140, 90, 50, 10, 0, 0, 0, 0, 0, 190, 150, 110, 70, 30, 0},
    {190, 150, 110, 70, 30, 0, 0, 0, 0, 0, 100, 0, 170, 0, 0, 0}
}};

constexpr std::array<CampaignOrdinaryDefaults, 17> ordinaryDefaults{{
    {0, 0, 2, 202},
    {1, 1, 2, 225},
    {2, 1, 1, 144},
    {1, 1, 1, 216},
    {2, 1, 1, 180},
    {3, 1, 3, 135},
    {2, 1, 1, 180},
    {5, 1, 3, 225},
    {2, 2, 2, 0},
    {4, 2, 2, 0},
    {1, 2, 3, 0},
    {1, 3, 1, 270},
    {1, 3, 1, 360},
    {1, 4, 0, std::nullopt},
    {1, 4, 0, std::nullopt},
    {0, 4, 0, std::nullopt},
    {0, 4, 0, std::nullopt}
}};

std::uint8_t byte(const std::vector<std::uint8_t>& ds, std::size_t offset) {
    if (offset >= ds.size()) {
        throw std::runtime_error("truncated campaign DS table");
    }
    return ds[offset];
}

std::uint16_t word(const std::vector<std::uint8_t>& ds, std::size_t offset) {
    return static_cast<std::uint16_t>(byte(ds, offset)
        | (static_cast<std::uint16_t>(byte(ds, offset + 1)) << 8U));
}

std::int16_t signedWord(const std::vector<std::uint8_t>& ds, std::size_t offset) {
    const auto value = word(ds, offset);
    return static_cast<std::int16_t>(value <= 0x7fffU ? value : static_cast<int>(value) - 0x10000);
}

bool ordinarySelectorSupported(const std::uint8_t type, const Model3d& model) {
    if (type == 6) {
        return model.flags == 1 && model.animationLimits == std::vector<std::uint8_t>{type6AnimationLimit};
    }
    if (type == 11 || type == 12) {
        return model.flags == 1 && model.animationLimits.size() == 1 && model.animationLimits.front() != 0;
    }
    if (type == 1 || type == 3 || type == 5 || type == 7 || type == 8) {
        return model.flags == 0 || (model.flags == 1 && model.animationLimits.size() <= 1
            && (model.animationLimits.empty() || model.animationLimits.front() != 0));
    }
    return model.flags == 0;
}
}

std::uint8_t campaignDifficultyIndex(const CampaignDifficulty difficulty) {
    const auto index=static_cast<std::uint8_t>(difficulty);
    if (index>=difficultyCount) {throw std::invalid_argument("invalid campaign difficulty");}
    return index;
}

std::array<std::int8_t,7> campaignInitialCharges(const CampaignDifficulty difficulty) {
    //8CEE/113A copies the immutable DS1587+7*difficulty row to1580.
    constexpr std::array<std::array<std::int8_t,7>,3> charges{{
        {{3,3,3,3,3,3,3}},{{3,1,1,1,1,1,1}},{{3,0,0,0,0,0,0}}}};
    return charges[campaignDifficultyIndex(difficulty)];
}

unsigned campaignContinueBudget(const CampaignDifficulty difficulty) {
    return 2U-campaignDifficultyIndex(difficulty); //0168/016B.
}

std::uint16_t campaignScoreAward(const std::uint16_t award,const CampaignDifficulty difficulty) {
    const auto index=campaignDifficultyIndex(difficulty);
    constexpr std::uint16_t maximumPositiveAward=0x7fff;
    constexpr std::uint16_t unscaledAwardThreshold=1000;
    if (award>maximumPositiveAward) {throw std::invalid_argument("campaign award requires positive signed16 input");}
    //7977..7986: positive awards below1000 multiply first, then SAR1.
    return award>=unscaledAwardThreshold ? award : static_cast<std::uint16_t>(award*(index+2U)/2U);
}

std::uint8_t campaignGuardianInitialHealth(const std::uint8_t level,const CampaignDifficulty difficulty) {
    const auto index=campaignDifficultyIndex(difficulty);
    constexpr std::array<std::uint8_t,7> health{15,20,20,40,50,50,20}; //DS0720.
    if (level>=health.size()) {throw std::invalid_argument("invalid guardian campaign level");}
    //477A..4813 overrides only the final guardian after installing its base row.
    if (level==health.size()-1 && index!=0) {return static_cast<std::uint8_t>(40U*index);}
    return health[level];
}

CampaignLevelTable decodeCampaignLevel(
    const std::vector<std::uint8_t>& ds,
    const std::uint8_t level,
    const std::uint8_t difficulty
) {
    if (level >= levelCount || difficulty >= difficultyCount) {
        throw std::runtime_error("invalid campaign level or difficulty");
    }
    CampaignLevelTable result;
    result.level = level;
    result.enemyLimit = byte(ds, difficultyTable + difficulty * levelCount + level);
    result.sky = byte(ds, skyTable + level);
    const auto pointerIndex = level * 2U;
    result.schedulerPointer = word(ds, schedulerPointers + pointerIndex);
    result.weightsPointer = word(ds, weightsPointers + pointerIndex);
    const auto types = word(ds, typePointers + pointerIndex);
    const auto xs = word(ds, xPointers + pointerIndex);
    const auto ys = word(ds, yPointers + pointerIndex);
    const auto yaws = word(ds, yawPointers + pointerIndex);
    const auto count = byte(ds, countTable + level);
    for (std::size_t index = 0; index < count; ++index) {
        result.actors.push_back({byte(ds, types + index), signedWord(ds, xs + index * 2),
            signedWord(ds, ys + index * 2),
            static_cast<std::uint16_t>(byte(ds, yaws + index * 2) << 8U)});
    }
    return result;
}

CampaignLevelDefinition campaignLevelDefinition(const std::uint8_t level, const std::uint8_t difficulty) {
    if (level >= levelCount || difficulty >= difficultyCount) {
        throw std::runtime_error("invalid campaign level or difficulty");
    }
    auto result = campaignDefinitions[level];
    result.table.enemyLimit = enemyLimits[level][difficulty];
    return result;
}

bool campaignHorizonOverlayEnabled(const std::uint8_t level) {
    if (level >= levelCount) {
        throw std::runtime_error("invalid campaign horizon level");
    }
    return horizonOverlays[level];
}

std::uint8_t campaignExitDestination(const std::uint8_t level, const CampaignExit exit) {
    if (level >= levelCount) {
        throw std::invalid_argument("invalid campaign exit level");
    }
    switch (exit) {
    case CampaignExit::greenReward:
        return static_cast<std::uint8_t>(level + 1);
    case CampaignExit::blueWarp:
        return static_cast<std::uint8_t>(level + 3);
    default:
        throw std::invalid_argument("invalid campaign exit type");
    }
}

CampaignOrdinaryDefaults campaignOrdinaryDefaults(const std::uint8_t type) {
    if (type >= ordinaryDefaults.size()) {
        throw std::runtime_error("unsupported campaign ordinary defaults type");
    }
    return ordinaryDefaults[type];
}

std::uint8_t campaignSpawnThreshold(const std::uint8_t level, const std::uint8_t index) {
    if (level >= levelCount || index >= spawnThresholds.front().size()) {
        throw std::runtime_error("unsupported campaign spawn threshold index");
    }
    return spawnThresholds[level][index];
}

std::optional<std::uint8_t> campaignSpawnType(const CampaignLevelDefinition& definition,
    const std::uint8_t thresholdIndex, const std::int16_t spawnedCount, const std::uint16_t randomWord) {
    const auto threshold = campaignSpawnThreshold(definition.table.level, thresholdIndex);
    if (spawnedCount >= definition.table.enemyLimit) {
        return std::nullopt;
    }
    const auto low = static_cast<std::uint8_t>(randomWord);
    const auto high = static_cast<std::uint8_t>(randomWord >> 8U);
    if (low >= threshold || high == 0) {
        return std::nullopt;
    }
    std::uint8_t selectedType = 0;
    std::uint8_t selectedWeight = 0;
    for (std::size_t index = 0; index < definition.weights.size(); ++index) {
        if (selectedWeight < high && high < definition.weights[index]) {
            selectedType = static_cast<std::uint8_t>(index + 1);
            selectedWeight = definition.weights[index];
        }
    }
    if (selectedType == 0) {
        return std::nullopt;
    }
    const auto level = definition.table.level;
    if ((level == 1 && spawnedCount > 6 && spawnedCount < 10)
        || (level == 3 && spawnedCount > 8 && spawnedCount < 12)) {
        return 4;
    }
    return selectedType;
}

void initializeCampaignActors(const CampaignLevelTable& table, CampaignActorLoader& loader) {
    if (table.level >= levelCount) {
        throw std::runtime_error("invalid campaign level");
    }
    for (std::size_t index = 0; index < table.actors.size(); ++index) {
        const auto& entry = table.actors[index];
        CampaignActorLoader::Actor actor;
        if (entry.type == levelFixedType) {
            actor = loader.allocateFixed(static_cast<std::uint8_t>(levelFixedType + table.level));
            loader.write(actor, CampaignActorField::auxiliaryWord1a, 0);
            loader.write(actor, CampaignActorField::classification, -2);
            if ((table.level == 0 && index == 5) || (table.level == 3 && index == 6)) {
                loader.write(actor, CampaignActorField::behavior, -13);
                loader.write(actor, CampaignActorField::health, 16);
            }
        } else if (entry.type == animatedFixedType) {
            actor = loader.allocateFixed(animatedFixedType);
            loader.write(actor, CampaignActorField::classification, -4);
            loader.write(actor, CampaignActorField::health, 100);
            loader.write(actor, CampaignActorField::height, 0);
            loader.write(actor, CampaignActorField::animation, 2);
            loader.setPosition(actor, entry.x, entry.y);
            loader.presentFixed(actor);
            actor = loader.allocateFixed(companionType);
            loader.write(actor, CampaignActorField::health, 100);
            loader.write(actor, CampaignActorField::animation, 0);
            loader.aliasAnimation(actor);
        } else {
            actor = loader.allocateOrdinary(entry.type);
            loader.setYaw(actor, entry.yaw);
        }
        loader.setPosition(actor, entry.x, entry.y);
    }
}

void validateCampaignOrdinaryActors(const CampaignLevelTable& table,
    const std::vector<Model3d>& shared, const std::vector<Model3d>& local) {
    constexpr std::size_t ordinaryPoolSize = 16;
    if (table.level >= levelCount || table.actors.empty() || table.actors.size() > ordinaryPoolSize) {
        throw std::invalid_argument("unsupported typed campaign ordinary table");
    }
    for (std::size_t index = 0; index < table.actors.size(); ++index) {
        auto type = table.actors[index].type;
        if (type == 0 || type >= ordinaryDefaults.size()) {
            throw std::invalid_argument("unsupported typed campaign ordinary model");
        }
        if ((table.level == 1 && index > 6 && index < 10)
            || (table.level == 3 && index > 8 && index < 12)) {
            type = 4;
        }
        if (type >= shared.size() || campaignModelRadius(shared, local, type) == 0) {
            throw std::invalid_argument("unsupported typed campaign ordinary model");
        }
        const auto& model = shared[type];
        if (!ordinarySelectorSupported(type, model)) {
            throw std::invalid_argument("unsupported typed campaign selector ownership");
        }
    }
}

CampaignOrdinaryConstructor::CampaignOrdinaryConstructor(const std::uint8_t level,
    const std::vector<std::uint8_t>& types, const std::vector<Model3d>& shared,
    const std::vector<Model3d>& local) : level_(level) {
    if (level >= levelCount || types.empty()) {
        throw std::invalid_argument("unsupported typed campaign constructor");
    }
    for (const auto type : types) {
        if (type == 0 || type >= ordinaryDefaults.size() || type >= shared.size()
            || campaignModelRadius(shared, local, type) == 0) {
            throw std::invalid_argument("unsupported typed campaign ordinary model");
        }
        const auto& model = shared[type];
        if (!ordinarySelectorSupported(type, model)) {
            throw std::invalid_argument("unsupported typed campaign selector ownership");
        }
        models_.push_back({type, campaignModelRadius(shared, local, type)});
    }
}

Level1Entity CampaignOrdinaryConstructor::construct(const std::uint8_t type, StellarRng& rng) const {
    const auto model = std::find_if(models_.begin(), models_.end(), [type](const auto& item) {
        return item.type == type;
    });
    if (model == models_.end()) {
        throw std::invalid_argument("unbound typed campaign ordinary model");
    }
    Level1Entity actor{};
    actor.type = type;
    actor.collisionRadius = model->radius;
    actor.renderProfile = WorldActorRenderProfile::noHandles;
    actor.animatedModelPresenterEnabled = false;
    if (type == 6) {
        actor.renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
    }
    actor.stateBytes4dTo52[0] = 0xff;
    const auto placement = rng.next();
    auto x = static_cast<std::int16_t>((placement >> 8U) & 0xfU);
    std::int16_t y = 16;
    if ((placement & 1U) != 0) {
        std::swap(x, y);
    }
    if ((placement & 2U) != 0) {
        x = static_cast<std::int16_t>(-x);
    }
    if ((placement & 4U) != 0) {
        y = static_cast<std::int16_t>(-y);
    }
    actor.position = {campaignCoordinate(x) - 1, campaignCoordinate(y) - 1, 0};
    if (type == 11 || type == 12) {
        actor.position.z = 1000;
        actor.stateBytes4dTo52[2] = 5;
        actor.animatedModelPresenterEnabled = true;
        actor.renderProfile = WorldActorRenderProfile::heapSelectorDeferredAnimation;
        //137e allocates a zero selector; it does not invoke130c.
    }
    if (type == 10 || type == 13) {
        const auto random = rng.next();
        const auto speed = static_cast<std::int16_t>((((random >> 8U) & 3U) + 7U) << 8U);
        actor.yawVelocity = (random & 1U) != 0 ? -speed : speed;
    }
    if (type == 14 && level_ > 4) {
        actor.type = level_ + 10;
    }
    actor.yaw = static_cast<std::uint16_t>(campaignCoordinate(rng.next() & 0xff0U));
    const auto defaults = campaignOrdinaryDefaults(type);
    if (actor.type < 13) {
        actor.forwardVelocity = defaults.speed.value_or(0);
    }
    if (actor.type < 17) {
        actor.stateBytes4dTo52[1] = defaults.health;
        actor.stateBytes4dTo52[4] = defaults.behavior;
        actor.weaponSlotCount = defaults.weaponSlots;
    }
    return actor;
}

std::vector<Level1Entity> constructCampaignOrdinaryActors(const CampaignLevelTable& table,
    StellarRng& rng, const std::vector<Model3d>& shared, const std::vector<Model3d>& local) {
    validateCampaignOrdinaryActors(table, shared, local);
    class Loader final : public CampaignActorLoader {
    public:
        Loader(StellarRng& rng, const CampaignOrdinaryConstructor& constructor, const std::uint8_t level)
            : rng_(rng), constructor_(constructor), level_(level) {}

        Actor allocateOrdinary(std::uint8_t type) override {
            const auto spawned = actors_.size();
            if ((level_ == 1 && spawned > 6 && spawned < 10)
                || (level_ == 3 && spawned > 8 && spawned < 12)) {
                type = 4;
            }
            auto actor = constructor_.construct(type, rng_);
            actor.allocationIndex = actors_.size();
            actors_.push_back(actor);
            return actors_.size() - 1;
        }
        Actor allocateFixed(std::uint8_t) override { throw std::logic_error("ordinary loader cannot allocate fixed actors"); }
        void write(Actor, CampaignActorField, std::int16_t) override { throw std::logic_error("ordinary loader cannot write fixed fields"); }
        void setPosition(const Actor actor, const std::int16_t x, const std::int16_t y) override {
            actors_.at(actor).position.x = campaignCoordinate(x);
            actors_.at(actor).position.y = campaignCoordinate(y);
        }
        void setYaw(const Actor actor, const std::uint16_t yaw) override { actors_.at(actor).yaw = yaw; }
        void presentFixed(Actor) override { throw std::logic_error("ordinary loader cannot present fixed actors"); }
        void aliasAnimation(Actor) override { throw std::logic_error("ordinary loader cannot alias fixed animation"); }
        std::vector<Level1Entity> takeActors() { return std::move(actors_); }
    private:
        std::vector<Level1Entity> actors_;
        StellarRng& rng_;
        const CampaignOrdinaryConstructor& constructor_;
        std::uint8_t level_;
    };
    std::vector<std::uint8_t> types;
    for (std::size_t index = 0; index < table.actors.size(); ++index) {
        const bool substituted = (table.level == 1 && index > 6 && index < 10)
            || (table.level == 3 && index > 8 && index < 12);
        types.push_back(substituted ? 4 : table.actors[index].type);
    }
    const CampaignOrdinaryConstructor constructor(table.level, types, shared, local);
    Loader loader(rng, constructor, table.level);
    initializeCampaignActors(table, loader);
    return loader.takeActors();
}

void validateLevel3LoaderActors(const std::vector<Model3d>& shared,
    const std::vector<Model3d>& local) {
    const CampaignOrdinaryConstructor ordinary(level3Index, {level3YawActorType, level3GroundActorType}, shared, local);
    static_cast<void>(ordinary);
    for (const auto type : {animatedFixedType, companionType}) {
        if (type >= shared.size() || campaignModelRadius(shared, local, type) == 0) {
            throw std::invalid_argument("Level3 loader requires its fixed models");
        }
        const auto& model = shared[type];
        if (model.flags != 1 || model.animationLimits.size() != 1 || model.animationLimits.front() == 0) {
            throw std::invalid_argument("Level3 loader requires single-selector fixed models");
        }
    }
}

std::vector<Level1Entity> constructLevel3LoaderActors(StellarRng& rng,
    const std::vector<Model3d>& shared, const std::vector<Model3d>& local) {
    validateLevel3LoaderActors(shared, local);
    return constructCampaignLoaderActors(campaignLevelDefinition(level3Index, 0).table, rng, shared, local);
}

namespace {
std::uint8_t initialOrdinaryType(const std::uint8_t level, const std::size_t spawned, const std::uint8_t type) {
    if ((level == 1 && spawned > 6 && spawned < 10) || (level == 3 && spawned > 8 && spawned < 12)) {
        return 4;
    }
    return type;
}

std::vector<std::uint8_t> initialOrdinaryTypes(const CampaignLevelTable& table) {
    std::vector<std::uint8_t> types;
    for (const auto& entry : table.actors) {
        if (entry.type != levelFixedType && entry.type != animatedFixedType) {
            types.push_back(initialOrdinaryType(table.level, types.size(), entry.type));
        }
    }
    return types;
}
}

void validateCampaignLoaderActors(const CampaignLevelTable& table,
    const std::vector<Model3d>& shared, const std::vector<Model3d>& local) {
    constexpr std::size_t poolSize = 16;
    if (table.level >= levelCount || table.actors.empty()) {
        throw std::invalid_argument("unsupported typed campaign loader table");
    }
    const auto types = initialOrdinaryTypes(table);
    if (types.size() > poolSize) {
        throw std::invalid_argument("unsupported typed campaign ordinary pool size");
    }
    if (!types.empty()) {
        const CampaignOrdinaryConstructor ordinary(table.level, types, shared, local);
        static_cast<void>(ordinary);
    }
    std::size_t fixedCount = 0;
    for (const auto& entry : table.actors) {
        if (entry.type != levelFixedType && entry.type != animatedFixedType) {
            continue;
        }
        const bool animated = entry.type == animatedFixedType;
        const auto firstType = animated ? animatedFixedType : static_cast<std::uint8_t>(levelFixedType + table.level);
        for (const auto type : animated ? std::vector<std::uint8_t>{animatedFixedType, companionType}
                                       : std::vector<std::uint8_t>{firstType}) {
            if (++fixedCount > poolSize || type >= shared.size() || campaignModelRadius(shared, local, type) == 0) {
                throw std::invalid_argument("unsupported typed campaign fixed model or pool size");
            }
            const auto& model = shared[type];
            if (animated ? model.flags != 1 || model.animationLimits.size() != 1 || model.animationLimits.front() == 0
                         : model.flags != 0) {
                throw std::invalid_argument("unsupported typed campaign fixed selector ownership");
            }
        }
    }
}

std::vector<Level1Entity> constructCampaignLoaderActors(const CampaignLevelTable& table,
    StellarRng& rng, const std::vector<Model3d>& shared, const std::vector<Model3d>& local) {
    validateCampaignLoaderActors(table, shared, local);
    std::optional<CampaignOrdinaryConstructor> ordinary;
    const auto types = initialOrdinaryTypes(table);
    if (!types.empty()) {
        ordinary.emplace(table.level, types, shared, local);
    }
    class Loader final : public CampaignActorLoader {
    public:
        Loader(StellarRng& rng, const std::optional<CampaignOrdinaryConstructor>& ordinary,
            const std::vector<Model3d>& shared, const std::vector<Model3d>& local, const CampaignLevelTable& table)
            : rng_(rng), ordinary_(ordinary), shared_(shared), local_(local), level_(table.level) {
            actors_.reserve(table.actors.size() + 1);
        }
        Actor allocateOrdinary(const std::uint8_t type) override {
            const auto resolvedType = initialOrdinaryType(level_, ordinaryCount_++, type);
            return append(ordinary_->construct(resolvedType, rng_));
        }
        Actor allocateFixed(const std::uint8_t type) override {
            Level1Entity actor{};
            actor.type = type;
            actor.collisionRadius = campaignModelRadius(shared_, local_, type);
            actor.stateBytes4dTo52[0] = 0xfe;
            actor.stateBytes4dTo52[1] = fixedActorInitialHealth;
            actor.animatedModelPresenterEnabled = false;
            actor.renderProfile = WorldActorRenderProfile::noHandles;
            return append(actor);
        }
        void write(const Actor index, const CampaignActorField field, const std::int16_t value) override {
            auto& actor = actors_.at(index);
            switch (field) {
            case CampaignActorField::auxiliaryWord1a:
                if (value != 0) {
                    throw std::logic_error("unsupported campaign fixed callback");
                }
                actor.animatedModelPresenterEnabled = false;
                actor.renderProfile = WorldActorRenderProfile::noHandles;
                break;
            case CampaignActorField::classification:
                actor.stateBytes4dTo52[0] = static_cast<std::uint8_t>(value);
                break;
            case CampaignActorField::health:
                actor.stateBytes4dTo52[1] = static_cast<std::uint8_t>(value);
                break;
            case CampaignActorField::animation:
                actor.stateBytes4dTo52[2] = static_cast<std::uint8_t>(value);
                break;
            case CampaignActorField::height:
                actor.position.z = value;
                break;
            case CampaignActorField::behavior:
                actor.stateBytes4dTo52[4] = static_cast<std::uint8_t>(value);
                break;
            }
        }
        void setPosition(const Actor index, const std::int16_t x, const std::int16_t y) override {
            actors_.at(index).position.x = campaignCoordinate(x);
            actors_.at(index).position.y = campaignCoordinate(y);
        }
        void setYaw(const Actor index, const std::uint16_t yaw) override {
            actors_.at(index).yaw = yaw;
        }
        void presentFixed(const Actor index) override {
            auto& actor = actors_.at(index);
            //137e installs130c and a zero heap selector; it does not run it.
            actor.renderProfile = WorldActorRenderProfile::heapSelectorDeferredAnimation;
            actor.animatedModelPresenterEnabled = true;
        }
        void aliasAnimation(const Actor index) override {
            actors_.at(index).renderProfile = WorldActorRenderProfile::inlineSelectorNoCallback;
        }
        std::vector<Level1Entity> takeActors() { return std::move(actors_); }

    private:
        Actor append(Level1Entity actor) {
            actor.allocationIndex = actors_.size();
            actors_.push_back(actor);
            return actors_.size() - 1;
        }
        StellarRng& rng_;
        const std::optional<CampaignOrdinaryConstructor>& ordinary_;
        const std::vector<Model3d>& shared_;
        const std::vector<Model3d>& local_;
        std::vector<Level1Entity> actors_;
        std::uint8_t level_;
        std::size_t ordinaryCount_ = 0;
    };
    Loader loader(rng, ordinary, shared, local, table);
    initializeCampaignActors(table, loader);
    return loader.takeActors();
}

std::int32_t campaignCoordinate(const std::int16_t value) noexcept {
    const auto bits = static_cast<std::uint16_t>(static_cast<std::uint16_t>(value) << 11U);
    return bits <= 0x7fffU ? bits : static_cast<std::int32_t>(bits) - 0x10000;
}

std::uint32_t campaignModelRadius(
    const std::vector<Model3d>& shared,
    const std::vector<Model3d>& local,
    const std::uint16_t type
) noexcept {
    const Model3d* model = nullptr;
    if (type < shared.size()) {
        model = &shared[type];
    } else if (static_cast<std::size_t>(type) - shared.size() < local.size()) {
        model = &local[static_cast<std::size_t>(type) - shared.size()];
    }
    if (model == nullptr) {
        return 0;
    }
    auto radius = static_cast<std::uint32_t>(model->collisionRadius);
    if ((model->collisionRadius & 0x8000U) != 0) {
        radius |= 0xffff0000U;
    }
    for (std::uint16_t shift = 0; shift < model->scaleShift; ++shift) {
        radius <<= 1U;
    }
    return radius;
}

Level1Entity decodeCampaignActor(const std::vector<std::uint8_t>& ds,
    const std::uint16_t actor, const std::size_t allocationIndex) {
    if (actor == 0 || actor + actorBytes > ds.size()) {
        throw std::runtime_error("invalid campaign actor export pointer");
    }
    const auto dword = [&ds](const std::size_t at) {
        return static_cast<std::uint32_t>(word(ds, at))
            | (static_cast<std::uint32_t>(word(ds, at + 2)) << 16U);
    };
    const auto signedDword = [&dword](const std::size_t at) {
        const auto value = dword(at);
        return value <= 0x7fffffffU ? static_cast<std::int32_t>(value)
            : static_cast<std::int32_t>(static_cast<std::int64_t>(value) - 0x100000000LL);
    };
    Level1Entity entity{};
    entity.allocationIndex = allocationIndex;
    entity.flags = byte(ds, actor);
    entity.type = signedWord(ds, actor + 2);
    entity.collisionRadius = dword(actor + 4);
    entity.position = {signedDword(actor + 8), signedDword(actor + 0xc), signedDword(actor + 0x10)};
    entity.pitch = word(ds, actor + 0x14);
    entity.roll = word(ds, actor + 0x16);
    entity.yaw = word(ds, actor + 0x18);
    entity.pitchVelocity = signedWord(ds, actor + 0x30);
    entity.rollVelocity = signedWord(ds, actor + 0x32);
    entity.yawVelocity = signedWord(ds, actor + 0x34);
    entity.xVelocity = signedWord(ds, actor + 0x36);
    entity.yVelocity = signedWord(ds, actor + 0x38);
    entity.zVelocity = signedWord(ds, actor + 0x3a);
    entity.forwardVelocity = signedWord(ds, actor + 0x3c);
    for (std::size_t index = 0; index < entity.stateBytes4dTo52.size(); ++index) {
        entity.stateBytes4dTo52[index] = byte(ds, actor + 0x4d + index);
    }
    entity.behaviorTimer = byte(ds, actor + 0x54);
    if (entity.type == 6) {
        entity.type6CloakTimer = word(ds, actor + 0x55);
    }
    entity.presenterDistance = dword(actor + 0x49);
    entity.targetHeading = word(ds, actor + 0x57);
    const auto weaponSlots = byte(ds, actor + 0x59);
    if (weaponSlots > entity.weaponSlotTimers.size()) {
        throw std::runtime_error("unsupported campaign actor weapon slot count");
    }
    entity.weaponSlotCount = weaponSlots;
    entity.weaponCooldown = byte(ds, actor + 0x5a);
    for (std::size_t index = 0; index < entity.weaponSlotTimers.size(); ++index) {
        entity.weaponSlotTimers[index] = byte(ds, actor + 0x5b + index);
    }
    const auto animation = word(ds, actor + animationPointerOffset);
    entity.animatedModelPresenterEnabled = word(ds, actor + 0x1a) == 0x130c;
    if (animation != 0) {
        entity.modelAnimationVariant = byte(ds, animation);
        entity.presentationModelAnimationVariant = entity.modelAnimationVariant;
    }
    entity.ownerIsPlayer = entity.type == 23;
    return entity;
}

CampaignMemoryLoader::CampaignMemoryLoader(std::vector<std::uint8_t>& ds, CampaignAllocationServices& services)
    : ds_(ds), services_(services) {
    if (ds.size() != 0x10000U) {
        throw std::runtime_error("campaign allocator requires complete DS storage");
    }
}

void CampaignMemoryLoader::putByte(const std::size_t offset, const std::uint8_t value) {
    ds_.at(offset) = value;
}

void CampaignMemoryLoader::putWord(const std::size_t offset, const std::uint16_t value) {
    putByte(offset, static_cast<std::uint8_t>(value));
    putByte(offset + 1, static_cast<std::uint8_t>(value >> 8U));
}

void CampaignMemoryLoader::putDword(const std::size_t offset, const std::uint32_t value) {
    putWord(offset, static_cast<std::uint16_t>(value));
    putWord(offset + 2, static_cast<std::uint16_t>(value >> 16U));
}

CampaignActorLoader::Actor CampaignMemoryLoader::freeActor(const std::size_t pool, const std::size_t end) const {
    for (auto cursor = pool; cursor < end; cursor += 2) {
        const auto actor = word(ds_, cursor);
        if (actor == 0 || actor + actorBytes > ds_.size()) {
            throw std::runtime_error("invalid campaign pool actor pointer");
        }
        if (byte(ds_, actor + healthOffset) == 0) {
            return actor;
        }
    }
    throw std::runtime_error("unsupported exhausted campaign actor pool");
}

void CampaignMemoryLoader::appendAllocation(const Actor actor) {
    constexpr std::size_t allocationCount = 0x4e44;
    constexpr std::size_t allocationList = 0x49f8;
    
    const auto count = word(ds_, allocationCount);
    if (count >= supportedAllocationLimit) {
        throw std::runtime_error("unsupported campaign allocation count");
    }
    putWord(allocationList + count * 2, static_cast<std::uint16_t>(actor));
    putWord(allocationCount, static_cast<std::uint16_t>(count + 1));
}

void CampaignMemoryLoader::clearActor(const Actor actor) {
    if (actor == 0 || actor + actorBytes > ds_.size()) {
        throw std::runtime_error("invalid campaign actor reset pointer");
    }
    const auto animation = word(ds_, actor + animationPointerOffset);
    if (animation != 0 && animation != actor + animationOffset) {
        services_.releaseAnimation(animation);
    }
    std::fill_n(ds_.begin() + actor, actorBytes, 0);
}

void CampaignMemoryLoader::resetLevel() {
    for (std::size_t cursor = 0x4c16; cursor < 0x4c36; cursor += 2) {
        const auto actor = word(ds_, cursor);
        clearActor(actor);
        putByte(actor + 0x4d, 0xff);
    }
    for (std::size_t cursor = 0x45b0; cursor < 0x45c0; cursor += 2) {
        const auto actor = word(ds_, cursor);
        clearActor(actor);
        putByte(actor + 0x4d, 0xfe);
    }
    for (std::size_t cursor = 0x4972; cursor < 0x4982; cursor += 2) {
        const auto actor = word(ds_, cursor);
        if (actor == 0 || actor + actorBytes > ds_.size()) {
            throw std::runtime_error("invalid campaign projectile reset pointer");
        }
        std::fill_n(ds_.begin() + actor, actorBytes, 0);
        aliasAnimation(actor);
        putByte(actor + 0x4d, 0xfd);
    }
    for (std::size_t cursor = 0x47d8; cursor < 0x47e8; cursor += 2) {
        const auto actor = word(ds_, cursor);
        clearActor(actor);
        putByte(actor, 0x88);
        putByte(actor + 0x4d, 0xfc);
    }
    for (const auto offset : {0x1294, 0x1292, 0x1290, 0x128e, 0x51fc, 0x4e44}) {
        putWord(offset, 0);
    }
    putByte(0x1299, 1);
    const auto player = word(ds_, 0x522e);
    if (player == 0 || player + actorBytes > ds_.size()) {
        throw std::runtime_error("invalid campaign player reset pointer");
    }
    // Player49f4 does not invoke animation release.
    std::fill_n(ds_.begin() + player, actorBytes, 0);
    putDword(player + 4, services_.modelRadius(0));
    putByte(player + healthOffset, 1);
    putByte(0x132e, 0);
    putWord(0x12ac, 0);
    putWord(0x12aa, 0);
    putWord(0x129e, 7);
    putWord(0x503c, 0);
    constexpr std::uint16_t maximumArmor = 680;
    constexpr std::uint16_t transitionArmorGain = 80;
    auto armor = static_cast<std::uint16_t>(word(ds_, 0x47f8) + transitionArmorGain);
    const auto signedArmor = armor <= 0x7fffU ? static_cast<int>(armor) : static_cast<int>(armor) - 0x10000;
    if (word(ds_, 0x51f4) == 0 || signedArmor > maximumArmor) {
        armor = maximumArmor;
    }
    putWord(0x47f8, armor);
    for (const auto offset : {0x132a, 0x5232, 0x12ce, 0x12d0, 0x12d2, 0x12d6, 0x12d8, 0x12da, 0x51ec}) {
        putWord(offset, 0);
    }
}

void CampaignMemoryLoader::constructPools(const std::function<std::uint16_t(std::size_t)>& allocateRecord) {
    struct Pool {
        std::size_t begin;
        std::size_t end;
        std::uint8_t type;
    };
    constexpr Pool pools[] = {{0x4c16, 0x4c36, 0}, {0x45b0, 0x45d0, 0},
        {0x4972, 0x4982, 23}, {0x47d8, 0x47e8, 0}};
    for (const auto& pool : pools) {
        for (auto cursor = pool.begin; cursor < pool.end; cursor += 2) {
            const auto actor = allocateRecord(actorBytes);
            if (actor == 0 || actor + actorBytes > ds_.size()) {
                throw std::runtime_error("invalid campaign pool allocation");
            }
            std::fill_n(ds_.begin() + actor, actorBytes, 0);
            putWord(actor + 2, pool.type);
            putDword(actor + 4, services_.modelRadius(pool.type));
            putWord(cursor, actor);
        }
    }
}

CampaignActorLoader::Actor CampaignMemoryLoader::allocateOrdinary(std::uint8_t type) {
    const auto level = signedWord(ds_, 0x51f4);
    const auto spawned = signedWord(ds_, 0x12ac);
    if ((level == 1 && spawned > 6 && spawned < 10)
        || (level == 3 && spawned > 8 && spawned < 12)) {
        type = 4;
    }
    const auto actor = freeActor(0x4c16, 0x4c36);
    appendAllocation(actor);
    const auto animation = word(ds_, actor + animationPointerOffset);
    if (animation != 0 && animation != actor + animationOffset) {
        services_.releaseAnimation(animation);
    }
    std::fill_n(ds_.begin() + actor, actorBytes, 0);
    putWord(actor + 2, type);
    putDword(actor + 4, services_.modelRadius(type));
    putByte(actor + 0x4d, 0xff);
    const auto positionRandom = services_.nextRandom();
    std::int16_t x = static_cast<std::int16_t>((positionRandom >> 8U) & 0xfU);
    std::int16_t y = 16;
    if ((positionRandom & 1U) != 0) {
        std::swap(x, y);
    }
    if ((positionRandom & 2U) != 0) {
        x = static_cast<std::int16_t>(-x);
    }
    if ((positionRandom & 4U) != 0) {
        y = static_cast<std::int16_t>(-y);
    }
    putWord(actor + 0x1a, 0);
    putDword(actor + 8, static_cast<std::uint32_t>(campaignCoordinate(x) - 1));
    putDword(actor + 0xc, static_cast<std::uint32_t>(campaignCoordinate(y) - 1));
    if (type == 11 || type == 12) {
        putDword(actor + 0x10, 1000);
        putByte(actor + animationOffset, 5);
        presentFixed(actor);
    }
    if (type == 10 || type == 13) {
        const auto random = services_.nextRandom();
        auto velocity = static_cast<std::int16_t>((((random >> 8U) & 3U) + 7U) << 8U);
        if ((random & 1U) != 0) {
            velocity = static_cast<std::int16_t>(-velocity);
        }
        putWord(actor + 0x34, static_cast<std::uint16_t>(velocity));
    }
    if (type == 14 && level > 4) {
        putWord(actor + 2, static_cast<std::uint16_t>(level + 10));
    }
    const auto headingRandom = services_.nextRandom();
    putWord(actor + 0x18, static_cast<std::uint16_t>(campaignCoordinate(static_cast<std::int16_t>(headingRandom & 0xff0U))));
    const auto finalType = word(ds_, actor + 2);
    if (finalType < 13) {
        putWord(actor + 0x3c, word(ds_, 0xa00 + type * 2));
    }
    if (finalType < 17) {
        putByte(actor + healthOffset, byte(ds_, 0x9ee + type));
        putByte(actor + 0x51, byte(ds_, 0xabe + type));
        putByte(actor + 0x59, byte(ds_, 0xa56 + type));
    }
    putWord(0x12aa, static_cast<std::uint16_t>(word(ds_, 0x12aa) + 1));
    putWord(0x12ac, static_cast<std::uint16_t>(word(ds_, 0x12ac) + 1));
    putByte(0x1296, 1);
    return actor;
}

CampaignActorLoader::Actor CampaignMemoryLoader::allocateFixed(const std::uint8_t type) {
    const auto actor = freeActor(0x45b0, 0x45d0);
    putByte(actor + 0x4d, 0xfe);
    putDword(actor + 0x10, 0);
    putByte(actor + healthOffset, 100);
    putWord(actor + 0x18, 0);
    putWord(actor + 0x16, 0);
    putWord(actor + 0x14, 0);
    putByte(actor, 0);
    putWord(actor + 2, type);
    putDword(actor + 4, services_.modelRadius(type));
    putByte(actor + 0x51, 0);
    putByte(0x1296, 1);
    appendAllocation(actor);
    return actor;
}

void CampaignMemoryLoader::write(const Actor actor, const CampaignActorField field, const std::int16_t value) {
    switch (field) {
    case CampaignActorField::auxiliaryWord1a: putWord(actor + 0x1a, static_cast<std::uint16_t>(value)); break;
    case CampaignActorField::classification: putByte(actor + 0x4d, static_cast<std::uint8_t>(value)); break;
    case CampaignActorField::health: putByte(actor + healthOffset, static_cast<std::uint8_t>(value)); break;
    case CampaignActorField::animation: putByte(actor + animationOffset, static_cast<std::uint8_t>(value)); break;
    case CampaignActorField::behavior: putByte(actor + 0x51, static_cast<std::uint8_t>(value)); break;
    case CampaignActorField::height: putDword(actor + 0x10, static_cast<std::uint32_t>(value)); break;
    }
}

void CampaignMemoryLoader::setPosition(const Actor actor, const std::int16_t x, const std::int16_t y) {
    putDword(actor + 8, static_cast<std::uint32_t>(campaignCoordinate(x)));
    putDword(actor + 0xc, static_cast<std::uint32_t>(campaignCoordinate(y)));
}

void CampaignMemoryLoader::setYaw(const Actor actor, const std::uint16_t yaw) {
    putWord(actor + 0x18, yaw);
}

void CampaignMemoryLoader::presentFixed(const Actor actor) {
    services_.presentFixed(static_cast<std::uint16_t>(actor));
}

void CampaignMemoryLoader::aliasAnimation(const Actor actor) {
    putWord(actor + animationPointerOffset, static_cast<std::uint16_t>(actor + animationOffset));
}
}

