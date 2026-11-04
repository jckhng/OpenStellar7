#include "stellar7/success_control_work.hpp"
#include <algorithm>
#include <cassert>
#include <stdexcept>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
namespace {
using namespace stellar7;
SuccessControlInput fixture(const SuccessPaletteChannel channel=SuccessPaletteChannel::green,
    const std::uint16_t front=0xa000) {
    const auto back=static_cast<std::uint16_t>(front==0xa000?0xa800:0xa000);
    return {{SuccessControlStage::inactive,channel,SuccessPaletteCompletionSource::soundHandle},
        {back,front,0xa400,back,back},0,true};
}
std::uint64_t dispatches(const SuccessControlWork& w) {
    std::uint64_t n=0;for(const auto& step:w.steps){for(const auto op:step.operations){assert(op.kind==NormalWorkKind::instructions);n+=op.count;}}return n;
}
std::vector<SuccessControlChild> children(const SuccessControlWork& w) {
    std::vector<SuccessControlChild> out;for(const auto& step:w.steps){if(step.child){out.push_back(*step.child);}}return out;
}
template<class Function> void rejects(Function f) {
    bool rejected=false;try{f();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
}
stellar7::SoundBlasterClockPhase phase() {
    using namespace stellar7;
    SoundBlasterClockPhase p{};
    p.remainingCycles = 5000;
    p.interrupts = AudioClockInterrupts::enabled;
    p.isa8DelayNanoseconds = 1020;
    p.dmaDacRate = 44100;
    p.pitPeriodMilliseconds = 55;
    p.irqDelayMilliseconds = 0.002;
    p.singleLinePeriodMilliseconds = 0.03;
    p.horizontalPeriodMilliseconds = 0.03;
    p.linesTotal = 400;
    p.minimumLatchDelayMilliseconds = 0.001;
    p.frameStart = AudioClockFrameStart::activeUnlockedVgaSurface;
    p.interruptDelivery = AudioClockInterruptDelivery::residentTimerIrq0;
    p.vgaInterrupt = AudioClockVgaInterrupt::fixedVgaNoDemand;
    p.devices = AudioClockDevicePhase{};
    p.devices->fixedPitAndMaskedDma = true;
    p.devices->framePeriodMilliseconds = 14;
    p.devices->displayEndMilliseconds = 12;
    p.devices->retraceStartMilliseconds = 0.001;
    p.devices->retraceEndMilliseconds = 10;
    p.devices->frame = AudioClockVgaFramePhase{1000.0 / 14, 0, 0};
    p.timer = AudioClockTimerPhase{1, 0, 5, 20, 20, 5};
    return p;
}

SuccessPaletteClockTiming paletteResult(const SuccessControlInput& input,const std::uint16_t status) {
    auto p=phase();p.callback.emplace();p.callback->program.emplace();p.callback->program->handleStatus=status;
    return successPaletteClockTiming(p,initialSuccessPalette(input.control.channel),{},SuccessPaletteExecution::advance,
        input.control.completion,120000);
}
void sourceEntry() {
    for(const auto channel:{SuccessPaletteChannel::green,SuccessPaletteChannel::blue}) {
        for(const auto front:{0xa000,0xa800}) {
            const auto in=fixture(channel,static_cast<std::uint16_t>(front));const auto w=successEntryOperations(in);
            assert(dispatches(w)==865 && w.control.stage==SuccessControlStage::polling);
            assert(w.pages.front==in.pages.front&&w.pages.back==in.pages.back&&w.pages.cache==in.pages.cache);
            assert(w.pages.draw==in.pages.front&&w.pages.copy==in.pages.back);
            assert(w.initializedPalette&&w.paletteInitializationStepOffset);
            assert(*w.paletteInitializationStepOffset<w.steps.size());
            assert(w.initializedPalette->values==initialSuccessPalette(channel).values);
            const auto list=children(w);
            const std::vector<SuccessControlChildKind> expected{SuccessControlChildKind::entryStop,
                SuccessControlChildKind::activateSuccess,SuccessControlChildKind::cockpit,
                SuccessControlChildKind::planarCopy,SuccessControlChildKind::sight,SuccessControlChildKind::radar,
                SuccessControlChildKind::residentPalette,SuccessControlChildKind::planarCopy,SuccessControlChildKind::countdownStore};
            assert(list.size()==expected.size());for(std::size_t i=0;i<list.size();++i){assert(list[i].kind==expected[i]);}
            assert(list[0].sourceCaller==0x6f60&&!list[0].guestElapsedQualified);
            assert(list[1].sourceCaller==0x6f6c&&list[1].sourceEntry==0x2e6&&!list[1].guestElapsedQualified);
            assert(list[1].arguments[0]==0&&list[1].arguments[1]==1&&list[1].arguments[2]==0x12bc);
            assert((list[3].arguments==std::array<std::int32_t,6>{0,0,320,146,in.pages.cache,in.pages.back}));
            assert((list[7].arguments==std::array<std::int32_t,6>{0,0,320,200,in.pages.back,in.pages.front}));
            assert(list[6].sourceEntry==0x705c&&list[6].guestElapsedQualified);
            assert(list[8].sourceEntry==0x7072&&list[8].arguments[0]==992&&!list[8].guestElapsedQualified);
            auto next=in;next.control=w.control;next.pages=w.pages;
            const auto poll=successPollOperations(next);assert(dispatches(poll)==0);
            assert(poll.control.stage==SuccessControlStage::awaitingPalette&&children(poll).size()==1);
            assert(children(poll)[0].sourceEntry==0x7078);
            next.control=poll.control;
            const auto repeated=successPaletteReturnOperations(next,paletteResult(next,0));
            assert(repeated.control.stage==SuccessControlStage::polling&&repeated.steps.empty());
            const auto left=successPaletteReturnOperations(next,paletteResult(next,0xffff));
            assert(left.control.stage==SuccessControlStage::finished&&dispatches(left)==6);
            assert(children(left).size()==1&&children(left)[0].kind==SuccessControlChildKind::returnStop);
            assert(children(left)[0].sourceCaller==0x7204&&children(left)[0].sourceReturn==0x7209);
            assert(left.pages.draw==w.pages.draw&&left.pages.copy==w.pages.copy);
            auto done=next;done.control=left.control;
            rejects([&]{successPollOperations(done);});rejects([&]{successEntryOperations(done);});
        }
    }
}
void negatives() {
    auto in=fixture();in.graphicsEnabled=false;rejects([&]{successEntryOperations(in);});
    in=fixture();in.levelIndex=1;rejects([&]{successEntryOperations(in);});
    in=fixture();in.pages.cache=0xa800;rejects([&]{successEntryOperations(in);});
    in=fixture();in.pages.front=in.pages.back;rejects([&]{successEntryOperations(in);});
    in=fixture();rejects([&]{successPollOperations(in);});
    in.control.stage=SuccessControlStage::awaitingPalette;
    auto wrong=paletteResult(in,0);wrong.state.channel=SuccessPaletteChannel::blue;
    rejects([&]{successPaletteReturnOperations(in,wrong);});
    wrong=paletteResult(in,0);wrong.boundary=SuccessPaletteBoundary::initialUploadComplete;
    rejects([&]{successPaletteReturnOperations(in,wrong);});
    in.control.completion=SuccessPaletteCompletionSource::timer;
    auto p=phase();p.timer->globalCount=0;
    const auto completed=successPaletteClockTiming(p,initialSuccessPalette(in.control.channel),{},SuccessPaletteExecution::advance,
        SuccessPaletteCompletionSource::timer,120000);
    assert(successPaletteReturnOperations(in,completed).control.stage==SuccessControlStage::finished);
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
    sourceEntry();negatives();return 0;
}
