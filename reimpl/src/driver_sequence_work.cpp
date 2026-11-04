#include "stellar7/driver_sequence_work.hpp"
#include "stellar7/audio_program_work.hpp"

#include <stdexcept>

namespace stellar7 {
namespace {

constexpr std::size_t voiceCount = 9;
constexpr std::size_t directVoiceStart = 6;
constexpr std::size_t directSlotCount = 3;
constexpr std::size_t channelCount = 16;
constexpr std::uint16_t initialProgram = 13;
constexpr std::uint16_t directChannelStart = 13;
constexpr std::uint16_t initialDirectWord = 4;
constexpr std::uint16_t unsetByte = 0xff;
constexpr std::size_t resourceHeaderBytes = 2;
constexpr std::size_t sequenceHeaderBytes = 34;
constexpr std::uint8_t channelSelected = 4;

class SequenceBuilder {
public:
    DriverSequenceWork work;

    void plain(const std::uint64_t count) {
        work.operations.push_back({NormalWorkKind::instructions, count});
        dispatches_ += count;
    }

    void write(const DriverSequenceEffectKind kind, const std::uint16_t index,
        const std::uint16_t value, const std::uint16_t pc) {
        plain(1);
        work.effects.push_back({kind, index, value, pc, dispatches_});
        auto& state = work.state;
        switch (kind) {
        case DriverSequenceEffectKind::owner: state.voices[index].owner = value; break;
        case DriverSequenceEffectKind::channelVoiceCount: state.channels[index].voiceCount = value; break;
        case DriverSequenceEffectKind::channelProgram: state.channels[index].program = value; break;
        case DriverSequenceEffectKind::channelCursor: state.channels[index].cursor = value; break;
        case DriverSequenceEffectKind::channelDeferral: state.channels[index].deferral = value; break;
        case DriverSequenceEffectKind::channelEnabled: state.channels[index].enabled = value; break;
        case DriverSequenceEffectKind::voiceMarker: state.voices[index].marker = value; break;
        case DriverSequenceEffectKind::velocity: state.voices[index].velocity = value; break;
        case DriverSequenceEffectKind::note: state.voices[index].note = value; break;
        case DriverSequenceEffectKind::sounding: state.voices[index].sounding = value; break;
        case DriverSequenceEffectKind::tuning: state.voices[index].tuning = value; break;
        case DriverSequenceEffectKind::voiceAuxiliary: state.voices[index].auxiliary = value; break;
        case DriverSequenceEffectKind::voiceWord: state.voices[index / 3].words[index % 3] = value; break;
        case DriverSequenceEffectKind::directResetWord: state.directResetWords[index / 2][index % 2] = value; break;
        case DriverSequenceEffectKind::directMode: state.directMode = value; break;
        case DriverSequenceEffectKind::handleMode: state.handleMode = value; break;
        case DriverSequenceEffectKind::handleStatus: state.handleStatus = value; break;
        case DriverSequenceEffectKind::handlePosition: state.handlePosition = value; break;
        case DriverSequenceEffectKind::channelCount: state.channelCount = value; break;
        case DriverSequenceEffectKind::sequenceMode: state.sequenceMode = value; break;
        case DriverSequenceEffectKind::descriptorOffset: state.descriptorOffset = value; break;
        case DriverSequenceEffectKind::wait: state.wait = value; break;
        case DriverSequenceEffectKind::parseOperation: state.parseOperation = value; break;
        case DriverSequenceEffectKind::frequencyLatch: state.frequencyLatch = value; break;
        case DriverSequenceEffectKind::fade: state.fade = value; break;
        case DriverSequenceEffectKind::fadeDivider: state.fadeDivider = value; break;
        case DriverSequenceEffectKind::repeatPosition: state.repeatPosition = value; break;
        case DriverSequenceEffectKind::accumulator: state.accumulator = value; break;
        }
    }

    void directReset() {
        plain(4); //206B..206E saves AX/BX/CX/DX.
        for (std::uint16_t slot = 0; slot < directSlotCount; ++slot) {
            write(DriverSequenceEffectKind::owner, directVoiceStart + slot, directChannelStart + slot, 0x206f + slot * 6);
        }
        for (std::uint16_t slot = 0; slot < directSlotCount; ++slot) {
            write(DriverSequenceEffectKind::channelVoiceCount, directVoiceStart + slot, 0, 0x2081 + slot * 6);
        }
        for (std::uint16_t slot = 0; slot < directSlotCount; ++slot) {
            write(DriverSequenceEffectKind::channelProgram, directVoiceStart + slot, initialProgram, 0x2093 + slot * 6);
        }
        constexpr std::array<DriverSequenceEffectKind, 4> fields{
            DriverSequenceEffectKind::note, DriverSequenceEffectKind::sounding,
            DriverSequenceEffectKind::tuning, DriverSequenceEffectKind::voiceAuxiliary};
        constexpr std::array<std::uint16_t, 4> sites{0x20a5, 0x20b7, 0x20c9, 0x20db};
        for (std::size_t field = 0; field < fields.size(); ++field) {
            for (std::uint16_t slot = 0; slot < directSlotCount; ++slot) {
                write(fields[field], directVoiceStart + slot, 0, sites[field] + slot * 6);
            }
        }
        constexpr std::array<std::uint16_t, 3> wordSites{0x20ed, 0x2102, 0x2117};
        for (std::uint16_t field = 0; field < wordSites.size(); ++field) {
            for (std::uint16_t slot = 0; slot < directSlotCount; ++slot) {
                // Source order121,F5,14D maps to native record orderF5,121,14D.
                const auto word = field == 0 ? 1 : field == 1 ? 0 : 2;
                write(DriverSequenceEffectKind::voiceWord, (directVoiceStart + slot) * 3 + word,
                    0, wordSites[field] + slot * 7);
            }
        }
        for (std::uint16_t field = 0; field < 2; ++field) {
            for (std::uint16_t slot = 0; slot < directSlotCount; ++slot) {
                write(DriverSequenceEffectKind::directResetWord, slot * 2 + field,
                    initialDirectWord, (field == 0 ? 0x212c : 0x2141) + slot * 7);
            }
        }
        write(DriverSequenceEffectKind::directMode, 0, 1, 0x2156);
        plain(5); //215C..2160 restores registers and RET.
    }

private:
    std::uint64_t dispatches_ = 0;
};

}  // namespace

DriverSequenceWork soundBlasterDirectResetOperations() {
    SequenceBuilder builder;
    builder.directReset();
    return builder.work;
}

DriverSequenceWork soundBlasterSequenceOperations(const std::vector<std::uint8_t>& sequence,
    const DriverSequenceAdmission admission) {
    if (admission != DriverSequenceAdmission::freshChannels && admission != DriverSequenceAdmission::inheritedChannels
        && admission != DriverSequenceAdmission::successInheritedChannels) {
        throw std::invalid_argument("unsupported Sound Blaster channel admission");
    }
    if (sequence.size() < resourceHeaderBytes + sequenceHeaderBytes
        || sequence[0] != 0x84 || sequence[1] != 0 || sequence[resourceHeaderBytes] != 0) {
        throw std::invalid_argument("unsupported Sound Blaster activation sequence");
    }
    std::size_t assignedVoices = 0;
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
        const auto count = sequence[resourceHeaderBytes + 1 + channel * 2] & 0x7f;
        const auto flags = sequence[resourceHeaderBytes + 2 + channel * 2];
        if ((flags & channelSelected) == 0 || count == 0) {
            continue;
        }
        const auto assignmentLimit = admission == DriverSequenceAdmission::successInheritedChannels ? voiceCount + 2 : voiceCount;
        if (channel == 15 || assignedVoices + count > assignmentLimit
            || (admission == DriverSequenceAdmission::freshChannels && (channel == 6 || channel == 7))) {
            throw std::invalid_argument("unsupported preserved-channel activation descriptor");
        }
        assignedVoices += count;
    }

    SequenceBuilder b;
    b.plain(1); //17E5 CALL206B.
    b.directReset();
    b.plain(4); //17E8..17F2 resolve the typed loaded handle descriptor.
    b.write(DriverSequenceEffectKind::handleMode, 0, 1, 0x17f5);
    b.write(DriverSequenceEffectKind::channelCount, 0, channelCount, 0x17fa);
    b.write(DriverSequenceEffectKind::sequenceMode, 0, 0, 0x1800);
    b.plain(3); //1806/180A type0 branch;1821 PUSH CX.
    b.write(DriverSequenceEffectKind::handleStatus, 0, 0, 0x1822);
    b.write(DriverSequenceEffectKind::handlePosition, 0, sequenceHeaderBytes, 0x1827);
    b.plain(6); //182C..183A direct mode selects six voices.
    for (std::uint16_t voice = 0; voice < directVoiceStart; ++voice) {
        b.write(DriverSequenceEffectKind::owner, voice, unsetByte, 0x1840);
        b.write(DriverSequenceEffectKind::channelVoiceCount, voice, 0, 0x1846);
        b.write(DriverSequenceEffectKind::channelProgram, voice, initialProgram, 0x184c);
        b.write(DriverSequenceEffectKind::channelCursor, voice, 0, 0x1852);
        b.write(DriverSequenceEffectKind::channelDeferral, voice, 0, 0x1858);
        b.write(DriverSequenceEffectKind::voiceMarker, voice, unsetByte, 0x185e);
        b.write(DriverSequenceEffectKind::channelEnabled, voice, 0, 0x1864);
        b.write(DriverSequenceEffectKind::velocity, voice, 0, 0x186a);
        b.write(DriverSequenceEffectKind::note, voice, 0, 0x1870);
        b.write(DriverSequenceEffectKind::sounding, voice, 0, 0x1876);
        b.write(DriverSequenceEffectKind::tuning, voice, 0, 0x187c);
        b.write(DriverSequenceEffectKind::voiceAuxiliary, voice, 0, 0x1882);
        b.plain(1); //1888 SHL DI.
        b.write(DriverSequenceEffectKind::voiceWord, voice * 3 + 1, 0, 0x188a);
        b.write(DriverSequenceEffectKind::voiceWord, voice * 3, 0, 0x1891);
        b.write(DriverSequenceEffectKind::voiceWord, voice * 3 + 2, 0, 0x1898);
        b.plain(3); //189F/18A1/18A2.
    }
    b.plain(4); //18A4..18AF skips preserved channels6/7, starts channel8.
    for (std::uint16_t channel = 8; channel < 10; ++channel) {
        b.write(DriverSequenceEffectKind::channelVoiceCount, channel, 0, 0x18b2);
        b.write(DriverSequenceEffectKind::channelProgram, channel, initialProgram, 0x18b8);
        b.write(DriverSequenceEffectKind::channelCursor, channel, 0, 0x18be);
        b.write(DriverSequenceEffectKind::channelDeferral, channel, 0, 0x18c4);
        // These source record writes extend through record9, beyond physical OPL voices.
        b.write(DriverSequenceEffectKind::note, channel, 0, 0x18ca);
        b.write(DriverSequenceEffectKind::sounding, channel, 0, 0x18d0);
        b.write(DriverSequenceEffectKind::channelEnabled, channel, 0, 0x18d6);
        b.write(DriverSequenceEffectKind::tuning, channel, 0, 0x18dc);
        b.write(DriverSequenceEffectKind::voiceAuxiliary, channel, 0, 0x18e2);
        b.plain(1);
        b.write(DriverSequenceEffectKind::voiceWord, channel * 3 + 1, 0, 0x18ea);
        b.write(DriverSequenceEffectKind::voiceWord, channel * 3, 0, 0x18f1);
        b.write(DriverSequenceEffectKind::voiceWord, channel * 3 + 2, 0, 0x18f8);
        b.plain(3); //18FF/1901/1902.
    }
    b.plain(1); //1904 MOV CX,5.
    for (std::uint16_t channel = 10; channel < 15; ++channel) {
        b.write(DriverSequenceEffectKind::channelVoiceCount, channel, 0, 0x1907);
        b.write(DriverSequenceEffectKind::channelProgram, channel, initialProgram, 0x190d);
        b.write(DriverSequenceEffectKind::channelCursor, channel, 0, 0x1913);
        b.write(DriverSequenceEffectKind::channelDeferral, channel, 0, 0x1919);
        b.write(DriverSequenceEffectKind::channelEnabled, channel, 0, 0x191f);
        b.plain(2); //1925/1926.
    }
    b.write(DriverSequenceEffectKind::descriptorOffset, 0, 1, 0x1928);
    b.plain(2); //192D/192F.
    std::uint16_t voice = 0;
    for (std::uint16_t channel = 0; channel < channelCount; ++channel) {
        const auto count = sequence[resourceHeaderBytes + 1 + channel * 2] & 0x7f;
        const auto flags = sequence[resourceHeaderBytes + 2 + channel * 2];
        b.plain(6); //1931..193E descriptor reads and device-flag branch.
        if ((flags & channelSelected) != 0) {
            b.plain(2); //1940/1943 count branch.
            if (count != 0) {
                b.plain(1); //1945 XOR BL.
                for (std::uint16_t assigned = 0; assigned < count; ++assigned) {
                    if (voice < voiceCount) {
                        b.write(DriverSequenceEffectKind::owner, voice, channel, 0x1947);
                    } else {
                        //1947 DI9/10 writes3C/3D: the existing channel-count bytes.
                        b.write(DriverSequenceEffectKind::channelVoiceCount, voice-voiceCount, channel, 0x1947);
                    }
                    b.plain(4); //194C..1951.
                    b.write(DriverSequenceEffectKind::channelVoiceCount, channel, assigned + 1, 0x1953);
                    b.plain(5); //1958..195E.
                    ++voice;
                }
            }
        }
        b.plain(4); //1960..1968.
    }
    b.write(DriverSequenceEffectKind::wait, 0, 0, 0x196a);
    b.write(DriverSequenceEffectKind::parseOperation, 0, 2, 0x1971);
    b.write(DriverSequenceEffectKind::frequencyLatch, 0, 0, 0x1977);
    b.write(DriverSequenceEffectKind::fade, 0, 0, 0x197d);
    b.plain(3); //1984/1987/198A derives divider20.
    b.write(DriverSequenceEffectKind::fadeDivider, 0, (8 + 2) * 2, 0x198c);
    b.write(DriverSequenceEffectKind::repeatPosition, 0, sequenceHeaderBytes, 0x1991);
    b.write(DriverSequenceEffectKind::accumulator, 0, 127, 0x1998);
    b.plain(3); //199F/19A2/19A3.
    return b.work;
}

DriverSequenceWork driverSequenceEffectWork(const DriverSequenceEffect& effect) {
    DriverSequenceWork result;
    auto& state = result.state;
    std::size_t limit = 1;
    switch (effect.kind) {
    case DriverSequenceEffectKind::owner:
    case DriverSequenceEffectKind::voiceMarker:
    case DriverSequenceEffectKind::velocity:
    case DriverSequenceEffectKind::note:
    case DriverSequenceEffectKind::sounding:
    case DriverSequenceEffectKind::tuning:
    case DriverSequenceEffectKind::voiceAuxiliary: limit = state.voices.size(); break;
    case DriverSequenceEffectKind::channelVoiceCount:
    case DriverSequenceEffectKind::channelProgram:
    case DriverSequenceEffectKind::channelCursor:
    case DriverSequenceEffectKind::channelDeferral:
    case DriverSequenceEffectKind::channelEnabled: limit = state.channels.size(); break;
    case DriverSequenceEffectKind::voiceWord: limit = state.voices.size()*3; break;
    case DriverSequenceEffectKind::directResetWord: limit = state.directResetWords.size()*2; break;
    default: break;
    }
    if (effect.index >= limit) {
        throw std::invalid_argument("activation source store index exceeds typed storage");
    }
        switch (effect.kind) {
        case DriverSequenceEffectKind::owner: state.voices[effect.index].owner = effect.value; break;
        case DriverSequenceEffectKind::channelVoiceCount: state.channels[effect.index].voiceCount = effect.value; break;
        case DriverSequenceEffectKind::channelProgram: state.channels[effect.index].program = effect.value; break;
        case DriverSequenceEffectKind::channelCursor: state.channels[effect.index].cursor = effect.value; break;
        case DriverSequenceEffectKind::channelDeferral: state.channels[effect.index].deferral = effect.value; break;
        case DriverSequenceEffectKind::channelEnabled: state.channels[effect.index].enabled = effect.value; break;
        case DriverSequenceEffectKind::voiceMarker: state.voices[effect.index].marker = effect.value; break;
        case DriverSequenceEffectKind::velocity: state.voices[effect.index].velocity = effect.value; break;
        case DriverSequenceEffectKind::note: state.voices[effect.index].note = effect.value; break;
        case DriverSequenceEffectKind::sounding: state.voices[effect.index].sounding = effect.value; break;
        case DriverSequenceEffectKind::tuning: state.voices[effect.index].tuning = effect.value; break;
        case DriverSequenceEffectKind::voiceAuxiliary: state.voices[effect.index].auxiliary = effect.value; break;
        case DriverSequenceEffectKind::voiceWord: state.voices[effect.index / 3].words[effect.index % 3] = effect.value; break;
        case DriverSequenceEffectKind::directResetWord: state.directResetWords[effect.index / 2][effect.index % 2] = effect.value; break;
        case DriverSequenceEffectKind::directMode: state.directMode = effect.value; break;
        case DriverSequenceEffectKind::handleMode: state.handleMode = effect.value; break;
        case DriverSequenceEffectKind::handleStatus: state.handleStatus = effect.value; break;
        case DriverSequenceEffectKind::handlePosition: state.handlePosition = effect.value; break;
        case DriverSequenceEffectKind::channelCount: state.channelCount = effect.value; break;
        case DriverSequenceEffectKind::sequenceMode: state.sequenceMode = effect.value; break;
        case DriverSequenceEffectKind::descriptorOffset: state.descriptorOffset = effect.value; break;
        case DriverSequenceEffectKind::wait: state.wait = effect.value; break;
        case DriverSequenceEffectKind::parseOperation: state.parseOperation = effect.value; break;
        case DriverSequenceEffectKind::frequencyLatch: state.frequencyLatch = effect.value; break;
        case DriverSequenceEffectKind::fade: state.fade = effect.value; break;
        case DriverSequenceEffectKind::fadeDivider: state.fadeDivider = effect.value; break;
        case DriverSequenceEffectKind::repeatPosition: state.repeatPosition = effect.value; break;
        case DriverSequenceEffectKind::accumulator: state.accumulator = effect.value; break;
        }
    return result;
}

SoundBlasterCallbackState applyDriverSequenceEffect(const SoundBlasterCallbackState& incoming,
    const DriverSequenceEffect& effect) {
    auto write = driverSequenceEffectWork(effect);
    if (effect.kind == DriverSequenceEffectKind::frequencyLatch) {
        write.state.frequencyLatch.reset(); //1977 targetsA5;186A already cleared physical velocity0.
    }
    auto result = applyDriverSequenceCallbackState(incoming,write);
    if (!incoming.program) {
        throw std::invalid_argument("partial activation requires retained physical program storage");
    }
    result.program = soundBlasterProgramSequenceState(*incoming.program,write.state);
    //030C is a separate wrapper write; a driver partial store cannot repeat it.
    result.program->repeatCount = incoming.program->repeatCount;
    result.program->handleVolume = incoming.program->handleVolume;
    return result;
}

SoundBlasterCallbackState applyDriverSequenceCallbackState(
    const SoundBlasterCallbackState& incoming, const DriverSequenceWork& work) {
    auto selected = incoming;
    for (std::size_t voice = 0; voice < voiceCount; ++voice) {
        const auto& state = work.state.voices[voice];
        if (state.owner) { selected.voices.channels[voice] = static_cast<std::uint8_t>(*state.owner); }
        if (state.note) { selected.voices.notes[voice] = static_cast<std::uint8_t>(*state.note); }
        if (state.sounding) { selected.sounding[voice] = static_cast<std::uint8_t>(*state.sounding); }
        if (state.tuning) { selected.voices.tuning[voice] = static_cast<std::uint8_t>(*state.tuning); }
        if (state.velocity) { selected.velocities[voice] = static_cast<std::uint8_t>(*state.velocity); }
    }
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
        const auto& state = work.state.channels[channel];
        if (state.enabled) { selected.channelEnabled[channel] = static_cast<std::uint8_t>(*state.enabled); }
        if (state.deferral) { selected.voices.channelDeferrals[channel] = static_cast<std::uint8_t>(*state.deferral); }
        if (state.cursor) { selected.channelVoiceCursors[channel] = static_cast<std::uint8_t>(*state.cursor); }
    }
    if (work.state.frequencyLatch) {
        selected.velocities[0] = static_cast<std::uint8_t>(*work.state.frequencyLatch); // Source95 is velocity0.
    }
    if (work.state.fade) {
        selected.fade = *work.state.fade;
    }
    selected.latch = selected.velocities[0] == 0xff
        ? SoundBlasterLevelLatch::Latched : SoundBlasterLevelLatch::Unlatched;
    return selected;
}

SoundBlasterCallbackState soundBlasterSequenceCallbackState(
    const SoundBlasterCallbackState& noHandle, const std::vector<std::uint8_t>& sequence,
    const DriverSequenceWork& result) {
    if (noHandle.domain != SoundBlasterCallbackDomain::noHandle || noHandle.active
        || !noHandle.driverEnabled || !noHandle.sequence.empty()
        || sequence.size() < resourceHeaderBytes + sequenceHeaderBytes
        || sequence[0] != 0x84 || sequence[1] != 0 || sequence[resourceHeaderBytes] != 0
        || result.state.handlePosition != sequenceHeaderBytes || result.state.parseOperation != 2) {
        throw std::invalid_argument("unsupported prepared Sound Blaster activation state");
    }
    for (const auto timer : noHandle.voices.directTimers) {
        if (timer != 0) {
            throw std::invalid_argument("activation requires zero direct timers");
        }
    }
    auto selected = applyDriverSequenceCallbackState(noHandle, result);
    selected.domain = SoundBlasterCallbackDomain::sequence;
    selected.sequence = sequence;
    selected.position = resourceHeaderBytes + sequenceHeaderBytes;
    selected.wait = 0;
    selected.runningStatus = 0;
    selected.fade = 0;
    selected.mode = 0;
    selected.operation = 2;
    selected.active = true;
    selected.latch = SoundBlasterLevelLatch::Unlatched;
    return selected;
}

}  // namespace stellar7
