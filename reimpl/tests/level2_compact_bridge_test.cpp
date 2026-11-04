#include "stellar7/gameplay.hpp"
#include "stellar7/level2_compact_bridge.hpp"

#include <cassert>
#include <stdexcept>

int main() {
    using namespace stellar7;
    const Level2CompactBridge bridge({0,16,32,40,9,17});
    assert((bridge.compactToSemantic==std::vector<std::size_t>{0,1,4,5}));
    assert((bridge.physicalKeys==std::vector<std::size_t>{0,16,9,17}));
    assert(bridge.compact(4)==2);
    Level1Entity retained{};
    retained.ownerIsPlayer=false;retained.ownerAllocationIndex=4;
    retained.collisionContactIsPlayer=false;retained.collisionContactAllocationIndex=0;
    bridge.project(retained);
    assert(retained.ownerAllocationIndex==2);
    assert(retained.collisionContactAllocationIndex==0);
    retained.ownerAllocationIndex=Level2CompactBridge::absent;
    retained.collisionContactAllocationIndex=Level2CompactBridge::absent;
    bridge.project(retained);
    assert(retained.ownerAllocationIndex==Level2CompactBridge::absent);
    bool rejected=false;
    try {const Level2CompactBridge invalid({0,0});} catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
    rejected=false;
    try {(void)bridge.compact(2);} catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected);
}
