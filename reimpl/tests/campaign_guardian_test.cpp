#include "../src/campaign_guardian.hpp"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace {
using namespace stellar7;

StellarRng random(const std::uint16_t choice) {
    std::array<std::uint16_t,StellarRng::wordCount> words{};words[55]=choice;
    return StellarRng(words,48,110,0,0);
}

Level1Entity guardian(const CampaignGuardianFamily family) {
    Level1Entity actor{};actor.type=44;
    actor.collisionRadius=family==CampaignGuardianFamily::level1 ? 2792 : family==CampaignGuardianFamily::level4 ? 2962
        : family==CampaignGuardianFamily::level6 ? 1701 : 994;
    actor.stateBytes4dTo52={255,40,2,0,5,0};actor.presenterDistance=6000;
    actor.renderProfile=WorldActorRenderProfile::heapSelectorDeferredAnimation;
    return actor;
}

struct Children {
    std::vector<CampaignGuardianAttack> attacks;
    std::vector<unsigned> damage;
    std::vector<std::size_t> retired;
    std::vector<std::int16_t> armor;
    bool allocationSucceeds=true;
    CampaignGuardianCallbacks callbacks() {
        return {
            [this](Level1Entity& actor,const std::uint8_t amount) {
                damage.push_back(amount);actor.stateBytes4dTo52[1]-=amount;
            },
            [this](const std::size_t slot) {retired.push_back(slot);},
            [this](const CampaignGuardianContact&,const std::uint8_t amount) {damage.push_back(amount);},
            [this](const std::int16_t delta) {armor.push_back(delta);},
            [this](const Level1Entity&,const CampaignGuardianAttack kind) {attacks.push_back(kind);return allocationSucceeds;}
        };
    }
};

void level1SourceBranches() {
    Children children;CampaignGuardianContext context{};auto rng=random(999);
    auto actor=guardian(CampaignGuardianFamily::level1);
    actor.presenterDistance=0x2000;actor.targetHeading=200;
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level1,rng,context,children.callbacks());
    assert(actor.forwardVelocity==0 && actor.yawVelocity==200 && !actor.animatedModelPresenterEnabled);
    assert(actor.renderProfile==WorldActorRenderProfile::heapSelectorNoCallback && actor.behaviorTimer==0);
    actor.presenterDistance=0x2001;
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level1,rng,context,children.callbacks());
    assert(actor.forwardVelocity==200 && actor.animatedModelPresenterEnabled && rng.calls()==0);
    actor.targetHeading=0;actor.presenterDistance=0x4fff;
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level1,rng,context,children.callbacks());
    assert(actor.behaviorTimer==20 && actor.yawVelocity==0);
    assert(children.attacks==std::vector<CampaignGuardianAttack>{CampaignGuardianAttack::aimedProjectile});
    actor.behaviorTimer=0;actor.presenterDistance=0x5000;
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level1,rng,context,children.callbacks());
    assert(actor.behaviorTimer==20 && children.attacks.size()==1 && rng.calls()==0);
    actor.behaviorTimer=0;actor.targetHeading=0x8000;
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level1,rng,context,children.callbacks());
    assert(actor.behaviorTimer==20 && actor.yawVelocity==0); //Signed ABS wraps.
}

void level1ElevatedHitAndContact() {
    Children children;CampaignGuardianContext context{};auto rng=random(0);
    auto actor=guardian(CampaignGuardianFamily::level1);
    actor.stateBytes4dTo52[1]=15;actor.behaviorTimer=2;actor.position.z=32768;
    Level1Entity shot{};shot.type=23;shot.collisionRadius=134;shot.position.z=2000;
    context.inheritedProjectiles={{2,shot}};
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level1,rng,context,children.callbacks());
    assert(children.damage.empty() && actor.position.z==32768);
    actor.behaviorTimer=2;
    context.inheritedProjectiles.front().actor.position.z=2001;
    context.inheritedProjectiles.push_back({3,context.inheritedProjectiles.front().actor});
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level1,rng,context,children.callbacks());
    assert(children.damage==std::vector<unsigned>({15,0}));
    assert(children.retired==std::vector<std::size_t>({2,3}) && actor.position.z==-32768);
    context.inheritedProjectiles.clear();actor.behaviorTimer=1;actor.collisionContactIsPlayer=true;
    actor.modelAnimationVariant=3;actor.forwardVelocity=77;context.contact=CampaignGuardianContact{0,0,true};
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level1,rng,context,children.callbacks());
    assert(actor.behaviorTimer==0 && actor.collisionContactIsPlayer && actor.forwardVelocity==77);
    assert(actor.modelAnimationVariant==3 && children.armor==std::vector<std::int16_t>{-100});
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level1,rng,context,children.callbacks());
    assert(!actor.collisionContactIsPlayer && children.damage.back()==20 && children.armor.size()==1);
    assert(rng.calls()==0);
}

void level4AimAndCooldown() {
    Children children;CampaignGuardianContext context{};auto rng=random(31);
    auto actor=guardian(CampaignGuardianFamily::level4);
    actor.presenterDistance=0x5000;actor.yawVelocity=55;
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level4,rng,context,children.callbacks());
    assert(actor.forwardVelocity==250 && actor.yawVelocity==0 && actor.behaviorTimer==31);
    assert(actor.animatedModelPresenterEnabled && children.attacks.empty() && rng.calls()==1);
    actor.behaviorTimer=0;actor.presenterDistance=0x4fff;rng=random(0);
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level4,rng,context,children.callbacks());
    assert(children.attacks==std::vector<CampaignGuardianAttack>{CampaignGuardianAttack::aimedProjectile});
    assert(actor.behaviorTimer==0);
    actor.presenterDistance=0x1500;actor.forwardVelocity=77;actor.targetHeading=200;rng=random(0);
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level4,rng,context,children.callbacks());
    assert(actor.forwardVelocity==77 && actor.yawVelocity==200 && rng.calls()==0);
    actor.targetHeading=0x8000;rng=random(0);
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level4,rng,context,children.callbacks());
    assert(actor.yawVelocity==0 && rng.calls()==1); //Source signed ABS wraps at8000.
    actor.targetHeading=1000;actor.position.z=500;actor.roll=0;actor.behaviorTimer=0;
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level4,rng,context,children.callbacks());
    assert(actor.yawVelocity==768 && actor.roll==static_cast<std::uint16_t>(-768));
    actor.position.z=499;actor.roll=77;
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level4,rng,context,children.callbacks());
    assert(actor.roll==77);
}

void level4ElevatedHitAndContact() {
    Children children;CampaignGuardianContext context{};auto rng=random(0);
    auto actor=guardian(CampaignGuardianFamily::level4);actor.behaviorTimer=2;actor.position.z=123;
    Level1Entity shot{};shot.type=23;shot.collisionRadius=134;shot.position.z=1500;
    context.inheritedProjectiles={{4,shot}};
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level4,rng,context,children.callbacks());
    assert(children.damage.empty() && children.retired.empty() && actor.position.z==123 && rng.calls()==0);
    context.inheritedProjectiles.front().actor.position.z=1501;
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level4,rng,context,children.callbacks());
    assert(children.damage==std::vector<unsigned>{17} && children.retired==std::vector<std::size_t>{4});
    assert(actor.stateBytes4dTo52[1]==23 && actor.position.z==123);
    actor.position.z=32768;actor.behaviorTimer=1;
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level4,rng,context,children.callbacks());
    assert(actor.position.z==-32768 && children.damage.size()==2 && children.retired.size()==2);
    context.inheritedProjectiles.clear();actor.behaviorTimer=1;actor.collisionContactIsPlayer=true;
    context.contact=CampaignGuardianContact{0,0,true};actor.forwardVelocity=250;actor.modelAnimationVariant=3;
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level4,rng,context,children.callbacks());
    assert(children.armor==std::vector<std::int16_t>{-100} && actor.forwardVelocity==0);
    assert(actor.modelAnimationVariant==0 && !actor.animatedModelPresenterEnabled && actor.collisionContactIsPlayer);
    actor.behaviorTimer=0;actor.targetHeading=1000;
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level4,rng,context,children.callbacks());
    assert(children.damage==std::vector<unsigned>({17,17,30}) && children.armor.size()==1);
    assert(!actor.collisionContactIsPlayer && actor.yawVelocity==768);
}

void level6TurnPulseAndEmission() {
    Children children;std::int16_t turn=123;CampaignGuardianContext context{};context.level6Turn=&turn;
    auto actor=guardian(CampaignGuardianFamily::level6);auto rng=random(0x8020);
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level6,rng,context,children.callbacks());
    assert(actor.yawVelocity==123 && actor.forwardVelocity==100 && actor.stateBytes4dTo52[2]==2);
    assert(turn==-32 && rng.calls()==1 && children.attacks==std::vector<CampaignGuardianAttack>{CampaignGuardianAttack::actor48});
    rng=random(64);
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level6,rng,context,children.callbacks());
    assert(actor.stateBytes4dTo52[5]==1 && actor.yawVelocity==-32 && turn==64);
    const auto calls=children.attacks.size();rng=random(99);
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level6,rng,context,children.callbacks());
    assert(actor.stateBytes4dTo52[5]==0 && actor.forwardVelocity==0 && actor.yawVelocity==0);
    assert(children.attacks.size()==calls+1 && children.attacks.back()==CampaignGuardianAttack::pulse46 && turn==64 && rng.calls()==1);
    actor.stateBytes4dTo52[5]=1;actor.collisionContactAllocationIndex=9;
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level6,rng,context,children.callbacks());
    assert(actor.collisionContactAllocationIndex==9 && children.attacks.size()==calls+1);
    actor.collisionContactAllocationIndex=static_cast<std::size_t>(-1);context.craftPresent=true;rng=random(64);
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level6,rng,context,children.callbacks());
    assert(children.attacks.size()==calls+1 && turn==64);
}

void level7VolleyAndProxy() {
    Children children;FixedPosition proxy{1,2,3};CampaignGuardianContext context{};
    context.playerPosition={100,200,300};context.playerProxyPosition=&proxy;
    auto actor=guardian(CampaignGuardianFamily::level7);auto rng=random(1000);
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level7,rng,context,children.callbacks());
    assert(proxy==context.playerPosition && actor.stateBytes4dTo52[5]==1 && actor.weaponCooldown==48 && actor.forwardVelocity==200);
    unsigned frames=0;
    while(actor.stateBytes4dTo52[5]==1) {
        advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level7,rng,context,children.callbacks());++frames;
    }
    assert(frames==49 && actor.stateBytes4dTo52[5]==2 && rng.calls()==50);
    assert(children.attacks==std::vector<CampaignGuardianAttack>(4,CampaignGuardianAttack::volleyProjectile80));
    actor.targetHeading=0x7000;
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level7,rng,context,children.callbacks());
    assert(actor.stateBytes4dTo52[5]==2 && actor.yawVelocity==-200 && actor.forwardVelocity==200);
    actor.targetHeading=0x7001;
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level7,rng,context,children.callbacks());
    assert(actor.stateBytes4dTo52[5]==0);
    actor.targetHeading=1000;rng=random(999);
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level7,rng,context,children.callbacks());
    assert(actor.stateBytes4dTo52[5]==3);
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level7,rng,context,children.callbacks());
    assert(actor.forwardVelocity==0 && actor.yawVelocity==768);
    actor.targetHeading=255;
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level7,rng,context,children.callbacks());
    assert(actor.stateBytes4dTo52[5]==0);
    actor.presenterDistance=4095;actor.collisionContactIsPlayer=true;context.contact=CampaignGuardianContact{0,0,true};
    advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level7,rng,context,children.callbacks());
    assert(children.damage==std::vector<unsigned>{20} && !actor.collisionContactIsPlayer && actor.stateBytes4dTo52[5]==2);
    actor.stateBytes4dTo52[5]=1;actor.weaponCooldown=0;children.allocationSucceeds=false;
    bool rejected=false;
    try {advanceCampaignGuardianBehavior(actor,CampaignGuardianFamily::level7,rng,context,children.callbacks());}
    catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
}
} //namespace

int main() {
    level1SourceBranches();
    level1ElevatedHitAndContact();
    level4AimAndCooldown();level4ElevatedHitAndContact();level6TurnPulseAndEmission();level7VolleyAndProxy();
}
