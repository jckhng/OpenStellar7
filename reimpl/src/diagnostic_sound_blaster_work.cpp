#include "diagnostic_commands.hpp"
#include "stellar7/audio_work.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace stellar7::diagnostics {
namespace {
void traceSoundBlasterKeyOffWork() {
    constexpr std::size_t maximumCases = 1024;
    constexpr std::size_t maximumEvents = 32;
    constexpr std::array<const char*, 4> kinds{"plain", "frequency_low", "frequency_high", "carrier_level"};
    const auto readBounded = [](const std::uint64_t maximum) {
        std::uint64_t value = 0;
        if (!(std::cin >> value) || value > maximum) {
            throw std::runtime_error("invalid bounded key-off work input");
        }
        return value;
    };
    const auto readByte = [&readBounded]() {
        return static_cast<std::uint8_t>(readBounded(std::numeric_limits<std::uint8_t>::max()));
    };
    const auto count = readBounded(maximumCases);
    if (count == 0) {
        throw std::runtime_error("empty key-off work batch");
    }
    std::cout << '[';
    for (std::size_t index = 0; index < count; ++index) {
        const auto suppressed = readBounded(1);
        const auto latched = readBounded(1);
        stellar7::SoundBlasterKeyOffState state{};
        for (auto& timer : state.directTimers) {
            timer = static_cast<std::uint16_t>(readBounded(std::numeric_limits<std::uint16_t>::max()));
        }
        for (auto* group : {&state.channels, &state.notes, &state.tuning}) {
            for (auto& value : *group) {
                value = readByte();
            }
        }
        for (auto& value : state.channelDeferrals) {
            value = readByte();
        }
        const auto eventCount = readBounded(maximumEvents);
        std::vector<stellar7::SoundBlasterExplicitKeyOff> events;
        for (std::size_t eventIndex = 0; eventIndex < eventCount; ++eventIndex) {
            const auto channel = readByte();
            const auto note = readByte();
            const auto delay = readByte();
            events.push_back({channel, note, delay});
        }
        const auto work = stellar7::soundBlasterExplicitKeyOffWork(state, events,
            suppressed ? stellar7::SoundBlasterFrequencyGate::Suppressed : stellar7::SoundBlasterFrequencyGate::Open,
            latched ? stellar7::SoundBlasterLevelLatch::Latched : stellar7::SoundBlasterLevelLatch::Unlatched);
        std::cout << (index == 0 ? "{" : ",{") << "\"dispatches\":" << work.dispatches
                  << ",\"port_reads\":" << work.portReads << ",\"port_writes\":" << work.portWrites
                  << ",\"returned_wait\":" << work.returnedWait << ",\"consumed_bytes\":" << work.consumedBytes
                  << ",\"latched\":" << (latched ? "true" : "false")
                  << ",\"timer_dispatches\":" << work.timerWork.dispatches
                  << ",\"direct_timers\":[" << work.timerWork.timers[0] << ',' << work.timerWork.timers[1] << ',' << work.timerWork.timers[2]
                  << "],\"events\":[";
        for (std::size_t eventIndex = 0; eventIndex < work.events.size(); ++eventIndex) {
            const auto& event = work.events[eventIndex];
            std::cout << (eventIndex == 0 ? "{" : ",{") << "\"selected_voice\":";
            if (event.selectedVoice < 0) {
                std::cout << "null";
            } else {
                std::cout << event.selectedVoice;
            }
            std::cout << ",\"parser_dispatches\":" << event.parserDispatches
                      << ",\"handler_dispatches\":" << event.handlerDispatches
                      << ",\"scan_dispatches\":" << event.scanDispatches
                      << ",\"delta_dispatches\":" << event.deltaDispatches
                      << ",\"event_start_dispatches\":" << event.eventStartDispatches
                      << ",\"frequency_start_dispatches\":" << event.frequencyStartDispatches << '}';
        }
        std::cout << "],\"blocks\":[";
        for (std::size_t blockIndex = 0; blockIndex < work.blocks.size(); ++blockIndex) {
            const auto& block = work.blocks[blockIndex];
            std::cout << (blockIndex == 0 ? "{" : ",{") << "\"kind\":\"" << kinds[static_cast<std::size_t>(block.kind)]
                      << "\",\"dispatches\":" << block.dispatches << ",\"voice\":" << block.voice << '}';
        }
        std::cout << "]}";
    }
    std::cin >> std::ws;
    if (!std::cin.eof()) {
        throw std::runtime_error("unexpected trailing key-off work input");
    }
    std::cout << "]\n";
}

void traceSoundBlasterNoteOnWork() {
    constexpr std::size_t maximumCases = 1024;
    constexpr std::size_t maximumEvents = 32;
    constexpr std::array<const char*, 4> kinds{"plain", "frequency_low", "frequency_high", "carrier_level"};
    const auto readBounded = [](const std::uint64_t maximum) {
        std::uint64_t value = 0;
        if (!(std::cin >> value) || value > maximum) {
            throw std::runtime_error("invalid bounded note-on work input");
        }
        return value;
    };
    const auto readArray = [&readBounded](auto& values) {
        for (auto& value : values) {
            using Value = std::decay_t<decltype(value)>;
            value = static_cast<Value>(readBounded(std::numeric_limits<Value>::max()));
        }
    };
    const auto count = readBounded(maximumCases);
    if (count == 0) {
        throw std::runtime_error("empty note-on work batch");
    }
    std::cout << '[';
    for (std::size_t index = 0; index < count; ++index) {
        const auto suppressed = readBounded(1);
        const auto latched = readBounded(1);
        stellar7::SoundBlasterNoteOnState state{};
        readArray(state.directTimers);
        readArray(state.channels);
        readArray(state.sounding);
        readArray(state.tuning);
        readArray(state.channelEnabled);
        readArray(state.channelVoiceCursors);
        const auto eventCount = readBounded(maximumEvents);
        std::vector<stellar7::SoundBlasterExplicitNoteOn> events;
        for (std::size_t eventIndex = 0; eventIndex < eventCount; ++eventIndex) {
            std::array<std::uint8_t, 4> event{};
            readArray(event);
            events.push_back({event[0], event[1], event[2], event[3]});
        }
        const auto work = stellar7::soundBlasterExplicitNoteOnWork(state, events,
            suppressed ? stellar7::SoundBlasterFrequencyGate::Suppressed : stellar7::SoundBlasterFrequencyGate::Open,
            latched ? stellar7::SoundBlasterLevelLatch::Latched : stellar7::SoundBlasterLevelLatch::Unlatched);
        std::cout << (index == 0 ? "{" : ",{") << "\"dispatches\":" << work.dispatches
                  << ",\"port_reads\":" << work.portReads << ",\"port_writes\":" << work.portWrites
                  << ",\"returned_wait\":" << work.returnedWait << ",\"consumed_bytes\":" << work.consumedBytes
                  << ",\"direct_timers\":[" << work.timerWork.timers[0] << ',' << work.timerWork.timers[1] << ',' << work.timerWork.timers[2]
                  << "],\"channel_voice_cursors\":[";
        for (std::size_t channel = 0; channel < work.channelVoiceCursors.size(); ++channel) {
            std::cout << (channel == 0 ? "" : ",") << static_cast<unsigned>(work.channelVoiceCursors[channel]);
        }
        std::cout << "],\"events\":[";
        for (std::size_t eventIndex = 0; eventIndex < work.events.size(); ++eventIndex) {
            const auto& event = work.events[eventIndex];
            std::cout << (eventIndex == 0 ? "{" : ",{") << "\"selected_voice\":";
            if (event.selectedVoice < 0) {
                std::cout << "null";
            } else {
                std::cout << event.selectedVoice;
            }
            std::cout << ",\"event_start_dispatches\":" << event.eventStartDispatches
                      << ",\"frequency_start_dispatches\":" << event.frequencyStartDispatches << '}';
        }
        std::cout << "],\"blocks\":[";
        for (std::size_t blockIndex = 0; blockIndex < work.blocks.size(); ++blockIndex) {
            const auto& block = work.blocks[blockIndex];
            std::cout << (blockIndex == 0 ? "{" : ",{") << "\"kind\":\"" << kinds[static_cast<std::size_t>(block.kind)]
                      << "\",\"dispatches\":" << block.dispatches << ",\"voice\":" << block.voice << '}';
        }
        std::cout << "]}";
    }
    std::cin >> std::ws;
    if (!std::cin.eof()) {
        throw std::runtime_error("unexpected trailing note-on work input");
    }
    std::cout << "]\n";
}

} // namespace

bool runSoundBlasterWorkCommand(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--trace-sbl-keyoff-work") {
        traceSoundBlasterKeyOffWork();
        return true;
    }
    if (argc == 2 && std::string(argv[1]) == "--trace-sbl-noteon-work") {
        traceSoundBlasterNoteOnWork();
        return true;
    }
    if (argc == 7 && std::string(argv[1]) == "--trace-sbl-direct-timer-work") {
        const std::string gate(argv[2]);
        const std::string latch(argv[3]);
        if (gate != "open" && gate != "suppressed") {
            throw std::runtime_error("frequency gate must be open or suppressed");
        }
        if (latch != "unlatched" && latch != "latched") {
            throw std::runtime_error("level latch must be unlatched or latched");
        }
        std::array<std::uint16_t, 3> timers{};
        for (std::size_t index = 0; index < timers.size(); ++index) {
            const auto argument = index + 4;
            std::size_t consumed = 0;
            const auto value = std::stoull(argv[argument], &consumed, 0);
            if (argv[argument][consumed] != '\0' || value > std::numeric_limits<std::uint16_t>::max()) {
                throw std::runtime_error("direct timer outside unsigned16 width");
            }
            timers[index] = static_cast<std::uint16_t>(value);
        }
        const auto work = stellar7::soundBlasterDirectTimerWork(timers,
            gate == "open" ? stellar7::SoundBlasterFrequencyGate::Open : stellar7::SoundBlasterFrequencyGate::Suppressed,
            latch == "latched" ? stellar7::SoundBlasterLevelLatch::Latched : stellar7::SoundBlasterLevelLatch::Unlatched);
        std::cout << "{\"base_dispatches\":" << work.dispatches << ",\"port_reads\":" << work.portReads
                  << ",\"port_writes\":" << work.portWrites << ",\"expired_mask\":" << static_cast<unsigned>(work.expiredMask)
                  << ",\"direct_timers\":[" << work.timers[0] << ',' << work.timers[1] << ',' << work.timers[2]
                  << "],\"scope\":\"Operation8 direct-timer helper including RET, not caller CALL; open gate requires untuned voices; hardware/PIC delays excluded\"}\n";
        return true;
    }
    if (argc == 2 && std::string(argv[1]) == "--trace-sbl-opl-work") {
        const auto work = stellar7::soundBlasterOplWork();
        std::cout << "{\"base_dispatches\":" << work.dispatches
                  << ",\"address_dispatches\":" << work.addressDispatches
                  << ",\"data_dispatches\":" << work.dataDispatches
                  << ",\"port_reads\":" << work.portReads
                  << ",\"port_writes\":" << work.portWrites
                  << ",\"scope\":\"Resident OPL writer includes RET, excludes caller CALL; hardware delays and scheduler charges require CPU-budget phase\"}\n";
        return true;
    }
    if (argc == 5 && std::string(argv[1]) == "--trace-sbl-wait-work") {
        std::array<std::uint16_t, 3> timers{};
        for (std::size_t index = 0; index < timers.size(); ++index) {
            const auto argument = index + 2;
            std::size_t consumed = 0;
            const auto value = std::stoull(argv[argument], &consumed, 0);
            if (argv[argument][consumed] != '\0' || value > std::numeric_limits<std::uint16_t>::max()) {
                throw std::runtime_error("direct timer outside unsigned16 width");
            }
            timers[index] = static_cast<std::uint16_t>(value);
        }
        const auto dispatches = stellar7::soundBlasterWaitDispatches(timers);
        std::cout << "{\"base_dispatches\":" << dispatches
                  << ",\"scope\":\"Active unended mode0 sequence, positive wait, no fade/expired direct timers; includes callback RETF, excludes IRQ CALLF and device/scheduler work\"}\n";
        return true;
    }
    return false;
}

} // namespace stellar7::diagnostics
