#include "native_level2_frame_fixture.hpp"

#include <exception>
#include <iostream>

int main(int argc,char** argv) {
    if(argc!=2) {
        return 2;
    }
    try {
        for(const std::uint8_t type:{1,4}) {
            stellar7::test::verifyMissingWeightedPlayableAdmission(argv[1],type);
        }
        std::cout<<"Missing weighted1/4 native/direct admission rejects atomically\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        return 1;
    }
}
