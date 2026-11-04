#include "playable_startup_resources.hpp"
#include "retained_menu_clock_domain.hpp"

#include <iostream>

namespace {
using namespace stellar7;
void require(const bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
template<class Action> void rejects(Action action) {
    bool rejected=false;
    try {action();}catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"strict original menu-expiry rejection changed");
}
#ifdef STELLAR7_IDLE_RED_CONTROL
constexpr auto nativeDecision=NormalWorkKind::startupMenuInputDecision;
#else
constexpr auto nativeDecision=NormalWorkKind::playableMenuNoAttractDecision;
#endif
void run(const std::filesystem::path& root) {
    const auto assets=naturalRoute::assets(root);
    const auto art=decodeNativeStartupAssets(naturalRoute::read(root/"STELART.RES"));
    const auto vga=decodeTaggedPayloads(naturalRoute::read(root/"STELLAR.RES"),{'V','G','A',':'});
    RecoveredFrontend frontend(assets,art,decodeFrameCopyProfile(vga.at(0)),10000,{},CampaignExecution::playable);
    for(unsigned call=0;frontend.startup().phase()!=NativeStartupPhase::awaitingMenuDecision&&call<10000;++call) {
        frontend.advanceStartup({});
    }
    require(frontend.startup().phase()==NativeStartupPhase::awaitingMenuDecision,"idle clock test missed actual menu entry");
    frontend.advanceStartup({}); //Actual native poll establishes unavailable-attract capability metadata.
    const auto phase=frontend.startup().recoveredMenuInput()->work.back().timing.phase;
    constexpr std::int64_t horizon=120000;
    const auto strict=normalWorkTiming(phase,{{NormalWorkKind::startupMenuInputDecision,1,0}},horizon);
    const auto native=normalWorkTiming(phase,{{nativeDecision,1,0}},horizon);
    require(strict.total.dispatches==native.total.dispatches&&strict.total.elapsedCycles==native.total.elapsedCycles
        &&strict.phase.timer->globalCount==native.phase.timer->globalCount
        &&strict.menuDecisionCountdownRead==native.menuDecisionCountdownRead,
        "positive-count native policy changed recovered decision work");

    //Declared controlled race, not a natural expiry fixture. Mirrors established pending-IRQ phase construction.
    auto race=phase;
    constexpr std::int64_t normalCycleQuantum=5000;
    const auto local=normalCycleQuantum-race.remainingCycles-race.deferredCycles;
    race.remainingCycles=1;race.deferredCycles=normalCycleQuantum-local-1;
    race.devices->irq0Requested=true;race.irqCheckRequested=true;
    race.timer->callbackCount=1;race.timer->globalCount=1;
    rejects([&]{normalWorkTiming(race,{{NormalWorkKind::startupMenuInputDecision,1,0}},horizon);});
    const auto renewed=normalWorkTiming(race,{{nativeDecision,1,0}},horizon);
    require(race.timer->globalCount==1&&renewed.menuDecisionCountdownRead==0
        &&!renewed.callbacks.empty()&&renewed.phase.timer->globalCount==menuCountdownRawIrqs,
        "IRQ expiry inside native decision did not capture zero then renew30000");
    require(renewed.phase.sequenceSelection->songOrdinal==1&&renewed.phase.sequenceSelection->recoveredMenuInput
        &&!renewed.phase.sequenceSelection->recoveredMenuActivated&&renewed.phase.sequenceSelection->beginFadeRequests==0
        &&renewed.phase.callback->sequence==phase.callback->sequence,
        "unavailable attract provider changed the source menu result or music resource owner");
    auto zero=phase;zero.timer->globalCount=0;
    rejects([&]{normalWorkTiming(zero,{{NormalWorkKind::startupMenuDecision,1,0}},horizon);});
    rejects([&]{normalWorkTiming(zero,{{NormalWorkKind::startupMenuInputDecision,1,0}},horizon);});
    auto inactive=zero;inactive.timer->activeMask=0;
    rejects([&]{normalWorkTiming(inactive,{{nativeDecision,1,0}},horizon);});
#ifndef STELLAR7_IDLE_RED_CONTROL
    auto sourceOnly=phase;sourceOnly.playableMenuNoAttract=false;
    rejects([&]{normalWorkTiming(sourceOnly,{{nativeDecision,1,0}},horizon);});
#endif
    std::cout<<"Native unavailable-attract IRQ expiry/renewal; positive work and strict guards PASS\n";
}
}
int main(int argc,char** argv) {
    if(argc!=2) {return 2;}
    try {run(argv[1]);return 0;}catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
