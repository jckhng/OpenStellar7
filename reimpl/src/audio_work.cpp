#include "stellar7/audio_work.hpp"
#include "stellar7/audio_program_work.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {

constexpr std::uint16_t maximumMusicFade = 15;
constexpr std::uint64_t callbackWrapperDispatches = 23;
constexpr std::uint64_t residentEntryDispatches = 21;
constexpr std::uint64_t sequenceWaitDispatches = 15;
constexpr std::uint64_t directTimerSetupReturnDispatches = 5;
constexpr std::uint64_t directTimerTestDispatches = 3;
constexpr std::uint64_t directTimerDecrementDispatches = 3;
constexpr std::uint64_t oplAddressReadCount = 6;
constexpr std::uint64_t oplDataReadCount = 37;
constexpr std::uint64_t oplReadLoopDispatches = 2;
constexpr std::uint64_t oplAddressSetupDispatches = 10;
constexpr std::uint64_t oplDataSetupReturnDispatches = 8;
constexpr std::uint64_t oplWriteCount = 2;
constexpr std::uint64_t directTimerExpiryDispatches = 2;
constexpr std::uint64_t keyOffWrapperDispatches = 13;
constexpr std::uint64_t frequencyEntryDispatches = 7;
constexpr std::uint64_t frequencyGateDispatches = 5;
constexpr std::uint64_t frequencySuppressedReturnDispatches = 3;
constexpr std::uint64_t frequencySetupDispatches = 12;
constexpr std::uint64_t frequencyLowWriteDispatches = 6;
constexpr std::uint64_t frequencyHighWriteDispatches = 22;
constexpr std::uint64_t frequencyLatchReturnDispatches = 10;
constexpr std::uint64_t frequencyLatchedExtraDispatches = 18;
constexpr std::uint64_t callbackWrapperReturnDispatches = 7;
constexpr std::uint64_t residentReturnDispatches = 9;
constexpr std::uint64_t explicitEventEntryDispatches = 15;
constexpr std::uint64_t explicitParserPrefixDispatches = 30;
constexpr std::uint64_t runningParserPrefixDispatches = 28;
constexpr std::uint64_t explicitParserReturnDispatches = 1;
constexpr std::uint64_t zeroVelocityHandlerPrefixDispatches = 15;
constexpr std::uint64_t zeroVelocityHandlerReturnDispatches = 6;
constexpr std::uint64_t keyOffScanSetupDispatches = 4;
constexpr std::uint64_t keyOffScanReturnDispatches = 3;
constexpr std::uint64_t keyOffOwnerTestDispatches = 2;
constexpr std::uint64_t keyOffNoteTestDispatches = 2;
constexpr std::uint64_t keyOffNextVoiceDispatches = 3;
constexpr std::uint64_t keyOffMatchedPrefixDispatches = 7;
constexpr std::uint64_t keyOffMatchedReturnDispatches = 2;
constexpr std::uint64_t keyOffWrapperPrefixDispatches = 8;
constexpr std::uint64_t keyOffWrapperReturnDispatches = 5;
constexpr std::uint64_t eventReturnDispatches = 4;
constexpr std::uint64_t simpleDeltaDispatches = 21;
constexpr std::uint64_t positiveDeltaDispatches = 6;
constexpr std::uint64_t callbackReturnDispatches = 6;
constexpr std::size_t maximumKeyOffEvents = 32;
constexpr std::uint8_t lowestKeyOffNote = 12;
constexpr std::uint8_t highestKeyOffNote = 107;
constexpr std::uint8_t highestMidiChannel = 15;
constexpr std::uint8_t maximumSimpleDelta = 127;
constexpr std::uint64_t positiveHandlerPrefixDispatches = 17;
constexpr std::uint64_t positiveLevelPrefixDispatches = 50;
constexpr std::uint64_t positiveLevelReturnDispatches = 8;
constexpr std::uint64_t disabledLevelDispatches = 6;
constexpr std::uint64_t positiveWrapperPrefixDispatches = 6;
constexpr std::uint64_t positiveWrapperReturnDispatches = 5;
constexpr std::uint64_t positiveCursorDispatches = 4;
constexpr std::uint64_t positiveHandlerReturnDispatches = 5;
constexpr std::uint64_t noOwnerCursorSetupDispatches = 3;
constexpr std::uint8_t explicitNoteOnStatus = 0x90;
constexpr std::size_t explicitNoteMessageBytes = 4;
constexpr std::size_t maximumSequenceCursor = 0xffff;
constexpr std::size_t runningNoteMessageBytes = 3;
constexpr std::uint64_t replacementCursorSetupDispatches = 2;

enum class NoteParserForm { explicitStatus, runningStatus };
enum class VoiceAllocationDomain { inactiveOnly, replaceActive };

struct FrequencyWork {
    std::uint64_t dispatches;
    std::uint64_t registerWrites;
};

FrequencyWork frequencyWork(const SoundBlasterFrequencyGate gate, const SoundBlasterLevelLatch latch) {
    const auto opl = soundBlasterOplWork();
    FrequencyWork work{frequencyEntryDispatches + frequencyGateDispatches + frequencySuppressedReturnDispatches, 0};
    if (gate != SoundBlasterFrequencyGate::Open) {
        return work;
    }
    work.dispatches = frequencyEntryDispatches + frequencyGateDispatches + frequencySetupDispatches
        + frequencyLowWriteDispatches + frequencyHighWriteDispatches + frequencyLatchReturnDispatches + 2 * opl.dispatches;
    work.registerWrites = 2;
    if (latch == SoundBlasterLevelLatch::Latched) {
        work.dispatches += frequencyLatchedExtraDispatches + opl.dispatches;
        ++work.registerWrites;
    }
    return work;
}

struct CallbackWorkStream {
    std::uint64_t& dispatches;
    std::uint64_t& portReads;
    std::uint64_t& portWrites;
    std::vector<SoundBlasterWorkBlock>& blocks;

    void plain(const std::uint64_t count) {
        dispatches += count;
        if (!blocks.empty() && blocks.back().kind == SoundBlasterWorkKind::plain) {
            blocks.back().dispatches += count;
            return;
        }
        blocks.push_back({SoundBlasterWorkKind::plain, count, -1});
    }

    void write(const SoundBlasterWorkKind kind, const int voice) {
        const auto opl = soundBlasterOplWork();
        blocks.push_back({kind, opl.dispatches, voice});
        dispatches += opl.dispatches;
        portReads += opl.portReads;
        portWrites += opl.portWrites;
    }
};

SoundBlasterKeyOffEventWork keyOffEvent(CallbackWorkStream& stream,
    const SoundBlasterKeyOffState& state, const SoundBlasterExplicitKeyOff& event,
    const SoundBlasterFrequencyGate gate, const SoundBlasterLevelLatch latch) {
    SoundBlasterKeyOffEventWork detail;
    detail.eventStartDispatches = stream.dispatches;
    detail.parserDispatches = explicitParserPrefixDispatches + explicitParserReturnDispatches;
    detail.handlerDispatches = zeroVelocityHandlerPrefixDispatches + zeroVelocityHandlerReturnDispatches;
    stream.plain(explicitParserPrefixDispatches + zeroVelocityHandlerPrefixDispatches);
    const auto scanStart = stream.dispatches;
    stream.plain(keyOffScanSetupDispatches);
    for (std::size_t voice = 0; voice < state.channels.size(); ++voice) {
        stream.plain(keyOffOwnerTestDispatches);
        if (state.channels[voice] != event.channel) {
            stream.plain(keyOffNextVoiceDispatches);
            continue;
        }
        stream.plain(keyOffNoteTestDispatches);
        if (state.notes[voice] != event.note) {
            stream.plain(keyOffNextVoiceDispatches);
            continue;
        }
        if (state.channelDeferrals[event.channel] == 1 || state.tuning[voice] != 0) {
            throw std::invalid_argument("tuned/deferred key-off outside bounded work family");
        }
        detail.selectedVoice = static_cast<int>(voice);
        stream.plain(keyOffMatchedPrefixDispatches + keyOffWrapperPrefixDispatches);
        detail.frequencyStartDispatches = stream.dispatches;
        if (gate == SoundBlasterFrequencyGate::Suppressed) {
            stream.plain(frequencyWork(gate, latch).dispatches);
        } else {
            stream.plain(frequencyEntryDispatches + frequencyGateDispatches + frequencySetupDispatches + frequencyLowWriteDispatches);
            stream.write(SoundBlasterWorkKind::frequencyLow, detail.selectedVoice);
            stream.plain(frequencyHighWriteDispatches);
            stream.write(SoundBlasterWorkKind::frequencyHigh, detail.selectedVoice);
            if (latch == SoundBlasterLevelLatch::Latched) {
                stream.plain(frequencyLatchedExtraDispatches);
                stream.write(SoundBlasterWorkKind::carrierLevel, detail.selectedVoice);
            }
            stream.plain(frequencyLatchReturnDispatches);
        }
        stream.plain(keyOffWrapperReturnDispatches + keyOffMatchedReturnDispatches);
        break;
    }
    stream.plain(keyOffScanReturnDispatches);
    detail.scanDispatches = stream.dispatches - scanStart;
    stream.plain(zeroVelocityHandlerReturnDispatches + explicitParserReturnDispatches + eventReturnDispatches);
    detail.deltaDispatches = simpleDeltaDispatches + (event.delay != 0 ? positiveDeltaDispatches : 0);
    stream.plain(detail.deltaDispatches);
    return detail;
}

SoundBlasterNoteOnEventWork noteOnEvent(CallbackWorkStream& stream,
    SoundBlasterCallbackState& state, const SoundBlasterExplicitNoteOn& event,
    const NoteParserForm form, const VoiceAllocationDomain allocationDomain) {
    const auto gate = state.gate;
    SoundBlasterNoteOnEventWork detail;
    detail.eventStartDispatches = stream.dispatches;
    stream.plain((form == NoteParserForm::explicitStatus
        ? explicitParserPrefixDispatches : runningParserPrefixDispatches) + positiveHandlerPrefixDispatches);
    bool ownsVoice = false;
    for (std::size_t voice = 0; voice < state.voices.channels.size(); ++voice) {
        stream.plain(keyOffOwnerTestDispatches);
        if (state.voices.channels[voice] == event.channel) {
            ownsVoice = true;
            stream.plain(keyOffNoteTestDispatches); // Sounding test, not retained-note matching.
            if (state.sounding[voice] == 0) {
                detail.selectedVoice = static_cast<int>(voice);
                break;
            }
        }
        stream.plain(keyOffNextVoiceDispatches);
    }
    bool replacing = false;
    if (detail.selectedVoice < 0 && ownsVoice) {
        if (allocationDomain == VoiceAllocationDomain::inactiveOnly) {
            throw std::invalid_argument("active voice stealing excluded from positive-note work");
        }
        stream.plain(replacementCursorSetupDispatches);
        auto voice = static_cast<std::size_t>(state.channelVoiceCursors[event.channel]);
        for (std::size_t distance = 0; distance < state.voices.channels.size(); ++distance) {
            stream.plain(keyOffOwnerTestDispatches);
            if (state.voices.channels[voice] == event.channel) {
                detail.selectedVoice = static_cast<int>(voice);
                break;
            }
            stream.plain(keyOffNextVoiceDispatches);
            ++voice;
            if (voice == state.voices.channels.size()) {
                voice = 0;
                stream.plain(1); //1e2a wraps before the second owner scan.
            }
        }
        replacing = true;
    }
    if (detail.selectedVoice < 0) {
        const auto cursor = state.channelVoiceCursors[event.channel];
        stream.plain(noOwnerCursorSetupDispatches + state.voices.channels.size() * (keyOffOwnerTestDispatches + keyOffNextVoiceDispatches));
        if (cursor == 0) {
            stream.plain(keyOffOwnerTestDispatches + keyOffNextVoiceDispatches);
        }
        stream.plain(positiveCursorDispatches + (cursor + 1 == state.voices.channels.size() ? 1 : 0));
        state.channelVoiceCursors[event.channel] = static_cast<std::uint8_t>(cursor == 0 ? 2 : (cursor + 1) % state.voices.channels.size());
    } else {
        const auto voice = static_cast<std::size_t>(detail.selectedVoice);
        if (gate == SoundBlasterFrequencyGate::Open && state.voices.tuning[voice] != 0) {
            throw std::invalid_argument("tuned positive-note frequency outside bounded work");
        }
        if (replacing) {
            stream.plain(2 + keyOffWrapperPrefixDispatches); // Deferred marker, CALL2331 and old-note wrapper.
            if (gate == SoundBlasterFrequencyGate::Suppressed) {
                stream.plain(frequencyWork(gate, state.latch).dispatches);
            } else {
                stream.plain(frequencyEntryDispatches + frequencyGateDispatches + frequencySetupDispatches + frequencyLowWriteDispatches);
                stream.write(SoundBlasterWorkKind::frequencyLow, detail.selectedVoice);
                stream.plain(frequencyHighWriteDispatches);
                stream.write(SoundBlasterWorkKind::frequencyHigh, detail.selectedVoice);
                if (state.latch == SoundBlasterLevelLatch::Latched) {
                    stream.plain(frequencyLatchedExtraDispatches);
                    stream.write(SoundBlasterWorkKind::carrierLevel, detail.selectedVoice);
                }
                stream.plain(frequencyLatchReturnDispatches);
            }
            stream.plain(keyOffWrapperReturnDispatches);
        }
        stream.plain(1); // CALL1e61, before its channel-enable test.
        if (state.channelEnabled[event.channel]) {
            stream.plain(positiveLevelPrefixDispatches);
            stream.write(SoundBlasterWorkKind::carrierLevel, detail.selectedVoice);
            stream.plain(positiveLevelReturnDispatches);
        } else {
            stream.plain(disabledLevelDispatches);
        }
        if (voice == 0) {
            state.latch = SoundBlasterLevelLatch::Unlatched;
        }
        stream.plain((replacing ? 1 : 2) + positiveWrapperPrefixDispatches); // Replacement set the marker before key-off.
        detail.frequencyStartDispatches = stream.dispatches;
        if (gate == SoundBlasterFrequencyGate::Suppressed) {
            stream.plain(frequencyWork(gate, state.latch).dispatches);
        } else {
            stream.plain(frequencyEntryDispatches + frequencyGateDispatches + frequencySetupDispatches + frequencyLowWriteDispatches);
            stream.write(SoundBlasterWorkKind::frequencyLow, detail.selectedVoice);
            stream.plain(frequencyHighWriteDispatches);
            stream.write(SoundBlasterWorkKind::frequencyHigh, detail.selectedVoice);
            if (state.latch == SoundBlasterLevelLatch::Latched) {
                stream.plain(frequencyLatchedExtraDispatches);
                stream.write(SoundBlasterWorkKind::carrierLevel, detail.selectedVoice);
            }
            stream.plain(frequencyLatchReturnDispatches);
        }
        state.voices.notes[voice] = event.note;
        state.velocities[voice] = state.channelEnabled[event.channel] ? event.velocity : 0;
        state.sounding[voice] = 1;
        stream.plain(positiveWrapperReturnDispatches + 1 + positiveCursorDispatches
            + (voice + 1 == state.voices.channels.size() ? 1 : 0));
        state.channelVoiceCursors[event.channel] = static_cast<std::uint8_t>((voice + 1) % state.voices.channels.size());
    }
    stream.plain(positiveHandlerReturnDispatches + explicitParserReturnDispatches + eventReturnDispatches
        + simpleDeltaDispatches + (event.delay != 0 ? positiveDeltaDispatches : 0));
    return detail;
}


}  // namespace

std::uint64_t soundBlasterWaitDispatches(const std::array<std::uint16_t, 3>& directTimers) {
    for (const auto timer : directTimers) {
        if (timer == 1) {
            throw std::invalid_argument("expiring direct timer requires separate voice/device work");
        }
    }
    const auto timers = soundBlasterDirectTimerWork(directTimers, SoundBlasterFrequencyGate::Open, SoundBlasterLevelLatch::Unlatched);
    return callbackWrapperDispatches + residentEntryDispatches + sequenceWaitDispatches + timers.dispatches;
}

SoundBlasterOplWork soundBlasterOplWork() {
    const auto address = oplAddressSetupDispatches + oplReadLoopDispatches * oplAddressReadCount;
    const auto data = oplDataSetupReturnDispatches + oplReadLoopDispatches * oplDataReadCount;
    return {address + data, address, data, oplAddressReadCount + oplDataReadCount, oplWriteCount};
}

SoundBlasterTimerWork soundBlasterDirectTimerWork(
    const std::array<std::uint16_t, 3>& timers,
    const SoundBlasterFrequencyGate gate,
    const SoundBlasterLevelLatch latch
) {
    const auto opl = soundBlasterOplWork();
    const auto frequency = frequencyWork(gate, latch);
    SoundBlasterTimerWork work{directTimerSetupReturnDispatches, 0, 0, timers, 0};
    for (std::size_t slot = 0; slot < work.timers.size(); ++slot) {
        work.dispatches += directTimerTestDispatches;
        if (work.timers[slot] == 0) {
            continue;
        }
        --work.timers[slot];
        work.dispatches += directTimerDecrementDispatches;
        if (work.timers[slot] != 0) {
            continue;
        }
        work.expiredMask |= static_cast<std::uint8_t>(1U << slot);
        work.dispatches += directTimerExpiryDispatches + keyOffWrapperDispatches + frequency.dispatches;
        work.portReads += frequency.registerWrites * opl.portReads;
        work.portWrites += frequency.registerWrites * opl.portWrites;
    }
    return work;
}

SoundBlasterKeyOffWork soundBlasterExplicitKeyOffWork(
    const SoundBlasterKeyOffState& state,
    const std::vector<SoundBlasterExplicitKeyOff>& events,
    const SoundBlasterFrequencyGate gate,
    const SoundBlasterLevelLatch latch
) {
    if (events.empty() || events.size() > maximumKeyOffEvents) {
        throw std::invalid_argument("explicit key-off event count outside bounded family");
    }
    for (const auto timer : state.directTimers) {
        if (timer == 1) {
            throw std::invalid_argument("direct expiry excluded from key-off callback work");
        }
    }
    for (std::size_t index = 0; index < events.size(); ++index) {
        const auto& event = events[index];
        if (event.channel > highestMidiChannel || event.note < lowestKeyOffNote || event.note > highestKeyOffNote
            || event.delay > maximumSimpleDelta || (event.delay != 0) != (index + 1 == events.size())) {
            throw std::invalid_argument("explicit key-off requires note range/simple terminal delta");
        }
    }
    SoundBlasterKeyOffWork work;
    work.timerWork = soundBlasterDirectTimerWork(state.directTimers, gate, latch);
    CallbackWorkStream stream{work.dispatches, work.portReads, work.portWrites, work.blocks};
    stream.plain(callbackWrapperDispatches - callbackWrapperReturnDispatches
        + residentEntryDispatches - residentReturnDispatches + explicitEventEntryDispatches + work.timerWork.dispatches);
    for (const auto& event : events) {
        work.events.push_back(keyOffEvent(stream, state, event, gate, latch));
    }
    stream.plain(callbackReturnDispatches + residentReturnDispatches + callbackWrapperReturnDispatches);
    work.returnedWait = events.back().delay - 1;
    work.consumedBytes = events.size() * 4;
    return work;
}

namespace {

SoundBlasterNoteOnWork noteOnWork(
    const SoundBlasterNoteOnState& state,
    const std::vector<SoundBlasterExplicitNoteOn>& events,
    const std::vector<NoteParserForm>& parserForms,
    const SoundBlasterFrequencyGate gate,
    const SoundBlasterLevelLatch latch,
    const VoiceAllocationDomain allocationDomain
) {
    if (events.empty() || events.size() > maximumKeyOffEvents
        || parserForms.size() != events.size()
        || (gate != SoundBlasterFrequencyGate::Open && gate != SoundBlasterFrequencyGate::Suppressed)
        || (latch != SoundBlasterLevelLatch::Unlatched && latch != SoundBlasterLevelLatch::Latched)) {
        throw std::invalid_argument("invalid bounded positive-note work domain");
    }
    for (const auto cursor : state.channelVoiceCursors) {
        if (cursor >= state.channels.size()) {
            throw std::invalid_argument("positive-note cursor outside physical voice range");
        }
    }
    for (const auto timer : state.directTimers) {
        if (timer == 1) {
            throw std::invalid_argument("direct expiry excluded from positive-note work");
        }
    }
    for (std::size_t index = 0; index < events.size(); ++index) {
        const auto& event = events[index];
        if (event.channel > highestMidiChannel || event.note < lowestKeyOffNote || event.note > highestKeyOffNote
            || event.velocity == 0 || event.velocity > maximumSimpleDelta || event.delay > maximumSimpleDelta
            || (event.delay != 0) != (index + 1 == events.size())) {
            throw std::invalid_argument("positive-note requires note/velocity range/simple terminal delta");
        }
    }
    SoundBlasterNoteOnWork work;
    work.channelVoiceCursors = state.channelVoiceCursors;
    work.timerWork = soundBlasterDirectTimerWork(state.directTimers, gate, latch);
    SoundBlasterCallbackState current{};
    current.voices.channels = state.channels;
    current.voices.tuning = state.tuning;
    current.sounding = state.sounding;
    current.channelEnabled = state.channelEnabled;
    current.channelVoiceCursors = state.channelVoiceCursors;
    current.gate = gate;
    current.latch = latch;
    CallbackWorkStream stream{work.dispatches, work.portReads, work.portWrites, work.blocks};
    stream.plain(callbackWrapperDispatches - callbackWrapperReturnDispatches
        + residentEntryDispatches - residentReturnDispatches + explicitEventEntryDispatches + work.timerWork.dispatches);
    for (std::size_t eventIndex = 0; eventIndex < events.size(); ++eventIndex) {
        const auto& event = events[eventIndex];
        work.events.push_back(noteOnEvent(stream, current, event, parserForms[eventIndex], allocationDomain));
        work.consumedBytes += parserForms[eventIndex] == NoteParserForm::explicitStatus
            ? explicitNoteMessageBytes : runningNoteMessageBytes;
    }
    stream.plain(callbackReturnDispatches + residentReturnDispatches + callbackWrapperReturnDispatches);
    work.channelVoiceCursors = current.channelVoiceCursors;
    work.returnedWait = events.back().delay - 1;
    return work;
}

}  // namespace

SoundBlasterNoteOnWork soundBlasterExplicitNoteOnWork(
    const SoundBlasterNoteOnState& state,
    const std::vector<SoundBlasterExplicitNoteOn>& events,
    const SoundBlasterFrequencyGate gate,
    const SoundBlasterLevelLatch latch
) {
    return noteOnWork(state, events, std::vector<NoteParserForm>(events.size(), NoteParserForm::explicitStatus),
        gate, latch, VoiceAllocationDomain::inactiveOnly);
}

SoundBlasterCallbackAdvance soundBlasterCallbackAdvance(const SoundBlasterCallbackState& state) {
    if (state.domain == SoundBlasterCallbackDomain::level2RetryNoHandle) {
        return soundBlasterLevel2RetryNoHandleAdvance(state);
    }
    if (state.domain == SoundBlasterCallbackDomain::successNoHandle) {
        return soundBlasterSuccessNoHandleAdvance(state);
    }
    if (state.domain == SoundBlasterCallbackDomain::noHandle) {
        if (!state.driverEnabled || state.active || !state.sequence.empty() || state.position != 0
            || state.wait != 0 || state.fade > maximumMusicFade || state.mode != 0 || state.operation != 0x1e
            || state.runningStatus != 0 || state.gate != SoundBlasterFrequencyGate::Open
            || state.latch != (state.velocities[0] == 0xff
                ? SoundBlasterLevelLatch::Latched : SoundBlasterLevelLatch::Unlatched)
            || std::any_of(state.voices.directTimers.begin(), state.voices.directTimers.end(),
                [](const auto timer) { return timer != 0; })) {
            throw std::invalid_argument("unsupported registered no-handle callback state");
        }
        // 0494 wrapper23 + driver dispatcher21 + 1C6B zero-timer helper14.
        return {state, SoundBlasterCallbackKind::noHandle, soundBlasterNoHandleDispatches,
            {{SoundBlasterWorkKind::plain, soundBlasterNoHandleDispatches, -1}}};
    }
    if (state.domain != SoundBlasterCallbackDomain::sequence || !state.driverEnabled) {
        throw std::invalid_argument("unsupported callback driver domain");
    }
    if (state.program) {
        return soundBlasterProgramCallbackAdvance(state);
    }
    if (!state.active || state.mode != 0 || state.operation != 2 || state.fade != 0
        || (state.gate != SoundBlasterFrequencyGate::Open && state.gate != SoundBlasterFrequencyGate::Suppressed)
        || (state.latch != SoundBlasterLevelLatch::Unlatched && state.latch != SoundBlasterLevelLatch::Latched)) {
        throw std::invalid_argument("unsupported callback sequencer domain");
    }
    SoundBlasterCallbackAdvance result{state, SoundBlasterCallbackKind::wait};
    if (state.wait != 0) {
        result.dispatches = soundBlasterWaitDispatches(state.voices.directTimers);
        result.blocks.push_back({SoundBlasterWorkKind::plain, result.dispatches, -1});
        --result.state.wait;
        for (auto& timer : result.state.voices.directTimers) {
            if (timer != 0) {
                --timer;
            }
        }
        return result;
    }
    std::vector<SoundBlasterExplicitNoteOn> events;
    std::vector<NoteParserForm> parserForms;
    auto cursor = state.position;
    auto runningStatus = state.runningStatus;
    while (events.size() < maximumKeyOffEvents) {
        if (cursor >= state.sequence.size() || cursor > maximumSequenceCursor) {
            throw std::invalid_argument("callback event outside bounded sequence");
        }
        const auto* message = state.sequence.data() + cursor;
        const auto form = (message[0] & 0x80) != 0 ? NoteParserForm::explicitStatus : NoteParserForm::runningStatus;
        const auto bytes = form == NoteParserForm::explicitStatus ? explicitNoteMessageBytes : runningNoteMessageBytes;
        if (state.sequence.size() - cursor < bytes || cursor > maximumSequenceCursor - bytes) {
            throw std::invalid_argument("callback event outside bounded sequence");
        }
        if (form == NoteParserForm::explicitStatus) {
            runningStatus = message[0];
            ++message;
        }
        if ((runningStatus & 0xf0) != explicitNoteOnStatus || message[2] > maximumSimpleDelta) {
            throw std::invalid_argument("unsupported callback event family or delta");
        }
        events.push_back({static_cast<std::uint8_t>(runningStatus & highestMidiChannel), message[0], message[1], message[2]});
        parserForms.push_back(form);
        cursor += bytes;
        if (message[2] != 0) {
            break;
        }
    }
    if (events.back().delay == 0) {
        throw std::invalid_argument("callback event group lacks terminal delta");
    }
    bool positive = false;
    for (std::size_t index = 0; index < events.size(); ++index) {
        const auto& event = events[index];
        if (event.note < lowestKeyOffNote || event.note > highestKeyOffNote
            || event.velocity > maximumSimpleDelta) {
            throw std::invalid_argument("callback note/velocity outside bounded family");
        }
        if (event.velocity == 0 && parserForms[index] != NoteParserForm::explicitStatus) {
            throw std::invalid_argument("running key-off parser outside bounded family");
        }
        positive = positive || event.velocity != 0;
    }
    if (positive) {
        for (const auto cursorValue : state.channelVoiceCursors) {
            if (cursorValue >= state.voices.channels.size()) {
                throw std::invalid_argument("positive-note cursor outside physical voice range");
            }
        }
        for (std::size_t voice = 0; voice < state.sounding.size(); ++voice) {
            if (state.sounding[voice] != 0 && (state.voices.notes[voice] < lowestKeyOffNote
                || state.voices.notes[voice] > highestKeyOffNote)) {
                throw std::invalid_argument("active callback note outside recovered frequency domain");
            }
        }
    }
    for (const auto timer : state.voices.directTimers) {
        if (timer == 1) {
            throw std::invalid_argument("direct expiry excluded from callback work");
        }
    }
    const auto timers = soundBlasterDirectTimerWork(state.voices.directTimers, state.gate, state.latch);
    SoundBlasterNoteOnWork work;
    CallbackWorkStream stream{work.dispatches, work.portReads, work.portWrites, work.blocks};
    stream.plain(callbackWrapperDispatches - callbackWrapperReturnDispatches
        + residentEntryDispatches - residentReturnDispatches + explicitEventEntryDispatches + timers.dispatches);
    result.state.voices.directTimers = timers.timers;
    for (std::size_t index = 0; index < events.size(); ++index) {
        const auto& event = events[index];
        if (event.velocity == 0) {
            const auto detail = keyOffEvent(stream, result.state.voices,
                {event.channel, event.note, event.delay}, result.state.gate, result.state.latch);
            if (detail.selectedVoice >= 0) {
                result.state.sounding[static_cast<std::size_t>(detail.selectedVoice)] = 0;
            }
            continue;
        }
        noteOnEvent(stream, result.state, event, parserForms[index], VoiceAllocationDomain::replaceActive);
    }
    stream.plain(callbackReturnDispatches + residentReturnDispatches + callbackWrapperReturnDispatches);
    result.kind = positive ? SoundBlasterCallbackKind::explicitNoteOn : SoundBlasterCallbackKind::explicitKeyOff;
    result.dispatches = work.dispatches;
    result.blocks = std::move(work.blocks);
    result.state.wait = events.back().delay - 1;
    result.state.position = cursor;
    result.state.runningStatus = static_cast<std::uint8_t>(explicitNoteOnStatus | events.back().channel);
    return result;
}

}  // namespace stellar7
