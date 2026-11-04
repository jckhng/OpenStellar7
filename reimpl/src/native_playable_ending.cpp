#include "stellar7/native_session.hpp"
#include "stellar7/blackout_work.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::int64_t endingWorkHorizon=120000;
constexpr std::int64_t endingDeviceHorizon=2*endingWorkHorizon;
constexpr std::size_t sceneWaitPacketBudget=1000;
constexpr std::uint16_t scenePageIrqs=24;
}

void NativeGameplaySession::waitPlayableIrqs(NativeSessionDelivery& delivery,SoundBlasterClockPhase& phase,
    const std::uint16_t rawCount,const std::size_t budget) {
    if (!playableCampaign_ || !phase.playableCampaign || budget==0 || budget>10000 || rawCount==0 || rawCount%4!=0) {
        throw std::invalid_argument("playable wait requires its retained source IRQ countdown");
    }
    const auto append=[&](ClockedPresentationWork work) {
        if (work.audio.firstSample!=delivery.firstSample+delivery.samples.size()) {
            throw std::logic_error("playable IRQ wait lost its retained PCM epoch");
        }
        phase=work.timing.phase;
        delivery.samples.insert(delivery.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
    };
    append(clock_.renderWork(phase,{{NormalWorkKind::playableIrqWaitStore,1,rawCount}},endingWorkHorizon));
    bool returned=false;
    while (!returned) {
        auto work=clock_.renderWork(phase,{{NormalWorkKind::playableIrqWaitPoll,std::min(budget,sceneWaitPacketBudget)}},
            endingWorkHorizon);
        returned=work.timing.countdownReturned;
        append(std::move(work));
        if (synth_.sampledVoiceObservation().phase==SampledVoicePhase::irqPending) {synth_.serviceSampledVoiceIrq();}
    }
}

NativeSessionDelivery NativeGameplaySession::advancePlayableEnding(const GameplayInput&,
    const std::size_t workPacketBudget) {
    if (!playableCampaign_ || !audio_->workPhase_ || !recoveredStartupPalette_ || !pixels_ || !radar_
        || level1Pages_ || !assets_.cockpitSettings || workPacketBudget==0 || workPacketBudget>10000) {
        throw std::invalid_argument("playable ending requires its retained campaign/device owners");
    }
    NativeSessionDelivery delivery;
    delivery.firstSample=clock_.elapsedSamples();
    auto phase=*audio_->workPhase_;
    phase.playableCampaign=true;
    auto palette=endingPalette_ ? *endingPalette_ : *recoveredStartupPalette_;
    const auto sourcePages=pages_.cachePage()==CockpitPageState::fromRecoveredMenu().cachePage();
    const auto pageLayout=sourcePages ? BlackoutPageLayout::recoveredStartup : BlackoutPageLayout::compatibility;
    const auto radarLayout=sourcePages ? RadarPageLayout::recoveredStartup : RadarPageLayout::compatibility;
    const auto append=[&](ClockedPresentationWork work) {
        if (work.audio.firstSample!=delivery.firstSample+delivery.samples.size()) {
            throw std::logic_error("playable ending lost its retained PCM epoch");
        }
        phase=work.timing.phase;
        delivery.samples.insert(delivery.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
    };
    const auto publishPixels=[&](const std::vector<std::uint8_t>& pixels) {
        frame_.pixels=pixels;
        frame_.palette=expandVgaDacPalette(std::vector<std::uint8_t>(palette.dac.colors.begin(),palette.dac.colors.end()));
        delivery.presentations.push_back({delivery.firstSample+delivery.samples.size(),frame_});
    };
    const auto publish=[&]() {publishPixels(*retainedPagePixels(NativePageRole::front));};
    const auto upload=[&](const std::uint8_t intensity) {
        auto work=clock_.renderPalette(phase,palette,{0,vgaPaletteColors,0,intensity},endingDeviceHorizon);
        palette=work.state;append(std::move(work.work));publish();
    };
    const auto commit=[&]() {
        audio_->workPhase_=phase;
        *recoveredStartupPalette_=palette;
        endingPalette_=palette;
    };
    const auto scenePalette=[&](const std::vector<Rgb>& colors) {
        if (colors.size()!=vgaPaletteColors) {throw std::invalid_argument("playable scene lost its palette bank");}
        std::size_t component=0;
        for (const auto color:colors) {
            for (const auto value:{color.red,color.green,color.blue}) {palette.source[component++]=value>>2;}
        }
        upload(vgaComponentMaximum);
    };
    const auto wait=[&](const std::uint16_t count) {
        if (count!=scenePageIrqs) {throw std::invalid_argument("playable scene changed its actual24 IRQ wait");}
        waitPlayableIrqs(delivery,phase,count,workPacketBudget);
    };
    if (model_.phase()==GameplayPhase::endingEntry && !ending_) {
        if (!campaignResources_ || campaignResources_->level>=assets_.campaignLevels.size() || !assets_.endingScene
            || successPalette_ || synth_.sampledVoiceObservation().phase!=SampledVoicePhase::unloaded) {
            throw std::invalid_argument("playable ending requires its actual departing bank and SCENED provider");
        }
        model_.validateEndingFirstPage();
        validateEndingSceneAssets(*assets_.endingScene);
        const auto& departing=assets_.campaignLevels[campaignResources_->level];
        if (campaignResources_->bitmaps!=&departing.bitmapGroups || campaignResources_->models!=&departing.models
            || campaignResources_->music!=&departing.musicSequence) {
            throw std::invalid_argument("playable ending changed its departing resource identities");
        }
        if (audio_->playableMusicEnabled()) {
            phase.callback=synth_.stopPlayableSequence();
            phase.callback=synth_.deselectPlayableSequence();
        } else {phase.callback=synth_.releaseSuspendedPlayableSequence();}
        synth_.releaseCampaignMusic();
        campaignResources_.reset();
        phase.sequenceSelection.reset();phase.successAudio.reset();phase.level2RetryMusic.reset();
        pages_.selectFront();
        endingAssets_=assets_.endingScene;
        ending_.emplace(endingAssets_->scene,RadarPages{pages_.cachePage(),pages_.backPage(),pages_.frontPage(),
            pages_.drawPage(),pages_.copyPage()},0,0);
        pageProjection_=NativePageProjection::retainedEndingEntry;
        commit();
        return delivery;
    }
    if (model_.phase()==GameplayPhase::endingEntry || model_.phase()==GameplayPhase::endingPresentation) {
        if (!ending_ || !endingAssets_ || campaignResources_) {
            throw std::invalid_argument("playable scene requires its retained interpreter and released campaign banks");
        }
        const auto first=model_.phase()==GameplayPhase::endingEntry;
        if (first) {model_.validateEndingFirstPage();}
        else {model_.validateEndingPage();}
        auto interpreter=*ending_;
        const auto work=interpreter.execute(first ? 0 : model_.presentationFrame());
        if ((first && (!work.present || work.delayIrqCounts!=0))
            || (!first && work.delayIrqCounts!=scenePageIrqs)) {
            throw std::invalid_argument("playable SCENED changed its page/countdown contract");
        }
        std::vector<std::vector<IndexedPageEffect>> effects(work.operations.size());
        for (std::size_t index=0;index<work.operations.size();++index) {
            const auto& operation=work.operations[index];
            if (!std::holds_alternative<ScenePaletteChange>(operation) && !std::holds_alternative<SceneSoundRequest>(operation)) {
                effects[index]=scenePagePixelEffects(operation,endingAssets_->scene,assets_.scoreFont,pages_.backPage());
            }
        }
        pages_.beginModal();pages_.selectCockpitCopy();
        if (first) {
            applySceneFrontWipe(*pixels_,pages_.frontPage(),radarLayout);
            upload(0);
        }
        for (std::size_t index=0;index<work.operations.size();++index) {
            const auto& operation=work.operations[index];
            if (const auto* change=std::get_if<ScenePaletteChange>(&operation)) {
                if (!first) {throw std::invalid_argument("playable timed scene changed its resident palette");}
                if (change->operation==ScenePaletteOperation::load) {
                    if (!change->asset) {throw std::invalid_argument("playable scene palette lacks an asset");}
                    scenePalette(std::get<std::vector<Rgb>>(endingAssets_->scene.assets.at(*change->asset)));
                }
                continue;
            }
            if (const auto* sound=std::get_if<SceneSoundRequest>(&operation)) {
                if (!audio_->playableMusicEnabled()) {continue;}
                if (sound->operation==SceneSoundOperation::loadVoice && first && sound->argument==3) {
                    std::copy(assets_.cockpitDacPalette.begin(),assets_.cockpitDacPalette.end(),palette.source.begin());
                    upload(vgaComponentMaximum);
                    synth_.loadPlayableSampledVoice(endingAssets_->sampledVoice);
                    scenePalette(std::get<std::vector<Rgb>>(endingAssets_->scene.assets.at(0)));
                    pixels_->apply(std::vector<IndexedPageEffect>{RadarPageCopy{pages_.cachePage(),pages_.backPage(),
                        {0,0},320,200}});
                } else if (sound->operation==SceneSoundOperation::playSequence && sound->argument==0
                    && model_.presentationFrame()==1) {
                    synth_.startSampledVoice();phase.callback=synth_.callbackWorkState();sceneVoiceSegment_=0;
                } else {throw std::invalid_argument("playable scene changed its source voice ordering");}
                continue;
            }
            if (first && std::holds_alternative<SceneBackgroundDraw>(operation)) {
                applyBlackoutClear(*pixels_,pages_.backPage(),pageLayout);
            }
            pixels_->apply(effects[index]);
        }
        pages_.selectArmorCopy();
        *ending_=std::move(interpreter);
        if (first) {model_.finishEndingFirstPage();}
        else {
            wait(work.delayIrqCounts);
            model_.finishEndingPage(work.present ? EndingPageResult::present : EndingPageResult::exhausted);
        }
        pageProjection_=NativePageProjection::retainedEndingPage;
        publish();commit();
        return delivery;
    }
    if (model_.phase()==GameplayPhase::endingTeardown) {
        model_.validateEndingTeardown();
        if (!ending_ || !endingAssets_ || campaignResources_) {
            throw std::invalid_argument("playable ending teardown lost its retained owners");
        }
        if (synth_.sampledVoiceObservation().phase==SampledVoicePhase::irqPending) {synth_.serviceSampledVoiceIrq();}
        if (synth_.sampledVoiceObservation().phase!=SampledVoicePhase::unloaded) {
            synth_.stopSampledVoice();synth_.releaseSampledVoice();
        }
        sceneVoiceSegment_=initialSceneVoiceSegment;
        ending_.reset();endingAssets_.reset();
        const auto fade=[&]() {
            const auto ownedFront=*retainedPagePixels(NativePageRole::front);
            auto work=clock_.renderBlackout(phase,{palette,{}},pages_,*pixels_,endingDeviceHorizon,pageLayout);
            for (auto& step:work.steps) {
                append(std::move(step.before));palette=step.palette.state;append(std::move(step.palette.work));publishPixels(ownedFront);
            }
            palette=work.state.palette;append(std::move(work.clear));publish();
        };
        fade();
        pages_.beginModal();applyBlackoutClear(*pixels_,pages_.backPage(),pageLayout);pages_.flip();
        const PageFlipClockState flip{PageFlipSynchronization::enabled,pages_.frontPage(),pages_.backPage(),0x3d4,
            sourcePages ? PageFlipLayout::recoveredStartup : PageFlipLayout::compatibility,PageFlipWaitDomain::retainedProgram};
        auto flipped=clock_.renderPageFlip(phase,flip,endingDeviceHorizon);
        append(std::move(flipped.work));publish();
        std::copy(assets_.cockpitDacPalette.begin(),assets_.cockpitDacPalette.end(),palette.source.begin());
        upload(vgaComponentMaximum);fade();
        model_.finishEndingTeardown();
        pageProjection_=NativePageProjection::retainedEndingPage;
        commit();
        return delivery;
    }
    throw std::invalid_argument("playable ending return requires its retained title/highscore provider");
}
} // namespace stellar7
