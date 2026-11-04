#include "native_player_entry.hpp"
#include "gameplay_asset_loader.hpp"
#include "resource_file.hpp"
#include "stellar7/frontend.hpp"
#include "stellar7/recovered_frontend.hpp"
#include "stellar7/resource_codec.hpp"
#include "stellar7/story_scene.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

namespace stellar7 {

int runInstalledPlayer(const stellar7::NativeProfilePaths& paths,
    const stellar7::HighscoreTable& table,const stellar7::NativePlayerAction action,const unsigned gameRate) {
    using namespace stellar7;
    const auto& data=paths.resourceDirectory;
    const auto resourceFiles=resolveNativeData(data);
    if(!resourceFiles.validation.ok) {throw std::runtime_error(resourceFiles.validation.message);}
    auto assets=loadGameplayAssets(data,{},{},table,&resourceFiles);
    const auto startup=decodeNativeStartupAssets(readFile(resourceFiles.file("STELART.RES")));
    const auto drivers=decodeTaggedPayloads(readFile(resourceFiles.file("STELLAR.RES")),{'V','G','A',':'});
    if(drivers.empty()) {throw std::runtime_error("Missing native frame-copy driver.");}
    const auto copyProfile=decodeFrameCopyProfile(drivers.front());
    assets.storyScenes=std::make_shared<const StorySceneAssets>(decodeStorySceneAssets(
        readFile(resourceFiles.file("SCENEB1.RES")),readFile(resourceFiles.file("SCENEA.RES")),
        {readFile(resourceFiles.file("SCENEB2.RES")),readFile(resourceFiles.file("SCENEB3.RES")),readFile(resourceFiles.file("SCENEB5.RES"))},
        readFile(resourceFiles.file("SCENEC.RES")),readFile(resourceFiles.file("VOICE.RES")),
        readFile(resourceFiles.file("DRAXON.RES")),readFile(resourceFiles.file("SCENEE.RES"))));
    assets.attractVoiceAvailable=true;
    if(!assets.endingScene || !assets.storyScenes) {throw std::runtime_error("Campaign presentation data is incomplete.");}
    if(action==NativePlayerAction::play) {
        GameplayRunOptions options;
        options.highscoreStore=std::make_shared<NativeProfileHighscoreStore>(paths.profileDirectory);
        options.menuFeatures=NativeMenuFeatures::releaseMenus;
#ifdef STELLAR7_ENHANCED
        options.menuFeatures=NativeMenuFeatures::enhancedMenus;
#endif
        options.nativeGameRate=gameRate;
        options.nativeOptionsPath=paths.profileDirectory/"enhanced.cfg";
        return runSdlPlayableNativeEntry(std::move(assets),startup,copyProfile,std::move(options));
    }
    // Exercise the installed data/save snapshot through the actual retained owners.
    // This device-free check does not qualify host audio, input or original timing.
    const auto beforeSave=readFile(paths.highscorePath);
    RecoveredFrontend frontend(assets,startup,copyProfile,10000,{},CampaignExecution::playable,
        NativePointerCapability::unavailable,NativeMenuFeatures::releaseMenus);
    std::uint64_t samples=0,pcmHash=14695981039346656037ULL,pixelsHash=14695981039346656037ULL;
    const auto hashByte=[](std::uint64_t& hash,const std::uint8_t byte) {hash^=byte;hash*=1099511628211ULL;};
    const auto collect=[&](const NativeSessionDelivery& delivery) {
        if(delivery.firstSample!=samples||delivery.exitRequested) {throw std::runtime_error("Install check lost the presentation clock.");}
        for(const auto value:delivery.samples) {
            const auto word=static_cast<std::uint16_t>(value);
            hashByte(pcmHash,static_cast<std::uint8_t>(word));hashByte(pcmHash,static_cast<std::uint8_t>(word>>8));
        }
        samples+=delivery.samples.size();
        if(samples!=frontend.elapsedSamples()) {throw std::runtime_error("Install check changed its audio epoch.");}
    };
    constexpr unsigned maximumStartupPackets=20000,maximumBeginPackets=128,maximumGameplayPackets=16;
    for(unsigned call=0;frontend.startup().phase()!=NativeStartupPhase::awaitingMenuDecision&&call<maximumStartupPackets;++call) {
        NativeMenuInput input{};
        if(frontend.startup().phase()==NativeStartupPhase::awaitingIntroInput||frontend.startup().storyActive()) {input.cancel=true;}
        collect(frontend.advanceStartup(input));
    }
    if(frontend.startup().phase()!=NativeStartupPhase::awaitingMenuDecision) {throw std::runtime_error("Install check did not reach the main menu.");}
    NativeMenuInput begin{};begin.confirm=true;
    collect(frontend.advanceStartup(begin));collect(frontend.advanceStartup({}));
    for(unsigned call=0;!frontend.session()&&call<maximumBeginPackets;++call) {collect(frontend.advanceStartup({}));}
    const auto* session=frontend.session();
    if(!session) {throw std::runtime_error("Install check did not enter Level 1.");}
    const auto* synth=&session->synth();
    unsigned adjacent=0;
    for(unsigned call=0;adjacent<2&&call<maximumGameplayPackets;++call) {
        const auto phase=session->model().phase();const auto tick=session->model().level2LoopTick();
        collect(frontend.advanceGameplay({}));
        if(frontend.session()!=session||&session->synth()!=synth) {throw std::runtime_error("Install check replaced retained gameplay owners.");}
        if(phase==GameplayPhase::playable&&session->model().phase()==GameplayPhase::playable) {
            if(session->model().level2LoopTick()!=static_cast<std::uint16_t>(tick+1)) {throw std::runtime_error("Install check skipped a frame.");}
            const auto& frame=frontend.frame();
            if(frame.pixels.size()!=320U*200U||frame.palette.size()!=256) {throw std::runtime_error("Install check has an invalid indexed display.");}
            for(const auto pixel:frame.pixels) {hashByte(pixelsHash,pixel);}
            for(const auto color:frame.palette) {hashByte(pixelsHash,color.red);hashByte(pixelsHash,color.green);hashByte(pixelsHash,color.blue);}
            ++adjacent;
        }
    }
    if(adjacent!=2||session->model().campaignLevel()!=0||readFile(paths.highscorePath)!=beforeSave) {
        throw std::runtime_error("Install check failed adjacent Level 1 frames or changed the save.");
    }
    std::cout<<"{\"verified\":true,\"resource_files\":"<<nativeDataRequirements().size()<<",\"adjacent_level1_frames\":"<<adjacent
        <<",\"samples\":"<<samples<<",\"pcm_fnv1a64\":"<<pcmHash
        <<",\"indexed_palette_fnv1a64\":"<<pixelsHash<<",\"save_unchanged\":true}\n";
    return 0;
}

} // namespace stellar7
