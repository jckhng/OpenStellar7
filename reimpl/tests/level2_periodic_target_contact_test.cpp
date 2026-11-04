#include "target_hit_work_fixture.inc"
#include "stellar7/level2_contact_work.hpp"
#include "stellar7/level2_projectile_prefix_work.hpp"
#include "stellar7/indexed_render.hpp"
#include <limits>
namespace {
BitmapFont scoreFont() {
    BitmapFont result{32,8,0,{}};
    for(std::size_t index=0;index<96;++index) {
        const auto width=static_cast<std::uint16_t>(index==0?3:2+index%4);
        IndexedBitmap glyph{width,8,std::vector<std::uint8_t>(width*8)};
        for(std::size_t pixel=0;pixel<glyph.pixels.size();++pixel) {glyph.pixels[pixel]=index==0?0:static_cast<std::uint8_t>((pixel+index)%3==0);}
        result.glyphs.push_back(std::move(glyph));
    }
    return result;
}
void targets() {
    for(const auto type:{1,4}) {
        for(const auto& control:controls) {
            if(control.type!=1) {continue;}
            auto before=input(control);before.target.type=static_cast<std::int16_t>(type);
            if(before.bonus.lastType==1) {before.bonus.lastType=static_cast<std::int16_t>(type);}
            const auto work=level1TargetHitOperations(before,Level1TargetHitDomain::level2PeriodicOrdinary);
            const auto expected=level1TargetHitOperations(input(control));
            const auto a=canonical(work),b=canonical(expected);
            assert(a.size()==b.size());
            for(std::size_t i=0;i<a.size();++i) {assert(a[i].kind==b[i].kind && a[i].count==b[i].count && a[i].value==b[i].value);}
            assert(work.targetHealth==expected.targetHealth && work.activeEnemies==expected.activeEnemies);
            assert(work.bonus.consecutive==expected.bonus.consecutive);
            assert(apply(before,work)==apply(input(control),expected));
            rejects([&]{level1TargetHitOperations(before,Level1TargetHitDomain::level2Constructor);});
        }
        auto alias=input(controls[0]);alias.target.type=static_cast<std::int16_t>(type);
        alias.targetIndex=15;alias.target.allocationIndex=15;alias.activeEnemies=12;
        for(auto& ref:alias.allocationOrder) {if(ref.kind==Level1PresentationKind::entity && ref.index==0) {ref.index=15;}}
        assert(level1TargetHitOperations(alias,Level1TargetHitDomain::level2PeriodicOrdinary).activeEnemies==11);
        auto third=alias;third.bonus={static_cast<std::int16_t>(type),2};
        rejects([&]{level1TargetHitOperations(third,Level1TargetHitDomain::level2PeriodicOrdinary);});
        auto bad=alias;bad.activeEnemies=13;
        rejects([&]{level1TargetHitOperations(bad,Level1TargetHitDomain::level2PeriodicOrdinary);});
    }
    const auto font=scoreFont();const auto pages=CockpitPageState::fromRecoveredMenu();
    for(const auto award:{20,30,25,10}) {
        const auto work=scoreOperations(80,static_cast<std::int16_t>(award),0,font,pages,ScoreAwardDomain::level2PeriodicOrdinary);
        assert(work.score==static_cast<unsigned>(80+award));
        IndexedPages actual,expected;
        for(const auto page:{pages.frontPage(),pages.backPage(),pages.cachePage()}) {
            for(std::int16_t y=0;y<200;++y) {for(std::int16_t x=0;x<320;++x) {
                actual.write(page,{x,y},0);expected.write(page,{x,y},0);
            }}
        }
        actual.apply(work.effects);
        expected.apply(scorePageEffects(work.score,font,pages.backPage(),pages.frontPage(),CockpitPageLayout::recoveredStartup));
        for(const auto page:{pages.frontPage(),pages.backPage(),pages.cachePage()}) {
            for(std::int16_t y=0;y<200;++y) {for(std::int16_t x=0;x<320;++x) {
                assert(actual.read(page,{x,y})==expected.read(page,{x,y}));
            }}
        }
    }
    rejects([&]{scoreOperations(0,30,0,font,pages,ScoreAwardDomain::level2Constructor);});
    rejects([&]{scoreOperations(0,50,0,font,pages,ScoreAwardDomain::level2PeriodicOrdinary);});
}
void contacts() {
    Level1Entity player{};player.type=0;player.collisionRadius=723;
    for(const auto type:{1,4,8,13,30}) {
        Level1Entity target{};target.type=static_cast<std::int16_t>(type);target.allocationIndex=15;
        target.collisionRadius=type==1?1342:type==4?928:type==8?860:type==13?559:868;
        target.stateBytes4dTo52={static_cast<std::uint8_t>(type==30?254:255),1,0,0,
            static_cast<std::uint8_t>(type==1||type==4?1:type==8?2:type==13?4:0),0};
        const auto work=level2SolidContactOperations(player,target,15,target.collisionRadius);
        assert(!actorOverlapOperations(work.contact.player,target).overlaps);
        assert(work.contact.player.forwardVelocity==-10 && work.contact.impulse==-10);
        assert(work.damageChildOffset.has_value()==(type==4));
        if(work.damageChildOffset) {assert(*work.damageChildOffset==2);}
        assert(std::count_if(work.contact.operations.begin(),work.contact.operations.end(),[](const auto op) {
            return op.kind==NormalWorkKind::gameplayLevel2SolidContactEvent && op.value==9;
        })==1);
        rejects([&]{level2SolidContactOperations(player,target,15,target.collisionRadius+1);});
        if(type==1) {
            const auto old=level1SolidContactOperations(player,target,15);
            assert(old.player.position==work.contact.player.position && old.operations.size()==work.contact.operations.size());
            for(std::size_t i=0;i<old.operations.size();++i) {
                const auto& a=old.operations[i];const auto& b=work.contact.operations[i];
                assert(a.count==b.count && a.value==b.value);
                assert(a.kind==b.kind || (a.kind==NormalWorkKind::gameplaySolidContactEvent
                    && b.kind==NormalWorkKind::gameplayLevel2SolidContactEvent));
            }
        }
    }
    for(const auto armor:{680,100,0,-796}) {
        const auto work=level2Type4DamageOperations(static_cast<std::int16_t>(armor),Level1PlayerCombatDomain::expiryAndLoss);
        assert(work.armor==armor-100 && work.fadeTicks==8);
        assert(work.palette.operationOffset==5 && work.palette.request.intensity==15);
        assert(work.operations[1].kind==NormalWorkKind::gameplayLevel2TargetHitEvent && work.operations[1].value==6);
    }
    rejects([&]{level2Type4DamageOperations(-797,Level1PlayerCombatDomain::expiryAndLoss);});
    rejects([&]{level2Type4DamageOperations(99,Level1PlayerCombatDomain::survivingOnly);});
    static_assert(level2MaximumFrameDamage==896);
}
}
int main() {targets();contacts();return 0;}
