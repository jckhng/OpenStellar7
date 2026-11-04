#include "stellar7/audio.hpp"
#include "stellar7/audio_clock.hpp"
#include "stellar7/normal_clock.hpp"
#include "stellar7/audio_work.hpp"
#include "stellar7/presentation_clock.hpp"
#include "stellar7/frame_copy_work.hpp"
#include "stellar7/frame_tail_work.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

std::vector<std::uint8_t> oneNoteSequence() {
    std::vector<std::uint8_t> sequence(36, 0);
    sequence[0] = 0x84;
    sequence[5] = 1;
    sequence[6] = 0x24;
    const std::vector<std::uint8_t> commands{
        0xc1, 0, 0,
        0x91, 60, 64, 1,
        0x81, 60, 64, 0xfc,
    };
    sequence.insert(sequence.end(), commands.begin(), commands.end());
    return sequence;
}

std::vector<std::uint8_t> percussionSequence() {
    std::vector<std::uint8_t> sequence(36, 0);
    sequence[0] = 0x84;
    sequence[23] = 1;
    sequence[24] = 0x14;
    const std::vector<std::uint8_t> commands{
        0xca, 69, 0,
        0x9a, 57, 64, 1,
        0x8a, 57, 64, 0xfc,
    };
    sequence.insert(sequence.end(), commands.begin(), commands.end());
    return sequence;
}

std::vector<std::uint8_t> ownedChannelSequence() {
    std::vector<std::uint8_t> sequence(36, 0);
    sequence[0] = 0x84;
    sequence[11] = 2;
    sequence[12] = 0x04;
    sequence[13] = 2;
    sequence[14] = 0x04;
    const std::vector<std::uint8_t> commands{
        0xc5, 0, 0,
        0x95, 60, 64, 1,
        0x85, 60, 64, 0xfc,
    };
    sequence.insert(sequence.end(), commands.begin(), commands.end());
    return sequence;
}

std::vector<std::uint8_t> sevenVoiceSequence() {
    std::vector<std::uint8_t> sequence(36, 0);
    sequence[0] = 0x84;
    sequence[5] = 7;
    sequence[6] = 0x04;
    const std::vector<std::uint8_t> commands{
        0xc1, 0, 0,
        0x91, 60, 64, 0,
        0x91, 61, 64, 0,
        0x91, 62, 64, 0,
        0x91, 63, 64, 0,
        0x91, 64, 64, 0,
        0x91, 65, 64, 0,
        0x91, 66, 98, 1,
        0xfc,
    };
    sequence.insert(sequence.end(), commands.begin(), commands.end());
    return sequence;
}

std::vector<std::uint8_t> twoImmediateNotesSequence() {
    std::vector<std::uint8_t> sequence(36, 0);
    sequence[0] = 0x84;
    sequence[5] = 2;
    sequence[6] = 0x04;
    const std::vector<std::uint8_t> commands{
        0xc1, 0, 0,
        0x91, 60, 64, 0,
        0x91, 61, 64, 1,
        0xfc,
    };
    sequence.insert(sequence.end(), commands.begin(), commands.end());
    return sequence;
}

std::vector<std::uint8_t> defaultProgramNoteSequence() {
    std::vector<std::uint8_t> sequence(36, 0);
    sequence[0] = 0x84;
    sequence[5] = 1;
    sequence[6] = 0x04;
    const std::vector<std::uint8_t> commands{
        0x91, 60, 64, 1,
        0xfc,
    };
    sequence.insert(sequence.end(), commands.begin(), commands.end());
    return sequence;
}

std::vector<std::uint8_t> unownedChannelSequence() {
    std::vector<std::uint8_t> sequence(36, 0);
    sequence[0] = 0x84;
    sequence[5] = 1;
    sequence[6] = 0x04;
    for (unsigned event = 0; event < 9; ++event) {
        sequence.insert(sequence.end(), {0x99, 42, 104, 1});
    }
    sequence.push_back(0xfc);
    return sequence;
}

std::vector<std::uint8_t> repeatedNoteOffSequence() {
    std::vector<std::uint8_t> sequence(36, 0);
    sequence[0] = 0x84;
    sequence[5] = 1;
    sequence[6] = 0x04;
    const std::vector<std::uint8_t> commands{
        0x91, 60, 64, 0,
        0x81, 60, 64, 0,
        0x81, 60, 64, 1,
        0xfc,
    };
    sequence.insert(sequence.end(), commands.begin(), commands.end());
    return sequence;
}

std::vector<std::uint8_t> nineVoiceProgramSequence() {
    std::vector<std::uint8_t> sequence(36, 0);
    sequence[0] = 0x84;
    sequence[5] = 9;
    sequence[6] = 0x04;
    const std::vector<std::uint8_t> commands{
        0xc1, 0, 1,
        0xfc,
    };
    sequence.insert(sequence.end(), commands.begin(), commands.end());
    return sequence;
}

std::vector<std::uint8_t> instrumentResource() {
    constexpr std::size_t headerSize = 19;
    constexpr std::size_t bankHalfSize = 0x540;
    std::vector<std::uint8_t> resource(
        headerSize + bankHalfSize * 2 + 2,
        0
    );
    resource[0] = 0x89;
    resource[1] = 0x11;
    resource[headerSize + bankHalfSize] = 0xab;
    resource[headerSize + bankHalfSize + 1] = 0xcd;
    auto* patch = resource.data() + headerSize;
    patch[1] = 1;
    patch[3] = 15;
    patch[4] = 7;
    patch[7] = 7;
    patch[13 + 1] = 1;
    patch[13 + 3] = 15;
    patch[13 + 4] = 7;
    patch[13 + 7] = 7;
    auto* percussionPatch = patch + 28;
    percussionPatch[8] = 15;
    constexpr std::size_t directEvent14VolumeBankOffset = 0x60c;
    resource[
        headerSize
        + bankHalfSize
        + 2
        + (directEvent14VolumeBankOffset - bankHalfSize)
    ] = 0x3f;
    return resource;
}

}  // namespace

int main() {
    {
        //Synthetic level rows isolate fade lifecycle; original rows are verified privately.
        std::vector<std::uint8_t> table(16 * 64);
        for (std::size_t i = 0; i < table.size(); ++i) { table[i] = static_cast<std::uint8_t>(i % 64); }
        std::vector<std::uint8_t> driver(0x903 + table.size());
        const std::array<std::uint8_t, 8> signature{'b', 'l', 'a', 's', 't', 'd', 'r', 'v'};
        std::copy(signature.begin(), signature.end(), driver.begin() + 10);
        std::copy(table.begin(), table.end(), driver.begin() + 0x903);
        assert(stellar7::decodeSoundBlasterVolumeTable(driver) == table);
        auto presentation = oneNoteSequence();
        presentation.resize(36);
        presentation.insert(presentation.end(), {0xc1, 0, 0, 0x91, 60, 64, 0xf8, 0xf8, 100, 0xfc});
        stellar7::SoundBlasterSynth startup({}, instrumentResource(), oneNoteSequence(),
            presentation, {}, {}, oneNoteSequence(), table);
        startup.beginLogo();
        assert(startup.startupCountdown() == 240);
        bool rejected = false;
        try { startup.beginPresentation(); } catch (const std::logic_error&) { rejected = true; }
        assert(rejected);
        rejected = false;
        try { startup.beginLogo(); } catch (const std::logic_error&) { rejected = true; }
        assert(rejected);
        for (unsigned irq = 0; irq < 239; ++irq) { startup.advanceStartupIrq(); }
        rejected = false;
        try { startup.triggerLogoEvent(); } catch (const std::logic_error&) { rejected = true; }
        assert(rejected && startup.startupCountdown() == 1);
        startup.advanceStartupIrq();
        startup.clearRegisterWrites();
        startup.triggerLogoEvent();
        assert(startup.startupPhase() == stellar7::SoundBlasterStartupPhase::logoPlaying);
        assert(startup.driverState().directTimers[0] == 30);
        rejected = false;
        try { startup.beginPresentation(); } catch (const std::logic_error&) { rejected = true; }
        assert(rejected);
        // Native624c/6646 requests once-only completion after animation.
        assert(startup.startupPhase() == stellar7::SoundBlasterStartupPhase::logoPlaying);
        startup.render(stellar7::SoundBlasterSynth::sampleRate / 10);
        assert(startup.logoActive());
        startup.finishLogo();
        assert(startup.startupPhase() == stellar7::SoundBlasterStartupPhase::logoEnding);
        while (startup.logoActive()) { startup.render(1); }
        startup.beginPresentation();
        assert(startup.startupPhase() == stellar7::SoundBlasterStartupPhase::presentation);
        startup.fadePresentation();
        const auto beforeBegin = startup.driverState();
        startup.requestGameplay();
        assert(startup.callbackWorkState().fade == 15);
        assert(startup.driverState().fadeDivider == beforeBegin.fadeDivider);
        rejected = false;
        try { stellar7::soundBlasterCallbackAdvance(startup.callbackWorkState()); }
        catch (const std::invalid_argument&) { rejected = true; }
        assert(rejected);
        std::size_t samples = 0;
        while (startup.driverState().sequenceActive) {
            startup.render(441);
            samples += 441;
            assert(samples < stellar7::SoundBlasterSynth::sampleRate * 8);
        }
        const auto faded = startup.driverState();
        assert(faded.fade == 0 && faded.fadeDivider == 0);
        assert(faded.sequencePosition > 36 && faded.sequenceWait > 0);
        assert(faded.notes[0] == 60);
        for (unsigned irq = 0; irq < 480; ++irq) { startup.advanceStartupIrq(); }
        startup.beginGameplay();
        assert(startup.startupPhase() == stellar7::SoundBlasterStartupPhase::gameplay);
    }

    // Native1d57 routes program changes by logical owner; direct notes use
    // the selected physical slot even when music owns that voice.
    auto liveMusic = sevenVoiceSequence();
    liveMusic[5] = 0;
    liveMusic[6] = 0;
    liveMusic[15] = 7;
    liveMusic[16] = 4;
    for (std::size_t index = 36; index < liveMusic.size(); ++index) {
        if (liveMusic[index] == 0xc1 || liveMusic[index] == 0x91) {
            liveMusic[index] = static_cast<std::uint8_t>(liveMusic[index] + 5);
        }
    }
    liveMusic[liveMusic.size() - 2] = 240;
    stellar7::SoundBlasterSynth musicOwnedDirect({}, instrumentResource(), liveMusic);
    musicOwnedDirect.startMusic();
    musicOwnedDirect.render(1);
    const auto ownedBefore = musicOwnedDirect.driverState();
    musicOwnedDirect.clearRegisterWrites();
    musicOwnedDirect.triggerLevel1Event(26);
    const auto ownedAfter = musicOwnedDirect.driverState();
    const auto& ownedWrites = musicOwnedDirect.registerWrites();
    assert(ownedWrites.size() == 3);
    assert(ownedWrites[0].registerNumber == 0xa6 && ownedWrites[0].value == 0x20);
    assert(ownedWrites[1].registerNumber == 0xb6 && ownedWrites[1].value == 0x36);
    assert(ownedWrites[2].registerNumber == 0x53 && ownedWrites[2].value == 0x40);
    assert(ownedAfter.velocities == ownedBefore.velocities);
    assert(ownedAfter.channelVoiceCursors == ownedBefore.channelVoiceCursors);
    assert(ownedAfter.directTimers[0] == 30 && ownedAfter.directLevelLatch);

    stellar7::SoundBlasterSynth directOwned({}, instrumentResource(), oneNoteSequence());
    directOwned.startMusic();
    directOwned.clearRegisterWrites();
    directOwned.triggerLevel1Event(26);
    assert(directOwned.registerWrites().size() == 19);
    assert(directOwned.registerWrites()[16].registerNumber == 0xa6);

    // A delayed logo must finish before the presentation history begins.
    auto delayedLogo = sevenVoiceSequence();
    delayedLogo[delayedLogo.size() - 2] = 240;
    stellar7::SoundBlasterSynth logoHistory(
        {}, instrumentResource(), {}, {}, {}, {}, delayedLogo
    );
    logoHistory.startLevel1(0);
    assert(logoHistory.driverState().velocities[6] == 98);
    stellar7::SoundBlasterSynth missingLogo({}, instrumentResource());
    missingLogo.startLevel1(0);
    assert(missingLogo.driverState().velocities[6] == 0);

    {
        stellar7::SoundBlasterNoteOnState state{};
        state.channels.fill(255);
        state.channels[2] = 8;
        state.channelVoiceCursors[9] = 5;
        state.directTimers = {2, 3, 4};
        const std::vector<stellar7::SoundBlasterExplicitNoteOn> events{{9, 42, 104, 0}, {8, 42, 104, 4}};
        using stellar7::SoundBlasterFrequencyGate;
        using stellar7::SoundBlasterLevelLatch;
        const auto suppressed = stellar7::soundBlasterExplicitNoteOnWork(state, events,
            SoundBlasterFrequencyGate::Suppressed, SoundBlasterLevelLatch::Unlatched);
        assert(suppressed.dispatches == 401 && suppressed.portWrites == 0);
        assert(suppressed.channelVoiceCursors[9] == 6 && suppressed.channelVoiceCursors[8] == 3);
        assert(suppressed.returnedWait == 3 && suppressed.consumedBytes == 8);
        assert(state.channelVoiceCursors[9] == 5 && state.sounding[2] == 0);
        state.channelEnabled[8] = 1;
        const auto levelOnly = stellar7::soundBlasterExplicitNoteOnWork(state, events,
            SoundBlasterFrequencyGate::Suppressed, SoundBlasterLevelLatch::Unlatched);
        assert(levelOnly.dispatches == 557 && levelOnly.portWrites == 2);
        const auto open = stellar7::soundBlasterExplicitNoteOnWork(state, events,
            SoundBlasterFrequencyGate::Open, SoundBlasterLevelLatch::Unlatched);
        assert(open.dispatches == 812 && open.portWrites == 6 && open.portReads == 129);
        assert(open.events[0].eventStartDispatches == 66 && open.events[1].eventStartDispatches == 241);
        assert(open.events[1].frequencyStartDispatches == 473);
        assert(open.blocks[0].dispatches == 353 && open.blocks[2].dispatches == 46);
        const auto latched = stellar7::soundBlasterExplicitNoteOnWork(state, events,
            SoundBlasterFrequencyGate::Open, SoundBlasterLevelLatch::Latched);
        assert(latched.dispatches == 934 && latched.portWrites == 8);
        const auto rejects = [&events](const stellar7::SoundBlasterNoteOnState& invalid) {
            try {
                stellar7::soundBlasterExplicitNoteOnWork(invalid, events,
                    SoundBlasterFrequencyGate::Open, SoundBlasterLevelLatch::Unlatched);
            } catch (const std::invalid_argument&) {
                return true;
            }
            return false;
        };
        auto invalid = state;
        invalid.sounding[2] = 1;
        assert(rejects(invalid));
        invalid = state;
        invalid.tuning[2] = 1;
        assert(rejects(invalid));
        invalid = state;
        invalid.directTimers[0] = 1;
        assert(rejects(invalid));
        invalid = state;
        invalid.channelVoiceCursors[9] = 9;
        assert(rejects(invalid));
    }
    {
        // Original callback427->428: running status and ordered old-note replacement.
        stellar7::SoundBlasterCallbackState state{};
        state.voices.channels = {1,7,8,255,255,255,13,14,15};
        state.voices.notes = {36,36,42,0,0,0,0,0,0};
        state.sounding = {1,0,0,0,0,0,0,0,0};
        state.velocities = {113,121,94,0,0,0,104,73,84};
        state.channelEnabled[1] = 127;
        state.channelEnabled[7] = 127;
        state.channelEnabled[8] = 127;
        state.channelVoiceCursors = {0,1,0,0,0,0,7,2,3,6,0,0,0,0,0,0};
        state.runningStatus = 0x98;
        state.sequence = {0x91,37,112,0,0x99,36,127,0,42,66,0,0x97,36,121,0,0x98,42,66,2};
        const auto advance = stellar7::soundBlasterCallbackAdvance(state);
        assert(advance.dispatches == 2419 && advance.state.position == 19 && advance.state.wait == 1);
        assert(advance.state.runningStatus == 0x98 && advance.state.channelVoiceCursors[9] == 8);
        assert(advance.state.voices.notes[0] == 37 && advance.state.velocities[0] == 112);
        assert(advance.state.voices.notes[1] == 36 && advance.state.velocities[1] == 121);
        assert(advance.state.voices.notes[2] == 42 && advance.state.velocities[2] == 66);
        assert(advance.state.sounding[0] == 1 && advance.state.sounding[1] == 1 && advance.state.sounding[2] == 1);
        assert(state.voices.notes[0] == 36 && state.position == 0 && state.wait == 0);
        using Kind = stellar7::SoundBlasterWorkKind;
        const std::vector<Kind> expectedKinds{Kind::frequencyLow,Kind::frequencyHigh,Kind::carrierLevel,
            Kind::frequencyLow,Kind::frequencyHigh,Kind::carrierLevel,Kind::frequencyLow,Kind::frequencyHigh,
            Kind::carrierLevel,Kind::frequencyLow,Kind::frequencyHigh};
        const std::vector<int> expectedVoices{0,0,0,0,0,1,1,1,2,2,2};
        std::size_t writer = 0;
        for (const auto& block : advance.blocks) {
            if (block.kind == Kind::plain) {
                continue;
            }
            assert(writer < expectedKinds.size());
            assert(block.kind == expectedKinds[writer] && block.voice == expectedVoices[writer]);
            ++writer;
        }
        assert(writer == expectedKinds.size());
        stellar7::SoundBlasterClockPhase phase{};
        phase.remainingCycles = 5000;
        phase.interrupts = stellar7::AudioClockInterrupts::masked;
        phase.interruptDelivery = stellar7::AudioClockInterruptDelivery::blockedByIrq0;
        phase.isa8DelayNanoseconds = 1020;
        phase.pitPeriodMilliseconds = 4.224;
        phase.singleLinePeriodMilliseconds = 0.031777556985616684;
        phase.horizontalPeriodMilliseconds = 0.031777557100297914;
        phase.linesTotal = 400;
        phase.minimumLatchDelayMilliseconds = 12;
        const auto timing = stellar7::soundBlasterCallbackTiming(phase, advance.blocks, advance.kind);
        assert(timing.total.dispatches == advance.dispatches && timing.opl.size() == writer);
    }
    {
        // Retained raw32: key-off must free voice0 before the running positive note.
        stellar7::SoundBlasterCallbackState state{};
        state.voices.channels = {1,7,8,255,255,255,13,14,15};
        state.voices.notes = {37,36,42,0,0,0,0,0,0};
        state.sounding = {1,0,0,0,0,0,0,0,0};
        state.velocities = {112,121,85,0,0,0,104,73,84};
        for (const auto channel : {1,6,7,8}) {
            state.channelEnabled[channel] = 127;
        }
        state.channelVoiceCursors = {0,1,0,0,0,0,7,2,3,3,0,0,0,0,0,0};
        state.runningStatus = 0x98;
        constexpr std::size_t groupStart = 237;
        state.position = groupStart;
        state.sequence.resize(groupStart);
        const std::vector<std::uint8_t> messages{0x91,37,0,0,38,113,0,0x99,36,127,0,
            42,78,0,0x97,36,121,0,0x98,42,78,5};
        state.sequence.insert(state.sequence.end(), messages.begin(), messages.end());
        const auto advance = stellar7::soundBlasterCallbackAdvance(state);
        assert(advance.kind == stellar7::SoundBlasterCallbackKind::explicitNoteOn);
        assert(advance.dispatches == 2425 && advance.state.position == 259 && advance.state.wait == 4);
        assert(advance.state.runningStatus == 0x98);
        assert((advance.state.voices.notes == std::array<std::uint8_t,9>{38,36,42,0,0,0,0,0,0}));
        assert((advance.state.velocities == std::array<std::uint8_t,9>{113,121,78,0,0,0,104,73,84}));
        assert((advance.state.sounding == std::array<std::uint8_t,9>{1,1,1,0,0,0,0,0,0}));
        assert(advance.state.channelVoiceCursors[9] == 5);
        assert(advance.state.voices.channels == state.voices.channels);
        assert(advance.state.voices.directTimers == state.voices.directTimers);
        assert(advance.state.voices.channelDeferrals == state.voices.channelDeferrals);
        assert(advance.state.voices.tuning == state.voices.tuning);
        assert(advance.state.channelEnabled == state.channelEnabled && advance.state.sequence == state.sequence);
        assert(advance.state.latch == state.latch && advance.state.gate == state.gate);
        using Kind = stellar7::SoundBlasterWorkKind;
        const std::array<std::uint64_t,12> expectedPlain{155,22,152,46,22,506,46,22,163,46,22,79};
        const std::array<Kind,11> expectedKinds{Kind::frequencyLow,Kind::frequencyHigh,Kind::carrierLevel,
            Kind::frequencyLow,Kind::frequencyHigh,Kind::carrierLevel,Kind::frequencyLow,Kind::frequencyHigh,
            Kind::carrierLevel,Kind::frequencyLow,Kind::frequencyHigh};
        const std::array<int,11> expectedVoices{0,0,0,0,0,1,1,1,2,2,2};
        assert(advance.blocks.size() == expectedPlain.size() + expectedKinds.size());
        for (std::size_t index = 0; index < advance.blocks.size(); ++index) {
            const auto& block = advance.blocks[index];
            if (index % 2 == 0) {
                assert(block.kind == Kind::plain && block.dispatches == expectedPlain[index / 2]);
            } else {
                assert(block.kind == expectedKinds[index / 2] && block.voice == expectedVoices[index / 2]);
                assert(block.dispatches == 104);
            }
        }
        auto control = state;
        control.sequence[groupStart + 1] = 38;
        const auto replacement = stellar7::soundBlasterCallbackAdvance(control);
        assert(replacement.dispatches == 2548);
        assert(replacement.state.voices.notes == advance.state.voices.notes);
        assert(replacement.state.velocities == advance.state.velocities && replacement.state.sounding == advance.state.sounding);
        assert(replacement.state.channelVoiceCursors == advance.state.channelVoiceCursors);
        assert(replacement.blocks[0].dispatches == 365 && replacement.blocks[4].dispatches == 66);
        assert(state.position == groupStart && state.voices.notes[0] == 37 && state.sounding[0] == 1);
        const auto rejects = [](const stellar7::SoundBlasterCallbackState& invalid) {
            try {
                stellar7::soundBlasterCallbackAdvance(invalid);
            } catch (const std::invalid_argument&) {
                return true;
            }
            return false;
        };
        for (unsigned mutation = 0; mutation < 5; ++mutation) {
            auto invalid = state;
            if (mutation == 0) { invalid.voices.directTimers[0] = 1; }
            if (mutation == 1) { invalid.voices.tuning[0] = 1; }
            if (mutation == 2) { invalid.voices.channelDeferrals[1] = 1; }
            if (mutation == 3) { invalid.sequence[groupStart + 5] = 0; }
            if (mutation == 4) { invalid.channelVoiceCursors[9] = 9; }
            assert(rejects(invalid));
        }
    }
    {
        stellar7::SoundBlasterSynth synth({}, instrumentResource(), unownedChannelSequence());
        synth.startMusic();
        synth.clearRegisterWrites();
        const std::array<std::uint8_t, 9> expected{2, 3, 4, 5, 6, 7, 8, 0, 2};
        for (const auto cursor : expected) {
            const auto before = synth.driverState();
            const auto remaining = before.clockPeriod - before.clockPhase;
            constexpr std::uint64_t pitInputClock = 1193182;
            synth.render(static_cast<std::size_t>((remaining + pitInputClock - 1) / pitInputClock) + 1);
            const auto after = synth.driverState();
            assert(after.channelVoiceCursors[9] == cursor);
            assert(after.notes == before.notes && after.velocities == before.velocities);
            assert(synth.registerWrites().empty());
        }
    }
    {
        stellar7::SoundBlasterKeyOffState state{};
        state.channels.fill(255);
        state.channels[2] = 8;
        state.notes[2] = 42;
        state.directTimers = {2, 3, 4};
        const std::vector<stellar7::SoundBlasterExplicitKeyOff> events{{9, 42, 0}, {8, 42, 2}};
        using stellar7::SoundBlasterFrequencyGate;
        using stellar7::SoundBlasterLevelLatch;
        const auto suppressed = stellar7::soundBlasterExplicitKeyOffWork(state, events,
            SoundBlasterFrequencyGate::Suppressed, SoundBlasterLevelLatch::Unlatched);
        assert(suppressed.dispatches == 358);
        assert(suppressed.portReads == 0 && suppressed.portWrites == 0);
        assert(suppressed.returnedWait == 1 && suppressed.consumedBytes == 8);
        assert(suppressed.events[0].eventStartDispatches == 66);
        assert(suppressed.events[1].eventStartDispatches == 195);
        assert(suppressed.events[1].frequencyStartDispatches == 273);
        assert(suppressed.events[0].selectedVoice == -1 && suppressed.events[1].selectedVoice == 2);
        assert(suppressed.blocks.size() == 1 && suppressed.blocks[0].dispatches == 358);
        assert((suppressed.timerWork.timers == std::array<std::uint16_t, 3>{1, 2, 3}));
        assert((state.directTimers == std::array<std::uint16_t, 3>{2, 3, 4}));
        assert(state.notes[2] == 42);
        const auto open = stellar7::soundBlasterExplicitKeyOffWork(state, events,
            SoundBlasterFrequencyGate::Open, SoundBlasterLevelLatch::Unlatched);
        assert(open.dispatches == 613 && open.portReads == 86 && open.portWrites == 4);
        assert(open.blocks.size() == 5);
        assert(open.blocks[0].dispatches == 303 && open.blocks[2].dispatches == 22);
        assert(open.blocks[1].kind == stellar7::SoundBlasterWorkKind::frequencyLow);
        assert(open.blocks[3].kind == stellar7::SoundBlasterWorkKind::frequencyHigh);
        const auto latched = stellar7::soundBlasterExplicitKeyOffWork(state, events,
            SoundBlasterFrequencyGate::Open, SoundBlasterLevelLatch::Latched);
        assert(latched.dispatches == 735 && latched.portReads == 129 && latched.portWrites == 6);
        assert(latched.blocks[5].kind == stellar7::SoundBlasterWorkKind::carrierLevel);
        const auto repeated = stellar7::soundBlasterExplicitKeyOffWork(state, {{8, 42, 0}, {8, 42, 2}},
            SoundBlasterFrequencyGate::Open, SoundBlasterLevelLatch::Unlatched);
        assert(repeated.portWrites == 8);
        assert(repeated.events[0].selectedVoice == 2 && repeated.events[1].selectedVoice == 2);
        const auto rejects = [&events](const stellar7::SoundBlasterKeyOffState& input) {
            try {
                stellar7::soundBlasterExplicitKeyOffWork(input, events,
                    SoundBlasterFrequencyGate::Suppressed, SoundBlasterLevelLatch::Unlatched);
            } catch (const std::invalid_argument&) {
                return true;
            }
            return false;
        };
        auto invalid = state;
        invalid.directTimers[0] = 1;
        assert(rejects(invalid));
        invalid = state;
        invalid.channelDeferrals[8] = 1;
        assert(rejects(invalid));
        invalid = state;
        invalid.tuning[2] = 1;
        assert(rejects(invalid));
    }
    {
        stellar7::SoundBlasterClockPhase phase{};
        phase.remainingCycles = 5000;
        phase.interrupts = stellar7::AudioClockInterrupts::masked;
        phase.isa8DelayNanoseconds = 1020;
        phase.pitPeriodMilliseconds = 4.224;
        phase.singleLinePeriodMilliseconds = 0.031777556985616684;
        phase.horizontalPeriodMilliseconds = 0.031777557100297914;
        phase.linesTotal = 400;
        phase.minimumLatchDelayMilliseconds = 12;
        const auto full = stellar7::soundBlasterOplTiming(phase);
        assert(full[0].dispatches == 22 && full[1].dispatches == 81);
        assert(full[0].elapsedCycles == 67 && full[1].elapsedCycles == 343);
        assert(full[0].delayCycles + full[1].delayCycles == 307);
        assert(full[0].terminalCharges + full[1].terminalCharges == 0);
        assert(full[1].remainingCycles == 4590);
        auto clamped = phase;
        clamped.remainingCycles = 11;
        clamped.deferredCycles = 4989;
        const auto signedClamp = stellar7::soundBlasterOplTiming(clamped);
        assert(signedClamp[0].delayCycles == 38);
        assert(signedClamp[0].elapsedCycles == 61);
        assert(signedClamp[0].terminalCharges == 1);
        assert(signedClamp[1].elapsedCycles == 343);
        assert(phase.remainingCycles == 5000 && phase.events.empty());

        using stellar7::NormalWorkKind;
        {
            // A known future identity is retained; RTC execution still rejects.
            constexpr auto pendingCmos = stellar7::AudioClockEventKind::pendingCmos;
            constexpr double deadlineMilliseconds = 0.2;
            constexpr std::int64_t deadlineCycles = 1000;
            auto pending = phase;
            pending.remainingCycles = deadlineCycles;
            pending.deferredCycles = 5000 - deadlineCycles;
            pending.events = {{deadlineMilliseconds, pendingCmos, 0}};
            const auto first = stellar7::normalWorkTiming(pending,
                {{NormalWorkKind::instructions, 3}}, deadlineCycles);
            const auto second = stellar7::normalWorkTiming(first.phase,
                {{NormalWorkKind::instructions, 19}}, deadlineCycles);
            const auto joined = stellar7::normalWorkTiming(pending,
                {{NormalWorkKind::instructions, 22}}, deadlineCycles);
            assert(first.total.elapsedCycles + second.total.elapsedCycles == joined.total.elapsedCycles);
            assert(second.phase.remainingCycles == joined.phase.remainingCycles);
            assert(second.phase.deferredCycles == joined.phase.deferredCycles);
            assert(second.phase.events.size() == 1 && second.total.boundedEvents.size() == 1);
            assert(second.phase.events.front().kind == pendingCmos);
            assert(second.phase.events.front().deadlineMilliseconds == deadlineMilliseconds);
            assert(second.phase.events.front().value == 0);
            assert(second.total.boundedEvents.front().kind == pendingCmos);
            assert(second.total.boundedEvents.front().deadlineMilliseconds == deadlineMilliseconds);
            assert(second.total.boundedEvents.front().value == 0);
            const auto rejects = [](const auto& entry, const std::uint64_t dispatches) {
                try {
                    stellar7::normalWorkTiming(entry,
                        {{NormalWorkKind::instructions, dispatches}}, 2000);
                } catch (const std::invalid_argument&) {
                    return true;
                }
                return false;
            };
            assert(rejects(pending, deadlineCycles + 1));
            assert(pending.remainingCycles == deadlineCycles && pending.events.size() == 1);
            auto unknown = pending;
            unknown.events.front().kind = stellar7::AudioClockEventKind::unsupported;
            assert(rejects(unknown, 1));
        }
        const std::vector<stellar7::NormalWorkOperation> prefix{
            {NormalWorkKind::instructions, 3}, {NormalWorkKind::repeatString, 10},
            {NormalWorkKind::port16Write, 1}};
        const std::vector<stellar7::NormalWorkOperation> suffix{
            {NormalWorkKind::string, 3}, {NormalWorkKind::repeatString, 9},
            {NormalWorkKind::port8Write, 1}};
        auto joined = prefix;
        joined.insert(joined.end(), suffix.begin(), suffix.end());
        auto retained = phase;
        retained.remainingCycles = 7;
        retained.events = {{1.0004, stellar7::AudioClockEventKind::scanline, 0}};
        const auto uninterrupted = stellar7::normalWorkTiming(retained, joined, 1000);
        const auto first = stellar7::normalWorkTiming(retained, prefix, 1000);
        const auto second = stellar7::normalWorkTiming(first.phase, suffix, 1000);
        assert(first.total.elapsedCycles + second.total.elapsedCycles == uninterrupted.total.elapsedCycles);
        assert(first.total.dispatches + second.total.dispatches == uninterrupted.total.dispatches);
        assert(first.total.terminalCharges + second.total.terminalCharges == uninterrupted.total.terminalCharges);
        assert(first.stringRestarts + second.stringRestarts == uninterrupted.stringRestarts);
        assert(first.stringIterations + second.stringIterations == uninterrupted.stringIterations);
        assert(first.total.millisecondRollovers + second.total.millisecondRollovers == uninterrupted.total.millisecondRollovers);
        assert(second.phase.remainingCycles == uninterrupted.phase.remainingCycles);
        assert(second.phase.deferredCycles == uninterrupted.phase.deferredCycles);
        assert(second.phase.linesDone == uninterrupted.phase.linesDone && second.phase.linesDone == 1);
        assert(second.phase.events.size() == uninterrupted.phase.events.size());
        assert(second.phase.events[0].deadlineMilliseconds == uninterrupted.phase.events[0].deadlineMilliseconds);
        assert(retained.remainingCycles == 7 && retained.linesDone == 0);

        // Joined rendering can exceed the old 12ms guard without changing work.
        constexpr std::uint64_t longDispatches = 90000;
        constexpr std::int64_t extendedHorizon = 120000;
        const auto longWhole = stellar7::normalWorkTiming(phase,
            {{NormalWorkKind::instructions, longDispatches}}, extendedHorizon);
        const auto longFirst = stellar7::normalWorkTiming(phase,
            {{NormalWorkKind::instructions, longDispatches / 2}}, extendedHorizon / 2);
        const auto longSecond = stellar7::normalWorkTiming(longFirst.phase,
            {{NormalWorkKind::instructions, longDispatches / 2}}, extendedHorizon / 2);
        assert(longWhole.total.dispatches == longDispatches);
        assert(longWhole.total.elapsedCycles == longFirst.total.elapsedCycles + longSecond.total.elapsedCycles);
        assert(longWhole.total.terminalCharges == longFirst.total.terminalCharges + longSecond.total.terminalCharges);
        assert(longWhole.phase.remainingCycles == longSecond.phase.remainingCycles);
        assert(longWhole.phase.deferredCycles == longSecond.phase.deferredCycles);
        bool rejectedLongHorizon = false;
        try {
            stellar7::normalWorkTiming(phase, {{NormalWorkKind::instructions, 1}}, extendedHorizon + 1);
        } catch (const std::invalid_argument&) {
            rejectedLongHorizon = true;
        }
        assert(rejectedLongHorizon);

        auto vga = phase;
        vga.remainingCycles = 1;
        vga.deferredCycles = 4999;
        vga.frameStart = stellar7::AudioClockFrameStart::activeUnlockedVgaSurface;
        vga.events = {{0.0002, stellar7::AudioClockEventKind::pendingVgaInterrupt, 0}};
        bool rejectedVga = false;
        try {
            stellar7::normalWorkTiming(vga, {{NormalWorkKind::instructions, 3}}, 1000);
        } catch (const std::invalid_argument&) {
            rejectedVga = true;
        }
        assert(rejectedVga);
        vga.vgaInterrupt = stellar7::AudioClockVgaInterrupt::fixedVgaNoDemand;
        const auto vgaService = stellar7::normalWorkTiming(vga, {{NormalWorkKind::instructions, 3}}, 1000);
        assert(vgaService.total.elapsedCycles == 4 && vgaService.total.terminalCharges == 1);
        assert(vgaService.phase.events.empty() && vgaService.phase.linesDone == 0);
        vga.events[0].value = 1;
        rejectedVga = false;
        try {
            stellar7::normalWorkTiming(vga, {{NormalWorkKind::instructions, 3}}, 1000);
        } catch (const std::invalid_argument&) {
            rejectedVga = true;
        }
        assert(rejectedVga);

        auto devicePhase = phase;
        devicePhase.vgaInterrupt = stellar7::AudioClockVgaInterrupt::fixedVgaNoDemand;
        stellar7::AudioClockDevicePhase devices{};
        devices.picTicks = 100;
        devices.pitStartMilliseconds = 100;
        devices.pitNowMilliseconds = 100;
        devices.fixedPitAndMaskedDma = true;
        devices.framePeriodMilliseconds = 14.268064195484556;
        devices.frameStartMilliseconds = 100;
        devices.displayEndMilliseconds = 12.711022840119165;
        devices.retraceStartMilliseconds = 13.092353525322741;
        devices.retraceEndMilliseconds = 13.155908639523336;
        devices.displayStart = 0x12345;
        devices.memoryMask = 0xffff;
        devices.configuredBytesSkip = 3;
        devices.pelPanning = 7;
        devices.verticalRetraceEndRegister = 0x10;
        devicePhase.devices = devices;
        devicePhase.events = {{0.0002, stellar7::AudioClockEventKind::displayStartLatch, 0},
            {0.0004, stellar7::AudioClockEventKind::panningLatch, 0},
            {0.0006, stellar7::AudioClockEventKind::pendingVgaInterrupt, 0},
            {2.2002, stellar7::AudioClockEventKind::pit, 0}};
        devicePhase.remainingCycles = 1;
        devicePhase.deferredCycles = 4999;
        const auto latched = stellar7::normalWorkTiming(devicePhase, {{NormalWorkKind::instructions, 5}}, 15000);
        assert(latched.phase.devices->realStart == 0x2345);
        assert(latched.phase.devices->drawnBytesSkip == 3 && latched.phase.devices->drawnPanning == 7);
        assert(latched.phase.devices->verticalRetraceTriggered);
        const auto recurring = stellar7::normalWorkTiming(latched.phase, {{NormalWorkKind::instructions, 11000}}, 15000);
        assert(recurring.phase.devices->picTicks == 102);
        assert(recurring.phase.devices->pitCycleBase == 1);
        assert(recurring.phase.devices->pitStartMilliseconds == 100 + phase.pitPeriodMilliseconds / 2);
        assert(recurring.phase.events.size() == 1);
        assert(recurring.phase.events.front().kind == stellar7::AudioClockEventKind::pit);
        assert(recurring.phase.events.front().deadlineMilliseconds > 4);
        assert(devicePhase.devices->realStart == 0 && devicePhase.devices->picTicks == 100);

        auto framePhase = devicePhase;
        framePhase.linesDone = 399;
        framePhase.frameStart = stellar7::AudioClockFrameStart::activeUnlockedVgaSurface;
        framePhase.devices->frame = stellar7::AudioClockVgaFramePhase{70, 100, 0};
        framePhase.events = {{0.0002, stellar7::AudioClockEventKind::scanline, 0},
            {0.001, stellar7::AudioClockEventKind::frameStart, 0}};
        const auto correctedFrame = stellar7::normalWorkTiming(framePhase, {{NormalWorkKind::instructions, 10}}, 1000);
        assert(correctedFrame.phase.devices->frame->framesSinceTimeBase == 1);
        assert(correctedFrame.phase.devices->frameStartMilliseconds == 100.001);
        const auto nextFrame = std::find_if(correctedFrame.phase.events.begin(), correctedFrame.phase.events.end(),
            [](const auto& event) { return event.kind == stellar7::AudioClockEventKind::frameStart; });
        assert(nextFrame != correctedFrame.phase.events.end());
        assert(nextFrame->deadlineMilliseconds == 0.001 + (static_cast<float>(devices.framePeriodMilliseconds) + 0.1));
        framePhase.devices->frame->timeBaseMilliseconds = 0;
        const auto earlyFrame = stellar7::normalWorkTiming(framePhase, {{NormalWorkKind::instructions, 10}}, 1000);
        const auto delayedFrame = std::find_if(earlyFrame.phase.events.begin(), earlyFrame.phase.events.end(),
            [](const auto& event) { return event.kind == stellar7::AudioClockEventKind::frameStart; });
        assert(delayedFrame != earlyFrame.phase.events.end());
        assert(delayedFrame->deadlineMilliseconds == 0.001 + (static_cast<float>(devices.framePeriodMilliseconds) - 0.1));

        auto irqPhase = phase;
        irqPhase.interrupts = stellar7::AudioClockInterrupts::enabled;
        irqPhase.interruptDelivery = stellar7::AudioClockInterruptDelivery::residentTimerIrq0;
        irqPhase.vgaInterrupt = stellar7::AudioClockVgaInterrupt::fixedVgaNoDemand;
        irqPhase.devices = devices;
        irqPhase.devices->irq0Requested = true;
        irqPhase.timer = stellar7::AudioClockTimerPhase{1, 0, 5, 3, 4};
        irqPhase.remainingCycles = 1;
        irqPhase.deferredCycles = 4999;
        irqPhase.events = {{0.0002, stellar7::AudioClockEventKind::irqCheck, 0}};
        const auto timerIrq = stellar7::normalWorkTiming(irqPhase, {{NormalWorkKind::instructions, 150}}, 1000);
        assert(timerIrq.total.dispatches == 196 && timerIrq.total.delayCycles == 3);
        assert(timerIrq.phase.timer->globalCount == 0 && timerIrq.phase.timer->biosCount == 4);
        assert(timerIrq.phase.timer->callbackCount == 2 && !timerIrq.phase.devices->irq0InService);
        assert(!timerIrq.phase.devices->irq0Requested);
        assert(timerIrq.phase.interrupts == stellar7::AudioClockInterrupts::enabled);
        assert(irqPhase.timer->biosCount == 5 && irqPhase.devices->irq0Requested);
        auto callbackIrqPhase = irqPhase;
        callbackIrqPhase.timer->callbackCount = 1;
        callbackIrqPhase.callback = stellar7::SoundBlasterCallbackState{};
        callbackIrqPhase.callback->wait = 2;
        const auto callbackIrq = stellar7::normalWorkTiming(callbackIrqPhase,
            {{NormalWorkKind::instructions, 150}}, 1000);
        assert(callbackIrq.total.dispatches == 271 && callbackIrq.total.delayCycles == 3);
        assert(callbackIrq.phase.timer->callbackCount == 4 && callbackIrq.phase.callback->wait == 1);
        assert(callbackIrq.callbacks.size() == 1 && callbackIrq.callbacks.front().advance.dispatches == 73);
        assert(callbackIrq.callbacks.front().cycleOffset > 0);
        assert(callbackIrqPhase.callback->wait == 2 && callbackIrqPhase.timer->callbackCount == 1);
        {
            auto flags = irqPhase;
            flags.interrupts = stellar7::AudioClockInterrupts::masked;
            flags.remainingCycles = 5000;
            flags.deferredCycles = 0;
            flags.events.clear();
            flags.irqCheckRequested = true;
            flags.irq0ActivationPossible = true;
            const auto shadow = stellar7::normalWorkTiming(flags,
                {{NormalWorkKind::sti, 1}, {NormalWorkKind::cli, 1}, {NormalWorkKind::instructions, 10}}, 1000);
            assert(shadow.total.dispatches == 12);
            assert(shadow.phase.interrupts == stellar7::AudioClockInterrupts::masked);
            assert(shadow.phase.timer->biosCount == flags.timer->biosCount);
            assert(shadow.phase.devices->irq0Requested);
            const auto enabled = stellar7::normalWorkTiming(flags,
                {{NormalWorkKind::sti, 1}, {NormalWorkKind::instructions, 1}}, 1000);
            assert(enabled.total.dispatches == 2 + 46);
            assert(enabled.phase.timer->biosCount == flags.timer->biosCount - 1);
            const auto popped = stellar7::normalWorkTiming(flags,
                {{NormalWorkKind::popFlagsEnabled, 1}}, 1000);
            assert(popped.total.dispatches == 1 + 46);
            assert(popped.phase.timer->biosCount == enabled.phase.timer->biosCount);
            assert(popped.total.terminalCharges < enabled.total.terminalCharges);
            const auto first = stellar7::normalWorkTiming(flags, {{NormalWorkKind::sti, 1}}, 1000);
            assert(first.phase.remainingCycles == 1 && first.phase.devices->irq0Requested);
            const auto second = stellar7::normalWorkTiming(first.phase,
                {{NormalWorkKind::cli, 1}, {NormalWorkKind::instructions, 10}}, 1000);
            assert(first.total.elapsedCycles + second.total.elapsedCycles == shadow.total.elapsedCycles);
            assert(second.phase.remainingCycles == shadow.phase.remainingCycles);
            bool rejectedFlags = false;
            try {
                stellar7::normalWorkTiming(flags, {{NormalWorkKind::sti, 2}}, 1000);
            } catch (const std::invalid_argument&) {
                rejectedFlags = true;
            }
            assert(rejectedFlags);
        }
        {
            auto sequence = defaultProgramNoteSequence();
            sequence.resize(36);
            sequence.insert(sequence.end(), {0xb1, 0x4e, 127, 0, 0xb1, 7, 85, 0, 0xc1, 0, 3,
                0x91, 60, 64, 3, 0x91, 60, 0, 3, 0xfc});
            stellar7::SoundBlasterSynth shared({}, instrumentResource(), sequence);
            shared.startMusic();
            shared.render(1);
            const auto waitState = shared.callbackWorkState();
            assert(waitState.wait == 2 && waitState.channelEnabled[1] == 127);
            assert((waitState.voices.channels == std::array<std::uint8_t, 9>{1, 255, 255, 255, 255, 255, 13, 14, 15}));
            stellar7::PresentationClock sharedClock(shared);
            auto incoming = callbackIrqPhase;
            incoming.callback->wait = 65000; // This external copy must not own the live sequence.
            const auto rendered = sharedClock.renderWork(incoming, {{NormalWorkKind::instructions, 150}}, 1000);
            assert(rendered.timing.callbacks.size() == 1);
            assert(shared.driverState().sequenceWait == 1);
            assert(shared.driverState().clockSource == stellar7::SoundBlasterClockSource::normalWork);
            assert(shared.driverState().normalClockCycles == static_cast<std::uint64_t>(rendered.timing.total.elapsedCycles));
            assert(rendered.audio.writes.empty());
            assert(rendered.audio.samples.size() == sharedClock.elapsedSamples());
            assert(rendered.audio.samples.size() == (rendered.timing.total.elapsedCycles * 44100 + 4'999'999) / 5'000'000);
            bool refusedTicker = false;
            try {
                sharedClock.renderFrame();
            } catch (const std::logic_error&) {
                refusedTicker = true;
            }
            assert(refusedTicker);
            stellar7::CockpitWorkBitmaps bitmaps;
            bitmaps.strip = {4, 1, {0, 0, 0, 0}};
            bitmaps.indicator = bitmaps.strip;
            bitmaps.overlays.fill(bitmaps.strip);
            const auto cockpitPlan = stellar7::cockpitOperations(
                {160, 4, 71, stellar7::CockpitPageRelation::differing}, bitmaps);
            assert(cockpitPlan.effects.size() == 9);
            const auto copy = std::get<stellar7::CockpitCopyRegion>(cockpitPlan.effects[0]);
            assert(copy.x == 96 && copy.y == 24 && copy.width == 120 && copy.height == 18);
            constexpr std::array<stellar7::CockpitBitmapRole, 8> cockpitRoles{
                stellar7::CockpitBitmapRole::strip, stellar7::CockpitBitmapRole::indicator,
                stellar7::CockpitBitmapRole::overlay0, stellar7::CockpitBitmapRole::overlay1,
                stellar7::CockpitBitmapRole::overlay2, stellar7::CockpitBitmapRole::overlay3,
                stellar7::CockpitBitmapRole::overlay4, stellar7::CockpitBitmapRole::overlay5};
            constexpr std::array<std::int16_t, 8> cockpitX{128, 164, 24, 277, 24, 260, 24, 292};
            constexpr std::array<std::int16_t, 8> cockpitY{121, 122, 38, 37, 24, 24, 141, 140};
            for (std::size_t index = 0; index < cockpitRoles.size(); ++index) {
                const auto placement = std::get<stellar7::CockpitBitmapPlacement>(cockpitPlan.effects[index + 1]);
                assert(placement.bitmap == cockpitRoles[index]);
                assert(placement.x == cockpitX[index] && placement.y == cockpitY[index]);
                assert(placement.mode == (index == 0 ? stellar7::PlanarBlitMode::opaque
                    : index == 1 ? stellar7::PlanarBlitMode::generalTransparent : stellar7::PlanarBlitMode::transparent));
            }
            for (const auto incomingX : {std::int16_t{131}, std::int16_t{177}}) {
                const auto bounce = stellar7::cockpitOperations(
                    {incomingX, 0, 81, stellar7::CockpitPageRelation::differing}, bitmaps);
                assert(bounce.state.indicatorVelocity == (incomingX == 131 ? 4 : -4));
                const auto placement = std::get<stellar7::CockpitBitmapPlacement>(bounce.effects[2]);
                assert(placement.x == bounce.state.indicatorX);
            }
            std::vector<stellar7::NormalWorkOperation> expectedCockpitOperations;
            const auto appendCockpitBody = [&expectedCockpitOperations](const std::uint64_t control,
                const std::vector<stellar7::NormalWorkOperation>& body) {
                expectedCockpitOperations.push_back({stellar7::NormalWorkKind::instructions, control});
                expectedCockpitOperations.insert(expectedCockpitOperations.end(), body.begin(), body.end());
            };
            appendCockpitBody(17, stellar7::planarCopyOperations(96, 120, 18));
            appendCockpitBody(11, stellar7::planarBitmapOperations(bitmaps.strip, 128, stellar7::PlanarBlitMode::opaque));
            appendCockpitBody(16, stellar7::planarBitmapOperations(bitmaps.indicator, 164, stellar7::PlanarBlitMode::generalTransparent));
            constexpr std::array<std::uint64_t, 6> overlayControls{14, 10, 9, 10, 10, 8};
            for (std::size_t index = 0; index < overlayControls.size(); ++index) {
                appendCockpitBody(overlayControls[index], stellar7::planarBitmapOperations(
                    bitmaps.overlays[index], cockpitX[index + 2], stellar7::PlanarBlitMode::transparent));
            }
            expectedCockpitOperations.push_back({stellar7::NormalWorkKind::instructions, 3});
            assert(cockpitPlan.operations.size() == expectedCockpitOperations.size());
            for (std::size_t index = 0; index < cockpitPlan.operations.size(); ++index) {
                assert(cockpitPlan.operations[index].kind == expectedCockpitOperations[index].kind);
                assert(cockpitPlan.operations[index].count == expectedCockpitOperations[index].count);
            }
            auto cockpitPhase = rendered.timing.phase;
            bitmaps.matchingPageBackground = bitmaps.strip;
            const auto matching = stellar7::cockpitOperations(
                {160, 4, 81, stellar7::CockpitPageRelation::matching}, bitmaps);
            assert(matching.effects.size() == cockpitPlan.effects.size() + 1);
            const auto background = std::get<stellar7::CockpitBitmapPlacement>(matching.effects[0]);
            assert(background.bitmap == stellar7::CockpitBitmapRole::matchingPageBackground);
            assert(background.x == 96 && background.y == 24);
            assert(background.mode == stellar7::PlanarBlitMode::opaque);
            assert(std::holds_alternative<stellar7::CockpitCopyRegion>(matching.effects[1]));
            assert(matching.state.indicatorX == cockpitPlan.state.indicatorX);
            assert(matching.state.indicatorVelocity == cockpitPlan.state.indicatorVelocity);
            const auto backgroundBody = stellar7::planarBitmapOperations(
                bitmaps.matchingPageBackground, 96, stellar7::PlanarBlitMode::opaque);
            std::vector<stellar7::NormalWorkOperation> expectedMatchingOperations{
                {stellar7::NormalWorkKind::instructions, 13}};
            expectedMatchingOperations.insert(expectedMatchingOperations.end(), backgroundBody.begin(), backgroundBody.end());
            expectedMatchingOperations.push_back({stellar7::NormalWorkKind::instructions, 13});
            expectedMatchingOperations.insert(expectedMatchingOperations.end(),
                cockpitPlan.operations.begin() + 1, cockpitPlan.operations.end());
            assert(matching.operations.size() == expectedMatchingOperations.size());
            for (std::size_t index = 0; index < matching.operations.size(); ++index) {
                assert(matching.operations[index].kind == expectedMatchingOperations[index].kind);
                assert(matching.operations[index].count == expectedMatchingOperations[index].count);
            }
            auto missingBackground = bitmaps;
            missingBackground.matchingPageBackground = {};
            try {
                stellar7::cockpitOperations({160, 4, 81, stellar7::CockpitPageRelation::matching}, missingBackground);
                assert(false);
            } catch (const std::invalid_argument&) {
            }
            assert(stellar7::cockpitOperations(
                {160, 4, 81, stellar7::CockpitPageRelation::differing}, missingBackground).effects.size() == 9);
            auto invalidBackground = bitmaps;
            invalidBackground.matchingPageBackground.pixels.pop_back();
            try {
                stellar7::cockpitOperations({160, 4, 81, stellar7::CockpitPageRelation::matching}, invalidBackground);
                assert(false);
            } catch (const std::invalid_argument&) {
            }
            try {
                stellar7::cockpitOperations({160, 4, 81,
                    static_cast<stellar7::CockpitPageRelation>(-1)}, bitmaps);
                assert(false);
            } catch (const std::invalid_argument&) {
            }
            cockpitPhase.devices->frame = stellar7::AudioClockVgaFramePhase{70, 100, 0};
            const auto cockpit = sharedClock.renderCockpit(cockpitPhase,
                {160, 4, 71, stellar7::CockpitPageRelation::differing}, bitmaps, 60000);
            assert(cockpit.state.indicatorX == 164);
            assert(cockpit.work.audio.samples.size() > 0);
            assert(shared.driverState().sequenceWait == 1);
            const auto horizon = sharedClock.renderHorizon(cockpit.work.timing.phase,
                {24, 145, 0, 71, 256, 18, stellar7::HorizonBackground::solid, stellar7::HorizonOverlay::disabled}, 60000);
            assert(horizon.timing.stringIterations == 8655);
            assert(!horizon.audio.samples.empty());
            assert(shared.driverState().normalClockCycles == static_cast<std::uint64_t>(
                rendered.timing.total.elapsedCycles + cockpit.work.timing.total.elapsedCycles + horizon.timing.total.elapsedCycles));
            assert(sharedClock.elapsedSamples() == rendered.audio.samples.size()
                + cockpit.work.audio.samples.size() + horizon.audio.samples.size());

            stellar7::SoundBlasterSynth whole({}, instrumentResource(), sequence);
            stellar7::SoundBlasterSynth split({}, instrumentResource(), sequence);
            whole.startMusic();
            split.startMusic();
            constexpr std::uint64_t pitInputClock = 1'193'182;
            constexpr std::uint64_t callbackDivisor = (0xffff / 13) * 4;
            constexpr std::size_t prefixSamples = (stellar7::SoundBlasterSynth::sampleRate
                * callbackDivisor * 2 + pitInputClock - 1) / pitInputClock + 1;
            assert(whole.render(prefixSamples) == split.render(prefixSamples));
            assert(whole.callbackWorkState().wait == 0);
            stellar7::Opl2Chip reference;
            for (const auto& write : whole.registerWrites()) {
                reference.writeRegister(write.registerNumber, write.value);
            }
            reference.render(prefixSamples);
            stellar7::PresentationClock wholeClock(whole), splitClock(split);
            const auto complete = wholeClock.renderWork(incoming, {{NormalWorkKind::instructions, 1500}}, 5000);
            const auto first = splitClock.renderWork(incoming, {{NormalWorkKind::instructions, 700}}, 5000);
            const auto second = splitClock.renderWork(first.timing.phase, {{NormalWorkKind::instructions, 800}}, 5000);
            auto combined = first.audio.samples;
            combined.insert(combined.end(), second.audio.samples.begin(), second.audio.samples.end());
            assert(complete.audio.samples == combined);
            assert(whole.callbackWorkState().wait == 2 && whole.driverState().notes[0] == 60);
            assert(complete.audio.writes.size() == 3);
            for (std::size_t writer = 0; writer < complete.audio.writes.size(); ++writer) {
                // Normal dispatch plus ISA8 delay precede the hardware write.
                assert(complete.audio.writes[writer].cycleOffset
                    == complete.timing.callbacks.front().timing.opl[writer][1].elapsedCycles + 4);
            }
            std::vector<std::int16_t> expected;
            for (const auto& write : complete.audio.writes) {
                const auto sample = (write.cycleOffset * 44100 + 4'999'999) / 5'000'000;
                const auto part = reference.render(static_cast<std::size_t>(sample) - expected.size());
                expected.insert(expected.end(), part.begin(), part.end());
                reference.writeRegister(write.write.registerNumber, write.write.value);
            }
            const auto tail = reference.render(complete.audio.samples.size() - expected.size());
            expected.insert(expected.end(), tail.begin(), tail.end());
            assert(expected == complete.audio.samples);
            assert(wholeClock.elapsedSamples() == splitClock.elapsedSamples());
            stellar7::SoundBlasterSynth rejected({}, instrumentResource(), sequence);
            rejected.startMusic();
            rejected.render(prefixSamples);
            const auto writesBefore = rejected.registerWrites().size();
            auto invalidTiming = complete.timing;
            invalidTiming.callbacks.front().timing.oplEffects.front().elapsedCycles = -1;
            bool refusedOrder = false;
            try {
                rejected.renderClocked(invalidTiming, 0);
            } catch (const std::invalid_argument&) {
                refusedOrder = true;
            }
            assert(refusedOrder && rejected.driverState().sequenceWait == 0);
            assert(rejected.driverState().notes[0] == 0 && rejected.registerWrites().size() == writesBefore);
            assert(rejected.renderClocked(complete.timing, 0).samples == complete.audio.samples);
            stellar7::SoundBlasterSynth keyOff({}, instrumentResource(), sequence);
            keyOff.startMusic();
            const auto keyOffPrefix = (stellar7::SoundBlasterSynth::sampleRate
                * callbackDivisor * 5 + pitInputClock - 1) / pitInputClock + 1;
            keyOff.render(static_cast<std::size_t>(keyOffPrefix));
            assert(keyOff.callbackWorkState().wait == 0 && keyOff.driverState().notes[0] == 60);
            stellar7::PresentationClock keyOffClock(keyOff);
            const auto released = keyOffClock.renderWork(incoming, {{NormalWorkKind::instructions, 150}}, 5000);
            assert(released.audio.writes.size() == 2);
            assert(released.audio.writes[0].write.registerNumber == 0xa0);
            assert(released.audio.writes[1].write.registerNumber == 0xb0);
            assert(keyOff.driverState().notes[0] == 60 && keyOff.callbackWorkState().sounding[0] == 0);
            assert(keyOff.driverState().sequenceWait == 2);
            auto retainedSequence = defaultProgramNoteSequence();
            retainedSequence.resize(36);
            retainedSequence.insert(retainedSequence.end(), {0xb6, 0x40, 127, 0, 0xb6, 0x4e, 127, 0, 0xb7, 0x4e, 64, 0,
                0xbf, 0x4e, 1, 0, 0x96, 60, 64, 3, 0xfc});
            stellar7::SoundBlasterSynth retained({}, instrumentResource(), retainedSequence, {}, sequence);
            retained.startMusic();
            retained.render(1);
            assert(retained.driverState().channelVoiceCursors[6] == 2);
            retained.switchLevel2();
            const auto switched = retained.callbackWorkState();
            assert(switched.channelVoiceCursors[6] == 2);
            assert(switched.channelEnabled[6] == 127 && switched.channelEnabled[7] == 64 && switched.channelEnabled[15] == 1);
            assert(switched.voices.channelDeferrals[6] == 1);

            stellar7::SoundBlasterSynth paletteSynth({}, instrumentResource(), sequence);
            stellar7::SoundBlasterSynth paletteReference({}, instrumentResource(), sequence);
            paletteSynth.startMusic();
            paletteReference.startMusic();
            assert(paletteSynth.render(1) == paletteReference.render(1));
            stellar7::PresentationClock paletteClock(paletteSynth);
            auto palettePhase = callbackIrqPhase;
            palettePhase.frameStart = stellar7::AudioClockFrameStart::activeUnlockedVgaSurface;
            palettePhase.devices->frame = stellar7::AudioClockVgaFramePhase{70, 100, 0};
            // This longer retained call needs the next frame event, unlike the short IRQ fixture.
            palettePhase.linesDone = palettePhase.linesTotal;
            palettePhase.events.push_back({palettePhase.devices->framePeriodMilliseconds,
                stellar7::AudioClockEventKind::frameStart, 0});
            palettePhase.callback->wait = 65000;
            stellar7::PaletteWorkState paletteState{};
            paletteState.source.fill(63);
            std::fill_n(paletteState.source.begin(), 3, 0);
            const stellar7::PaletteRequest paletteRequest{0, 256, 0, 3};
            auto referencePhase = palettePhase;
            referencePhase.callback = paletteReference.callbackWorkState();
            const auto referencePalette = stellar7::paletteClockTiming(referencePhase,
                paletteState, paletteRequest, 120000);
            const auto referenceAudio = paletteReference.renderClocked(referencePalette.timing, 0);
            const auto clockedPalette = paletteClock.renderPalette(palettePhase,
                paletteState, paletteRequest, 120000);
            assert(!clockedPalette.work.timing.callbacks.empty());
            assert(clockedPalette.work.audio.samples == referenceAudio.samples);
            assert(paletteSynth.driverState().sequenceWait == paletteReference.driverState().sequenceWait);
            assert(paletteSynth.driverState().sequenceWait != 64999);
            assert(clockedPalette.state.dac.colors == referencePalette.state.dac.colors);
            const auto retainedPalette = paletteClock.renderPalette(clockedPalette.work.timing.phase,
                clockedPalette.state, {0, 256, 0, 59}, 120000);
            assert(retainedPalette.state.source == paletteState.source);
            const auto paletteCycles = clockedPalette.work.timing.total.elapsedCycles
                + retainedPalette.work.timing.total.elapsedCycles;
            assert(paletteSynth.driverState().normalClockCycles == static_cast<std::uint64_t>(paletteCycles));
            assert(paletteClock.elapsedSamples() == clockedPalette.work.audio.samples.size()
                + retainedPalette.work.audio.samples.size());
            bool paletteLegacyFrameRejected = false;
            try {
                paletteClock.renderFrame();
            } catch (const std::logic_error&) {
                paletteLegacyFrameRejected = true;
            }
            assert(paletteLegacyFrameRejected);

            auto blackoutSequence = sequence;
            blackoutSequence.pop_back();
            for (unsigned repeat = 0; repeat < 32; ++repeat) {
                blackoutSequence.insert(blackoutSequence.end(), {0x91, 61, 64, 7, 0x91, 61, 0, 7});
            }
            blackoutSequence.push_back(0xfc);
            stellar7::SoundBlasterSynth blackoutSynth({}, instrumentResource(), blackoutSequence);
            stellar7::SoundBlasterSynth blackoutReference({}, instrumentResource(), blackoutSequence);
            blackoutSynth.startMusic();
            blackoutReference.startMusic();
            assert(blackoutSynth.render(1) == blackoutReference.render(1));
            stellar7::PresentationClock blackoutClock(blackoutSynth), referenceClock(blackoutReference);
            auto blackoutPhase = palettePhase;
            blackoutPhase.timer->callbackReload = 1;
            blackoutPhase.timer->biosCount = blackoutPhase.timer->biosReload = 2;
            blackoutPhase.bios = stellar7::AudioClockBiosPhase{};
            blackoutPhase.bios->domain = stellar7::AudioClockBiosDomain::normalVgaDefaultTimer;
            blackoutPhase.events.insert(blackoutPhase.events.begin() + 1,
                {1.0, stellar7::AudioClockEventKind::pit, 0});
            stellar7::BlackoutWorkState blackoutState{};
            blackoutState.palette = paletteState;
            blackoutState.surface.firstColor = 12;
            stellar7::CockpitPageState blackoutRoles;
            stellar7::IndexedPages blackoutPages;
            const auto blackedOut = blackoutClock.renderBlackout(blackoutPhase,
                blackoutState, blackoutRoles, blackoutPages, 120000);
            std::vector<std::int16_t> actualBlackout, expectedBlackout;
            std::vector<stellar7::ClockedOplRegisterWrite> actualOpl, expectedOpl;
            std::uint64_t actualCycles = 0, expectedCycles = 0;
            const auto appendWork = [](const stellar7::ClockedPresentationWork& work,
                auto& samples, auto& writes, auto& cursor) {
                samples.insert(samples.end(), work.audio.samples.begin(), work.audio.samples.end());
                for (auto write : work.audio.writes) {
                    write.cycleOffset += static_cast<std::int64_t>(cursor);
                    writes.push_back(write);
                }
                cursor += static_cast<std::uint64_t>(work.timing.total.elapsedCycles);
            };
            auto referenceIncoming = blackoutPhase;
            auto referenceBank = blackoutState.palette;
            for (std::size_t i = 0; i < stellar7::blackoutPaletteSteps; ++i) {
                appendWork(blackedOut.steps[i].before, actualBlackout, actualOpl, actualCycles);
                appendWork(blackedOut.steps[i].palette.work, actualBlackout, actualOpl, actualCycles);
                const auto before = referenceClock.renderWork(referenceIncoming,
                    {{NormalWorkKind::instructions, i == 0 ? 33U : 24U}}, 120000);
                const auto upload = referenceClock.renderPalette(before.timing.phase, referenceBank,
                    {0, 256, 0, static_cast<std::uint8_t>(63 - 4 * i)}, 120000);
                appendWork(before, expectedBlackout, expectedOpl, expectedCycles);
                appendWork(upload.work, expectedBlackout, expectedOpl, expectedCycles);
                referenceIncoming = upload.work.timing.phase;
                referenceBank = upload.state;
            }
            std::vector<stellar7::NormalWorkOperation> clear{{NormalWorkKind::instructions, 16}};
            const auto clearBody = stellar7::fullFrontClearOperations();
            clear.insert(clear.end(), clearBody.begin(), clearBody.end());
            clear.push_back({NormalWorkKind::instructions, 8});
            const auto referenceClear = referenceClock.renderWork(referenceIncoming, clear, 120000);
            appendWork(referenceClear, expectedBlackout, expectedOpl, expectedCycles);
            appendWork(blackedOut.clear, actualBlackout, actualOpl, actualCycles);
            assert(actualBlackout == expectedBlackout && actualCycles == expectedCycles);
            assert(!actualOpl.empty() && actualOpl.size() == expectedOpl.size());
            for (std::size_t i = 0; i < actualOpl.size(); ++i) {
                assert(actualOpl[i].cycleOffset == expectedOpl[i].cycleOffset);
                assert(actualOpl[i].write.registerNumber == expectedOpl[i].write.registerNumber);
                assert(actualOpl[i].write.value == expectedOpl[i].write.value);
            }
            assert(blackoutClock.elapsedSamples() == actualBlackout.size());
            assert(blackoutSynth.driverState().normalClockCycles == actualCycles);
            assert(blackoutSynth.driverState().sequenceWait == blackoutReference.driverState().sequenceWait);
            assert(blackedOut.clear.timing.phase.remainingCycles == referenceClear.timing.phase.remainingCycles);
            assert(blackedOut.clear.timing.phase.deferredCycles == referenceClear.timing.phase.deferredCycles);
            assert(blackedOut.clear.timing.phase.bios->timerTicks == referenceClear.timing.phase.bios->timerTicks);
            assert(blackedOut.clear.timing.phase.devices->picTicks == referenceClear.timing.phase.devices->picTicks);
            const auto adjacent = blackoutClock.renderWork(blackedOut.clear.timing.phase,
                {{NormalWorkKind::instructions, 150}}, 120000);
            const auto adjacentReference = referenceClock.renderWork(referenceClear.timing.phase,
                {{NormalWorkKind::instructions, 150}}, 120000);
            assert(adjacent.audio.samples == adjacentReference.audio.samples);
            assert(adjacent.timing.total.elapsedCycles == adjacentReference.timing.total.elapsedCycles);

            stellar7::SoundBlasterSynth flipping({}, instrumentResource(), sequence);
            stellar7::SoundBlasterSynth flipReference({}, instrumentResource(), sequence);
            flipping.startMusic();
            flipReference.startMusic();
            assert(flipping.render(1) == flipReference.render(1));
            stellar7::PresentationClock flipClock(flipping);
            auto flipPhase = callbackIrqPhase;
            flipPhase.frameStart = stellar7::AudioClockFrameStart::activeUnlockedVgaSurface;
            flipPhase.devices->frame = stellar7::AudioClockVgaFramePhase{70, 100, 0};
            flipPhase.callback->wait = 65000;
            const auto flipped = flipClock.renderPageFlip(flipPhase,
                {stellar7::PageFlipSynchronization::disabled, 0xa400, 0xa000, 0x3d4}, 1000);
            assert(flipped.work.timing.callbacks.size() == 1);
            assert(flipping.callbackWorkState().wait == 1);
            assert(flipped.state.frontPage == 0xa400 && flipped.state.backPage == 0xa000);
            assert(flipped.lowReads == 0 && flipped.highReads == 0);
            const auto referenceFlip = flipReference.renderClocked(flipped.work.timing, 0);
            assert(referenceFlip.samples == flipped.work.audio.samples);
            const auto afterFlip = flipClock.renderWork(flipped.work.timing.phase,
                stellar7::frameCopyOperations(stellar7::FrameCopyProfile::modeXNoop).operations, 1000);
            const auto referenceTail = flipReference.renderClocked(afterFlip.timing,
                static_cast<std::uint64_t>(flipped.work.timing.total.elapsedCycles));
            assert(referenceTail.samples == afterFlip.audio.samples);
            assert(flipClock.elapsedSamples() == referenceFlip.samples.size() + referenceTail.samples.size());
            assert(flipping.driverState().normalClockCycles == static_cast<std::uint64_t>(
                flipped.work.timing.total.elapsedCycles + afterFlip.timing.total.elapsedCycles));
            assert(flipping.callbackWorkState().wait == flipReference.callbackWorkState().wait);

            stellar7::FrameTailState tailState{5, {}, 0, 0, {}, {}};
            tailState.actors.resize(7, {8, {}});
            stellar7::FrameTailGuards tailGuards{};
            tailGuards.armor = 680;
            tailGuards.missionActive = true;
            const auto tailPlan = stellar7::frameTailOperations(tailState, tailGuards);
            const auto clockedTail = flipClock.renderWork(afterFlip.timing.phase, tailPlan.operations, 1000);
            const auto beforeTailCycles = flipped.work.timing.total.elapsedCycles + afterFlip.timing.total.elapsedCycles;
            const auto referenceFrameTail = flipReference.renderClocked(clockedTail.timing,
                static_cast<std::uint64_t>(beforeTailCycles));
            assert(tailPlan.state.loopTick == 6);
            assert(clockedTail.timing.phase.interrupts == stellar7::AudioClockInterrupts::enabled);
            assert(referenceFrameTail.samples == clockedTail.audio.samples);
            assert(flipClock.elapsedSamples() == referenceFlip.samples.size() + referenceTail.samples.size()
                + referenceFrameTail.samples.size());
            assert(flipping.driverState().normalClockCycles == static_cast<std::uint64_t>(
                beforeTailCycles + clockedTail.timing.total.elapsedCycles));
            assert(flipping.callbackWorkState().wait == flipReference.callbackWorkState().wait);
        }
        for (const auto exitCount : {1, 3}) {
            auto unsupported = irqPhase;
            unsupported.timer->callbackCount = exitCount;
            unsupported.timer->biosCount = exitCount == 1 ? 5 : 1;
            bool rejected = false;
            try {
                stellar7::normalWorkTiming(unsupported, {{NormalWorkKind::instructions, 150}}, 1000);
            } catch (const std::invalid_argument&) {
                rejected = true;
            }
            assert(rejected);
        }

        using stellar7::SoundBlasterCallbackKind;
        using stellar7::SoundBlasterWorkKind;
        const std::vector<stellar7::SoundBlasterWorkBlock> blocks{
            {SoundBlasterWorkKind::plain, 303, -1}, {SoundBlasterWorkKind::frequencyLow, 104, 2},
            {SoundBlasterWorkKind::plain, 22, -1}, {SoundBlasterWorkKind::frequencyHigh, 104, 2},
            {SoundBlasterWorkKind::plain, 80, -1}};
        auto callbackPhase = phase;
        callbackPhase.interruptDelivery = stellar7::AudioClockInterruptDelivery::blockedByIrq0;
        const auto callback = stellar7::soundBlasterCallbackTiming(callbackPhase, blocks, SoundBlasterCallbackKind::explicitKeyOff);
        assert(callback.total.dispatches == 613 && callback.total.delayCycles == 614);
        assert(callback.total.elapsedCycles == 1227 && callback.total.terminalCharges == 0);
        assert(callback.opl.size() == 2 && callback.interruptBoundaries.size() == 2);
        assert(callback.opl[0][0].dispatches == 303 && callback.opl[0][1].dispatches == 325);
        assert(callback.opl[0][2].dispatches == 406 && callback.opl[1][0].dispatches == 429);
        assert(callback.interruptBoundaries[0].dispatches == 595 && callback.interruptBoundaries[1].dispatches == 604);
        callbackPhase.irqCheckRequested = true;
        const auto forced = stellar7::soundBlasterCallbackTiming(callbackPhase, blocks, SoundBlasterCallbackKind::explicitKeyOff);
        assert(forced.total.dispatches == 613 && forced.total.delayCycles == 614);
        assert(forced.total.elapsedCycles == 1228 && forced.total.terminalCharges == 1);
        assert(forced.total.remainingCycles == 3772 && forced.total.deferredCycles == 0);
        assert(forced.interruptBoundaries[1].elapsedCycles == callback.interruptBoundaries[1].elapsedCycles + 1);
        assert(callbackPhase.remainingCycles == 5000 && callbackPhase.irqCheckRequested);
        // The retained raw27 group scans two absent notes and releases voices1/2.
        stellar7::SoundBlasterCallbackState keyOffState{};
        keyOffState.voices.channels = {1,7,8,255,255,255,13,14,15};
        keyOffState.voices.notes = {37,36,42,0,0,0,0,0,0};
        keyOffState.sounding = {1,1,1,0,0,0,0,0,0};
        keyOffState.sequence = {0x99,36,0,0,0x97,36,0,0,0x99,42,0,0,0x98,42,0,2};
        const auto keyOff = stellar7::soundBlasterCallbackAdvance(keyOffState);
        assert(keyOff.dispatches == 1118);
        std::size_t keyOffWriter = 0;
        for (const auto& block : keyOff.blocks) {
            if (block.kind == SoundBlasterWorkKind::plain) {
                continue;
            }
            assert(keyOffWriter < 4 && block.voice == static_cast<int>(1 + keyOffWriter / 2));
            assert(block.kind == (keyOffWriter % 2 == 0
                ? SoundBlasterWorkKind::frequencyLow : SoundBlasterWorkKind::frequencyHigh));
            ++keyOffWriter;
        }
        assert(keyOffWriter == 4);
        const auto released = stellar7::soundBlasterCallbackTiming(callbackPhase, keyOff.blocks, keyOff.kind);
        assert(released.total.dispatches == keyOff.dispatches && released.total.elapsedCycles > 2000);
        assert(released.opl.size() == 4 && released.interruptBoundaries.size() == 2);
        assert(keyOff.state.position == 16 && keyOff.state.wait == 1);
        assert(keyOff.state.sounding[0] == 1 && keyOff.state.sounding[1] == 0 && keyOff.state.sounding[2] == 0);
        const auto wait = stellar7::soundBlasterCallbackTiming(callbackPhase,
            {{SoundBlasterWorkKind::plain, 82, -1}}, SoundBlasterCallbackKind::wait);
        assert(wait.total.elapsedCycles == 82 && wait.opl.empty() && wait.interruptBoundaries.empty());
        const auto rejectsCallback = [](const stellar7::SoundBlasterClockPhase& input,
                                       const std::vector<stellar7::SoundBlasterWorkBlock>& work,
                                       const SoundBlasterCallbackKind kind) {
            try {
                stellar7::soundBlasterCallbackTiming(input, work, kind);
            } catch (const std::invalid_argument&) {
                return true;
            }
            return false;
        };
        assert(rejectsCallback(phase, blocks, SoundBlasterCallbackKind::explicitKeyOff));
        assert(rejectsCallback(callbackPhase, blocks, SoundBlasterCallbackKind::wait));
        assert(rejectsCallback(callbackPhase,
            {{SoundBlasterWorkKind::plain, 1, -1}, {SoundBlasterWorkKind::plain, 30, -1}},
            SoundBlasterCallbackKind::explicitKeyOff));
        assert(rejectsCallback(callbackPhase, {{SoundBlasterWorkKind::plain, 9000, -1}},
            SoundBlasterCallbackKind::explicitKeyOff));
        assert(rejectsCallback(callbackPhase, {{SoundBlasterWorkKind::frequencyLow, 103, 2}}, SoundBlasterCallbackKind::explicitKeyOff));
        assert(rejectsCallback(callbackPhase, {{SoundBlasterWorkKind::plain, 1, 2}}, SoundBlasterCallbackKind::wait));
        assert(rejectsCallback(callbackPhase, {{SoundBlasterWorkKind::plain, 2000, -1}, {SoundBlasterWorkKind::plain, 1, -1}}, SoundBlasterCallbackKind::wait));
        auto future = callbackPhase;
        future.remainingCycles = 1000;
        future.deferredCycles = 4000;
        future.events = {{0.2, stellar7::AudioClockEventKind::pendingVgaInterrupt, 0}};
        const auto pending = stellar7::soundBlasterCallbackTiming(future,
            {{SoundBlasterWorkKind::plain, 82, -1}}, SoundBlasterCallbackKind::wait);
        assert(pending.total.boundedEvents.size() == 1);
        future.remainingCycles = 1;
        future.deferredCycles = 4999;
        future.events[0].deadlineMilliseconds = 0;
        assert(rejectsCallback(future, {{SoundBlasterWorkKind::plain, 82, -1}}, SoundBlasterCallbackKind::wait));

        const auto rejects = [](const stellar7::SoundBlasterClockPhase& input) {
            try {
                stellar7::soundBlasterOplTiming(input);
            } catch (const std::invalid_argument&) {
                return true;
            }
            return false;
        };
        auto invalid = phase;
        invalid.interrupts = stellar7::AudioClockInterrupts::enabled;
        assert(rejects(invalid));
        invalid = phase;
        invalid.deferredCycles = 1;
        assert(rejects(invalid));
        invalid = phase;
        invalid.singleLinePeriodMilliseconds = std::numeric_limits<double>::quiet_NaN();
        assert(rejects(invalid));
        invalid = phase;
        invalid.remainingCycles = 1;
        invalid.deferredCycles = 4999;
        invalid.events = {{0, stellar7::AudioClockEventKind::unsupported, 0}};
        assert(rejects(invalid));
        invalid.events = {{0, stellar7::AudioClockEventKind::dmaDac, 0}};
        assert(rejects(invalid));
        invalid.events = {{0, stellar7::AudioClockEventKind::frameStart, 0}};
        invalid.linesDone = invalid.linesTotal;
        assert(rejects(invalid));
        invalid.events = {{0, stellar7::AudioClockEventKind::pit, 0}};
        invalid.pitPeriodMilliseconds = 0.1;
        assert(rejects(invalid));
        invalid = phase;
        invalid.events = {{1, stellar7::AudioClockEventKind::scanline, 0},
                          {0, stellar7::AudioClockEventKind::scanline, 0}};
        assert(rejects(invalid));
    }
    {
        using stellar7::FrameSynchronization;
        const auto low = stellar7::pageFlipNanoseconds(7'000'000, FrameSynchronization::verticalRetrace);
        assert(low >= 6'090'000 && low <= 6'100'000);
        assert(low == stellar7::pageFlipNanoseconds(7'000'000 + 14'268'064,
                                                    FrameSynchronization::verticalRetrace));
        assert(stellar7::pageFlipNanoseconds(13'100'000, FrameSynchronization::verticalRetrace) > 14'000'000);
        assert(stellar7::pageFlipNanoseconds(13'085'953, FrameSynchronization::verticalRetrace) < 10'000);
        assert(stellar7::pageFlipNanoseconds(13'085'954, FrameSynchronization::verticalRetrace) > 14'000'000);
        assert(stellar7::pageFlipNanoseconds(0, FrameSynchronization::disabled) == 5'600);
        assert(stellar7::pageFlipNanoseconds(13'100'000, FrameSynchronization::disabled) == 5'600);
    }
    {
        const auto schedule = stellar7::levelEntrySchedule(13'107'561);
        std::size_t samples = 0;
        for (std::size_t index = 0; index < schedule.size(); ++index) {
            const auto& step = schedule[index];
            assert(step.maskedSamples >= 47 && step.maskedSamples <= 48);
            samples += step.beforeUploadSamples + step.maskedSamples + step.afterUploadSamples;
            assert(samples == (step.elapsedNanoseconds * stellar7::SoundBlasterSynth::sampleRate
                               + 999'999'999) / 1'000'000'000);
            if (index != 0) {
                assert(step.elapsedNanoseconds > schedule[index - 1].elapsedNanoseconds);
            }
        }
        assert(schedule.size() == 16);
        const auto wrapped = stellar7::levelEntrySchedule(13'107'561 + 14'268'064);
        for (std::size_t index = 0; index < schedule.size(); ++index) {
            assert(wrapped[index].elapsedNanoseconds == schedule[index].elapsedNanoseconds);
        }
        const auto alreadyHigh = stellar7::levelEntrySchedule(9'292'354);
        const auto justMissed = stellar7::levelEntrySchedule(9'353'509);
        assert(alreadyHigh.front().elapsedNanoseconds < 4'900'000);
        assert(justMissed.front().elapsedNanoseconds > 18'000'000);
        assert(stellar7::startsLevelEntry(stellar7::GameplayPhase::successPresentation,
                                         stellar7::GameplayPhase::level2Handoff));
        assert(!stellar7::startsLevelEntry(stellar7::GameplayPhase::level2Handoff,
                                          stellar7::GameplayPhase::level2));
        assert(stellar7::startsLevelEntry(stellar7::GameplayPhase::continuePrompt,
                                         stellar7::GameplayPhase::level2Handoff));
    }
    {
        stellar7::SoundBlasterSynth whole({}, instrumentResource(), oneNoteSequence());
        stellar7::SoundBlasterSynth split({}, instrumentResource(), oneNoteSequence());
        whole.startMusic();
        split.startMusic();
        stellar7::PresentationClock replay(whole);
        stellar7::PresentationClock live(split);
        assert(replay.renderFrame() == live.renderFrame());
        const auto replayPcm = replay.renderEntry();
        std::vector<std::int16_t> livePcm;
        const auto schedule = live.entrySchedule();
        for (const auto& step : schedule) {
            const auto pcm = live.renderEntryStep(step);
            livePcm.insert(livePcm.end(), pcm.begin(), pcm.end());
        }
        assert(replayPcm == livePcm);
        assert(replay.elapsedSamples() == live.elapsedSamples());
        assert(replay.elapsedSamples() == replayPcm.size() + stellar7::gameplayAudioSamplesPerFrame);
        assert(whole.driverState().clockPhase == split.driverState().clockPhase);
        assert(whole.driverState().notes == split.driverState().notes);
        const auto wholeWrites = whole.registerWrites();
        const auto splitWrites = split.registerWrites();
        assert(wholeWrites.size() == splitWrites.size());
        for (std::size_t index = 0; index < wholeWrites.size(); ++index) {
            assert(wholeWrites[index].registerNumber == splitWrites[index].registerNumber);
            assert(wholeWrites[index].value == splitWrites[index].value);
        }
        assert(replay.renderFrame() == live.renderFrame());
    }
    {
        stellar7::SoundBlasterSynth masked({}, instrumentResource(), oneNoteSequence());
        stellar7::SoundBlasterSynth reference({}, instrumentResource(), oneNoteSequence());
        masked.startMusic();
        reference.startMusic();
        masked.triggerLevel1Event(12);
        reference.triggerLevel1Event(12);
        const auto before = masked.driverState();
        masked.clearRegisterWrites();
        constexpr std::size_t uploadSamples = 48;
        assert(masked.renderInterruptMasked(uploadSamples).size() == uploadSamples);
        assert(masked.registerWrites().empty());
        assert(masked.driverState().notes == before.notes);
        assert(masked.driverState().directTimers == before.directTimers);
        assert(masked.driverState().clockPhase > before.clockPhase);
        masked.render(1);
        reference.render(uploadSamples + 1);
        assert(!masked.registerWrites().empty());
        assert(masked.driverState().notes == reference.driverState().notes);
        assert(masked.driverState().directTimers == reference.driverState().directTimers);
        assert(masked.driverState().clockPhase == reference.driverState().clockPhase);

        constexpr std::size_t maximumMaskSamples =
            stellar7::SoundBlasterSynth::sampleRate * 5041ULL / 1193182;
        masked.renderInterruptMasked(maximumMaskSamples - 1);
        masked.render(0);
        masked.renderInterruptMasked(1);
        const auto phase = masked.driverState().clockPhase;
        bool rejected = false;
        try {
            masked.renderInterruptMasked(1);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
        assert(masked.driverState().clockPhase == phase);
        masked.render(1);
        assert(masked.renderInterruptMasked(uploadSamples).size() == uploadSamples);
    }
    {
        stellar7::SoundBlasterSynth masked({}, instrumentResource(), oneNoteSequence());
        stellar7::SoundBlasterSynth reference({}, instrumentResource(), oneNoteSequence());
        masked.startMusic();
        reference.startMusic();
        masked.render(1);
        reference.render(1);
        const auto clock = masked.driverState();
        constexpr std::uint64_t pitInputClock = 1193182;
        constexpr std::size_t halfUploadSamples = 24;
        const auto untilCallback = static_cast<std::size_t>(
            (clock.clockPeriod - clock.clockPhase + pitInputClock - 1) / pitInputClock
        );
        masked.render(untilCallback - halfUploadSamples);
        reference.render(untilCallback + halfUploadSamples + 1);
        const auto notes = masked.driverState().notes;
        masked.clearRegisterWrites();
        const auto pcm = masked.renderInterruptMasked(halfUploadSamples * 2);
        assert(std::any_of(pcm.begin(), pcm.end(), [](const auto sample) { return sample != 0; }));
        assert(masked.registerWrites().empty());
        assert(masked.driverState().notes == notes);
        masked.render(1);
        assert(!masked.registerWrites().empty());
        assert(masked.driverState().notes == reference.driverState().notes);
        assert(masked.driverState().clockPhase == reference.driverState().clockPhase);
    }
    stellar7::Opl2Chip opl2;
    assert(opl2.nativeSampleRate() == 49715);
    opl2.writeRegister(0x01, 0x20);
    opl2.writeRegister(0x20, 0x01);
    opl2.writeRegister(0x23, 0x01);
    opl2.writeRegister(0x40, 0x10);
    opl2.writeRegister(0x43, 0x00);
    opl2.writeRegister(0x60, 0xf0);
    opl2.writeRegister(0x63, 0xf0);
    opl2.writeRegister(0x80, 0x77);
    opl2.writeRegister(0x83, 0x77);
    opl2.writeRegister(0xc0, 0x00);
    opl2.writeRegister(0xa0, 0x98);
    opl2.writeRegister(0xb0, 0x31);
    const auto oplSounding = opl2.render(stellar7::Opl2Chip::sampleRate / 10);
    assert(
        std::any_of(
            oplSounding.begin(),
            oplSounding.end(),
            [](const std::int16_t value) { return value != 0; }
        )
    );
    opl2.reset();
    const auto oplSilent = opl2.render(stellar7::Opl2Chip::sampleRate / 100);
    assert(
        std::all_of(
            oplSilent.begin(),
            oplSilent.end(),
            [](const std::int16_t value) { return value == 0; }
        )
    );

    stellar7::SoundBlasterSynth soundBlaster(
        {},
        instrumentResource(),
        oneNoteSequence()
    );
    assert(soundBlaster.hasInstrumentBank());
    assert(soundBlaster.registerWrites().size() == 158);
    assert(soundBlaster.registerWrites()[0].registerNumber == 0x01);
    assert(soundBlaster.registerWrites()[0].value == 0x00);
    assert(soundBlaster.registerWrites()[117].registerNumber == 0x35);
    assert(soundBlaster.registerWrites()[118].registerNumber == 0xbd);
    assert(soundBlaster.registerWrites()[139].registerNumber == 0xe0);
    assert(soundBlaster.registerWrites()[157].registerNumber == 0x01);
    assert(soundBlaster.registerWrites()[157].value == 0x20);
    soundBlaster.startMusic();
    const auto fmSequence = soundBlaster.render(
        stellar7::SoundBlasterSynth::sampleRate
        / stellar7::SoundBlasterSynth::sequenceRate
    );
    assert(
        std::any_of(
            fmSequence.begin(),
            fmSequence.end(),
            [](const std::int16_t value) { return value != 0; }
        )
    );

    stellar7::SoundBlasterSynth level2SoundBlaster(
        {},
        instrumentResource(),
        {},
        {},
        oneNoteSequence()
    );
    level2SoundBlaster.clearRegisterWrites();
    level2SoundBlaster.startLevel2();
    const auto level2Sequence = level2SoundBlaster.render(
        stellar7::SoundBlasterSynth::sampleRate
        / stellar7::SoundBlasterSynth::sequenceRate
    );
    assert(!level2SoundBlaster.registerWrites().empty());
    assert(
        std::any_of(
            level2Sequence.begin(),
            level2Sequence.end(),
            [](const std::int16_t value) { return value != 0; }
        )
    );

    stellar7::SoundBlasterSynth level3SoundBlaster(
        {},
        instrumentResource(),
        {},
        {},
        {},
        oneNoteSequence()
    );
    level3SoundBlaster.clearRegisterWrites();
    level3SoundBlaster.startLevel3();
    const auto level3Sequence = level3SoundBlaster.render(
        stellar7::SoundBlasterSynth::sampleRate
        / stellar7::SoundBlasterSynth::sequenceRate
    );
    assert(!level3SoundBlaster.registerWrites().empty());
    assert(
        std::any_of(
            level3Sequence.begin(),
            level3Sequence.end(),
            [](const std::int16_t value) { return value != 0; }
        )
    );

    stellar7::SoundBlasterSynth handoffSoundBlaster(
        {},
        instrumentResource(),
        {},
        {},
        oneNoteSequence(),
        oneNoteSequence()
    );
    handoffSoundBlaster.startLevel2();
    handoffSoundBlaster.clearRegisterWrites();
    handoffSoundBlaster.switchLevel3();
    // The native resource handoff changes the resident sequencer without
    // issuing the all-voice key-off sweep used by a fresh start.
    assert(handoffSoundBlaster.registerWrites().empty());
    handoffSoundBlaster.render(
        stellar7::SoundBlasterSynth::sampleRate
        / stellar7::SoundBlasterSynth::sequenceRate
    );
    assert(!handoffSoundBlaster.registerWrites().empty());

    // The controlled native presenter boundary follows the resident switch
    // before another music tick.  Starting the success handle must therefore
    // release the retained zero-note state on every physical voice.
    stellar7::SoundBlasterSynth residentReleaseSoundBlaster(
        {oneNoteSequence()},
        instrumentResource(),
        {},
        {},
        oneNoteSequence(),
        oneNoteSequence()
    );
    residentReleaseSoundBlaster.startLevel2();
    residentReleaseSoundBlaster.clearRegisterWrites();
    residentReleaseSoundBlaster.switchLevel3();
    residentReleaseSoundBlaster.clearRegisterWrites();
    residentReleaseSoundBlaster.startSuccess();
    const auto& residentReleaseWrites =
        residentReleaseSoundBlaster.registerWrites();
    assert(residentReleaseWrites.size() == 18);
    for (std::size_t voice = 0; voice < 9; ++voice) {
        const std::size_t index = voice * 2;
        assert(
            residentReleaseWrites[index].registerNumber
            == static_cast<std::uint8_t>(0xa0 + voice)
        );
        assert(residentReleaseWrites[index].value == 0x57);
        assert(
            residentReleaseWrites[index + 1].registerNumber
            == static_cast<std::uint8_t>(0xb0 + voice)
        );
        assert(residentReleaseWrites[index + 1].value == 0x01);
    }

    stellar7::SoundBlasterSynth successSoundBlaster(
        {oneNoteSequence()},
        instrumentResource()
    );
    successSoundBlaster.clearRegisterWrites();
    successSoundBlaster.startSuccess();
    const auto successSequence = successSoundBlaster.render(
        stellar7::SoundBlasterSynth::sampleRate
        / stellar7::SoundBlasterSynth::sequenceRate
    );
    assert(!successSoundBlaster.registerWrites().empty());
    assert(
        std::any_of(
            successSequence.begin(),
            successSequence.end(),
            [](const std::int16_t value) { return value != 0; }
        )
    );

    stellar7::SoundBlasterSynth percussionSoundBlaster(
        {percussionSequence()},
        instrumentResource()
    );
    percussionSoundBlaster.clearRegisterWrites();
    percussionSoundBlaster.startSuccess();
    percussionSoundBlaster.render(
        stellar7::SoundBlasterSynth::sampleRate
        / stellar7::SoundBlasterSynth::sequenceRate
    );
    assert(
        std::none_of(
            percussionSoundBlaster.registerWrites().begin(),
            percussionSoundBlaster.registerWrites().end(),
            [](const stellar7::OplRegisterWrite write) {
                return write.registerNumber == 0x40
                    && write.value == 0x0f;
            }
        )
    );
    const auto& percussionWrites =
        percussionSoundBlaster.registerWrites();
    assert(percussionWrites.size() >= 3);
    const std::size_t percussionTail = percussionWrites.size() - 3;
    assert(percussionWrites[percussionTail + 0].registerNumber == 0x43);
    assert(percussionWrites[percussionTail + 1].registerNumber == 0xa0);
    assert(percussionWrites[percussionTail + 2].registerNumber == 0xb0);

    stellar7::SoundBlasterSynth ownedSoundBlaster(
        {ownedChannelSequence()},
        instrumentResource()
    );
    ownedSoundBlaster.clearRegisterWrites();
    ownedSoundBlaster.startSuccess();
    ownedSoundBlaster.render(
        stellar7::SoundBlasterSynth::sampleRate
        / stellar7::SoundBlasterSynth::sequenceRate
    );
    assert(
        std::any_of(
            ownedSoundBlaster.registerWrites().begin(),
            ownedSoundBlaster.registerWrites().end(),
            [](const stellar7::OplRegisterWrite write) {
                return write.registerNumber == 0xa2;
            }
        )
    );
    assert(
        std::any_of(
            ownedSoundBlaster.registerWrites().begin(),
            ownedSoundBlaster.registerWrites().end(),
            [](const stellar7::OplRegisterWrite write) {
                return write.registerNumber == 0xc3;
            }
        )
    );
    assert(
        std::none_of(
            ownedSoundBlaster.registerWrites().begin(),
            ownedSoundBlaster.registerWrites().end(),
            [](const stellar7::OplRegisterWrite write) {
                return write.registerNumber == 0xa0;
            }
        )
    );

    stellar7::SoundBlasterSynth immediateNotesSoundBlaster(
        {twoImmediateNotesSequence()},
        instrumentResource()
    );
    immediateNotesSoundBlaster.clearRegisterWrites();
    immediateNotesSoundBlaster.startSuccess();
    immediateNotesSoundBlaster.render(
        stellar7::SoundBlasterSynth::sampleRate
        / stellar7::SoundBlasterSynth::sequenceRate
    );
    const auto& immediateWrites =
        immediateNotesSoundBlaster.registerWrites();
    assert(immediateWrites.size() >= 6);
    const std::size_t immediateTail = immediateWrites.size() - 6;
    assert(immediateWrites[immediateTail + 0].registerNumber == 0x43);
    assert(immediateWrites[immediateTail + 1].registerNumber == 0xa0);
    assert(immediateWrites[immediateTail + 2].registerNumber == 0xb0);
    assert(immediateWrites[immediateTail + 3].registerNumber == 0x44);
    assert(immediateWrites[immediateTail + 4].registerNumber == 0xa1);
    assert(immediateWrites[immediateTail + 5].registerNumber == 0xb1);

    stellar7::SoundBlasterSynth defaultProgramSoundBlaster(
        {defaultProgramNoteSequence()},
        instrumentResource()
    );
    defaultProgramSoundBlaster.clearRegisterWrites();
    defaultProgramSoundBlaster.startSuccess();
    defaultProgramSoundBlaster.render(
        stellar7::SoundBlasterSynth::sampleRate
        / stellar7::SoundBlasterSynth::sequenceRate
    );
    const auto& defaultProgramWrites =
        defaultProgramSoundBlaster.registerWrites();
    assert(defaultProgramWrites.size() == 3);
    assert(defaultProgramWrites[0].registerNumber == 0x43);
    assert(defaultProgramWrites[1].registerNumber == 0xa0);
    assert(defaultProgramWrites[2].registerNumber == 0xb0);

    stellar7::SoundBlasterSynth repeatedNoteOffSoundBlaster(
        {repeatedNoteOffSequence()},
        instrumentResource()
    );
    repeatedNoteOffSoundBlaster.clearRegisterWrites();
    repeatedNoteOffSoundBlaster.startSuccess();
    repeatedNoteOffSoundBlaster.render(
        stellar7::SoundBlasterSynth::sampleRate
        / stellar7::SoundBlasterSynth::sequenceRate
    );
    const auto& repeatedNoteOffWrites =
        repeatedNoteOffSoundBlaster.registerWrites();
    assert(repeatedNoteOffWrites.size() == 7);
    assert(repeatedNoteOffWrites[3].registerNumber == 0xa0);
    assert(repeatedNoteOffWrites[5].registerNumber == 0xa0);
    assert(repeatedNoteOffWrites[4].value == repeatedNoteOffWrites[6].value);

    stellar7::SoundBlasterSynth endedHandleSoundBlaster(
        {defaultProgramNoteSequence()},
        instrumentResource()
    );
    endedHandleSoundBlaster.startSuccess();
    assert(endedHandleSoundBlaster.successActive());
    endedHandleSoundBlaster.clearRegisterWrites();
    endedHandleSoundBlaster.render(
        2 * stellar7::SoundBlasterSynth::sampleRate
        / stellar7::SoundBlasterSynth::sequenceRate
    );
    const auto automaticStopWrites =
        endedHandleSoundBlaster.registerWrites();
    assert(!endedHandleSoundBlaster.successActive());
    assert(automaticStopWrites.size() >= 18);
    for (std::size_t voice = 0; voice < 9; ++voice) {
        const std::size_t index = automaticStopWrites.size() - 18 + voice * 2;
        assert(automaticStopWrites[index].registerNumber == 0xa0 + voice);
        assert(automaticStopWrites[index + 1].registerNumber == 0xb0 + voice);
        assert((automaticStopWrites[index + 1].value & 0x20) == 0);
    }

    endedHandleSoundBlaster.clearRegisterWrites();
    endedHandleSoundBlaster.stopSuccess();
    const auto& explicitStopWrites =
        endedHandleSoundBlaster.registerWrites();
    assert(explicitStopWrites.size() == 18);
    assert(std::equal(
        automaticStopWrites.end() - 18,
        automaticStopWrites.end(),
        explicitStopWrites.begin(),
        [](const stellar7::OplRegisterWrite left,
           const stellar7::OplRegisterWrite right) {
            return left.registerNumber == right.registerNumber
                && left.value == right.value;
        }
    ));

    auto loopMarkerSequence = defaultProgramNoteSequence();
    loopMarkerSequence.pop_back();
    const std::vector<std::uint8_t> loopTail{0xcf, 0x7f, 0, 0x91, 61, 64, 1, 0xfc};
    loopMarkerSequence.insert(loopMarkerSequence.end(), loopTail.begin(), loopTail.end());
    stellar7::SoundBlasterSynth loopingSoundBlaster({}, instrumentResource(), loopMarkerSequence);
    loopingSoundBlaster.startMusic();
    constexpr std::uint64_t callbackPeriod = 5041ULL * 4 * stellar7::SoundBlasterSynth::sampleRate;
    constexpr std::uint64_t callbackClock = 1193182;
    constexpr auto secondCallback = (2 * callbackPeriod + callbackClock - 1) / callbackClock;
    constexpr auto thirdCallback = (3 * callbackPeriod + callbackClock - 1) / callbackClock;
    loopingSoundBlaster.render(secondCallback + 1);
    assert(loopingSoundBlaster.registerWrites().back().registerNumber == 0xb8);
    loopingSoundBlaster.clearRegisterWrites();
    loopingSoundBlaster.render(thirdCallback - secondCallback);
    assert(std::any_of(loopingSoundBlaster.registerWrites().begin(),
        loopingSoundBlaster.registerWrites().end(), [](const stellar7::OplRegisterWrite write) {
            return write.registerNumber == 0xa0 && write.value == 0x6b;
        }));
    assert(std::none_of(loopingSoundBlaster.registerWrites().begin(),
        loopingSoundBlaster.registerWrites().end(), [](const stellar7::OplRegisterWrite write) {
            return write.registerNumber == 0xa0 && write.value == 0x57;
        }));

    stellar7::SoundBlasterSynth headerLoopSoundBlaster(
        {}, instrumentResource(), defaultProgramNoteSequence()
    );
    headerLoopSoundBlaster.startMusic();
    headerLoopSoundBlaster.render(secondCallback);
    headerLoopSoundBlaster.clearRegisterWrites();
    headerLoopSoundBlaster.render(1);
    assert(std::any_of(headerLoopSoundBlaster.registerWrites().begin(),
        headerLoopSoundBlaster.registerWrites().end(), [](const stellar7::OplRegisterWrite write) {
            return write.registerNumber == 0xa0 && write.value == 0x57;
        }));
    headerLoopSoundBlaster.triggerLevel1Event(14);
    headerLoopSoundBlaster.render(stellar7::SoundBlasterSynth::sampleRate);
    assert(headerLoopSoundBlaster.driverState().directTimers[0] == 0);

    stellar7::SoundBlasterSynth endedEffectSoundBlaster(
        {defaultProgramNoteSequence()}, instrumentResource()
    );
    endedEffectSoundBlaster.startSuccess();
    while (endedEffectSoundBlaster.successActive()) {
        endedEffectSoundBlaster.render(1);
    }
    endedEffectSoundBlaster.triggerLevel1Event(14);
    const auto endedEffectTimers = endedEffectSoundBlaster.driverState().directTimers;
    endedEffectSoundBlaster.clearRegisterWrites();
    endedEffectSoundBlaster.render(stellar7::SoundBlasterSynth::sampleRate);
    assert(endedEffectSoundBlaster.driverState().directTimers == endedEffectTimers);
    assert(endedEffectSoundBlaster.registerWrites().empty());
    endedEffectSoundBlaster.stopSuccess();
    endedEffectSoundBlaster.render(stellar7::SoundBlasterSynth::sampleRate);
    assert(endedEffectSoundBlaster.driverState().directTimers[0] == 0);

    // Native 216a prefers the highest tied slot only when all slots are busy.
    struct DirectSlotTieCase {
        std::array<int, 3> initialEvents;
        std::array<std::uint16_t, 3> expectedTimers;
        std::uint8_t stolenVoice;
    };
    const std::array<DirectSlotTieCase, 4> directSlotTies{{
        {{{14, 14, 14}}, {{12, 12, 30}}, 8},
        {{{14, 14, 5}}, {{12, 30, 60}}, 7},
        {{{14, 5, 14}}, {{12, 60, 30}}, 8},
        {{{5, 14, 14}}, {{60, 12, 30}}, 8},
    }};
    for (const auto& test : directSlotTies) {
        stellar7::SoundBlasterSynth tiedEffects({}, instrumentResource());
        for (const auto event : test.initialEvents) {
            tiedEffects.triggerLevel1Event(event);
        }
        tiedEffects.clearRegisterWrites();
        tiedEffects.triggerLevel1Event(9);
        assert(tiedEffects.driverState().directTimers == test.expectedTimers);
        assert(tiedEffects.registerWrites().front().registerNumber
            == 0xa0 + test.stolenVoice);
    }

    // CP215: reproduce native2260..2297 timer transitions without importing
    // RAM. This synthetic prelude supplies timers only, not native voice state.
    stellar7::SoundBlasterSynth nativeHistorySoundBlaster({}, instrumentResource());
    const auto advanceHistoryCallbacks = [&](const unsigned count) {
        for (unsigned callback = 0; callback < count; ++callback) {
            const auto state = nativeHistorySoundBlaster.driverState();
            const auto untilCallback = state.clockPhase >= state.clockPeriod ? 0
                : (state.clockPeriod - state.clockPhase + callbackClock - 1) / callbackClock;
            nativeHistorySoundBlaster.render(static_cast<std::size_t>(untilCallback + 1));
        }
    };
    nativeHistorySoundBlaster.triggerLevel1Event(5);
    advanceHistoryCallbacks(21);
    nativeHistorySoundBlaster.triggerLevel1Event(9);
    nativeHistorySoundBlaster.triggerLevel1Event(6);
    advanceHistoryCallbacks(12);
    assert((nativeHistorySoundBlaster.driverState().directTimers
        == std::array<std::uint16_t, 3>{27, 18, 68}));
    struct NativeTimerEvent {
        unsigned callbacksBefore;
        int eventId;
        unsigned callbacksAfter;
        std::array<std::uint16_t, 3> expectedTimers;
    };
    // Intervening event-free updates are accumulated into callbacksBefore.
    const std::array<NativeTimerEvent, 5> nativeTimerEvents{{
        {13, 14, 3, {11, 9, 52}},
        {6, 12, 2, {3, 118, 44}},
        {12, 14, 0, {12, 106, 32}},
        {8, 5, 1, {59, 97, 23}},
        {29, 14, 1, {29, 67, 11}},
    }};
    for (const auto& event : nativeTimerEvents) {
        advanceHistoryCallbacks(event.callbacksBefore);
        nativeHistorySoundBlaster.triggerLevel1Event(event.eventId);
        advanceHistoryCallbacks(event.callbacksAfter);
        assert(nativeHistorySoundBlaster.driverState().directTimers == event.expectedTimers);
    }
    advanceHistoryCallbacks(2);
    assert((nativeHistorySoundBlaster.driverState().directTimers
        == std::array<std::uint16_t, 3>{27, 65, 9}));

    stellar7::SoundBlasterSynth coldLevel2SoundBlaster(
        {sevenVoiceSequence()}, instrumentResource(), {}, {}, nineVoiceProgramSequence()
    );
    coldLevel2SoundBlaster.startLevel2();
    coldLevel2SoundBlaster.clearRegisterWrites();
    coldLevel2SoundBlaster.render(stellar7::SoundBlasterSynth::sampleRate
        / stellar7::SoundBlasterSynth::sequenceRate);
    // Direct Level2 bootstrap derives the reserved-voice state from the
    // decoded prior success cue instead of overwriting it with velocity1.
    assert(std::any_of(coldLevel2SoundBlaster.registerWrites().begin(),
        coldLevel2SoundBlaster.registerWrites().end(),
        [](const stellar7::OplRegisterWrite write) {
            return write.registerNumber == 0x53 && write.value == 0x0e;
        }));

    stellar7::SoundBlasterSynth retainedTimerSoundBlaster(
        {defaultProgramNoteSequence()}, instrumentResource(), defaultProgramNoteSequence()
    );
    retainedTimerSoundBlaster.startMusic();
    retainedTimerSoundBlaster.triggerLevel1Event(14);
    retainedTimerSoundBlaster.clearRegisterWrites();
    retainedTimerSoundBlaster.startSuccess();
    // Native4122:2605 sees the direct-event FF latch at success entry.
    // Each release pair therefore has a following carrier40h write.
    assert(retainedTimerSoundBlaster.registerWrites().size() == 27);
    retainedTimerSoundBlaster.clearRegisterWrites();
    retainedTimerSoundBlaster.render(stellar7::SoundBlasterSynth::sampleRate);
    // Startup retains CS:bf/c1/c3. Automatic shutdown freezes the remaining
    // ten callbacks until explicit release removes the ended handle.
    assert(std::count_if(retainedTimerSoundBlaster.registerWrites().begin(),
        retainedTimerSoundBlaster.registerWrites().end(),
        [](const stellar7::OplRegisterWrite write) { return write.registerNumber == 0xa6; }) == 1);
    assert(retainedTimerSoundBlaster.driverState().directTimers[0] == 10);
    retainedTimerSoundBlaster.stopSuccess();
    retainedTimerSoundBlaster.clearRegisterWrites();
    retainedTimerSoundBlaster.render(stellar7::SoundBlasterSynth::sampleRate);
    assert(retainedTimerSoundBlaster.driverState().directTimers[0] == 0);
    assert(retainedTimerSoundBlaster.registerWrites().size() == 2);
    const auto& retainedTimerWrites = retainedTimerSoundBlaster.registerWrites();
    assert(retainedTimerWrites[retainedTimerWrites.size() - 2].value == 0x57);
    assert(retainedTimerWrites.back().value == 0x01);
    stellar7::SoundBlasterSynth retrySoundBlaster(
        {}, instrumentResource(), defaultProgramNoteSequence(), {}, oneNoteSequence()
    );
    retrySoundBlaster.startMusic();
    retrySoundBlaster.triggerLevel1Event(14);
    retrySoundBlaster.render(1);
    const auto retryEntry = retrySoundBlaster.driverState();
    retrySoundBlaster.clearRegisterWrites();
    retrySoundBlaster.stopMusic();
    const auto retryStopped = retrySoundBlaster.driverState();
    assert(!retrySoundBlaster.registerWrites().empty());
    assert(retryStopped.clockPhase == retryEntry.clockPhase);
    assert(retryStopped.clockPeriod == retryEntry.clockPeriod);
    assert(retryStopped.directTimers == retryEntry.directTimers);
    assert(retryStopped.directLevelLatch == retryEntry.directLevelLatch);
    assert(retryStopped.notes == retryEntry.notes);
    retrySoundBlaster.clearRegisterWrites();
    retrySoundBlaster.stopMusic();
    assert(retrySoundBlaster.registerWrites().empty());
    retrySoundBlaster.startMusic();
    const auto retryStarted = retrySoundBlaster.driverState();
    assert(retryStarted.sequencePosition == 36);
    assert(retryStarted.sequenceWait == 0);
    assert(retryStarted.runningStatus == 0);
    assert(retryStarted.clockPhase == retryEntry.clockPhase);
    assert(retryStarted.directTimers == retryEntry.directTimers);
    assert(retrySoundBlaster.registerWrites().empty());

    retrySoundBlaster.stopMusic();
    retrySoundBlaster.clearRegisterWrites();
    const auto level2RetryEntry = retrySoundBlaster.driverState();
    retrySoundBlaster.switchLevel2();
    const auto level2RetryStarted = retrySoundBlaster.driverState();
    assert(level2RetryStarted.clockPhase == level2RetryEntry.clockPhase);
    assert(level2RetryStarted.clockPeriod == level2RetryEntry.clockPeriod);
    assert(level2RetryStarted.directTimers == level2RetryEntry.directTimers);
    assert(level2RetryStarted.sequenceWait == 0 && level2RetryStarted.runningStatus == 0);
    assert(!level2RetryStarted.directLevelLatch);
    for (std::size_t voice = 6; voice < 9; ++voice) {
        assert(level2RetryStarted.velocities[voice] == level2RetryEntry.velocities[voice]);
    }
    assert(retrySoundBlaster.registerWrites().empty());

    auto delayedSuccess = defaultProgramNoteSequence();
    delayedSuccess[delayedSuccess.size() - 2] = 60;
    stellar7::SoundBlasterSynth nativeClockSoundBlaster(
        {delayedSuccess}, instrumentResource(), delayedSuccess
    );
    nativeClockSoundBlaster.startMusic();
    nativeClockSoundBlaster.triggerLevel1Event(14);
    nativeClockSoundBlaster.clearRegisterWrites();
    const auto entryDriver = nativeClockSoundBlaster.driverState();
    assert(entryDriver.directTimers[0] == 12);
    assert(entryDriver.directLevelLatch);
    assert(entryDriver.clockPeriod == 44100ULL * 5041 * 4);
    assert(entryDriver.clockPhase == entryDriver.clockPeriod);
    auto detachedDriver = nativeClockSoundBlaster.driverState();
    detachedDriver.notes.fill(255);
    detachedDriver.velocities.fill(255);
    detachedDriver.directTimers.fill(0);
    detachedDriver.directLevelLatch = false;
    detachedDriver.clockPhase = 0;
    detachedDriver.clockPeriod = 0;
    const auto unchangedDriver = nativeClockSoundBlaster.driverState();
    assert(unchangedDriver.notes == entryDriver.notes);
    assert(unchangedDriver.velocities == entryDriver.velocities);
    assert(unchangedDriver.directTimers == entryDriver.directTimers);
    assert(unchangedDriver.directLevelLatch == entryDriver.directLevelLatch);
    assert(unchangedDriver.clockPhase == entryDriver.clockPhase);
    assert(unchangedDriver.clockPeriod == entryDriver.clockPeriod);
    assert(nativeClockSoundBlaster.registerWrites().empty());
    // Original IRQ0 divisor5041 and callback reload4. With a tick at sample0,
    // a twelve-tick direct event expires on callback11, not at 11/60 seconds.
    constexpr std::uint64_t pitClock = 1193182;
    constexpr std::uint64_t audioPitCycles = 5041 * 4;
    constexpr std::size_t expirySample = static_cast<std::size_t>(
        (11 * audioPitCycles * stellar7::SoundBlasterSynth::sampleRate + pitClock - 1) / pitClock
    );
    nativeClockSoundBlaster.render(expirySample);
    assert(nativeClockSoundBlaster.driverState().directTimers[0] == 1);
    assert(std::none_of(nativeClockSoundBlaster.registerWrites().begin(),
        nativeClockSoundBlaster.registerWrites().end(),
        [](const stellar7::OplRegisterWrite write) { return write.registerNumber == 0xa6; }));
    nativeClockSoundBlaster.clearRegisterWrites();
    nativeClockSoundBlaster.render(1);
    assert(nativeClockSoundBlaster.driverState().directTimers[0] == 0);
    // The intervening music note clears the direct-level latch.
    assert(nativeClockSoundBlaster.registerWrites().size() == 2);
    assert(nativeClockSoundBlaster.registerWrites()[0].registerNumber == 0xa6);
    assert(nativeClockSoundBlaster.registerWrites()[1].registerNumber == 0xb6);

    stellar7::SoundBlasterSynth phasePreservingSoundBlaster(
        {delayedSuccess}, instrumentResource(), delayedSuccess, {}, delayedSuccess
    );
    phasePreservingSoundBlaster.startMusic();
    phasePreservingSoundBlaster.triggerLevel1Event(14);
    constexpr std::size_t midCallbackSample = 100;
    phasePreservingSoundBlaster.render(midCallbackSample);
    const auto beforeSequenceSwitch = phasePreservingSoundBlaster.driverState();
    phasePreservingSoundBlaster.startSuccess();
    assert(phasePreservingSoundBlaster.driverState().clockPhase
        == beforeSequenceSwitch.clockPhase);
    assert(phasePreservingSoundBlaster.driverState().directTimers
        == beforeSequenceSwitch.directTimers);
    phasePreservingSoundBlaster.render(expirySample - midCallbackSample);
    assert(phasePreservingSoundBlaster.driverState().directTimers[0] == 1);
    phasePreservingSoundBlaster.render(1);
    assert(phasePreservingSoundBlaster.driverState().directTimers[0] == 0);
    const auto beforeLevelSwitch = phasePreservingSoundBlaster.driverState();
    phasePreservingSoundBlaster.switchLevel2();
    assert(phasePreservingSoundBlaster.driverState().clockPhase == beforeLevelSwitch.clockPhase);

    stellar7::SoundBlasterSynth resetTimerSoundBlaster(
        {delayedSuccess}, instrumentResource(), defaultProgramNoteSequence()
    );
    resetTimerSoundBlaster.startMusic();
    resetTimerSoundBlaster.triggerLevel1Event(14);
    resetTimerSoundBlaster.startSuccess();
    resetTimerSoundBlaster.clearRegisterWrites();
    resetTimerSoundBlaster.render(stellar7::SoundBlasterSynth::sampleRate / 2);
    const auto& resetTimerWrites = resetTimerSoundBlaster.registerWrites();
    // The surviving SFX timer expires before any success shutdown has
    // populated the reset voice's cached frequency. 2331 still maps note0.
    assert(resetTimerWrites[resetTimerWrites.size() - 2].registerNumber == 0xa6);
    assert(resetTimerWrites[resetTimerWrites.size() - 2].value == 0x57);
    assert(resetTimerWrites.back().value == 0x01);

    stellar7::SoundBlasterSynth retainedVoiceSoundBlaster(
        {nineVoiceProgramSequence()},
        instrumentResource(),
        sevenVoiceSequence()
    );
    retainedVoiceSoundBlaster.startMusic();
    retainedVoiceSoundBlaster.render(
        stellar7::SoundBlasterSynth::sampleRate
        / stellar7::SoundBlasterSynth::sequenceRate
    );
    retainedVoiceSoundBlaster.clearRegisterWrites();
    retainedVoiceSoundBlaster.startSuccess();
    retainedVoiceSoundBlaster.render(
        stellar7::SoundBlasterSynth::sampleRate
        / stellar7::SoundBlasterSynth::sequenceRate
    );
    assert(
        std::any_of(
            retainedVoiceSoundBlaster.registerWrites().begin(),
            retainedVoiceSoundBlaster.registerWrites().end(),
            [](const stellar7::OplRegisterWrite write) {
                return write.registerNumber == 0x53
                    && write.value == 0x0e;
            }
        )
    );
    retainedVoiceSoundBlaster.clearRegisterWrites();
    retainedVoiceSoundBlaster.triggerLevel1Event(14);
    const auto& directWrites = retainedVoiceSoundBlaster.registerWrites();
    // The success fixture owns all nine voices, so channel13 cannot refresh
    // the carrier before its fixed physical note.
    assert(directWrites.size() == 3);
    assert(directWrites[directWrites.size() - 3].registerNumber == 0xa6);
    assert(directWrites[directWrites.size() - 3].value == 0x57);
    assert(directWrites[directWrites.size() - 2].registerNumber == 0xb6);
    assert(directWrites[directWrites.size() - 2].value == 0x21);
    assert(directWrites[directWrites.size() - 1].registerNumber == 0x53);
    assert(directWrites[directWrites.size() - 1].value == 0x40);

    stellar7::SoundBlasterSynth level3EventSoundBlaster(
        {},
        instrumentResource()
    );
    level3EventSoundBlaster.clearRegisterWrites();
    level3EventSoundBlaster.triggerLevel1Event(12);
    const auto& level3EventWrites =
        level3EventSoundBlaster.registerWrites();
    assert(level3EventWrites.size() == 19);
    assert(level3EventWrites[15].registerNumber == 0x53);
    assert(level3EventWrites[16].registerNumber == 0xa6);
    assert(level3EventWrites[16].value == 0x57);
    assert(level3EventWrites[17].registerNumber == 0xb6);
    assert(level3EventWrites[17].value == 0x21);
    assert(level3EventWrites[18].registerNumber == 0x53);
    assert(level3EventWrites[18].value == 0x40);

    const auto beforeDirectReset = level3EventSoundBlaster.driverState();
    const auto beforeDirectResetWrites = level3EventSoundBlaster.registerWrites().size();
    level3EventSoundBlaster.resetDirectEffects();
    const auto afterDirectReset = level3EventSoundBlaster.driverState();
    assert(level3EventSoundBlaster.registerWrites().size() == beforeDirectResetWrites);
    assert(afterDirectReset.directTimers == beforeDirectReset.directTimers);
    assert(afterDirectReset.clockPhase == beforeDirectReset.clockPhase);
    assert(afterDirectReset.sequencePosition == beforeDirectReset.sequencePosition);
    assert(afterDirectReset.sequenceWait == beforeDirectReset.sequenceWait);
    assert(afterDirectReset.fade == beforeDirectReset.fade);
    assert(afterDirectReset.fadeDivider == beforeDirectReset.fadeDivider);
    assert(afterDirectReset.carrierFallback == beforeDirectReset.carrierFallback);
    assert(afterDirectReset.operatorFallback == beforeDirectReset.operatorFallback);
    for (std::size_t voice = 0; voice < 9; ++voice) {
        if (voice < 6) {
            assert(afterDirectReset.notes[voice] == beforeDirectReset.notes[voice]);
            assert(afterDirectReset.velocities[voice] == beforeDirectReset.velocities[voice]);
            continue;
        }
        assert(afterDirectReset.notes[voice] == 0);
        assert(afterDirectReset.velocities[voice] == beforeDirectReset.velocities[voice]);
        assert(afterDirectReset.carrierKsl[voice] == 0);
        assert(afterDirectReset.operatorKsl[voice] == 0);
        assert(afterDirectReset.operatorEnabled[voice]);
    }

    assert(stellar7::PcSpeakerSynth::sequenceRate == 15);
    assert(stellar7::pcSpeakerDivisor(23) == 0);
    assert(stellar7::pcSpeakerDivisor(60) == 5080);
    assert(stellar7::pcSpeakerDivisor(35, 0x2156) == 21126);
    assert(stellar7::pcSpeakerDivisor(120) == 0);

    assert(stellar7::level1EventSequenceIndex(14) == 3);
    assert(stellar7::level1EventSequenceIndex(6) == 13);
    assert(stellar7::level1EventSequenceIndex(17) == -1);

    stellar7::PcSpeakerSynth synth({oneNoteSequence()});
    synth.startMusic();
    constexpr std::size_t samplesPerTick =
        stellar7::PcSpeakerSynth::sampleRate
        / stellar7::PcSpeakerSynth::sequenceRate;
    const auto sounding = synth.render(samplesPerTick);
    assert(
        std::any_of(
            sounding.begin(),
            sounding.end(),
            [](const std::int16_t value) { return value != 0; }
        )
    );
    const auto silent = synth.render(samplesPerTick);
    assert(
        std::all_of(
            silent.begin(),
            silent.end(),
            [](const std::int16_t value) { return value == 0; }
        )
    );
}
