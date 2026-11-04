#pragma once
#include "stellar7/recovered_frontend.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace naturalRoute {
using namespace stellar7;
std::vector<std::uint8_t> read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("missing original resource: " + path.string());
    }
    return {std::istreambuf_iterator<char>(file), {}};
}

GameplayAssets assets(const std::filesystem::path& root) {
    const auto cockpit = read(root / "COCKPIT.RES");
    const auto main = read(root / "STELLAR.RES");
    const auto art = read(root / "STELART.RES");
    GameplayAssets result{};
    result.artGroups = decodeBitmapGroups(art);
    const auto palettes = decodePalettes(art);
    result.uiPalette = palettes.back();
    for (const auto color : palettes.at(2)) {
        for (const auto component : {color.red, color.green, color.blue}) {
            result.cockpitDacPalette.push_back(component >> 2);
        }
    }
    result.cockpit = applyVgaDacPalette(decodeFirstIndexedScreenResource(cockpit), result.cockpitDacPalette);
    const auto fonts = decodeFontResources(main);
    result.titleFont = fonts.at(0);
    result.scoreFont = fonts.at(1);
    result.soundSequences = decodeTaggedPayloads(cockpit, {'S','N','G',':'});
    result.briefingText = decodeTaggedPayloads(cockpit, {'T','X','T',':'}).at(0);
    result.soundBlasterInstrumentResource = decodeTaggedPayloads(main, {'0','0','3',':'}).at(0);
    const auto songs = decodeTaggedPayloads(main, {'S','N','G',':'});
    result.logoMusicSequence = songs.at(0);
    result.presentationMusicSequence = songs.at(1);
    result.soundBlasterVolumeTable = decodeSoundBlasterVolumeTable(decodeTaggedPayloads(main, {'S','B','L',':'}).at(0));
    result.models = decodeFirstModelTableResource(cockpit);
    for (unsigned level = 1; level <= 7; ++level) {
        const auto name = "LEVEL" + std::to_string(level) + ".RES";
        const auto bytes = read(root / name);
        result.campaignLevels.push_back({name, decodeBitmapGroups(bytes),
            decodeTaggedPayloads(bytes, {'S','N','G',':'}).at(0), decodeFirstModelTableResource(bytes)});
    }
    const auto& local = result.campaignLevels.front().models;
    result.models.insert(result.models.end(), local.begin(), local.end());
    result.level2Models=result.campaignLevels.at(1).models;
    result.level3Models=result.campaignLevels.at(2).models;
    result.emulateTargetPaletteTiming=true;
    result.sharedModelRegistry = {256, 256};
    result.modelColorTables = decodeByteArrays(main);
    result.cockpitSettings=loadCockpitSettings(root);
    result.hallOfFameScores=loadHallOfFameScores(root);
    result.highscoreTable=loadHighscoreTable(root/highscoreFileName);
    if (!result.highscoreTable) {throw std::runtime_error("actual FAME.HAL is required");}
    if (std::filesystem::is_regular_file(root/"SCENED.RES") && std::filesystem::is_regular_file(root/"VOICE.RES")) {
        result.endingScene=std::make_shared<const EndingSceneAssets>(
            decodeEndingSceneAssets(read(root/"SCENED.RES"),read(root/"VOICE.RES")));
    }
    return result;
}

}
