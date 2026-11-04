#include "stellar7/audio_repeat_work.hpp"
#include "stellar7/audio_program_work.hpp"
#include "stellar7/audio_program_tables.hpp"
#include "stellar7/audio_direct_work.hpp"
#include "retained_menu_clock_domain.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <utility>
#include <string>
#include <sstream>

namespace stellar7 {
namespace {

constexpr std::size_t physicalVoices = 9;
constexpr std::size_t instrumentCount = 96;
constexpr std::size_t instrumentBytes = 28;
constexpr std::size_t volumeRows = 16;
constexpr std::size_t volumeColumns = 64;
constexpr std::uint16_t volumeTableAddress = 0x0903;
constexpr std::uint16_t instrumentBankAddress = 0x0d28;
constexpr std::uint16_t fadeDividerReload = 20;
constexpr std::uint8_t simpleDeltaLimit = 127;
constexpr std::uint16_t bendCenter = 0x2000;
constexpr std::uint16_t bendStep = 0xab;
enum class ProgramEofAdmission { onceOnly, retainedLevel1, retainedLevel2, retainedMenu };
constexpr std::uint16_t sequencePayloadHeaderBytes = 34;
enum class NoteSound { off, on };
enum class FrequencyHandle { sequence, direct };
constexpr std::uint16_t directHandleVolume = 15; // Immutable driverD26.
constexpr std::uint8_t lowestSequenceNote = 12;
constexpr std::uint8_t highestSequenceNote = 107;
constexpr int highestFrequencyIndex = 508;
constexpr std::array<std::uint8_t, 18> operatorOffsets{
    0, 1, 2, 3, 4, 5, 8, 9, 10, 11, 12, 13, 16, 17, 18, 19, 20, 21};
constexpr std::array<std::uint8_t, 18> voiceOperators{
    0, 3, 1, 4, 2, 5, 6, 9, 7, 10, 8, 11, 12, 15, 13, 16, 14, 17};

class ProgramBuilder {
public:
    explicit ProgramBuilder(const SoundBlasterCallbackState& incoming,
        const std::optional<std::uint16_t> callerVolume = std::nullopt,
        const ProgramEofAdmission admission = ProgramEofAdmission::onceOnly)
        : result{incoming, SoundBlasterCallbackKind::programControl}, callerVolume_(callerVolume),
          eofAdmission_(admission) {}

    SoundBlasterCallbackAdvance result;
    int diagnosticEvent = -1;
    int diagnosticProgram = -1;

    void synchronizeLatch() {
        result.state.latch = result.state.velocities[0] == 0xff
            ? SoundBlasterLevelLatch::Latched : SoundBlasterLevelLatch::Unlatched;
    }

    void plain(const std::uint64_t count) {
        result.dispatches += count;
        if (!result.blocks.empty() && result.blocks.back().kind == SoundBlasterWorkKind::plain) {
            result.blocks.back().dispatches += count;
        } else {
            result.blocks.push_back({SoundBlasterWorkKind::plain, count, -1});
        }
    }

    void write(const std::uint8_t reg, const std::uint8_t value) {
        const auto count = soundBlasterOplWork().dispatches;
        result.blocks.push_back({SoundBlasterWorkKind::registerWrite, count, -1, reg, value});
        result.dispatches += count;
    }

    void operatorRegisters(const std::uint8_t operatorIndex, const std::uint8_t* patch,
        const std::uint8_t wave, const std::uint8_t voice, const bool carrier) {
        auto& program = *result.state.program;
        const auto offset = operatorOffsets[operatorIndex];
        //23E2 dispatches eight source helper calls in this order.
        plain(1);
        plain(9 + (program.amplitudeDepth != 0) + (program.vibratoDepth != 0)); //25A3..25C0
        write(0xbd, static_cast<std::uint8_t>((program.amplitudeDepth ? 0x80 : 0)
            | (program.vibratoDepth ? 0x40 : 0)));
        plain(3);
        plain(1);
        plain(7 + (program.noteSelect != 0)); //2443..2454
        write(8, program.noteSelect ? 0x40 : 0);
        plain(3);
        plain(1);
        plain(33); //23FB..243B
        write(static_cast<std::uint8_t>(0x40 + offset),
            static_cast<std::uint8_t>(((patch[0] & 3) << 6) | (patch[8] & 63)));
        plain(5);
        plain(1);
        plain(6); //245A..2465 carrier test.
        if (!carrier) {
            plain(27 + (patch[12] == 0)); //2467..24A3
            write(static_cast<std::uint8_t>(0xc0 + voice),
                static_cast<std::uint8_t>(((patch[2] << 1) | (patch[12] == 0)) & 15));
        }
        plain(4);
        plain(1);
        plain(32); //24AA..24E9
        write(static_cast<std::uint8_t>(0x60 + offset),
            static_cast<std::uint8_t>((patch[3] << 4) | (patch[6] & 15)));
        plain(4);
        plain(1);
        plain(32); //24F0..252F
        write(static_cast<std::uint8_t>(0x80 + offset),
            static_cast<std::uint8_t>((patch[4] << 4) | (patch[7] & 15)));
        plain(4);
        plain(1);
        plain(41 + (patch[9] != 0) + (patch[10] != 0) + (patch[5] != 0) + (patch[11] != 0));
        write(static_cast<std::uint8_t>(0x20 + offset), static_cast<std::uint8_t>(
            (patch[9] ? 0x80 : 0) | (patch[10] ? 0x40 : 0) | (patch[5] ? 0x20 : 0)
            | (patch[11] ? 0x10 : 0) | (patch[1] & 15)));
        plain(4);
        plain(1);
        plain(28); //25C6..25FE, admitted waveform select enabled.
        write(static_cast<std::uint8_t>(0xe0 + offset), wave & 3);
        plain(4);
        plain(1); //23FA RET.
    }

    void patchVoice(const std::uint8_t voice, const std::uint8_t programNumber) {
        if (programNumber >= instrumentCount) {
            throw std::invalid_argument("unsupported sequence program family");
        }
        auto& program = *result.state.program;
        auto& level = program.voices[voice];
        const auto* patch = program.instrumentBank.data() + programNumber * instrumentBytes;
        if (patch[0] > 3 || patch[13] > 3 || patch[8] > 63 || patch[21] > 63) {
            throw std::invalid_argument("unsupported program patch level fields");
        }
        //2298..22AB, then the actual patch12 branch.
        plain(13);
        if (patch[12] == 0) {
            plain(8); //22B6..22CA preserves the prior operatorDisabled word.
            level.operatorScale = patch[0];
            level.operatorFallback = 63 - patch[8];
        } else {
            plain(2); //22AD/22B4.
            level.operatorDisabled = 0xffff;
        }
        level.carrierScale = patch[13];
        level.carrierFallback = 63 - patch[21];
        //22CF..230A includes first2373 CALL;22CF..2318 totals30 instructions.
        plain(24); //22CF..230A.
        for (std::uint8_t part = 0; part < 2; ++part) {
            if (part != 0) {
                plain(6); //230D..2318, including second2373 CALL.
            }
            const auto operatorIndex = voiceOperators[voice * 2 + part];
            auto& record = program.operators[operatorIndex];
            const auto* op = patch + part * 13;
            plain(22); //2373..2392 copy setup.
            for (std::size_t byte = 0; byte < 13; ++byte) {
                plain(10); //2394..23A6.
                record[byte] = op[byte];
            }
            plain(5); //23A8..23B2 includes23E2 CALL.
            record[13] = patch[26 + part] & 3;
            operatorRegisters(operatorIndex, op, record[13], voice, part != 0);
            plain(6); //23B5..23BA.
        }
        plain(7); //231B..2321 RET.
    }

    std::uint8_t attenuation(const std::uint16_t index) const {
        const auto& state = result.state;
        const auto& program = *result.state.program;
        const auto volume = state.fade != 0 ? state.fade : callerVolume_.value_or(program.handleVolume);
        const auto address = static_cast<std::uint16_t>(volumeTableAddress + (volume << 6) + index);
        std::uint8_t scaled;
        if (address >= volumeTableAddress && address - volumeTableAddress < program.volumeTable.size()) {
            scaled = program.volumeTable[address - volumeTableAddress];
        } else if (address >= instrumentBankAddress && address - instrumentBankAddress < program.instrumentBank.size()) {
            scaled = program.instrumentBank[address - instrumentBankAddress];
        } else if (callerVolume_ == gameplayPlayerHitCallerVolume && index < 64
            && program.playerHitVolumeRow && program.playerHitVolumeRow->volume == gameplayPlayerHitCallerVolume) {
            scaled = program.playerHitVolumeRow->values[index];
        } else if (callerVolume_ == gameplayEnemyType20CallerVolume && program.enemy20CallerWindow
            && index >= enemy20CallerIndexBegin
            && index - enemy20CallerIndexBegin < program.enemy20CallerWindow->payloadPrefix.size()) {
            scaled = program.enemy20CallerWindow->payloadPrefix[index - enemy20CallerIndexBegin];
        } else if (callerVolume_ == gameplayEnemyType20CallerVolume && index == enemy20McbIndex
            && address == enemy20McbAddress && program.enemy20McbByte) {
            scaled = program.enemy20McbByte->value();
        } else if (callerVolume_ == gameplayEnemyType20CallerVolume && state.fade == 0
            && program.enemy20McbByte && program.enemy20McbByte->nativeProcessByte(index)) {
            scaled = *program.enemy20McbByte->nativeProcessByte(index);
        } else if (callerVolume_ == gameplayBombCallerVolume && state.fade == 0
            && index < bomb7CallerRowBytes && address == bomb7CallerRowAddress + index && program.bomb7CallerRow) {
            scaled = program.bomb7CallerRow->values()[index];
        } else {
            std::ostringstream detail;
            detail << "program level lookup exceeds owned driver storage"
                << " event=" << diagnosticEvent << " program=" << diagnosticProgram
                << " callerVolume=" << (callerVolume_ ? std::to_string(*callerVolume_) : "none")
                << " fade=" << state.fade << " index=" << index
                << " address=0x" << std::hex << address;
            throw std::invalid_argument(detail.str());
        }
        return static_cast<std::uint8_t>(63 - scaled);
    }

    std::uint8_t scaledLevel(const std::uint16_t index) {
        plain(result.state.fade != 0 ? 17 : 19); //2C7D..2CAF, fade or handle volume.
        return attenuation(index);
    }

    void operatorLevel(const std::uint8_t voice) {
        const auto& level = result.state.program->voices[voice];
        const auto velocity = result.state.velocities[voice];
        plain(11); //2BCB..2BE1, including scale and velocity test.
        plain(velocity != 0 ? 5 : 2); //2BE3..2BEE or2BF0..2BF2.
        plain(result.state.fade != 0 ? 16 : 18); //2BF7..2C28 inline volume lookup.
        const auto value = attenuation(velocity != 0 ? velocity >> 1 : level.operatorFallback);
        plain(10); //2C29..2C40, including CALL26E5.
        const auto offset = operatorOffsets[voiceOperators[voice * 2]];
        write(static_cast<std::uint8_t>(0x40 + offset),
            static_cast<std::uint8_t>((level.operatorScale << 6) | value));
        plain(4); //2C43..2C46.
    }

    void carrierVolume(const std::uint8_t voice) {
        const auto& level = result.state.program->voices[voice];
        plain(5); //2C47..2C53 includes CALL2C71.
        plain(6); //2C71..2C7C.
        const auto velocity = result.state.velocities[voice];
        plain(velocity != 0 ? 6 : 3); //2C55..2C60 or2C62..2C69, including CALL2C7D.
        const auto value = scaledLevel(velocity != 0 ? velocity >> 1 : level.carrierFallback);
        plain(2); //2C6C/2C6D CALL2CB0.
        plain(11); //2CB0..2CC9.
        const auto carrier = operatorOffsets[voiceOperators[voice * 2 + 1]];
        write(static_cast<std::uint8_t>(0x40 + carrier),
            static_cast<std::uint8_t>((level.carrierScale << 6) | value));
        plain(3); //2CCC/2CCD, then2C70 RET.
    }

    void carrierLevel(const std::uint8_t voice) {
        plain(12); //2B9A..2BB7, one selected voice.
        if (result.state.program->voices[voice].operatorDisabled != 0xffff) {
            plain(1); //2BB9 CALL2BCB.
            operatorLevel(voice);
        }
        plain(1); //2BBC CALL2C47.
        carrierVolume(voice);
        plain(10); //2BBF..2BCA.
    }

    void fade() {
        auto& state = result.state;
        auto& program = *state.program;
        plain(5); //2CFE..2D0B.
        --program.fadeDivider;
        if (program.fadeDivider != 0) {
            plain(3); //2D3C..2D3E.
            return;
        }
        plain(3); //2D0D..2D18.
        --state.fade;
        if (state.fade == 0) {
            plain(1 + 4); //2D1A CALL2D83,2D83..2D91 through CALL2DA0.
            keyOffAllVoices();
            plain(4); //2D94..2D9F: STI, selected handle/status, RET.
            program.handleStatus = 0xffff;
            plain(3); //2D1D..2D27.
            program.repeatCount = 0;
            state.active = false;
            plain(3); //2D3C..2D3E.
            return;
        }
        plain(2 + 10); //2D29/2D2C and2B9A..2BA9 all-voice selection.
        for (std::uint8_t voice = 0; voice < physicalVoices; ++voice) {
            plain(4); //2BAE..2BB7.
            if (program.voices[voice].operatorDisabled != 0xffff) {
                plain(1); //2BB9 CALL2BCB.
                operatorLevel(voice);
            }
            plain(1); //2BBC CALL2C47.
            carrierVolume(voice);
            plain(4); //2BBF..2BC3.
        }
        plain(6 + 4 + 3); //2BC5..2BCA,2D2F..2D37,2D3C..2D3E.
        program.fadeDivider = fadeDividerReload;
    }

    void frequency(const std::uint8_t voice, const std::uint8_t note, const NoteSound sound,
        const FrequencyHandle handle = FrequencyHandle::sequence) {
        auto& state = result.state;
        const auto& program = *state.program;
        plain(12); //2605..2626, BP8 handle-volume gate.
        state.sounding[voice] = sound == NoteSound::on ? 1 : 0;
        state.voices.notes[voice] = note;
        const auto handleVolume = handle == FrequencyHandle::direct ? directHandleVolume : program.handleVolume;
        if (handleVolume == 0) {
            plain(3); //2628/2629/262A: logical state still changes without frequency output.
            return;
        }
        plain(10); //262B..263D.
        int index = note * 4;
        const auto tuning = state.voices.tuning[voice];
        if (tuning != 0) {
            plain(1 + 5); //263F CALL2B6F,2B6F..2B7D.
            const auto magnitude = program.voices[voice].tuningMagnitude;
            if (tuning == 1) {
                plain(2); //2B7F/2B81.
                index += magnitude;
            } else {
                plain(1); //2B83.
                index -= magnitude;
            }
            plain(2); //2B85/2B88 signed lower bound.
            if (index < 0) {
                plain(2); //2B8A/2B8D.
                index = -1;
            } else {
                plain(2); //2B8F/2B93.
                if (index > highestFrequencyIndex) {
                    plain(1); //2B95.
                    index = -1;
                }
            }
            plain(2); //2B98/2B99.
        }
        plain(2); //2642/2645.
        if (index < 0) {
            plain(1 + 7); //2647,26C7..26CD.
            return;
        }
        const auto frequency = soundBlasterProgramFrequency(static_cast<std::size_t>(index));
        const auto block = soundBlasterProgramBlock(static_cast<std::size_t>(index));
        plain(6); //264A..2659 CALL26E5.
        write(static_cast<std::uint8_t>(0xa0 + voice), static_cast<std::uint8_t>(frequency));
        plain(22); //265C..268A CALL26E5.
        write(static_cast<std::uint8_t>(0xb0 + voice), static_cast<std::uint8_t>(
            (sound == NoteSound::on ? 0x20 : 0) | (block << 2) | (frequency >> 8)));
        plain(2); //268D/2693.
        if (state.velocities[0] == 0xff) {
            plain(16); //2698..26C1 CALL26E5.
            const auto carrier = operatorOffsets[voiceOperators[voice * 2 + 1]];
            write(static_cast<std::uint8_t>(0x40 + carrier),
                static_cast<std::uint8_t>(63 - state.velocities[0]));
            plain(3); //26C4..26C6.
        } else {
            plain(1); //2695.
        }
        plain(7); //26C7..26CD.
        state.latch = state.velocities[0] == 0xff
            ? SoundBlasterLevelLatch::Latched : SoundBlasterLevelLatch::Unlatched;
    }

    void noteFrequency(const std::uint8_t voice, const std::uint8_t note, const NoteSound sound,
        const FrequencyHandle handle = FrequencyHandle::sequence) {
        plain(sound == NoteSound::on ? 6 : 8); //2322..2329 or2331..233E CALL2605.
        frequency(voice, note, sound, handle);
        plain(5); //232C..2330 or2341..2345.
    }

    void timers() {
        plain(2); //1C6B/1C6C.
        for (std::size_t slot = 0; slot < result.state.voices.directTimers.size(); ++slot) {
            plain(3); //MOV/OR/JZ at1C6D,1C82,1C97.
            auto& timer = result.state.voices.directTimers[slot];
            if (timer == 0) {
                continue;
            }
            plain(3); //DEC/store/JNZ.
            --timer;
            if (timer == 0) {
                plain(2); //MOV BX,voice and CALL2331.
                const auto voice = static_cast<std::uint8_t>(slot + 6);
                noteFrequency(voice, result.state.voices.notes[voice], NoteSound::off);
            }
        }
        plain(3); //1CAC..1CAE.
    }

    void keyOffAllVoices() {
        plain(5); //2DA0/2DA6: OPL mode;2DCC/2DCD/2DCE.
        for (std::uint8_t voice = 0; voice < physicalVoices; ++voice) {
            plain(2); //2DD0 MOV BX,DI and2DD2 CALL2331.
            noteFrequency(voice, result.state.voices.notes[voice], NoteSound::off);
            plain(3); //2DD5/2DD6/2DD9.
        }
        plain(2 + 3); //2DDB/2DDE,2E02..2E04.
    }

    void finishSequence() {
        auto& state = result.state;
        if (state.program->repeatCount != 1
            && (eofAdmission_ != ProgramEofAdmission::retainedLevel1 && eofAdmission_ != ProgramEofAdmission::retainedLevel2
                    && eofAdmission_ != ProgramEofAdmission::retainedMenu
                || state.program->repeatCount != 0xffff
                || (state.fade != 0 && eofAdmission_ != ProgramEofAdmission::retainedMenu)
                || state.program->loopMarker < sequencePayloadHeaderBytes
                || static_cast<std::size_t>(state.program->loopMarker) + 2 >= state.sequence.size())) {
            throw std::invalid_argument("repeating program EOF requires recovered restart work");
        }
        plain(7); //1D08..1D26, including CALL2DA0.
        state.program->handleStatus = 0xffff;
        state.position = state.program->loopMarker + 2;
        state.operation = 2;
        state.wait = 0;
        keyOffAllVoices();
        plain(1); //1D29 RET.
        state.active = false;
    }

    void noteLevel(const std::uint8_t channel, const std::uint8_t voice,
        const std::uint8_t velocity) {
        auto& state = result.state;
        plain(4); //1E61..1E6B.
        if (state.channelEnabled[channel] == 0) {
            plain(2); //1E89/1E8F.
            state.velocities[voice] = 0;
            synchronizeLatch();
            return;
        }
        plain(8); //1E6D..1E7A CALL2C7D.
        state.velocities[voice] = velocity;
        synchronizeLatch();
        const auto value = scaledLevel(velocity >> 1);
        plain(1 + 6); //1E7D CALL2C71, six-instruction scale helper.
        plain(1 + 11); //1E80 CALL2CB0,2CB0..2CC9.
        const auto carrier = operatorOffsets[voiceOperators[voice * 2 + 1]];
        write(static_cast<std::uint8_t>(0x40 + carrier), static_cast<std::uint8_t>(
            (state.program->voices[voice].carrierScale << 6) | value));
        plain(2 + 6); //2CCC/2CCD,1E83..1E8F.
    }

    void noteOff(const std::uint8_t channel, const std::uint8_t note) {
        auto& state = result.state;
        plain(4); //1E90..1E94.
        for (std::uint8_t voice = 0; voice < physicalVoices; ++voice) {
            plain(2); //1E99/1E9E.
            if (state.voices.channels[voice] == channel) {
                plain(2); //1EA0/1EA5.
                if (state.voices.notes[voice] == note) {
                    plain(5); //1EA7..1EB4.
                    if (state.voices.channelDeferrals[channel] == 1) {
                        plain(2); //1EB6/1EBB.
                        state.program->voices[voice].program = note;
                    } else {
                        plain(2); //1EBD/1EC3 CALL2331.
                        state.program->voices[voice].program = 0xff;
                        noteFrequency(voice, state.voices.notes[voice], NoteSound::off);
                    }
                    plain(2 + 3); //1EC6/1EC7,1ED0..1ED2.
                    return;
                }
            }
            plain(3); //1ECA..1ECE.
        }
        plain(3); //1ED0..1ED2.
    }

    void noteOn(const std::uint8_t channel, const std::uint8_t note,
        const std::uint8_t velocity) {
        auto& state = result.state;
        plain(8); //1DAC..1DBA.
        if (note < lowestSequenceNote) {
            plain(1 + 5); //1DBC,1E5C..1E60.
            return;
        }
        plain(2); //1DBF/1DC2.
        if (note > highestSequenceNote) {
            plain(1 + 5); //1DC4,1E5C..1E60.
            return;
        }
        plain(4); //1DC7..1DCF.
        if (velocity == 0) {
            plain(1); //1DD1 CALL1E90.
            noteOff(channel, note);
            plain(1 + 5); //1DD4,1E5C..1E60.
            return;
        }
        plain(3); //1DD7..1DDB.
        std::uint8_t selected = physicalVoices;
        bool replacement = false;
        for (std::uint8_t voice = 0; voice < physicalVoices; ++voice) {
            plain(2); //1DDF/1DE4.
            if (state.voices.channels[voice] == channel) {
                plain(2); //1DE6/1DEC.
                if (state.sounding[voice] == 0) {
                    selected = voice;
                    break;
                }
            }
            plain(3); //1DFD..1E01.
        }
        auto cursor = state.channelVoiceCursors[channel];
        if (cursor >= physicalVoices) {
            throw std::invalid_argument("invalid program voice cursor");
        }
        if (selected == physicalVoices) {
            plain(2); //1E03/1E05.
            for (; cursor < physicalVoices; ++cursor) {
                plain(2); //1E0A/1E0F.
                if (state.voices.channels[cursor] == channel) {
                    selected = cursor;
                    replacement = true;
                    break;
                }
                plain(3); //1E23..1E28.
            }
            if (selected == physicalVoices) {
                plain(1); //1E2A.
                cursor = 0;
                do {
                    plain(2); //1E2C/1E31.
                    if (state.voices.channels[cursor] == channel) {
                        selected = cursor;
                        replacement = true;
                        break;
                    }
                    plain(3); //1E45..1E4C.
                    ++cursor;
                } while (cursor < state.channelVoiceCursors[channel]);
            }
        }
        if (selected != physicalVoices) {
            if (replacement) {
                plain(2); //1E11/1E17 or1E33/1E39.
                state.program->voices[selected].program = 0xff;
                noteFrequency(selected, state.voices.notes[selected], NoteSound::off);
            }
            plain(1); //1DEE/1E1A/1E3C CALL1E61.
            noteLevel(channel, selected, velocity);
            if (!replacement) {
                plain(1); //1DF1.
                state.program->voices[selected].program = 0xff;
            }
            plain(1); //1DF7/1E1D/1E3F CALL2322.
            noteFrequency(selected, note, NoteSound::on);
            plain(1); //1DFA/1E20/1E42.
            cursor = selected;
        }
        plain(3); //1E4E..1E53.
        ++cursor;
        if (cursor >= physicalVoices) {
            plain(1); //1E55.
            cursor = 0;
        }
        plain(1 + 5); //1E57,1E5C..1E60.
        state.channelVoiceCursors[channel] = cursor;
    }

    void changeProgram(const std::uint8_t channel, const std::uint8_t number) {
        auto& program = *result.state.program;
        //1D97 computes an offset;2298 reads the bank only for an owned voice.
        plain(5); //1D57..1D60 CALL1D97.
        plain(13); //1D97..1DAB bank offset28*program.
        plain(12); //1D63..1D7B, channel program store.
        program.channelPrograms[channel] = number;
        for (std::uint8_t voice = 0; voice < physicalVoices; ++voice) {
            plain(2); //1D7D/1D82.
            if (result.state.voices.channels[voice] == channel) {
                plain(1); //1D84 CALL2298.
                patchVoice(voice, number);
                plain(2); //1D87 PUSH CX,1D88 CALL2B9A.
                carrierLevel(voice);
                plain(1); //1D8B POP CX.
            }
            plain(3); //1D8C..1D90.
        }
        plain(5); //1D92..1D96.
    }

    void sustainChannel(const std::uint8_t channel, const std::uint8_t value) {
        auto& state = result.state;
        plain(7); //2001..200F prologue, channel/count setup and value test.
        if (value != 0) {
            plain(1); //2037: source normalizes every nonzero value to1.
            state.voices.channelDeferrals[channel] = 1;
            plain(3); //203D..203F.
            return;
        }
        plain(2); //2011/2017.
        state.voices.channelDeferrals[channel] = 0;
        for (std::uint8_t voice = 0; voice < physicalVoices; ++voice) {
            plain(2); //2019/201E owner check.
            if (state.voices.channels[voice] == channel) {
                plain(2); //2020/2026 deferred-note marker check.
                if (state.program->voices[voice].program != 0xff) {
                    plain(2); //2028 marker clear and202E CALL2331.
                    state.program->voices[voice].program = 0xff;
                    noteFrequency(voice, state.voices.notes[voice], NoteSound::off);
                }
            }
            plain(2); //2031/2033, always all9 physical slots in order.
        }
        plain(1 + 3); //2035/203D..203F.
    }

    void enableChannel(const std::uint8_t channel, const std::uint8_t value) {
        plain(8); //1CE1..1CF2.
        result.state.channelEnabled[channel] = value;
        if (value == 0) {
            plain(3); //1CF4..1CF8.
            for (auto& velocity : result.state.velocities) {
                plain(3); //1CFA/1D00/1D02.
                velocity = 0;
            }
            plain(1); //1D04.
            synchronizeLatch();
        }
        plain(3); //1D05..1D07.
    }

    void pitchBend(const std::uint8_t channel, const std::uint8_t low, const std::uint8_t high) {
        auto& program = *result.state.program;
        const auto bend = static_cast<std::uint16_t>((high << 7) | low);
        plain(9); //1ED3..1EE4.
        for (std::size_t voice = 0; voice < physicalVoices; ++voice) {
            plain(2); //1EE6/1EEB.
            if (result.state.voices.channels[voice] == channel) {
                plain(3); //1EED..1EF2.
                std::uint8_t magnitude = 0;
                if (bend == bendCenter) {
                    plain(3); //1EF4..1EFC.
                    result.state.voices.tuning[voice] = 0;
                } else {
                    plain(2); //1EFE/1F02.
                    if (bend > bendCenter) {
                        plain(4); //1F04..1F0F.
                        result.state.voices.tuning[voice] = 1;
                        magnitude = static_cast<std::uint8_t>((bend - bendCenter) / bendStep);
                    } else {
                        plain(3); //1F11..1F16.
                        result.state.voices.tuning[voice] = 0xff;
                        magnitude = static_cast<std::uint8_t>((bendCenter - bend) / bendStep);
                    }
                    plain(3); //1F1C..1F21 DIV.
                }
                program.voices[voice].tuningMagnitude = magnitude;
                plain(3); //1F23/1F28/1F2E.
                if (result.state.sounding[voice] != 0) {
                    plain(6); //1F30..1F3D CALL2605.
                    frequency(static_cast<std::uint8_t>(voice), result.state.voices.notes[voice], NoteSound::on);
                    plain(1); //1F40.
                }
                plain(1); //1F41.
            }
            plain(3); //1F42..1F46.
        }
        plain(5); //1F48..1F4C.
    }
private:
    std::optional<std::uint16_t> callerVolume_;
    ProgramEofAdmission eofAdmission_;
};

//0494's ended-once return leaves independently owned direct effects sounding.
bool endedProgramVoicesValid(const SoundBlasterCallbackState& state) {
    constexpr std::size_t firstDirectVoice=6;
    for(std::size_t voice=0;voice<state.sounding.size();++voice) {
        if(state.sounding[voice]==0) { continue; }
        if(state.program->repeatCount!=1 || voice<firstDirectVoice) { return false; }
        const auto slot=voice-firstDirectVoice;
        if(slot>=state.voices.directTimers.size() || state.voices.directTimers[slot]==0) {
            return false;
        }
    }
    return true;
}

enum class ProgramStorageDomain { selected, successNoHandle, level2RetryNoHandle };
void requireDomain(const SoundBlasterCallbackState& state,
    const ProgramStorageDomain domain = ProgramStorageDomain::selected) {
    const bool noHandle = domain != ProgramStorageDomain::selected;
    const auto callbackDomain = domain == ProgramStorageDomain::successNoHandle ? SoundBlasterCallbackDomain::successNoHandle
        : domain == ProgramStorageDomain::level2RetryNoHandle ? SoundBlasterCallbackDomain::level2RetryNoHandle
        : SoundBlasterCallbackDomain::sequence;
    if (!state.program || state.domain != callbackDomain
        || !state.driverEnabled || state.mode != 0 || state.operation != (noHandle ? 0x1e : 2)
        || state.fade >= volumeRows || state.gate != SoundBlasterFrequencyGate::Open
        || state.latch != (state.velocities[0] == 0xff
            ? SoundBlasterLevelLatch::Latched : SoundBlasterLevelLatch::Unlatched)) {
        throw std::invalid_argument("unsupported program callback domain");
    }
    const auto& program = *state.program;
    if (program.instrumentBank.size() != instrumentCount * instrumentBytes
        || program.volumeTable.size() != volumeRows * volumeColumns
        || (program.repeatCount != 0xffff && program.repeatCount != 1
            && !(program.repeatCount == 0 && program.handleStatus == 0xffff && !state.active))
        || program.handleVolume >= volumeRows || program.waveformMask == 0
        || program.noteSelect != 0 || program.amplitudeDepth != 0 || program.vibratoDepth != 0
        || std::any_of(state.sounding.begin(), state.sounding.end(), [](const auto value) { return value > 1; })
        || std::any_of(state.velocities.begin() + 1, state.velocities.end(), [](const auto value) { return value > 127; })
        || (state.velocities[0] > 127 && state.velocities[0] != 0xff)) {
        throw std::invalid_argument("unsupported program callback storage");
    }
    const bool ended = program.handleStatus == 0xffff && !state.active
        && (program.repeatCount == 0 || program.repeatCount == 1 || program.repeatCount == 0xffff);
    if (!noHandle && ((!ended && (program.handleStatus != 0 || !state.active))
        || (ended && ((program.repeatCount == 1 && state.wait != 0)
            || (program.repeatCount != 0xffff && !endedProgramVoicesValid(state)))))) {
        throw std::invalid_argument("unsupported selected program lifecycle");
    }
    for (const auto value : program.volumeTable) {
        if (value > 63) { throw std::invalid_argument("invalid program volume table"); }
    }
    for (std::size_t voice = 0; voice < physicalVoices; ++voice) {
        const auto& level = program.voices[voice];
        const auto tuning = state.voices.tuning[voice];
        if (level.carrierScale > 3 || level.operatorScale > 3
            || level.carrierFallback > 63 || level.operatorFallback > 63
            || (tuning != 0 && tuning != 1 && tuning != 0xff)) {
            throw std::invalid_argument("unsupported program voice storage");
        }
    }
}

}  // namespace

static SoundBlasterCallbackAdvance programCallbackAdvance(const SoundBlasterCallbackState& state,
    const ProgramEofAdmission admission) {
    requireDomain(state);
    ProgramBuilder b(state, std::nullopt, admission);
    if (!state.active && state.program->repeatCount == 1) {
        b.plain(17 + 7); //0494..04B1 selected ended/once path,04CA..04D0 return.
        b.result.kind = SoundBlasterCallbackKind::programWait;
        return b.result;
    }
    const bool resume = !state.active;
    if (resume) {
        //0494 resumes terminal0 or stoppedFFFF, retaining cursor/wait.
        b.result.state.program->repeatCount = 0xffff;
        b.result.state.program->handleStatus = 0;
        b.result.state.active = true;
    }
    b.plain(16 + (resume ? (state.program->repeatCount == 0 ? 8 : 6) : 0) + 12 + 5);
    b.timers();
    b.plain(4); //1A47/1A4D/1A8B/1A91.
    if (state.wait != 0) {
        b.plain(5); //1A93/1A96/1AA1/1AA6/1AAC.
        --b.result.state.wait;
        if (b.result.state.fade != 0) {
            b.plain(1); //1AAE CALL2CFE.
            b.fade();
        }
        b.plain(1 + 9 + 7); //1AB1,dispatcher/wrapper return.
        b.result.kind = SoundBlasterCallbackKind::programWait;
        return b.result;
    }
    b.plain(6); //1AB2..1AC2 including CLI and parser operation test.
    auto cursor = state.position;
    auto status = state.runningStatus;
    for (std::size_t event = 0; event < soundBlasterProgramEventLimit; ++event) {
        if (cursor >= state.sequence.size()) { throw std::invalid_argument("truncated program callback"); }
        const bool explicitStatus = (state.sequence[cursor] & 0x80) != 0;
        if (explicitStatus) { status = state.sequence[cursor++]; }
        if (explicitStatus && status == 0xfc) {
            //1B95's unused lookahead is not a modeled input or invented padding.
            b.plain(19 + 9); //1B40..1B9D, C/B/F dispatch and1C06 CALL1D08.
            b.finishSequence();
            b.result.state.runningStatus = status;
            b.plain(1 + 3 + 9 + 7); //1C09,1C68..1C6A,dispatcher/wrapper return.
            return b.result;
        }
        const auto family = status & 0xf0;
        const auto channel = status & 15;
        if (family != 0xb0 && family != 0xc0 && family != 0xe0 && family != 0x90) {
            throw std::invalid_argument("unsupported program callback message");
        }
        const auto bytes = family == 0xc0 ? 1U : 2U;
        if (cursor + bytes >= state.sequence.size()) { throw std::invalid_argument("truncated program event"); }
        const auto first = state.sequence[cursor++];
        const auto second = bytes == 2 ? state.sequence[cursor++] : 0;
        if (first > simpleDeltaLimit || second > simpleDeltaLimit) {
            throw std::invalid_argument("invalid program event data");
        }
        b.plain(explicitStatus ? 19 : 17); //1B40..1B9D.
        if (family == 0xc0) {
            b.plain(5); //1B9E..1BAC: ordinary-channel CALL1D57.
            if (channel == 15) {
                if (first != 0x7f || cursor < 4 || cursor - 4 > 0xffff) {
                    throw std::invalid_argument("unsupported sequence status marker");
                }
                b.plain(19); //1D2A..1D4F,1D54..1D56:7F loop marker.
                b.result.state.program->loopMarker = static_cast<std::uint16_t>(cursor - 4);
                b.plain(1); //1BAA.
            } else {
                b.changeProgram(channel, first);
            }
            b.plain(3); //1BAF,1C52,1C58.
        } else if (family == 0xb0) {
            b.plain(4); //C-family then B-family dispatch checks.
            constexpr std::uint8_t sustainController = 0x40;
            constexpr std::array<std::uint8_t, 5> controllers{0x4b, sustainController, 0x4c, 0x60, 0x4e};
            for (const auto controller : controllers) {
                b.plain(2);
                if (first != controller) { continue; }
                if (controller == sustainController) {
                    b.plain(1); //1BCD CALL2001.
                    b.sustainChannel(channel, second);
                    break;
                }
                if (controller != 0x4e) {
                    throw std::invalid_argument("unsupported program controller handler id="+std::to_string(first)
                        +" value="+std::to_string(second)+" channel="+std::to_string(channel)
                        +" cursor="+std::to_string(cursor));
                }
                b.plain(1); //1BF7 CALL1CE1.
                b.enableChannel(channel, second);
                break;
            }
            b.plain(5); //1BFA,1C50,1C51,1C52,1C58.
        } else if (family == 0xe0) {
            b.plain(15); //1B9E..1C42 E-family handler call.
            b.pitchBend(channel, first, second);
            b.plain(5); //1C45,1C50,1C51,1C52,1C58.
        } else {
            b.plain(11); //1B9E/B-family/F-family,BP8,9-family checks and1C2C CALL.
            b.noteOn(channel, first, second);
            b.plain(5); //1C2F,1C50,1C51,1C52,1C58.
        }
        const auto delta = state.sequence[cursor++];
        if (delta > simpleDeltaLimit) { throw std::invalid_argument("extended program delta is not admitted"); }
        b.plain(21); //1ABC..1B1D simple delta parse.
        if (delta == 0) { continue; }
        b.plain(5); //1B1F..1B38 positive delta and fade test.
        b.result.state.position = cursor;
        b.result.state.runningStatus = status;
        b.result.state.wait = delta - 1;
        if (b.result.state.fade != 0) {
            b.plain(1); //1B3A CALL2CFE, before parser cursor publication.
            b.fade();
        }
        b.plain(1); //1B3D JMP1C5B.
        //1C5B..1C6A, dispatcher return9, wrapper return7; existing STI/POPF tail.
        b.plain(6 + 9 + 7);
        return b.result;
    }
    throw std::invalid_argument("program callback lacks bounded terminal delta");
}

SoundBlasterCallbackAdvance soundBlasterProgramCallbackAdvance(const SoundBlasterCallbackState& state) {
    return programCallbackAdvance(state, ProgramEofAdmission::onceOnly);
}

SoundBlasterCallbackAdvance soundBlasterMenuCallbackAdvance(const SoundBlasterCallbackState& state) {
    if (state.domain != SoundBlasterCallbackDomain::sequence || !state.driverEnabled || !state.program) {
        throw std::invalid_argument("retained menu callback requires its selected program");
    }
    return programCallbackAdvance(state, ProgramEofAdmission::retainedMenu);
}

SoundBlasterCallbackAdvance soundBlasterLevel1CallbackAdvance(const SoundBlasterCallbackState& state) {
    if (state.domain != SoundBlasterCallbackDomain::sequence || !state.driverEnabled || !state.program) {
        throw std::invalid_argument("retained Level1 callback requires its selected program");
    }
    return programCallbackAdvance(state, ProgramEofAdmission::retainedLevel1);
}

SoundBlasterCallbackAdvance soundBlasterLevel2CallbackAdvance(const SoundBlasterCallbackState& state) {
    if (state.domain != SoundBlasterCallbackDomain::sequence || !state.driverEnabled || !state.program) {
        throw std::invalid_argument("retained Level2 callback requires its selected program");
    }
    return programCallbackAdvance(state, ProgramEofAdmission::retainedLevel2);
}

SoundBlasterCallbackAdvance soundBlasterPlayableCallbackAdvance(const SoundBlasterCallbackState& state) {
    switch (state.domain) {
    case SoundBlasterCallbackDomain::sequence: return soundBlasterLevel2CallbackAdvance(state);
    case SoundBlasterCallbackDomain::successNoHandle: return soundBlasterSuccessNoHandleAdvance(state);
    case SoundBlasterCallbackDomain::level2RetryNoHandle: return soundBlasterLevel2RetryNoHandleAdvance(state);
    default: return soundBlasterCallbackAdvance(state);
    }
}

enum class DirectEventAdmission { runningProgram, selectedProgram };

static SoundBlasterDirectAdvance directAdvance(
    const SoundBlasterCallbackState& incoming, const std::array<std::uint8_t, 8>& record,
    const SoundBlasterDirectCaller caller,const DirectEventAdmission admission) {
    const bool storyNoHandle=admission==DirectEventAdmission::selectedProgram
        &&(caller==SoundBlasterDirectCaller::storyFirstCue||caller==SoundBlasterDirectCaller::storySecondCue)
        &&incoming.domain==SoundBlasterCallbackDomain::level2RetryNoHandle;
    requireDomain(incoming,storyNoHandle?ProgramStorageDomain::level2RetryNoHandle:ProgramStorageDomain::selected);
    if(storyNoHandle) {soundBlasterLevel2RetryNoHandleAdvance(incoming);}
    const bool playableSystem = caller == SoundBlasterDirectCaller::gameplaySystemActivate
        || caller == SoundBlasterDirectCaller::gameplaySystemCycle
        || caller == SoundBlasterDirectCaller::gameplaySuperPrimaryFire
        || caller == SoundBlasterDirectCaller::gameplayTurboActivate
        || caller == SoundBlasterDirectCaller::gameplayShieldDefeat
        || caller == SoundBlasterDirectCaller::gameplayScannerPulse
        || caller == SoundBlasterDirectCaller::gameplayDecoy
        || caller == SoundBlasterDirectCaller::gameplayBomb
        || caller == SoundBlasterDirectCaller::storyFirstCue || caller == SoundBlasterDirectCaller::storySecondCue;
    if (playableSystem && admission != DirectEventAdmission::selectedProgram) {
        throw std::invalid_argument("system direct caller requires explicit playable execution");
    }
    if (admission==DirectEventAdmission::runningProgram && (!incoming.active || incoming.program->handleStatus != 0)) {
        throw std::invalid_argument("direct event requires a running selected program");
    }
    if (record[0] >= instrumentCount || record[1] > simpleDeltaLimit) {
        throw std::invalid_argument("unsupported direct event program or note");
    }
    if ((caller != SoundBlasterDirectCaller::unownedChannel && caller != SoundBlasterDirectCaller::gameplayPrimaryFire
            && caller != SoundBlasterDirectCaller::gameplayEnemyFire && caller != SoundBlasterDirectCaller::gameplayPlayerHit
            && caller != SoundBlasterDirectCaller::gameplayPlayerCombat
            && caller != SoundBlasterDirectCaller::gameplayTargetImpact
            && caller != SoundBlasterDirectCaller::gameplaySystemBonus
            && caller != SoundBlasterDirectCaller::gameplayFuelBay
            && caller != SoundBlasterDirectCaller::gameplayArrival
            && caller != SoundBlasterDirectCaller::gameplayGuardianDefeat
            && caller != SoundBlasterDirectCaller::gameplayEnemyType20
            && caller != SoundBlasterDirectCaller::gameplaySolidContact
            && caller != SoundBlasterDirectCaller::gameplayOrdinaryDeath
            && caller != SoundBlasterDirectCaller::gameplaySystemActivate
            && caller != SoundBlasterDirectCaller::gameplaySystemCycle
            && caller != SoundBlasterDirectCaller::gameplaySuperPrimaryFire
            && caller != SoundBlasterDirectCaller::gameplayTurboActivate
            && caller != SoundBlasterDirectCaller::gameplayShieldDefeat
            && caller != SoundBlasterDirectCaller::gameplayScannerPulse
            && caller != SoundBlasterDirectCaller::gameplayDecoy
            && caller != SoundBlasterDirectCaller::gameplayBomb
            && caller != SoundBlasterDirectCaller::storyFirstCue && caller != SoundBlasterDirectCaller::storySecondCue)
        || (caller == SoundBlasterDirectCaller::storyFirstCue
            && (record != storyFirstCueDirectRecord || incoming.fade != 0))
        || (caller == SoundBlasterDirectCaller::storySecondCue
            && (record != storySecondCueDirectRecord || incoming.fade != 0))
        || (caller == SoundBlasterDirectCaller::gameplaySystemActivate && record != gameplaySystemActivateDirectRecord)
        || (caller == SoundBlasterDirectCaller::gameplaySystemCycle && record != gameplaySystemCycleDirectRecord)
        || (caller == SoundBlasterDirectCaller::gameplaySuperPrimaryFire
            && (record != gameplaySuperPrimaryFireDirectRecord || incoming.fade != 0))
        || (caller == SoundBlasterDirectCaller::gameplayTurboActivate
            && (record != gameplayTurboActivateDirectRecord || incoming.fade != 0))
        || (caller == SoundBlasterDirectCaller::gameplayShieldDefeat
            && (record != gameplayShieldDefeatDirectRecord || incoming.fade != 0))
        || (caller == SoundBlasterDirectCaller::gameplayScannerPulse
            && (record != gameplayScannerPulseDirectRecord || incoming.fade != 0))
        || (caller == SoundBlasterDirectCaller::gameplayDecoy
            && (record != gameplayDecoyDirectRecord || incoming.fade != 0))
        || (caller == SoundBlasterDirectCaller::gameplayBomb
            && (record != gameplayBombDirectRecord || incoming.fade != 0 || !incoming.program->bomb7CallerRow))
        || (caller == SoundBlasterDirectCaller::gameplayPrimaryFire && record != gameplayPrimaryFireDirectRecord)
        || (caller == SoundBlasterDirectCaller::gameplayFuelBay && record != gameplayFuelBayDirectRecord)
        || (caller == SoundBlasterDirectCaller::gameplayEnemyFire && record != gameplayEnemyType17DirectRecord)
        || (caller == SoundBlasterDirectCaller::gameplayEnemyType20
            && (record != gameplayEnemyType20DirectRecord || incoming.fade != 0 || !incoming.program->enemy20CallerWindow))
        || (caller == SoundBlasterDirectCaller::gameplaySolidContact
            && (record != gameplaySolidContactDirectRecord || incoming.fade != 0))
        || (caller == SoundBlasterDirectCaller::gameplayOrdinaryDeath
            && (record != gameplayOrdinaryDeathDirectRecord || incoming.fade != 0))
        || (caller == SoundBlasterDirectCaller::gameplayPlayerCombat
            && (record != gameplayPlayerCombatDirectRecord || incoming.fade != 0))
        || (caller == SoundBlasterDirectCaller::gameplayTargetImpact
            && (record != gameplayTargetImpactDirectRecord || incoming.fade != 0))
        || (caller == SoundBlasterDirectCaller::gameplaySystemBonus
            && (record != gameplaySystemBonusDirectRecord || incoming.fade != 0))
        || (caller == SoundBlasterDirectCaller::gameplayArrival
            && (record != gameplayArrivalDirectRecord || incoming.fade != 0))
        || (caller == SoundBlasterDirectCaller::gameplayGuardianDefeat
            && (record != gameplayGuardianDefeatDirectRecord || incoming.fade != 0))
        || (caller == SoundBlasterDirectCaller::gameplayPlayerHit
            && (record != gameplayPlayerHitDirectRecord || incoming.fade != 0
                || !incoming.program->playerHitVolumeRow
                || incoming.program->playerHitVolumeRow->volume != gameplayPlayerHitCallerVolume))) {
        throw std::invalid_argument("unsupported direct event caller window");
    }
    // During1D57, CS:B0 still points to the caller event record, not a song handle.
    std::optional<std::uint16_t> callerVolume;
    if (caller == SoundBlasterDirectCaller::gameplayPrimaryFire) {
        callerVolume = gameplayPrimaryFireCallerVolume;
    } else if (caller == SoundBlasterDirectCaller::gameplayEnemyFire) {
        callerVolume = gameplayEnemyType17CallerVolume;
    } else if (caller == SoundBlasterDirectCaller::gameplayFuelBay) {
        callerVolume = gameplayFuelBayCallerVolume;
    } else if (caller == SoundBlasterDirectCaller::gameplayEnemyType20) {
        callerVolume = gameplayEnemyType20CallerVolume;
    } else if (caller == SoundBlasterDirectCaller::gameplaySolidContact) {
        callerVolume = gameplaySolidContactCallerVolume;
    } else if (caller == SoundBlasterDirectCaller::gameplayOrdinaryDeath) {
        callerVolume = gameplayOrdinaryDeathCallerVolume;
    } else if (caller == SoundBlasterDirectCaller::gameplayPlayerCombat) {
        callerVolume = gameplayPlayerCombatCallerVolume;
    } else if (caller == SoundBlasterDirectCaller::gameplayTargetImpact) {
        callerVolume = gameplayTargetImpactCallerVolume;
    } else if (caller == SoundBlasterDirectCaller::gameplaySystemBonus) {
        callerVolume = gameplaySystemBonusCallerVolume;
    } else if (caller == SoundBlasterDirectCaller::gameplayArrival) {
        callerVolume = gameplayArrivalCallerVolume;
    } else if (caller == SoundBlasterDirectCaller::gameplayGuardianDefeat) {
        callerVolume = gameplayGuardianDefeatCallerVolume;
    } else if (caller == SoundBlasterDirectCaller::gameplayPlayerHit) {
        callerVolume = gameplayPlayerHitCallerVolume;
    } else if (caller == SoundBlasterDirectCaller::gameplaySystemActivate) {
        callerVolume = gameplaySystemActivateCallerVolume;
    } else if (caller == SoundBlasterDirectCaller::gameplaySystemCycle) {
        callerVolume = gameplaySystemCycleCallerVolume;
    } else if (caller == SoundBlasterDirectCaller::gameplaySuperPrimaryFire) {
        callerVolume = gameplaySuperPrimaryFireCallerVolume;
    } else if (caller == SoundBlasterDirectCaller::gameplayTurboActivate) {
        callerVolume = gameplayTurboActivateCallerVolume;
    } else if (caller == SoundBlasterDirectCaller::gameplayShieldDefeat) {
        callerVolume = gameplayShieldDefeatCallerVolume;
    } else if (caller == SoundBlasterDirectCaller::gameplayScannerPulse) {
        callerVolume = gameplayScannerPulseCallerVolume;
    } else if (caller == SoundBlasterDirectCaller::gameplayDecoy) {
        callerVolume = gameplayDecoyCallerVolume;
    } else if (caller == SoundBlasterDirectCaller::gameplayBomb) {
        callerVolume = gameplayBombCallerVolume;
    } else if (caller == SoundBlasterDirectCaller::storyFirstCue || caller == SoundBlasterDirectCaller::storySecondCue) {
        callerVolume = storyCueCallerVolume;
    }
    ProgramBuilder b(incoming, callerVolume);
    b.diagnosticEvent = record[5];
    b.diagnosticProgram = record[0];
    auto& timers = b.result.state.voices.directTimers;
    std::size_t slot = 0;
    for (; slot < timers.size(); ++slot) {
        b.plain(3); //216E/2172/2174,217E/2182/2184,218E/2192/2194.
        if (timers[slot] == 0) {
            b.plain(2); //MOV BX,slot and CALL21DA.
            break;
        }
    }
    const bool replacing = slot == timers.size();
    if (replacing) {
        slot = 0;
        b.plain(4); //219E..21AA, signed timer selection.
        if (static_cast<std::int16_t>(timers[0]) >= static_cast<std::int16_t>(timers[1])) {
            slot = 1;
            b.plain(2); //21AC/21AF.
        }
        b.plain(2); //21B3/21B8.
        if (static_cast<std::int16_t>(timers[slot]) >= static_cast<std::int16_t>(timers[2])) {
            slot = 2;
            b.plain(1); //21BA.
        }
        b.plain(3); //21BD/21BF/21C2 CALL2331.
        const auto voice = static_cast<std::uint8_t>(slot + 6);
        b.noteFrequency(voice, b.result.state.voices.notes[voice], NoteSound::off);
        b.plain(5); //21C5..21D2, timer clear and CALL21DA.
        timers[slot] = 0;
    }
    const auto voice = static_cast<std::uint8_t>(slot + 6);
    const auto channel = static_cast<std::uint8_t>(slot + 13);
    if (caller == SoundBlasterDirectCaller::unownedChannel
        && std::find(incoming.voices.channels.begin(), incoming.voices.channels.end(), channel)
        != incoming.voices.channels.end()) {
        throw std::invalid_argument("direct program owner requires recovered caller volume storage");
    }
    b.plain(7); //21DA..21E6 CALL1D57; source helper also increments SI.
    b.changeProgram(channel, record[0]);
    b.plain(9); //21E9..21F8: note/timer loads and95FF alias store.
    timers[slot] = static_cast<std::uint16_t>(record[2] | (record[3] << 8));
    b.result.state.velocities[0] = 0xff;
    b.synchronizeLatch();
    b.plain(6); //21FE..220A: source scratch handleD0E and CALL2322.
    b.noteFrequency(voice, record[1], NoteSound::on, FrequencyHandle::direct);
    b.plain(2); //220D/220E.
    if (!replacing) {
        b.plain(1); //217C/218C/219C JMP21D5.
    }
    return {std::move(b.result.state), std::move(b.result.blocks)};
}

SoundBlasterDirectAdvance soundBlasterDirectAdvance(const SoundBlasterCallbackState& incoming,
    const std::array<std::uint8_t,8>& record,const SoundBlasterDirectCaller caller) {
    return directAdvance(incoming,record,caller,DirectEventAdmission::runningProgram);
}

SoundBlasterDirectAdvance soundBlasterPlayableDirectAdvance(const SoundBlasterCallbackState& incoming,
    const std::array<std::uint8_t,8>& record,const SoundBlasterDirectCaller caller) {
    return directAdvance(incoming,record,caller,DirectEventAdmission::selectedProgram);
}

SoundBlasterDirectAdvance soundBlasterStopAdvance(const SoundBlasterCallbackState& incoming,
    const SoundBlasterStopProfile profile) {
    requireDomain(incoming);
    if (profile != SoundBlasterStopProfile::zeroDirectTimers
        && profile != SoundBlasterStopProfile::retainedDirectTimers) {
        throw std::invalid_argument("unsupported source stop profile");
    }
    if (profile == SoundBlasterStopProfile::zeroDirectTimers
        && std::any_of(incoming.voices.directTimers.begin(), incoming.voices.directTimers.end(),
            [](const auto timer) { return timer != 0; })) {
        throw std::invalid_argument("source stop requires zero direct timers");
    }
    ProgramBuilder b(incoming);
    b.plain(1); //2D91 CALL2DA0, between2D90 CLI and2D94 STI.
    b.keyOffAllVoices();
    return {std::move(b.result.state), std::move(b.result.blocks)};
}

SoundBlasterCallbackAdvance soundBlasterSuccessNoHandleAdvance(const SoundBlasterCallbackState& state) {
    requireDomain(state,ProgramStorageDomain::successNoHandle);
    if (state.active || !state.sequence.empty() || state.position != 0 || state.wait != 0) {
        throw std::invalid_argument("success no-current callback has selected handle fields");
    }
    ProgramBuilder b(state);
    b.result.kind = SoundBlasterCallbackKind::successNoHandle;
    b.plain(15 + 12); //0494..04DB,dispatcher CALL1C6B.
    b.timers();
    b.plain(9 + 8); //Dispatcher return;04DF/04CA..04D0.
    return b.result;
}

SoundBlasterCallbackState soundBlasterSuccessDeselectState(const SoundBlasterCallbackState& incoming) {
    if (incoming.domain != SoundBlasterCallbackDomain::sequence || !incoming.program
        || incoming.active || incoming.program->handleStatus != 0xffff) {
        throw std::invalid_argument("success current clear requires its completed source stop");
    }
    auto result = incoming;
    result.domain = SoundBlasterCallbackDomain::successNoHandle;
    result.active = false;
    result.sequence.clear();
    result.position = 0;
    result.wait = 0;
    result.operation = 0x1e;
    soundBlasterSuccessNoHandleAdvance(result);
    return result;
}

SoundBlasterCallbackAdvance soundBlasterLevel2RetryNoHandleAdvance(const SoundBlasterCallbackState& state) {
    requireDomain(state,ProgramStorageDomain::level2RetryNoHandle);
    if (state.active || !state.sequence.empty() || state.position != 0 || state.wait != 0) {
        throw std::invalid_argument("Level2 retry no-current callback has selected handle fields");
    }
    ProgramBuilder b(state);
    b.result.kind = SoundBlasterCallbackKind::level2RetryNoHandle;
    b.plain(15 + 12); //0494..04DB,dispatcher CALL1C6B.
    b.timers();
    b.plain(9 + 8); //Dispatcher return;04DF/04CA..04D0.
    return b.result;
}
SoundBlasterCallbackState soundBlasterLevel2RetryDeselectState(const SoundBlasterCallbackState& incoming) {
    if (incoming.domain != SoundBlasterCallbackDomain::sequence || !incoming.program
        || incoming.active || incoming.program->handleStatus != 0xffff) {
        throw std::invalid_argument("Level2 retry current clear requires its completed source stop");
    }
    auto result = incoming;
    result.domain = SoundBlasterCallbackDomain::level2RetryNoHandle;
    result.active = false;result.sequence.clear();result.position = 0;result.wait = 0;result.operation = 0x1e;
    soundBlasterLevel2RetryNoHandleAdvance(result);
    return result;
}

SoundBlasterCallbackState soundBlasterDeselectState(const SoundBlasterCallbackState& incoming) {
    soundBlasterStopAdvance(incoming);
    auto state = incoming;
    state.domain = SoundBlasterCallbackDomain::noHandle;
    state.active = false;
    state.sequence.clear();
    state.position = 0;
    state.wait = 0;
    state.runningStatus = 0;
    state.operation = 0x1e;
    state.program.reset();
    soundBlasterCallbackAdvance(state);
    return state;
}

SoundBlasterProgramState soundBlasterProgramInitialState(
    const std::vector<std::uint8_t>& bank, const std::vector<std::uint8_t>& volume,
    const std::array<std::array<std::uint8_t, 14>, 18>& operators,
    const DriverSequenceState& sequenceState) {
    if (bank.size() != instrumentCount * instrumentBytes || volume.size() != volumeRows * volumeColumns) {
        throw std::invalid_argument("unsupported initial program resources");
    }
    SoundBlasterProgramState result{};
    result.instrumentBank = bank;
    result.volumeTable = volume;
    result.operators = operators;
    return soundBlasterProgramSequenceState(result, sequenceState);
}

SoundBlasterProgramState soundBlasterProgramSequenceState(
    const SoundBlasterProgramState& incoming, const DriverSequenceState& sequenceState) {
    auto result = incoming;
    for (std::size_t channel = 0; channel < result.channelPrograms.size(); ++channel) {
        if (sequenceState.channels[channel].program) {
            result.channelPrograms[channel] = static_cast<std::uint8_t>(*sequenceState.channels[channel].program);
        }
    }
    for (std::size_t voice = 0; voice < physicalVoices; ++voice) {
        auto& target = result.voices[voice];
        const auto& source = sequenceState.voices[voice];
        if (source.marker) { target.program = static_cast<std::uint8_t>(*source.marker); }
        if (source.auxiliary) { target.tuningMagnitude = static_cast<std::uint8_t>(*source.auxiliary); }
        if (source.words[0]) { target.carrierScale = *source.words[0]; }
        if (source.words[1]) { target.operatorScale = *source.words[1]; }
        if (source.words[2]) { target.operatorDisabled = *source.words[2]; }
    }
    if (sequenceState.handleStatus) { result.handleStatus = *sequenceState.handleStatus; }
    if (sequenceState.accumulator) { result.accumulator = *sequenceState.accumulator; }
    if (sequenceState.fadeDivider) { result.fadeDivider = *sequenceState.fadeDivider; }
    if (sequenceState.repeatPosition) { result.loopMarker = *sequenceState.repeatPosition; }
    result.repeatCount = 0xffff; //030C initializes the new handle, not the prior allocation.
    result.handleVolume = 15;
    return result;
}

SoundBlasterCallbackState soundBlasterDirectResetState(const SoundBlasterCallbackState& incoming) {
    if (!incoming.program || incoming.domain != SoundBlasterCallbackDomain::sequence) {
        throw std::invalid_argument("selected direct reset requires owned program storage");
    }
    const auto reset = soundBlasterDirectResetOperations();
    auto result = applyDriverSequenceCallbackState(incoming,reset);
    result.program = soundBlasterProgramSequenceState(*incoming.program,reset.state);
    //206B does not perform02E6's new-handle initialization.
    result.program->repeatCount = incoming.program->repeatCount;
    result.program->handleVolume = incoming.program->handleVolume;
    return result;
}

SoundBlasterCallbackState soundBlasterPlayableDirectResetState(const SoundBlasterCallbackState& incoming) {
    if (incoming.domain==SoundBlasterCallbackDomain::sequence) {return soundBlasterDirectResetState(incoming);}
    if (!incoming.program || (incoming.domain!=SoundBlasterCallbackDomain::successNoHandle
        && incoming.domain!=SoundBlasterCallbackDomain::level2RetryNoHandle)) {
        throw std::invalid_argument("playable direct reset requires its retained program owner");
    }
    const auto reset=soundBlasterDirectResetOperations();
    auto result=applyDriverSequenceCallbackState(incoming,reset);
    result.program=soundBlasterProgramSequenceState(*incoming.program,reset.state);
    result.program->repeatCount=incoming.program->repeatCount;
    result.program->handleVolume=incoming.program->handleVolume;
    return result;
}

}  // namespace stellar7
