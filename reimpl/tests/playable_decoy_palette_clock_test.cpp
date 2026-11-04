#include "keyboard_clock_access.hpp"
#include "playable_decoy.hpp"
#include <algorithm>
#include <stdexcept>

namespace {
using namespace stellar7;
void require(const bool condition,const char* message) {
    if(!condition) {throw std::runtime_error(message);}
}
template<class Call> void rejects(Call call) {
    bool rejected=false;
    try {call();} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"unqualified direct palette entry was admitted");
}
SoundBlasterClockPhase phase() {
    SoundBlasterClockPhase p{};
    p.remainingCycles=5000;p.interrupts=AudioClockInterrupts::enabled;
    p.isa8DelayNanoseconds=1020;p.dmaDacRate=44100;p.pitPeriodMilliseconds=55;
    p.irqDelayMilliseconds=0.002;p.singleLinePeriodMilliseconds=0.03;p.horizontalPeriodMilliseconds=0.03;
    p.linesTotal=400;p.minimumLatchDelayMilliseconds=0.001;
    p.frameStart=AudioClockFrameStart::activeUnlockedVgaSurface;
    p.interruptDelivery=AudioClockInterruptDelivery::residentTimerIrq0;
    p.vgaInterrupt=AudioClockVgaInterrupt::fixedVgaNoDemand;p.playableCampaign=true;
    p.devices=AudioClockDevicePhase{};p.devices->fixedPitAndMaskedDma=true;
    p.devices->framePeriodMilliseconds=14;p.devices->displayEndMilliseconds=12;
    p.devices->retraceStartMilliseconds=0.001;p.devices->retraceEndMilliseconds=10;
    p.devices->frame=AudioClockVgaFramePhase{1000.0/14,0,0};
    p.timer=AudioClockTimerPhase{1,0,5,20,20,5};
    return p;
}
void protocol(const PaletteDacMode mode) {
    PaletteWorkState input{};
    for(std::size_t index=0;index<vgaPaletteBytes;++index) {
        input.source[index]=static_cast<std::uint8_t>((index*11+5)%64);
        input.output[index]=static_cast<std::uint8_t>((index*13+9)%64);
        input.dac.colors[index]=static_cast<std::uint8_t>((index*7+3)%64);
    }
    input.dac.mode=mode;input.dac.writeIndex=201;input.dac.readIndex=200;
    input.dac.latch=std::array<std::uint8_t,3>{11,22,33};
    const auto value=playableDecoyPaletteClockTimingBound(phase(),input,240000,nullptr);
    const auto extra=mode==PaletteDacMode::write ? 1U : 0U;
    require(value.writes.size()==384+1+extra && value.commits.size()==128,"516B port/commit count differs");
    if(extra!=0) {require(value.writes.front().port==0x3c9 && value.writes.front().value==0,"missing preliminary DAC mode byte");}
    require(value.writes[extra].port==0x3c8 && value.writes[extra].value==0,"516B did not select color0");
    const auto raw=playableDecoyPaletteBytes();
    require(raw[85*3]==63 && raw[86*3]==66 && raw[87*3]==69,"source red ramp was incorrectly pre-masked");
    for(std::size_t index=0;index<raw.size();++index) {
        require(value.writes[index+extra+1].port==0x3c9 && value.writes[index+extra+1].value==raw[index],
            "raw OUT3C9 sequence changed");
        require(value.state.dac.colors[index]==(raw[index]&63),"six-bit DAC latch did not mask raw component");
    }
    require(std::equal(input.dac.colors.begin()+384,input.dac.colors.end(),value.state.dac.colors.begin()+384)
        && input.source==value.state.source && input.output==value.state.output,
        "direct1026 upload overwrote upper128/source/output");
    require(value.state.dac.writeIndex==128 && value.state.dac.readIndex==127
        && value.state.dac.pelIndex==0 && value.state.dac.mode==PaletteDacMode::write,
        "resident516B terminal DAC cursor differs");
    require(value.highReads>0 && value.uploadStartCycles<value.uploadEndCycles
        && value.uploadEndCycles<value.timing.total.elapsedCycles,
        "direct upload omitted retained retrace/CPU work");
    auto source=phase();source.playableCampaign=false;
    rejects([&]{playableDecoyPaletteClockTimingBound(source,input,240000,nullptr);});
    input.dac.mode=PaletteDacMode::write;input.dac.pelIndex=2;input.dac.latch.reset();
    rejects([&]{playableDecoyPaletteClockTimingBound(phase(),input,240000,nullptr);});
}
}
int main() {protocol(PaletteDacMode::write);protocol(PaletteDacMode::read);}
