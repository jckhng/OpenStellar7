#include "stellar7/gameplay.hpp"
#include "stellar7/combat_scene_work.hpp"
#include "stellar7/radar_work.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::size_t projectileBase=32;
constexpr std::size_t effectBase=40;

std::vector<Level1ObjectRef> identities(const std::vector<std::size_t>& keys) {
    std::vector<Level1ObjectRef> result;
    for(std::size_t index=0;index<keys.size();++index) {
        const auto key=keys[index];
        if(key<projectileBase) {result.push_back({Level1PresentationKind::entity,index});}
        else if(key>=projectileBase && key<effectBase) {result.push_back({Level1PresentationKind::projectile,key-projectileBase});}
        else if(key>=effectBase && key<48) {result.push_back({Level1PresentationKind::deathEffect,key-effectBase});}
        else {throw std::invalid_argument("Level2 combat callback lost its physical identity");}
    }
    return result;
}
}

void GameplayModel::commitRecoveredLevel2CombatMain(const Level1CombatSceneWork& work) {
    if(phase_!=GameplayPhase::level2 || activeCampaignLevel_!=1 || !level2PrefixGuards_
        || !level2HasAdvanced_ || level2TailConsumed_ || work.rngAfterMain || !work.beams.empty()) {
        throw std::invalid_argument("Level2 combat main requires its completed source prefix");
    }
    const auto refs=identities(recoveredLevel2RenderKeys());
    std::vector<Level1ObjectRef> allocations;
    for(const auto index:level2AllocationListOrder_) {allocations.push_back(refs.at(index));}
    auto entities=level2Entities_;auto effects=level2DeathEffectPool_;auto active=level2DeathEffectPoolActive_;
    std::array<bool,Level1State::deathEffectPoolSize> selected{};
    std::size_t retired=0;
    for(const auto& observation:work.effects) {
        const auto slot=observation.poolSlot;
        if(slot>=selected.size() || selected[slot] || observation.ordinal>=level2ActiveListOrder_.size()) {
            throw std::invalid_argument("Level2 main callback has an invalid FC cursor");
        }
        selected[slot]=true;
        const auto index=level2ActiveListOrder_[observation.ordinal];
        if(refs.at(index)!=Level1ObjectRef{Level1PresentationKind::deathEffect,slot} || !active[slot]) {
            throw std::invalid_argument("Level2 main callback differs from the current OLD FC identity");
        }
        auto& actor=entities.at(index);const auto& returned=observation.work;
        const auto age=actor.stateBytes4dTo52[1];const auto end=actor.stateBytes4dTo52[4];
        const auto terminal=static_cast<unsigned>(age)+1U>=end;
        if(actor.type!=-1 || actor.stateBytes4dTo52[0]!=252 || age==0 || end==0 || age>=end
            || returned.retired!=terminal || returned.removal.has_value()!=terminal
            || (returned.state.flags!=0x88 && returned.state.flags!=0x8c)
            || returned.state.projectedRadius!=0
            || (terminal ? returned.state.age!=0 || returned.state.endAge!=0 || returned.state.callback!=0
                : returned.state.age!=age+1 || returned.state.endAge!=end || returned.state.callback!=0x0ac8)) {
            throw std::invalid_argument("Level2 main callback differs from the source FC age/retirement writes");
        }
        if(returned.removal) {
            const auto& remove=*returned.removal;
            const Level1ObjectRef identity{Level1PresentationKind::deathEffect,slot};
            const auto found=std::find(allocations.begin(),allocations.end(),identity);
            if(found==allocations.end() || remove.removedIndex!=static_cast<std::size_t>(found-allocations.begin())
                || remove.lastIndex+1!=allocations.size()) {
                throw std::invalid_argument("Level2 main callback lost sequential481A identity");
            }
            allocations[remove.removedIndex]=allocations.back();allocations.pop_back();++retired;
        }
        actor.flags=returned.state.flags;actor.stateBytes4dTo52[1]=returned.state.age;
        actor.stateBytes4dTo52[4]=returned.state.endAge;actor.animatedModelPresenterEnabled=false;
        actor.renderProfile=terminal?WorldActorRenderProfile::noHandles:WorldActorRenderProfile::bitmapEffectCallback;
        effects[slot]=actor;active[slot]=!terminal;
    }
    for(const auto index:level2ActiveListOrder_) {
        const auto ref=refs.at(index);
        if(ref.kind==Level1PresentationKind::deathEffect && !selected[ref.index]) {
            throw std::invalid_argument("Level2 main callback omitted an OLD FC record");
        }
    }
    if(allocations!=work.allocationOrderAfterMain) {
        throw std::invalid_argument("Level2 main callback registry differs from its sequential removals");
    }
    std::vector<std::size_t> semantic;
    for(const auto ref:allocations) {
        const auto found=std::find(refs.begin(),refs.end(),ref);
        if(found==refs.end()) {throw std::logic_error("Level2 main lost a physical registry record");}
        semantic.push_back(static_cast<std::size_t>(found-refs.begin()));
    }
    level2Entities_=std::move(entities);level2DeathEffectPool_=std::move(effects);level2DeathEffectPoolActive_=active;
    level2AllocationListOrder_=semantic;level2ObjectListOrder_=std::move(semantic);
    if(retired!=0) {level2Visibility_.removalSucceeded();level2AllocationRevision_+=retired;}
}

void GameplayModel::commitRecoveredLevel2CombatRadar(const RadarWork& work) {
    if(phase_!=GameplayPhase::level2 || activeCampaignLevel_!=1 || !level2PrefixGuards_
        || !level2HasAdvanced_ || level2TailConsumed_ || !work.cpuWorkQualified
        || work.actors.size()!=level2ActiveListOrder_.size()) {
        throw std::invalid_argument("Level2 combat radar requires its ordered source callback return");
    }
    const auto keys=recoveredLevel2RenderKeys();
    for(std::size_t ordinal=0;ordinal<work.actors.size();++ordinal) {
        const auto index=level2ActiveListOrder_[ordinal];const auto key=keys.at(index);
        const auto expected=level2Entities_.at(index).flags;
        if(work.actors[ordinal].flags!=expected || work.actors[ordinal].projectedY!=0) {
            throw std::invalid_argument("Level2 radar did not restore the source main flags/clearY");
        }
        if(key>=effectBase && level2Entities_[index].type!=-1) {
            throw std::invalid_argument("Level2 radar lost its FC physical key");
        }
    }
    //Radar698A restores incoming main flags. Projection/cache is owned by RadarState.
}
} // namespace stellar7
