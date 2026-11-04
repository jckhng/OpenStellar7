#include "stellar7/type42_pickup_work.hpp"
#include "stellar7/indexed_render.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <vector>
using namespace stellar7;
struct PickupControl {std::uint8_t payload;unsigned parity;std::size_t targetIndex;std::uint32_t incoming;};
constexpr std::array<PickupControl,2> controls{{{7,0,1,60},{3,1,2,150}}};
Level1Type42PickupInput input(const PickupControl c){
    Level1Type42PickupInput result{};result.pickup.allocationIndex=6;result.pickup.type=42;
    result.pickup.collisionRadius=330;result.pickup.position={1234,-5678,100};
    result.pickup.stateBytes4dTo52={254,1,c.payload,0,0,c.payload};
    result.systems.fill(3);result.score=c.incoming;
    result.allocationOrder={{Level1PresentationKind::entity,0},{Level1PresentationKind::projectile,0}};
    result.allocationOrder.insert(result.allocationOrder.begin()+c.targetIndex,{Level1PresentationKind::entity,6});return result;
}
std::unique_ptr<IndexedPages> pixels(){
    auto result=std::make_unique<IndexedPages>();
    for(std::uint16_t index=0;index<3;++index){const auto page=static_cast<std::uint16_t>(0xa000+index*0x400);
        for(std::int16_t y=0;y<200;++y){for(std::int16_t x=0;x<320;++x){
            result->write(page,{x,y},static_cast<std::uint8_t>((x*29+y*17)^(x>>2)^(index*83)));}}}
    return result;
}
std::uint64_t hash(const IndexedPages& value,const std::uint16_t page){
    auto result=14695981039346656037ULL;for(std::int16_t y=0;y<200;++y){for(std::int16_t x=0;x<320;++x){
        result=(result^value.read(page,{x,y}))*1099511628211ULL;}}return result;
}
struct Op {int kind;std::uint64_t count;std::uint16_t value=0;};
void add(std::vector<Op>& out,const NormalWorkOperation op){
    const auto kind=static_cast<int>(op.kind);
    if(kind==0&&!out.empty()&&out.back().kind==0){out.back().count+=op.count;}
    else{out.push_back({kind,op.count,op.value});}
}
using Pixel=std::array<std::uint32_t,4>;
void apply(IndexedPages& pages,std::vector<Pixel>& writes,const std::vector<IndexedPageEffect>& effects){
    for(const auto& effect:effects){
        if(const auto* pixel=std::get_if<RadarPixelWrite>(&effect)){
            writes.push_back({pixel->page,static_cast<std::uint32_t>(pixel->point.x),static_cast<std::uint32_t>(pixel->point.y),pixel->color});
        }else{const auto& copy=std::get<RadarPageCopy>(effect);
            for(std::uint16_t y=0;y<copy.height;++y){for(std::uint16_t x=0;x<copy.coveredWidth;++x){
                const ProjectedPoint point{static_cast<std::int16_t>(copy.position.x+x),static_cast<std::int16_t>(copy.position.y+y)};
                writes.push_back({copy.destinationPage,static_cast<std::uint32_t>(point.x),static_cast<std::uint32_t>(point.y),pages.read(copy.sourcePage,point)});}}}
        pages.apply(std::vector<IndexedPageEffect>{effect});
    }
}

#include <cassert>
#include <iostream>
#include <limits>
#include <stdexcept>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif
namespace {
template<class Action>void rejects(Action action){bool rejected=false;try{action();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}
BitmapFont font(){BitmapFont f{32,8,0,{}};for(std::size_t i=0;i<96;++i){
    const auto width=static_cast<std::uint16_t>(i==0?3:2+i%4);IndexedBitmap glyph{width,8,std::vector<std::uint8_t>(width*8)};
    for(std::size_t j=0;j<glyph.pixels.size();++j){glyph.pixels[j]=i==0?0:static_cast<std::uint8_t>((j+i)%3==0);}f.glyphs.push_back(std::move(glyph));}return f;}
std::vector<IndexedBitmap> art(){std::vector<IndexedBitmap> result(57,{4,1,{1,0,2,0}});
    for(const auto item:std::array<std::array<std::uint16_t,3>,3>{{{43,16,20},{47,40,12},{49,8,3}}}){
        auto& frame=result[item[0]];frame={item[1],item[2],std::vector<std::uint8_t>(item[1]*item[2])};
        for(std::size_t j=0;j<frame.pixels.size();++j){frame.pixels[j]=j%3?static_cast<std::uint8_t>(j%13+1):0;}}
    return result;}
void control(const PickupControl c,const BitmapFont& scoreFont,const std::vector<IndexedBitmap>& group){
    auto roles=CockpitPageState::fromRecoveredMenu();if(c.parity){roles.flip();}auto before=input(c);
    before.pickup.flags=0x8c;before.pickup.pitch=123;before.pickup.rollVelocity=2048;
    before.pickup.forwardVelocity=13;before.pickup.presenterDistance=65000;before.pickup.targetHeading=12345;
    before.pickup.collisionContactIsPlayer=true;before.pickup.ownerAllocationIndex=7;
    const auto work=level1Type42PickupOperations(before,scoreFont,group,roles);
    assert(work.score==c.incoming+100&&work.systems==before.systems&&work.pickup.stateBytes4dTo52[1]==0);
    assert(work.pickup.stateBytes4dTo52[0]==254&&work.pickup.stateBytes4dTo52[2]==c.payload&&work.pickup.stateBytes4dTo52[5]==c.payload);
    assert(work.pickup.flags==before.pickup.flags&&work.pickup.pitch==before.pickup.pitch&&work.pickup.position==before.pickup.position);
    assert(work.pickup.rollVelocity==2048&&work.pickup.forwardVelocity==13&&work.pickup.presenterDistance==65000&&work.pickup.targetHeading==12345);
    assert(work.pickup.collisionContactIsPlayer&&work.pickup.ownerAllocationIndex==7);
    assert(work.removal.removedIndex==c.targetIndex&&work.removal.lastIndex==2&&work.removal.replacement==before.allocationOrder.back());
    assert(work.allocationOrder==std::vector<Level1ObjectRef>({{Level1PresentationKind::entity,0},{Level1PresentationKind::projectile,0}}));
    assert(work.refreshRequested&&work.drawPage==roles.backPage()&&work.copyPage==roles.frontPage());
    assert(work.fillStyle==1&&work.fillColor==137&&work.selectedFont==0&&work.steps.size()==5);
    const auto& score=std::get<ScoreWork>(work.steps[1]);assert(score.score==work.score&&score.fontSelections.size()==2);
    assert(!score.fontSelections[0].guestElapsedQualified&&!score.fontSelections[1].guestElapsedQualified);
    const auto audio=std::get<Level1Type42PickupAudio>(work.steps[3]);
    assert(audio.event==24&&audio.callerBegin==0x9103&&audio.callerEnd==0x910d&&audio.callerDispatches==4);
    auto actual=pixels();auto expected=std::make_unique<IndexedPages>(*actual);std::vector<Pixel> writes;
    expected->apply(scorePageEffects(work.score,scoreFont,roles.backPage(),roles.frontPage(),CockpitPageLayout::recoveredStartup));
    expected->apply(statusRepairPageEffects(c.payload-1,work.systems,before.timers,group,roles.backPage(),roles.frontPage(),CockpitPageLayout::recoveredStartup));
    for(const auto& step:work.steps){if(const auto* body=std::get_if<Level1Type42PickupDrawing>(&step)){apply(*actual,writes,body->effects);}
        else if(const auto* child=std::get_if<ScoreWork>(&step)){apply(*actual,writes,child->effects);}}
    for(const std::uint16_t page:{0xa000,0xa400,0xa800}){assert(hash(*actual,page)==hash(*expected,page));}
}
}
int main(){
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    try{const auto scoreFont=font();const auto group=art();const auto roles=CockpitPageState::fromRecoveredMenu();
        for(const auto c:controls){control(c,scoreFont,group);}
        auto in=input(controls.front());in.systems[6]=2;rejects([&]{level1Type42PickupOperations(in,scoreFont,group,roles);});
        in=input(controls.front());in.systems[6]=-3;rejects([&]{level1Type42PickupOperations(in,scoreFont,group,roles);});
        in=input(controls.back());in.timers[2]=1;rejects([&]{level1Type42PickupOperations(in,scoreFont,group,roles);});
        in=input(controls.front());in.selectedSystem=6;rejects([&]{level1Type42PickupOperations(in,scoreFont,group,roles);});
        in=input(controls.front());in.pickup.stateBytes4dTo52[5]=1;rejects([&]{level1Type42PickupOperations(in,scoreFont,group,roles);});
        in=input(controls.front());in.drawCallback=0x130c;rejects([&]{level1Type42PickupOperations(in,scoreFont,group,roles);});
        in=input(controls.front());in.pickup.stateBytes4dTo52[4]=1;rejects([&]{level1Type42PickupOperations(in,scoreFont,group,roles);});
        in=input(controls.front());in.allocationOrder.push_back(in.allocationOrder.back());rejects([&]{level1Type42PickupOperations(in,scoreFont,group,roles);});
        in=input(controls.front());in.allocationOrder.erase(in.allocationOrder.begin()+1);rejects([&]{level1Type42PickupOperations(in,scoreFont,group,roles);});
        in=input(controls.front());in.score=2147483548U;rejects([&]{level1Type42PickupOperations(in,scoreFont,group,roles);});
        Level1Type42PickupInput empty{};rejects([&]{level1Type42PickupOperations(empty,scoreFont,group,roles);});
        rejects([&]{scoreOperations(0,100,0,scoreFont,roles);});
        rejects([&]{scoreOperations(0,100,0,scoreFont,roles,ScoreAwardDomain::initialOrdinary);});
        rejects([&]{scoreOperations(0,20,0,scoreFont,roles,ScoreAwardDomain::systemBonus);});
        rejects([&]{scoreOperations(0,50,0,scoreFont,roles,ScoreAwardDomain::systemBonus);});
        assert(scoreOperations(0,20,0,scoreFont,roles).score==20);
        assert(scoreOperations(0,50,0,scoreFont,roles,ScoreAwardDomain::initialOrdinary).score==50);
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}return 0;
}
