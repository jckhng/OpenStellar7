#pragma once

#include "stellar7/failure_effect_work.hpp"

namespace stellar7 {
//Same BA88..BC09 child/typed observations as failure effects, explicit ordinary
//viewport159/84 bitmap profile. Caller supplies each transformed world center.
//Live constructor ages1..11/end12 or3*b+14..3*b+16/end3*b+17, b0..4.
//1334=0/radarPass0; main callback ages once. No pool, registry or model is owned.
FailureEffectWork level1CombatEffectOperations(const FailureEffectInput& input,
    const std::vector<IndexedBitmap>& effectFrames);
//Deployment guardian constructor: first13+3*b/end17+3*b, b0..5.
//Same callback and observations; the index31 source NULL alias still executes
//the clipped bitmap header/clip path before retiring the source FC slot.
FailureEffectWork level1GuardianEffectOperations(const FailureEffectInput& input,
    const std::vector<IndexedBitmap>& effectFrames);
}
