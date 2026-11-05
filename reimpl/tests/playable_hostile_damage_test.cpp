#include "playable_startup_resources.hpp"
#include "stellar7/campaign_loader.hpp"
#include "level1_reward_model_bank.hpp"
#include <algorithm>
#include <iostream>
#include <memory>

namespace {
void require(bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
}
namespace stellar7 {
enum class HostileContactCase { hit,shielded,miss };
enum class ContactOrder { allocation, reversed };
struct Level1ReloadTestAccess {
    static void shield(Level1State& state,bool enabled) {
        state.systemTimers_[1]=enabled?500:0;state.systemStatusLevels_[1]=enabled?-2:3;
    }
};
struct GameplayModelTestAccess {
    static std::unique_ptr<GameplayModel> coldLevel1(const GameplayAssets& assets) {
        const auto count=level1RewardModelCapacity(assets);
        const std::vector<Model3d> shared(assets.models.begin(),assets.models.begin()+count);
        auto model=std::make_unique<GameplayModel>(GameplayModel::fromNativeStartup());
        model->adoptPlayableLevel1(shared,assets.campaignLevels.front().models);
        model->advancePlayableCampaign({});model->finishPlayableCampaignFrame();
        require(model->playerArmor()==680,"initial Level1 frame lost full armor");
        return model;
    }
    static std::unique_ptr<GameplayModel> rammerContact(const GameplayAssets& assets) {
        const auto count=level1RewardModelCapacity(assets);
        const std::vector<Model3d> shared(assets.models.begin(),assets.models.begin()+count);
        auto table=campaignLevelDefinition(1,0).table;table.actors={{4,0,0,0}};
        StellarRng rng;auto actors=constructCampaignLoaderActors(table,rng,shared,assets.campaignLevels.at(1).models);
        actors.front().forwardVelocity=actors.front().xVelocity=actors.front().yVelocity=0;
        auto model=std::make_unique<GameplayModel>();model->playableCampaignFrames_=true;
        model->initializeRetainedCampaignLevel(1,std::move(actors),rng,shared,assets.campaignLevels.at(1).models);
        model->completePlayableCampaignPreloop();model->level2ActiveListOrder_=model->level2AllocationListOrder_;
        model->level2PlayerArmor_=680;Level1ReloadTestAccess::shield(model->level1_,false);
        return model;
    }
    static std::unique_ptr<GameplayModel> commonLoader(const GameplayAssets& assets,std::uint8_t level) {
        const auto count=level1RewardModelCapacity(assets);
        const std::vector<Model3d> shared(assets.models.begin(),assets.models.begin()+count);
        const auto& local=assets.campaignLevels.at(level).models;
        StellarRng rng;auto actors=constructCampaignLoaderActors(campaignLevelDefinition(level,0).table,rng,shared,local);
        auto model=std::make_unique<GameplayModel>();model->playableCampaignFrames_=true;
        model->initializeRetainedCampaignLevel(level,std::move(actors),rng,shared,local);
        if(level==2) {model->completeLevel3Preloop();} else {model->completePlayableCampaignPreloop();}
        model->advancePlayableCampaign({});model->finishPlayableCampaignFrame();return model;
    }
    // Controlled contact pose on a genuinely allocated hostile projectile. This
    // tests the shared loader/collision path, not natural combat ancestry.
    static std::pair<std::size_t,int> contact(GameplayModel& model,HostileContactCase mode) {
        auto owner=std::find_if(model.level2Entities_.begin(),model.level2Entities_.end(),[](const auto& actor) {
            return actor.stateBytes4dTo52[0]==0xff&&actor.weaponSlotCount>0;
        });
        require(owner!=model.level2Entities_.end(),"loader has no armed owner");
        const auto index=static_cast<std::size_t>(owner-model.level2Entities_.begin());
        owner->weaponCooldown=0;owner->weaponSlotTimers.fill(0);owner->targetHeading=owner->yaw;
        require(model.fireLevel2EnemyProjectile(index),"actual owner did not allocate a hostile projectile");
        const auto shot=std::find_if(model.level2Entities_.begin(),model.level2Entities_.end(),[](const auto& actor) {
            return actor.stateBytes4dTo52[0]==0xfd&&!actor.ownerIsPlayer;
        });
        require(shot!=model.level2Entities_.end(),"hostile projectile missing");
        shot->position=model.level2Player_.position;
        if(mode==HostileContactCase::miss) {shot->position.x+=100000;}
        shot->forwardVelocity=shot->xVelocity=shot->yVelocity=shot->zVelocity=0;
        const auto slot=model.level2EntityProjectilePoolSlots_[static_cast<std::size_t>(shot-model.level2Entities_.begin())];
        model.level2ProjectilePool_[slot]=*shot;
        Level1ReloadTestAccess::shield(model.level1_,mode==HostileContactCase::shielded);
        const int damage=shot->type==19?80:shot->type==18?64:shot->type==20?56:shot->type==22?104:40;
        return {slot,damage};
    }
    // Declared synthetic overlap on two genuinely allocated FD records. Keep
    // their distinct types/damage so traversal order is observable across frames.
    static std::array<std::size_t, 2> pairedShots(GameplayModel& model, const ContactOrder order) {
        std::array<std::size_t, 2> slots{};
        for (auto& slot : slots) {
            const auto owner = std::find_if(model.level2Entities_.begin(), model.level2Entities_.end(),
                [](const auto& actor) { return actor.stateBytes4dTo52[0] == 0xff && actor.weaponSlotCount > 0; });
            require(owner != model.level2Entities_.end(), "paired contact has no armed owner");
            const auto free = std::find(model.level2ProjectilePoolActive_.begin(), model.level2ProjectilePoolActive_.end(), false);
            require(free != model.level2ProjectilePoolActive_.end(), "paired contact has no free FD record");
            slot = static_cast<std::size_t>(free - model.level2ProjectilePoolActive_.begin());
            owner->weaponCooldown = 0;
            owner->weaponSlotTimers.fill(0);
            owner->targetHeading = owner->yaw;
            require(model.fireLevel2EnemyProjectile(static_cast<std::size_t>(owner - model.level2Entities_.begin())),
                "paired contact allocation failed");
        }
        isolateActors(model);
        for (std::size_t index = 0; index < slots.size(); ++index) {
            const auto found = std::find(model.level2EntityProjectilePoolSlots_.begin(),
                model.level2EntityProjectilePoolSlots_.end(), slots[index]);
            auto& shot = model.level2Entities_[static_cast<std::size_t>(found - model.level2EntityProjectilePoolSlots_.begin())];
            shot.type = index == 0 ? 17 : 19;
            shot.position = model.level2Player_.position;
            shot.stateBytes4dTo52[1] = 10;
            model.level2ProjectilePool_[slots[index]] = shot;
        }
        if (order == ContactOrder::reversed) {
            const auto first = std::find(model.level2EntityProjectilePoolSlots_.begin(), model.level2EntityProjectilePoolSlots_.end(), slots[0]);
            const auto second = std::find(model.level2EntityProjectilePoolSlots_.begin(), model.level2EntityProjectilePoolSlots_.end(), slots[1]);
            const auto firstIndex = static_cast<std::size_t>(first - model.level2EntityProjectilePoolSlots_.begin());
            const auto secondIndex = static_cast<std::size_t>(second - model.level2EntityProjectilePoolSlots_.begin());
            for (auto* list : {&model.level2ActiveListOrder_, &model.level2AllocationListOrder_, &model.level2ObjectListOrder_}) {
                std::iter_swap(std::find(list->begin(), list->end(), firstIndex), std::find(list->begin(), list->end(), secondIndex));
            }
            std::swap(slots[0], slots[1]);
        }
        Level1ReloadTestAccess::shield(model.level1_, false);
        return slots;
    }
    static bool projectileActive(const GameplayModel& model, const std::size_t slot) {
        return model.level2ProjectilePoolActive_[slot];
    }
    static std::unique_ptr<GameplayModel> pairedSolids(const GameplayAssets& assets, const ContactOrder order) {
        const auto count = level1RewardModelCapacity(assets);
        const std::vector<Model3d> shared(assets.models.begin(), assets.models.begin() + count);
        auto table = campaignLevelDefinition(1, 0).table;
        table.actors = {{1, 0, 0, 0}, {4, 0, 0, 0}};
        StellarRng rng;
        auto actors = constructCampaignLoaderActors(table, rng, shared, assets.campaignLevels.at(1).models);
        // Equal controlled bounds isolate last-contact selection from model radii.
        constexpr std::uint32_t contactRadius = 200;
        for (auto& actor : actors) {
            actor.forwardVelocity = actor.xVelocity = actor.yVelocity = actor.zVelocity = actor.yawVelocity = 0;
            actor.weaponSlotCount = 0;
            actor.collisionRadius = contactRadius;
        }
        auto model = std::make_unique<GameplayModel>();
        model->playableCampaignFrames_ = true;
        model->initializeRetainedCampaignLevel(1, std::move(actors), rng, shared, assets.campaignLevels.at(1).models);
        model->completePlayableCampaignPreloop();
        model->level2Player_.collisionRadius = contactRadius;
        model->level2PlayerArmor_ = 680;
        model->level2ActiveListOrder_ = model->level2AllocationListOrder_;
        if (order == ContactOrder::reversed) {
            for (auto* list : {&model->level2ActiveListOrder_, &model->level2AllocationListOrder_, &model->level2ObjectListOrder_}) {
                std::reverse(list->begin(), list->end());
            }
        }
        Level1ReloadTestAccess::shield(model->level1_, false);
        return model;
    }
    static bool controlledShotRetired(const GameplayModel& model,std::size_t slot) {
        // A later callback may reuse the retired FD slot in this same frame.
        // Genuine replacement launches move at500; the controlled hit had zero speed.
        return !model.level2ProjectilePoolActive_[slot]||model.level2ProjectilePool_[slot].forwardVelocity!=0;
    }
    static void isolateActors(GameplayModel& model) {
        for(std::size_t index=0;index<model.level2Entities_.size();++index) {
            auto& actor=model.level2Entities_[index];
            actor.position={100000+static_cast<std::int32_t>(index)*10000,100000,0};
            actor.forwardVelocity=actor.xVelocity=actor.yVelocity=actor.zVelocity=0;
            actor.weaponSlotCount=0;
        }
    }
    static void approachingShot(GameplayModel& model) {
        isolateActors(model);
        const auto found=std::find_if(model.level2Entities_.begin(),model.level2Entities_.end(),[](const auto& actor) {return actor.type==1;});
        require(found!=model.level2Entities_.end(),"Level1 armed owner missing");
        auto& owner=*found;
        owner.position=model.level2Player_.position;owner.position.y-=2000;
        owner.pitch=owner.roll=0;owner.yaw=fixedHeadingBetween(owner.position,model.level2Player_.position);
        owner.targetHeading=owner.yaw;owner.weaponSlotCount=1;owner.weaponCooldown=0;owner.weaponSlotTimers.fill(0);
        const auto index=static_cast<std::size_t>(&owner-model.level2Entities_.data());
        require(model.fireLevel2EnemyProjectile(index),"approaching shot did not allocate");
        // Only the emitter is moved away; the new shot retains its own launch pose and speed.
        auto live=std::find_if(model.level2Entities_.begin(),model.level2Entities_.end(),[](const auto& actor) {return actor.type==1;});
        live->position={100000,100000,0};live->weaponSlotCount=0;
        Level1ReloadTestAccess::shield(model.level1_,false);
    }
    static void ordinaryContact(GameplayModel& model) {
        isolateActors(model);
        auto target=std::find_if(model.level2Entities_.begin(),model.level2Entities_.end(),[](const auto& actor) {return actor.type==1;});
        require(target!=model.level2Entities_.end(),"Level1 ordinary actor missing");
        target->position=model.level2Player_.position;
        Level1ReloadTestAccess::shield(model.level1_,false);
    }
};
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"requires original resources");using namespace stellar7;
        const auto assets=naturalRoute::assets(std::filesystem::u8path(argv[1]));
        for(const std::uint8_t level:{1,2,3,4,5,6,0}) {
            for(const auto mode:{HostileContactCase::hit,HostileContactCase::shielded,HostileContactCase::miss}) {
                auto model=GameplayModelTestAccess::commonLoader(assets,level);
                const auto contact=GameplayModelTestAccess::contact(*model,mode);
                const auto armor=model->playerArmor();const auto events=model->level2AudioEvents().size();
                model->advancePlayableCampaign({});model->finishPlayableCampaignFrame();
                const int expected=mode==HostileContactCase::hit?armor-contact.second:armor;
                std::cout<<"level="<<int(level+1)<<" mode="<<static_cast<int>(mode)<<" armor="<<armor<<" -> "<<model->playerArmor()<<" expected="<<expected<<'\n';
                require(model->playerArmor()==expected,"hostile contact armor mismatch");
                const auto& damage=model->combatFeedback().damage;
                if(mode==HostileContactCase::hit) {
                    require(damage&&damage->active(level,model->ticks())&&damage->amount==contact.second,
                        "confirmed armor loss did not produce bounded combat feedback");
                } else {
                    require(!damage||!damage->active(level,model->ticks()),"shielded or missed shot produced damage feedback");
                }
                if(mode!=HostileContactCase::miss) {
                    require(GameplayModelTestAccess::controlledShotRetired(*model,contact.first),"hostile contact did not retire projectile");
                    if(mode==HostileContactCase::hit) {
                        const auto& audio=model->level2AudioEvents();
                        require(std::any_of(audio.begin()+events,audio.end(),[](auto event) {return event.eventId==6;}),"contact omitted impact audio");
                    }
                }
                model->advancePlayableCampaign({});model->finishPlayableCampaignFrame();
                require(model->playerArmor()==expected,"retired hostile hit twice on adjacent frame");
            }
        }
        for (const auto order : {ContactOrder::allocation, ContactOrder::reversed}) {
            auto model = GameplayModelTestAccess::commonLoader(assets, 1);
            const auto slots = GameplayModelTestAccess::pairedShots(*model, order);
            const auto armor = model->playerArmor();
            const auto firstDamage = order == ContactOrder::allocation ? 40 : 80;
            model->advancePlayableCampaign({});
            model->finishPlayableCampaignFrame();
            require(model->playerArmor() == armor - firstDamage, "hostile traversal did not stop at its first contact");
            require(!GameplayModelTestAccess::projectileActive(*model, slots[0])
                && GameplayModelTestAccess::projectileActive(*model, slots[1]), "hostile traversal retired the wrong physical record");
            require(std::count_if(model->level2AudioEvents().begin(), model->level2AudioEvents().end(),
                [](const auto& event) { return event.eventId == 6; }) == 1, "first hostile contact changed impact count");
            model->advancePlayableCampaign({});
            model->finishPlayableCampaignFrame();
            require(model->playerArmor() == armor - 120 && !GameplayModelTestAccess::projectileActive(*model, slots[1]),
                "adjacent hostile traversal lost its remaining contact");

            auto solids = GameplayModelTestAccess::pairedSolids(assets, order);
            solids->advancePlayableCampaign({});
            solids->finishPlayableCampaignFrame();
            require(solids->playerArmor() == (order == ContactOrder::allocation ? 580 : 680),
                "player traversal did not respond to its last solid contact");
        }
        require(!campaignStoryScene(2),"Level2 to3 incorrectly schedules narration");
        {
            auto model=GameplayModelTestAccess::coldLevel1(assets);
            const auto armor=model->playerArmor();GameplayModelTestAccess::approachingShot(*model);
            for(unsigned frame=0;frame<8&&model->playerArmor()==armor;++frame) {
                model->advancePlayableCampaign({});model->finishPlayableCampaignFrame();
            }
            std::cout<<"moving Type17 armor="<<armor<<" -> "<<model->playerArmor()<<'\n';
            require(model->playerArmor()==armor-40,"moving hostile shot did not deliver original Type17 damage");
        }
        {
            auto model=GameplayModelTestAccess::coldLevel1(assets);
            const auto armor=model->playerArmor();GameplayModelTestAccess::ordinaryContact(*model);
            model->advancePlayableCampaign({});model->finishPlayableCampaignFrame();
            std::cout<<"ordinary Type1 contact armor="<<armor<<" -> "<<model->playerArmor()<<'\n';
            require(std::any_of(model->level2AudioEvents().begin(),model->level2AudioEvents().end(),
                [](const auto& event) {return event.eventId==9;}),"ordinary control did not reach solid backout");
            require(model->playerArmor()==armor,"ordinary solid contact invented damage absent from0784/0E50");
            GameplayModelTestAccess::isolateActors(*model);
            for(unsigned frame=0;frame<8;++frame) {model->advancePlayableCampaign({});model->finishPlayableCampaignFrame();}
            require(model->playerArmor()==armor,"old contact word caused damage after separation");
        }
        {
            auto model=GameplayModelTestAccess::rammerContact(assets);
            model->advancePlayableCampaign({});model->finishPlayableCampaignFrame();
            require(model->playerArmor()==580,"unshielded Type4 must remove100 once from full armor");
            GameplayModelTestAccess::isolateActors(*model);
            for(unsigned frame=0;frame<8;++frame) {model->advancePlayableCampaign({});model->finishPlayableCampaignFrame();}
            require(model->playerArmor()==580,"Type4 stale contact kept damaging after separation");
        }
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
