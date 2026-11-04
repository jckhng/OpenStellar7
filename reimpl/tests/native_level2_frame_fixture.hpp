#pragma once
#include "stellar7/native_session.hpp"
#include <memory>
#include <filesystem>
#include <string>
namespace stellar7::test {
//Controlled inactive Level1 success plus live-clock transition fixture.
//Returns before genuine first01D0; does not establish natural menu/success ancestry.
std::unique_ptr<NativeGameplaySession> makeRecoveredLevel2PreloopSession(const std::filesystem::path& resourceDirectory);
// Same live source owners through first display, followed by a controlled wave clear.
std::unique_ptr<NativeGameplaySession> makePlayableLevel2ClearedWaveSession(const std::filesystem::path& resourceDirectory);
std::unique_ptr<NativeGameplaySession> makePlayableLevel2Session(const std::filesystem::path& resourceDirectory);
enum class PlayableScoreFixture { rankMiss, rankHit };
enum class PlayableMouseFixture { enabled, disabled };
//Controlled final-bank loader/ending boundary on the same live source-clock owners.
std::unique_ptr<NativeGameplaySession> makePlayableEndingSession(const std::filesystem::path& resourceDirectory,
    PlayableScoreFixture scores = PlayableScoreFixture::rankMiss, std::shared_ptr<HighscoreStore> store = {});
//Controlled lethal component boundary after retained startup/Level2 frames.
std::unique_ptr<NativeGameplaySession> makePlayableLethalSession(const std::filesystem::path& resourceDirectory,
    PlayableScoreFixture scores, std::shared_ptr<HighscoreStore> store = {},
    NativePointerCapability pointerCapability = NativePointerCapability::unavailable,
    PlayableMouseFixture mouse = PlayableMouseFixture::enabled);
//Controlled lethal entry with no Continue stock; destruction must return directly to its title caller.
std::unique_ptr<NativeGameplaySession> makePlayableExhaustedSession(const std::filesystem::path& resourceDirectory,
    PlayableScoreFixture scores, std::shared_ptr<HighscoreStore> store = {});
std::string playableSuccessDiagnostic(const GameplayModel& model);
enum class PlayableGuardianEntry { clearedWave, lateFight };
//Controlled actual-bank boundaries on retained native owners; no preceding combat ancestry.
std::unique_ptr<NativeGameplaySession> makePlayableGuardianSession(const std::filesystem::path& resourceDirectory,
    std::uint8_t level, PlayableGuardianEntry entry);
// Same controlled entry with explicit native menu/device capability ownership.
std::unique_ptr<NativeGameplaySession> makePlayableGuardianSession(const std::filesystem::path& resourceDirectory,
    std::uint8_t level, PlayableGuardianEntry entry, NativeMenuFeatures menuFeatures);
std::unique_ptr<NativeGameplaySession> makePlayableBlueWarpSession(const std::filesystem::path& resourceDirectory,
    std::uint8_t level);
bool hasNativeBluePalette(const NativeGameplaySession& session);
//Controlled contact pose on the bank's own fuel station; audio/pages/clock retained.
std::unique_ptr<NativeGameplaySession> makePlayableFuelBaySession(const std::filesystem::path& resourceDirectory,std::uint8_t level,std::uint32_t distance=0);
//Source503C,0086/0088/008A and129C, observed without a product getter.
std::array<std::int64_t,5> observeFuelBay(const GameplayModel& model);
//Controlled third-kill producer at the player; the native pages/synth/clock remain owned.
std::unique_ptr<NativeGameplaySession> makePlayableBonusPickupSession(const std::filesystem::path& resourceDirectory,
    std::uint8_t level,std::uint8_t sourceType);
enum class DestructionTarget {ordinary,guardian,survivor};
enum class DestructionPool {available,full};
// Controlled incoming lethal shot; no preceding combat ancestry is claimed.
std::unique_ptr<NativeGameplaySession> makeDestructionSession(const std::filesystem::path& root,
    std::uint8_t level,DestructionTarget target,int distance,int height,DestructionPool pool);
void prepareNextDestruction(NativeGameplaySession& session,DestructionTarget target,int distance,int height);
//Missing periodic-only model rejects before either source first display or playable activation.
void verifyMissingWeightedPlayableAdmission(const std::filesystem::path& resourceDirectory,std::uint8_t type);
} //namespace stellar7::test
