#include "stellar7/type42_pickup_work.hpp"
#include "stellar7/indexed_render.hpp"
#include "stellar7/render_work.hpp"
#include "stellar7/ui_bitmap_work.hpp"
#include "stellar7/ui_fill_work.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
constexpr std::size_t registryCapacity = 64;
constexpr std::uint8_t restoredStatus = 3;
constexpr std::uint8_t readyColor = 137; //Immutable12EC[1].
constexpr std::int16_t pickupAward = 100;
constexpr Viewport display{0,319,0,199};
constexpr std::array<CockpitCopyRegion,7> rectangles{{{7,151,29,17},{7,167,29,17},
    {35,151,26,33},{60,151,30,17},{60,167,30,17},{7,183,41,17},{47,183,43,17}}};
constexpr std::array<ProjectedPoint,7> icons{{{14,156},{17,170},{42,155},{64,153},
    {66,172},{14,187},{50,186}}};
constexpr std::size_t pipCount = 3;
constexpr std::array<ProjectedPoint,21> pips{{{10,154},{10,158},{10,162},{10,170},{10,174},{10,178},
    {42,178},{46,178},{50,178},{84,154},{84,158},{84,162},{84,170},{84,174},{84,178},
    {10,186},{10,190},{10,194},{84,186},{84,190},{84,194}}};

void instructions(Level1Type42PickupDrawing& work,const std::uint64_t count) {
    work.operations.push_back({NormalWorkKind::instructions,count});
}
template<class Work>void append(Level1Type42PickupDrawing& result,const Work& child) {
    result.operations.insert(result.operations.end(),child.operations.begin(),child.operations.end());
    result.effects.insert(result.effects.end(),child.effects.begin(),child.effects.end());
}
std::size_t validate(const Level1Type42PickupInput& input) {
    const auto& pickup=input.pickup;
    const auto payload=pickup.stateBytes4dTo52[5];
    if (pickup.type!=42 || pickup.collisionRadius!=330 || input.drawCallback!=0
        || pickup.stateBytes4dTo52[0]!=254 || pickup.stateBytes4dTo52[1]!=1
        || pickup.stateBytes4dTo52[4]!=0 || (payload!=3 && payload!=7)
        || pickup.stateBytes4dTo52[2]!=payload || input.selectedSystem!=-1 || input.difficulty!=0
        || !std::all_of(input.systems.begin(),input.systems.end(),[](const auto v){return v==restoredStatus;})
        || !std::all_of(input.timers.begin(),input.timers.end(),[](const auto v){return v==0;})
        || input.allocationOrder.empty() || input.allocationOrder.size()>registryCapacity) {
        throw std::invalid_argument("unsupported initial Type42 player pickup");
    }
    for (std::size_t index=0;index<input.allocationOrder.size();++index) {
        const auto ref=input.allocationOrder[index];
        if ((ref.kind!=Level1PresentationKind::entity && ref.kind!=Level1PresentationKind::projectile
                && ref.kind!=Level1PresentationKind::deathEffect)
            || (ref.kind==Level1PresentationKind::projectile && ref.index>=Level1State::projectilePoolSize)
            || (ref.kind==Level1PresentationKind::deathEffect && ref.index>=Level1State::deathEffectPoolSize)
            || std::find(input.allocationOrder.begin(),input.allocationOrder.begin()+index,ref)
                !=input.allocationOrder.begin()+index) {
            throw std::invalid_argument("Type42 pickup requires unique owned registry refs");
        }
    }
    const auto found=std::find(input.allocationOrder.begin(),input.allocationOrder.end(),
        Level1ObjectRef{Level1PresentationKind::entity,pickup.allocationIndex});
    if (found==input.allocationOrder.end()) {
        throw std::invalid_argument("Type42 pickup requires its live registry identity");
    }
    return static_cast<std::size_t>(found-input.allocationOrder.begin());
}
}

Level1Type42PickupWork level1Type42PickupOperations(const Level1Type42PickupInput& input,
    const BitmapFont& scoreFont,const std::vector<IndexedBitmap>& artGroup4,const CockpitPageState& pages) {
    const auto removedIndex=validate(input);
    const auto system=static_cast<std::size_t>(input.pickup.stateBytes4dTo52[5]-1);
    //Independent semantic provider validates resources and the same physical roles.
    statusRepairPageEffects(system,input.systems,input.timers,artGroup4,
        pages.backPage(),pages.frontPage(),CockpitPageLayout::recoveredStartup);
    const auto score=scoreOperations(input.score,pickupAward,input.difficulty,scoreFont,pages,ScoreAwardDomain::systemBonus);
    const auto lastIndex=input.allocationOrder.size()-1;
    Level1Type42PickupWork result{input.pickup,score.score,input.systems,
        {removedIndex,lastIndex,input.allocationOrder.back()},input.allocationOrder,true,
        pages.backPage(),pages.frontPage(),1,readyColor,0,{}};
    result.pickup.stateBytes4dTo52[1]=0;
    result.systems[system]=restoredStatus;
    result.allocationOrder[removedIndex]=result.allocationOrder.back();
    result.allocationOrder.pop_back();
    Level1Type42PickupDrawing prefix{};
    instructions(prefix,7); //082C..083B:health0/payload/CALL90F2.
    instructions(prefix,7); //90F2..90FD:entry and score100 CALL7970.
    result.steps.push_back(std::move(prefix));
    result.steps.push_back(score);
    Level1Type42PickupDrawing cleanup{};
    instructions(cleanup,1); //9100 score argument cleanup.
    result.steps.push_back(std::move(cleanup));
    result.steps.push_back(Level1Type42PickupAudio{}); //9103..910A owned by audio executor.

    Level1Type42PickupDrawing repair{};
    instructions(repair,6); //910D..911B positive status restore/CALL8ED2.
    instructions(repair,15); //8ED2..8EFA status3/timer0 branch/CALL8F40.
    instructions(repair,36); //8F40..8F92, style/color/page setup and fill caller.
    const auto rectangle=rectangles[system];
    append(repair,uiFillOperations({static_cast<std::int16_t>(rectangle.x+2),
        static_cast<std::int16_t>(rectangle.y+2),static_cast<std::int16_t>(rectangle.width-4),
        static_cast<std::int16_t>(rectangle.height-4)},readyColor,pages.backPage()));
    instructions(repair,16); //8F97..8FCF status-nonzero icon41+system caller.
    append(repair,uiBitmapOperations(artGroup4.at(41+system),icons[system],display,pages.backPage()));
    instructions(repair,10); //8FD4..8FED pip table and index setup.
    instructions(repair,1); //8FEF initial index load, outside8FF2 loop.
    for (std::size_t pip=0;pip<pipCount;++pip) {
        instructions(repair,15); //8FF2..9019 status3>pip uses bitmap49.
        append(repair,uiBitmapOperations(artGroup4.at(49),pips[system*pipCount+pip],display,pages.backPage()));
        instructions(repair,5); //901E..9028 cleanup/next/branch, including last test.
    }
    instructions(repair,8); //902A..903E COPY=FRONT and resident-copy caller.
    instructions(repair,1); //22A1 installed106A JMPF.
    const auto copy=planarCopyOperations(rectangle.x,rectangle.width,rectangle.height);
    repair.operations.insert(repair.operations.end(),copy.begin(),copy.end());
    const auto covered=planarCopyWork(rectangle.x,rectangle.width,rectangle.height);
    repair.effects.emplace_back(RadarPageCopy{pages.backPage(),pages.frontPage(),
        {static_cast<std::int16_t>(rectangle.x-rectangle.x%4),rectangle.y},
        static_cast<std::uint16_t>(4*covered.stringIterations/rectangle.height),static_cast<std::uint16_t>(rectangle.height)});
    instructions(repair,6); //9043..904B 8F40 epilogue.
    instructions(repair,7); //8EFD..8F1B selection-1 branch/return.
    instructions(repair,5); //911E..9125 return90F2.
    instructions(repair,3); //083E..0842 cleanup and CALL481A.
    instructions(repair,18+7*removedIndex+(removedIndex==lastIndex?0:5)); //481A search/swap/return.
    instructions(repair,2); //0845 cleanup and0848 JMP094F.
    result.steps.push_back(std::move(repair));
    return result;
}

} // namespace stellar7
