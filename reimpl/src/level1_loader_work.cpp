#include "stellar7/level1_loader_work.hpp"
#include "stellar7/audio_program_work.hpp"
#include "stellar7/indexed_render.hpp"
#include "stellar7/ui_text_work.hpp"
#include "stellar7/ui_fill_work.hpp"
#include "playable_detail_render.hpp"

#include <stdexcept>
#include <algorithm>
#include <utility>
#include <optional>

namespace stellar7 {
namespace {
constexpr std::int64_t activationHorizon = 120000;
constexpr std::uint8_t level1SongOrdinal = 2;

template<class Work, class Prepare>
SoundBlasterClockPhase executeActivation(SoundBlasterClockPhase phase,
    const DriverSequenceWork& activation, const SoundBlasterCallbackState& prepared,
    Work work, Prepare prepare) {
    const auto run = [&](const std::vector<NormalWorkOperation>& operations) { phase = work(phase,operations); };
    //5098..509F; 02E6 index0/head/driver guards; 0478 and driver entry through op6 CALL17E5.
    run({{NormalWorkKind::instructions,4 + 19 + 11 + 12}});
    run(activation.operations);
    run({{NormalWorkKind::instructions,9 + 5}}); //Dispatcher and0478 return.
    prepare();
    phase.callback = applyDriverSequenceCallbackState(*phase.callback,activation);
    phase.sequenceSelection = AudioClockSequenceSelectionPhase{prepared};
    phase.sequenceSelection->songOrdinal = level1SongOrdinal;
    run({{NormalWorkKind::instructions,3}, //031B/031E/0323.
        {NormalWorkKind::soundBlasterSequencePublish,1,level1SongOrdinal}, //0325.
        {NormalWorkKind::instructions,5 + 3 + 1}}); //0329..032F;50A4..50AC stores loop0.
    return phase;
}
}

SoundBlasterClockPhase level1MusicActivationPhase(const SoundBlasterClockPhase& phase,
    const SoundBlasterCallbackState& prepared) {
    if (!phase.sequenceSelection || phase.sequenceSelection->songOrdinal != 1
        || !phase.sequenceSelection->beginCurrentCleared || !phase.timer || phase.timer->globalCount != 0
        || !phase.callback || phase.callback->domain != SoundBlasterCallbackDomain::noHandle) {
        throw std::invalid_argument("Level1 music preflight requires the completed Begin owner");
    }
    const auto activation = soundBlasterSequenceOperations(prepared.sequence,DriverSequenceAdmission::inheritedChannels);
    return executeActivation(phase,activation,prepared,[](const auto& incoming, const auto& operations) {
        return normalWorkTiming(incoming,operations,activationHorizon).phase;
    },[]() {});
}

Level1MusicActivation activateLevel1Music(PresentationClock& clock, SoundBlasterSynth& synth,
    const SoundBlasterClockPhase& phase) {
    if (!phase.sequenceSelection || phase.sequenceSelection->songOrdinal != 1
        || !phase.sequenceSelection->beginCurrentCleared || !phase.timer || phase.timer->globalCount != 0
        || !phase.callback || phase.callback->domain != SoundBlasterCallbackDomain::noHandle) {
        throw std::invalid_argument("Level1 music requires the retained stopped Begin owner");
    }
    const auto prepared = synth.previewRecoveredLevel1();
    auto activation = soundBlasterSequenceOperations(prepared.sequence,DriverSequenceAdmission::inheritedChannels);
    level1MusicActivationPhase(phase,prepared);
    Level1MusicActivation result{std::move(activation),{},phase,clock.elapsedSamples(),{}};
    result.phase = executeActivation(phase,result.activation,prepared,
        [&](const auto& incoming, const auto& operations) {
            auto work = clock.renderWork(incoming,operations,activationHorizon);
            result.samples.insert(result.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
            const auto next = work.timing.phase;
            result.work.push_back(std::move(work));
            return next;
        },[&]() { synth.prepareRecoveredLevel1(); });
    return result;
}

SoundBlasterClockPhase level1RetryMusicStopPhase(const SoundBlasterClockPhase& phase) {
    return normalWorkTiming(phase,{{NormalWorkKind::level1RetryMusicStop,1}},activationHorizon).phase;
}

Level1RetryMusicStop stopLevel1RetryMusic(PresentationClock& clock, SoundBlasterSynth& synth,
    const SoundBlasterClockPhase& phase) {
    const auto live = synth.callbackWorkState();
    if (!phase.callback || live.domain != phase.callback->domain) {
        throw std::invalid_argument("retry stop lost its retained live callback owner");
    }
    level1RetryMusicStopPhase(phase);
    auto work = clock.renderWork(phase,{{NormalWorkKind::level1RetryMusicStop,1}},activationHorizon);
    Level1RetryMusicStop result{{},work.timing.phase,work.audio.firstSample,work.audio.samples};
    result.work.push_back(std::move(work));
    return result;
}

SoundBlasterClockPhase level1RetryMusicActivationPhase(const SoundBlasterClockPhase& phase,
    const SoundBlasterCallbackState& prepared) {
    if (!phase.sequenceSelection || phase.sequenceSelection->songOrdinal != level1SongOrdinal
        || !phase.sequenceSelection->published || !phase.sequenceSelection->level1RetryCurrentCleared
        || !phase.timer || !phase.callback || phase.callback->domain != SoundBlasterCallbackDomain::noHandle) {
        throw std::invalid_argument("Level1 retry activation requires its actual retained stop");
    }
    const auto activation = soundBlasterSequenceOperations(prepared.sequence,DriverSequenceAdmission::inheritedChannels);
    return executeActivation(phase,activation,prepared,[](const auto& incoming, const auto& operations) {
        return normalWorkTiming(incoming,operations,activationHorizon).phase;
    },[]() {});
}

Level1MusicActivation activateLevel1RetryMusic(PresentationClock& clock, SoundBlasterSynth& synth,
    const SoundBlasterClockPhase& phase) {
    const auto prepared = synth.previewRecoveredLevel1Retry();
    auto activation = soundBlasterSequenceOperations(prepared.sequence,DriverSequenceAdmission::inheritedChannels);
    level1RetryMusicActivationPhase(phase,prepared);
    Level1MusicActivation result{std::move(activation),{},phase,clock.elapsedSamples(),{}};
    result.phase = executeActivation(phase,result.activation,prepared,
        [&](const auto& incoming, const auto& operations) {
            auto work = clock.renderWork(incoming,operations,activationHorizon);
            result.samples.insert(result.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
            const auto next = work.timing.phase;
            result.work.push_back(std::move(work));
            return next;
        },[&]() { synth.prepareRecoveredLevel1Retry(); });
    return result;
}

SoundBlasterClockPhase level1DirectResetPhase(const SoundBlasterClockPhase& initial) {
    if (!initial.sequenceSelection || initial.sequenceSelection->songOrdinal != level1SongOrdinal
        || !initial.sequenceSelection->published || initial.sequenceSelection->level1DirectResetCompleted
        || !initial.callback || initial.callback->domain != SoundBlasterCallbackDomain::sequence
        || !initial.callback->program || !initial.callback->active
        || initial.callback->program->handleStatus != 0
        || std::any_of(initial.callback->voices.directTimers.begin(),initial.callback->voices.directTimers.end(),
            [](const auto timer) { return timer != 0; })) {
        throw std::invalid_argument("Level1 reset requires the selected new gameplay allocation");
    }
    const auto reset = soundBlasterDirectResetOperations();
    const std::vector<NormalWorkOperation> prefix{{NormalWorkKind::instructions,3 + 11 + 12}};
    const std::vector<NormalWorkOperation> suffix{{NormalWorkKind::instructions,9 + 5 + 1}};
    auto phase = normalWorkTiming(initial,prefix,activationHorizon).phase;
    const auto body = normalWorkTiming(phase,reset.operations,activationHorizon);
    if (!body.callbacks.empty()) {
        throw std::invalid_argument("Level1 reset callback observer requires ordered partial driver writes");
    }
    phase = body.phase;
    phase.callback = soundBlasterDirectResetState(*phase.callback);
    phase.sequenceSelection->level1DirectResetCompleted = true;
    return normalWorkTiming(phase,suffix,activationHorizon).phase;
}

Level1DirectReset resetLevel1DirectEffects(PresentationClock& clock, SoundBlasterSynth& synth,
    const SoundBlasterClockPhase& initial) {
    synth.previewRecoveredLevel1Reset();
    level1DirectResetPhase(initial);
    const auto reset = soundBlasterDirectResetOperations();
    const std::vector<NormalWorkOperation> prefix{{NormalWorkKind::instructions,3 + 11 + 12}};
    const std::vector<NormalWorkOperation> suffix{{NormalWorkKind::instructions,9 + 5 + 1}};

    Level1DirectReset result{{},initial,clock.elapsedSamples(),{}};
    const auto run = [&](const auto& operations) {
        auto work = clock.renderWork(result.phase,operations,activationHorizon);
        result.phase = work.timing.phase;
        result.samples.insert(result.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
        result.work.push_back(std::move(work));
    };
    run(prefix);
    run(reset.operations);
    result.phase.callback = synth.completeRecoveredLevel1Reset();
    result.phase.sequenceSelection->level1DirectResetCompleted = true;
    run(suffix);
    return result;
}

enum class LoaderDetailDomain { bounded, full };

static Level1LoaderPages level1LoaderPages(const GameplayAssets& assets, const GameplayModel& model,
    const CockpitPageState& incomingPages, const IndexedPages& incomingPixels,
    const Level1LoaderPageDomain domain, const std::optional<std::int16_t> playableDetail,
    const LoaderDetailDomain detailDomain = LoaderDetailDomain::bounded,
    const NativeLevel1BeginPages beginPages = NativeLevel1BeginPages::ordinary) {
    constexpr std::size_t panelGroup = 3;
    constexpr std::size_t overlayGroup = 4;
    constexpr Viewport display{0,319,0,199};
    constexpr std::uint8_t titleColor = 239;
    const auto& record = model.lastCockpitInitialization();
    if ((domain != Level1LoaderPageDomain::freshBegin && domain != Level1LoaderPageDomain::recoveredRetry)
        || (beginPages != NativeLevel1BeginPages::ordinary && beginPages != NativeLevel1BeginPages::storyTransferred)
        || (beginPages == NativeLevel1BeginPages::storyTransferred && domain != Level1LoaderPageDomain::freshBegin)) {
        throw std::invalid_argument("unsupported Level1 loader page domain");
    }
    const bool retry = domain == Level1LoaderPageDomain::recoveredRetry;
    const bool sourceCut = retry
        ? model.phase() == GameplayPhase::level1RetryHandoff && model.level1State().levelIndex() == 0
            && model.level1State().loopTick() == 0
            && model.level1State().activeListOrder().empty() && model.level1State().semanticActiveListOrder().empty()
        : model.phase() == GameplayPhase::level1 && model.ticks() == 0;
    const bool displayRoles = (incomingPages.frontPage() == 0xa000 && incomingPages.backPage() == 0xa800)
        || ((retry || beginPages == NativeLevel1BeginPages::storyTransferred)
            && incomingPages.frontPage() == 0xa800 && incomingPages.backPage() == 0xa000);
    if (!assets.cockpitSettings || assets.artGroups.size() <= overlayGroup
        || assets.artGroups[panelGroup].size() < 3 || assets.artGroups[overlayGroup].size() < 7
        || !sourceCut || record.levelIndex != 0 || record.displaySteps != 2
        || incomingPages.cachePage() != 0xa400 || !displayRoles) {
        throw std::invalid_argument("Level1 loader pages require the fresh retained resource/profile cut");
    }
    Level1LoaderPages plan{incomingPages,incomingPixels,RadarState(assets.artGroups[panelGroup]),{},{},{}};
    auto& bitmaps = plan.bitmaps;
    bitmaps.strip = assets.artGroups[panelGroup][1];
    bitmaps.matchingPageBackground = assets.artGroups[panelGroup][2];
    bitmaps.indicator = assets.artGroups[overlayGroup][6];
    for (std::size_t index = 0; index < bitmaps.overlays.size(); ++index) {
        bitmaps.overlays[index] = assets.artGroups[overlayGroup][index];
    }
    for (std::size_t index = 0; index < plan.cockpit.size(); ++index) {
        const auto state = record.states[index];
        const auto cockpit = detailDomain == LoaderDetailDomain::full ? playableFullCockpitOperations
            : playableDetail ? playableCockpitOperations : cockpitOperations;
        plan.cockpit[index] = cockpit({state.x,state.velocity,playableDetail.value_or(assets.cockpitSettings->viewSize),
            index == 0 ? CockpitPageRelation::matching : CockpitPageRelation::differing},bitmaps);
        if (plan.cockpit[index].state.indicatorX != record.states[index+1].x
            || plan.cockpit[index].state.indicatorVelocity != record.states[index+1].velocity) {
            throw std::logic_error("Level1 loader cockpit differs from its canonical provider record");
        }
    }
    const auto append = [&](const auto& operations) {
        plan.operations.insert(plan.operations.end(),operations.begin(),operations.end());
    };
    const auto plain = [&](const std::uint64_t count) { plan.operations.push_back({NormalWorkKind::instructions,count}); };
    const auto fullCopy = [&](const std::uint16_t source, const std::uint16_t destination) {
        plain(5 + 10 + 1); //50D3/5104 through8484 and22A1 JMPF.
        append(planarCopyOperations(0,320,200));
        plan.pixels.apply(std::vector<IndexedPageEffect>{RadarPageCopy{source,destination,{0,0},320,200}});
    };
    plan.pixels.apply(cockpitResourcePageEffects(assets.cockpit,plan.pages.cachePage(),CockpitPageLayout::recoveredStartup));
    plain(4); //50C4..50CA.
    plan.pages.selectCockpitDraw();
    plan.pages.selectCockpitCopy();
    append(plan.cockpit[0].operations);
    plan.pixels.apply(cockpitPageEffects(plan.cockpit[0],bitmaps,plan.pages.cachePage(),plan.pages.drawPage()));
    plain(1 + 11); //50D0/7936 through width CALLF.
    const auto width = uiTextWidthOperations(assets.scoreFont,UiFontLayout::proportional,"SOL");
    append(width.operations);
    plain(7);
    const auto title = uiTextOperations(assets.scoreFont,UiFontLayout::proportional,"SOL",
        {static_cast<std::int16_t>(160-width.width/2),30},titleColor,plan.pages.cachePage(),display);
    append(title.operations);
    plan.pixels.apply(title.effects);
    plain(3);
    plan.pages.beginFrame();
    fullCopy(plan.pages.cachePage(),plan.pages.backPage());
    plain(11); //50E2..50F7.
    const auto clear = uiFillOperations({24,24,272,122},0,plan.pages.backPage());
    append(clear.operations);
    plan.pixels.apply(clear.effects);
    plain(1); //50FC.
    append(plan.cockpit[1].operations);
    plan.pixels.apply(cockpitPageEffects(plan.cockpit[1],bitmaps,plan.pages.cachePage(),plan.pages.backPage()));
    for (const auto& actor : model.level1State().entities()) { plan.radar.initializeAllocation(actor.allocationIndex); }
    //48D4 clears every FC projection on load/retry;4580 later preserves reuse.
    constexpr std::size_t effectAllocationBase = 32;
    for (std::size_t slot = 0; slot < Level1State::deathEffectPoolSize; ++slot) {
        plan.radar.initializeAllocation(effectAllocationBase + slot);
    }
    const auto& player = model.level1State().player();
    const auto radar = plan.radar.initializeLevel1(model.playerArmor(),player.position,player.yaw,
        {plan.pages.cachePage(),plan.pages.backPage(),plan.pages.frontPage(),plan.pages.drawPage(),plan.pages.copyPage()});
    append(radar.operations);
    plan.pixels.apply(radar);
    plan.pages.selectArmorCopy();
    fullCopy(plan.pages.backPage(),plan.pages.frontPage());
    plain(5); //5113..512D skips optional upload and returns.
    return plan;
}

Level1LoaderPages planLevel1LoaderPages(const GameplayAssets& assets, const GameplayModel& model,
    const CockpitPageState& pages, const IndexedPages& pixels, const Level1LoaderPageDomain domain) {
    return level1LoaderPages(assets,model,pages,pixels,domain,std::nullopt);
}

Level1LoaderPages planPlayableLevel1LoaderPages(const GameplayAssets& assets, const GameplayModel& model,
    const CockpitPageState& pages, const IndexedPages& pixels, const std::int16_t detail,
    const Level1LoaderPageDomain domain) {
    return level1LoaderPages(assets,model,pages,pixels,domain,detail);
}

Level1LoaderPages planStoryLevel1LoaderPages(const GameplayAssets& assets, const GameplayModel& model,
    const CockpitPageState& pages, const IndexedPages& pixels, const std::int16_t detail) {
    return level1LoaderPages(assets,model,pages,pixels,Level1LoaderPageDomain::freshBegin,detail,
        LoaderDetailDomain::bounded,NativeLevel1BeginPages::storyTransferred);
}

Level1LoaderPages planFullDetailPlayableLevel1LoaderPages(const GameplayAssets& assets, const GameplayModel& model,
    const CockpitPageState& pages, const IndexedPages& pixels, const std::int16_t detail,
    const Level1LoaderPageDomain domain, const NativeLevel1BeginPages beginPages) {
    return level1LoaderPages(assets,model,pages,pixels,domain,detail,LoaderDetailDomain::full,beginPages);
}

} // namespace stellar7
