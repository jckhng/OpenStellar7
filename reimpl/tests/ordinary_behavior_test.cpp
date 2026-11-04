#include "ordinary_behavior.hpp"
#include "hazard_contact.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace {

using stellar7::Level1Entity;
using stellar7::StellarRng;

StellarRng rngFor(std::initializer_list<std::uint16_t> draws) {
    std::array<std::uint16_t, StellarRng::wordCount> table{};
    std::size_t index = 0;
    for (const auto value : draws) {
        assert(index < 24);
        table[55 - index] = value;
        ++index;
    }
    return {table, 0x30, 0x6e, 0, 0};
}

Level1Entity type5(std::uint8_t behavior) {
    Level1Entity actor{};
    actor.type = 5;
    actor.stateBytes4dTo52[5] = behavior;
    actor.presenterDistance = 0x6000;
    return actor;
}

void initialChoiceAndCruise() {
    auto actor = type5(0);
    auto rng = rngFor({0, 0x0170});
    int fires = 0;
    stellar7::advanceType5Behavior(actor, rng, 5, [&](Level1Entity&) {
        ++fires;
        return true;
    });
    assert(rng.calls() == 2 && fires == 0);
    assert(actor.stateBytes4dTo52[5] == 1 && actor.forwardVelocity == 135);
    assert(actor.yawVelocity == -368 && actor.behaviorTimer == 1);

    actor.behaviorTimer = 1;
    stellar7::advanceType5Behavior(actor, rng, 5, [&](Level1Entity& current) {
        ++fires;
        current.weaponCooldown = 12;
        current.behaviorTimer = 1;
        return true;
    });
    assert(rng.calls() == 2 && fires == 1);
    assert(actor.stateBytes4dTo52[5] == 0 && actor.behaviorTimer == 1);
    assert(actor.weaponCooldown == 12);

    for (const std::uint16_t choice : {50, 120}) {
        auto selected = type5(0);
        auto selectionRng = rngFor({choice, 8});
        int selectedFires = 0;
        stellar7::advanceType5Behavior(selected, selectionRng, 5,
            [&](Level1Entity&) {
                ++selectedFires;
                assert(selectionRng.calls() == 1);
                return false;
            });
        assert(selectionRng.calls() == 2 && selectedFires == 1);
        assert(selected.stateBytes4dTo52[5] == (choice == 50 ? 2 : 3));
    }
}

void aimingAndFireOrder() {
    auto actor = type5(2);
    actor.targetHeading = static_cast<std::uint16_t>(-400);
    auto rng = rngFor({8});
    int fires = 0;
    stellar7::advanceType5Behavior(actor, rng, 5, [&](Level1Entity& current) {
        ++fires;
        assert(rng.calls() == 0);
        assert(current.forwardVelocity == 0 && current.yawVelocity == -271);
        return false;
    });
    assert(fires == 1 && rng.calls() == 1);
    assert(actor.stateBytes4dTo52[5] == 2);

    actor.presenterDistance = 0x6001;
    rng = rngFor({8});
    stellar7::advanceType5Behavior(actor, rng, 5, [&](Level1Entity&) {
        ++fires;
        return true;
    });
    assert(fires == 1 && rng.calls() == 1);
    assert(actor.stateBytes4dTo52[5] == 0);
}

void levelDependentStateThreeReset() {
    for (const std::uint16_t draw : {4, 7}) {
        for (const std::uint16_t level : {4, 5}) {
            auto actor = type5(3);
            actor.targetHeading = 400;
            auto rng = rngFor({draw});
            int fires = 0;
            stellar7::advanceType5Behavior(actor, rng, level, [&](Level1Entity& current) {
                ++fires;
                assert(rng.calls() == 0);
                assert(current.yawVelocity == 271);
                return false;
            });
            assert(fires == 1 && rng.calls() == 1);
            assert(actor.stateBytes4dTo52[5] == (level == 5 ? 0 : 3));
        }
    }
    for (const std::uint16_t draw : {3, 8}) {
        auto actor = type5(3);
        auto rng = rngFor({draw});
        stellar7::advanceType5Behavior(actor, rng, 5, [](Level1Entity&) { return false; });
        assert(actor.stateBytes4dTo52[5] == (draw == 3 ? 0 : 3));
    }
}

void contactAndRecovery() {
    auto actor = type5(4);
    actor.collisionContactAllocationIndex = 9;
    actor.collisionContactIsPlayer = true;
    actor.forwardVelocity = 5;
    actor.behaviorTimer = 0;
    auto rng = rngFor({0x0200});
    int fires = 0;
    stellar7::advanceType5Behavior(actor, rng, 5, [&](Level1Entity&) {
        ++fires;
        return false;
    });
    assert(rng.calls() == 1 && fires == 0);
    assert(actor.collisionContactAllocationIndex == static_cast<std::size_t>(-1));
    assert(!actor.collisionContactIsPlayer);
    assert(actor.forwardVelocity == -67 && actor.yawVelocity == -512);
    assert(actor.behaviorTimer == 0x3f && actor.stateBytes4dTo52[5] == 4);

    actor.behaviorTimer = 1;
    stellar7::advanceType5Behavior(actor, rng, 5, [&](Level1Entity&) {
        ++fires;
        return false;
    });
    assert(rng.calls() == 1 && fires == 1);
    assert(actor.behaviorTimer == 0 && actor.forwardVelocity == 135);
    assert(actor.stateBytes4dTo52[5] == 0);

    actor.stateBytes4dTo52[5] = 5;
    rng = rngFor({0x0020});
    stellar7::advanceType5Behavior(actor, rng, 5, [&](Level1Entity&) {
        ++fires;
        return false;
    });
    assert(rng.calls() == 1 && fires == 1);
    assert(actor.stateBytes4dTo52[5] == 5 && actor.forwardVelocity == 135);
    assert(actor.yawVelocity == 0 && actor.behaviorTimer == 0);
}

void type8RangeAndHeading() {
    Level1Entity actor{};
    actor.type = 8;
    actor.targetHeading = 400;
    actor.presenterDistance = 0x6000;
    int fires = 0;
    const auto fire = [&](Level1Entity&) {
        ++fires;
        return false;
    };
    stellar7::advanceType8Behavior(actor, fire);
    assert(fires == 1 && actor.yawVelocity == 143);

    actor.presenterDistance = 0x6001;
    actor.targetHeading = static_cast<std::uint16_t>(-400);
    stellar7::advanceType8Behavior(actor, fire);
    assert(fires == 1 && actor.yawVelocity == -143);

    actor.position.z = -1;
    stellar7::advanceType8Behavior(actor, fire);
    assert(fires == 2 && actor.yawVelocity == -143);
}

void type9RangeBeforeAimAndCooldown() {
    Level1Entity actor{};
    actor.type=9;actor.targetHeading=400;actor.yawVelocity=91;
    actor.presenterDistance=0x6001;actor.weaponCooldown=2;
    auto rng=rngFor({0x1234});
    int fires=0;
    const auto fire=[&](Level1Entity& current) {
        ++fires;
        assert(current.yawVelocity==143 && rng.calls()==0);
        if(current.weaponCooldown!=0) {--current.weaponCooldown;return false;}
        rng.next();return true;
    };
    stellar7::advanceType9Behavior(actor,fire);
    assert(fires==0 && actor.yawVelocity==91 && actor.weaponCooldown==2 && rng.calls()==0);

    actor.presenterDistance=0x6000;
    stellar7::advanceType9Behavior(actor,fire);
    assert(fires==1 && actor.yawVelocity==143 && actor.weaponCooldown==1 && rng.calls()==0);
    stellar7::advanceType9Behavior(actor,fire);
    assert(fires==2 && actor.weaponCooldown==0 && rng.calls()==0);
    stellar7::advanceType9Behavior(actor,fire);
    assert(fires==3 && rng.calls()==1);
}

void type9SignedDistanceAndHeadingWrap() {
    Level1Entity actor{};
    actor.type=9;actor.presenterDistance=0xffffffffU;
    actor.yaw=65530;actor.targetHeading=20;
    int fires=0;
    stellar7::advanceType9Behavior(actor,[&](Level1Entity& current) {
        ++fires;assert(current.yawVelocity==26);return false;
    });
    assert(fires==1 && actor.yawVelocity==26);

    actor.presenterDistance=0x6001;actor.position.z=-1;actor.yawVelocity=77;
    stellar7::advanceType9Behavior(actor,[&](Level1Entity&) {++fires;return false;});
    assert(fires==1 && actor.yawVelocity==77);

    actor.presenterDistance=0;actor.yaw=0;actor.targetHeading=static_cast<std::uint16_t>(-400);
    stellar7::advanceType9Behavior(actor,[&](Level1Entity& current) {
        ++fires;assert(current.yawVelocity==-143);return false;
    });
    assert(fires==2 && actor.yawVelocity==-143);
}

void type6FreshChoiceAndCloak() {
    using namespace stellar7;
    Level1Entity actor{};actor.type=6;actor.flags=4;actor.presenterDistance=0x6000;
    std::uint16_t cloakTimer=0;
    auto rng=rngFor({0,0x0170});
    int fires=0;
    const auto fire=[&](Level1Entity&) {++fires;assert(cloakTimer==199 && rng.calls()==2);return false;};
    advanceType6Behavior(actor,rng,{cloakTimer,5,Type6ScannerState::inactive},fire);
    assert(rng.calls()==2 && fires==0 && cloakTimer==200 && actor.flags==4);
    assert(actor.stateBytes4dTo52[5]==1 && actor.forwardVelocity==180 && actor.yawVelocity==-624);
    assert(actor.behaviorTimer==1 && actor.type12MissGate==2);
    assert(actor.renderProfile==WorldActorRenderProfile::inlineSelectorNoCallback && !actor.animatedModelPresenterEnabled);
    advanceType6Behavior(actor,rng,{cloakTimer,5,Type6ScannerState::inactive},fire);
    assert(fires==1 && cloakTimer==199 && actor.flags==0x84 && actor.stateBytes4dTo52[5]==0);
}

void type6CloakAndScanner() {
    using namespace stellar7;
    for(const auto initial:{0,200,196,195,5,4,1,65535}) {
        Level1Entity actor{};actor.type=6;actor.flags=4;actor.presenterDistance=0x6001;
        actor.stateBytes4dTo52[5]=1;actor.behaviorTimer=255;actor.stateBytes4dTo52[2]=1;
        auto rng=rngFor({});auto timer=static_cast<std::uint16_t>(initial);
        int fires=0;
        advanceType6Behavior(actor,rng,{timer,5,Type6ScannerState::inactive},[&](Level1Entity&) {++fires;return false;});
        const auto blinks=initial!=0 && (initial>195 && initial<32768 || initial<5 || initial>=32768);
        assert(timer==(initial==0?200:static_cast<std::uint16_t>(initial-1)) && rng.calls()==0 && fires==0);
        assert(actor.flags==(blinks?0x84:4) && actor.stateBytes4dTo52[2]==(blinks?0:1));
    }
    Level1Entity actor{};actor.type=6;actor.flags=0x84;actor.presenterDistance=0x6001;
    actor.stateBytes4dTo52[5]=1;actor.behaviorTimer=255;
    std::uint16_t timer=100;auto rng=rngFor({});
    applyType6Scanner(actor,Type6ScannerState::active);
    assert(actor.flags==4 && actor.stateBytes4dTo52[2]==1 && timer==100);
    advanceType6Behavior(actor,rng,{timer,5,Type6ScannerState::active},[](Level1Entity&) {return false;});
    assert(timer==99 && actor.stateBytes4dTo52[2]==1);
    timer=200;
    advanceType6Behavior(actor,rng,{timer,5,Type6ScannerState::active},[](Level1Entity&) {return false;});
    assert(timer==199 && actor.flags==4 && actor.stateBytes4dTo52[2]==0);
    applyType6Scanner(actor,Type6ScannerState::inactive);
    assert(actor.flags==0x84 && actor.stateBytes4dTo52[2]==0);
    actor.type=5;actor.flags=4;actor.stateBytes4dTo52[2]=7;
    applyType6Scanner(actor,Type6ScannerState::active);
    assert(actor.flags==4 && actor.stateBytes4dTo52[2]==7);
}

void type6TankStatesAndRngOrder() {
    using namespace stellar7;
    for(const auto level:{4,5,6}) {
        Level1Entity actor{};actor.type=6;actor.stateBytes4dTo52[5]=3;
        actor.presenterDistance=0x6000;actor.targetHeading=500;
        std::uint16_t timer=100;auto rng=rngFor({5});int fires=0;
        advanceType6Behavior(actor,rng,{timer,static_cast<std::uint8_t>(level),Type6ScannerState::inactive},[&](Level1Entity& current) {
            ++fires;assert(rng.calls()==0 && timer==99 && current.yawVelocity==399);return false;
        });
        assert(fires==1 && rng.calls()==1 && actor.stateBytes4dTo52[5]==(level>=5?0:3));
    }
    Level1Entity actor{};actor.type=6;actor.stateBytes4dTo52[5]=4;actor.forwardVelocity=1;
    actor.collisionContactIsPlayer=true;actor.collisionContactAllocationIndex=9;
    std::uint16_t timer=100;auto rng=rngFor({0x0200});int fires=0;
    advanceType6Behavior(actor,rng,{timer,5,Type6ScannerState::inactive},[&](Level1Entity&) {++fires;return false;});
    assert(actor.forwardVelocity==-81 && actor.yawVelocity==-768 && actor.behaviorTimer==63);
    assert(!actor.collisionContactIsPlayer && actor.collisionContactAllocationIndex==static_cast<std::size_t>(-1));
    assert(rng.calls()==1 && fires==0);
    actor.behaviorTimer=1;
    advanceType6Behavior(actor,rng,{timer,5,Type6ScannerState::inactive},[&](Level1Entity&) {++fires;return false;});
    assert(actor.forwardVelocity==180 && actor.stateBytes4dTo52[5]==0 && fires==1 && rng.calls()==1);
}

void type6ChoiceBoundariesAndStoppedAttack() {
    using namespace stellar7;
    for(const auto choice:{47,48,127,128,255}) {
        Level1Entity actor{};actor.type=6;actor.presenterDistance=0x6000;
        std::uint16_t timer=0;auto rng=rngFor({static_cast<std::uint16_t>(choice),8});int fires=0;
        advanceType6Behavior(actor,rng,{timer,5,Type6ScannerState::inactive},[&](Level1Entity&) {
            ++fires;assert(rng.calls()==1 && timer==200);return false;
        });
        assert(rng.calls()==2 && actor.stateBytes4dTo52[5]==(choice<48?1:choice<128?2:3));
        assert(fires==(choice<48?0:1));
    }
    Level1Entity actor{};actor.type=6;actor.stateBytes4dTo52[5]=2;actor.targetHeading=500;
    actor.presenterDistance=0x6001;actor.forwardVelocity=180;
    std::uint16_t timer=100;auto rng=rngFor({8});int fires=0;
    advanceType6Behavior(actor,rng,{timer,5,Type6ScannerState::inactive},[&](Level1Entity&) {++fires;return false;});
    assert(actor.forwardVelocity==0 && actor.yawVelocity==399 && actor.stateBytes4dTo52[5]==0);
    assert(fires==0 && rng.calls()==1);

    actor.stateBytes4dTo52[5]=5;rng=rngFor({0x8170});
    advanceType6Behavior(actor,rng,{timer,5,Type6ScannerState::inactive},[](Level1Entity&) {return false;});
    assert(actor.forwardVelocity==180 && actor.yawVelocity==-112 && actor.behaviorTimer==1);
    assert(rng.calls()==1 && actor.stateBytes4dTo52[5]==5);
}

void hazardContactOrder() {
    for(const auto type:{13,14,15,16,17}) {
        Level1Entity actor{};actor.type=static_cast<std::int16_t>(type);actor.stateBytes4dTo52[1]=1;
        std::int16_t active=3;std::vector<int> order;
        const auto handled=stellar7::retireContactHazard(actor,active,[&] {
            assert(actor.stateBytes4dTo52[1]==0 && active==3);order.push_back(1);
        },[&](const std::int16_t damage) {
            assert(active==2 && damage==50);order.push_back(2);
        });
        const auto hazard=type>=14 && type<=16;
        assert(handled==hazard && active==(hazard?2:3));
        assert(order==(hazard?std::vector<int>{1,2}:std::vector<int>{}));
        assert(actor.stateBytes4dTo52[1]==(hazard?0:1));
    }
}

}  // namespace

int main() {
    initialChoiceAndCruise();
    aimingAndFireOrder();
    levelDependentStateThreeReset();
    contactAndRecovery();
    type8RangeAndHeading();
    type9RangeBeforeAimAndCooldown();
    type9SignedDistanceAndHeadingWrap();
    type6FreshChoiceAndCloak();
    type6CloakAndScanner();
    type6TankStatesAndRngOrder();
    type6ChoiceBoundariesAndStoppedAttack();
    hazardContactOrder();
}
