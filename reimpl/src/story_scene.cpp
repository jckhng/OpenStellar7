#include "stellar7/story_scene.hpp"
#include "stellar7/sampled_voice.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::size_t defeatScenePageCount=139;
constexpr std::size_t defeatVoiceGroup=3;
constexpr std::array<std::size_t,8> trackStarts{1,10,19,54,58,62,64,68};
constexpr std::array<std::size_t,8> trackEnds{9,18,53,57,61,63,67,89};
bool bridgeScene(const StoryScene scene) { return scene!=StoryScene::earthOpening && scene!=StoryScene::draxon && scene!=StoryScene::defeat; }
}
const SceneResource& storySceneResource(const StorySceneAssets& assets,const StoryScene scene) {
    switch (scene) {
    case StoryScene::bridgeOpening: return assets.bridge;
    case StoryScene::earthOpening: return assets.earth;
    case StoryScene::level2: return assets.campaign[0];
    case StoryScene::level4: return assets.campaign[1];
    case StoryScene::level7: return assets.campaign[2];
    case StoryScene::draxon: return assets.draxon;
    case StoryScene::defeat:
        if (!assets.defeat) { throw std::invalid_argument("Game over requires its SCENEE provider"); }
        return *assets.defeat;
    }
    throw std::invalid_argument("invalid Story scene");
}
const SceneResource& storySceneGraphics(const StorySceneAssets& assets,const StoryScene scene) {
    return bridgeScene(scene) ? assets.bridge : storySceneResource(assets,scene);
}
std::optional<StoryScene> campaignStoryScene(const std::size_t level) {
    switch (level) {
    case 1: return StoryScene::level2;
    case 3: return StoryScene::level4;
    case 6: return StoryScene::level7;
    default: return std::nullopt;
    }
}
StorySceneController::StorySceneController(const StorySceneAssets& assets,const StoryScene scene,const RadarPages pages)
    : interpreter_(storySceneResource(assets,scene),pages,0,0,&storySceneGraphics(assets,scene)),
      graphics_(&storySceneGraphics(assets,scene)),composite_(bridgeScene(scene)) {}

StorySceneStep StorySceneController::advance(const std::uint16_t keyScan) {
    if (result_!=StorySceneResult::playing) { throw std::logic_error("Story scene already returned"); }
    auto staged=*this;
    StorySceneStep step;
    if (keyScan==1 && entered_) {
        staged.result_=StorySceneResult::skipped;step.result=staged.result_;*this=std::move(staged);return step;
    }
    if (!entered_) {
        step.pages={0};step.work=staged.interpreter_.execute(0);staged.entered_=true;
        if (!composite_) { staged.mainPage_=1; }
    } else if (!composite_) {
        step.pages={staged.mainPage_++};step.work=staged.interpreter_.execute(step.pages.front());
        step.waitRawIrqs=keyScan==0 ? step.work.delayIrqCounts : 0;
    } else if (mainPage_>=98 && mainPage_<=158) {
        step.pages={staged.mainPage_++};step.work=staged.interpreter_.execute(step.pages.front());step.waitRawIrqs=20;
    } else {
        for (std::size_t track=0;track<staged.tracks_.size();++track) {
            step.pages.push_back(trackStarts[track]+staged.tracks_[track]);
            if (++staged.tracks_[track]+trackStarts[track]>=trackEnds[track]) { staged.tracks_[track]=0; }
        }
        step.waitRawIrqs=staged.repeats_!=0 ? 50 : 0; //127C..1285 uses unsigned CMP/CMC/SBB.
        auto remaining=staged.repeats_;
        if (staged.delay_!=0 && remaining==-1) { remaining=static_cast<std::int16_t>(staged.delay_>>2); }
        if (remaining>=1 && keyScan==0) { staged.repeats_=static_cast<std::int16_t>(remaining-1); }
        else { ++staged.mainPage_;staged.repeats_=-1; }
        step.pages.push_back(staged.mainPage_);
        step.work=staged.interpreter_.executeComposite(step.pages);
    }
    staged.delay_=static_cast<std::uint16_t>(step.work.delayIrqCounts/4);
    if (!step.work.present) {
        staged.result_=StorySceneResult::complete;
        if (composite_) {step.waitRawIrqs=0;} //1110 skips02C4 after a missing main page;0252 does not.
    }
    step.result=staged.result_;*this=std::move(staged);return step;
}
StorySceneAssets decodeStorySceneAssets(const std::vector<std::uint8_t>& bridge,
    const std::vector<std::uint8_t>& earth,const std::array<std::vector<std::uint8_t>,3>& campaign,
    const std::vector<std::uint8_t>& draxon,const std::vector<std::uint8_t>& voice,
    const std::vector<std::uint8_t>& background,const std::vector<std::uint8_t>& defeat) {
    StorySceneAssets assets{decodeSceneResource(bridge),decodeSceneResource(earth),
        {decodeSceneResource(campaign[0]),decodeSceneResource(campaign[1]),decodeSceneResource(campaign[2])},
        decodeSceneResource(draxon),{}};
    constexpr std::array<std::size_t,3> campaignPages{92,95,163};
    if (assets.bridge.program.pages.size()!=95 || assets.bridge.assets.size()!=5
        || assets.earth.program.pages.size()!=191 || assets.draxon.program.pages.size()!=6) {
        throw std::invalid_argument("Story requires complete source B1/A/C resources");
    }
    for (std::size_t index=0;index<campaign.size();++index) {
        if (assets.campaign[index].program.pages.size()!=campaignPages[index] || !assets.campaign[index].assets.empty()) {
            throw std::invalid_argument("Story requires shared B1 graphics and complete B2/B3/B5 scripts");
        }
    }
    for (std::size_t group=0;group<(defeat.empty() ? 2U : assets.voices.size());++group) {
        assets.voices[group]=decodeSoundSequenceGroup(voice,group);
        for (const auto& segment:assets.voices[group]) { decodeSampledVoice(segment); }
    }
    //Historical component fixtures omit wrapper audio; installed players always
    //supply the separately validated DRAXON resource.
    if (!background.empty()) {
        const auto sequences=decodeSoundSequenceGroup(background,0);
        if (sequences.size()!=1 || sequences.front().size()<36
            || sequences.front()[sequences.front().front()==0x84 ? 2 : 0]!=0) {
            throw std::invalid_argument("Story requires one DRAXON MIDI sequence");
        }
        assets.backgroundSequence=sequences.front();
    }
    if (!defeat.empty()) {
        assets.defeat=decodeSceneResource(defeat);
        if (assets.defeat->program.pages.size()!=defeatScenePageCount || assets.voices[defeatVoiceGroup].size()!=2) {
            throw std::invalid_argument("Game over requires complete SCENEE and VOICE SSM4");
        }
        constexpr RadarPages pages{0xa400,0xa800,0xa000,0xa800,0xa000};
        StorySceneController controller(assets,StoryScene::defeat,pages);
        while (controller.result()==StorySceneResult::playing) {controller.advance();}
    }
    // Validate every reached command before publishing the immutable provider.
    constexpr RadarPages pages{0xa400,0xa800,0xa000,0xa800,0xa000};
    for (const auto scene:{StoryScene::bridgeOpening,StoryScene::earthOpening,StoryScene::level2,
            StoryScene::level4,StoryScene::level7,StoryScene::draxon}) {
        StorySceneController controller(assets,scene,pages);
        std::size_t steps=0;
        while (controller.result()==StorySceneResult::playing && steps++<20000) { controller.advance(); }
        if (controller.result()!=StorySceneResult::complete) { throw std::invalid_argument("Story controller failed to reach EOF"); }
    }
    return assets;
}
}
