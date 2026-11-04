#pragma once

#include "stellar7/cockpit_state.hpp"
#include "stellar7/indexed_pages.hpp"

#include <string>

namespace stellar7 {

enum class ScoreAwardDomain { ordinaryTank, initialOrdinary, systemBonus, guardianDefeat, transitionDisplay, level2Constructor, level2PeriodicOrdinary };
enum class ScoreFontSelection { score, primary };
struct ScoreFontProviderRead {
    std::size_t operationOffset;
    ScoreFontSelection selection;
    bool guestElapsedQualified = false;
};

struct ScoreWork {
    std::uint32_t score;
    // Physical roles are preserved; selectors are explicit source write receipts.
    CockpitPageState pages;
    std::uint16_t drawPage;
    std::uint16_t copyPage;
    std::uint8_t textColor;
    std::string text;
    std::vector<NormalWorkOperation> operations;
    std::vector<IndexedPageEffect> effects;
    std::vector<ScoreFontProviderRead> fontSelections;
};

//7970..7A1B includes7A1C/CRT decimal, fill/text/copy; excludes outer caller/IRQs.
//Native4848 font-selection bodies remain explicit providers, not zero-time claims.
//Ordinary Type1 award20/difficulty0, nonnegative signed32 score without overflow.
//Full display, style1 and color12E9=239 are the retained HUD drawing contract.
ScoreWork scoreOperations(std::uint32_t incomingScore, std::int16_t award,
    std::uint16_t difficulty, const BitmapFont& scoreFont, const CockpitPageState& pages,
    ScoreAwardDomain domain = ScoreAwardDomain::ordinaryTank);
//initialOrdinary additionally admits immutable Type11 award50. Fixed29 has no
//source7970 call and must never be represented as award0 or award5.
//systemBonus explicitly admits only the90F2 pickup award100; old defaults stay strict.
//level2Constructor admits Type8 award25 and Type13 award10. Type30 skips7970.
//level2PeriodicOrdinary additionally admits actual Type1 award20/Type4 award30.

} // namespace stellar7
