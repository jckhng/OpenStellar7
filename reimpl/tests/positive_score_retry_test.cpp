#include "stellar7/retry_work.hpp"
#include "stellar7/gameplay.hpp"
#include <cassert>
#include <limits>
#include <stdexcept>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace stellar7 {
struct Level1ReloadTestAccess {static void score(Level1State& level,std::uint32_t value) {level.score_=value;}};
struct GameplayModelTestAccess {
    static GameplayModel scoredPostBlackout(std::uint32_t score) {
        auto model=GameplayModel::fromNativeStartup();Level1ReloadTestAccess::score(model.level1_,score);
        model.phase_=GameplayPhase::failurePostExplosionBlackout;model.failureFromLevel2_=false;model.ticks_=73;
        return model;
    }
};
}
using namespace stellar7;
namespace {
template<class Action>void rejects(Action action) {
    bool rejected=false;try {action();}catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
}
std::uint64_t dispatches(const Level1RetryWork& work) {
    std::uint64_t count=0;for(const auto& segment:work.segments) {for(const auto& op:segment.operations) {count+=op.count;}}return count;
}
Level1RetryProfile profile() {
    Level1RetryProfile result{};result.decision=ContinueOutcome::yes;result.remainingContinues=2;
    HighscoreTable::Entries entries{};const std::array<std::uint32_t,10> values{5155,4815,4470,3960,2770,2100,1920,1870,1820,1800};
    for(std::size_t i=0;i<entries.size();++i) {entries[i].score=values[i];}
    result.currentHighscores=HighscoreTable({},entries);result.systemLevels.fill(1);
    result.frontPage=0xa000;result.backPage=0xa800;result.cachePage=0xa400;return result;
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    for(const auto score:{1U,20U,50U,1000U,1799U,1800U}) {
        auto input=profile();input.score=score;
        const auto header=input.currentHighscores->header();const auto original=input.currentHighscores->entries();
        const auto work=level1RankMissRetryOperations(input);
        assert(input.score==score&&dispatches(work)==226); //Original131I prefix + reused95I suffix.
        assert(work.observation.score==0&&work.observation.remainingContinues==1&&work.observation.armor==680);
        assert(input.currentHighscores->header()==header);
        for(std::size_t i=0;i<original.size();++i) {assert(input.currentHighscores->entries()[i].score==original[i].score&&input.currentHighscores->entries()[i].name==original[i].name);}
        auto model=GameplayModelTestAccess::scoredPostBlackout(score);model.enterRecoveredContinue();
        assert(model.score()==score&&model.ticks()==73);model.pollRecoveredContinue({});
        GameplayInput up{};up.forward=true;model.pollRecoveredContinue(up);model.pollRecoveredContinue({});
        GameplayInput enter{};enter.confirm=true;model.pollRecoveredContinue(enter);
        assert(model.pollRecoveredContinue({})==ContinueDecision::yes&&model.score()==score&&model.ticks()==73);
        model.resumeRecoveredContinueRetry(); //Source0417/041A now clear, not before330A.
        assert(model.score()==0&&model.ticks()==73);
    }
    auto input=profile();input.score=1801;rejects([&] {level1RankMissRetryOperations(input);});
    input.score=5156;rejects([&] {level1RankMissRetryOperations(input);});
    input.score=std::uint32_t{1}<<31;rejects([&] {level1RankMissRetryOperations(input);});
    input.score=0xffffffffU;rejects([&] {level1RankMissRetryOperations(input);});
    input=profile();input.score=0x10001;HighscoreTable::Entries entries{};
    for(auto& row:entries) {row.score=0x20000;}
    entries[0].score=entries[1].score=0x10001;input.currentHighscores=HighscoreTable({},entries);
    assert(dispatches(level1RankMissRetryOperations(input))==210); //Two equal high words, eight high-word misses.
    entries[9].score=0x10000;input.currentHighscores=HighscoreTable({},entries);rejects([&] {level1RankMissRetryOperations(input);});
    entries[9].score=0xffffffffU;input.currentHighscores=HighscoreTable({},entries);rejects([&] {level1RankMissRetryOperations(input);});
    input=profile();input.score=20;input.decision=ContinueOutcome::no;rejects([&] {level1RankMissRetryOperations(input);});
    input=profile();input.score=20;input.remainingContinues=0;rejects([&] {level1RankMissRetryOperations(input);});
    input=profile();input.score=20;input.currentHighscores.reset();rejects([&] {level1RankMissRetryOperations(input);});
}
