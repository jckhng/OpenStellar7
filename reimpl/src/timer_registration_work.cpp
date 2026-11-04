#include "stellar7/timer_registration_work.hpp"

namespace stellar7 {
namespace {

constexpr std::uint16_t callbackPeriod = 4;
constexpr std::uint16_t callbackOffset = 0x0494;
constexpr std::uint16_t callbackSegment = 0x1ebc;
constexpr std::uint16_t callbackMask = 1;

}  // namespace

std::vector<NormalWorkOperation> timerRegistrationOperations() {
    return {
        //26B1..26CA,26D3: installed/free-slot tests and period load.
        {NormalWorkKind::instructions, 14},
        {NormalWorkKind::timerCallbackReload, 1, callbackPeriod}, //26D6
        {NormalWorkKind::timerCallbackCount, 1, callbackPeriod}, //26DA
        {NormalWorkKind::instructions, 1}, //26DE loads callback offset.
        {NormalWorkKind::timerCallbackOffset, 1, callbackOffset}, //26E1
        {NormalWorkKind::instructions, 1}, //26E5 loads callback segment.
        {NormalWorkKind::timerCallbackSegment, 1, callbackSegment}, //26E8
        {NormalWorkKind::cli, 1}, //26EC
        {NormalWorkKind::timerCallbackActivate, 1, callbackMask}, //26ED
        {NormalWorkKind::sti, 1}, //26F1
        {NormalWorkKind::instructions, 6} //26F2..26FA returns slot ordinal1.
    };
}

}  // namespace stellar7
