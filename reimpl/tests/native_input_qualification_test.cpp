#include "stellar7/gameplay.hpp"
#include <stdexcept>
#include <iostream>
using namespace stellar7;
int main() {
    try {
        for(unsigned variant=0;variant<5;++variant) {
            GameplayModel model;const auto ticks=model.ticks();const auto phase=model.phase();
            GameplayInput input;
            switch(variant) {
            case 0:input.joystick=NativeJoystickSample{true,32768,32768,0};break;
            case 1:input.analog=JoystickAxes{127,-127};break;
            case 2:input.joystickAction=GameplaySystemAction::activateSelected;break;
            case 3:input.sourceKey=0x011b;break;
            case 4:input.systemAction=GameplaySystemAction::calibrateJoystick;break;
            }
            bool rejected=false;
            try {model.tick(input);} catch(const std::invalid_argument&) {rejected=true;}
            if(!rejected||model.ticks()!=ticks||model.phase()!=phase) {throw std::runtime_error("SourceWork admitted native input or mutated before rejection");}
        }
        std::cout<<"Native inputs rejected before SourceWork mutation PASS\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
