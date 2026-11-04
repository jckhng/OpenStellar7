//Private proposal: do not execute until root promotes and owns the test run.
#include "playable_pointer.hpp"

#include <iostream>
#include <stdexcept>

namespace {
using namespace stellar7;
void require(bool value,const char* message) {if (!value) {throw std::runtime_error(message);}}
void run() {
    const auto& boxes=mainPointerHitBoxes();
    require(pointerWidgetHit(boxes,{123,64})==0 && pointerWidgetHit(boxes,{204,75})==0,
        "source inclusive main hit boundary changed");
    require(!pointerWidgetHit(boxes,{122,64}) && !pointerWidgetHit(boxes,{205,75}),
        "decorative bitmap admitted a non-hit pixel");
    require(pointerWidgetHit(preferencePointerHitBoxes(),{52,77})==1
        && !pointerWidgetHit(preferencePointerHitBoxes(),{84,80}),"toggle25 hit is not its source17x14 extent");
    auto poll=pointerWidgetPoll({},boxes,{160,69},PointerButton::held);
    require(poll.pressed==0 && !poll.activated,"mouse press activated early");
    poll=pointerWidgetPoll(poll.pressed,boxes,{160,69},PointerButton::held);
    require(poll.pressed==0 && !poll.released,"same held hit lost its pressed state");
    poll=pointerWidgetPoll(poll.pressed,boxes,{0,0},PointerButton::held);
    require(!poll.pressed && poll.released==0 && !poll.activated,"held move-off failed to cancel");
    poll=pointerWidgetPoll(poll.pressed,boxes,{160,69},PointerButton::released);
    require(!poll.activated,"cancelled press activated on later release");
    poll=pointerWidgetPoll({},boxes,{160,69},PointerButton::held);
    poll=pointerWidgetPoll(poll.pressed,boxes,{0,0},PointerButton::released);
    require(poll.activated==0,"source release incorrectly re-hit the retained active widget");
    for(const auto* table:{&mainPointerHitBoxes(),&preferencePointerHitBoxes(),&briefingPointerHitBoxes(),
        &scoresPointerHitBoxes(),&continuePointerHitBoxes(),&namePointerHitBoxes()}) {
        const ProjectedPoint point{table->front().x,table->front().y};
        auto cancelled=pointerWidgetPoll({},*table,point,PointerButton::held);
        require(cancelled.pressed==0,"cancel regression did not acquire its widget");
        cancelled=pointerWidgetPoll(cancelled.pressed,*table,{-100,-100},PointerButton::cancel);
        require(!cancelled.pressed&&cancelled.released==0&&!cancelled.activated,
            "explicit focus cancel did not release paint without activation");
        cancelled=pointerWidgetPoll(cancelled.pressed,*table,point,PointerButton::released);
        require(!cancelled.pressed&&!cancelled.activated,"focus-cancelled widget activated on release");
    }
    require(sourcePointerPosition({-1,300}).x==0 && sourcePointerPosition({-1,300}).y==196,
        "source5B9E pointer bounds changed");
    std::vector<std::uint8_t> scratch(500);
    scratch[0]=123;scratch[2]=64;scratch[4]=81;scratch[6]=11;
    require(pointerWidgetHit(sourceWidgetHitBoxes(scratch,1),{204,75})==0,"retained scratch hit disagrees");
    bool rejected=false;
    try {pointerWidgetPoll(9,boxes,{0,0},PointerButton::released);} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"invalid retained active widget admitted");
}
}
int main() {try {run();std::cout<<"Source pointer hits/hold/cancel/release PASS\n";return 0;}
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
