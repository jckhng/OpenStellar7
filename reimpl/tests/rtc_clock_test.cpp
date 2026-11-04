#include "stellar7/normal_clock.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
stellar7::SoundBlasterClockPhase phase() {
    using namespace stellar7;
    SoundBlasterClockPhase p{};
    p.remainingCycles = 1;
    p.deferredCycles = 4999;
    p.interrupts = AudioClockInterrupts::masked;
    p.isa8DelayNanoseconds = 1020;
    p.pitPeriodMilliseconds = 55;
    p.singleLinePeriodMilliseconds = 0.03;
    p.horizontalPeriodMilliseconds = 0.03;
    p.linesTotal = 400;
    p.minimumLatchDelayMilliseconds = 0.001;
    p.vgaInterrupt = AudioClockVgaInterrupt::fixedVgaNoDemand;
    p.devices = AudioClockDevicePhase{};
    p.devices->fixedPitAndMaskedDma = true;
    p.devices->picTicks = 211000;
    p.devices->framePeriodMilliseconds = 14;
    p.devices->displayEndMilliseconds = 12;
    p.devices->retraceStartMilliseconds = 13;
    p.devices->retraceEndMilliseconds = 13.1;
    p.bios = AudioClockBiosPhase{};
    p.bios->domain = AudioClockBiosDomain::normalVgaDefaultTimer;
    p.rtc = AudioClockRtcPhase{};
    p.rtc->domain = AudioClockRtcDomain::inactiveNoAccess;
    p.rtc->registerB = 3;
    p.rtc->acknowledged = true;
    p.rtc->endedMilliseconds = 210000;
    p.rtc->delayMilliseconds = 0.9765625f;
    p.rtc->divider = 6;
    p.rtc->selected = 7;
    p.rtc->bcd = true;
    p.rtc->calendar = {18,10,5,2,12,10,2026};
    p.rtc->clockTime = 1791753018;
    p.events = {{0,AudioClockEventKind::pendingCmos,0}};
    return p;
}

stellar7::NormalWorkTiming advance(const stellar7::SoundBlasterClockPhase& p) {
    return stellar7::normalWorkTiming(p,{{stellar7::NormalWorkKind::instructions,1}},100);
}

bool rejects(const stellar7::SoundBlasterClockPhase& p) {
    try {
        advance(p);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}
}

int main() {
    using namespace stellar7;
    const auto p = phase();
    const auto result = advance(p);
    assert(result.total.dispatches == 1 && result.total.delayCycles == 0);
    assert(result.phase.rtc->endedMilliseconds == 211000);
    assert(result.phase.rtc->calendar[0] == 19 && result.phase.rtc->clockTime == p.rtc->clockTime+1);
    assert(result.phase.rtc->registerC == 0 && result.phase.rtc->acknowledged);
    assert(result.phase.events.size() == 1 && result.phase.events[0].kind == AudioClockEventKind::pendingCmos);
    const double index = 211000.0004;
    const auto delay = static_cast<float>(212000-index);
    assert(result.phase.events[0].deadlineMilliseconds == static_cast<double>(delay));
    assert(result.phase.events[0].deadlineMilliseconds != 1000);
    assert(p.rtc->calendar[0] == 18 && p.rtc->endedMilliseconds == 210000);

    const auto adjacent = advance(result.phase);
    const auto joined = normalWorkTiming(p,{{NormalWorkKind::instructions,2}},100);
    assert(adjacent.phase.rtc->calendar == joined.phase.rtc->calendar);
    assert(adjacent.phase.rtc->endedMilliseconds == joined.phase.rtc->endedMilliseconds);
    assert(adjacent.phase.events[0].deadlineMilliseconds == joined.phase.events[0].deadlineMilliseconds);
    auto equal = phase();
    equal.events.push_back({static_cast<double>(delay),AudioClockEventKind::pendingVgaInterrupt,0});
    const auto ordered = advance(equal);
    assert(ordered.phase.events[0].kind == AudioClockEventKind::pendingVgaInterrupt);
    assert(ordered.phase.events[1].kind == AudioClockEventKind::pendingCmos);

    auto early = phase();
    early.devices->picTicks = 1999;
    early.remainingCycles = 1;
    early.deferredCycles = 10;
    early.rtc->endedMilliseconds = 1000;
    early.events[0].deadlineMilliseconds = 0.998;
    const auto clamped = advance(early);
    assert(clamped.phase.rtc->endedMilliseconds == 1000);
    assert(clamped.phase.rtc->calendar == early.rtc->calendar);
    assert(clamped.phase.events[0].deadlineMilliseconds == 0.998 + static_cast<double>(0.01f));
    auto rollover = phase();
    rollover.devices->picTicks = 210999;
    rollover.deferredCycles = 0;
    rollover.events[0].deadlineMilliseconds = 0.999999;
    const auto rolled = advance(rollover);
    assert(rolled.phase.devices->picTicks == 211000 && rolled.phase.rtc->endedMilliseconds == 211000);
    const auto rolloverDelay = static_cast<float>(212000-211000.0002);
    assert(rolled.phase.events[0].deadlineMilliseconds == (0.999999-1.0) + static_cast<double>(rolloverDelay));

    auto control = phase();
    control.rtc->locked = true;
    const auto locked = advance(control);
    assert(locked.phase.rtc->endedMilliseconds == 211000);
    assert(locked.phase.rtc->calendar == control.rtc->calendar && locked.phase.rtc->clockTime == control.rtc->clockTime);
    control = phase();
    control.rtc->calendar = {59,59,23,7,30,1,2026};
    const auto day = advance(control);
    assert((day.phase.rtc->calendar == std::array<std::uint16_t,7>{0,0,0,1,1,2,2026}));
    control.rtc->calendar = {59,59,23,2,29,11,2026};
    assert((advance(control).phase.rtc->calendar == std::array<std::uint16_t,7>{0,0,0,3,1,1,2027}));
    control.rtc->calendar = {59,59,23,2,28,2,2024};
    assert((advance(control).phase.rtc->calendar == std::array<std::uint16_t,7>{0,0,0,3,1,3,2024}));
    control.rtc->calendar = {59,59,23,6,29,11,65535};
    assert((advance(control).phase.rtc->calendar == std::array<std::uint16_t,7>{0,0,0,7,1,1,0}));

    control = phase(); control.rtc.reset(); assert(rejects(control));
    control = phase(); control.bios.reset(); assert(rejects(control));
    control = phase(); control.bios->syncTime = true; assert(rejects(control));
    control = phase(); control.rtc->domain = AudioClockRtcDomain::unsupported; assert(rejects(control));
    for (const auto flag : {0x10,0x20,0x40}) {
        control = phase(); control.rtc->registerB |= flag; assert(rejects(control));
    }
    control = phase(); control.rtc->registerC = 0x40; assert(rejects(control));
    control = phase(); control.rtc->delayMilliseconds = 0; assert(rejects(control));
    control = phase(); control.rtc->endedMilliseconds = std::numeric_limits<double>::infinity(); assert(rejects(control));
    control = phase(); control.rtc->clockTime = std::numeric_limits<std::int64_t>::max(); assert(rejects(control));
    assert(control.rtc->clockTime == std::numeric_limits<std::int64_t>::max());
    control = phase(); control.events[0].value = 1; assert(rejects(control));
    control = phase(); control.events.push_back(control.events[0]); assert(rejects(control));
    control = phase(); control.events.clear(); assert(rejects(control));
    control = phase(); control.rtc->endedMilliseconds = -1; assert(rejects(control));
    control = phase(); control.events[0].kind = AudioClockEventKind::unsupported; assert(rejects(control));
}
