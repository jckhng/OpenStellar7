#pragma once

#include "stellar7/audio_work.hpp"

namespace stellar7 {

inline constexpr std::array<std::uint8_t, 8> startupLogoDirectRecord{0x02, 0x50, 0x1e, 0, 0x46, 0x1a, 0xff, 0xff};
inline constexpr std::uint16_t gameplayPrimaryFireEvent = 14;
// Immutable DS1C4E; selected sound4 uses program54, note0 and timer12.
inline constexpr std::array<std::uint8_t, 8> gameplayPrimaryFireDirectRecord{0x36, 0, 0x0c, 0, 0x39, 0x0e, 2, 0};
// DS1C66 is event17's first word, also event14's caller-record+18h.
inline constexpr std::uint16_t gameplayPrimaryFireCallerVolume = 40;
inline constexpr std::uint16_t gameplayFuelBayEvent = 1;
//Immutable DS1BE6; caller+18h DS1BFE aliases the owned instrument bank.
inline constexpr std::array<std::uint8_t,8> gameplayFuelBayDirectRecord{0x1d,0,0x50,0,0x19,1,5,0};
inline constexpr std::uint16_t gameplayFuelBayCallerVolume = 32;
inline constexpr std::uint16_t gameplayEnemyType17Event = 2;
inline constexpr std::uint16_t gameplayEnemyType20Event = 15;
inline constexpr std::array<std::uint8_t, 8> gameplayEnemyType17DirectRecord{0x34, 0, 0x3c, 0, 0x1a, 2, 0x0d, 0};
inline constexpr std::array<std::uint8_t, 8> gameplayEnemyType20DirectRecord{0x2a, 0, 0x28, 0, 0x3a, 0x0f, 1, 0};
// Immutable event5/event18 first words, read through each caller record+18h.
inline constexpr std::uint16_t gameplayEnemyType17CallerVolume = 33;
inline constexpr std::uint16_t gameplayEnemyType20CallerVolume = 300;
inline constexpr std::uint16_t gameplayPlayerHitEvent = 6;
inline constexpr std::array<std::uint8_t, 8> gameplayPlayerHitDirectRecord{0x2c, 0, 0x50, 0, 0x1e, 6, 0x0c, 0};
inline constexpr std::uint16_t gameplayPlayerHitCallerVolume = 0x2724;
inline constexpr std::uint16_t gameplayPlayerCombatEvent = 3;
// Immutable DS1BF6; source-selected sound4 uses program31, note0 and timer60.
inline constexpr std::array<std::uint8_t, 8> gameplayPlayerCombatDirectRecord{0x1f, 0, 0x3c, 0, 0x1b, 3, 7, 0};
// DS1C0E is event6's first word, also event3's caller-record+18h.
inline constexpr std::uint16_t gameplayPlayerCombatCallerVolume = 44;
inline constexpr std::uint16_t gameplayTargetImpactEvent = 5;
//Immutable DS1C06 and caller+18h DS1C1E; the wrapped row lies in the owned bank.
inline constexpr std::array<std::uint8_t, 8> gameplayTargetImpactDirectRecord{0x21, 0, 0x3c, 0, 0x1d, 5, 0x0b, 0};
inline constexpr std::uint16_t gameplayTargetImpactCallerVolume = 0x1823;

inline constexpr std::uint16_t gameplaySystemBonusEvent = 24;
//Immutable DS1C9E and caller+18h DS1CB6; wrapped0DC3 lies in the owned bank.
inline constexpr std::array<std::uint8_t,8> gameplaySystemBonusDirectRecord{0x32,0x18,0x50,0,0x52,0x18,6,0};
inline constexpr std::uint16_t gameplaySystemBonusCallerVolume = 0x0c13;

inline constexpr std::uint16_t gameplayArrivalEvent = 17;
//Immutable DS1C66; caller+18h DS1C7E selects owned bank row13C3..1402.
inline constexpr std::array<std::uint8_t,8> gameplayArrivalDirectRecord{0x28,0,0x68,1,0x49,0x11,0xff,0xff};
inline constexpr std::uint16_t gameplayArrivalCallerVolume = 43;

inline constexpr std::uint16_t gameplayGuardianDefeatEvent = 12;
//Immutable DS1C3E; caller+18h DS1C56 selects owned bank row1383..13C2.
inline constexpr std::array<std::uint8_t,8> gameplayGuardianDefeatDirectRecord{0x27,0,0x78,0,0x35,0x0c,4,0};
inline constexpr std::uint16_t gameplayGuardianDefeatCallerVolume = 42;

inline constexpr std::uint16_t gameplaySolidContactEvent = 9;
//Immutable DS1C26; caller+18h DS1C3E selects owned bank row12C3..1302.
inline constexpr std::array<std::uint8_t,8> gameplaySolidContactDirectRecord{0x24,0x27,0x1e,0,0x21,9,9,0};
inline constexpr std::uint16_t gameplaySolidContactCallerVolume = 39;

inline constexpr std::uint16_t gameplayOrdinaryDeathEvent = 18;
//Immutable DS1C6E; caller+18h DS1C86 selects the owned bank volume alias.
inline constexpr std::array<std::uint8_t,8> gameplayOrdinaryDeathDirectRecord{0x2c,1,0x50,0,0x4a,0x12,0x0c,0};
inline constexpr std::uint16_t gameplayOrdinaryDeathCallerVolume = 47;

inline constexpr std::uint16_t gameplayBombEvent = 7;
//Pinned DS1C16; caller+18h DS1C2E reaches the retained menu-song allocation.
inline constexpr std::array<std::uint8_t,8> gameplayBombDirectRecord{0x22,0,0x3c,0,0x1f,7,8,0};
inline constexpr std::uint16_t gameplayBombCallerVolume = 0x1a24;

inline constexpr std::uint16_t gameplaySystemActivateEvent = 8;
//Pinned unpacked DS1C1E, caller+18h DS1C36.
inline constexpr std::array<std::uint8_t,8> gameplaySystemActivateDirectRecord{0x23,0x18,0x0f,0,0x20,8,8,0};
inline constexpr std::uint16_t gameplaySystemActivateCallerVolume = 38;
inline constexpr std::uint16_t gameplaySystemCycleEvent = 16;
//Pinned unpacked DS1C5E, caller+18h DS1C76.
inline constexpr std::array<std::uint8_t,8> gameplaySystemCycleDirectRecord{0x2e,0x24,0x0f,0,0x3b,16,0,0};
inline constexpr std::uint16_t gameplaySystemCycleCallerVolume = 33;

inline constexpr std::uint16_t gameplaySuperPrimaryFireEvent = 0;
//Pinned unpacked DS1BDE; caller+18h DS1BF6.
inline constexpr std::array<std::uint8_t,8> gameplaySuperPrimaryFireDirectRecord{0x31,0x00,0x78,0x00,0x51,0x00,0x03,0x00};
inline constexpr std::uint16_t gameplaySuperPrimaryFireCallerVolume = 0x001f;

inline constexpr std::uint16_t gameplayTurboActivateEvent = 20;
//Pinned unpacked DS1C7E; caller+18h DS1C96.
inline constexpr std::array<std::uint8_t,8> gameplayTurboActivateDirectRecord{0x2b,0x00,0x2c,0x01,0x4e,0x14,0xff,0xff};
inline constexpr std::uint16_t gameplayTurboActivateCallerVolume = 0x0000;

inline constexpr std::uint16_t gameplayShieldDefeatEvent = 21;
//Pinned unpacked DS1C86; caller+18h DS1C9E.
inline constexpr std::array<std::uint8_t,8> gameplayShieldDefeatDirectRecord{0x2f,0x00,0x3c,0x00,0x4f,0x15,0x0c,0x00};
inline constexpr std::uint16_t gameplayShieldDefeatCallerVolume = 0x1832;

inline constexpr std::uint16_t gameplayScannerPulseEvent = 22;
//Pinned unpacked DS1C8E; caller+18h DS1CA6.
inline constexpr std::array<std::uint8_t,8> gameplayScannerPulseDirectRecord{0x30,0x01,0x3c,0x00,0x50,0x16,0xff,0xff};
inline constexpr std::uint16_t gameplayScannerPulseCallerVolume = 0x0035;

inline constexpr std::uint16_t gameplayDecoyEvent = 25;
//Pinned unpacked DS1CA6; caller+18h DS1CBE.
inline constexpr std::array<std::uint8_t,8> gameplayDecoyDirectRecord{0x35,0x00,0x1e,0x00,0x53,0x19,0xff,0xff};
inline constexpr std::uint16_t gameplayDecoyCallerVolume = 0x0c14;

//B4A6 negative Story selectors map to the final two immutable direct records.
inline constexpr std::uint16_t storyFirstCueEvent=27, storySecondCueEvent=28;
inline constexpr std::array<std::uint8_t,8> storyFirstCueDirectRecord{0x13,0x0c,0x1e,0,0x23,0x1b,0xff,0xff};
inline constexpr std::array<std::uint8_t,8> storySecondCueDirectRecord{0x14,0x0c,0x2c,1,0x47,0x1c,0xff,0xff};
inline constexpr std::uint16_t storyCueCallerVolume=0; //Immutable DS1CCE/1CD6.

enum class SoundBlasterDirectCaller { unownedChannel, gameplayPrimaryFire, gameplayEnemyFire, gameplayPlayerHit, gameplayPlayerCombat, gameplayTargetImpact, gameplaySystemBonus, gameplayArrival, gameplayGuardianDefeat, gameplayEnemyType20, gameplaySolidContact, gameplayOrdinaryDeath, gameplaySystemActivate, gameplaySystemCycle, gameplaySuperPrimaryFire, gameplayTurboActivate, gameplayShieldDefeat, gameplayScannerPulse, gameplayDecoy, gameplayBomb, storyFirstCue, storySecondCue, gameplayFuelBay };
// NormalWorkKind::gameplayPrimaryFireEvent owns0710..0717, including CALL B3E6,
// complete driverop12, CLI/STI and caller cleanup. Fire caller excludes these.
// gameplayEnemyFireEvent owns9A1B PUSH/CALL B3E6 and9A1F cleanup, including
// the complete driver call; event selection9A0D..9A18 remains with its caller.
// Only Type17/event2 is admitted. Type20/event15 reaches an unowned volume alias.
// gameplayPlayerHitEvent owns0E7F MOV/0E82 PUSH/0E83 CALL/0E86 MOVSP,BP
// and the complete driver call. Its row must be installed by the resource owner.
// failureExplosionEvent owns748F MOV/7492 PUSH/7493 CALL/7496 ADD SP,2.
// Both event6 callers have four dispatches and share the same driver/owned row.
// gameplayTargetHitEvent owns1238..123F(event6) or127A..1281(event5),
// including their four caller dispatches and complete driver operation. Event6
// reuses its existing immutable volume-row/driver owner.
// gameplayBonusPickupEvent owns9103/9106/9107/910A and complete driver operation.
// gameplayArrivalEvent owns4705/4708/4709/470C and complete driver operation.
// gameplayGuardianDefeatEvent owns11F1/11F4/11F5/11F8 and complete driver operation.
// gameplayPlayerCombatEvent owns0356 MOV/0359 PUSH/035A CALL/035D cleanup
// and the complete driver call; its transient volume aliases the owned bank.

struct SoundBlasterDirectAdvance {
    SoundBlasterCallbackState state;
    std::vector<SoundBlasterWorkBlock> blocks;
};

// Source216E..21D2 and nested callees; excludes216D CLI and21D5 STI.
// The default rejects owned direct channels. gameplayPrimaryFire admits only
// its exact record with the immutable caller+18h volume word; owners persist.
// gameplayEnemyFire admits only the exact event2 record. Event15 is unsupported.
SoundBlasterDirectAdvance soundBlasterDirectAdvance(
    const SoundBlasterCallbackState& incoming, const std::array<std::uint8_t, 8>& record,
    SoundBlasterDirectCaller caller = SoundBlasterDirectCaller::unownedChannel);

// A direct effect may be requested between a repeating MIDI EOF and its next
// callback. The selected handle stays ended until that actual callback runs.
SoundBlasterDirectAdvance soundBlasterPlayableDirectAdvance(const SoundBlasterCallbackState& incoming,
    const std::array<std::uint8_t,8>& record,SoundBlasterDirectCaller caller);

enum class SoundBlasterStopProfile { zeroDirectTimers, retainedDirectTimers };
// Source2D91 and complete2DA0 OPL branch; excludes surrounding CLI/STI.
//The explicit retained profile preserves independent direct countdowns.
SoundBlasterDirectAdvance soundBlasterStopAdvance(const SoundBlasterCallbackState& incoming,
    SoundBlasterStopProfile profile = SoundBlasterStopProfile::zeroDirectTimers);
SoundBlasterCallbackState soundBlasterDeselectState(const SoundBlasterCallbackState& incoming);

}  // namespace stellar7
