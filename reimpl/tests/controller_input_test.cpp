#include "controller_input.hpp"
#include <iostream>
#include <stdexcept>
namespace {
void require(bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
}
int main() try {
    using namespace stellar7;
    ControllerInput neutral;neutral.connected=true;
    auto firing=neutral;firing.set(ControllerButton::fire,true);
    ControllerState state;state.poll(firing,true);
    require(!state.held(ControllerButton::fire),"controller bypassed neutral connection gate");
    state.poll(neutral,true);state.poll(firing,true);
    require(state.held(ControllerButton::fire)&&state.pressed(ControllerButton::fire),"controller fire edge lost");
    state.poll(firing,true);require(!state.pressed(ControllerButton::fire),"held input repeated an action");
    state.poll(firing,false);state.poll(firing,true);
    require(!state.held(ControllerButton::fire),"focus return bypassed neutral gate");
    state.poll(neutral,true);state.poll(firing,true);state.poll({},true);
    require(!state.held(ControllerButton::fire),"disconnect retained fire");
    ControllerMailbox mailbox;mailbox.publish(neutral,100);mailbox.take(100);
    mailbox.publish(firing,110);mailbox.publish(neutral,120);
    require(mailbox.take(120).down(ControllerButton::fire),"render/main boundary dropped a short tap");
    require(!mailbox.take(120).down(ControllerButton::fire),"render/main boundary dropped release");
    mailbox.publish(firing,130);mailbox.publish({},140);
    require(!mailbox.take(140).connected,"focus loss replayed queued input");
    mailbox.publish(neutral,141);mailbox.take(141);mailbox.publish({},142);mailbox.publish(firing,143);
    require(!mailbox.take(143).connected,"reconnect lost cancellation edge between consumer polls");
    mailbox.publish(firing,150);require(!mailbox.take(401).connected,"stale worker input remained active");
    for(unsigned i=0;i<17;++i) {mailbox.publish(i%2?neutral:firing,500+i);}
    require(!mailbox.take(517).connected,"overflow did not cancel controls before reconnecting");
    std::cout<<"Controller neutral/focus/disconnect, short taps, stale input and overflow pass\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
