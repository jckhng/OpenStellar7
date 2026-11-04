#include "stellar7/bomb7_audio_work.hpp"
#include "stellar7/presentation_clock.hpp"
#include "stellar7/resource_codec.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

namespace {
void level2HitLiveChecks(stellar7::PresentationClock&, stellar7::SoundBlasterSynth&,
    stellar7::SoundBlasterClockPhase&);
}
#define main included_level2_music_main
#include "level2_music_fixture.inc"
#undef main

namespace stellar7 {
struct Bomb7AudioTestAccess {
    static std::optional<SoundBlasterBomb7CallerRow> fromMenuSong(const std::vector<std::uint8_t>& payload) {
        return SoundBlasterBomb7CallerRow::fromMenuSong(payload);
    }
};
}

namespace {
void level2HitLiveChecks(PresentationClock&, SoundBlasterSynth&, SoundBlasterClockPhase&) {}

SoundBlasterCallbackState matchingOwner(const SoundBlasterBomb7CallerRow& row, const std::uint8_t index) {
    auto state = fixture();
    state.program->bomb7CallerRow = row;
    state.voices.channels[7] = 14;
    state.voices.directTimers = {100, 2, 60};
    state.velocities[7] = static_cast<std::uint8_t>(2 * index);
    constexpr std::size_t programBytes = 28;
    const auto patch = gameplayBombDirectRecord[0] * programBytes;
    state.program->instrumentBank[patch + 12] = 0;
    state.program->instrumentBank[patch + 8] = 63;
    state.program->instrumentBank[patch + 21] = 63;
    return state;
}

void componentChecks(const std::vector<std::uint8_t>& payload) {
    const auto row = Bomb7AudioTestAccess::fromMenuSong(payload);
    assert(row);
    const auto copied = *row;
    assert(copied == *row);
    const auto separateGeneration = Bomb7AudioTestAccess::fromMenuSong(payload);
    assert(separateGeneration && *separateGeneration != *row);
    for (std::size_t index = 0; index < bomb7CallerRowBytes; ++index) {
        assert(row->values()[index] == payload[bomb7MenuPayloadOffset + index]);
        auto incoming = matchingOwner(*row, static_cast<std::uint8_t>(index));
        const auto before = callbackFields(incoming);
        const auto work = soundBlasterPlayableDirectAdvance(incoming, gameplayBombDirectRecord,
            SoundBlasterDirectCaller::gameplayBomb);
        assert(callbackFields(incoming) == before);
        assert(work.state.program->bomb7CallerRow == incoming.program->bomb7CallerRow);
        assert(work.state.program->instrumentBank == incoming.program->instrumentBank);
        assert(work.state.program->volumeTable == incoming.program->volumeTable);
        assert(work.state.position == incoming.position && work.state.wait == incoming.wait);
        assert(work.state.voices.directTimers == (std::array<std::uint16_t, 3>{100, 60, 60}));
        assert(work.state.program->channelPrograms[14] == gameplayBombDirectRecord[0]);
        //Voice7 carrier54: first patch write, then the old-owner caller-volume refresh.
        std::vector<std::uint8_t> carrierWrites;
        for (const auto& block : work.blocks) {
            if (block.kind == SoundBlasterWorkKind::registerWrite && block.registerNumber == 0x54) {
                carrierWrites.push_back(block.value);
            }
        }
        assert(carrierWrites.size() >= 2);
        assert(carrierWrites[1] == static_cast<std::uint8_t>(63 - row->values()[index]));
        rejects([&] { soundBlasterDirectAdvance(incoming, gameplayBombDirectRecord, SoundBlasterDirectCaller::gameplayBomb); });
    }
    auto incoming = matchingOwner(*row, 52);
    auto changed = gameplayBombDirectRecord;
    ++changed[1];
    rejects([&] { soundBlasterPlayableDirectAdvance(incoming, changed, SoundBlasterDirectCaller::gameplayBomb); });
    auto faded = incoming;
    faded.fade = 1;
    rejects([&] { soundBlasterPlayableDirectAdvance(faded, gameplayBombDirectRecord, SoundBlasterDirectCaller::gameplayBomb); });
    auto missing = incoming;
    missing.program->bomb7CallerRow.reset();
    rejects([&] { soundBlasterPlayableDirectAdvance(missing, gameplayBombDirectRecord, SoundBlasterDirectCaller::gameplayBomb); });
    auto unsupported = incoming;
    unsupported.velocities[7] = 255;
    rejects([&] { soundBlasterPlayableDirectAdvance(unsupported, gameplayBombDirectRecord, SoundBlasterDirectCaller::gameplayBomb); });
    auto eof = incoming;
    eof.active = false;
    eof.program->handleStatus = 0xffff;
    const auto betweenCallbacks = soundBlasterPlayableDirectAdvance(eof, gameplayBombDirectRecord,
        SoundBlasterDirectCaller::gameplayBomb);
    assert(!betweenCallbacks.state.active && betweenCallbacks.state.program->handleStatus == 0xffff);
    assert(betweenCallbacks.state.program->bomb7CallerRow == row);

    auto malformed = payload;
    malformed.pop_back();
    assert(!Bomb7AudioTestAccess::fromMenuSong(malformed));
    malformed = payload;
    malformed.front() = 0;
    assert(!Bomb7AudioTestAccess::fromMenuSong(malformed));
}
}

int main(const int argc, char** argv) {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try {
        if (argc != 2) { throw std::invalid_argument("requires supplied resource directory"); }
        std::ifstream input(std::filesystem::path(argv[1]) / "STELLAR.RES", std::ios::binary);
        if (!input) { throw std::invalid_argument("missing STELLAR.RES"); }
        const std::vector<std::uint8_t> resource{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        const auto songs = decodeSoundSequenceGroup(resource, 2);
        assert(songs.size() == 1);
        componentChecks(songs.front());
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "B7 uses its decoded retained menu-song row; strict/owner/fade/index negatives reject\n";
}
