#include "stellar7/retry_work.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace stellar7 {
namespace {
Level1RetryWork rankMissRetryOperations(const Level1RetryProfile& input,
    const std::uint16_t expectedLevelIndex) {
    const bool physicalRoles = input.cachePage == 0xa400
        && ((input.frontPage == 0xa000 && input.backPage == 0xa800)
            || (input.frontPage == 0xa800 && input.backPage == 0xa000));
    if (input.decision != ContinueOutcome::yes || input.levelIndex != expectedLevelIndex
        || input.score > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())
        || input.remainingContinues == 0 || input.introduction > 255
        || (expectedLevelIndex == 1 && input.introduction != 0) || !input.currentHighscores
        || input.globalExitRequested || !input.currentSequencePublished || input.selectedSystem != -1
        || !physicalRoles || std::any_of(input.systemLevels.begin(),input.systemLevels.end(),
            [](const auto value) { return value <= 0; })) {
        throw std::invalid_argument(expectedLevelIndex == 0
            ? "Level1 retry requires accepted low-score Yes, current table, ordinary systems and retained physical roles"
            : "Level2 retry requires accepted rank-miss Yes, introduction0, current table and retained physical roles");
    }
    for (const auto& entry : input.currentHighscores->entries()) {
        if ((entry.score >> 16) >= 0x8000) {
            throw std::invalid_argument("source signed highscore comparison would enter the unsupported name editor");
        }
    }
    if (input.currentHighscores->rank(input.score)) {
        throw std::invalid_argument("source retry reaches the unsupported highscore name editor");
    }
    Level1RetryWork result{};
    result.observation = {expectedLevelIndex,static_cast<std::uint16_t>(input.remainingContinues-1),680,0,0,0,0,
        input.systemTimers,input.introduction,input.frontPage,input.backPage,input.cachePage,true,true,true};
    for (std::size_t i = 0; i < result.observation.systemTimers.size(); ++i) {
        if (i != 3) { result.observation.systemTimers[i] = 0; } //48D4 omits source12D4.
    }
    const auto plain = [&](const std::uint64_t count) {
        if (result.segments.empty() || result.segments.back().kind != RetryWorkKind::ordinary) {
            result.segments.push_back({});
        }
        auto& operations = result.segments.back().operations;
        if (!operations.empty() && operations.back().kind == NormalWorkKind::instructions) {
            operations.back().count += count;
        } else { operations.push_back({NormalWorkKind::instructions,count}); }
    };
    const auto child = [&](const RetryWorkKind kind, const std::uint16_t cs,
        const std::uint16_t entry, const std::uint16_t resume, const bool qualified) {
        result.segments.push_back({kind,cs,entry,resume,{},{},qualified});
    };
    plain(1); /*040C..040C.*/
    plain(8); /*330A..3318.*/
    for (const auto& entry : input.currentHighscores->entries()) {
        plain(5); /*331B..3327.*/
        if ((entry.score >> 16) == (input.score >> 16)) { plain(2); /*3329..332B.*/ }
        plain(4); /*332D..3335.*/
    }
    plain(5); /*3337..333C.*/
    plain(9); /*040F..042C.*/
    child(RetryWorkKind::releasePaletteProvider,0x1764,0x54dc,0x0431,false);
    plain(4); /*0431..0438.*/
    child(RetryWorkKind::detachLevelResourceProvider,0x0824,0xc01a,0x043d,false);
    plain(4); /*043D..0442.*/
    child(RetryWorkKind::releaseDetachedResourceProvider,0x0824,0xbfba,0x0447,false);
    plain(3); /*0447..044E.*/
    child(RetryWorkKind::releaseMusic,0x1ebc,0x018c,0x0453,false);
    plain(7); /*0453..0464.*/
    child(RetryWorkKind::stopPendingSound,0x0824,0x51a6,0x0469,true);
    plain(6); /*0469..0480.*/
    plain(2); /*0186..018B.*/
    plain(2); /*0190..0195.*/
    plain(5); /*019A..01A5.*/
    plain(7); /*4E06..4E12.*/
    child(RetryWorkKind::selectScoreFontProvider,0x1764,0x4848,0x4e17,false);
    plain(3); /*4E17..4E1F.*/
    if (input.introduction != 0) {
        plain(9); /*4E21..4E30.*/
        plain(2); /*4E71..4E77.*/
        if (input.introduction != 255) {
            plain(3); /*4E79..4E7D.*/
            result.observation.introduction = 1;
        }
    }
    //Decoded resource/model replacement is an explicit native provider.
    //This source interval ends before4F50; it owns no source audio stop.
    child(RetryWorkKind::levelResourceBootstrapProvider,0x0824,0x4e80,0x4f50,false);
    plain(1); /*4F50..4F50.*/
    child(RetryWorkKind::stopPendingSound,0x0824,0x51a6,0x4f55,true);
    plain(2); /*4F55..4F56.*/
    child(RetryWorkKind::modelResourcesProvider,0x0824,0x4a34,0x4f59,false);
    plain(2); /*4F59..4F5C.*/
    result.modelReloadSegment = result.segments.size();
    child(RetryWorkKind::modelReloadProvider,0x0824,0x48d4,0x4f5f,false);
    plain(2); /*4F5F..4F60.*/
    //Release of the temporary level screen resource, separate from model ownership.
    child(RetryWorkKind::releaseDetachedResourceProvider,0x1764,0x08f6,0x4f65,false);
    plain(3); /*4F65..4F6C.*/
    child(RetryWorkKind::levelMusicResourcesProvider,0x0824,0x6e6c,0x4f6f,false);
    plain(1); /*4F6F..4F6F.*/
    //One constructor receipt at this source boundary; no second RNG advance.
    child(RetryWorkKind::retainedConstructorCompletionProvider,0x0824,0x4f72,0x508f,false);
    plain(3); /*508F..5092.*/
    result.loaderPagesSegment = result.segments.size();
    child(RetryWorkKind::loaderPages,0x0824,0x50c4,0x5095,true);
    plain(1); /*5095..5095.*/
    //Full caller-owning helpers: no duplicate5098..50AC or01A8..01B1 work here.
    child(RetryWorkKind::musicActivation,0x0824,0x5098,0x50af,true);
    plain(6); /*50AF..50B6.*/
    plain(11); //Resident1B38 through its CALLF077A.
    child(RetryWorkKind::palette,0x3568,0x077a,0x50bb,true);
    result.segments.back().palette = PaletteRequest{0,256,0,0};
    plain(3); //Resident1B38 epilogue.
    plain(6); /*50BB..50C3.*/
    child(RetryWorkKind::directReset,0x0824,0x01a8,0x01b4,true);
    plain(3); /*01B4..01B7.*/
    child(RetryWorkKind::scoreHudProvider,0x0824,0x7970,0x01ba,false);
    plain(2); /*01BA..01BD.*/
    child(RetryWorkKind::retainedGroundInitializationProvider,0x0824,0x4c16,0x01c0,false);
    plain(1); /*01C0..01C0.*/
    child(RetryWorkKind::statusHudProvider,0x0824,0x8f1c,0x01c3,false);
    plain(2); /*01C3..01C8.*/
    plain(1); /*01CD..01CD.*/
    result.controllerRebindSegment = result.segments.size();
    //Native keyboard provider changes its loop epoch only. No source BIOS queue clear exists.
    child(RetryWorkKind::controllerRebindProvider,0x0824,0x01d0,0x01d0,false);
    return result;
}
} // namespace

Level1RetryWork level1RankMissRetryOperations(const Level1RetryProfile& input) {
    return rankMissRetryOperations(input,0);
}

Level1RetryWork level2RankMissRetryOperations(const Level1RetryProfile& input) {
    return rankMissRetryOperations(input,1);
}
} // namespace stellar7
