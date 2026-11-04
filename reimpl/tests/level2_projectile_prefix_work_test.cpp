#include "stellar7/level2_projectile_prefix_work.hpp"
#include "level2_control_prefix_fixture.hpp"
#include "score_work_fixture.inc"
#include <tuple>
#include <cstdio>
#include <cstdlib>

using namespace stellar7;
namespace {
using Op = std::tuple<NormalWorkKind,std::uint64_t,std::uint32_t>;
std::vector<Op> canonical(const std::vector<NormalWorkOperation>& operations) {
    std::vector<Op> result;
    for (const auto& op : operations) {
        if (!result.empty() && op.kind == NormalWorkKind::instructions
            && std::get<0>(result.back()) == op.kind) {
            std::get<1>(result.back()) += op.count;
        } else { result.emplace_back(op.kind,op.count,op.value); }
    }
    return result;
}
Level1TargetPrefixInput input() {
    const auto ordinary = fixture();
    Level1TargetPrefixInput result{};
    auto& prefix = result.prefix;
    prefix.player = ordinary.player;
    prefix.entities = ordinary.actors;
    prefix.active = true;
    prefix.lastListCamera = ordinary.lastListCamera;
    for (std::size_t index = 0; index < prefix.entities.size(); ++index) {
        prefix.entities[index].allocationIndex = index;
        prefix.allocationOrder.push_back({Level1PresentationKind::entity,index});
    }
    for (const auto index : ordinary.activeOrder) {
        prefix.activeOrder.push_back({Level1PresentationKind::entity,index});
    }
    for (std::size_t slot = 0; slot < prefix.projectiles.size(); ++slot) {
        prefix.projectiles[slot].poolSlot = slot;
        prefix.projectiles[slot].stateBytes4dTo54[0] = 253;
    }
    result.activeEnemies = 5;
    return result;
}
void shot(Level1TargetPrefixInput& input, const std::uint8_t life, const bool listed) {
    auto& prefix = input.prefix;
    auto& value = prefix.projectiles[0];
    value.type = 23; value.collisionRadius = 134;
    value.position = {-1000,-8000,0}; value.forwardVelocity = 500;
    value.ownerIsPlayer = true;
    value.stateBytes4dTo54 = {253,life,0,27,0,41,61,62};
    prefix.allocationOrder.push_back({Level1PresentationKind::projectile,0});
    if (listed) { prefix.activeOrder.push_back({Level1PresentationKind::projectile,0}); }
    else { prefix.refreshRequested = true; }
}
bool contains(const std::vector<Level1ObjectRef>& list, const Level1ObjectRef ref) {
    return std::find(list.begin(),list.end(),ref) != list.end();
}
void ordinaryEquivalence(const BitmapFont& scoreFont, const CockpitPageState& pages) {
    for (const auto velocity : {0,80,-512,512}) {
        auto ordinary = fixture();
        ordinary.player.yaw = 0x1300;
        ordinary.player.yawVelocity = static_cast<std::int16_t>(velocity);
        ordinary.player.forwardVelocity = 30;
        auto before = input();
        before.prefix.player = ordinary.player;
        const auto expected = controlledLevel2PrefixOperations(ordinary);
        const auto work = level2ProjectilePrefixOperations(before,{199,680,0},scoreFont,pages);
        const auto& actual = work.damage.combat.prefix;
        assert(canonical(actual.operations) == canonical(expected.operations));
        assert(fields(actual.player) == fields(expected.player));
        assert(actual.activeOrder.size() == expected.activeOrder.size());
        for (std::size_t index = 0; index < actual.entities.size(); ++index) {
            assert(fields(actual.entities[index]) == fields(expected.actors[index]));
            assert(actual.entities[index].stateBytes4dTo52 == expected.actors[index].stateBytes4dTo52);
            assert(actual.activeOrder[index].index == expected.activeOrder[index]);
        }
        assert(work.scoreSteps.empty() && work.targetHits.empty() && work.damage.combat.audioEvents.empty());
    }
}
void updateAndRetire(const BitmapFont& scoreFont, const CockpitPageState& pages) {
    auto pending = input(); shot(pending,64,false);
    const auto born = level2ProjectilePrefixOperations(pending,{16,680,0},scoreFont,pages);
    const auto& birth = born.damage.combat.prefix;
    assert(birth.projectiles[0].stateBytes4dTo54[1] == 64);
    assert(birth.projectiles[0].position == pending.prefix.projectiles[0].position);
    assert(contains(birth.activeOrder,{Level1PresentationKind::projectile,0}));
    assert(!birth.refreshRequested && birth.lastListCamera == birth.player.position);
    for (const auto life : {64,63,2,1}) {
        auto before = input(); shot(before,static_cast<std::uint8_t>(life),true);
        auto& incoming = before.prefix.projectiles[0];
        incoming.flags = 4; incoming.yawVelocity = -40;
        incoming.xVelocity = 77; incoming.yVelocity = -13;
        Level1Entity motion{};
        motion.position = incoming.position; motion.forwardVelocity = incoming.forwardVelocity;
        motion.yawVelocity = incoming.yawVelocity;
        motion.xVelocity = incoming.xVelocity; motion.yVelocity = incoming.yVelocity;
        const auto expected = flatKinematicsOperations(motion).actor;
        const auto work = level2ProjectilePrefixOperations(before,{65535,680,0},scoreFont,pages);
        const auto& result = work.damage.combat.prefix;
        const auto& actual = result.projectiles[0];
        assert(actual.stateBytes4dTo54[1] == life-1 && actual.position == expected.position);
        assert(actual.yaw == expected.yaw && actual.flags == 4 && actual.xVelocity == 77 && actual.yVelocity == -13);
        assert(actual.stateBytes4dTo54[3] == 27 && actual.stateBytes4dTo54[5] == 41
            && actual.stateBytes4dTo54[6] == 61 && actual.stateBytes4dTo54[7] == 62);
        assert(contains(result.allocationOrder,{Level1PresentationKind::projectile,0}) == (life != 1));
        assert(contains(result.activeOrder,{Level1PresentationKind::projectile,0}) == (life != 1));
        assert(work.damage.combat.audioEvents.empty() && work.targetHits.empty() && work.scoreSteps.empty());
        assert(result.rng.calls() == before.prefix.rng.calls());
    }
}
void targetHits(const BitmapFont& scoreFont, const CockpitPageState& pages) {
    for (const auto targetIndex : {1U,0U,4U}) {
        for (const auto health : {1U,2U}) {
            auto before = input(); shot(before,64,true);
            auto& target = before.prefix.entities[targetIndex];
            target.stateBytes4dTo52[1] = static_cast<std::uint8_t>(health);
            const auto targetPosition = flatKinematicsOperations(target).actor.position;
            Level1Entity motion{}; motion.forwardVelocity = 500;
            const auto movement = flatKinematicsOperations(motion).actor.position;
            before.prefix.projectiles[0].position = {targetPosition.x-movement.x,
                targetPosition.y-movement.y,targetPosition.z-movement.z};
            const auto work = level2ProjectilePrefixOperations(before,{32,680,0},scoreFont,pages);
            const auto& result = work.damage.combat.prefix;
            const auto lethal = health == 1;
            const auto fixed = target.type == 30;
            const auto award = target.type == 8 ? 25U : target.type == 13 ? 10U : 0U;
            assert(result.entities[targetIndex].stateBytes4dTo52[1] == health-1);
            assert(!result.projectiles[0].active());
            assert(!contains(result.allocationOrder,{Level1PresentationKind::projectile,0}));
            assert(contains(result.allocationOrder,{Level1PresentationKind::entity,targetIndex}) == !lethal);
            assert(contains(result.activeOrder,{Level1PresentationKind::entity,targetIndex}) == !lethal);
            assert(work.activeEnemies == 5-(lethal && !fixed));
            assert(work.targetHits.size() == 1 && work.targetHits[0].targetIndex == targetIndex);
            assert(work.damage.combat.audioEvents.size() == 1);
            assert(work.damage.combat.audioEvents[0].eventId == (lethal ? 6 : 5));
            assert(work.score == (lethal ? award : 0));
            assert(work.scoreSteps.size() == static_cast<std::size_t>(lethal && !fixed));
            if (!work.scoreSteps.empty()) {
                assert(work.scoreSteps[0].operationOffset < result.operations.size());
                assert(!work.scoreSteps[0].work.fontSelections.empty());
            }
            const auto& effect = work.effects[0];
            assert(effect.listed && effect.entity.type == -1 && effect.entity.stateBytes4dTo52[0] == 252);
            assert(contains(result.activeOrder,{Level1PresentationKind::deathEffect,0}));
            std::size_t events = 0;
            for (const auto& op : result.operations) {
                assert(op.kind != NormalWorkKind::gameplayTargetHitEvent);
                if (op.kind == NormalWorkKind::gameplayLevel2TargetHitEvent) {
                    ++events; assert(op.count == 1 && op.value == (lethal ? 6 : 5));
                }
            }
            assert(events == 1);
        }
    }
}
void nearAxisChase(const BitmapFont& scoreFont, const CockpitPageState& pages) {
    struct ChaseCase { FixedPosition position; std::int16_t xVelocity; std::int16_t yVelocity; };
    //Original01D0..02A3 CPU controls, including both2000-unit corridor edges.
    const std::array<ChaseCase,12> cases{{
        {{1500,27000,0},-13,-224}, {{-1500,27000,0},12,-224},
        {{-24000,1500,0},223,-14}, {{-24000,-1500,0},223,13},
        {{1999,27000,0},-17,-224}, {{2000,27000,0},-112,-112},
        {{-1999,27000,0},16,-224}, {{-2000,27000,0},112,-112},
        {{-24000,1999,0},223,-19}, {{-24000,2000,0},112,-112},
        {{-24000,-1999,0},223,18}, {{-24000,-2000,0},112,112}
    }};
    for (const auto& test : cases) {
        auto before = input();
        auto& actor = before.prefix.entities[0];
        actor.position = test.position;
        actor.xVelocity = actor.yVelocity = 0;
        actor.yawVelocity = 0;
        const auto work = level2ProjectilePrefixOperations(before,{93,680,0},scoreFont,pages);
        const auto& returned = work.damage.combat.prefix.entities[0];
        assert(returned.position == test.position);
        assert(returned.xVelocity == test.xVelocity && returned.yVelocity == test.yVelocity);
        assert(work.damage.combat.audioEvents.empty() && work.targetHits.empty());
    }
}
void playerOverlap(const BitmapFont& scoreFont,const CockpitPageState& pages,const unsigned targetIndex) {
    auto before=input();
    std::swap(before.prefix.entities[0],before.prefix.entities[targetIndex]);
    for(std::size_t index=0;index<before.prefix.entities.size();++index) {before.prefix.entities[index].allocationIndex=index;}
    auto& actor=before.prefix.entities[targetIndex];
    actor.position={0,700,0};actor.xVelocity=0;actor.yVelocity=-112;actor.yawVelocity=0;
    const auto work=level2ProjectilePrefixOperations(before,{155,680,0},scoreFont,pages);
    const auto& result=work.damage.combat.prefix;
    assert(work.activeEnemies==4 && work.damage.armor==552 && work.damage.fadeTicks==8);
    assert(result.entities[targetIndex].stateBytes4dTo52[1]==0 && result.entities[targetIndex].collisionContactIsPlayer);
    assert(!contains(result.allocationOrder,{Level1PresentationKind::entity,targetIndex}));
    assert(!contains(result.activeOrder,{Level1PresentationKind::entity,targetIndex}));
    const auto receipt=std::find_if(result.contacts.begin(),result.contacts.end(),[](const auto& contact) {
        return contact.step==Level1ContactStep::actorPlayerOverlap;
    });
    assert(receipt!=result.contacts.end());
    auto operation=receipt->operationBegin;
    std::uint64_t beforeAudio=0;
    while(operation<result.operations.size() && result.operations[operation].kind==NormalWorkKind::instructions) {
        beforeAudio+=result.operations[operation++].count;
    }
    assert(beforeAudio==(targetIndex==0?36:targetIndex==3?57:73)); //Original09A9..09DB.
    assert(work.damage.combat.audioEvents.size()==2);
    for(const auto& event:work.damage.combat.audioEvents) {assert(event.loopTick==155 && event.eventId==6);}
    assert(work.damage.paletteSteps.size()==1);
    const auto& flash=work.damage.paletteSteps[0].request;
    assert(flash.start==128 && flash.count==128 && flash.targetColor==238 && flash.intensity==15);
    assert(!work.effects[0].listed && work.score==before.score);
    before.prefix.player=result.player;before.prefix.entities=result.entities;
    before.prefix.allocationOrder=result.allocationOrder;before.prefix.activeOrder=result.activeOrder;
    before.prefix.lastListCamera=result.lastListCamera;before.prefix.refreshRequested=result.refreshRequested;
    before.activeEnemies=work.activeEnemies;before.prefix.fadeTicks=work.damage.fadeTicks;
    for(int remaining=7;remaining>=0;--remaining) {
        const auto next=level2ProjectilePrefixOperations(before,{156,552,0},scoreFont,pages);
        assert(next.damage.armor==552 && next.damage.fadeTicks==remaining);
        assert(next.damage.paletteSteps.size()==1 && next.damage.combat.audioEvents.empty());
        const auto& request=next.damage.paletteSteps[0].request;
        assert(request.start==0 && request.count==256 && request.targetColor==238 && request.intensity==63-remaining*4);
        before.prefix.fadeTicks=next.damage.fadeTicks;
    }
}

void periodicLiftAltitude(const BitmapFont& scoreFont,const CockpitPageState& pages) {
    const auto sourceInput=[]() {
        auto result=input();
        for(auto& actor:result.prefix.entities) {
            actor.animatedModelPresenterEnabled=false;
            actor.renderProfile=WorldActorRenderProfile::noHandles;
        }
        return result;
    };
    const Level2PeriodicOrdinaryProfile descriptor{44,{0,1342},{0,928},{0,441},{0,134},{0,1,2,3,16,17,4}};
    const Level2ProjectilePrefixProfile strict{199,680,0,std::nullopt,Level1PlayerCombatDomain::expiryAndLoss};
    auto periodic=strict;periodic.periodic=descriptor;
    for(const auto altitude:{0,45,990,1000}) {
        auto before=sourceInput();before.prefix.entities[0].position.z=altitude;
        const auto expected=level2ProjectilePrefixOperations(before,strict,scoreFont,pages);
        const auto actual=level2ProjectilePrefixOperations(before,periodic,scoreFont,pages);
        const auto& left=expected.damage.combat.prefix;const auto& right=actual.damage.combat.prefix;
        assert(canonical(left.operations)==canonical(right.operations));
        assert(left.entities.size()==right.entities.size());
        for(std::size_t index=0;index<left.entities.size();++index) {
            assert(fields(left.entities[index])==fields(right.entities[index]));
            assert(left.entities[index].stateBytes4dTo52==right.entities[index].stateBytes4dTo52);
        }
        assert(right.entities[0].position.z>=0 && right.entities[0].position.z<=1000);
    }
    auto touching=sourceInput();
    const auto fixed=touching.prefix.entities[4].position;
    touching.prefix.entities[0].position={fixed.x-112,fixed.y+112,0};
    for(const auto altitude:{45,90}) {
        const auto expected=level2ProjectilePrefixOperations(touching,strict,scoreFont,pages);
        const auto actual=level2ProjectilePrefixOperations(touching,periodic,scoreFont,pages);
        const auto& returned=actual.damage.combat.prefix;
        assert(returned.entities[0].position.z==altitude);
        assert(canonical(returned.operations)==canonical(expected.damage.combat.prefix.operations));
        touching.prefix.player=returned.player;touching.prefix.entities=returned.entities;
        touching.prefix.projectiles=returned.projectiles;touching.prefix.allocationOrder=returned.allocationOrder;
        touching.prefix.activeOrder=returned.activeOrder;touching.prefix.lastListCamera=returned.lastListCamera;
        touching.prefix.refreshRequested=returned.refreshRequested;touching.prefix.rng=returned.rng;
        touching.prefix.fadeTicks=actual.damage.fadeTicks;touching.prefix.impulse=returned.impulse;
        touching.effects=actual.effects;touching.bonus=actual.bonus;touching.activeEnemies=actual.activeEnemies;touching.score=actual.score;
    }
    for(const auto altitude:{-1,1001}) {
        auto bad=sourceInput();bad.prefix.entities[0].position.z=altitude;
        rejects([&] {level2ProjectilePrefixOperations(bad,periodic,scoreFont,pages);});
    }
    for(const auto index:{1U,4U}) {
        auto bad=sourceInput();bad.prefix.entities[index].position.z=45;
        rejects([&] {level2ProjectilePrefixOperations(bad,periodic,scoreFont,pages);});
    }
    auto vertical=sourceInput();vertical.prefix.entities[0].position.z=45;vertical.prefix.entities[0].zVelocity=1;
    rejects([&] {level2ProjectilePrefixOperations(vertical,periodic,scoreFont,pages);});
    auto pending=sourceInput();auto& birth=pending.prefix.entities[0];
    birth.position.z=45;birth.xVelocity=0;birth.yVelocity=0;birth.yaw=0;
    pending.prefix.activeOrder.erase(pending.prefix.activeOrder.begin());
    pending.prefix.allocationOrder.erase(pending.prefix.allocationOrder.begin());
    pending.prefix.allocationOrder.push_back({Level1PresentationKind::entity,0});pending.prefix.refreshRequested=true;
    rejects([&] {level2ProjectilePrefixOperations(pending,periodic,scoreFont,pages);});
}

} // namespace

int main() {
#ifdef _MSC_VER
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    try {
    const auto scoreFont = font();
    const auto pages = CockpitPageState::fromRecoveredMenu();
    ordinaryEquivalence(scoreFont,pages);
    periodicLiftAltitude(scoreFont,pages);
    updateAndRetire(scoreFont,pages);
    targetHits(scoreFont,pages);
    nearAxisChase(scoreFont,pages);
    for(const auto index:{0U,3U,6U}) {playerOverlap(scoreFont,pages,index);}
    auto bad = input(); shot(bad,64,false); bad.prefix.refreshRequested = false;
    rejects([&] { level2ProjectilePrefixOperations(bad,{16,680,0},scoreFont,pages); });
    bad = input(); shot(bad,64,true); bad.prefix.projectiles[0].type = 20;
    rejects([&] { level2ProjectilePrefixOperations(bad,{16,680,0},scoreFont,pages); });
    bad = input(); bad.prefix.entities[0].type = 11;
    rejects([&] { level2ProjectilePrefixOperations(bad,{16,680,0},scoreFont,pages); });
    bad = input(); bad.activeEnemies = 4;
    rejects([&] { level2ProjectilePrefixOperations(bad,{16,680,0},scoreFont,pages); });
    bad = input(); bad.prefix.player.yawVelocity = 513;
    rejects([&] { level2ProjectilePrefixOperations(bad,{16,680,0},scoreFont,pages); });
    bad = input();
    rejects([&] { level1ProjectilePrefixOperations(bad.prefix); });
    } catch(const std::exception& error) {
        std::fprintf(stderr,"%s\n",error.what());
        return 1;
    }
}
