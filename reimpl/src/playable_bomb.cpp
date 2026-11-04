#include "playable_bomb.hpp"

#include <stdexcept>

namespace stellar7 {
std::uint16_t playableBombContactScore(const std::int16_t originalType) {
    //Pinned unpacked DS0A9C..0AFD; current campaign actor domain0..48.
    constexpr std::array<std::uint16_t,49> points{
        0,20,35,20,30,75,90,0,25,30,40,50,60,10,5,5,5,
        256,257,257,257,514,770,1027,1028,4,4106,3604,6674,9,12,6,10,8,4,4,6,6,5,2,6,6,4,3,3,46,50,32,48};
    if(originalType<0 || static_cast<std::size_t>(originalType)>=points.size()) {
        throw std::invalid_argument("Bomb contact score exceeds the actual campaign actor domain");
    }
    return points[originalType];
}

std::optional<std::size_t> playableBombSlot(
    const std::array<Level1Entity,playableBombFixedRecordCount>& records,
    const std::array<bool,playableBombFixedRecordCount>& known) {
    for(std::size_t slot=0;slot<records.size();++slot) {
        if(!known[slot]) {throw std::invalid_argument("Bomb4506 requires known FE history through its first free record");}
        if(records[slot].stateBytes4dTo52[1]==0) {return slot;}
    }
    return std::nullopt;
}

Level1Entity preparePlayableBombActor(const Level1Entity& retained,const Level1Entity& player,
    const std::uint32_t model43Radius) {
    if(retained.stateBytes4dTo52[1]!=0 || model43Radius==0) {
        throw std::invalid_argument("Bomb birth requires a free physical FE record and actual model43 radius");
    }
    auto bomb=retained;
    //4506 partial writes preserve selector/callback, velocities, cached fields and contacts.
    bomb.stateBytes4dTo52[0]=0xfe;
    bomb.position.z=0;
    bomb.stateBytes4dTo52[1]=100;
    bomb.pitch=bomb.roll=bomb.yaw=0;
    bomb.flags=0;
    bomb.type=43;
    bomb.collisionRadius=model43Radius;
    bomb.stateBytes4dTo52[4]=0;
    //8E20 overlays the player pose, grounds it before one real3B3A, then stops forward motion.
    bomb.stateBytes4dTo52[1]=1;
    bomb.position=player.position;
    bomb.position.z=0;
    bomb.pitch=player.pitch;bomb.roll=player.roll;bomb.yaw=player.yaw;
    bomb.forwardVelocity=-1000;
    advanceEntityKinematics(bomb);
    bomb.forwardVelocity=0;
    return bomb;
}
} //namespace stellar7
