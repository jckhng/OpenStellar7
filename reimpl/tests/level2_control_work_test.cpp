#include "stellar7/frame_tail_work.hpp"
#include "stellar7/simulation_prefix_work.hpp"
#include <cassert>
#include <stdexcept>
#include <utility>
#include "level2_control_prefix_fixture.hpp"
using namespace stellar7;
namespace {
template<class Function> void rejectsControl(Function function) {
    bool rejected=false;try { function(); } catch(const std::invalid_argument&) { rejected=true; }assert(rejected);
}
std::vector<std::pair<int,std::uint64_t>> canonical(const std::vector<NormalWorkOperation>& work) {
    std::vector<std::pair<int,std::uint64_t>> result;
    for(const auto& op:work) {
        const auto kind=static_cast<int>(op.kind);
        if(!result.empty() && result.back().first==kind) { result.back().second+=op.count; }
        else { result.push_back({kind,op.count}); }
    }
    return result;
}
void sourceTails() {
    { //periodic-priority-negative-clamp
        FrameTailState state{};state.loopTick=16;state.playerPosition={24,-31,0};
        state.yawVelocity=432;state.forwardVelocity=-195;state.actors={{8,{12288,24576,0}},{8,{-20480,4096,0}},{8,{24576,-8192,0}},{13,{-24576,28672,0}},{13,{12288,-24576,0}},{30,{-8192,20480,0}},{30,{8192,-20480,0}}};
        FrameTailGuards guards{};guards.armor=680;guards.missionActive=true;
        guards.keyboardHead=guards.keyboardTail=0x1e;guards.heldKeys={false,true,false,false,true,false,true,true,false};
        guards.periodic=FrameTailPeriodicInput{1,5,5,12,0,82,32,0};
        rejectsControl([&] { frameTailOperations(state,guards); });
        const auto work=level2ControlFrameTailOperations(state,guards);
        const std::vector<std::pair<int,std::uint64_t>> expected={{0,41},{5,1},{0,7},{6,1},{0,7},{5,1},{0,7},{6,1},{0,8},{5,1},{0,7},{6,1},{0,7},{5,1},{0,7},{6,1},{0,7},{5,1},{0,7},{6,1},{0,8},{5,1},{0,7},{6,1},{0,7},{5,1},{0,7},{6,1},{0,8},{5,1},{0,7},{6,1},{0,12},{5,1},{0,8},{7,1},{0,10},{5,1},{0,7},{6,1},{0,432}};
        assert(canonical(work.operations)==expected);
        assert(work.state.loopTick==17 && work.state.yawVelocity==512
            && work.state.forwardVelocity==-200 && work.state.playerPosition==FixedPosition{});
        const std::vector<FixedPosition> positions={{12264,24607,0},{-20504,4127,0},{24552,-8161,0},{-24600,28703,0},{12264,-24545,0},{-8216,20511,0},{8168,-20449,0}};
        for(std::size_t i=0;i<positions.size();++i) { assert(work.state.actors[i].position==positions[i]); }
        assert(work.state.scheduler==FrameTailScheduler::zeroThresholdDraw);
        auto invalid=guards;invalid.firstFrame=true;
        rejectsControl([&] { level2ControlFrameTailOperations(state,invalid); });
        invalid=guards;invalid.heldKeys[8]=true;
        rejectsControl([&] { level2ControlFrameTailOperations(state,invalid); });
    }
    { //adjacent-opposite-positive-clamp
        FrameTailState state{};state.loopTick=17;state.playerPosition={24,-31,0};
        state.yawVelocity=-480;state.forwardVelocity=195;state.actors={{8,{12288,24576,0}},{8,{-20480,4096,0}},{8,{24576,-8192,0}},{13,{-24576,28672,0}},{13,{12288,-24576,0}},{30,{-8192,20480,0}},{30,{8192,-20480,0}}};
        FrameTailGuards guards{};guards.armor=680;guards.missionActive=true;
        guards.keyboardHead=guards.keyboardTail=0x1e;guards.heldKeys={false,false,false,false,true,false,false,true,false};
        rejectsControl([&] { frameTailOperations(state,guards); });
        const auto work=level2ControlFrameTailOperations(state,guards);
        const std::vector<std::pair<int,std::uint64_t>> expected={{0,41},{5,1},{0,7},{6,1},{0,7},{5,1},{0,7},{6,1},{0,7},{5,1},{0,7},{6,1},{0,7},{5,1},{0,7},{6,1},{0,7},{5,1},{0,7},{6,1},{0,8},{5,1},{0,7},{6,1},{0,7},{5,1},{0,7},{6,1},{0,7},{5,1},{0,7},{6,1},{0,12},{5,1},{0,8},{7,1},{0,10},{5,1},{0,7},{6,1},{0,287}};
        assert(canonical(work.operations)==expected);
        assert(work.state.loopTick==18 && work.state.yawVelocity==-512
            && work.state.forwardVelocity==200 && work.state.playerPosition==FixedPosition{});
        const std::vector<FixedPosition> positions={{12264,24607,0},{-20504,4127,0},{24552,-8161,0},{-24600,28703,0},{12264,-24545,0},{-8216,20511,0},{8168,-20449,0}};
        for(std::size_t i=0;i<positions.size();++i) { assert(work.state.actors[i].position==positions[i]); }
        assert(work.state.scheduler==FrameTailScheduler::none);
        auto invalid=guards;invalid.firstFrame=true;
        rejectsControl([&] { level2ControlFrameTailOperations(state,invalid); });
        invalid=guards;invalid.heldKeys[8]=true;
        rejectsControl([&] { level2ControlFrameTailOperations(state,invalid); });
    }
}
void prefixDomain() {
    auto input=fixture();
    const auto old=retainedLevel2PrefixOperations(input);
    const auto same=controlledLevel2PrefixOperations(input);
    assert(fields(old.player)==fields(same.player) && old.activeOrder==same.activeOrder);
    assert(canonical(old.operations)==canonical(same.operations));
    input.player.yaw=0x1300;input.player.yawVelocity=80;input.player.forwardVelocity=30;
    rejectsControl([&] { retainedLevel2PrefixOperations(input); });
    const auto next=controlledLevel2PrefixOperations(input);
    const auto player=flatKinematicsOperations(input.player).actor;
    assert(fields(next.player)==fields(player) && next.actors.size()==7 && next.activeOrder.size()==7);
    input.player.yawVelocity=513;
    rejectsControl([&] { controlledLevel2PrefixOperations(input); });
    input.player.yawVelocity=0;input.refreshRequested=true;
    rejectsControl([&] { controlledLevel2PrefixOperations(input); });
}
}
int main() { sourceTails();prefixDomain(); }
