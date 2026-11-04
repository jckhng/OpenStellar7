// Component fixture: retain a real public-startup driver, then exercise synth
// publication directly. This does not qualify UI/persistence/cold-start paths.
#include "playable_startup_resources.hpp"
#include "stellar7/audio_direct_work.hpp"
#include "stellar7/success_palette_step.hpp"
#include <algorithm>
#include <iostream>

namespace stellar7 {
struct NativeSessionTestAccess {
    static SoundBlasterSynth& synth(NativeGameplaySession& session) {return session.synth_;}
    static PresentationClock& clock(NativeGameplaySession& session) {return session.clock_;}
};
}
namespace {
using namespace stellar7;
void require(const bool valid,const char* message) {
    if (!valid) {throw std::runtime_error(message);}
}
template<class Function> void rejects(Function function) {
    bool rejected=false;
    try {function();} catch (const std::invalid_argument&) {rejected=true;}
    require(rejected,"invalid MUSIC publication/store was admitted");
}
void sameDescriptor(const SoundBlasterDriverState& a,const SoundBlasterDriverState& b) {
    require(a.sequencePosition==b.sequencePosition && a.sequenceWait==b.sequenceWait
        && a.runningStatus==b.runningStatus && a.sequenceActive==b.sequenceActive && a.fade==b.fade
        && a.sequencePositions==b.sequencePositions && a.sequenceAllocations==b.sequenceAllocations
        && a.sequenceActivity==b.sequenceActivity,
        "MUSIC changed retained descriptor cursor/lifetime");
}
void run(const std::filesystem::path& root) {
    const auto assets=naturalRoute::assets(root);
    const auto startup=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto drivers=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    require(drivers.size()==1,"missing actual VGA driver");
    auto frontend=std::make_unique<RecoveredFrontend>(assets,startup,decodeFrameCopyProfile(drivers.front()),
        10000,std::shared_ptr<HighscoreStore>{},CampaignExecution::playable);
    unsigned polls=0;
    for (unsigned call=0;!frontend->session() && call<10000;++call) {
        NativeMenuInput input{};
        if (frontend->startup().phase()==NativeStartupPhase::awaitingMenuDecision) {input.confirm=polls++==1;}
        frontend->advanceStartup(input);
    }
    require(frontend->session()!=nullptr,"public startup did not reach Begin");
    frontend->advanceGameplay({});
    auto& session=const_cast<NativeGameplaySession&>(*frontend->session());
    auto& synth=NativeSessionTestAccess::synth(session);
    auto& clock=NativeSessionTestAccess::clock(session);
    auto phase=*session.recoveredClockPhase();
    require(phase.playableCampaign,"fixture did not adopt native campaign");
    const auto selected=synth.callbackWorkState();
    require(selected.domain==SoundBlasterCallbackDomain::sequence,"fixture requires default MUSIC On");
    const auto device=synth.driverState();
    phase.callback=synth.setPlayableMusicPublication(PlayableMusicPublication::suspended);
    phase.playableMusicOff=true;
    require(phase.callback->domain==SoundBlasterCallbackDomain::level2RetryNoHandle,
        "MUSIC Off kept current handle published");
    sameDescriptor(device,synth.driverState());
    require(selected.program->handleStatus==phase.callback->program->handleStatus
        && selected.program->repeatCount==phase.callback->program->repeatCount
        && selected.program->loopMarker==phase.callback->program->loopMarker,
        "MUSIC Off mutated descriptor metadata");
    require(device.normalClockCycles==synth.driverState().normalClockCycles
        && device.clockPhase==synth.driverState().clockPhase,"toggle reset/advanced device epoch");

    phase.callback=synth.triggerPlayableEvent(gameplayPrimaryFireEvent);
    const auto fired=*phase.callback;
    require(std::any_of(fired.voices.directTimers.begin(),fired.voices.directTimers.end(),
        [](const auto timer) {return timer!=0;}),"MUSIC Off suppressed independent direct SFX");
    sameDescriptor(device,synth.driverState());
    bool callback=false;
    for (unsigned packet=0;packet<100 && !callback;++packet) {
        auto work=clock.renderWork(phase,{{NormalWorkKind::instructions,1000}},120000);
        phase=work.timing.phase;callback=!work.timing.callbacks.empty();
    }
    require(callback,"fixture did not cross a real source callback");
    sameDescriptor(device,synth.driverState());
    require(phase.callback->voices.directTimers!=fired.voices.directTimers,
        "MUSIC Off froze independent direct-effect timers");
    const auto writes=synth.registerWrites().size();
    phase.callback=synth.setPlayableMusicPublication(PlayableMusicPublication::current);
    phase.playableMusicOff=false;
    require(synth.registerWrites().size()==writes,"MUSIC On wrote/reset OPL instead of publishing B94");
    sameDescriptor(device,synth.driverState());
    require(selected.sequence==phase.callback->sequence && selected.position==phase.callback->position
        && selected.wait==phase.callback->wait,"MUSIC On did not republish retained cursor");

    phase.callback=synth.selectPlayableSequence(assets.soundSequences.front(),PlayableSequenceMode::once,
        PlayableMusicPublication::suspended);
    phase.playableMusicOff=true;phase.playableSuccessCountdownWritten=false;
    require(synth.driverState().sequenceAllocated && phase.callback->program->repeatCount==1
        && phase.callback->program->handleStatus==0,"success lost once allocation");
    auto stored=clock.renderWork(phase,{{NormalWorkKind::playableSuccessCountdownStore,1,992}},120000);
    phase=stored.timing.phase;
    require(phase.playableSuccessCountdownWritten && phase.timer->globalCount>0
        && phase.timer->globalCount<=992,"success did not store its source03E0 countdown");
    rejects([&] {normalWorkTiming(phase,{{NormalWorkKind::playableSuccessCountdownStore,1,992}},120000);});
    auto invalid=phase;invalid.playableSuccessCountdownWritten=false;invalid.playableMusicOff=false;
    rejects([&] {normalWorkTiming(invalid,{{NormalWorkKind::playableSuccessCountdownStore,1,992}},120000);});
    // Controlled phase copy isolates source7093's timer completion boundary.
    // The full992 IRQ interval belongs to the public campaign regression.
    auto expired=phase;expired.timer->globalCount=0;
    const SuccessPaletteResidentCursor cursor{initialSuccessPalette(SuccessPaletteChannel::green),
        SuccessPaletteExecution::advance,SuccessPaletteCompletionSource::timer,SuccessPaletteResidentStage::entry};
    const auto finished=successPaletteStepTiming(expired,cursor,PaletteWorkState{},32,120000);
    require(finished.boundary==SuccessPaletteBoundary::presentationComplete
        && expired.callback->program->handleStatus==0,"timer completion waited for suspended descriptor EOF");
    const auto beforeRelease=synth.registerWrites().size();
    phase.callback=synth.releaseSuspendedPlayableSequence();
    require(synth.registerWrites().size()==beforeRelease,"empty51A6 issued op0E during resource release");
    require(!synth.driverState().sequenceAllocated,"suspended resource borrow was retained after release");
    std::cout<<"MUSIC retained publication / direct SFX / source7072 component PASS\n";
}
}
int main(int argc,char** argv) {
    if (argc!=2) {return 2;}
    try {run(argv[1]);return 0;}
    catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
