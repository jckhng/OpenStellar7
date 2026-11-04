#include "stellar7/render_work.hpp"
#include "stellar7/failure_model_work.hpp"
#include <cassert>
#include <stdexcept>

namespace {
using namespace stellar7;
template<class Action> void rejects(Action action) {
    bool rejected=false;
    try { action(); } catch (const std::invalid_argument&) { rejected=true; }
    assert(rejected);
}
std::uint64_t dispatches(const WorldModelCullWork& work) {
    std::uint64_t count=0;
    for (const auto& operation:work.operations) {
        assert(operation.kind==NormalWorkKind::instructions);
        count+=operation.count;
    }
    return count;
}
void sameOperations(const WorldModelCullWork& first,const WorldModelCullWork& second) {
    assert(first.operations.size()==second.operations.size());
    for (std::size_t index=0;index<first.operations.size();++index) {
        assert(first.operations[index].kind==second.operations[index].kind);
        assert(first.operations[index].count==second.operations[index].count);
        assert(first.operations[index].value==second.operations[index].value);
    }
}
Model3d model() {
    Model3d value{};
    value.flags=1; value.collisionRadius=1342;
    ModelLod lod{}; lod.flags=128; lod.threshold=255;
    ModelGroup group{}; group.vertexBlock=1; group.animationOffset=-1;
    group.vertices={{{0,0,0}}}; lod.groups={group,group};
    value.lods={lod}; return value;
}
}
int main() {
    using namespace stellar7;
    constexpr std::uint32_t radius=1342;
    //622 source ship-center dispatches; two negative-X abs walks add8.
    for (const auto y:{4224,5000,10000,30000}) {
        for (const auto x:{-1342,-1296,-1,0,1,1296,1342}) {
            const FixedPosition center{x,y,0};
            const auto work=level1AxisTankModelCullOperations(center);
            assert(!work.culled && !work.clipped);
            assert(work.projectedRadius==radius*128/(y-radius));
            const auto expected=622U+(x<0 ? 8U : 0U)-((x==-1342 || x==1342) ? 18U : 0U);
            assert(dispatches(work)==expected);
            //Same source branches as retained full-view sphere: neither viewport clips.
            sameOperations(work,failureModelCullOperations(center,radius,{0,318,0,199}));
            rejects([&] { worldModelCullOperations(center,radius); });
        }
    }
    assert(dispatches(level1AxisTankModelCullOperations({-1296,4224,0}))==630);
    for (const auto center:{FixedPosition{1343,4224,0},FixedPosition{-1343,4224,0},
        FixedPosition{0,1469,0},FixedPosition{0,1470,0},FixedPosition{0,4000,0},
        FixedPosition{0,30659,0},FixedPosition{0,4224,1},FixedPosition{0,4224,-1}}) {
        rejects([&] { level1AxisTankModelCullOperations(center); });
    }
    const auto tank=model();
    const FixedPosition center{-1296,4224,0};
    const WorldModelViewInput view{makeFixedMatrixQ14(0,0,0),center};
    const auto work=level1AxisTankModelSetupOperations(center,{0,0,0},59,tank,256,256,view);
    assert(work.translation==center && work.selectedLodIndex==0 && work.groupOrder.size()==2);
    rejects([&] { worldModelSetupOperations(center,{0,0,0},59,1,tank,256,256,view); });
    rejects([&] { level1AxisTankModelSetupOperations(center,{1,0,0},59,tank,256,256,view); });
    rejects([&] { level1AxisTankModelSetupOperations(center,{0,1,0},59,tank,256,256,view); });
    rejects([&] { level1AxisTankModelSetupOperations(center,{0,0,0},58,tank,256,256,view); });
    rejects([&] { level1AxisTankModelSetupOperations(center,{0,0,0},59,tank,44,256,view); });
    auto wrong=tank; wrong.collisionRadius=748;
    rejects([&] { level1AxisTankModelSetupOperations(center,{0,0,0},59,wrong,256,256,view); });
}
