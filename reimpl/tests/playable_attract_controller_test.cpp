#include "playable_attract_controller.hpp"
#include <cassert>
#include <stdexcept>
using namespace stellar7;

int main() {
    PlayableAttractController c;
    assert(!c.menuDecision(1,true));
    assert(!c.menuDecision(0,false));
    assert(c.stage()==AttractStage::menu);
    assert(c.menuDecision(0,true));
    c.enterBriefing();
    for(std::size_t page=0;page<attractBriefingPages;++page) {
        assert(c.page()==page);
        assert(!c.briefingDecision(1,AttractBriefingAction::none).nextPage);
        const auto decision=c.briefingDecision(0,AttractBriefingAction::none);
        assert(decision.countdown==attractBriefingPageCountdown);
        assert(decision.closed==(page+1==attractBriefingPages));
        assert(decision.nextPage==(page+1<attractBriefingPages));
    }
    assert(c.stage()==AttractStage::intro && c.automatic() && c.tours()==1);
    c.introReturned();assert(c.stage()==AttractStage::credits);
    c.creditsReturned();assert(c.stage()==AttractStage::postCredits);
    c.postCreditsReturned();c.enterBriefing();
    assert(c.stage()==AttractStage::briefing && c.page()==0);
    //A real recognized control has precedence over expiration and clears02C2.
    const auto control=c.briefingDecision(0,AttractBriefingAction::faster);
    assert(control.countdown==attractBriefingInputCountdown);
    assert(!control.nextPage && !control.closed && !c.automatic());
    assert(!c.briefingDecision(0,AttractBriefingAction::none).nextPage);
    assert(c.briefingDecision(0,AttractBriefingAction::exit).closed);
    assert(c.stage()==AttractStage::menu && c.tours()==1);
    //Expiry overrides a captured Begin result only when the resource actually opens.
    assert(c.menuDecision(0,true));c.enterBriefing();
    assert(c.briefingDecision(10,AttractBriefingAction::next).nextPage);
    assert(!c.automatic() && c.page()==1);
}
