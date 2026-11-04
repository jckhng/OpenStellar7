#include "campaign_guardian.hpp"
#include "stellar7/simulation_prefix_work.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::size_t noContact=std::numeric_limits<std::size_t>::max();
constexpr std::int16_t guardianType=44;
constexpr std::int16_t deliveryType=7;
constexpr std::int16_t playerShotType=23;
constexpr std::int16_t standardTurnLimit=0x200;
constexpr std::int16_t fastTurnLimit=0x300;

std::int16_t magnitude(const std::int16_t value) noexcept {
    return value<0 ? static_cast<std::int16_t>(-static_cast<int>(value)) : value;
}

std::int16_t negate(const std::int16_t value) noexcept {
    return static_cast<std::int16_t>(-static_cast<int>(value));
}

void steer(Level1Entity& actor,std::int16_t difference,const std::int16_t limit) noexcept {
    if(difference==std::numeric_limits<std::int16_t>::min()) {difference=0x7fff;}
    actor.yawVelocity=static_cast<std::int16_t>(std::clamp<int>(difference,-limit,limit));
    if(actor.position.z<500) {return;}
    const auto combined=static_cast<std::int16_t>(static_cast<std::uint16_t>(actor.yawVelocity*16)+actor.roll);
    const auto opposite=negate(combined);
    const auto adjustment=opposite>=0 ? opposite/16 : -((-static_cast<int>(opposite)+15)/16);
    actor.roll=static_cast<std::uint16_t>(actor.roll+adjustment);
}

bool touching(const Level1Entity& actor) noexcept {
    return actor.collisionContactIsPlayer || actor.collisionContactAllocationIndex!=noContact;
}

void clearContact(Level1Entity& actor) noexcept {
    actor.collisionContactIsPlayer=false;actor.collisionContactAllocationIndex=noContact;
}

void hitContact(Level1Entity& actor,const CampaignGuardianContext& context,
    const CampaignGuardianCallbacks& callbacks,const std::uint8_t damage) {
    if(!context.contact || !callbacks.damageContact) {
        throw std::invalid_argument("guardian contact requires its physical hit child");
    }
    callbacks.damageContact(*context.contact,damage);clearContact(actor);
}

bool attack(const Level1Entity& actor,const CampaignGuardianCallbacks& callbacks,
    const CampaignGuardianAttack kind) {
    if(!callbacks.attack) {throw std::invalid_argument("guardian attack requires its physical allocator child");}
    return callbacks.attack(actor,kind);
}

void level1(Level1Entity& actor,const CampaignGuardianContext& context,
    const CampaignGuardianCallbacks& callbacks) {
    const auto difference=static_cast<std::int16_t>(actor.targetHeading-actor.yaw);
    for(const auto& shot:context.inheritedProjectiles) {
        if(shot.actor.type!=playerShotType || shot.actor.position.z<=2000) {continue;}
        const auto savedAltitude=static_cast<std::int16_t>(actor.position.z);
        actor.position.z=shot.actor.position.z;
        const auto overlaps=actorOverlapOperations(actor,shot.actor).overlaps;
        if(overlaps) {
            if(!callbacks.damageGuardian || !callbacks.retireProjectile) {
                throw std::invalid_argument("Level1 guardian elevated hit requires1176 and481A children");
            }
            callbacks.damageGuardian(actor,actor.stateBytes4dTo52[1]);
            callbacks.retireProjectile(shot.physicalSlot);
        }
        actor.position.z=savedAltitude;
    }
    if(actor.behaviorTimer==0) {
        if(touching(actor) && (!context.contact || context.contact->type!=deliveryType)) {
            hitContact(actor,context,callbacks,20);
        }
        if(static_cast<std::int32_t>(actor.presenterDistance)<=0x2000) {
            actor.forwardVelocity=0;
            actor.renderProfile=WorldActorRenderProfile::heapSelectorNoCallback;
            actor.animatedModelPresenterEnabled=false;
        } else {
            actor.forwardVelocity=200;
            actor.renderProfile=WorldActorRenderProfile::heapSelectorDeferredAnimation;
            actor.animatedModelPresenterEnabled=true;
        }
        if(magnitude(difference)<200) {
            actor.behaviorTimer=20;
            actor.yawVelocity=0;
            if(static_cast<std::int32_t>(actor.presenterDistance)<0x5000) {
                attack(actor,callbacks,CampaignGuardianAttack::aimedProjectile);
            }
        } else {
            steer(actor,difference,512);
        }
    } else {
        --actor.behaviorTimer;
    }
    if(actor.collisionContactIsPlayer) {
        if(!callbacks.changePlayerArmor) {
            throw std::invalid_argument("Level1 guardian requires its armor child");
        }
        callbacks.changePlayerArmor(-100);
    }
}

void level4(Level1Entity& actor,StellarRng& rng,const CampaignGuardianContext& context,
    const CampaignGuardianCallbacks& callbacks) {
    constexpr std::int32_t vulnerableShotAltitude=1500;
    constexpr std::uint8_t elevatedHitDamage=17;
    constexpr std::uint8_t contactDamage=30;
    constexpr std::uint32_t approachDistance=0x1500;
    constexpr std::uint32_t firingDistance=0x5000;
    constexpr std::int16_t alignment=200;
    const auto difference=static_cast<std::int16_t>(actor.targetHeading-actor.yaw);
    for(const auto& shot:context.inheritedProjectiles) {
        if(shot.actor.type!=playerShotType || shot.actor.position.z<=vulnerableShotAltitude) {continue;}
        const auto savedAltitude=static_cast<std::int16_t>(actor.position.z);
        actor.position.z=shot.actor.position.z;
        if(actorOverlapOperations(actor,shot.actor).overlaps) {
            if(!callbacks.damageGuardian || !callbacks.retireProjectile) {
                throw std::invalid_argument("elevated guardian hit requires its damage and retirement children");
            }
            callbacks.damageGuardian(actor,elevatedHitDamage);callbacks.retireProjectile(shot.physicalSlot);
        }
        actor.position.z=savedAltitude;
    }
    if(actor.behaviorTimer!=0) {--actor.behaviorTimer;} else {
        if(touching(actor)) {
            if(!context.contact) {throw std::invalid_argument("Level4 guardian lost its contact identity");}
            if(context.contact->type!=deliveryType) {hitContact(actor,context,callbacks,contactDamage);}
        }
        if(static_cast<std::int32_t>(actor.presenterDistance)>static_cast<std::int32_t>(approachDistance)) {
            actor.forwardVelocity=250;actor.animatedModelPresenterEnabled=true;
            actor.renderProfile=WorldActorRenderProfile::heapSelectorDeferredAnimation;
        }
        if(magnitude(difference)<alignment) {
            actor.behaviorTimer=static_cast<std::uint8_t>(rng.next()&0x1fU);actor.yawVelocity=0;
            if(static_cast<std::int32_t>(actor.presenterDistance)<static_cast<std::int32_t>(firingDistance)) {
                attack(actor,callbacks,CampaignGuardianAttack::aimedProjectile);
            }
        } else {steer(actor,difference,fastTurnLimit);}
    }
    if(actor.collisionContactIsPlayer) {
        if(!callbacks.changePlayerArmor) {throw std::invalid_argument("Level4 guardian requires its armor child");}
        callbacks.changePlayerArmor(-100);actor.modelAnimationVariant=0;actor.presentationModelAnimationVariant=0;
        actor.forwardVelocity=0;actor.animatedModelPresenterEnabled=false;
        actor.renderProfile=WorldActorRenderProfile::heapSelectorNoCallback;
    }
}

void level6(Level1Entity& actor,StellarRng& rng,const CampaignGuardianContext& context,
    const CampaignGuardianCallbacks& callbacks) {
    if(!context.level6Turn) {throw std::invalid_argument("Level6 guardian requires retained DS41B4");}
    const auto choice=rng.next();
    auto& behavior=actor.stateBytes4dTo52[5];
    if(behavior==1) {
        actor.yawVelocity=0;actor.forwardVelocity=0;
        if(!touching(actor)) {attack(actor,callbacks,CampaignGuardianAttack::pulse46);}
        behavior=0;return;
    }
    actor.stateBytes4dTo52[2]=2;actor.forwardVelocity=100;
    steer(actor,*context.level6Turn,standardTurnLimit);actor.type=guardianType;
    if(choice<200) {behavior=1;}
    if(!context.craftPresent && (choice&0x1fU)==0 && !touching(actor)) {
        attack(actor,callbacks,CampaignGuardianAttack::actor48);
    } else {clearContact(actor);}
    const auto amount=static_cast<std::int16_t>(choice&0x1ffU);
    *context.level6Turn=(choice&0x8000U)!=0 ? negate(amount) : amount;
}

void level7(Level1Entity& actor,StellarRng& rng,const CampaignGuardianContext& context,
    const CampaignGuardianCallbacks& callbacks) {
    if(!context.playerProxyPosition) {throw std::invalid_argument("Level7 guardian requires its separate player proxy");}
    const auto difference=static_cast<std::int16_t>(actor.targetHeading-actor.yaw);
    const auto choice=rng.next();*context.playerProxyPosition=context.playerPosition;
    if(touching(actor)) {hitContact(actor,context,callbacks,20);}
    auto& behavior=actor.stateBytes4dTo52[5];
    switch(behavior) {
    case 0:
        steer(actor,difference,standardTurnLimit);actor.forwardVelocity=200;
        if(magnitude(difference)<0x100) {actor.weaponCooldown=48;behavior=1;}
        else if(choice<1000) {behavior=3;}
        if(static_cast<std::int32_t>(actor.presenterDistance)<0x1000) {behavior=2;}
        return;
    case 1:
        steer(actor,difference,fastTurnLimit);
        if((actor.weaponCooldown&15U)==0 && !attack(actor,callbacks,CampaignGuardianAttack::volleyProjectile80)) {
            throw std::invalid_argument("Level7 guardian995E null-shot write is unsupported");
        }
        if(actor.weaponCooldown!=0) {--actor.weaponCooldown;} else {behavior=2;}
        return;
    case 2:
        steer(actor,negate(difference),200);
        if(magnitude(difference)>0x7000) {behavior=0;}
        return;
    case 3:
        steer(actor,difference,fastTurnLimit);actor.forwardVelocity=0;
        if(magnitude(difference)<0x100) {behavior=0;}
        return;
    }
}
} //namespace

void advanceCampaignGuardianBehavior(Level1Entity& actor,const CampaignGuardianFamily family,
    StellarRng& rng,const CampaignGuardianContext& context,const CampaignGuardianCallbacks& callbacks) {
    const auto radius=family==CampaignGuardianFamily::level1 ? 2792U
        : family==CampaignGuardianFamily::level4 ? 2962U
        : family==CampaignGuardianFamily::level6 ? 1701U : family==CampaignGuardianFamily::level7 ? 994U : 0U;
    const auto maximumBehavior=family==CampaignGuardianFamily::level1 || family==CampaignGuardianFamily::level4 ? 255U
        : family==CampaignGuardianFamily::level6 ? 1U : 3U;
    if(radius==0 || actor.type!=guardianType || actor.collisionRadius!=radius
        || actor.stateBytes4dTo52[0]!=255 || actor.stateBytes4dTo52[4]!=5
        || actor.stateBytes4dTo52[5]>maximumBehavior
        || rng.indexA()>=StellarRng::wordCount*2 || rng.indexB()>=StellarRng::wordCount*2
        || rng.indexA()%2!=0 || rng.indexB()%2!=0) {
        throw std::invalid_argument("unsupported campaign guardian source state");
    }
    switch(family) {
    case CampaignGuardianFamily::level1: level1(actor,context,callbacks);return;
    case CampaignGuardianFamily::level4: level4(actor,rng,context,callbacks);return;
    case CampaignGuardianFamily::level6: level6(actor,rng,context,callbacks);return;
    case CampaignGuardianFamily::level7: level7(actor,rng,context,callbacks);return;
    }
}
} //namespace stellar7
