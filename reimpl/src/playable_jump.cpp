#include "playable_jump.hpp"

#include <algorithm>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr std::size_t bombSystem=3;
constexpr std::uint16_t bombEvent=7;
constexpr std::size_t decoySystem=0;
constexpr std::int16_t decoyDuration=900;
constexpr std::int16_t decoyRevealEnd=860;
constexpr std::int16_t decoyExpiryBlink=60;
constexpr std::uint16_t decoyEvent=25;
constexpr std::size_t jumpSystem=2;
constexpr std::size_t shieldSystem=1;
constexpr std::size_t scannerSystem=4;
constexpr std::size_t superSystem=5;
constexpr std::size_t turboSystem=6;
constexpr std::int16_t noSelection=-1;
constexpr std::int16_t launchIncrement=220;
constexpr std::int16_t gravityIncrement=10;
constexpr std::uint16_t activationEvent=8;
constexpr std::uint16_t selectionEvent=16;
constexpr std::int16_t timedDuration=500;
constexpr std::int16_t turboDuration=200;
constexpr std::uint16_t turboEvent=20;
constexpr std::uint16_t scannerEvent=22;

bool timedSystem(const std::size_t system) {
    return system==shieldSystem || system==scannerSystem || system==superSystem || system==turboSystem;
}

bool admits(const PlayableSystemDomain domain,const std::size_t system) {
    return system==jumpSystem || (domain!=PlayableSystemDomain::jumpOnly && timedSystem(system))
        || ((domain==PlayableSystemDomain::decoyBundle || domain==PlayableSystemDomain::fullBundle) && system==decoySystem)
        || (domain==PlayableSystemDomain::fullBundle && system==bombSystem);
}

std::int16_t requestedSystem(const GameplaySystemAction action,const PlayableSystemDomain domain) {
    switch(action) {
    case GameplaySystemAction::jump:return jumpSystem;
    case GameplaySystemAction::decoy:if(admits(domain,decoySystem)) {return decoySystem;} break;
    case GameplaySystemAction::bomb:if(domain==PlayableSystemDomain::fullBundle) {return bombSystem;} break;
    case GameplaySystemAction::shield:if(domain!=PlayableSystemDomain::jumpOnly) {return shieldSystem;} break;
    case GameplaySystemAction::scanner:if(domain!=PlayableSystemDomain::jumpOnly) {return scannerSystem;} break;
    case GameplaySystemAction::superPrimary:if(domain!=PlayableSystemDomain::jumpOnly) {return superSystem;} break;
    case GameplaySystemAction::turbo:if(domain!=PlayableSystemDomain::jumpOnly) {return turboSystem;} break;
    case GameplaySystemAction::cycleSelected:if(domain!=PlayableSystemDomain::jumpOnly) {return noSelection;} break;
    case GameplaySystemAction::none:
    case GameplaySystemAction::activateSelected:return noSelection;
    }
    throw std::invalid_argument("unsupported playable system action");
}

void select(PlayableJumpResult& result,const std::int16_t requested) {
    auto& state=result.state;
    const auto previous=state.selectedSystem;
    if(previous!=noSelection) {result.effects.push_back(SystemBorder{static_cast<std::size_t>(previous),false});}
    if(requested==noSelection) {
        result.effects.push_back(SystemAudio{selectionEvent});
        state.selectedSystem=noSelection;
        auto candidate=previous;
        for(std::size_t count=0;count<state.statuses.size();++count) {
            candidate=static_cast<std::int16_t>((candidate+1)%state.statuses.size());
            if(state.statuses[candidate]>0) {state.selectedSystem=candidate;break;}
        }
    } else {
        state.selectedSystem=state.statuses[requested]==0 ? noSelection : requested;
    }
    if(state.selectedSystem!=noSelection) {
        const auto selected=static_cast<std::size_t>(state.selectedSystem);
        result.effects.push_back(SystemBorder{selected,true});
        result.effects.push_back(SystemCopy{selected});
    }
    if(previous!=noSelection) {result.effects.push_back(SystemCopy{static_cast<std::size_t>(previous)});}
}

void normalize(PlayableJumpResult& result,const std::size_t system) {
    auto& state=result.state;
    auto& status=state.statuses[system];
    if(status<0) {status=static_cast<std::int8_t>(-status);}
    const auto style=state.timers[system]!=0 ? SystemPanelStyle::active
        : status!=0 ? SystemPanelStyle::ready : SystemPanelStyle::unavailable;
    result.effects.push_back(SystemPanel{system,style,static_cast<std::uint8_t>(status)});
    if(state.selectedSystem==static_cast<std::int16_t>(system) && status==0) {select(result,noSelection);}
}
} //namespace

void validatePlayableJumpState(const PlayableJumpState& state,const PlayableSystemDomain domain) {
    if(domain!=PlayableSystemDomain::jumpOnly && domain!=PlayableSystemDomain::timedBundle
        && domain!=PlayableSystemDomain::decoyBundle && domain!=PlayableSystemDomain::fullBundle) {
        throw std::invalid_argument("unsupported playable system domain");
    }
    const auto increment=state.timers[jumpSystem];
    const auto grounded=increment==0 && state.playerZ==0;
    const auto flightHeight=(launchIncrement-increment)*(launchIncrement+gravityIncrement+increment)/20;
    if(state.selectedSystem<noSelection || state.selectedSystem>=static_cast<std::int16_t>(state.statuses.size())
        || increment < -launchIncrement || increment > launchIncrement || increment%gravityIncrement!=0
        || (!grounded && state.playerZ!=flightHeight)
        || (grounded && state.statuses[jumpSystem]<0)) {
        throw std::invalid_argument("invalid retained playable Jump state");
    }
    for(std::size_t system=0;system<state.statuses.size();++system) {
        //48D4 deliberately retains system3's unrelated word12D4.
        const auto allowed=admits(domain,system);
        const auto maximum=system==decoySystem ? decoyDuration
            : system==turboSystem ? turboDuration : timedDuration;
        if((system!=jumpSystem && system!=3 && !allowed && state.timers[system]!=0)
            || (system!=jumpSystem && system!=3 && allowed
                && (state.timers[system]<0 || state.timers[system]>maximum))
            || state.statuses[system]>3 || state.statuses[system]<(allowed ? -3 : 0)
            || (system==bombSystem && state.statuses[system]<0)
            || (system!=jumpSystem && system!=bombSystem && allowed && state.statuses[system]<0 && state.timers[system]==0)) {
            throw std::invalid_argument("playable system state exceeds its admitted ability domain");
        }
    }
}

PlayableJumpResult advancePlayableJump(const PlayableJumpState& incoming,const PlayableSystemDomain domain) {
    validatePlayableJumpState(incoming,domain);
    PlayableJumpResult result{incoming,{}};
    auto& state=result.state;
    auto& increment=state.timers[jumpSystem];
    //0312/0F72 precedes918E, fire and the queued key callback.
    if(admits(domain,decoySystem) && state.timers[decoySystem]!=0) {
        const auto remaining=--state.timers[decoySystem];
        const auto strip=(remaining&2)!=0 ? DecoyStripMode::visible : DecoyStripMode::restore;
        if(remaining<decoyExpiryBlink) {
            result.effects.push_back(SystemDecoyStrip{strip});
            if(remaining==0) {
                //B452(25) is the existing op14 no-op; no duplicate synth event.
                normalize(result,decoySystem);
                result.effects.push_back(SystemDecoyStrip{DecoyStripMode::restore});
                result.effects.push_back(SystemDecoyPalette{DecoyPaletteMode::restore});
            }
        } else if(remaining>=decoyRevealEnd) {
            result.effects.push_back(SystemDecoyStrip{strip});
            if(remaining==decoyRevealEnd) {
                result.effects.push_back(SystemDecoyStrip{DecoyStripMode::visible});
                result.effects.push_back(SystemDecoyPalette{DecoyPaletteMode::decoy});
            }
        }
    }
    if(domain!=PlayableSystemDomain::jumpOnly && state.timers[shieldSystem]!=0) {
        if(--state.timers[shieldSystem]==0) {normalize(result,shieldSystem);}
    }
    //918E performs signed-word addition into a dword, then SUB word12D2,10.
    if(increment!=0 || state.playerZ>0) {
        state.playerZ=static_cast<std::int32_t>(static_cast<std::uint32_t>(state.playerZ)
            +static_cast<std::uint32_t>(static_cast<std::int32_t>(increment)));
        increment=static_cast<std::int16_t>(static_cast<std::uint16_t>(increment)-gravityIncrement);
        if(state.playerZ<=0) {state.playerZ=0;increment=0;normalize(result,jumpSystem);}
    }
    if(domain!=PlayableSystemDomain::jumpOnly) {
        if(state.timers[scannerSystem]!=0) {
            if(--state.timers[scannerSystem]==0) {
                normalize(result,scannerSystem);
                result.scannerRefresh=PlayableScannerRefresh::inactive;
            } else if((state.timers[scannerSystem]&7)==0) {
                result.effects.push_back(SystemAudio{scannerEvent});
            }
        }
        if(state.timers[superSystem]!=0 && --state.timers[superSystem]==0) {normalize(result,superSystem);}
        if(state.timers[turboSystem]!=0 && --state.timers[turboSystem]==0) {
            normalize(result,turboSystem);
            //B452(20) dispatches the SBL op14 no-op; do not stop a sequence or direct voice.
        }
    }
    return result;
}

PlayableJumpResult applyPlayableJumpAction(const PlayableJumpState& incoming,const GameplaySystemAction action,
    const PlayableSystemDomain domain) {
    validatePlayableJumpState(incoming,domain);
    const auto requested=requestedSystem(action,domain);
    //Reject another actual activation atomically, before event8/charge/UI writes.
    if(action==GameplaySystemAction::activateSelected && incoming.selectedSystem!=noSelection
        && !admits(domain,static_cast<std::size_t>(incoming.selectedSystem))
        && incoming.statuses[incoming.selectedSystem]>0) {
        throw std::invalid_argument("playable system callback has no admitted implementation");
    }
    if(action!=GameplaySystemAction::none && incoming.playerZ>0 && incoming.timers[jumpSystem]==0
        && incoming.statuses[jumpSystem]>0
        && (action==GameplaySystemAction::jump
            || (action==GameplaySystemAction::activateSelected && incoming.selectedSystem==jumpSystem))) {
        //A repair followed by an apex relaunch is outside this initial single-flight domain.
        throw std::invalid_argument("playable Jump has no qualified airborne relaunch");
    }
    PlayableJumpResult result{incoming,{}};
    if(action==GameplaySystemAction::none) {return result;}
    if(action==GameplaySystemAction::cycleSelected) {select(result,noSelection);return result;}
    if(action!=GameplaySystemAction::activateSelected) {select(result,requested);}
    result.effects.push_back(SystemAudio{activationEvent});
    auto& state=result.state;
    if(state.selectedSystem==noSelection || state.statuses[state.selectedSystem]<=0) {return result;}
    const auto system=static_cast<std::size_t>(state.selectedSystem);
    auto& charges=state.statuses[system];
    --charges;
    result.effects.push_back(SystemPanel{system,SystemPanelStyle::active,static_cast<std::uint8_t>(charges)});
    charges=static_cast<std::int8_t>(-charges);
    if(system==bombSystem) {
        result.effects.push_back(SystemAudio{bombEvent});
        result.bombRefresh=PlayableBombRefresh::deploy;
        normalize(result,bombSystem); //8E84 always calls8ED2, including full FE.
    } else if(system==decoySystem) {
        if(state.timers[system]==0) {
            state.timers[system]=decoyDuration;
            result.proxyRefresh=PlayableProxyRefresh::copyPlayerRecord;
            result.effects.push_back(SystemAudio{decoyEvent});
        }
    } else if(system==jumpSystem) {
        if(state.timers[system]==0) {state.timers[system]=launchIncrement;}
    } else {
        if(system==turboSystem) {result.effects.push_back(SystemAudio{turboEvent});}
        if(state.timers[system]==0) {state.timers[system]=system==turboSystem ? turboDuration : timedDuration;}
        if(system==scannerSystem) {result.scannerRefresh=PlayableScannerRefresh::active;}
    }
    return result;
}

PlayableJumpResult normalizePlayableSystemsAfterReset(const PlayableJumpState& incoming,
    const PlayableSystemDomain domain) {
    auto validation=incoming;
    for(std::size_t system=0;system<validation.statuses.size();++system) {
        if(admits(domain,system) && validation.statuses[system]<0) {
            validation.statuses[system]=static_cast<std::int8_t>(-validation.statuses[system]);
        }
    }
    validatePlayableJumpState(validation,domain);
    if(incoming.playerZ!=0 || incoming.timers[jumpSystem]!=0
        || (admits(domain,decoySystem) && incoming.timers[decoySystem]!=0)
        || incoming.timers[shieldSystem]!=0 || incoming.timers[scannerSystem]!=0
        || incoming.timers[superSystem]!=0 || incoming.timers[turboSystem]!=0) {
        throw std::invalid_argument("playable status initialization requires completed48D4 system reset");
    }
    PlayableJumpResult result{incoming,{}};
    for(std::size_t system=0;system<incoming.statuses.size();++system) {normalize(result,system);}
    if(result.state.selectedSystem>=0) {select(result,result.state.selectedSystem);}
    return result;
}

PlayablePrimaryProfile playablePrimaryProfile(const std::int16_t superTimer) {
    if(superTimer<0 || superTimer>timedDuration) {throw std::invalid_argument("invalid super-primary timer");}
    return superTimer==0 ? PlayablePrimaryProfile{0,64,9,14,true}
        : PlayablePrimaryProfile{1,80,5,0,false};
}

std::int16_t playableArmorAfterDamage(const std::int16_t armor,const std::int16_t units,
    const PlayableShield shield) {
    if(shield!=PlayableShield::inactive && shield!=PlayableShield::active) {
        throw std::invalid_argument("invalid playable shield state");
    }
    const auto delta=units<0 ? 4*static_cast<std::int32_t>(units)
        : shield==PlayableShield::active ? 0 : -4*static_cast<std::int32_t>(units);
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(static_cast<std::int32_t>(armor)+delta));
}

std::int16_t advancePlayableForwardVelocity(const std::int16_t incoming,const GameplayInput& input,
    const PlayablePropulsion propulsion) {
    if(propulsion==PlayablePropulsion::turbo) {
        //A0B8 ADD is a signed word before its sole upper-cap comparison.
        const auto accelerated=static_cast<std::int16_t>(static_cast<std::uint16_t>(incoming)+40);
        return std::min<std::int16_t>(400,accelerated);
    }
    if(propulsion!=PlayablePropulsion::ordinary) {throw std::invalid_argument("invalid playable propulsion");}
    if(input.analog && !input.left && !input.right && !input.forward && !input.reverse) {
        const auto y=input.analog->y;
        return static_cast<std::int16_t>(std::clamp(std::abs(static_cast<int>(y))<16 ? 0 : -2*static_cast<int>(y),-200,200));
    }
    if(input.forward || input.reverse) {
        return static_cast<std::int16_t>(std::clamp(static_cast<int>(incoming)+(input.reverse ? -10 : 10),-200,200));
    }
    if(input.left || input.right) {return incoming;}
    if(incoming>0) {return static_cast<std::int16_t>(std::max(0,static_cast<int>(incoming)-10));}
    return static_cast<std::int16_t>(std::min(0,static_cast<int>(incoming)+10));
}
} //namespace stellar7
