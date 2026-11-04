#include "stellar7/retry_work.hpp"
#include "stellar7/level1_transition_work.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <stdexcept>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace {
using namespace stellar7;
template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
Level1RetryProfile profile() {
    Level1RetryProfile input{};
    input.decision = ContinueOutcome::yes;
    input.levelIndex = 1;
    input.score = 1350;
    input.remainingContinues = 2;
    input.systemLevels.fill(3);
    input.systemTimers = {1,2,3,17,5,6,7};
    input.frontPage = 0xa000; input.backPage = 0xa800; input.cachePage = 0xa400;
    HighscoreTable::Entries rows{};
    for (auto& row : rows) { row.score = 1800; }
    input.currentHighscores = HighscoreTable({},rows);
    return input;
}
std::size_t child(const Level1RetryWork& work, const RetryWorkKind kind) {
    const auto found = std::find_if(work.segments.begin(),work.segments.end(),
        [&](const auto& segment) { return segment.kind == kind; });
    assert(found != work.segments.end());
    return static_cast<std::size_t>(found-work.segments.begin());
}
void compareStreams(const Level1RetryWork& first, const Level1RetryWork& second) {
    assert(first.segments.size() == second.segments.size());
    for (std::size_t i=0;i<first.segments.size();++i) {
        const auto& a=first.segments[i]; const auto& b=second.segments[i];
        assert(a.kind==b.kind && a.sourceCs==b.sourceCs && a.sourceEntry==b.sourceEntry
            && a.sourceResume==b.sourceResume && a.guestElapsedQualified==b.guestElapsedQualified);
        assert(a.operations.size()==b.operations.size() && a.palette.has_value()==b.palette.has_value());
        for (std::size_t j=0;j<a.operations.size();++j) {
            assert(a.operations[j].kind==b.operations[j].kind && a.operations[j].count==b.operations[j].count
                && a.operations[j].value==b.operations[j].value);
        }
        if (a.palette) {
            assert(a.palette->start==b.palette->start && a.palette->count==b.palette->count
                && a.palette->targetColor==b.palette->targetColor && a.palette->intensity==b.palette->intensity);
        }
    }
}
ContinueWork acceptYes() {
    std::vector<IndexedBitmap> art(23);
    const auto bitmap=[&](std::size_t index,std::uint16_t width,std::uint16_t height) {
        art[index]={width,height,std::vector<std::uint8_t>(width*height,37)};
    };
    bitmap(0,16,20); bitmap(11,8,7); bitmap(12,40,40); bitmap(17,120,33);
    bitmap(19,120,33); bitmap(20,264,2); bitmap(22,88,11);
    const BitmapFont font{32,6,0,std::vector<IndexedBitmap>(64,{8,6,std::vector<std::uint8_t>(48)})};
    IndexedPages pages;
    for (const auto page : {0xa000,0xa400,0xa800}) {
        for (std::int16_t y=0;y<200;++y) {
            for (std::int16_t x=0;x<320;++x) { pages.write(page,{x,y},static_cast<std::uint8_t>(x+y)); }
        }
    }
    ContinueState state{};
    state.scratch.assign(500,91); state.transfer.assign(1024,37);
    state.frontPage=0xa000; state.backPage=0xa800;
    state.pointer={163,69}; state.oldRectangle={163,69,16,21};
    auto work=continueEntryOperations(state,pages,art,font,2);
    assert(work.state.stage==ContinueStage::polling && work.state.selected==1);
    work=continuePollOperations(work.state,work.pages,art,font,{});
    ContinueKeyboardPacket up{}; up.directions[4]=true;
    work=continuePollOperations(work.state,work.pages,art,font,up);
    assert(work.state.selected==0 && work.outcome==ContinueOutcome::pending);
    ContinueKeyboardPacket enter{}; enter.enter=true;
    work=continuePollOperations(work.state,work.pages,art,font,enter);
    assert(work.state.pressed==1 && work.outcome==ContinueOutcome::pending);
    work=continuePollOperations(work.state,work.pages,art,font,{});
    assert(work.state.stage==ContinueStage::finished && work.outcome==ContinueOutcome::yes);
    assert(work.state.frontPage==state.frontPage && work.state.backPage==state.backPage);
    return work;
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    try {
        using namespace stellar7;
        const auto yes=acceptYes();
        for (const auto score : {0U,20U,1350U,1800U}) {
            auto input=profile(); input.score=score; input.decision=yes.outcome;
            const auto before=*input.currentHighscores;
            const auto work=level2RankMissRetryOperations(input);
            assert(input.score==score && input.levelIndex==1 && input.remainingContinues==2);
            assert(work.observation.levelIndex==1 && work.observation.remainingContinues==1);
            assert(work.observation.score==0 && work.observation.armor==680 && work.observation.loopTick==0);
            assert(work.observation.fadeTicks==0 && work.observation.ability==0 && work.observation.introduction==0);
            assert((work.observation.systemTimers==std::array<std::int16_t,7>{0,0,0,17,0,0,0}));
            assert(work.observation.firstEntryDisplay && work.observation.oldPresenterListEmpty
                && work.observation.listRefreshRequested);
            for (std::size_t i=0;i<before.entries().size();++i) {
                assert(input.currentHighscores->entries()[i].score==before.entries()[i].score
                    && input.currentHighscores->entries()[i].name==before.entries()[i].name);
            }
            auto level1=input; level1.levelIndex=0;
            const auto old=level1RankMissRetryOperations(level1);
            compareStreams(work,old); //Same intro0/source branches; level identity is an observation.
            assert(old.observation.levelIndex==0);
            std::uint64_t dispatches=0;
            for (const auto& segment : work.segments) {
                for (const auto& operation : segment.operations) { dispatches+=operation.count; }
                assert(segment.sourceEntry!=0x0784 && segment.sourceEntry!=0x0a9a);
            }
            assert(dispatches==226);
            const auto constructors=child(work,RetryWorkKind::retainedConstructorCompletionProvider);
            const auto ground=child(work,RetryWorkKind::retainedGroundInitializationProvider);
            assert(work.segments[work.modelReloadSegment].sourceEntry==0x48d4
                && work.modelReloadSegment<constructors && constructors<work.loaderPagesSegment
                && work.loaderPagesSegment<ground && ground<work.controllerRebindSegment);
            for (const auto kind : {RetryWorkKind::modelReloadProvider,RetryWorkKind::retainedConstructorCompletionProvider,
                RetryWorkKind::loaderPages,RetryWorkKind::musicActivation,RetryWorkKind::palette,
                RetryWorkKind::directReset,RetryWorkKind::retainedGroundInitializationProvider}) {
                assert(std::count_if(work.segments.begin(),work.segments.end(),
                    [&](const auto& segment) { return segment.kind==kind; })==1);
            }
            assert(std::count_if(work.segments.begin(),work.segments.end(),
                [](const auto& segment) { return segment.kind==RetryWorkKind::stopPendingSound; })==2);
        }
        auto input=profile(); std::swap(input.frontPage,input.backPage);
        assert(level2RankMissRetryOperations(input).observation.frontPage==0xa800);
        for (const auto introduction : {1,2,255,256}) {
            auto bad=profile(); bad.introduction=static_cast<std::uint16_t>(introduction);
            rejects([&] { level2RankMissRetryOperations(bad); });
        }
        for (const auto index : {0,2,6}) {
            auto bad=profile(); bad.levelIndex=static_cast<std::uint16_t>(index);
            rejects([&] { level2RankMissRetryOperations(bad); });
        }
        rejects([&] { level1RankMissRetryOperations(profile()); });
        for (const auto decision : {ContinueOutcome::pending,ContinueOutcome::no,ContinueOutcome::cancelled,ContinueOutcome::unavailable}) {
            auto bad=profile(); bad.decision=decision; rejects([&] { level2RankMissRetryOperations(bad); });
        }
        auto bad=profile(); bad.score=1801; rejects([&] { level2RankMissRetryOperations(bad); });
        bad=profile(); bad.score=std::uint32_t{1}<<31; rejects([&] { level2RankMissRetryOperations(bad); });
        bad=profile(); bad.remainingContinues=0; rejects([&] { level2RankMissRetryOperations(bad); });
        bad=profile(); bad.currentHighscores.reset(); rejects([&] { level2RankMissRetryOperations(bad); });
        bad=profile(); bad.currentSequencePublished=false; rejects([&] { level2RankMissRetryOperations(bad); });
        bad=profile(); bad.globalExitRequested=true; rejects([&] { level2RankMissRetryOperations(bad); });
        bad=profile(); bad.selectedSystem=0; rejects([&] { level2RankMissRetryOperations(bad); });
        bad=profile(); bad.systemLevels[0]=0; rejects([&] { level2RankMissRetryOperations(bad); });
        bad=profile(); bad.cachePage=0xa800; rejects([&] { level2RankMissRetryOperations(bad); });
        auto rows=profile().currentHighscores->entries(); rows.back().score=0xffffffff;
        bad=profile(); bad.currentHighscores=HighscoreTable({},rows);
        rejects([&] { level2RankMissRetryOperations(bad); });

        //Explicit same-Level2 reset uses CURRENT physical pools, including live/retired FD19.
        //No success flags, cold constructor, clock, page import or RNG initialization occurs here.
        Level2PoolResetInput pools{}; pools.armor=680; pools.systemTimers={1,2,3,17,5,6,7};
        for (auto& actor : pools.ordinary) { actor.renderProfile=WorldActorRenderProfile::noHandles; }
        for (auto& actor : pools.fixed) { actor.renderProfile=WorldActorRenderProfile::noHandles; }
        for (auto& actor : pools.effects) { actor.renderProfile=WorldActorRenderProfile::noHandles; }
        pools.ordinary[0].type=8; pools.ordinary[0].stateBytes4dTo52={255,1,0,2,84,0};
        pools.ordinary[1].type=13; pools.ordinary[1].stateBytes4dTo52={255,0,0,0,0,0};
        for (std::size_t slot=0;slot<pools.projectiles.size();++slot) {
            auto& actor=pools.projectiles[slot]; actor.type=slot==7 ? 23 : 19;
            actor.renderProfile=WorldActorRenderProfile::inlineSelectorNoCallback;
            actor.stateBytes4dTo52={253,static_cast<std::uint8_t>(slot&1 ? 0 : 42),2,9,1,0};
            actor.position={1234,-5678,0}; actor.flags=4;
        }
        pools.fixed[12].type=30; pools.fixed[12].flags=4; pools.fixed[12].position={99,-101,202};
        pools.fixed[12].stateBytes4dTo52={254,100,3,4,5,6};
        pools.effects[3].type=-1; pools.effects[3].flags=0x8c;
        pools.effects[3].stateBytes4dTo52={252,20,0,0,24,0};
        Model3d playerModel{}; playerModel.collisionRadius=723;
        const auto reset=level2PoolReset(pools,playerModel);
        assert(reset.state.armor==680 && reset.state.systemTimers[3]==17 && reset.selectorReleases.empty());
        assert(pools.projectiles[0].type==19 && pools.projectiles[0].stateBytes4dTo52[1]==42);
        for (const auto& actor : reset.state.projectiles) {
            assert(actor.type==0 && actor.stateBytes4dTo52[0]==253 && actor.stateBytes4dTo52[1]==0
                && actor.renderProfile==WorldActorRenderProfile::inlineSelectorNoCallback && actor.position==FixedPosition{});
        }
        assert(reset.state.fixed[12].type==pools.fixed[12].type && reset.state.fixed[12].flags==pools.fixed[12].flags
            && reset.state.fixed[12].position==pools.fixed[12].position
            && reset.state.fixed[12].stateBytes4dTo52==pools.fixed[12].stateBytes4dTo52);
        assert(reset.state.effects[3].type==0 && reset.state.effects[3].flags==0x88);
        std::vector<Model3d> shared(44); shared[0]=playerModel;
        shared[8].flags=1; shared[8].collisionRadius=860; shared[13].collisionRadius=559; shared[30].collisionRadius=868;
        std::array<std::uint16_t,56> table{};
        for (unsigned i=0;i<table.size();++i) { table[i]=static_cast<std::uint16_t>(i*1193U+83U); }
        const StellarRng failureReturn(table,48,110,0,100);
        const auto constructors=level2LoaderActors(failureReturn,shared,{});
        assert(failureReturn.calls()==100 && constructors.rng.calls()==112);
        const auto ground=level2RetainedGroundInitializationOperations(constructors.rng);
        assert(ground.rng.calls()==124 && constructors.actors.size()==7 && constructors.activeEnemies==5);
        const std::array<unsigned,7> types{8,8,8,13,13,30,30};
        for (std::size_t i=0;i<types.size();++i) { assert(constructors.actors[i].actor.type==types[i]); }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
