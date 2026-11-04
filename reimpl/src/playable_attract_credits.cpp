#include "playable_attract.hpp"
#include "stellar7/ui_fill_work.hpp"

namespace stellar7 {
namespace {constexpr CreditsColors colors{{{147,141,135,128,135,141,147},147,135,128},156};}
void PlayableAttractCredits::advance(AttractPresentation& p,const PlayableAttractInput& input) {
    auto& o=p.o;
    const bool cold=caller_==CreditsPresentationCaller::coldStartup;
    if(o.creditsSkip!=0) {complete_=true;return;}
    if(!entered_) {
        if (cold) { p.normal({{NormalWorkKind::instructions,3}}); }
        const auto drawing=creditsDrawingOperations(o.assets.artGroups.at(0),o.assets.titleFont,o.assets.scoreFont,colors,o.pages.backPage());
        p.normal(drawing.operations);o.pixels.apply(drawing.effects);
        if (cold) { p.normal({{NormalWorkKind::instructions,1}}); }
        p.blackout();
        if (cold) {
            p.normal({{NormalWorkKind::instructions,11}});
            p.normal(planarCopyOperations(0,320,200));
        }
        p.copy(o.pages.backPage(),o.pages.frontPage());p.publish();
        if (cold) {
            p.normal({{NormalWorkKind::instructions,2}});
            for (std::uint8_t intensity=0;intensity<=60;intensity+=4) {
                p.normal({{NormalWorkKind::instructions,intensity==0?20U:25U}});p.upload(intensity);
            }
            p.normal({{NormalWorkKind::instructions,9},{NormalWorkKind::playableStartupCreditsCountdown,1,7200}});
        } else { p.fadeIn();p.store(7200); }
        entered_=true;
        if (input.sourceKey==0) { return; }
    }
    auto hold=o.clock.renderWork(o.phase,{{cold?NormalWorkKind::playableStartupCreditsPoll:NormalWorkKind::playableAttractCreditsPoll,p.budget,input.sourceKey}},120000);
    const auto done=hold.timing.countdownReturned;p.append(std::move(hold));
    if(!done) {return;}
    if (cold) { p.normal({{NormalWorkKind::instructions,1}}); }
    p.blackout();
    const auto clear=uiFillOperations({0,0,320,200},0,o.pages.frontPage());
    if (cold) { p.normal({{NormalWorkKind::instructions,9}}); }
    p.normal(clear.operations);
    if (cold) { p.normal({{NormalWorkKind::instructions,2}}); }
    o.pixels.apply(clear.effects);
    if (cold) { p.normal({{NormalWorkKind::instructions,2}}); }
    p.cockpitPalette();
    if (cold) { p.normal({{NormalWorkKind::instructions,6}}); }
    complete_=true;
}
}
