#pragma once

#include "stellar7/level1.hpp"

#include <functional>

namespace stellar7 {

enum class Type6ScannerState { inactive, active };
struct Type6BehaviorContext {
    std::uint16_t& cloakTimer; //Canonical actor word+55, not952C's stack miss gate.
    std::uint8_t levelIndex;
    Type6ScannerState scanner;
};

// Source 932C class-1 Type 5/6 and94DA class-2 Type8/9 callbacks.
// The caller owns the physical actor record and projectile allocation.
void advanceType5Behavior(Level1Entity& actor, StellarRng& rng, std::uint16_t levelNumber,
    const std::function<bool(Level1Entity&)>& fire);
void advanceType8Behavior(Level1Entity& actor,
    const std::function<bool(Level1Entity&)>& fire);
void advanceType9Behavior(Level1Entity& actor,
    const std::function<bool(Level1Entity&)>& fire);
void advanceType6Behavior(Level1Entity& actor, StellarRng& rng,
    Type6BehaviorContext context, const std::function<bool(Level1Entity&)>& fire);
//925E's actor visit, after9158 activation or918E expiry; caller owns12D6.
void applyType6Scanner(Level1Entity& actor, Type6ScannerState scanner);

}  // namespace stellar7
