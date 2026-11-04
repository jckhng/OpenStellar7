#include "playable_startup_resources.hpp"
#include "stellar7/native_game_rate.hpp"
#include <iostream>

namespace {
using namespace stellar7;
void require(const bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
void run(const std::filesystem::path& root) {
    require(defaultNativeGameRate==25,"installed player must default to the explicit25-update native policy");
    for(const auto rate:nativeGameRates) {
        NativeFramePacer pacer(rate);std::uint64_t next=1000;
        const auto count=rate?rate:1;
        for(unsigned frame=0;frame<count;++frame) {next=pacer.deadline(next);}
        require(next==1000+(rate?SoundBlasterSynth::sampleRate:0),"native rate does not preserve one second of audio time");
        pacer.reset();require(pacer.deadline(0)==(rate?SoundBlasterSynth::sampleRate/rate:0),"native rate reset retained an old epoch");
    }
    auto game=naturalRoute::assets(root);
    const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto drivers=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    const auto profile=decodeFrameCopyProfile(drivers.at(0));
    RecoveredFrontend strict(game,art,profile);
    bool rejected=false;try {strict.paceGameplay(100);}catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"source execution admitted portable pacing");
    RecoveredFrontend frontend(game,art,profile,10000,{},CampaignExecution::playable,
        NativePointerCapability::unavailable,NativeMenuFeatures::fullGraphicDetail);
    for(unsigned call=0;frontend.startup().phase()!=NativeStartupPhase::awaitingMenuDecision&&call<20000;++call) {frontend.advanceStartup({});}
    require(frontend.startup().phase()==NativeStartupPhase::awaitingMenuDecision,"pacing fixture missed menu");
    NativeMenuInput begin{};begin.confirm=true;frontend.advanceStartup(begin);frontend.advanceStartup({});
    for(unsigned call=0;!frontend.session()&&call<128;++call) {frontend.advanceStartup({});}
    require(frontend.session()!=nullptr,"pacing fixture missed Begin");
    for(unsigned call=0;frontend.session()->model().phase()!=GameplayPhase::playable&&call<16;++call) {frontend.advanceGameplay({});}
    const auto* session=frontend.session();const auto* synth=&session->synth();
    require(session->model().phase()==GameplayPhase::playable,"pacing fixture missed retained ordinary frame");
    for(const auto rate:{25U,35U,50U,70U}) {
        const auto first=frontend.elapsedSamples();const auto tick=session->model().level2LoopTick();
        const auto frame=frontend.frame();const auto armor=session->model().playerArmor();const auto score=session->model().score();
        const auto player=session->model().level2Player();
        const auto pace=frontend.paceGameplay(first+SoundBlasterSynth::sampleRate/rate);
        require(pace.firstSample==first&&!pace.samples.empty()&&pace.presentations.empty()&&pace.events.empty(),"pacing invented source visuals/events or lost PCM");
        require(frontend.elapsedSamples()==first+pace.samples.size()&&frontend.elapsedSamples()>=first+SoundBlasterSynth::sampleRate/rate,
            "pacing did not advance the same audio timeline");
        require(frontend.session()==session&&&session->synth()==synth&&session->model().level2LoopTick()==tick
            &&session->model().playerArmor()==armor&&session->model().score()==score
            &&session->model().level2Player().position==player.position&&session->model().level2Player().yaw==player.yaw
            &&frontend.frame().pixels==frame.pixels,"pacing advanced or replaced simulation/render owners");
        const auto next=frontend.advanceGameplay({});
        require(next.firstSample==first+pace.samples.size()&&session->model().level2LoopTick()==static_cast<std::uint16_t>(tick+1),
            "pacing broke adjacent frame/audio continuity");
    }
    const auto at=frontend.elapsedSamples();
    require(frontend.paceGameplay(at).samples.empty(),"uncapped pacing inserted audio");
    rejected=false;try {frontend.paceGameplay(at+SoundBlasterSynth::sampleRate);}catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"pacing admitted an unbounded idle");
    std::cout<<"Native game-rate/audio-clock/retained-state/adjacent-frame PASS\n";
}
}
int main(int argc,char** argv) {try {if(argc!=2) {return 2;}run(argv[1]);return 0;}catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
