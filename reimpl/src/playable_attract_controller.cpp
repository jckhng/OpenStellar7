#include "playable_attract_controller.hpp"
#include <stdexcept>

namespace stellar7 {
bool PlayableAttractController::menuDecision(const std::uint16_t countdown,const bool available) {
    if(stage_!=AttractStage::menu) {throw std::logic_error("attract decision requires its retained main-menu caller");}
    if(countdown!=0 || !available) {return false;}
    automatic_=true;stage_=AttractStage::briefing;page_=0;return true;
}
void PlayableAttractController::enterBriefing() {
    if(stage_!=AttractStage::briefing) {throw std::logic_error("attract Briefing requires1D9E caller");}
    page_=0;
}
AttractBriefingDecision PlayableAttractController::briefingDecision(const std::uint16_t countdown,
    const AttractBriefingAction action) {
    if(stage_!=AttractStage::briefing) {throw std::logic_error("attract Briefing poll requires its live local owner");}
    AttractBriefingDecision result;
    auto selected=action;
    if(selected!=AttractBriefingAction::none) {
        result.countdown=attractBriefingInputCountdown;automatic_=false;
    } else if(automatic_ && countdown==0) {
        result.countdown=attractBriefingPageCountdown;selected=AttractBriefingAction::next;
    }
    if(selected==AttractBriefingAction::exit || (selected==AttractBriefingAction::next && page_+1==attractBriefingPages)) {
        result.closed=true;
        if(automatic_) {stage_=AttractStage::intro;++tours_;}
        else {stage_=AttractStage::menu;}
    } else if(selected==AttractBriefingAction::next) {
        ++page_;result.nextPage=true;
    }
    return result;
}
void PlayableAttractController::introReturned() {
    if(stage_!=AttractStage::intro) {throw std::logic_error("attract lost6070 return");}
    stage_=AttractStage::credits;
}
void PlayableAttractController::creditsReturned() {
    if(stage_!=AttractStage::credits) {throw std::logic_error("attract lostB12A return");}
    stage_=AttractStage::postCredits;
}
void PlayableAttractController::postCreditsReturned() {
    if(stage_!=AttractStage::postCredits) {throw std::logic_error("attract lost159F wait return");}
    stage_=AttractStage::briefing;page_=0;
}
}
