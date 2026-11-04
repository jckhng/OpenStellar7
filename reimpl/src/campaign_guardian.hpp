#pragma once

#include "stellar7/level1.hpp"

#include <functional>
#include <optional>

namespace stellar7 {

enum class CampaignGuardianFamily { level4, level6, level7, level1 };
enum class CampaignGuardianAttack { aimedProjectile, volleyProjectile80, pulse46, actor48 };

struct CampaignGuardianContact {
    //An identity supplied by the caller's physical pool layer, not a live vector reference.
    std::size_t identity;
    std::int16_t type;
    bool player = false;
};

struct CampaignGuardianProjectile {
    std::size_t physicalSlot;
    Level1Entity actor;
};

struct CampaignGuardianContext {
    FixedPosition playerPosition{};
    //DS4858's separately allocated type0 record; AA26 copies XYZ only.
    FixedPosition* playerProxyPosition = nullptr;
    //DS41B4: startup-zeroed persistent word. AEBE alone writes it; do not
    //reset it on guardian construction, level load or retry.
    std::int16_t* level6Turn = nullptr;
    bool craftPresent = false;
    //Call-time OLD list order, including retained Type23 pointers; no health filter.
    std::vector<CampaignGuardianProjectile> inheritedProjectiles;
    std::optional<CampaignGuardianContact> contact;
};

struct CampaignGuardianCallbacks {
    //1176 child. Apply its health result to the supplied shadow as well as
    //pool/effect/reward ownership; A7F0 continues even after a lethal hit.
    std::function<void(Level1Entity&,std::uint8_t)> damageGuardian;
    std::function<void(std::size_t)> retireProjectile;
    std::function<void(const CampaignGuardianContact&,std::uint8_t)> damageContact;
    //0E7C(-25), expressed as its actual signed armor change, including audio.
    std::function<void(std::int16_t)> changePlayerArmor;
    //ACEC,995E,4506(46),AE10(source,0), respectively. The physical owner is
    //a caller-held shadow, so children may insert/erase canonical pool views.
    std::function<bool(const Level1Entity&,CampaignGuardianAttack)> attack;
};

//Semantic A6BE/A7F0/AEBE/AA26 callbacks, excluding caller9298 and main/radar130C.
//Use the actual level's local guardian radius. No CPU-work certification.
void advanceCampaignGuardianBehavior(Level1Entity& actor,CampaignGuardianFamily family,
    StellarRng& rng,const CampaignGuardianContext& context,const CampaignGuardianCallbacks& callbacks);

} //namespace stellar7
