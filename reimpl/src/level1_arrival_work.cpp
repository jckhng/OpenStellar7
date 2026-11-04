#include "stellar7/level1_arrival_work.hpp"
#include "stellar7/simulation_prefix_work.hpp"
#include "level1_ordinary_constructor.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>
namespace stellar7 {
namespace {
constexpr std::uint8_t enemyLimit=10;
constexpr std::int16_t maximumPostWaveSpawned=12;
constexpr std::uint8_t level2EnemyLimit=12;
constexpr std::int16_t level2MaximumPostWaveSpawned=14;
constexpr std::uint16_t allocationCapacity=32;
constexpr std::uint16_t rngIndexLimit=StellarRng::wordCount*2;
void plain(std::vector<NormalWorkOperation>& out,std::uint64_t n) { out.push_back({NormalWorkKind::instructions,n}); }
void validate(const Level1ArrivalInput& input,const StellarRng& rng,
    const detail::OrdinaryConstructorDomain domain=detail::OrdinaryConstructorDomain::level1) {
    const auto& pool=input.pool;
    const auto level2=domain==detail::OrdinaryConstructorDomain::level2PostWave;
    const auto limit=level2 ? level2EnemyLimit : enemyLimit;
    const auto maximum=level2 ? level2MaximumPostWaveSpawned : maximumPostWaveSpawned;
    if((domain!=detail::OrdinaryConstructorDomain::level1 && !level2)
        || input.enemyFlag>1 || pool.enemyLimit!=limit || pool.spawnedEnemies<0 || pool.spawnedEnemies>maximum
        || pool.activeEnemies<0 || pool.activeEnemies>pool.spawnedEnemies || pool.allocationCount>allocationCapacity
        || rng.indexA()>=rngIndexLimit || rng.indexB()>=rngIndexLimit || rng.indexA()%2 || rng.indexB()%2
        || input.player.pitch!=0 || input.player.roll!=0 || input.player.position.z!=0
        || input.player.pitchVelocity!=0 || input.player.rollVelocity!=0) {
        throw std::invalid_argument(level2 ? "unsupported Level2 arrival input" : "unsupported Level1 arrival input");
    }
    std::vector<std::size_t> identities;
    for(const auto& slot:pool.ordinaryPool) {
        if((slot.health!=0 && !slot.entityIndex) || (slot.entityIndex && (*slot.entityIndex>=pool.nextEntityIndex
            || std::find(identities.begin(),identities.end(),*slot.entityIndex)!=identities.end()))) {
            throw std::invalid_argument("invalid arrival ordinary-slot projection");
        }
        if(slot.entityIndex) { identities.push_back(*slot.entityIndex); }
    }
    for(const auto id:{input.craftEntityIndex,input.guardianEntityIndex}) {
        if(id && std::find(identities.begin(),identities.end(),*id)==identities.end()) {
            throw std::invalid_argument("arrival identity lacks ordinary storage");
        }
    }
}
}
Level1PeriodicSpawnWork level1PostWavePeriodicOperations(const Level1PeriodicSpawnInput& input,
    const StellarRng& rng,const std::vector<Model3d>& models) {
    validate({0,{},{},{},input},rng);
    if(input.spawnedEnemies<enemyLimit) { throw std::invalid_argument("post-wave periodic requires reached limit"); }
    return {rng,Level1PeriodicSpawnOutcome::limit,{},{},{{NormalWorkKind::instructions,13}},{},{}};
}
static Level1ArrivalWork arrivalPlan(const Level1ArrivalInput& input,
    const StellarRng& incomingRng,const std::vector<Model3d>& models,
    detail::OrdinaryConstructorProfile* profile) {
    const auto domain=profile ? profile->domain : detail::OrdinaryConstructorDomain::level1;
    validate(input,incomingRng,domain);
    Level1ArrivalWork result{incomingRng,Level1ArrivalOutcome::enemies,{},{}};
    std::vector<NormalWorkOperation> before;
    plain(before,5+2); //46C8..46D5.
    const auto finish=[&](Level1ArrivalOutcome outcome,std::uint64_t branch) {
        plain(before,branch+5); result.outcome=outcome;result.steps.emplace_back(std::move(before));return result;
    };
    if(input.enemyFlag==0) { return finish(Level1ArrivalOutcome::enemies,1); }
    plain(before,2); //46DA/46DF.
    if(input.craftEntityIndex) { return finish(Level1ArrivalOutcome::craftPresent,1); }
    plain(before,2); //46E4/46E9.
    if(input.guardianEntityIndex) { return finish(Level1ArrivalOutcome::guardianPresent,1); }
    plain(before,4); //46EE..46F7.
    if(input.pool.spawnedEnemies<input.pool.enemyLimit) { return finish(Level1ArrivalOutcome::unfinished,0); }
    if(domain==detail::OrdinaryConstructorDomain::level2PostWave && input.pool.spawnedEnemies>level2EnemyLimit) {
        throw std::invalid_argument("Level2 first arrival requires its ordinary-wave total");
    }
    if(input.pool.allocationCount>=allocationCapacity) { throw std::invalid_argument("arrival allocation registry is full"); }
    auto child=detail::ordinaryConstructorOperations(input.pool,incomingRng,models,7,profile);
    if(!child.spawn || child.provider || !child.afterProvider.empty()) {
        throw std::invalid_argument("arrival requires a free ordinary slot without prior selector owner");
    }
    plain(before,3); //46F9/46FB/46FC.
    if(child.selectorRelease) {
        const auto split=child.selectorRelease->operationOffset;
        before.insert(before.end(),child.beforeProvider.begin(),child.beforeProvider.begin()+split);
        result.steps.emplace_back(std::move(before));
        result.steps.emplace_back(child.selectorRelease->call);
        before.assign(child.beforeProvider.begin()+split,child.beforeProvider.end());
    } else {before.insert(before.end(),child.beforeProvider.begin(),child.beforeProvider.end());}
    plain(before,2); //46FF/4702.
    result.steps.emplace_back(std::move(before));
    result.steps.emplace_back(Level1ArrivalAudioChild{}); //4705..470C owned by timed audio.
    std::vector<NormalWorkOperation> after;
    plain(after,11+6); //470F..472C;4731..473F copy setup.
    after.push_back({NormalWorkKind::repeatString,6});
    plain(after,3); //4744..474B.
    for(unsigned n=0;n<3;++n) { after.push_back({NormalWorkKind::string,1}); }
    plain(after,5); //4751..4761 includingCALL3B3A.
    auto actor=child.spawn->actor;
    actor.position=input.player.position;actor.position.z=5000;
    actor.pitch=input.player.pitch;actor.roll=input.player.roll;actor.yaw=input.player.yaw;
    actor.stateBytes4dTo52[1]=100;actor.stateBytes4dTo52[2]=0;
    actor.stateBytes4dTo52[4]=5;actor.stateBytes4dTo52[5]=0;actor.forwardVelocity=static_cast<std::int16_t>(0x9000);
    const auto motion=flatKinematicsOperations(actor);actor=motion.actor;
    after.insert(after.end(),motion.operations.begin(),motion.operations.end());
    plain(after,4+5); //4764..476F and4773..4778.
    actor.forwardVelocity=400;
    result.rng=child.rng;result.outcome=Level1ArrivalOutcome::born;result.birth=child.spawn;
    result.birth->actor=actor;result.steps.emplace_back(std::move(after));
    return result;
}
Level1ArrivalWork level1ArrivalOperations(const Level1ArrivalInput& input,
    const StellarRng& rng,const std::vector<Model3d>& models) {
    if(input.pool.selectorReuse!=Level1SelectorReuseDomain::rejectAnimated || input.pool.retainedEntities) {
        throw std::invalid_argument("arrival requires its strict selector domain");
    }
    return arrivalPlan(input,rng,models,nullptr);
}
Level1ArrivalWork level1RetainedArrivalOperations(const Level1ArrivalInput& input,
    const StellarRng& rng,const std::vector<Model3d>& models) {
    validateLevel1SelectorProjection(input.pool);
    detail::OrdinaryConstructorProfile profile{false,input.pool.retainedEntities,{}};
    return arrivalPlan(input,rng,models,&profile);
}
Level1ArrivalWork level2ArrivalOperations(const Level1ArrivalInput& input,
    const StellarRng& rng,const std::vector<Model3d>& models) {
    if(input.pool.selectorReuse!=Level1SelectorReuseDomain::rejectAnimated || input.pool.retainedEntities) {
        throw std::invalid_argument("Level2 first arrival requires its strict selector domain");
    }
    detail::OrdinaryConstructorProfile profile{};
    profile.domain=detail::OrdinaryConstructorDomain::level2PostWave;
    return arrivalPlan(input,rng,models,&profile);
}
} // namespace stellar7
