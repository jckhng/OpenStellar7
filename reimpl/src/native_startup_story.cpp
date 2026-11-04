#include "stellar7/native_startup.hpp"
#include "playable_story.hpp"
#include "playable_attract.hpp"
#include "story_audio_delivery.hpp"
#include "stellar7/ui_text_work.hpp"
#include "stellar7/ui_fill_work.hpp"
#include <algorithm>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::string_view opening="SOMEWHERE IN THE HEART OF THE$ARCTURAN EMPIRE ...."; //DS1126.
constexpr std::uint8_t typewriterColor=148; //Immutable DS12EB in this source's VGA UI table.
constexpr std::int64_t workHorizon=120000;
constexpr std::int64_t deviceHorizon=2*workHorizon;
constexpr std::uint64_t nativeTitleMinimumSamples=SoundBlasterSynth::sampleRate;
constexpr std::uint64_t nativeTitlePollSamples=SoundBlasterSynth::sampleRate/100;
constexpr std::uint64_t nativeTitleIdleDispatches=256;
enum class IntroReturnPalette { select, inherited };
}
bool NativeStartupSequence::storyActive() const noexcept {
    return phase_==NativeStartupPhase::awaitingIntroInput && audio_
        && hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::storyPanels);
}
std::size_t NativeStartupSequence::storyPage() const noexcept {return story_ ? story_->page() : 0;}
SampledVoiceObservation NativeStartupSequence::storyVoice() const noexcept {
    return audio_ ? audio_->synth_.sampledVoiceObservation() : SampledVoiceObservation{SampledVoicePhase::unloaded,0,0,0,0,128,false};
}
NativeSessionDelivery NativeStartupSequence::pollStoryIntro(const NativeMenuInput input,const std::size_t workPacketBudget,const NativeDeliverySink& sink) {
    if(!storyActive() || !game_.storyScenes || !audio_->settings_ || !audio_->fonts_ || !audio_->workPhase_
        || !audio_->presentation_ || !audio_->logoRelease_ || workPacketBudget==0 || workPacketBudget>10000) {
        throw std::invalid_argument("Story intro requires retained post-logo owners and complete resources");
    }
    const auto setting=audio_->settings_->settings.configuration.options[5];
    if(setting!=0 && setting!=255) {throw std::invalid_argument("Story intro admits source option0/255");}
    const bool fullPresentation=hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::automaticAttract);
    auto returnPalette=game_.cockpitDacPalette;
    if(fullPresentation) {
        returnPalette.clear();
        for(const auto color:resources_.menuScreen.palette) {
            for(const auto value:{color.red,color.green,color.blue}) { returnPalette.push_back(value>>2); }
        }
    }
    NativeSessionDelivery delivery;delivery.firstSample=elapsedSamples();
    StoryDeliveryStream stream(sink);
    if(!audio_->intro_) {
        NativeStartupIntroRun run{};run.firstSample=delivery.firstSample;run.mode=setting==0
            ? NativeIntroPresentation::storySkipped : NativeIntroPresentation::storyPlayed;
        run.palette=audio_->presentation_->palette;audio_->intro_=std::move(run);
    }
    auto phase=*audio_->workPhase_;auto palette=audio_->intro_->palette;
    phase.playableCampaign=true;
    if(!storyRoles_) {storyRoles_=CockpitPageState::fromRecoveredMenu();storyRoles_->selectFront();}
    auto& roles=*storyRoles_;
    const auto deviceBudget=[&]() {return phase.callback && phase.callback->program ? deviceHorizon : workHorizon;};
    const auto publish=[&]() {
        frame_.pixels=pages_.snapshot(roles.frontPage());frame_.palette=expandVgaDacPalette(
            std::vector<std::uint8_t>(palette.dac.colors.begin(),palette.dac.colors.end()));
        delivery.presentations.push_back({elapsedSamples(),frame_});
    };
    const auto append=[&](ClockedPresentationWork work) {
        if(work.audio.firstSample!=delivery.firstSample+delivery.samples.size()) {throw std::logic_error("Story intro lost retained PCM epoch");}
        phase=work.timing.phase;delivery.samples.insert(delivery.samples.end(),work.audio.samples.begin(),work.audio.samples.end());
        if(audio_->synth_.sampledVoiceObservation().phase==SampledVoicePhase::irqPending) {audio_->synth_.serviceSampledVoiceIrq();}
        stream.publish(delivery);
    };
    const auto commit=[&]() {
        *audio_->workPhase_=phase;audio_->intro_->palette=palette;audio_->intro_->finalPhase=phase;
        audio_->intro_->samples.insert(audio_->intro_->samples.end(),delivery.samples.begin(),delivery.samples.end());
    };
    const auto wait=[&](const std::uint16_t raw) {
        append(audio_->clock_.renderWork(phase,{{NormalWorkKind::playableStoryWaitStore,1,raw}},workHorizon));
        bool done=false;
        while(!done) {
            auto work=audio_->clock_.renderWork(phase,{{NormalWorkKind::playableStoryWaitPoll,std::min<std::size_t>(workPacketBudget,1000)}},workHorizon);
            done=work.timing.countdownReturned;append(std::move(work));
        }
    };
    const auto blink=[&](const std::size_t cycles) {
        for(std::size_t cycle=0;cycle<cycles;++cycle) {
            for(const auto color:{typewriterColor,std::uint8_t{0}}) {
                const auto work=uiFillOperations({introX_,introY_,4,8},color,roles.frontPage());
                pages_.apply(work.effects);publish();wait(12);
            }
        }
    };
    const auto blackout=[&]() {
        const auto prior=pages_.snapshot(roles.frontPage());
        auto work=audio_->clock_.renderBlackout(phase,{palette,{}},roles,pages_,deviceBudget(),BlackoutPageLayout::recoveredStartup);
        for(auto& step:work.steps) {
            append(std::move(step.before));palette=step.palette.state;append(std::move(step.palette.work));
            frame_.pixels=prior;frame_.palette=expandVgaDacPalette(std::vector<std::uint8_t>(palette.dac.colors.begin(),palette.dac.colors.end()));
            delivery.presentations.push_back({elapsedSamples(),frame_});
        }
        palette=work.state.palette;append(std::move(work.clear));publish();
    };
    const auto finish=[&](const IntroReturnPalette selection) {
        if(selection==IntroReturnPalette::select || !fullPresentation) {
            std::copy(returnPalette.begin(),returnPalette.end(),palette.source.begin());
            auto selected=audio_->clock_.renderPalette(phase,palette,{0,vgaPaletteColors,0,vgaComponentMaximum},deviceBudget());
            palette=selected.state;append(std::move(selected.work));publish();
        }
        if(fullPresentation && audio_->attractCreditsSkip_==0) {
            introTitleStage_=IntroTitleStage::overlays;
            stream.publish(delivery);
            AttractPresentation p({pages_,roles,palette,audio_->synth_,audio_->clock_,phase,
                game_,resources_,audio_->settings_->settings.configuration,audio_->attractCreditsSkip_,{}},delivery,workPacketBudget,sink);
            p.titleScreen();
            stream.adopt(delivery);publish();
        }
        phase.playableCampaign=false;
        commit();phase_=NativeStartupPhase::awaitingMenuMusic;
    };
    if(story_) {
        const auto key=static_cast<std::uint16_t>(input.cancel ? 1 : input.confirm||input.up||input.down||input.left||input.right ? 0x39 : 0);
        auto child=story_->advance({pages_,roles,palette,audio_->synth_,audio_->clock_,phase,game_.scoreFont,
            returnPalette,audio_->playableMusicEnabled(),audio_->settings_->settings.configuration.options[3]!=0},
            {key,audio_->joystickButtonHeld()||(input.pointer && (input.pointer->primary || input.pointer->secondary))},workPacketBudget,sink);
        delivery=std::move(child);stream.adopt(delivery);publish();
        const auto result=story_->result();
        if(result==StorySceneResult::playing) {commit();stream.publish(delivery);return delivery;}
        if(activeStory_==StoryScene::bridgeOpening && result==StorySceneResult::complete) {
            activeStory_=StoryScene::earthOpening;
            story_=std::make_shared<PlayableStoryPresenter>(*game_.storyScenes,*activeStory_,
                RadarPages{roles.cachePage(),roles.backPage(),roles.frontPage(),roles.drawPage(),roles.copyPage()});
            commit();stream.publish(delivery);return delivery;
        }
        if (result==StorySceneResult::skipped) { audio_->attractCreditsSkip_=1; }
        story_.reset();activeStory_.reset();finish(IntroReturnPalette::inherited);stream.publish(delivery);return delivery;
    }
    if(setting==0 || input.cancel) {
        if (input.cancel) { audio_->attractCreditsSkip_=1; }
        blackout();finish(IntroReturnPalette::select);stream.publish(delivery);return delivery;
    }
    if(!introStarted_) {
        blackout();std::copy(returnPalette.begin(),returnPalette.end(),palette.source.begin());
        auto selected=audio_->clock_.renderPalette(phase,palette,{0,vgaPaletteColors,0,vgaComponentMaximum},deviceBudget());
        palette=selected.state;append(std::move(selected.work));publish();blink(5);introStarted_=true;
    }
    if(introCharacter_<opening.size()) {
        const auto character=opening[introCharacter_++];blink(character=='$'?5:1);
        if(character=='$') {introX_=40;introY_=92;}
        else {
            const auto text=uiTextOperations(game_.titleFont,UiFontLayout::fixed,std::string_view(&character,1),
                {introX_,introY_},typewriterColor,roles.frontPage(),{0,319,0,199});
            pages_.apply(text.effects);introX_=static_cast<std::int16_t>(introX_+8);publish();
        }
        commit();stream.publish(delivery);return delivery;
    }
    blink(5);blackout();introTypingFinished_=true;activeStory_=StoryScene::bridgeOpening;
    story_=std::make_shared<PlayableStoryPresenter>(*game_.storyScenes,*activeStory_,
        RadarPages{roles.cachePage(),roles.backPage(),roles.frontPage(),roles.drawPage(),roles.copyPage()});
    commit();stream.publish(delivery);return delivery;
}

bool NativeStartupSequence::titleActive() const noexcept {
    return audio_ && hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::storyPanels)
        && hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::automaticAttract)
        && introTitleStage_!=IntroTitleStage::inactive
        && (phase_==NativeStartupPhase::awaitingIntroInput || phase_==NativeStartupPhase::awaitingMenuMusic
            || phase_==NativeStartupPhase::awaitingMenuResources);
}

NativeSessionDelivery NativeStartupSequence::pollIntroTitle(const NativeMenuInput input,
    const std::size_t budget,const NativeDeliverySink& sink) {
    if(!titleActive() || phase_!=NativeStartupPhase::awaitingMenuResources || !audio_->workPhase_
        || !audio_->intro_ || !audio_->settings_ || !audio_->menuSong_ || !storyRoles_ || budget==0 || budget>10000) {
        throw std::invalid_argument("cold title requires its retained full presentation caller");
    }
    NativeSessionDelivery delivery;delivery.firstSample=elapsedSamples();
    auto& palette=audio_->intro_->palette;
    AttractPresentation p({pages_,*storyRoles_,palette,audio_->synth_,audio_->clock_,*audio_->workPhase_,
        game_,resources_,audio_->settings_->settings.configuration,audio_->attractCreditsSkip_,{}},delivery,budget,sink);
    //5F4C(0) consumes an ordinary key; Escape or either held button returns1.
    const bool escaped=input.sourceKey!=0 ? (input.sourceKey>>8)==1 : input.cancel;
    const bool skipped=escaped || audio_->joystickButtonHeld()
        || (input.pointer && (input.pointer->primary || input.pointer->secondary));
    if(introTitleStage_==IntroTitleStage::overlays) {
        if(skipped) { audio_->attractCreditsSkip_=1;introTitleStage_=IntroTitleStage::inactive; }
        else {
            p.titleOverlays();introTitleStage_=IntroTitleStage::returnInput;
            if(hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::nativeTitlePacing)) {
                nativeTitleEndSample_=elapsedSamples()+nativeTitleMinimumSamples;
            }
        }
    } else {
        if(!skipped && elapsedSamples()<nativeTitleEndSample_) {
            const auto deadline=std::min(nativeTitleEndSample_,elapsedSamples()+nativeTitlePollSamples);
            // Explicit native presentation pacing, not recovered loader CPU work.
            // Like native game-speed pacing, retain devices/music without host silence.
            while(elapsedSamples()<deadline) {p.normal({{NormalWorkKind::instructions,nativeTitleIdleDispatches}});}
            if(elapsedSamples()<nativeTitleEndSample_) {return delivery;}
        }
        audio_->attractCreditsSkip_=skipped ? 1 : 0;
        introTitleStage_=IntroTitleStage::inactive;
    }
    frame_.pixels=pages_.snapshot(storyRoles_->frontPage());
    frame_.palette=expandVgaDacPalette(std::vector<std::uint8_t>(palette.dac.colors.begin(),palette.dac.colors.end()));
    return delivery;
}
}
