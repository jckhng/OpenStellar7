#pragma once
#include "playable_campaign_route_controls.hpp"

namespace naturalRoute {
enum class Level1Stage { wave, guardianWait, guardian, pickup };

struct Level1Policy {
    Level1Stage stage=Level1Stage::wave;
    unsigned frames=0;
    bool retried=false;

    GameplayInput operator()(const GameplayModel& model) {
        constexpr unsigned prefixFrames=256;
        if (frames++<prefixFrames) {
            const auto phase=(frames-1)%32;
            GameplayInput input{};
            input.left=phase>=1 && phase<=7;
            input.right=phase>=8 && phase<=17;
            input.forward=(phase>=1 && phase<=7) || phase==16 || phase==17;
            input.reverse=phase>=8 && phase<=16;
            input.fire=phase==3 || phase==12 || phase==16;
            return input;
        }
        const auto& actors=model.level2Entities();
        const auto& player=model.level2Player();
        const auto craft=find(model,7,7);
        const auto guardian=find(model,44,45);
        const auto reward=find(model,25,25);
        if (stage==Level1Stage::wave && craft!=noObject) {stage=Level1Stage::guardianWait;}
        if (guardian!=noObject) {stage=Level1Stage::guardian;}
        if (stage==Level1Stage::guardian && guardian==noObject) {stage=Level1Stage::pickup;}
        const Level1Entity* target=nullptr;
        if (stage==Level1Stage::wave) {
            for (const auto& candidate:actors) {
                if (!candidate.active() || candidate.stateBytes4dTo52[0]!=0xff
                    || std::abs(candidate.position.z)>static_cast<std::int32_t>((candidate.collisionRadius+134U)>>1U)) {
                    continue;
                }
                if (!target || candidate.presenterDistance<target->presenterDistance) {target=&candidate;}
            }
        } else if (stage==Level1Stage::guardian) {
            target=&actors[guardian];
        } else if (stage==Level1Stage::pickup && reward!=noObject) {
            target=&actors[reward];
        }
        if (!target) {return {};}
        const bool pickup=stage==Level1Stage::pickup;
        const bool movingBackward=pickup && player.forwardVelocity<0;
        const auto heading=static_cast<std::uint16_t>(fixedHeadingBetween(player.position,target->position)
            +(movingBackward ? 0x8000U : 0U));
        const auto error=static_cast<std::int16_t>(heading-player.yaw);
        const auto desired=std::clamp(static_cast<int>(error)/4,-0x200,0x200);
        GameplayInput input{};
        input.left=player.yawVelocity<desired-0x28;
        input.right=player.yawVelocity>desired+0x28;
        input.fire=!pickup;
        if (stage==Level1Stage::guardian) {
            constexpr std::uint32_t originalApproach=26000,originalRetreat=22000;
            constexpr std::uint32_t retryRetreat=0x7400,retryApproach=0x7c00;
            input.forward=target->presenterDistance>(retried ? retryApproach : originalApproach);
            input.reverse=target->presenterDistance<(retried ? retryRetreat : originalRetreat);
            if (!input.forward && !input.reverse && player.forwardVelocity!=0) {
                input.left=false;input.right=false;
            }
        } else {
            input.forward=!movingBackward;input.reverse=movingBackward;
        }
        return input;
    }
};

}
