#pragma once

#include "playable_campaign_route_controls.hpp"

#include <optional>
#include <stdexcept>

namespace naturalBlue {
using namespace stellar7;

enum class Stage { shootF3, collect26, destination };

//Driver observations only. Every control borrows the current retained actor;
//compact indices and pointers are never retained across a native advance.
class Controls {
public:
    GameplayInput operator()(const GameplayModel& model) {
        const auto bank = model.campaignLevel();
        if (bank == finalDestination) {
            stage_ = Stage::destination;
            return {};
        }
        if (bank != firstBank && bank != warpBank) {
            throw std::runtime_error("blue route departed its actual1->4->7 banks");
        }
        if (model.phase() != GameplayPhase::playable) { return {}; }
        if (model.level2Player().position.z != 0 || model.level1State().systemTimers()[jumpSystem] != 0) {
            throw std::runtime_error("blue route requires ordinary ground fire");
        }

        const auto* producer = unique(model, producerType(bank), rewardBehavior);
        const auto* reward = unique(model, blueRewardType, std::nullopt);
        if (!bank_ || *bank_ != bank) {
            const auto rowIndex = bank == firstBank ? firstProducerRow : warpProducerRow;
            const auto row = campaignLevelDefinition(static_cast<std::uint8_t>(bank), 0).table.actors.at(rowIndex);
            if (!producer || reward || producer->stateBytes4dTo52[1] != initialProducerHealth
                || !(producer->position == FixedPosition{campaignCoordinate(row.x), campaignCoordinate(row.y), 0})
                || row.type != firstProducerType) {
                throw std::runtime_error("blue route did not observe its untouched actual F3 constructor");
            }
            bank_ = bank;
            producerSlot_.reset();
            rewardSlot_.reset();
            lastHealth_ = initialProducerHealth;
            stage_ = Stage::shootF3;
        }

        if (reward) {
            if (producer) { throw std::runtime_error("blue26 coexists with its live F3 producer"); }
            stableSlot(model, *reward, rewardSlot_);
            stage_ = Stage::collect26;
            return test::campaignPilotControls(model, *reward, test::CampaignPilotGoal::collect);
        }
        if (!producer || stage_ != Stage::shootF3) {
            throw std::runtime_error("blue route lost its producer before observing actual26");
        }
        stableSlot(model, *producer, producerSlot_);
        const auto health = producer->stateBytes4dTo52[1];
        if (health > lastHealth_) { throw std::runtime_error("natural F3 health unexpectedly increased"); }
        lastHealth_ = health;
        return naturalRoute::approach(model, *producer, test::CampaignPilotGoal::combat);
    }

    Stage stage() const noexcept { return stage_; }
    std::uint8_t producerHealth() const noexcept { return lastHealth_; }

private:
    static constexpr std::size_t firstBank = 0;
    static constexpr std::size_t warpBank = 3;
    static constexpr std::size_t finalDestination = 6;
    static constexpr std::size_t jumpSystem = 2;
    static constexpr std::size_t firstProducerRow = 5;
    static constexpr std::size_t warpProducerRow = 6;
    static constexpr std::int16_t firstProducerType = 29;
    static constexpr std::int16_t blueRewardType = 26;
    static constexpr std::uint8_t fixedCategory = 0xfe;
    static constexpr std::uint8_t rewardBehavior = 0xf3;
    static constexpr std::uint8_t initialProducerHealth = 16;

    static std::int16_t producerType(const std::size_t bank) {
        return static_cast<std::int16_t>(firstProducerType + bank);
    }

    static const Level1Entity* unique(const GameplayModel& model, const std::int16_t type,
        const std::optional<std::uint8_t> behavior) {
        const Level1Entity* found = nullptr;
        for (const auto& actor : model.level2Entities()) {
            if (!actor.active() || actor.type != type || actor.stateBytes4dTo52[0] != fixedCategory
                || (behavior && actor.stateBytes4dTo52[4] != *behavior)) {
                continue;
            }
            if (found) { throw std::runtime_error("blue target has ambiguous retained ownership"); }
            found = &actor;
        }
        return found;
    }

    static void stableSlot(const GameplayModel& model, const Level1Entity& target,
        std::optional<std::size_t>& knownSlot) {
        const auto& presented = model.level2PresentationEntities();
        for (std::size_t index = 0; index < presented.size(); ++index) {
            const auto& actor = presented[index];
            if (!actor.active() || actor.type != target.type
                || actor.stateBytes4dTo52[0] != fixedCategory
                || actor.stateBytes4dTo52[4] != target.stateBytes4dTo52[4]) {
                continue;
            }
            //The existing getter withholds a stale projection after allocations.
            const auto slot = model.level2PresentationPoolSlot(index);
            if (!slot) { continue; }
            if (knownSlot && knownSlot != slot) {
                throw std::runtime_error("blue target changed its physical FE owner");
            }
            knownSlot = slot;
        }
    }

    std::optional<std::size_t> bank_;
    std::optional<std::size_t> producerSlot_;
    std::optional<std::size_t> rewardSlot_;
    std::uint8_t lastHealth_ = initialProducerHealth;
    Stage stage_ = Stage::shootF3;
};
} //namespace naturalBlue
