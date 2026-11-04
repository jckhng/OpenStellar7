#include "playable_attract.hpp"
#include "briefing_strip.hpp"
#include "playable_menu_pages.hpp"
#include "playable_pointer.hpp"
#include <array>

namespace stellar7 {
namespace {
constexpr std::array<std::array<std::size_t,4>,6> links{{
    {{5,2,0,1}},{{1,1,0,1}},{{0,3,2,2}},{{2,4,3,3}},{{3,5,4,4}},{{4,0,5,5}}}};
constexpr std::array<ProjectedPoint,6> pointers{{{67,172},{248,172},{32,46},{32,64},{32,82},{32,100}}};
constexpr std::array<AttractBriefingAction,6> actions{{AttractBriefingAction::exit,AttractBriefingAction::next,
    AttractBriefingAction::stop,AttractBriefingAction::faster,AttractBriefingAction::zoomIn,AttractBriefingAction::zoomOut}};
}
PlayableAttractBriefing::PlayableAttractBriefing(const GameplayAssets& assets,const PlayableModelDetail detail)
    :text_(parseBriefingText(assets.briefingText)),viewer_(assets,detail) {}
AttractBriefingAction PlayableAttractBriefing::input(const PlayableAttractInput& input,const CockpitSettings& configuration) {
    if(retainedEscape_) {return AttractBriefingAction::exit;}
    if((input.sourceKey>>8)==1 || input.menu.cancel) {
        //1FB9/1FBE: first Escape decrements localSI then requests NEXT; the
        //net page stays fixed. Next1F9E sees retained12E4==1 and exits.
        retainedEscape_=true;return AttractBriefingAction::escapePending;
    }
    std::optional<std::size_t> direction;
    if(input.menu.up) {direction=0;} else if(input.menu.down) {direction=1;}
    else if(input.menu.left) {direction=2;} else if(input.menu.right) {direction=3;}
    if(direction && armed_) {
        selection_=links.at(selection_).at(*direction);pressed_.reset();pointer_.reset();armed_=false;
        return AttractBriefingAction::none; //5936 result0 does not clear02C2.
    }
    if(!direction) {armed_=true;}
    const bool pointer=configuration.options[0]!=0 && input.menu.pointer;
    if(pointer && input.menu.pointer->phase==NativePointerPhase::cancel) {
        pressed_.reset();return AttractBriefingAction::none;
    }
    if(pointer) {
        pointer_=sourcePointerPosition(input.menu.pointer->position);
        const auto poll=pointerWidgetPoll(pressed_,briefingPointerHitBoxes(),*pointer_,
            input.menu.confirm || input.menu.pointer->primary || input.menu.pointer->secondary ? PointerButton::held : PointerButton::released);
        pressed_=poll.pressed;
        return poll.activated ? actions.at(*poll.activated) : AttractBriefingAction::none;
    }
    if(input.menu.confirm && !pressed_) {pressed_=selection_;}
    if(input.menu.confirm || !pressed_) {return AttractBriefingAction::none;}
    const auto selected=*pressed_;pressed_.reset();return actions.at(selected);
}
void PlayableAttractBriefing::control(const AttractBriefingAction action) {
    if(action>=AttractBriefingAction::stop && action<=AttractBriefingAction::zoomOut) {
        viewer_.control(static_cast<BriefingViewerControl>(static_cast<int>(action)-static_cast<int>(AttractBriefingAction::stop)));
    }
}
void PlayableAttractBriefing::paint(AttractPresentation& p,const std::size_t page) {
    auto& o=p.o;const auto back=o.pages.backPage(),front=o.pages.frontPage();
    o.pixels.apply(briefingShellEffects(o.assets.artGroups.at(0),o.assets.titleFont,back));
    o.pixels.apply(briefingTextEffects(text_,page,o.assets.scoreFont,back));
    if(painted_!=page) {viewer_.select(page,o.assets);painted_=page;}
    if(briefingPreview(page)) {o.pixels.apply(briefingStripEffects(viewer_.stripIndex(),o.assets,back));}
    o.pixels.apply(viewer_.poll(o.assets,back));
    o.pixels.apply(briefingDetailsEffects(page,o.assets.titleFont,o.assets.scoreFont,back));
    for(std::size_t widget=0;widget<pointers.size();++widget) {
        o.pixels.apply(briefingWidgetEffects(o.assets.artGroups.at(0),o.assets.titleFont,widget,
            pressed_&&*pressed_==widget ? MenuWidgetState::pressed : MenuWidgetState::released,back));
    }
    p.copy(back,front);paintPlayableMenuPointer(o.pixels,o.assets.artGroups.at(0).at(0),pointer_.value_or(pointers.at(selection_)),back,front);
    auto flip=o.clock.renderPageFlip(o.phase,{PageFlipSynchronization::enabled,front,back,0x3d4,
        PageFlipLayout::recoveredStartup,PageFlipWaitDomain::retainedProgram},240000);
    p.append(std::move(flip.work));p.publish();
}
}
