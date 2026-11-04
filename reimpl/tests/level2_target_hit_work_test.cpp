#include "target_hit_work_fixture.inc"

int main() {
    //1176 branches on signed type bounds, category and class, not Level1 identity.
    for (const auto type : {8,13,30}) {
        const auto fixed = type == 30;
        for (const auto& control : controls) {
            if ((control.type == 29) != fixed) { continue; }
            auto before = input(control);
            before.target.type = static_cast<std::int16_t>(type);
            before.target.stateBytes4dTo52[4] = static_cast<std::uint8_t>(type == 8 ? 2 : type == 13 ? 4 : 0);
            if (before.bonus.lastType == control.type) {
                before.bonus.lastType = static_cast<std::int16_t>(type);
            }
            rejects([&] { level1TargetHitOperations(before); });
            rejects([&] { level1TargetHitOperations(before,Level1TargetHitDomain::initialOrdinary); });
            const auto work = level1TargetHitOperations(before,Level1TargetHitDomain::level2Constructor);
            assert(work.targetHealth == control.healthAfter);
            assert(work.destroyed == (control.healthAfter == 0));
            assert(work.activeEnemies == control.activeAfter);
            assert(work.bonus.consecutive == control.streakAfter);
            const auto expectedType = work.destroyed ? type : before.bonus.lastType;
            assert(work.bonus.lastType == expectedType);
            assert(apply(before,work) == control.allocationsAfter);
            assert(work.effect.has_value() == !control.effectBytes.empty());
            if (work.effect) { assert(apply(*work.effect) == control.effectBytes); }
            const auto actual = canonical(work);
            assert(actual.size() == control.operations.size());
            for (std::size_t index = 0; index < actual.size(); ++index) {
                const auto a = actual[index], b = control.operations[index];
                assert(a.kind == b.kind && a.count == b.count && a.value == b.value);
            }
            auto bad = before;
            bad.target.stateBytes4dTo52[4] = 5;
            rejects([&] { level1TargetHitOperations(bad,Level1TargetHitDomain::level2Constructor); });
            bad = before; bad.activeEnemies = 6;
            rejects([&] { level1TargetHitOperations(bad,Level1TargetHitDomain::level2Constructor); });
            if (type != 8 && !fixed && before.target.stateBytes4dTo52[1] == 1) {
                bad = before; bad.bonus = {static_cast<std::int16_t>(type),2};
                rejects([&] { level1TargetHitOperations(bad,Level1TargetHitDomain::level2Constructor); });
            }
        }
    }
    auto third = input(controls[0]);
    third.target.type=8;third.target.stateBytes4dTo52[4]=2;third.bonus={8,2};
    const auto noBonus=level1TargetHitOperations(third,Level1TargetHitDomain::level2Constructor);
    assert(noBonus.destroyed && noBonus.bonus.lastType==8 && noBonus.bonus.consecutive==3);
    const auto branch=canonical(noBonus);
    assert(branch.size()==5 && branch[0].kind==0 && branch[0].count==125);
    assert(branch[1].kind==2 && branch[1].count==6 && branch[2].kind==0 && branch[2].count==49);
    assert(branch[3].kind==-1 && branch[3].count==4 && branch[3].value==6
        && branch[4].kind==0 && branch[4].count==8);
    for (const auto& control : controls) {
        rejects([&] { level1TargetHitOperations(input(control),Level1TargetHitDomain::level2Constructor); });
    }
}
