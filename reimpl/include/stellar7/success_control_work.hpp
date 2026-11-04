#pragma once

#include "stellar7/success_palette_clock.hpp"
#include <optional>

namespace stellar7 {

enum class SuccessControlStage { inactive, polling, awaitingPalette, finished };
struct SuccessControlState {
    SuccessControlStage stage = SuccessControlStage::inactive;
    SuccessPaletteChannel channel = SuccessPaletteChannel::green;
    SuccessPaletteCompletionSource completion = SuccessPaletteCompletionSource::soundHandle;
};

//Call-time observations from the existing page owner, never new page allocations.
struct SuccessControlPages {
    std::uint16_t back;
    std::uint16_t front;
    std::uint16_t cache;
    std::uint16_t draw;
    std::uint16_t copy;
};
struct SuccessControlInput {
    SuccessControlState control;
    SuccessControlPages pages;
    std::uint16_t levelIndex = 0;
    bool graphicsEnabled = true;
};

enum class SuccessControlChildKind { entryStop, activateSuccess, cockpit,
    planarCopy, sight, radar, residentPalette, countdownStore, returnStop };
struct SuccessControlChild {
    SuccessControlChildKind kind;
    std::uint16_t sourceCaller;
    std::uint16_t sourceSegment;
    std::uint16_t sourceEntry;
    std::uint16_t sourceReturn;
    //Copy:x,y,width,height,COPY,DRAW. Audio:channel,repeat,resource-word.
    std::array<std::int32_t,6> arguments{};
    //False means a retained child/provider admission still must be qualified;
    //it never represents zero elapsed guest work or an untimed synth trigger.
    bool guestElapsedQualified = false;
};
enum class SuccessControlStoreKind { warningClear, drawPage, copyPage };
struct SuccessControlStore {
    SuccessControlStoreKind kind;
    std::uint16_t value;
    std::uint16_t sourcePc;
};
struct SuccessControlStep {
    std::vector<NormalWorkOperation> operations;
    std::optional<SuccessControlChild> child;
    //Applied after these source instructions, before the next child/step.
    std::vector<SuccessControlStore> stores;
};
struct SuccessControlWork {
    SuccessControlState control;
    SuccessControlPages pages;
    std::vector<SuccessControlStep> steps;
    std::optional<SuccessPaletteState> initializedPalette;
    //Initialization has completed before the step at this offset.
    std::optional<std::size_t> paletteInitializationStepOffset;
};

//Whole graphics-enabled6F52 entry through7072, before7078. Child work excluded.
//No model/ground/actor/RNG update, source page flip, blackout or resource reload.
SuccessControlWork successEntryOperations(const SuccessControlInput& input);
//One source7078 poll; qualified resident child owns the branch/loop/upload once.
SuccessControlWork successPollOperations(const SuccessControlInput& input);
//Consume that actual child's return;7204 stop and7209..720E epilogue on completion.
SuccessControlWork successPaletteReturnOperations(const SuccessControlInput& input,
    const SuccessPaletteClockTiming& paletteReturn);

} // namespace stellar7
