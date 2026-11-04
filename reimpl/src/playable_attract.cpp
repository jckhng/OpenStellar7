#include "playable_attract.hpp"
#include <stdexcept>

namespace stellar7 {
PlayableAttractRunner::PlayableAttractRunner(const std::uint16_t capturedCountdown,const bool available) {
    if(!controller_.menuDecision(capturedCountdown,available)) {throw std::invalid_argument("attract requires actual captured expiry and successful VOICE.RES open");}
}
bool PlayableAttractRunner::acceptsSourceKey() const noexcept {
    if(controller_.stage()==AttractStage::briefing) {return briefing_.has_value();}
    if(controller_.stage()==AttractStage::credits) {return credits_ && credits_->acceptsKey();}
    return controller_.stage()==AttractStage::intro && intro_ && intro_->acceptsKey();
}
NativeSessionDelivery PlayableAttractRunner::advance(PlayableAttractOwners owners,const PlayableAttractInput& input,const std::size_t budget) {
    NativeSessionDelivery delivery;delivery.firstSample=owners.clock.elapsedSamples();
    if(!owners.phase.playableAutomaticAttract) {throw std::invalid_argument("attract runner requires explicit feature authorization");}
    if(returnedToMenu()) {throw std::logic_error("completed attract owner must return to actual menu polling");}
    if(!callerCampaign_) {
        if(owners.phase.attractBorrow!=AudioClockAttractBorrow::inactive) {throw std::logic_error("attract cannot replace another active borrow");}
        callerCampaign_=owners.phase.playableCampaign;
    }
    owners.phase.playableCampaign=true;
    owners.phase.attractBorrow=AudioClockAttractBorrow::menuPresentation;
    AttractPresentation p(owners,delivery,budget);
    if(controller_.stage()==AttractStage::briefing) {
        if(!briefing_) {
            controller_.enterBriefing();briefing_.emplace(owners.assets,owners.sharedDetail);
            p.store(attractBriefingInitialCountdown);briefing_->paint(p,controller_.page());return delivery;
        }
        const auto action=briefing_->input(input,owners.configuration);
        const auto decision=controller_.briefingDecision(p.read(),action);
        if(decision.countdown) {p.store(*decision.countdown);}
        briefing_->control(action);
        if(!decision.closed) {briefing_->paint(p,controller_.page());return delivery;}
        //1D9E releases its text/local Level1 viewer and resets its local pools;
        //these are not gameplay actors or imported save state.
        briefing_.reset();p.blackout();
        if(controller_.stage()==AttractStage::intro) {intro_.emplace();}
        else {
            owners.phase.attractBorrow=AudioClockAttractBorrow::inactive;
            owners.phase.playableCampaign=*callerCampaign_;
        }
        return delivery;
    }
    if(controller_.stage()==AttractStage::intro) {
        if(!intro_) {intro_.emplace();}
        intro_->advance(p,input);
        if(intro_->complete()) {intro_.reset();controller_.introReturned();credits_.emplace();}
        return delivery;
    }
    if(controller_.stage()==AttractStage::credits) {
        credits_->advance(p,input);
        if(credits_->complete()) {credits_.reset();controller_.creditsReturned();}
        return delivery;
    }
    if(!postCreditsEntered_) {p.store(600);p.normal({{NormalWorkKind::instructions,3}});postCreditsEntered_=true;}
    if(p.wait()) {
        p.blackout();p.copy(owners.pages.frontPage(),owners.pages.backPage());p.cockpitPalette();
        controller_.postCreditsReturned();postCreditsEntered_=false;
    }
    return delivery;
}
}
