#include "stellar7/success_initial_work.hpp"

#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::uint16_t initialPaletteCaller = 0x7069;
constexpr std::uint16_t initialPaletteEntry = 0x705c;
constexpr std::uint16_t initialPaletteReturn = 0x706f;

std::size_t initialPaletteStep(const SuccessControlWork& work) {
    for (std::size_t index = 0; index < work.steps.size(); ++index) {
        const auto& child = work.steps[index].child;
        if (child && child->kind == SuccessControlChildKind::residentPalette
            && child->sourceCaller == initialPaletteCaller && child->sourceEntry == initialPaletteEntry
            && child->sourceReturn == initialPaletteReturn
            && child->arguments[0] == static_cast<std::int32_t>(SuccessPaletteExecution::initial)) {
            return index;
        }
    }
    throw std::logic_error("success entry lacks its initial resident palette boundary");
}
}

SuccessControlWork successInitialEntryOperations(const SuccessControlInput& input) {
    auto work = successEntryOperations(input);
    work.steps.resize(initialPaletteStep(work) + 1);
    work.control.stage = SuccessControlStage::awaitingPalette;
    return work;
}

SuccessControlWork successInitialPaletteReturnOperations(const SuccessControlInput& input,
    const SuccessPaletteClockTiming& returned) {
    const auto initial = initialSuccessPalette(input.control.channel);
    if (input.control.stage != SuccessControlStage::awaitingPalette
        || returned.boundary != SuccessPaletteBoundary::initialUploadComplete
        || returned.state.channel != input.control.channel || returned.state.values != initial.values
        || returned.state.directions != initial.directions) {
        throw std::invalid_argument("success entry suffix requires its completed initial palette");
    }
    //Reuse the exact immutable source plan; return only work after706C.
    auto entry = input;
    entry.control.stage = SuccessControlStage::inactive;
    auto work = successEntryOperations(entry);
    work.steps.erase(work.steps.begin(),work.steps.begin() + initialPaletteStep(work) + 1);
    work.initializedPalette.reset();
    work.paletteInitializationStepOffset.reset();
    return work;
}
} // namespace stellar7
