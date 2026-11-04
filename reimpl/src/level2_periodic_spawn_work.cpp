#include "stellar7/level2_periodic_spawn_work.hpp"
#include "stellar7/campaign_loader.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::uint8_t levelIndex = 1;
constexpr std::size_t sharedModelCount = 44;
constexpr std::size_t allocationCapacity = 32;
constexpr std::size_t transientPoolCapacity = 8;
constexpr std::size_t logicalCapacity = 16 + 16 + 8 + 8;
constexpr std::uint16_t actorBytes = 99;
constexpr std::int16_t postWaveSpawnLimit = 14;
using Operations = std::vector<NormalWorkOperation>;

void instructions(Operations& operations, const std::uint64_t count) {
    operations.push_back({NormalWorkKind::instructions, count});
}

std::uint16_t random(Operations& operations, StellarRng& rng) {
    instructions(operations, 20 + (rng.indexA() == 0 ? 1 : 0) + (rng.indexB() == 0 ? 1 : 0));
    return rng.next();
}

bool ordinaryType(const std::int16_t type) {
    return type == 1 || type == 4 || type == 8 || type == 13;
}

std::int32_t coordinate(const std::int16_t value) {
    const auto word = static_cast<std::uint16_t>(static_cast<std::uint16_t>(value) << 11U);
    return (word <= 0x7fffU ? std::int32_t{word} : std::int32_t{word} - 0x10000) - 1;
}

void validate(const Level2PeriodicSpawnInput& input, const StellarRng& rng) {
    if (input.difficulty != 0 || input.activeEnemies < 0
        || input.activeEnemies > input.spawnedEnemies || input.spawnedEnemies < 0
        || input.spawnedEnemies > postWaveSpawnLimit || input.allocationOrder.size() > allocationCapacity
        || input.nextEntityIndex > logicalCapacity
        || rng.indexA() >= StellarRng::wordCount * 2 || rng.indexB() >= StellarRng::wordCount * 2
        || rng.indexA() % 2 != 0 || rng.indexB() % 2 != 0) {
        throw std::invalid_argument("unsupported Level2 periodic spawn profile");
    }
    std::vector<Level1ObjectRef> listed;
    for (const auto ref : input.allocationOrder) {
        const bool valid = ref.kind == Level1PresentationKind::entity ? ref.index < input.nextEntityIndex
            : (ref.kind == Level1PresentationKind::projectile || ref.kind == Level1PresentationKind::deathEffect)
                && ref.index < transientPoolCapacity;
        if (!valid || std::find(listed.begin(), listed.end(), ref) != listed.end()) {
            throw std::invalid_argument("Level2 periodic allocation references must be valid and unique");
        }
        listed.push_back(ref);
    }
    std::vector<std::size_t> identities;
    for (const auto& slot : input.ordinaryPool) {
        const auto& actor = slot.actor;
        const auto postWave = actor.type == 7 || actor.type == 44;
        if (actor.stateBytes4dTo52[0] != 255 || (postWave ? actor.renderProfile != (actor.type == 7
                ? WorldActorRenderProfile::inlineSelectorNoCallback : WorldActorRenderProfile::heapSelectorDeferredAnimation)
                    : actor.renderProfile != WorldActorRenderProfile::noHandles)
            || (!postWave && (actor.animatedModelPresenterEnabled || actor.primaryType11AnimationSelector))
            || (actor.type != 0 && !ordinaryType(actor.type) && !postWave)) {
            throw std::invalid_argument("unsupported Level2 periodic physical ordinary ancestry");
        }
        if (!slot.entityIndex) {
            if (actor.type != 0 || actor.active()) {
                throw std::invalid_argument("materialized Level2 ordinary slot lacks its logical mapping");
            }
            continue;
        }
        const auto index = *slot.entityIndex;
        const auto ref = Level1ObjectRef{Level1PresentationKind::entity, index};
        if (index >= input.nextEntityIndex || actor.allocationIndex != index || (!ordinaryType(actor.type) && !postWave)
            || std::find(identities.begin(), identities.end(), index) != identities.end()
            || actor.active() != (std::find(listed.begin(), listed.end(), ref) != listed.end())) {
            throw std::invalid_argument("Level2 periodic ordinary mappings must preserve current physical aliases");
        }
        identities.push_back(index);
    }
}

void construct(const Level2PeriodicSpawnInput& input, const std::vector<Model3d>& shared,
    Level2PeriodicSpawnWork& result) {
    const auto type = *result.effectiveType;
    const auto free = std::find_if(input.ordinaryPool.begin(), input.ordinaryPool.end(),
        [](const auto& slot) { return !slot.actor.active(); });
    const auto slot = static_cast<std::size_t>(free - input.ordinaryPool.begin());
    //4370..43A1 Level2 branch: <7, forced7..9, or >9, before43A5.
    const auto prefix = input.spawnedEnemies < 7 ? 10U : input.spawnedEnemies <= 9 ? 11U : 12U;
    auto& operations = result.operations;
    if (free == input.ordinaryPool.end()) {
        instructions(operations, prefix + 2 + 7 * input.ordinaryPool.size() + 4);
        result.outcome = Level1PeriodicSpawnOutcome::poolFull;
        return;
    }
    if (input.allocationOrder.size() == allocationCapacity
        || (!free->entityIndex && input.nextEntityIndex == logicalCapacity) || shared.size() != sharedModelCount
        || shared[type].scaleShift != 0 || shared[type].collisionRadius == 0) {
        throw std::invalid_argument("unsupported Level2 periodic owned model/available allocation profile");
    }
    instructions(operations, prefix + 11 + 7 * slot); //Through43D2, no prior selector.
    instructions(operations, 2 + 8); //43DB/CALL49F4,49F4 clear setup.
    operations.push_back({NormalWorkKind::repeatString, actorBytes});
    instructions(operations, 4); //49F4 return, before43DF.
    const auto cacheReset = Level2PeriodicCacheReset{operations.size(), 0x43df, slot};
    instructions(operations, 1 + 6 + 48 + 6); //43DF..4401, whole sharedD03C/C05C.

    const auto placement = random(operations, result.rng);
    instructions(operations, 37 + 18 + ((placement & 1U) != 0 ? 2 : 0)
        + ((placement & 2U) != 0 ? 1 : 0) + ((placement & 4U) != 0 ? 1 : 0));
    auto x = static_cast<std::int16_t>((placement >> 8U) & 15U);
    std::int16_t y = 16;
    if ((placement & 1U) != 0) { std::swap(x, y); }
    if ((placement & 2U) != 0) { x = static_cast<std::int16_t>(-x); }
    if ((placement & 4U) != 0) { y = static_cast<std::int16_t>(-y); }

    Level1Entity actor{};
    actor.allocationIndex = free->entityIndex.value_or(input.nextEntityIndex);
    actor.type = type;
    actor.collisionRadius = shared[type].collisionRadius;
    actor.position = {coordinate(x), coordinate(y), 0};
    actor.animatedModelPresenterEnabled = false;
    actor.renderProfile = WorldActorRenderProfile::noHandles;
    instructions(operations, 5); //4404..4411, no11/12 selector constructor.
    if (type == 13) {
        instructions(operations, 2 + 1); //4428/442C,4434 CALLB39E.
        const auto spin = random(operations, result.rng);
        instructions(operations, 6 + ((spin & 1U) != 0 ? 1 : 0));
        const auto magnitude = static_cast<std::int16_t>((spin & 0x0300U) + 0x0700U);
        actor.yawVelocity = (spin & 1U) != 0 ? static_cast<std::int16_t>(-magnitude) : magnitude;
    } else {
        instructions(operations, 4); //4428..4432.
    }
    instructions(operations, 2 + 1); //444C/4450,4466 CALLB39E.
    const auto heading = random(operations, result.rng);
    instructions(operations, 3 + 9 + 4 + (type == 13 ? 0 : 4) + 2 + 7 + 8);
    actor.yaw = static_cast<std::uint16_t>(static_cast<std::uint16_t>(heading & 0x0ff0U) << 11U);
    const auto defaults = campaignOrdinaryDefaults(type);
    actor.forwardVelocity = defaults.speed.value_or(0);
    actor.stateBytes4dTo52 = {255, defaults.health, 0, 0, defaults.behavior, 0};
    actor.weaponSlotCount = defaults.weaponSlots;
    result.outcome = Level1PeriodicSpawnOutcome::spawned;
    result.spawn = Level2PeriodicSpawnReceipt{static_cast<std::uint8_t>(slot), actor.allocationIndex, actor,
        input.allocationOrder.size(), static_cast<std::int16_t>(input.activeEnemies + 1),
        static_cast<std::int16_t>(input.spawnedEnemies + 1), true, cacheReset};
    result.allocationOrder.push_back({Level1PresentationKind::entity, actor.allocationIndex});
}
} // namespace

Level2PeriodicSpawnWork level2PeriodicSpawnOperations(const Level2PeriodicSpawnInput& input,
    const StellarRng& incomingRng, const std::vector<Model3d>& ownedShared) {
    validate(input, incomingRng);
    const auto definition = campaignLevelDefinition(levelIndex, input.difficulty);
    Level2PeriodicSpawnWork result{incomingRng, Level1PeriodicSpawnOutcome::limit, {}, {}, {}, input.allocationOrder, {}};
    auto& operations = result.operations;
    instructions(operations, 9); //0DDC..0DF0.
    if (input.spawnedEnemies >= definition.table.enemyLimit) {
        instructions(operations, 4); //0E4B..0E4F.
        return result;
    }
    instructions(operations, 1); //0DF2 CALLB39E.
    const auto choice = random(operations, result.rng);
    instructions(operations, 6); //0DF5..0E05.
    if (static_cast<std::uint8_t>(choice) >= campaignSpawnThreshold(levelIndex,
        static_cast<std::uint8_t>(input.activeEnemies))) {
        result.outcome = Level1PeriodicSpawnOutcome::threshold;
        instructions(operations, 4);
        return result;
    }
    instructions(operations, 2); //0E07/0E0B.
    const auto high = static_cast<std::uint8_t>(choice >> 8U);
    if (high == 0) {
        result.outcome = Level1PeriodicSpawnOutcome::highZero;
        instructions(operations, 4);
        return result;
    }
    instructions(operations, 2); //0E0D/0E11; selectedType initialized at0DE3.
    std::uint8_t selectedWeight = 0;
    for (std::size_t index = 0; index < definition.weights.size(); ++index) {
        const auto weight = definition.weights[index];
        instructions(operations, 6); //0E13..0E22.
        if (selectedWeight < high) {
            instructions(operations, 2); //0E24/0E27.
            if (high < weight) {
                selectedWeight = weight;
                result.selectedType = static_cast<std::uint8_t>(index + 1);
                instructions(operations, 4); //0E29..0E32.
            }
        }
        instructions(operations, 3); //0E35..0E39.
    }
    instructions(operations, 2); //0E3B/0E3F.
    if (!result.selectedType) {
        result.outcome = Level1PeriodicSpawnOutcome::noType;
        instructions(operations, 4);
        return result;
    }
    if (!ordinaryType(*result.selectedType)) {
        throw std::invalid_argument("unsupported Level2 periodic selected constructor");
    }
    result.effectiveType = input.spawnedEnemies >= 7 && input.spawnedEnemies <= 9 ? 4 : *result.selectedType;
    instructions(operations, 3); //0E41/0E44/0E45 CALL4370.
    construct(input, ownedShared, result);
    instructions(operations, 5); //0E48,0E4B..0E4F.
    return result;
}

} // namespace stellar7
