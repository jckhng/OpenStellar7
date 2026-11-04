#include "playable_startup_resources.hpp"
#include "playable_attract_controller.hpp"
#include <iostream>
using namespace stellar7;
namespace {
void require(bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
template<class Action> void rejects(Action action) {
    bool rejected=false;try {action();}catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"strict/native attract guard admitted unsupported work");
}
void run(const std::filesystem::path& root) {
    const auto assets=naturalRoute::assets(root);
    const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto vga=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    RecoveredFrontend f(assets,art,decodeFrameCopyProfile(vga.at(0)),10000,{},CampaignExecution::playable);
    for(unsigned call=0;f.startup().phase()!=NativeStartupPhase::awaitingMenuDecision&&call<10000;++call) {f.advanceStartup({});}
    f.advanceStartup({});
    auto phase=f.startup().recoveredMenuInput()->work.back().timing.phase;
    constexpr auto decision=NormalWorkKind::playableAttractMenuDecision;
    constexpr std::int64_t horizon=120000;
    rejects([&]{normalWorkTiming(phase,{{decision,1}},horizon);});
    phase.playableAutomaticAttract=true;
    //Controlled pending-IRQ fixture: incoming1 becomes0 inside1611, not before call.
    auto race=phase;constexpr std::int64_t quantum=5000;
    const auto local=quantum-race.remainingCycles-race.deferredCycles;
    race.remainingCycles=1;race.deferredCycles=quantum-local-1;
    race.devices->irq0Requested=true;race.irqCheckRequested=true;
    race.timer->callbackCount=1;race.timer->globalCount=1;
    const auto expired=normalWorkTiming(race,{{decision,1}},horizon);
    require(race.timer->globalCount==1 && expired.menuDecisionCountdownRead==0
        && !expired.callbacks.empty() && expired.phase.timer->globalCount==attractMenuCountdown,
        "attract lost captured-zero IRQ race/1618 renewal");
    PlayableAttractController c;require(c.menuDecision(*expired.menuDecisionCountdownRead,true),"captured expiry did not enter Briefing");
    auto noSelection=race;noSelection.sequenceSelection.reset();noSelection.playableCampaign=true;
    require(normalWorkTiming(noSelection,{{decision,1}},horizon).menuDecisionCountdownRead==0,
        "returned title incorrectly requires cold sequence-selection receipts");
    auto zero=phase;zero.timer->globalCount=0;
    rejects([&]{normalWorkTiming(zero,{{NormalWorkKind::startupMenuInputDecision,1}},horizon);});
    rejects([&]{normalWorkTiming(zero,{{NormalWorkKind::startupMenuDecision,1}},horizon);});
    auto inactive=phase;inactive.timer->activeMask=0;
    rejects([&]{normalWorkTiming(inactive,{{decision,1}},horizon);});
    const auto stored=normalWorkTiming(phase,{{NormalWorkKind::playableAttractCountdownStore,1,6000}},horizon);
    require(stored.phase.timer->globalCount==6000,"attract wrote a fitted countdown");
    require(stored.phase.sequenceSelection->logoFramesCompleted==phase.sequenceSelection->logoFramesCompleted
        &&stored.phase.sequenceSelection->creditsInputReturned==phase.sequenceSelection->creditsInputReturned,
        "attract reset cold one-shot metadata");
    const auto credits=normalWorkTiming(stored.phase,{{NormalWorkKind::playableAttractCountdownStore,1,7200}},horizon);
    const auto idle=normalWorkTiming(credits.phase,{{NormalWorkKind::playableAttractCreditsPoll,3}},horizon);
    require(!idle.countdownReturned && idle.countdownPolls==3,"credits invented a key or skipped its actual countdown");
    const auto space=normalWorkTiming(credits.phase,{{NormalWorkKind::playableAttractCreditsPoll,3,0x3920}},horizon);
    require(space.countdownReturned && space.countdownPolls==1 && space.creditsCountdownRead!=0,
        "B2B0 TEST AH,FF did not accept an actual non-Escape scan");
    auto creditsZero=credits.phase;creditsZero.timer->globalCount=0;
    const auto ended=normalWorkTiming(creditsZero,{{NormalWorkKind::playableAttractCreditsPoll,3}},horizon);
    require(ended.countdownReturned && ended.countdownPolls==1 && ended.creditsCountdownRead==0,
        "credits expiry was not decided from captured87CA zero");
}
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {run(argv[1]);std::cout<<"Attract captured IRQ expiry and strict guards PASS\n";return 0;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
