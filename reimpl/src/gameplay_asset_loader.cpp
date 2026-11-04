#include "gameplay_asset_loader.hpp"
#include "resource_file.hpp"
#include "stellar7/audio.hpp"
#include "stellar7/cockpit_state.hpp"
#include "stellar7/ending_scene.hpp"
#include "stellar7/resource_codec.hpp"
#include <array>
#include <iomanip>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace stellar7 {

stellar7::GameplayAssets loadGameplayAssets(
    const std::filesystem::path& dataDirectory,
    const std::string& palettePath,
    const std::string& level3SuccessSequencePath,
    const std::optional<stellar7::HighscoreTable>& profileTable,
    const stellar7::NativeDataFiles* resourceFiles
) {
    const std::filesystem::path root(dataDirectory);
    const auto resourcePath=[&](const std::string& name) {
        return resourceFiles?resourceFiles->file(name):root/name;
    };
    const auto cockpitResource = readFile(resourcePath("COCKPIT.RES"));
    const auto mainResource = readFile(resourcePath("STELLAR.RES"));
    auto fonts = stellar7::decodeFontResources(mainResource);
    if (fonts.size() < 2) {
        throw std::runtime_error("STELLAR.RES lacks the cockpit score font");
    }
    std::vector<std::vector<std::uint8_t>> levelResources;
    std::vector<stellar7::CampaignLevelAssets> campaignLevels;
    levelResources.reserve(7);
    campaignLevels.reserve(7);
    for (std::size_t levelIndex = 0; levelIndex < 7; ++levelIndex) {
        const std::string resourceName =
            "LEVEL" + std::to_string(levelIndex + 1) + ".RES";
        levelResources.push_back(readFile(resourcePath(resourceName)));
        auto bitmapGroups = stellar7::decodeBitmapGroups(
            levelResources.back()
        );
        if (bitmapGroups.empty()) {
            throw std::runtime_error(
                resourceName + " lacks a bitmap group"
            );
        }
        auto songs = stellar7::decodeTaggedPayloads(
            levelResources.back(),
            std::array<char, 4>{'S', 'N', 'G', ':'}
        );
        campaignLevels.push_back({
            resourceName,
            std::move(bitmapGroups),
            songs.empty()
                ? std::vector<std::uint8_t>{}
                : std::move(songs.front()),
            stellar7::decodeFirstModelTableResource(levelResources.back()),
        });
    }

    const auto artResource = readFile(resourcePath("STELART.RES"));
    auto artGroups = stellar7::decodeBitmapGroups(artResource);
    auto uiPalettes = stellar7::decodePalettes(artResource);
    if (artGroups.size() < 5) {
        throw std::runtime_error("STELART.RES lacks gameplay bitmap groups");
    }
    if (uiPalettes.size() < 3) {
        throw std::runtime_error("STELART.RES lacks the UI palette");
    }
    auto cockpitPalette = palettePath.empty()
        ? std::vector<std::uint8_t>{} : readFile(palettePath);
    if (palettePath.empty()) {
        constexpr std::size_t gameplayPaletteIndex = 2;
        constexpr std::size_t vgaPaletteEntries = 256;
        const auto& source = uiPalettes[gameplayPaletteIndex];
        if (source.size() != vgaPaletteEntries) {
            throw std::runtime_error("STELART.RES gameplay palette is incomplete");
        }
        // Invert the codec's lossless six-bit DAC expansion; no captured palette.
        for (const auto color : source) {
            for (const auto component : {color.red, color.green, color.blue}) {
                cockpitPalette.push_back(static_cast<std::uint8_t>(component >> 2));
            }
        }
    }
    const auto& originalCockpitPalette = uiPalettes[2];
    bool cockpitPaletteMatchesResource = cockpitPalette.size() == originalCockpitPalette.size() * 3;
    for (std::size_t color = 0; cockpitPaletteMatchesResource && color < originalCockpitPalette.size(); ++color) {
        const auto source = originalCockpitPalette[color];
        cockpitPaletteMatchesResource = cockpitPalette[color * 3] == (source.red >> 2)
            && cockpitPalette[color * 3 + 1] == (source.green >> 2)
            && cockpitPalette[color * 3 + 2] == (source.blue >> 2);
    }
    auto models = stellar7::decodeFirstModelTableResource(cockpitResource);
    auto levelModels = campaignLevels[0].models;
    auto level2Models = campaignLevels[1].models;
    auto level3Models = campaignLevels[2].models;
    models.insert(
        models.end(),
        std::make_move_iterator(levelModels.begin()),
        std::make_move_iterator(levelModels.end())
    );
    const auto loadIndexedSequence = [](
        const std::filesystem::path& sequenceRoot,
        const std::string& label
    ) {
        std::vector<stellar7::IndexedScreen> frames;
        for (std::size_t index = 0;; ++index) {
            std::ostringstream stemBuilder;
            stemBuilder << "frame_" << std::setfill('0') << std::setw(4)
                        << index;
            const auto stem = stemBuilder.str();
            const auto indexedPath = sequenceRoot / (stem + ".indexed");
            const auto dacPath = sequenceRoot / (stem + ".dac");
            if (
                !std::filesystem::is_regular_file(indexedPath)
                || !std::filesystem::is_regular_file(dacPath)
            ) {
                break;
            }
            auto pixels = readFile(indexedPath.string());
            auto dac = readFile(dacPath.string());
            if (pixels.size() != 320U * 200U || dac.size() != 256U * 3U) {
                throw std::runtime_error(
                    label + " capture frame has an invalid size"
                );
            }
            frames.push_back({
                std::move(pixels),
                stellar7::expandVgaDacPalette(dac),
            });
        }
        if (frames.empty()) {
            throw std::runtime_error(
                label + " capture directory has no frames"
            );
        }
        return frames;
    };
    std::vector<stellar7::IndexedScreen> level3SuccessFrames;
    std::vector<stellar7::IndexedScreen> level3ContactSuccessFrames;
    std::vector<stellar7::IndexedScreen> level3HandoffFrames;
    std::vector<stellar7::IndexedScreen> level3GuardianFrames;
    if (!level3SuccessSequencePath.empty()) {
        const std::filesystem::path sequenceRoot(
            level3SuccessSequencePath
        );
        level3SuccessFrames = loadIndexedSequence(
            sequenceRoot,
            "Level 3 success"
        );
        const auto contactRoot = sequenceRoot / "contact";
        const auto contactSurfaceRoot = sequenceRoot / "contact_surface";
        if (std::filesystem::is_directory(contactSurfaceRoot)) {
            level3ContactSuccessFrames = loadIndexedSequence(
                contactSurfaceRoot,
                "Level 3 synchronized contact surface"
            );
        } else if (std::filesystem::is_directory(contactRoot)) {
            level3ContactSuccessFrames = loadIndexedSequence(
                contactRoot,
                "Level 3 contact success"
            );
        }
        const auto handoffRoot = sequenceRoot / "handoff";
        if (std::filesystem::is_directory(handoffRoot)) {
            level3HandoffFrames = loadIndexedSequence(
                handoffRoot,
                "Level 3 handoff"
            );
        }
        const auto guardianRoot = sequenceRoot / "guardian";
        if (std::filesystem::is_directory(guardianRoot)) {
            level3GuardianFrames = loadIndexedSequence(
                guardianRoot,
                "Level 3 guardian"
            );
        }
    }
    return {
        stellar7::applyVgaDacPalette(
            stellar7::decodeFirstIndexedScreenResource(cockpitResource),
            cockpitPalette
        ),
        {},
        std::move(artGroups),
        std::move(models),
        std::move(uiPalettes.back()),
        stellar7::decodeTaggedPayloads(
            cockpitResource,
            std::array<char, 4>{'S', 'N', 'G', ':'}
        ),
        [&mainResource]() {
            auto resources = stellar7::decodeTaggedPayloads(
                mainResource,
                std::array<char, 4>{'0', '0', '3', ':'}
            );
            return resources.empty()
                ? std::vector<std::uint8_t>{}
                : std::move(resources.front());
        }(),
        {},
        [&mainResource]() {
            auto resources = stellar7::decodeTaggedPayloads(
                mainResource,
                std::array<char, 4>{'S', 'N', 'G', ':'}
            );
            return resources.size() < 2
                ? std::vector<std::uint8_t>{}
                : std::move(resources[1]);
        }(),
        std::move(cockpitPalette),
        true,
        {},
        {},
        std::move(campaignLevels),
        std::move(level3SuccessFrames),
        std::move(level3ContactSuccessFrames),
        std::move(level3HandoffFrames),
        std::move(level3GuardianFrames),
        std::move(level2Models),
        std::move(level3Models),
        false,
        false,
        std::move(fonts[1]),
        [&mainResource]() {
            auto resources = stellar7::decodeTaggedPayloads(
                mainResource, std::array<char, 4>{'S', 'N', 'G', ':'}
            );
            return resources.empty() ? std::vector<std::uint8_t>{} : std::move(resources.front());
        }(),
        stellar7::decodeByteArrays(mainResource),
        {256, 256},
        std::move(fonts[0]),
        profileTable ? std::optional<stellar7::CockpitSettings>{stellar7::decodeCockpitSettings(
            {profileTable->header().begin(),profileTable->header().end()})} : std::filesystem::is_regular_file(root / stellar7::highscoreFileName)
            ? std::optional<stellar7::CockpitSettings>{stellar7::loadCockpitSettings(root)} : std::nullopt,
        [&]() -> std::optional<stellar7::HallOfFameScores> {
            if(!profileTable) {return stellar7::loadHallOfFameScores(root);}
            stellar7::HallOfFameScores scores{};
            for(std::size_t index=0;index<scores.size();++index) {scores[index]=profileTable->entries()[index].score;}
            return scores;
        }(),
        [&mainResource]() {
            const auto drivers = stellar7::decodeTaggedPayloads(mainResource, {'S', 'B', 'L', ':'});
            if (drivers.size() != 1) {
                throw std::runtime_error("native audio requires one Sound Blaster driver resource");
            }
            return stellar7::decodeSoundBlasterVolumeTable(drivers.front());
        }(),
        profileTable ? profileTable : stellar7::loadHighscoreTable(root / stellar7::highscoreFileName),
        [&resourcePath, cockpitPaletteMatchesResource]() -> std::shared_ptr<const stellar7::EndingSceneAssets> {
            //B50E's12C6 is the retained third STELART PAL, not an override bank.
            if (!cockpitPaletteMatchesResource || !std::filesystem::is_regular_file(resourcePath("SCENED.RES"))
                || !std::filesystem::is_regular_file(resourcePath("VOICE.RES"))) {
                return {};
            }
            return std::make_shared<stellar7::EndingSceneAssets>(stellar7::decodeEndingSceneAssets(
                readFile(resourcePath("SCENED.RES")), readFile(resourcePath("VOICE.RES"))));
        }(),
        [&cockpitResource]() {
            auto text=stellar7::decodeTaggedPayloads(cockpitResource,{'T','X','T',':'});
            return text.empty()?std::vector<std::uint8_t>{}:std::move(text.front());
        }(),
    };
}

} // namespace stellar7
