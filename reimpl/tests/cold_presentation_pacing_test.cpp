#include "playable_startup_resources.hpp"
#include "stellar7/story_scene.hpp"
#include <iostream>

namespace {
using namespace stellar7;
void require(bool value,const char* message) { if (!value) { throw std::runtime_error(message); } }
GameplayAssets assets(const std::filesystem::path& root) {
    auto result=naturalRoute::assets(root);
    result.storyScenes=std::make_shared<const StorySceneAssets>(decodeStorySceneAssets(
        naturalRoute::read(root/"SCENEB1.RES"),naturalRoute::read(root/"SCENEA.RES"),
        {naturalRoute::read(root/"SCENEB2.RES"),naturalRoute::read(root/"SCENEB3.RES"),naturalRoute::read(root/"SCENEB5.RES")},
        naturalRoute::read(root/"SCENEC.RES"),naturalRoute::read(root/"VOICE.RES")));
    result.cockpitSettings=CockpitSettings{81,{1,0,1,1,0,255}};
    result.highscoreTable->setConfiguration(*result.cockpitSettings);
    return result;
}
std::vector<std::int16_t> run(const GameplayAssets& game,const NativeStartupAssets& art,FrameCopyProfile copy,std::size_t budget) {
    RecoveredFrontend frontend(game,art,copy,budget,{},CampaignExecution::playable,
        NativePointerCapability::portablePointer,NativeMenuFeatures::releaseMenus);
    std::vector<std::int16_t> pcm;
    const auto advance=[&](NativeMenuInput input=NativeMenuInput{}) {
        const auto first=frontend.elapsedSamples();auto delivery=frontend.advanceStartup(input);
        require(delivery.firstSample==first && first+delivery.samples.size()==frontend.elapsedSamples(),"cold pacing lost PCM continuity");
        pcm.insert(pcm.end(),delivery.samples.begin(),delivery.samples.end());
    };
    for (unsigned count=0;frontend.startup().phase()!=NativeStartupPhase::awaitingIntroInput && count<60000;++count) { advance(); }
    require(frontend.startup().phase()==NativeStartupPhase::awaitingIntroInput,"missed retained logo return");
    std::cout<<"postLogo samples="<<frontend.elapsedSamples()<<" countdown="
        <<frontend.startup().recoveredLogoRelease()->finalPhase.timer->globalCount<<'\n';
    for (unsigned count=0;frontend.startup().phase()!=NativeStartupPhase::awaitingMenuConstruction && count<60000;++count) { advance(); }
    require(frontend.startup().phase()==NativeStartupPhase::awaitingMenuConstruction,"missed credits entry");
    const auto entry=frontend.elapsedSamples();advance();
    require(frontend.startup().phase()==NativeStartupPhase::awaitingMenuConstruction,
        "cold startup synthesized Escape instead of holding credits");
    require(frontend.frame().pixels!=std::vector<std::uint8_t>(64000),"credits were not published before their wait");
    for (unsigned count=0;frontend.startup().phase()==NativeStartupPhase::awaitingMenuConstruction && count<120000;++count) { advance(); }
    require(frontend.startup().phase()==NativeStartupPhase::awaitingMenuEntry,"credits did not expire naturally");
    const auto& returned=frontend.startup().recoveredCredits()->finalPhase;
    const auto minimumSamples=7200*returned.pitPeriodMilliseconds*SoundBlasterSynth::sampleRate/1000;
    require(frontend.elapsedSamples()-entry>=minimumSamples-SoundBlasterSynth::sampleRate/200,
        "credits returned before their recovered7200 IRQ countdown");
    require(returned.sequenceSelection->creditsInputReturned,"credits lost their actual return proof");
    std::cout<<"credits samples="<<frontend.elapsedSamples()-entry<<" budget="<<budget<<'\n';
    for (unsigned count=0;frontend.startup().phase()!=NativeStartupPhase::awaitingMenuDecision && count<100;++count) { advance(); }
    require(frontend.startup().phase()==NativeStartupPhase::awaitingMenuDecision,"credits did not join main menu");
    NativeMenuInput begin;begin.confirm=true;advance(begin);
    for (unsigned count=0;!frontend.session() && count<128;++count) { advance(); }
    require(frontend.session(),"pacing lost actual Begin");
    for (unsigned frame=0;frame<3;++frame) {
        const auto first=frontend.elapsedSamples();auto delivery=frontend.advanceGameplay({});
        require(delivery.firstSample==first && first+delivery.samples.size()==frontend.elapsedSamples(),"pacing lost adjacent gameplay PCM");
        pcm.insert(pcm.end(),delivery.samples.begin(),delivery.samples.end());
    }
    return pcm;
}
void dismissCredits(GameplayAssets game,const NativeStartupAssets& art,FrameCopyProfile copy,const std::uint16_t key) {
    game.cockpitSettings->options[2]=0;game.cockpitSettings->options[5]=0;
    game.highscoreTable->setConfiguration(*game.cockpitSettings);
    RecoveredFrontend frontend(game,art,copy,257,{},CampaignExecution::playable,
        NativePointerCapability::portablePointer,NativeMenuFeatures::releaseMenus);
    for (unsigned count=0;!frontend.startup().creditsActive() && count<120000;++count) { frontend.advanceStartup({}); }
    require(frontend.startup().creditsActive(),"MUSIC/STORY Off missed credits entry");
    frontend.advanceStartup({});
    require(frontend.presentationInputActive() && frontend.attractAcceptsSourceKey(),"cold credits did not accept actual source keys");
    const auto before=frontend.elapsedSamples();NativeMenuInput input;input.sourceKey=key;
    const auto delivery=frontend.advanceStartup(input);
    require(delivery.firstSample==before && before+delivery.samples.size()==frontend.elapsedSamples(),"credit key lost retained PCM");
    require(frontend.startup().phase()==NativeStartupPhase::awaitingMenuEntry,"credit key failed to dismiss without waiting");
    require(frontend.elapsedSamples()-before<2*SoundBlasterSynth::sampleRate,"credit key waited for natural expiry");
    require(frontend.startup().recoveredCredits()->sourceKey==key,"credit key was replaced by synthesized Escape");
    frontend.advanceStartup({});
    require(frontend.startup().phase()==NativeStartupPhase::awaitingMenuDecision && !frontend.session(),"credit dismissal activated a menu item");
}
void skippedIntroCredits(const GameplayAssets& game,const NativeStartupAssets& art,FrameCopyProfile copy) {
    RecoveredFrontend frontend(game,art,copy,257,{},CampaignExecution::playable,
        NativePointerCapability::portablePointer,NativeMenuFeatures::releaseMenus);
    bool skipped=false;
    for (unsigned count=0;!frontend.startup().creditsActive() && count<120000;++count) {
        NativeMenuInput input;
        if (!skipped && frontend.startup().storyVoice().transferred!=0) { input.cancel=true;skipped=true; }
        frontend.advanceStartup(input);
    }
    require(skipped && frontend.startup().creditsActive(),"skip fixture missed actual Story return");
    const auto before=frontend.elapsedSamples();const auto frame=frontend.frame();
    const auto incoming=frontend.startup().recoveredMenuResources()->finalPhase;
    const auto expected=normalWorkTiming(incoming,{{NormalWorkKind::instructions,17}},120000);
    const auto delivery=frontend.advanceStartup({});
    const auto& credits=*frontend.startup().recoveredCredits();
    require(frontend.startup().phase()==NativeStartupPhase::awaitingMenuEntry,"skipped Story did not bypass credits");
    require(credits.finalPhase.sequenceSelection->creditsSkipReturned && !credits.finalPhase.sequenceSelection->creditsCountdownWritten && !credits.finalPhase.sequenceSelection->creditsInputReturned
        && credits.sourceKey==0 && credits.drawing.operations.empty(),"skipped Story executed credits or synthesized a key");
    require(credits.driverAfter.normalClockCycles-credits.driverBefore.normalClockCycles==expected.total.elapsedCycles,
        "skipped credits lost its exact17-dispatch return");
    require(frontend.frame().pixels==frame.pixels && delivery.firstSample==before && before+delivery.samples.size()==frontend.elapsedSamples(),
        "skipped credits changed pixels or lost PCM");
    frontend.advanceStartup({});
    require(frontend.startup().phase()==NativeStartupPhase::awaitingMenuDecision,"skipped credits failed its actual menu entry");
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"requires actual resources");const std::filesystem::path root=argv[1];
        const auto game=assets(root);const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
        const auto drivers=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
        const auto copy=decodeFrameCopyProfile(drivers.at(0));
        require(run(game,art,copy,10000)==run(game,art,copy,257),"cold pacing changed execution across packet budgets");
        dismissCredits(game,art,copy,0x1e61);dismissCredits(game,art,copy,0x011b);skippedIntroCredits(game,art,copy);
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
