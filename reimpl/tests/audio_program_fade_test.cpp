#include "stellar7/audio_program_work.hpp"
#include "stellar7/audio_direct_work.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
#include <stdexcept>
#include <utility>

namespace {
using namespace stellar7;
enum class Path { wait, parsed, expiry, alias, terminalReentry, stoppedReentry, stoppedVoicesReentry, endedOnce, stop };

SoundBlasterCallbackState fixture(const Path path, const std::uint16_t fade,
    const std::uint16_t divider) {
    SoundBlasterCallbackState state{};
    state.program.emplace();
    auto& program = *state.program;
    program.instrumentBank.resize(96 * 28);
    for (std::size_t index = 0; index < program.instrumentBank.size(); ++index) {
        program.instrumentBank[index] = static_cast<std::uint8_t>((index * 7 + 13) % 64);
    }
    program.volumeTable.resize(16 * 64);
    for (std::size_t row = 0; row < 16; ++row) {
        for (std::size_t column = 0; column < 64; ++column) {
            program.volumeTable[row * 64 + column] = static_cast<std::uint8_t>((row * 3 + column * 5) % 64);
        }
    }
    state.fade = fade;
    program.fadeDivider = divider;
    state.sequence.resize(36);
    state.sequence.insert(state.sequence.end(), {0x91, 65, 90, 5, 0xfc});
    state.position = 36;
    state.wait = path == Path::parsed || path == Path::endedOnce ? 0 : 9;
    state.channelEnabled.fill(1);
    state.voices.channels.fill(0xff);
    state.voices.channels[0] = 1;
    for (std::size_t voice = 0; voice < state.sounding.size(); ++voice) {
        state.voices.notes[voice] = static_cast<std::uint8_t>(60 + voice);
        state.velocities[voice] = static_cast<std::uint8_t>(voice % 3 == 0 ? 0 : 32 + 8 * voice);
        state.sounding[voice] = voice == 2 || voice == 5 || (path == Path::parsed && voice == 0) ? 0 : 1;
        auto& level = program.voices[voice];
        level.carrierScale = static_cast<std::uint16_t>((voice + 1) % 4);
        level.carrierFallback = static_cast<std::uint16_t>(31 - voice);
        level.operatorScale = static_cast<std::uint16_t>(voice % 4);
        level.operatorFallback = static_cast<std::uint16_t>(20 + voice);
        level.operatorDisabled = voice % 2 == 0 ? 0 : 0xffff;
    }
    if (path == Path::alias) {
        state.velocities[0] = 0xff;
        state.latch = SoundBlasterLevelLatch::Latched;
    }
    if (path == Path::expiry) {
        state.voices.directTimers = {1, 2, 0};
    }
    if (path == Path::terminalReentry || path == Path::stoppedReentry
        || path == Path::stoppedVoicesReentry || path == Path::endedOnce) {
        state.active = false;
        if (path != Path::stoppedVoicesReentry) {
            state.sounding.fill(0);
        }
        program.handleStatus = 0xffff;
        program.repeatCount = path == Path::terminalReentry ? 0 : path == Path::endedOnce ? 1 : 0xffff;
    }
    return state;
}

std::uint64_t blockHash(const std::vector<SoundBlasterWorkBlock>& blocks) {
    constexpr std::uint64_t offset = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    auto hash = offset;
    for (const auto& block : blocks) {
        assert(block.kind == SoundBlasterWorkKind::plain || block.kind == SoundBlasterWorkKind::registerWrite);
        assert(block.voice == -1);
        for (const auto value : {static_cast<std::uint64_t>(block.kind), block.dispatches,
                 static_cast<std::uint64_t>(block.registerNumber), static_cast<std::uint64_t>(block.value)}) {
            hash = (hash ^ value) * prime;
        }
    }
    return hash;
}

template<class Function>
void rejects(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    auto beforeReset = fixture(Path::wait,7,9);
    beforeReset.program->repeatCount = 17;
    beforeReset.program->handleVolume = 6;
    for (std::size_t voice = 0; voice < beforeReset.program->voices.size(); ++voice) {
        beforeReset.program->voices[voice].program = static_cast<std::uint8_t>(voice+30);
    }
    const auto afterReset = soundBlasterDirectResetState(beforeReset);
    assert(afterReset.position == beforeReset.position && afterReset.wait == beforeReset.wait);
    assert(afterReset.velocities == beforeReset.velocities);
    assert(afterReset.program->repeatCount == 17 && afterReset.program->handleVolume == 6);
    assert(afterReset.program->operators == beforeReset.program->operators);
    for (std::size_t voice = 0; voice < beforeReset.program->voices.size(); ++voice) {
        const auto& old = beforeReset.program->voices[voice];
        const auto& now = afterReset.program->voices[voice];
        assert(now.program == old.program);
        assert(now.carrierFallback == old.carrierFallback && now.operatorFallback == old.operatorFallback);
    }
    struct Case {
        Path path;
        std::uint16_t fade, divider;
        std::uint64_t dispatches, hash;
        std::uint16_t nextFade, nextDivider, nextWait, repeat, status;
    };
    //Full original0494 source CPU receipts, synthetic tables/metadata/ABI.
    //Private program-fade-p21-20261018/controlled-cpu-v3.json records every PC/store/OUT.
    constexpr std::array<Case, 15> cases{{
        {Path::wait, 15, 7, 82, 0x86dc107e0b7e812fULL, 15, 6, 8, 0xffff, 0},
        {Path::wait, 15, 1, 2314, 0xdc7e5ddb858067a4ULL, 14, 20, 8, 0xffff, 0},
        {Path::wait, 1, 1, 2699, 0x0e0f495c2a66b5ebULL, 0, 0, 8, 0, 0xffff},
        {Path::parsed, 10, 7, 625, 0x02cf6c3be9fa75b0ULL, 10, 6, 4, 0xffff, 0},
        {Path::parsed, 3, 1, 2863, 0xcaa9ca74710e809dULL, 2, 20, 4, 0xffff, 0},
        {Path::parsed, 1, 1, 3242, 0x045e420751bf757eULL, 0, 0, 4, 0, 0xffff},
        {Path::expiry, 7, 1, 2605, 0x32d8e9722edb5809ULL, 6, 20, 8, 0xffff, 0},
        {Path::wait, 15, 0, 82, 0x86dc107e0b7e812fULL, 15, 0xffff, 8, 0xffff, 0},
        {Path::alias, 15, 1, 2320, 0x73c675a11787d5d9ULL, 14, 20, 8, 0xffff, 0},
        {Path::alias, 1, 1, 3797, 0x4e64807e0d7ecff7ULL, 0, 0, 8, 0, 0xffff},
        {Path::terminalReentry, 0, 0, 81, 0x8f858b7e10667fdaULL, 0, 0, 8, 0xffff, 0},
        {Path::stoppedReentry, 7, 7, 88, 0x52e32e7dee0e892dULL, 7, 6, 8, 0xffff, 0},
        {Path::stoppedVoicesReentry, 7, 7, 88, 0x52e32e7dee0e892dULL, 7, 6, 8, 0xffff, 0},
        {Path::endedOnce, 0, 20, 24, 0x7d41ee7f280e33edULL, 0, 20, 0, 1, 0xffff},
        {Path::stop, 15, 7, 2603, 0x3d2084129baa2443ULL, 15, 7, 9, 0xffff, 0}
    }};
    for (const auto& test : cases) {
        const auto input = fixture(test.path, test.fade, test.divider);
        auto actual = SoundBlasterCallbackAdvance{input, SoundBlasterCallbackKind::programControl};
        if (test.path == Path::stop) {
            auto stopped = soundBlasterStopAdvance(input);
            actual.state = std::move(stopped.state);
            actual.blocks = std::move(stopped.blocks);
            for (const auto& block : actual.blocks) {
                actual.dispatches += block.dispatches;
            }
        } else {
            actual = soundBlasterCallbackAdvance(input);
        }
        assert(actual.dispatches == test.dispatches && blockHash(actual.blocks) == test.hash);
        const auto& state = actual.state;
        const auto& program = *state.program;
        assert(state.fade == test.nextFade && program.fadeDivider == test.nextDivider);
        assert(state.wait == test.nextWait && program.repeatCount == test.repeat && program.handleStatus == test.status);
        assert(state.active == (test.status == 0));
        assert(state.position == (test.path == Path::parsed ? 40 : 36));
        assert(state.runningStatus == (test.path == Path::parsed ? 0x91 : 0));
        assert(state.operation == 2 && state.sequence == input.sequence);
        assert(state.voices.channels == input.voices.channels && state.voices.tuning == input.voices.tuning);
        assert(state.voices.channelDeferrals == input.voices.channelDeferrals && state.channelEnabled == input.channelEnabled);
        assert(program.instrumentBank == input.program->instrumentBank && program.volumeTable == input.program->volumeTable);
        assert(program.operators == input.program->operators && program.channelPrograms == input.program->channelPrograms);
        assert(program.handleVolume == input.program->handleVolume && program.accumulator == input.program->accumulator);
        assert(program.loopMarker == input.program->loopMarker && program.waveformMask == input.program->waveformMask);
        for (std::size_t voice = 0; voice < state.sounding.size(); ++voice) {
            assert(state.voices.notes[voice] == (test.path == Path::parsed && voice == 0 ? 65 : 60 + voice));
            assert(state.velocities[voice] == (test.path == Path::parsed && voice == 0 ? 90 : input.velocities[voice]));
            const bool silenced = test.status == 0xffff || test.path == Path::stop
                || (test.path == Path::expiry && voice == 6);
            const auto sounding = test.path == Path::parsed && voice == 0 ? 1 : input.sounding[voice];
            assert(state.sounding[voice] == (silenced ? 0 : sounding));
            const auto& before = input.program->voices[voice];
            const auto& after = program.voices[voice];
            assert(after.program == (test.path == Path::parsed && voice == 0 ? 0xff : before.program));
            assert(after.tuningMagnitude == before.tuningMagnitude && after.carrierScale == before.carrierScale
                && after.carrierFallback == before.carrierFallback && after.operatorScale == before.operatorScale
                && after.operatorFallback == before.operatorFallback && after.operatorDisabled == before.operatorDisabled);
        }
        auto cursors = input.channelVoiceCursors;
        if (test.path == Path::parsed) {
            cursors[1] = 1;
        }
        assert(state.channelVoiceCursors == cursors);
        assert((state.voices.directTimers == (test.path == Path::expiry
            ? std::array<std::uint16_t, 3>{0, 1, 0} : input.voices.directTimers)));
    }

    auto terminal = fixture(Path::wait, 1, 1);
    terminal.wait = 1;
    const auto ended = soundBlasterCallbackAdvance(terminal);
    assert(ended.state.wait == 0 && !ended.state.active && ended.state.program->repeatCount == 0);
    const auto resumed = soundBlasterCallbackAdvance(ended.state);
    assert(resumed.state.active && resumed.state.position == 40 && resumed.state.wait == 4);
    assert(resumed.state.program->repeatCount == 0xffff && resumed.state.program->handleStatus == 0);

    const auto deselectionInput = fixture(Path::wait, 7, 7);
    const auto deselected = soundBlasterDeselectState(deselectionInput);
    assert(!deselected.active && deselected.domain == SoundBlasterCallbackDomain::noHandle);
    assert(deselected.fade == 7 && deselected.sounding == deselectionInput.sounding);
    assert(deselected.voices.notes == deselectionInput.voices.notes && !deselected.program);

    auto unsupported = fixture(Path::wait, 16, 1);
    rejects([&]() { soundBlasterCallbackAdvance(unsupported); });
    unsupported = fixture(Path::wait, 15, 1);
    unsupported.program->volumeTable.clear();
    rejects([&]() { soundBlasterCallbackAdvance(unsupported); });
    unsupported = fixture(Path::wait, 15, 1);
    unsupported.program->repeatCount = 0;
    rejects([&]() { soundBlasterCallbackAdvance(unsupported); });
    unsupported = fixture(Path::parsed, 15, 1);
    unsupported.sequence[36] = 0xfc;
    rejects([&]() { soundBlasterCallbackAdvance(unsupported); });
    unsupported = fixture(Path::wait, 15, 1);
    unsupported.voices.directTimers[0] = 1;
    rejects([&]() { soundBlasterStopAdvance(unsupported); });
}
