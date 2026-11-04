#include "stellar7/native_session.hpp"
#include "playable_story.hpp"
#include "level1_reward_model_bank.hpp"
#include <algorithm>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::array<NativeSessionStart,7> starts{NativeSessionStart::level1,NativeSessionStart::level2,
    NativeSessionStart::level3,NativeSessionStart::level4,NativeSessionStart::level5,NativeSessionStart::level6,NativeSessionStart::level7};
}
std::size_t NativeGameplaySession::storyPage() const noexcept {return story_ ? story_->page() : 0;}
bool NativeGameplaySession::schedulePlayableStory(const std::size_t destination,const bool retry) {
    if(!hasNativeMenuFeature(audio_->menuFeatures_,NativeMenuFeatures::storyPanels)) {return false;}
    if(!audio_->playableStoryProfile()) {throw std::invalid_argument("Story requires canonical option0/255");}
    if(storyDestination_) {
        if(*storyDestination_!=destination || storyRetry_!=retry || story_) {throw std::logic_error("Story loader continuation changed");}
        storyDestination_.reset();activeStory_.reset();return false;
    }
    const auto scene=campaignStoryScene(destination);
    if(!audio_->playableStoryEnabled() || !scene) {return false;}
    if(!assets_.storyScenes || !audio_->workPhase_ || !pixels_ || !recoveredStartupPalette_) {
        throw std::invalid_argument("Story requires its preflighted shared B1 scene provider");
    }
    storyDestination_=destination;storyRetry_=retry;activeStory_=scene;
    story_=std::make_shared<PlayableStoryPresenter>(*assets_.storyScenes,*scene,
        RadarPages{pages_.cachePage(),pages_.backPage(),pages_.frontPage(),pages_.drawPage(),pages_.copyPage()});
    return true;
}
NativeSessionDelivery NativeGameplaySession::advancePlayableStory(const GameplayInput& input,const std::size_t workPacketBudget,const NativeDeliverySink& sink) {
    if(!story_ || !activeStory_ || (!storyDestination_ && *activeStory_!=StoryScene::defeat) || !pixels_ || !audio_->workPhase_ || !recoveredStartupPalette_
        || input.systemAction!=GameplaySystemAction::none) {throw std::invalid_argument("Story lost its retained caller");}
    auto delivery=story_->advance({*pixels_,pages_,*recoveredStartupPalette_,synth_,clock_,*audio_->workPhase_,
        assets_.scoreFont,assets_.cockpitDacPalette,audio_->playableMusicEnabled(),audio_->playableSoundsEnabled()},
        {static_cast<std::uint16_t>(input.exit ? 1 : input.left||input.right||input.forward||input.reverse||input.confirm ? 0x39 : 0),
            input.fire || audio_->joystickButtonHeld() || (input.pointer && (input.pointer->primary || input.pointer->secondary))},workPacketBudget,sink);
    frame_.pixels=pixels_->snapshot(pages_.frontPage());
    frame_.palette=expandVgaDacPalette(std::vector<std::uint8_t>(recoveredStartupPalette_->dac.colors.begin(),recoveredStartupPalette_->dac.colors.end()));
    const auto result=story_->result();
    if(result==StorySceneResult::playing) {return delivery;}
    if(*activeStory_==StoryScene::level7 && result==StorySceneResult::complete) {
        activeStory_=StoryScene::draxon;
        story_=std::make_shared<PlayableStoryPresenter>(*assets_.storyScenes,*activeStory_,
            RadarPages{pages_.cachePage(),pages_.backPage(),pages_.frontPage(),pages_.drawPage(),pages_.copyPage()});
        return delivery;
    }
    story_.reset();activeStory_.reset();return delivery;
}
NativeSessionDelivery NativeGameplaySession::reloadPlayableRetry(const std::size_t workPacketBudget) {
    const auto destination=model_.campaignLevel();
    if(destination>=starts.size() || assets_.campaignLevels.size()<=destination || !campaignResources_
        || campaignResources_->level!=destination || !audio_->workPhase_ || !recoveredStartupPalette_
        || workPacketBudget==0 || workPacketBudget>10000) {throw std::invalid_argument("playable retry lost same-level owners");}
    NativeSessionDelivery delivery;delivery.firstSample=clock_.elapsedSamples();
    auto phase=*audio_->workPhase_;auto palette=*recoveredStartupPalette_;
    const auto count=level1RewardModelCapacity(assets_);
    const std::vector<Model3d> shared(assets_.models.begin(),assets_.models.begin()+count);
    const auto& resources=assets_.campaignLevels[destination];
    synth_.validateCampaignMusicLoad(resources.musicSequence);
    model_.completePlayableCampaignRetry(shared,resources.models);initializePages(starts[destination]);
    phase.callback=synth_.selectPlayableSequence(resources.musicSequence,PlayableSequenceMode::repeat,audio_->playableMusicPublication());
    phase.playableMusicOff=!audio_->playableMusicEnabled();phase.playableSuccessCountdownWritten=false;
    phase.sequenceSelection.reset();phase.successAudio.reset();phase.level2RetryMusic.reset();
    level2Cursor_=0;failureCursor_=0;failurePages_=false;continuePages_=false;
    nameEntry_.reset();nameEntryPages_=false;hallPages_=false;successPalette_.reset();recoveredSuccessState_.reset();
    pageProjection_=NativePageProjection::retainedPlayableLoader;
    auto uploaded=clock_.renderPalette(phase,palette,{0,vgaPaletteColors,0,0},240000);
    *audio_->workPhase_=uploaded.work.timing.phase;*recoveredStartupPalette_=uploaded.state;
    delivery.samples=std::move(uploaded.work.audio.samples);
    frame_.pixels=pixels_->snapshot(pages_.frontPage());frame_.palette=expandVgaDacPalette(
        std::vector<std::uint8_t>(uploaded.state.dac.colors.begin(),uploaded.state.dac.colors.end()));
    delivery.presentations.push_back({clock_.elapsedSamples(),frame_});return delivery;
}
}
