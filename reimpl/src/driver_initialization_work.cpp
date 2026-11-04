#include "stellar7/driver_initialization_work.hpp"

#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::size_t resourceHeaderBytes = 19;
constexpr std::size_t bankHalfBytes = 0x540;
constexpr std::uint64_t oplDispatches = 104;
constexpr std::uint64_t oplDataDispatch = 23;
constexpr std::array<std::uint8_t, 18> operatorOffsets{{0,1,2,3,4,5,8,9,10,11,12,13,16,17,18,19,20,21}};
constexpr std::array<std::uint8_t, 18> channelOffsets{{0,1,2,0,1,2,3,4,5,3,4,5,6,7,8,6,7,8}};
constexpr std::array<std::uint8_t, 13> modulatorPatch{{1,1,3,15,5,0,1,3,15,0,0,0,1}};
constexpr std::array<std::uint8_t, 13> carrierPatch{{0,1,1,15,7,0,2,4,0,0,0,1,0}};

class InitializationBuilder {
public:
    SoundBlasterInitializationWork build(const std::vector<std::uint8_t>& resource) {
        plain(6); //2E67..2E76.
        copyBank(resource, resourceHeaderBytes, 0, 0x2e7c);
        plain(6); //2E85..2E91 marker test.
        plain(1); //2E93 second-bank loop setup.
        copyBank(resource, resourceHeaderBytes + bankHalfBytes + 2, bankHalfBytes, 0x2e99);
        plain(1); //2EA2 CALL220F.
        resetOpl();
        plain(1); //2EA5 CALL2A0C; entire hardware probe is replaced.
        result_.hostServices.push_back({SoundBlasterInitializationHostServiceKind::portableOplCapability,
            0x2ea5, dispatches_});
        plain(3); //2EA8,2EAA,2EB1 success flag.
        result_.state.ready = true;
        effect(SoundBlasterInitializationEffectKind::ready, 0, 1, 0x2eb1);
        plain(1); //2EB7.
        result_.state.accumulator = 0;
        effect(SoundBlasterInitializationEffectKind::accumulator, 0, 0, 0x2eb7);
        plain(2); //2EBE,2EC1.
        result_.state.operation2Entry = 0x2d83;
        effect(SoundBlasterInitializationEffectKind::operation2Entry, 0, 0x2d83, 0x2ec1);
        plain(3); //2EC8,2ECB,2ECD RET.
        return std::move(result_);
    }

private:
    void plain(const std::uint64_t count) {
        result_.operations.push_back({NormalWorkKind::instructions, count});
        dispatches_ += count;
    }

    void effect(const SoundBlasterInitializationEffectKind kind, const std::uint16_t index,
        const std::uint16_t value, const std::uint16_t pc) {
        result_.effects.push_back({kind, index, value, pc, dispatches_});
    }

    void opl(const std::uint8_t reg, const std::uint8_t value) {
        result_.operations.push_back({NormalWorkKind::soundBlasterOplWrite, 1,
            static_cast<std::uint16_t>((static_cast<std::uint16_t>(reg) << 8) | value)});
        result_.effects.push_back({SoundBlasterInitializationEffectKind::oplRegister,
            reg, value, 0x26f8, dispatches_ + oplDataDispatch});
        dispatches_ += oplDispatches;
    }

    void copyBank(const std::vector<std::uint8_t>& resource, const std::size_t source,
        const std::size_t destination, const std::uint16_t pc) {
        for (std::size_t index = 0; index < bankHalfBytes; ++index) {
            plain(2); //Load/store, then INC SI/INC DI/LOOP.
            result_.state.bank.push_back(resource[source + index]);
            effect(SoundBlasterInitializationEffectKind::bankByte,
                static_cast<std::uint16_t>(destination + index), resource[source + index], pc);
            plain(3);
        }
    }

    void rhythm() {
        plain(9); //25A3: initial depth flags zero, including CALL26E5.
        opl(0xbd, 0);
        plain(3);
    }

    void noteSelect() {
        plain(7); //2443: note-select flag zero, including CALL26E5.
        opl(0x08, 0);
        plain(3);
    }

    void patchOutputs(const std::size_t index, const bool carrier) {
        const auto& patch = result_.state.operators[index];
        plain(1); rhythm(); //23E2.
        plain(1); noteSelect();
        plain(1); //23E8 CALL23FB.
        plain(33);
        opl(static_cast<std::uint8_t>(0x40 + operatorOffsets[index]),
            static_cast<std::uint8_t>((patch[0] << 6) | (patch[8] & 0x3f)));
        plain(5);
        plain(1); //23EB CALL245A.
        if (carrier) {
            plain(10); //245A skips its channel write for carrier operators.
        } else {
            plain(33); //B7=1 skips the conditional INC CL.
            opl(static_cast<std::uint8_t>(0xc0 + channelOffsets[index]),
                static_cast<std::uint8_t>((patch[2] * 2 + (patch[12] == 0 ? 1 : 0)) & 0x0f));
            plain(4);
        }
        plain(1); //23EE CALL24AA.
        plain(32);
        opl(static_cast<std::uint8_t>(0x60 + operatorOffsets[index]),
            static_cast<std::uint8_t>((patch[3] << 4) | (patch[6] & 0x0f)));
        plain(4);
        plain(1); //23F1 CALL24F0.
        plain(32);
        opl(static_cast<std::uint8_t>(0x80 + operatorOffsets[index]),
            static_cast<std::uint8_t>((patch[4] << 4) | (patch[7] & 0x0f)));
        plain(4);
        plain(1); //23F4 CALL2536.
        std::uint8_t flags = 0;
        const std::array<std::uint8_t, 4> fields{{9,10,5,11}};
        const std::array<std::uint8_t, 4> bits{{0x80,0x40,0x20,0x10}};
        std::uint64_t taken = 0;
        for (std::size_t field = 0; field < fields.size(); ++field) {
            if (patch[fields[field]] != 0) {
                flags |= bits[field];
                ++taken;
            }
        }
        plain(41 + taken); //Fixed body excludes four conditional OR instructions.
        opl(static_cast<std::uint8_t>(0x20 + operatorOffsets[index]),
            static_cast<std::uint8_t>(flags | (patch[1] & 0x0f)));
        plain(4);
        plain(1); //23F7 CALL25C6; FC02 is still zero, so no waveform write.
        plain(3);
        plain(1); //23FA RET.
    }

    void installPatch(const std::size_t index, const bool carrier) {
        const auto& patch = carrier ? carrierPatch : modulatorPatch;
        plain(8); //23BB..23C4 scratch-copy setup.
        for (std::uint16_t byte = 0; byte < patch.size(); ++byte) {
            plain(2);
            result_.state.scratch[byte] = patch[byte];
            effect(SoundBlasterInitializationEffectKind::scratchByte, byte, patch[byte], 0x23c9);
            plain(4);
        }
        plain(3); //23D5..23DA CALL2373.
        plain(22); //2373..2392 operator-record setup.
        for (std::uint16_t byte = 0; byte < patch.size(); ++byte) {
            plain(4);
            result_.state.operators[index][byte] = result_.state.scratch[byte];
            effect(SoundBlasterInitializationEffectKind::operatorByte,
                static_cast<std::uint16_t>(index * 14 + byte), patch[byte], 0x239a);
            plain(6);
        }
        plain(3); //23A8..23AC mode byte is DX&3=0.
        result_.state.operators[index][13] = 0;
        effect(SoundBlasterInitializationEffectKind::operatorByte,
            static_cast<std::uint16_t>(index * 14 + 13), 0, 0x23ac);
        plain(2); //23B1..23B2 CALL23E2.
        patchOutputs(index, carrier);
        plain(6); //2373 epilogue23B5..23BA.
        plain(5); //23BB epilogue23DD..23E1.
    }

    void resetOpl() {
        plain(6); //220F..2217.
        opl(0x01, 0);
        plain(2); //221A..221C CALL224F.
        plain(1); //224F clears rhythm mode.
        effect(SoundBlasterInitializationEffectKind::rhythmMode, 0, 0, 0x224f);
        plain(1); //2255 CALL2346.
        plain(5); //2346..234A loop setup.
        for (std::size_t index = 0; index < operatorOffsets.size(); ++index) {
            const bool carrier = index % 6 >= 3;
            plain(4); //234C..2356 classification.
            plain(2); //Template LEA/CALL23BB.
            installPatch(index, carrier);
            if (carrier) {
                plain(1); //235F JMP after the carrier-template call.
            }
            plain(3); //2368..236C loop control.
        }
        plain(5); //236E..2372.
        plain(1); rhythm(); //2258 CALL25A3.
        plain(1); //225B RET.
        plain(4); //221F..2225 amplitude-depth clear.
        effect(SoundBlasterInitializationEffectKind::amplitudeDepth, 0, 0, 0x2225);
        plain(1);
        effect(SoundBlasterInitializationEffectKind::vibratoDepth, 0, 0, 0x222a);
        plain(1);
        effect(SoundBlasterInitializationEffectKind::noteSelect, 0, 0, 0x222f);
        plain(1); rhythm();
        plain(1); noteSelect();
        plain(1); //223A BX=0.
        for (std::uint8_t channel = 0; channel < 9; ++channel) {
            plain(1); //223C CALL26CE.
            plain(5);
            opl(static_cast<std::uint8_t>(0xa0 + channel), 0);
            plain(4);
            opl(static_cast<std::uint8_t>(0xb0 + channel), 0);
            plain(3);
            plain(3); //223F..2243.
        }
        plain(2); //2245..2248 CALL225C.
        plain(3); //225C,225E,2269.
        result_.state.waveformMask = 0x20;
        effect(SoundBlasterInitializationEffectKind::waveformMask, 0, 0x20, 0x2269);
        plain(3); //2270..2272.
        for (const auto offset : operatorOffsets) {
            plain(5); //2274..2281.
            opl(static_cast<std::uint8_t>(0xe0 + offset), 0);
            plain(3);
        }
        plain(4); //228A..2293.
        opl(0x01, 0x20);
        plain(2); //2296..2297.
        plain(4); //224B..224E.
    }

    SoundBlasterInitializationWork result_{};
    std::uint64_t dispatches_ = 0;
};
}

SoundBlasterInitializationWork soundBlasterInitializationOperations(
    const std::vector<std::uint8_t>& instrumentResource) {
    if (instrumentResource.size() != resourceHeaderBytes + 2 * bankHalfBytes + 2
        || instrumentResource[0] != 0x89 || instrumentResource[1] != 0x11
        || instrumentResource[resourceHeaderBytes + bankHalfBytes] != 0xab
        || instrumentResource[resourceHeaderBytes + bankHalfBytes + 1] != 0xcd) {
        throw std::invalid_argument("unsupported first Sound Blaster instrument resource");
    }
    return InitializationBuilder{}.build(instrumentResource);
}

}  // namespace stellar7


