#include "stellar7/retry_work.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace {
template<class Function> void rejects(Function function) {
    bool rejected = false;
    try { function(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
std::uint64_t instructions(const stellar7::Level1RetryWork& work) {
    std::uint64_t count = 0;
    for (const auto& segment : work.segments) {
        for (const auto& operation : segment.operations) { count += operation.count; }
    }
    return count;
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace stellar7;
    HighscoreTable::Entries entries{};
    for (auto& entry : entries) { entry.score = 1800; }
    Level1RetryProfile input{};
    input.decision = ContinueOutcome::yes;
    input.remainingContinues = 2;
    input.currentHighscores = HighscoreTable({},entries);
    input.systemLevels.fill(1);
    input.systemTimers = {10,11,12,13,14,15,16};
    input.frontPage = 0xa000; input.backPage = 0xa800; input.cachePage = 0xa400;
    const auto retry = level1RankMissRetryOperations(input);
    assert(instructions(retry) == 226); //Original control: low-word score0 rank miss, introduction0.
    assert(retry.observation.remainingContinues == 1 && retry.observation.levelIndex == 0);
    assert(retry.observation.score == 0 && retry.observation.armor == 680 && retry.observation.loopTick == 0);
    assert(retry.observation.fadeTicks == 0 && retry.observation.ability == 0);
    assert((retry.observation.systemTimers == std::array<std::int16_t,7>{0,0,0,13,0,0,0}));
    assert(retry.observation.oldPresenterListEmpty && retry.observation.listRefreshRequested && retry.observation.firstEntryDisplay);
    assert(retry.segments[retry.modelReloadSegment].kind == RetryWorkKind::modelReloadProvider);
    assert(retry.segments[retry.loaderPagesSegment].kind == RetryWorkKind::loaderPages);
    assert(retry.segments[retry.controllerRebindSegment].kind == RetryWorkKind::controllerRebindProvider);
    assert(retry.modelReloadSegment < retry.loaderPagesSegment && retry.loaderPagesSegment < retry.controllerRebindSegment);
    for (const auto kind : {RetryWorkKind::modelReloadProvider,RetryWorkKind::retainedConstructorCompletionProvider,
        RetryWorkKind::loaderPages,RetryWorkKind::musicActivation,RetryWorkKind::directReset,
        RetryWorkKind::retainedGroundInitializationProvider,RetryWorkKind::palette}) {
        assert(std::count_if(retry.segments.begin(),retry.segments.end(),[&](const auto& s) { return s.kind == kind; }) == 1);
    }
    const auto palette = std::find_if(retry.segments.begin(),retry.segments.end(),[](const auto& s) { return s.palette.has_value(); });
    assert(palette != retry.segments.end() && palette->palette->start == 0 && palette->palette->count == 256
        && palette->palette->targetColor == 0 && palette->palette->intensity == 0);
    auto profile = input;
    profile.introduction = 255;
    const auto permanent = level1RankMissRetryOperations(profile);
    assert(permanent.observation.introduction == 255 && instructions(permanent) == 226+11);
    profile.introduction = 2;
    const auto nonpermanent = level1RankMissRetryOperations(profile);
    assert(nonpermanent.observation.introduction == 1 && instructions(nonpermanent) == 240);
    profile = input; profile.frontPage = 0xa800; profile.backPage = 0xa000;
    const auto reverse = level1RankMissRetryOperations(profile);
    assert(reverse.observation.frontPage == profile.frontPage && reverse.observation.backPage == profile.backPage);
    profile = input; profile.currentHighscores.reset();
    rejects([&] { level1RankMissRetryOperations(profile); });
    entries.back().score = 0xffffffff;
    profile.currentHighscores = HighscoreTable({},entries);
    rejects([&] { level1RankMissRetryOperations(profile); });
    profile = input; profile.score = 1801;
    rejects([&] { level1RankMissRetryOperations(profile); });
    profile = input; profile.levelIndex = 1;
    rejects([&] { level1RankMissRetryOperations(profile); });
    profile = input; profile.remainingContinues = 0;
    rejects([&] { level1RankMissRetryOperations(profile); });
    profile = input; profile.decision = ContinueOutcome::no;
    rejects([&] { level1RankMissRetryOperations(profile); });
    profile = input; profile.currentSequencePublished = false;
    rejects([&] { level1RankMissRetryOperations(profile); });
    profile = input; profile.selectedSystem = 0;
    rejects([&] { level1RankMissRetryOperations(profile); });
    profile = input; profile.systemLevels[0] = 0;
    rejects([&] { level1RankMissRetryOperations(profile); });
    profile = input; profile.cachePage = 0xa800;
    rejects([&] { level1RankMissRetryOperations(profile); });
}
