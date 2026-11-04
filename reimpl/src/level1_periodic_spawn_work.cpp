#include "stellar7/level1_periodic_spawn_work.hpp"
#include "stellar7/campaign_loader.hpp"
#include "level1_ordinary_constructor.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
bool level1RetiredType11Selector(const Level1Entity& actor) noexcept {
    return actor.type==11 && actor.stateBytes4dTo52[0]==255 && !actor.active()
        && (actor.primaryType11AnimationSelector
            || actor.renderProfile==WorldActorRenderProfile::heapSelectorDeferredAnimation
            || actor.renderProfile==WorldActorRenderProfile::heapSelectorNoCallback);
}
void validateLevel1SelectorProjection(const Level1PeriodicSpawnInput& input) {
    if(input.selectorReuse!=Level1SelectorReuseDomain::retainedType11 || !input.retainedEntities
        || input.nextEntityIndex!=input.retainedEntities->size()) {
        throw std::invalid_argument("retained selector release requires canonical FF storage");
    }
    std::size_t slot=0;
    for(std::size_t index=0;index<input.retainedEntities->size();++index) {
        const auto& actor=(*input.retainedEntities)[index];
        if(actor.stateBytes4dTo52[0]!=255) {continue;}
        if(slot==input.ordinaryPool.size()) {throw std::invalid_argument("retained selector pool exceeds source capacity");}
        const auto& projected=input.ordinaryPool[slot++];
        const auto heap=actor.primaryType11AnimationSelector
            || actor.renderProfile==WorldActorRenderProfile::heapSelectorDeferredAnimation
            || actor.renderProfile==WorldActorRenderProfile::heapSelectorNoCallback;
        if(actor.allocationIndex!=index || !projected.entityIndex || *projected.entityIndex!=index
            || projected.health!=actor.stateBytes4dTo52[1] || projected.hasAnimation!=heap) {
            throw std::invalid_argument("selector release requires the actual physical FF projection");
        }
    }
    for(;slot<input.ordinaryPool.size();++slot) {
        const auto& projected=input.ordinaryPool[slot];
        if(projected.entityIndex || projected.health!=0 || projected.hasAnimation) {
            throw std::invalid_argument("unmaterialized selector slot has retained state");
        }
    }
}
namespace {
constexpr std::uint8_t enemyLimit = 10;
constexpr std::uint16_t allocationCapacity = 32;
constexpr std::uint16_t sharedModelCapacity = 44;
constexpr std::uint16_t actorBytes = 99;
constexpr std::uint16_t selectorBytes = 1;
constexpr std::uint16_t maximumRngIndex = (StellarRng::wordCount - 1) * 2;
using Operations = std::vector<NormalWorkOperation>;

void instructions(Operations& operations, const std::uint64_t count) {
    operations.push_back({NormalWorkKind::instructions, count});
}

std::uint16_t random(Operations& operations, StellarRng& rng) {
    instructions(operations, 20 + (rng.indexA() == 0 ? 1 : 0) + (rng.indexB() == 0 ? 1 : 0));
    return rng.next();
}

std::int32_t coordinate(const std::int16_t value) {
    const auto word = static_cast<std::uint16_t>(static_cast<std::uint16_t>(value) << 11U);
    return (word <= 0x7fffU ? std::int32_t{word} : std::int32_t{word} - 0x10000) - 1;
}

void validate(const Level1PeriodicSpawnInput& input, const StellarRng& rng) {
    if (input.enemyLimit != enemyLimit || input.activeEnemies < 0
        || input.activeEnemies > input.spawnedEnemies || input.spawnedEnemies < 0
        || input.spawnedEnemies > enemyLimit || input.allocationCount > allocationCapacity
        || rng.indexA() > maximumRngIndex || rng.indexB() > maximumRngIndex
        || rng.indexA() % 2 != 0 || rng.indexB() % 2 != 0) {
        throw std::invalid_argument("unsupported Level1 periodic spawn profile");
    }
    std::vector<std::size_t> identities;
    for (const auto& slot : input.ordinaryPool) {
        if (slot.health != 0 && !slot.entityIndex) {
            throw std::invalid_argument("active ordinary slot lacks its existing entity identity");
        }
        if (slot.entityIndex) {
            if (*slot.entityIndex >= input.nextEntityIndex
                || std::find(identities.begin(), identities.end(), *slot.entityIndex) != identities.end()) {
                throw std::invalid_argument("ordinary spawn pool identities must be distinct and existing");
            }
            identities.push_back(*slot.entityIndex);
        }
    }
}
}

static Level1PeriodicSpawnWork periodicSpawnPlan(const Level1PeriodicSpawnInput& input,
    const StellarRng& incomingRng,const std::vector<Model3d>& sharedModels,
    detail::OrdinaryConstructorProfile* profile) {
    validate(input, incomingRng);
    Level1PeriodicSpawnWork result{incomingRng, Level1PeriodicSpawnOutcome::limit, {}, {}, {}, {}, {}};
    auto& before = result.beforeProvider;
    instructions(before, 9); //0DDC..0DF0.
    if (input.spawnedEnemies >= input.enemyLimit) {
        instructions(before, 4); //0E4B..0E4F.
        return result;
    }
    instructions(before, 1); //0DF2 CALLB39E.
    const auto choice = random(before, result.rng);
    instructions(before, 6); //0DF5..0E05.
    const auto definition = campaignLevelDefinition(0, 1);
    const auto activeIndex = static_cast<std::uint8_t>(input.activeEnemies);
    if (static_cast<std::uint8_t>(choice) >= campaignSpawnThreshold(0, activeIndex)) {
        result.outcome = Level1PeriodicSpawnOutcome::threshold;
        instructions(before, 4);
        return result;
    }
    instructions(before, 2); //0E07/0E0B.
    const auto high = static_cast<std::uint8_t>(choice >> 8U);
    if (high == 0) {
        result.outcome = Level1PeriodicSpawnOutcome::highZero;
        instructions(before, 4);
        return result;
    }
    instructions(before, 2); //0E0D/0E11.
    std::uint8_t selectedWeight = 0;
    for (const auto weight : definition.weights) {
        instructions(before, 6); //0E13..0E22.
        if (selectedWeight < high) {
            instructions(before, 2); //0E24/0E27.
            if (high < weight) {
                selectedWeight = weight;
                instructions(before, 4); //0E29..0E32.
            }
        }
        instructions(before, 3); //0E35..0E39.
    }
    result.selectedType = campaignSpawnType(definition, activeIndex, input.spawnedEnemies, choice);
    instructions(before, 2); //0E3B/0E3F.
    if (!result.selectedType) {
        result.outcome = Level1PeriodicSpawnOutcome::noType;
        instructions(before, 4);
        return result;
    }
    const auto type = *result.selectedType;
    if (type != 1 && type != 3 && type != 11) {
        throw std::invalid_argument("unsupported Level1 periodic selected type");
    }
    instructions(before, 3); //0E41/0E44/0E45 CALL4370.
    auto child = detail::ordinaryConstructorOperations(input,result.rng,sharedModels,type,profile);
    before.insert(before.end(),child.beforeProvider.begin(),child.beforeProvider.end());
    result.rng = child.rng; result.outcome = child.outcome; result.spawn = child.spawn;
    result.provider = child.provider; result.afterProvider = std::move(child.afterProvider);
    if(child.selectorRelease) {
        child.selectorRelease->operationOffset+=before.size()-child.beforeProvider.size();
        result.selectorRelease=child.selectorRelease;
    }
    instructions(result.provider ? result.afterProvider : before,5);
    return result;
}

Level1PeriodicSpawnWork level1PeriodicSpawnOperations(const Level1PeriodicSpawnInput& input,
    const StellarRng& rng,const std::vector<Model3d>& models) {
    if(input.selectorReuse!=Level1SelectorReuseDomain::rejectAnimated || input.retainedEntities) {
        throw std::invalid_argument("ordinary spawn requires its strict selector domain");
    }
    return periodicSpawnPlan(input,rng,models,nullptr);
}
Level1PeriodicSpawnWork level1RetainedPeriodicSpawnOperations(const Level1PeriodicSpawnInput& input,
    const StellarRng& rng,const std::vector<Model3d>& models) {
    validateLevel1SelectorProjection(input);
    detail::OrdinaryConstructorProfile profile{false,input.retainedEntities,{}};
    return periodicSpawnPlan(input,rng,models,&profile);
}

namespace detail {
Level1PeriodicSpawnWork ordinaryConstructorOperations(const Level1PeriodicSpawnInput& input,
    const StellarRng& incomingRng,const std::vector<Model3d>& sharedModels,const std::uint8_t type,OrdinaryConstructorProfile* profile) {
    const auto level2PostWave=profile && profile->domain==OrdinaryConstructorDomain::level2PostWave;
    const auto level2Emission=profile && profile->domain==OrdinaryConstructorDomain::level2GuardianEmission;
    const auto level2=level2PostWave || level2Emission;
    const auto level2Guardian=level2PostWave && type==44;
    constexpr std::uint8_t level2EnemyLimit=12;
    if(profile && ((profile->domain!=OrdinaryConstructorDomain::level1 && !level2)
        || (level2PostWave && ((type!=7 && type!=44) || profile->guardian!=(type==44)
            || input.enemyLimit!=level2EnemyLimit || input.spawnedEnemies<level2EnemyLimit))
        || (level2Emission && (type!=13 || profile->guardian || input.enemyLimit!=level2EnemyLimit
            || input.spawnedEnemies<14)))) {
        throw std::invalid_argument("unsupported ordinary constructor domain");
    }
    if(type!=1 && type!=3 && type!=7 && type!=11 && !(type==44 && profile && profile->guardian) && !level2Emission) {
        throw std::invalid_argument("unsupported ordinary constructor type");
    }
    Level1PeriodicSpawnWork result{incomingRng,Level1PeriodicSpawnOutcome::limit,type,{},{},{},{}};
    auto& before=result.beforeProvider;
    const auto free = std::find_if(input.ordinaryPool.begin(), input.ordinaryPool.end(),
        [](const auto& slot) { return slot.health == 0; });
    const auto slot = static_cast<std::size_t>(free - input.ordinaryPool.begin());
    if (free == input.ordinaryPool.end()) {
        result.outcome = Level1PeriodicSpawnOutcome::poolFull;
        instructions(before, 10 + 7 * input.ordinaryPool.size() + 4 + (level2 ? 4 : 0));
        return result;
    }
    if ((free->hasAnimation && !profile) || input.allocationCount >= allocationCapacity) {
        throw std::invalid_argument("ordinary spawn requires available registry and no prior animation storage");
    }
    if(profile && !profile->guardian && !level2Emission && profile->retainedEntities && free->entityIndex) {
        const auto& prior=profile->retainedEntities->at(*free->entityIndex);
        if(prior.type!=1 && prior.type!=3 && prior.type!=11) {
            throw std::invalid_argument("ordinary selector reuse does not admit other prior source types");
        }
        if(prior.type==11 && !free->hasAnimation) {
            throw std::invalid_argument("retired Type11 lost its heap selector projection");
        }
    }
    if(free->hasAnimation && !profile->guardian && !level2Emission) {
        if(!profile->retainedEntities || !free->entityIndex
            || *free->entityIndex>=profile->retainedEntities->size()
            || !level1RetiredType11Selector((*profile->retainedEntities)[*free->entityIndex])) {
            throw std::invalid_argument("ordinary13AC release requires the retired physical Type11 owner");
        }
    }
    if(level2Emission && free->hasAnimation && (!free->entityIndex || !profile->retainedEntities
        || *free->entityIndex>=profile->retainedEntities->size()
        || (*profile->retainedEntities)[*free->entityIndex].active()
        || ((*profile->retainedEntities)[*free->entityIndex].renderProfile!=WorldActorRenderProfile::heapSelectorDeferredAnimation
            && (*profile->retainedEntities)[*free->entityIndex].renderProfile!=WorldActorRenderProfile::heapSelectorNoCallback))) {
        throw std::invalid_argument("guardian emission selector release requires its retired heap owner");
    }
    constexpr std::uint32_t level2GuardianRadius=1147;
    const auto radius = type == 1 ? 1342U : type == 3 ? 770U : type == 7 ? 13052U
        : type == 44 ? (level2Guardian ? level2GuardianRadius : 2792U) : type==13 ? 559U : 748U;
    //The native resource owner appends the level bank after this shared block.
    if (sharedModels.size() < (type==44 ? sharedModelCapacity+1 : sharedModelCapacity) || sharedModels[type].scaleShift != 0
        || sharedModels[type].collisionRadius != radius || sharedModels[type].flags != (type==44 && !level2Guardian ? 3 : 1)
        || (type==44 && sharedModels[type].animationLimits != (level2Guardian
            ? std::vector<std::uint8_t>{6} : std::vector<std::uint8_t>{8,8,8}))
        || ((type == 11 || type == 7) && sharedModels[type].animationLimits != std::vector<std::uint8_t>{static_cast<std::uint8_t>(type == 7 ? 4 : 8)})) {
        throw std::invalid_argument("unsupported Level1 ordinary source model registry");
    }
    instructions(before, 19 + 7 * slot + (level2 ? 4 : 0)); //Level2 4370 count comparisons before43A5.
    if(free->hasAnimation) {
        instructions(before,2+10); //43D4/CALL13AC;13AC through CALLDCCA.
        profile->releaseOperationOffset=before.size();
        if(!profile->guardian) {
            result.selectorRelease=Level1SelectorReleaseBoundary{before.size(),{*free->entityIndex}};
        }
        instructions(before,5+15+7+1); //13C3..13CD with actual DCC6(FFFF) gate,13D0 return,43D8.
    }
    instructions(before,2+8); //4370 pool/append/animation0;49F4 clear setup.
    before.push_back({NormalWorkKind::repeatString, actorBytes});
    instructions(before, 4 + 1 + 6 + 6 + (type==44 ? 40 : 27) + 15 + 6); //49F4 return;43DF..4401;D03C/C05C.
    //4862 includes its CALLB39E and two complete417A conversions.
    const auto placement = random(before, result.rng);
    instructions(before, 37 + 18 + ((placement & 1U) != 0 ? 2 : 0)
        + ((placement & 2U) != 0 ? 1 : 0) + ((placement & 4U) != 0 ? 1 : 0));
    auto first = static_cast<std::int16_t>((placement >> 8U) & 15U);
    std::int16_t second = 16;
    if ((placement & 1U) != 0) { std::swap(first, second); }
    if ((placement & 2U) != 0) { first = static_cast<std::int16_t>(-first); }
    if ((placement & 4U) != 0) { second = static_cast<std::int16_t>(-second); }
    const auto defaults = type==44 ? CampaignOrdinaryDefaults{0,0,0,{}} : campaignOrdinaryDefaults(type);
    Level1Entity actor{};
    actor.allocationIndex = free->entityIndex.value_or(input.nextEntityIndex);
    actor.type = type; actor.collisionRadius = radius;
    actor.position = {coordinate(first), coordinate(second), type == 11 ? 1000 : 0};
    actor.forwardVelocity = defaults.speed.value_or(0);
    actor.stateBytes4dTo52 = {255, defaults.health, static_cast<std::uint8_t>(type == 11 ? 5 : 0), 0, defaults.behavior, 0};
    actor.weaponSlotCount = defaults.weaponSlots;
    actor.animatedModelPresenterEnabled = type == 11;
    actor.renderProfile = type == 11 ? WorldActorRenderProfile::heapSelectorDeferredAnimation
        : type == 7 ? WorldActorRenderProfile::inlineSelectorNoCallback : WorldActorRenderProfile::noHandles;
    Operations* suffix = &before;
    if (type == 11) {
        instructions(before, 8 + 14 + 18 + 27 + 7); //4404..4422,137E/CFBC/C05C,3956 through CALLDCC6.
        result.provider = Level1SpawnProviderCall{actor.allocationIndex, selectorBytes, 0xdcc6, false};
        suffix = &result.afterProvider;
        instructions(*suffix, 9); //3962..397E non-null clear setup.
        suffix->push_back({NormalWorkKind::repeatString, selectorBytes});
        instructions(*suffix, 6 + 7 + 1); //3956 return;137E pointer store/return;4425 cleanup.
    } else {
        instructions(*suffix, 5); //4404..4411.
    }
    if(level2Emission) {
        instructions(*suffix,3); //4428..4434, including CALLB39E.
        const auto spin=random(*suffix,result.rng);
        instructions(*suffix,6+((spin&1U)!=0?1:0));
        const auto magnitude=static_cast<std::int16_t>((spin&0x0300U)+0x0700U);
        actor.yawVelocity=(spin&1U)!=0 ? static_cast<std::int16_t>(-magnitude) : magnitude;
        instructions(*suffix,3); //444C..4466.
    } else {
        instructions(*suffix, 6); //4428..4466, no spinner/remap.
    }
    const auto heading = random(*suffix, result.rng);
    //4466 CALLB39E, heading417A, defaults/counters/refresh and4370 return.
    instructions(*suffix, 29 + 9 - (type==44 ? 10 : level2Emission ? 5 : 0));
    actor.yaw = static_cast<std::uint16_t>(static_cast<std::uint16_t>(heading & 0xff0U) << 11U);
    result.outcome = Level1PeriodicSpawnOutcome::spawned;
    result.spawn = Level1PeriodicSpawnReceipt{static_cast<std::uint8_t>(slot), actor, input.allocationCount,
        static_cast<std::int16_t>(input.activeEnemies + 1), static_cast<std::int16_t>(input.spawnedEnemies + 1), true};
    return result;
}
} // namespace detail

} // namespace stellar7
