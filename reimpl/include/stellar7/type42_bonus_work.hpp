#pragma once

#include "stellar7/resource_codec.hpp"
#include "stellar7/target_hit_work.hpp"

namespace stellar7 {

constexpr std::size_t level1BonusFixedPoolSize = 16;
enum class Level1BonusFixedProvenance { unknown, reset, retained };
struct Level1BonusFixedSlot {
    Level1BonusFixedProvenance provenance = Level1BonusFixedProvenance::unknown;
    std::optional<std::size_t> entityIndex;
    Level1Entity entity{};
    std::uint16_t drawCallback = 0;
};
struct Level1Type42BonusInput {
    Level1Entity source;
    Level1TargetBonusHistory history;
    std::array<Level1BonusFixedSlot, level1BonusFixedPoolSize> fixedSlots{};
    std::size_t nextEntityIndex = 0;
    std::vector<Level1ObjectRef> allocationOrder;
    bool refreshRequested = false;
};
struct Level1Type42BonusReceipt {
    std::size_t poolSlot;
    std::size_t entityIndex;
    std::size_t appendedAllocationIndex;
    bool materializeEntity;
    Level1Entity entity;
    std::uint16_t drawCallback;
    //904C changes+1C to this record's+4F; it never allocates animation storage.
    std::uint8_t inlineSelectorOffset = 0x4f;
};
struct Level1Type42BonusWork {
    Level1TargetBonusHistory history;
    std::optional<Level1Type42BonusReceipt> birth;
    std::vector<Level1ObjectRef> allocationOrder;
    bool refreshRequested;
    std::vector<NormalWorkOperation> operations;
};

//Whole90A4..RET, including904C/4506 and loaded-modelD03C/C05C.
//Type1/FF/class1 emits7; Type11/FF/class3 emits3. Other sources reject.
//Exclude caller11A0/11A1/11A4,1176's FC constructor, score and target retirement.
//No RNG/audio/heap/resource loading occurs. Model42 must already be loaded.
//firstBlockCapacity is the source registry header (qualified44 or native256),
//not the number of decoded model rows.
//Native provider supplies actual45B0 order, known retained/reset bytes and stable
//entity identities. Reset provenance is available only where49F4 actually ran;
//48D4 clears eight fixed records, although4506 scans sixteen. Reject an unknown
//slot when the scan reaches it. Unmaterialized reset storage becomes one new
//canonical entity only on successful allocation; no synthetic active entity.
//Apply birth exactly once, append live49F8 before FC append/target swap;45D2 and
//all old-list membership remain unchanged. Full pool still resets the streak.
//Source writes only: flags/type/radius/position/angles/inline+4F/category/health/
//class/+52/rollVelocity/yawVelocity. Every other typed field remains unchanged.
//Selected draw callback must be0; other retained presenter families stay outside
//this native adapter. Render profile/selector metadata describes+1A/+1C, not a
//second source write. Type42 update/render/contact are separate dependencies.
Level1Type42BonusWork level1Type42BonusOperations(const Level1Type42BonusInput& input,
    std::uint16_t firstBlockCapacity, const Model3d& model42);

} // namespace stellar7
