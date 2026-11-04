#include "playable_story.hpp"
#include "story_audio_delivery.hpp"
#include "stellar7/blackout_work.hpp"
#include <algorithm>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::int64_t workHorizon=120000;
constexpr std::int64_t deviceHorizon=2*workHorizon;
constexpr std::size_t nativeSpeechPollIterations=512;
constexpr std::uint16_t firstSceneEffect=27;
constexpr std::uint16_t secondSceneEffect=28;
}
PlayableStoryPresenter::PlayableStoryPresenter(const StorySceneAssets& assets,const StoryScene scene,const RadarPages pages)
    :assets_(&assets),controller_(assets,scene,pages) {}

NativeSessionDelivery PlayableStoryPresenter::advance(PlayableStoryOwners o,const PlayableStoryInput input,
    const std::size_t workPacketBudget,const NativeDeliverySink& sink) {
    if (result_!=StorySceneResult::playing || workPacketBudget==0 || workPacketBudget>10000
        || o.cockpitPalette.size()!=vgaPaletteBytes || !o.phase.timer || !o.phase.callback) {
        throw std::invalid_argument("Story presenter requires its retained native owners and active scene");
    }
    NativeSessionDelivery delivery;delivery.firstSample=o.clock.elapsedSamples();
    StoryDeliveryStream stream(sink);
    const auto append=[&](ClockedPresentationWork work) {
        if(work.audio.firstSample!=delivery.firstSample+delivery.samples.size()) {throw std::logic_error("Story lost retained PCM epoch");}
        o.phase=work.timing.phase;delivery.samples.insert(delivery.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
        if(o.synth.sampledVoiceObservation().phase==SampledVoicePhase::irqPending) {o.synth.serviceSampledVoiceIrq();}
        stream.publish(delivery);
    };
    const auto publish=[&](const std::vector<std::uint8_t>& pixels) {
        delivery.presentations.push_back({o.clock.elapsedSamples(),{pixels,expandVgaDacPalette(
            std::vector<std::uint8_t>(o.palette.dac.colors.begin(),o.palette.dac.colors.end()))}});
    };
    const auto current=[&]() {return o.pixels.snapshot(o.pages.frontPage());};
    const auto deviceBudget=[&]() {return o.phase.callback && o.phase.callback->program ? deviceHorizon : workHorizon;};
    const auto flipDomain=[&]() {return o.phase.callback && o.phase.callback->program
        ? PageFlipWaitDomain::retainedProgram : PageFlipWaitDomain::bounded;};
    const auto upload=[&](const std::uint8_t intensity) {
        auto work=o.clock.renderPalette(o.phase,o.palette,{0,vgaPaletteColors,0,intensity},deviceBudget());
        o.palette=work.state;append(std::move(work.work));publish(current());
    };
    const auto selectPalette=[&](const std::vector<Rgb>& colors) {
        if(colors.size()!=vgaPaletteColors) {throw std::invalid_argument("Story palette bank is incomplete");}
        std::size_t component=0;
        for(const auto c:colors) {for(const auto value:{c.red,c.green,c.blue}) {o.palette.source[component++]=value>>2;}}
        upload(vgaComponentMaximum);
    };
    const auto voiceIdle=[&]() {
        const auto phase=o.synth.sampledVoiceObservation().phase;
        return phase==SampledVoicePhase::unloaded || phase==SampledVoicePhase::loaded
            || phase==SampledVoicePhase::complete || phase==SampledVoicePhase::stopped;
    };
    const auto voicePoll=[&]() {
        //Source B4DA..B4E2 busy descriptor poll. This fixed transport batch does
        //not qualify original DMA completion scheduling or provider elapsed time.
        append(o.clock.renderWork(o.phase,{{NormalWorkKind::instructions,3*nativeSpeechPollIterations}},workHorizon));
    };
    const auto releaseVoice=[&]() {
        if(o.synth.sampledVoiceObservation().phase==SampledVoicePhase::irqPending) {o.synth.serviceSampledVoiceIrq();}
        if(o.synth.sampledVoiceObservation().phase!=SampledVoicePhase::unloaded) {
            o.synth.stopSampledVoice();o.synth.releaseSampledVoice();
        }
        loadedSegment_.reset();currentSegment_=initialSceneVoiceSegment;
    };
    const auto blackout=[&]() {
        const auto pixels=current();auto work=o.clock.renderBlackout(o.phase,{o.palette,{}},o.pages,o.pixels,
            deviceBudget(),BlackoutPageLayout::recoveredStartup);
        for(auto& step:work.steps) {
            append(std::move(step.before));o.palette=step.palette.state;append(std::move(step.palette.work));publish(pixels);
        }
        o.palette=work.state.palette;append(std::move(work.clear));publish(current());
    };
    if(!entered_) {
        if(input.keyScan!=0 || input.buttonHeld) {voicePoll();stream.publish(delivery);return delivery;}
        if(o.synth.sampledVoiceObservation().phase!=SampledVoicePhase::unloaded) {
            throw std::invalid_argument("Story entry requires released prior scene speech");
        }
        //The caller releases the departing bank after the scene returns. The
        //borrow and physical device epoch persist while the current song stops.
        if(o.synth.driverState().sequenceSelected) {
            o.phase.callback=o.synth.stopPlayableSequence();o.phase.callback=o.synth.deselectPlayableSequence();
        } else if(o.synth.driverState().sequenceAllocated) {
            o.phase.callback=o.phase.callback->domain==SoundBlasterCallbackDomain::noHandle
                ? o.synth.releasePlayableStoryLogo() : o.synth.releaseSuspendedPlayableSequence();
        }
        if(o.caller!=PlayableStoryCaller::menuAttract) {
            o.phase.sequenceSelection.reset();o.phase.successAudio.reset();o.phase.level2RetryMusic.reset();
        }
        o.phase.playableCampaign=true;
        entered_=true;
    }
    auto key=input.keyScan;
    if(!input.buttonHeld) {buttonHeld_=false;}
    else if(!buttonHeld_) {key=0x39;buttonHeld_=true;}
    if(key==1 && step_) {
        step_=controller_.advance(key);operation_=0;
    }
    if(pendingResult_!=StorySceneResult::playing) {
        if(input.keyScan!=0 || input.buttonHeld) {voicePoll();stream.publish(delivery);return delivery;}
        releaseVoice();
        if(backgroundStarted_) {
            if(o.synth.driverState().sequenceSelected) {
                o.phase.callback=o.synth.stopPlayableSequence();
                o.phase.callback=o.synth.deselectPlayableSequence();
            } else {
                o.phase.callback=o.synth.releaseSuspendedPlayableSequence();
            }
            backgroundStarted_=false;
        }
        blackout();
        o.pages.beginModal();applyBlackoutClear(o.pixels,o.pages.backPage(),BlackoutPageLayout::recoveredStartup);o.pages.flip();
        auto flipped=o.clock.renderPageFlip(o.phase,{PageFlipSynchronization::enabled,o.pages.frontPage(),o.pages.backPage(),
            0x3d4,PageFlipLayout::recoveredStartup,flipDomain()},deviceBudget());
        append(std::move(flipped.work));publish(current());
        std::copy(o.cockpitPalette.begin(),o.cockpitPalette.end(),o.palette.source.begin());upload(vgaComponentMaximum);blackout();
        result_=pendingResult_;stream.publish(delivery);return delivery;
    }
    if(!step_) {
        step_=controller_.advance(key==1 ? key : voiceAdvance_ ? 0x39 : key);operation_=0;voiceAdvance_=false;
    }
    for(;operation_<step_->work.operations.size();++operation_) {
        const auto& operation=step_->work.operations[operation_];
        if(const auto* change=std::get_if<ScenePaletteChange>(&operation)) {
            if(change->operation==ScenePaletteOperation::selectBank) {
                applySceneFrontWipe(o.pixels,o.pages.frontPage(),RadarPageLayout::recoveredStartup);upload(0);
            }
            if(change->operation==ScenePaletteOperation::load) {
                if(!change->asset) {throw std::invalid_argument("Story palette lacks source asset");}
                palette_=std::get<std::vector<Rgb>>(controller_.graphics().assets.at(*change->asset));selectPalette(*palette_);
            }
            continue;
        }
        if(const auto* sound=std::get_if<SceneSoundRequest>(&operation)) {
            if(sound->operation==SceneSoundOperation::loadVoice) {
                if(!o.musicEnabled) {continue;}
                if(sound->argument==0 || sound->argument>assets_->voices.size()) {throw std::invalid_argument("Story voice selector outside resource");}
                if(!voiceIdle()) {voicePoll();stream.publish(delivery);return delivery;}
                releaseVoice();voiceGroup_=sound->argument-1;
                std::copy(o.cockpitPalette.begin(),o.cockpitPalette.end(),o.palette.source.begin());upload(vgaComponentMaximum);
                o.synth.loadPlayableSampledVoice(assets_->voices.at(*voiceGroup_).at(0));loadedSegment_=0;
                if(!palette_) {throw std::invalid_argument("Story voice load lacks retained scene palette");}
                selectPalette(*palette_);
                o.pixels.apply(std::vector<IndexedPageEffect>{RadarPageCopy{o.pages.cachePage(),o.pages.backPage(),{0,0},320,200}});
                continue;
            }
            const auto segment=static_cast<std::int16_t>(sound->argument);
            if(segment<0) {
                if(o.soundsEnabled) {o.phase.callback=o.synth.triggerPlayableEvent(segment==-1 ? firstSceneEffect : secondSceneEffect);}
                continue;
            }
            if(!o.musicEnabled || !voiceGroup_ || currentSegment_==segment) {continue;}
            if(static_cast<std::size_t>(segment)>=assets_->voices.at(*voiceGroup_).size()) {
                throw std::invalid_argument("Story speech segment outside selected source SSM");
            }
            if(!voiceIdle()) {voicePoll();stream.publish(delivery);return delivery;}
            if(!loadedSegment_ || *loadedSegment_!=static_cast<std::size_t>(segment)) {
                releaseVoice();o.synth.loadPlayableSampledVoice(assets_->voices.at(*voiceGroup_).at(segment));loadedSegment_=segment;
            }
            o.synth.startSampledVoice();o.phase.callback=o.synth.callbackWorkState();currentSegment_=segment;
            continue;
        }
        applyScenePageOperation(o.pixels,operation,controller_.graphics(),o.font,o.pages.backPage(),captures_);
    }
    //1110 executes page0 before120C tests the sampled-voice handle. B1 owns
    //speech here; the other composite bridge scripts select DRAXON descriptor0.
    if(!backgroundStarted_ && controller_.composite() && step_->pages==std::vector<std::size_t>{0}
        && o.synth.sampledVoiceObservation().phase==SampledVoicePhase::unloaded
        && !assets_->backgroundSequence.empty()) {
        o.phase.callback=o.synth.selectPlayableSequence(assets_->backgroundSequence,PlayableSequenceMode::repeat,
            o.musicEnabled ? PlayableMusicPublication::current : PlayableMusicPublication::suspended);
        backgroundStarted_=true;
    }
    publish(current());
    const auto count=step_->waitRawIrqs;
    if(controller_.composite() && step_->pages.size()>1 && count==0) {
        append(o.clock.renderWork(o.phase,{{NormalWorkKind::playableStoryWaitStore,1,0}},workHorizon));
    }
    if(count!=0) {
        if(controller_.composite()) {
            append(o.clock.renderWork(o.phase,{{NormalWorkKind::playableStoryWaitStore,1,count}},workHorizon));
            bool returned=false;
            while(!returned) {
                auto work=o.clock.renderWork(o.phase,{{NormalWorkKind::playableStoryWaitPoll,nativeSpeechPollIterations}},workHorizon);
                returned=work.timing.countdownReturned;append(std::move(work));
                const auto voice=o.synth.sampledVoiceObservation();
                if(!returned && voice.phase==SampledVoicePhase::complete && currentSegment_>=0) {
                    //1110:135A/B5B8 sets the next key to Space and repeat10000.
                    //Observe the native PCM completion at this fixed transport
                    //boundary; original DMA/foreground phase remains unqualified.
                    voiceAdvance_=true;break;
                }
            }
        } else {
            append(o.clock.renderWork(o.phase,{{NormalWorkKind::playableIrqWaitStore,1,count}},workHorizon));
            bool returned=false;
            while(!returned) {
                auto work=o.clock.renderWork(o.phase,{{NormalWorkKind::playableIrqWaitPoll,nativeSpeechPollIterations}},workHorizon);
                returned=work.timing.countdownReturned;append(std::move(work));
            }
        }
    }
    if(step_->result!=StorySceneResult::playing) {pendingResult_=step_->result;}
    step_.reset();stream.publish(delivery);return delivery;
}
}
