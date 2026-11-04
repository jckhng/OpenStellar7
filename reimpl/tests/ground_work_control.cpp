#include "stellar7/render_work.hpp"
#include "stellar7/ground_steering_work.hpp"

#include <cassert>
#include <stdexcept>

namespace {
using namespace stellar7;
std::vector<NormalWorkOperation> groundCanonical(const std::vector<NormalWorkOperation>& input) {
    std::vector<NormalWorkOperation> result;
    for(const auto op:input) {
        if(op.kind==NormalWorkKind::instructions && !result.empty() && result.back().kind==op.kind) {result.back().count+=op.count;}
        else {result.push_back(op);}
    }
    return result;
}
void level1ThinGroundRegression() {
    struct Control {
        IndexedBitmap bitmap;ProjectedPoint position;
        std::vector<NormalWorkOperation> operations;
        std::vector<GroundPixelWrite> pixels;
    };
    //Ten cached335 controls plus root's one actual76 Level1 frame9 source child.
    const std::vector<Control> controls{
        {{16,3,{0,0,73,69,66,66,64,64,69,73,0,0,0,0,0,0,66,66,72,72,73,73,73,73,72,66,68,0,0,0,0,0,0,0,0,74,74,74,74,74,74,0,0,0,0,0,0,0}},{294,124},
            {{NormalWorkKind::instructions,13},{NormalWorkKind::repeatString,5},{NormalWorkKind::instructions,82},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,10},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,35},{NormalWorkKind::string,1},{NormalWorkKind::instructions,9},{NormalWorkKind::string,1},{NormalWorkKind::instructions,2},{NormalWorkKind::string,1},{NormalWorkKind::instructions,8},{NormalWorkKind::string,1},{NormalWorkKind::instructions,39},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,15},{NormalWorkKind::string,1},{NormalWorkKind::instructions,9},{NormalWorkKind::string,1},{NormalWorkKind::instructions,2},{NormalWorkKind::string,1},{NormalWorkKind::instructions,8},{NormalWorkKind::string,1},{NormalWorkKind::instructions,44},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,45},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,22}},
            {{{294,125},66},{{295,125},66}}},
        {{16,3,{0,0,73,69,66,66,64,64,69,73,0,0,0,0,0,0,66,66,72,72,73,73,73,73,72,66,68,0,0,0,0,0,0,0,0,74,74,74,74,74,74,0,0,0,0,0,0,0}},{295,124},
            {{NormalWorkKind::instructions,13},{NormalWorkKind::repeatString,5},{NormalWorkKind::instructions,82},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,10},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,35},{NormalWorkKind::string,1},{NormalWorkKind::instructions,9},{NormalWorkKind::string,1},{NormalWorkKind::instructions,2},{NormalWorkKind::string,1},{NormalWorkKind::instructions,8},{NormalWorkKind::string,1},{NormalWorkKind::instructions,43},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,44},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,45},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,22}},
            {{{295,125},66}}},
        {{16,3,{0,0,73,69,66,66,64,64,69,73,0,0,0,0,0,0,66,66,72,72,73,73,73,73,72,66,68,0,0,0,0,0,0,0,0,74,74,74,74,74,74,0,0,0,0,0,0,0}},{294,145},
            {{NormalWorkKind::instructions,13},{NormalWorkKind::repeatString,5},{NormalWorkKind::instructions,86},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,10},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,35},{NormalWorkKind::string,1},{NormalWorkKind::instructions,39},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,15},{NormalWorkKind::string,1},{NormalWorkKind::instructions,44},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,45},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,22}},
            {}},
        {{16,2,{0,74,69,66,66,64,66,72,0,0,0,0,0,0,0,0,68,74,73,72,72,72,72,74,68,0,0,0,0,0,0,0}},{294,145},
            {{NormalWorkKind::instructions,13},{NormalWorkKind::repeatString,5},{NormalWorkKind::instructions,86},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,10},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,35},{NormalWorkKind::string,1},{NormalWorkKind::instructions,39},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,15},{NormalWorkKind::string,1},{NormalWorkKind::instructions,2},{NormalWorkKind::string,1},{NormalWorkKind::instructions,43},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,45},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,22}},
            {{{295,145},74}}},
        {{16,4,{0,0,0,0,0,74,74,74,74,74,74,74,0,0,0,0,0,68,66,70,69,68,66,71,71,71,68,66,68,66,68,0,74,66,66,72,72,72,73,73,73,73,73,73,72,66,68,68,0,0,0,0,74,74,74,74,74,74,74,74,0,0,0,0}},{294,134},
            {{NormalWorkKind::instructions,13},{NormalWorkKind::repeatString,5},{NormalWorkKind::instructions,82},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,10},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,35},{NormalWorkKind::string,1},{NormalWorkKind::instructions,9},{NormalWorkKind::string,1},{NormalWorkKind::instructions,9},{NormalWorkKind::string,1},{NormalWorkKind::instructions,2},{NormalWorkKind::string,1},{NormalWorkKind::instructions,8},{NormalWorkKind::string,1},{NormalWorkKind::instructions,39},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,15},{NormalWorkKind::string,1},{NormalWorkKind::instructions,9},{NormalWorkKind::string,1},{NormalWorkKind::instructions,2},{NormalWorkKind::string,1},{NormalWorkKind::instructions,8},{NormalWorkKind::string,1},{NormalWorkKind::instructions,2},{NormalWorkKind::string,1},{NormalWorkKind::instructions,8},{NormalWorkKind::string,1},{NormalWorkKind::instructions,44},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,45},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,22}},
            {{{294,136},74},{{295,135},68},{{295,136},66}}},
        {{16,4,{0,0,0,0,0,74,74,74,74,74,74,74,0,0,0,0,0,68,66,70,69,68,66,71,71,71,68,66,68,66,68,0,74,66,66,72,72,72,73,73,73,73,73,73,72,66,68,68,0,0,0,0,74,74,74,74,74,74,74,74,0,0,0,0}},{295,134},
            {{NormalWorkKind::instructions,13},{NormalWorkKind::repeatString,5},{NormalWorkKind::instructions,82},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,10},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,35},{NormalWorkKind::string,1},{NormalWorkKind::instructions,9},{NormalWorkKind::string,1},{NormalWorkKind::instructions,9},{NormalWorkKind::string,1},{NormalWorkKind::instructions,2},{NormalWorkKind::string,1},{NormalWorkKind::instructions,8},{NormalWorkKind::string,1},{NormalWorkKind::instructions,43},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,44},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,45},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,22}},
            {{{295,136},74}}},
        {{16,2,{0,74,69,66,66,64,66,72,0,0,0,0,0,0,0,0,68,74,73,72,72,72,72,74,68,0,0,0,0,0,0,0}},{294,118},
            {{NormalWorkKind::instructions,13},{NormalWorkKind::repeatString,5},{NormalWorkKind::instructions,82},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,10},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,35},{NormalWorkKind::string,1},{NormalWorkKind::instructions,9},{NormalWorkKind::string,1},{NormalWorkKind::instructions,2},{NormalWorkKind::string,1},{NormalWorkKind::instructions,38},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,15},{NormalWorkKind::string,1},{NormalWorkKind::instructions,2},{NormalWorkKind::string,1},{NormalWorkKind::instructions,8},{NormalWorkKind::string,1},{NormalWorkKind::instructions,2},{NormalWorkKind::string,1},{NormalWorkKind::instructions,43},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,45},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,22}},
            {{{294,119},68},{{295,118},74},{{295,119},74}}},
        {{16,2,{0,74,69,66,66,64,66,72,0,0,0,0,0,0,0,0,68,74,73,72,72,72,72,74,68,0,0,0,0,0,0,0}},{295,118},
            {{NormalWorkKind::instructions,13},{NormalWorkKind::repeatString,5},{NormalWorkKind::instructions,82},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,10},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,35},{NormalWorkKind::string,1},{NormalWorkKind::instructions,9},{NormalWorkKind::string,1},{NormalWorkKind::instructions,2},{NormalWorkKind::string,1},{NormalWorkKind::instructions,42},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,44},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,45},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,22}},
            {{{295,119},68}}},
        {{16,2,{0,0,0,74,74,74,0,0,0,0,0,0,0,0,0,0,66,73,72,73,73,73,72,68,68,0,0,0,0,0,0,0}},{294,110},
            {{NormalWorkKind::instructions,13},{NormalWorkKind::repeatString,5},{NormalWorkKind::instructions,82},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,10},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,35},{NormalWorkKind::string,1},{NormalWorkKind::instructions,9},{NormalWorkKind::string,1},{NormalWorkKind::instructions,2},{NormalWorkKind::string,1},{NormalWorkKind::instructions,38},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,15},{NormalWorkKind::string,1},{NormalWorkKind::instructions,9},{NormalWorkKind::string,1},{NormalWorkKind::instructions,2},{NormalWorkKind::string,1},{NormalWorkKind::instructions,43},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,45},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,22}},
            {{{294,111},66},{{295,111},73}}},
        {{16,2,{0,0,0,74,74,74,0,0,0,0,0,0,0,0,0,0,66,73,72,73,73,73,72,68,68,0,0,0,0,0,0,0}},{295,110},
            {{NormalWorkKind::instructions,13},{NormalWorkKind::repeatString,5},{NormalWorkKind::instructions,82},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,10},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,35},{NormalWorkKind::string,1},{NormalWorkKind::instructions,9},{NormalWorkKind::string,1},{NormalWorkKind::instructions,2},{NormalWorkKind::string,1},{NormalWorkKind::instructions,42},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,44},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,45},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,22}},
            {{{295,111},66}}},
        {{8,3,{0,22,17,0,0,0,0,0,22,21,18,16,0,0,0,0,32,16,16,32,0,0,0,0}},{294,124},
            {{NormalWorkKind::instructions,13},{NormalWorkKind::repeatString,5},{NormalWorkKind::instructions,82},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,10},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,35},{NormalWorkKind::string,1},{NormalWorkKind::instructions,9},{NormalWorkKind::string,1},{NormalWorkKind::instructions,2},{NormalWorkKind::string,1},{NormalWorkKind::instructions,8},{NormalWorkKind::string,1},{NormalWorkKind::instructions,2},{NormalWorkKind::string,1},{NormalWorkKind::instructions,38},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,15},{NormalWorkKind::string,1},{NormalWorkKind::instructions,2},{NormalWorkKind::string,1},{NormalWorkKind::instructions,8},{NormalWorkKind::string,1},{NormalWorkKind::instructions,2},{NormalWorkKind::string,1},{NormalWorkKind::instructions,8},{NormalWorkKind::string,1},{NormalWorkKind::instructions,2},{NormalWorkKind::string,1},{NormalWorkKind::instructions,43},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,45},{NormalWorkKind::port8Write,1},{NormalWorkKind::instructions,22}},
            {{{294,125},22},{{294,126},32},{{295,124},22},{{295,125},21},{{295,126},16}}},
    };
    constexpr Viewport viewport{24,295,24,145};
    const StellarRng rng;
    for(const auto& c:controls) {
        GroundControlState state{};state.points.fill({static_cast<std::int16_t>(c.position.x*32),static_cast<std::int16_t>(c.position.y*32)});
        std::array<IndexedBitmap,8> bitmaps;bitmaps.fill(c.bitmap);
        const auto work=level1GroundSpriteExecution(state,rng,bitmaps,viewport);
        const auto bodySize=(work.operations.size()-2-2*state.points.size())/state.points.size();
        assert((work.operations.size()-2-2*state.points.size())%state.points.size()==0);
        for(std::size_t draw=0;draw<state.points.size();++draw) {
            const auto begin=work.operations.begin()+2+draw*(bodySize+2);
            const auto body=groundCanonical(std::vector<NormalWorkOperation>(begin,begin+bodySize));
            assert(body.size()==c.operations.size());
            for(std::size_t i=0;i<body.size();++i) {assert(body[i].kind==c.operations[i].kind && body[i].count==c.operations[i].count);}
        }
        assert(work.pixels.size()==c.pixels.size()*state.points.size() && work.rng.calls()==rng.calls());
        for(std::size_t i=0;i<work.pixels.size();++i) {
            const auto& wanted=c.pixels[i%c.pixels.size()];
            assert(work.pixels[i].point==wanted.point && work.pixels[i].color==wanted.color);
        }
        if(c.bitmap.width==16) {
            const auto old=groundSteeringExecution(state,rng,bitmaps,viewport);
            assert(old.operations.size()==work.operations.size() && old.pixels.size()==work.pixels.size());
            for(std::size_t i=0;i<old.operations.size();++i) {assert(old.operations[i].kind==work.operations[i].kind && old.operations[i].count==work.operations[i].count);}
        } else {
            bool rejected=false;try {groundSteeringExecution(state,rng,bitmaps,viewport);}catch(const std::invalid_argument&) {rejected=true;}
            assert(rejected); //Legacy8-wide thin admission stays strict.
        }
    }
    //Closed Level1 source8-wide height1..4 family, including both empty-plane cases.
    for(const std::uint16_t height:{1,2,3,4}) {
        for(const std::int16_t x:{294,295}) {
            IndexedBitmap bitmap{8,height,std::vector<std::uint8_t>(8*height,7)};
            std::array<IndexedBitmap,8> bitmaps;bitmaps.fill(bitmap);
            GroundControlState state{};state.points.fill({static_cast<std::int16_t>(x*32),static_cast<std::int16_t>(124*32)});
            const auto result=level1GroundSpriteExecution(state,rng,bitmaps,viewport);
            assert(result.pixels.size()==state.points.size()*(296-x)*height);
            assert(result.rng.calls()==rng.calls());
        }
    }
    const auto rejects=[&](const IndexedBitmap& bitmap,const Viewport view) {
        std::array<IndexedBitmap,8> bitmaps;bitmaps.fill(bitmap);
        GroundControlState state{};state.points.fill({294*32,124*32});
        bool rejected=false;try {level1GroundSpriteExecution(state,rng,bitmaps,view);}catch(const std::invalid_argument&) {rejected=true;}
        assert(rejected);
    };
    rejects({8,5,std::vector<std::uint8_t>(40,7)},viewport);
    rejects({24,6,std::vector<std::uint8_t>(144,7)},viewport);
    rejects({8,3,std::vector<std::uint8_t>(23,7)},viewport);
    rejects({8,3,std::vector<std::uint8_t>(24,7)},{24,294,24,145});
}

void level2ThinGroundRegression() {
    struct Control {std::uint16_t width,height;std::int16_t x,y;std::uint16_t columns,rows;std::uint64_t childDispatches;};
    //698B..69A5 right trim;6B3C/6BE3/6C8A/6D31 retain empty-plane OUT/JZ work.
    const std::array<Control,8> controls{{
        {24,6,294,139,2,6,437},{24,6,295,139,1,6,363},
        {24,6,294,141,2,5,417},{24,6,295,141,1,5,355},
        {24,6,294,145,2,1,321},{24,6,295,145,1,1,307},
        {8,1,294,103,2,1,317},{8,1,295,103,1,1,303}}};
    constexpr Viewport viewport{24,295,24,145};const StellarRng rng;
    const auto rejects=[](auto action) {
        bool rejected=false;try {action();}catch(const std::invalid_argument&) {rejected=true;}
        assert(rejected);
    };
    for(const auto c:controls) {
        IndexedBitmap bitmap{c.width,c.height,std::vector<std::uint8_t>(c.width*c.height)};
        for(std::size_t i=0;i<bitmap.pixels.size();++i) {bitmap.pixels[i]=static_cast<std::uint8_t>(i+1);}
        std::array<IndexedBitmap,8> bitmaps;bitmaps.fill(bitmap);
        GroundControlState state{};state.points.fill({static_cast<std::int16_t>(c.x*32),static_cast<std::int16_t>(c.y*32)});
        rejects([&] {groundSteeringExecution(state,rng,bitmaps,viewport);});
        const auto work=level2GroundSpriteExecution(state,rng,bitmaps,viewport);
        std::uint64_t dispatches=0,strings=0,ports=0;
        for(const auto operation:work.operations) {
            dispatches+=operation.kind==NormalWorkKind::repeatString ? 1 : operation.count;
            if(operation.kind==NormalWorkKind::repeatString || operation.kind==NormalWorkKind::string) {strings+=operation.count;}
            if(operation.kind==NormalWorkKind::port8Write) {ports+=operation.count;}
        }
        const auto pointCount=state.points.size();
        assert(dispatches==16+6+(pointCount-1)+pointCount*(47+7+c.childDispatches));
        assert(strings==pointCount*(5+2*c.columns*c.rows) && ports==5*pointCount);
        assert(work.rng.calls()==rng.calls() && work.pixels.size()==pointCount*c.columns*c.rows);
        std::size_t pixel=0;
        for(std::size_t point=0;point<pointCount;++point) {
            for(std::uint16_t plane=0;plane<c.columns;++plane) {
                for(std::uint16_t row=0;row<c.rows;++row) {
                    const auto& write=work.pixels[pixel++];
                    assert(write.point.x==c.x+plane && write.point.y==c.y+row);
                    assert(write.color==bitmap.pixels[row*c.width+plane]);
                }
            }
        }
        state.points.fill({293*32,static_cast<std::int16_t>(c.y*32)});
        const auto old=groundSteeringExecution(state,rng,bitmaps,viewport);
        const auto adjacent=level2GroundSpriteExecution(state,rng,bitmaps,viewport);
        assert(old.operations.size()==adjacent.operations.size() && old.pixels.size()==adjacent.pixels.size());
        for(std::size_t index=0;index<old.operations.size();++index) {
            assert(old.operations[index].kind==adjacent.operations[index].kind && old.operations[index].count==adjacent.operations[index].count);
        }
        state.points.fill({294*32,static_cast<std::int16_t>(c.y*32)});
        bitmaps.fill({c.width,c.height,std::vector<std::uint8_t>(c.width*c.height-1,7)});
        rejects([&] {level2GroundSpriteExecution(state,rng,bitmaps,viewport);});
    }
    GroundControlState state{};state.points.fill({294*32,139*32});
    std::array<IndexedBitmap,8> bitmaps;bitmaps.fill({24,5,std::vector<std::uint8_t>(120,7)});
    rejects([&] {level2GroundSpriteExecution(state,rng,bitmaps,viewport);});
    bitmaps.fill({8,2,std::vector<std::uint8_t>(16,7)});
    rejects([&] {level2GroundSpriteExecution(state,rng,bitmaps,viewport);});
    bitmaps.fill({24,6,std::vector<std::uint8_t>(144,7)});
    rejects([&] {level2GroundSpriteExecution(state,rng,bitmaps,{24,294,24,145});});
}

}

int main() {
    level1ThinGroundRegression();
    level2ThinGroundRegression();
    using namespace stellar7;
    GroundControlState state{{{{24 * 32, 90 * 32}, {295 * 32 + 31, 146 * 32 + 31},
        {100 * 32, 98 * 32}, {101 * 32, 99 * 32}, {102 * 32, 138 * 32},
        {103 * 32, 139 * 32}}}, 0, 0, 0};
    const auto work = groundControlOperations(state);
    assert(work.prefix.size() == 1 && work.prefix[0].count == 16);
    assert(work.suffix.size() == 1 && work.suffix[0].count == 6);
    const std::array<std::uint16_t, 6> shades{7, 0, 6, 5, 1, 0};
    std::uint64_t controlDispatches = work.prefix[0].count + work.suffix[0].count;
    for (std::size_t index = 0; index < work.draws.size(); ++index) {
        const auto& draw = work.draws[index];
        assert(draw.screen.x == state.points[index].x / 32);
        assert(draw.screen.y == state.points[index].y / 32);
        assert(draw.shadeIndex == shades[index]);
        assert(draw.prefix[0].kind == NormalWorkKind::instructions);
        assert(draw.prefix[0].count == 47);
        assert(draw.suffix[0].count == (index == 5 ? 7 : 8));
        controlDispatches += draw.prefix[0].count + draw.suffix[0].count;
    }
    assert(controlDispatches == 351);
    state.turn = 15;
    assert(groundControlOperations(state).draws[0].shadeIndex == 7);
    const auto rejected = [](const GroundControlState& candidate) {
        try {
            groundControlOperations(candidate);
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };
    auto invalid = state;
    invalid.turn = -1;
    assert(rejected(invalid));
    invalid = state;
    invalid.turn = 16;
    assert(rejected(invalid));
    invalid = state;
    invalid.speed = 1;
    assert(rejected(invalid));
    invalid = state;
    invalid.cameraHeight = 1;
    assert(rejected(invalid));
    for (const GroundPoint point : std::array<GroundPoint, 4>{{{23 * 32, 90 * 32},
        {296 * 32, 90 * 32}, {24 * 32, 89 * 32}, {24 * 32, 147 * 32}}}) {
        invalid = state;
        invalid.points[0] = point;
        assert(rejected(invalid));
    }

    std::array<IndexedBitmap, 8> bitmaps;
    for (auto& bitmap : bitmaps) {
        bitmap.width = 8;
        bitmap.height = 1;
        bitmap.pixels.assign(8, 1);
    }
    std::array<std::uint16_t, StellarRng::wordCount> table{};
    table[0] = 0xFFFF;
    table[55] = 1;
    const StellarRng rng(table, 0, 0, 0, 0);
    auto resetState = state;
    resetState.points.fill({296 * 32, 90 * 32});
    const auto execution = groundClippedExecution(resetState, rng, bitmaps, {24, 295, 24, 145});
    assert(rng.calls() == 0 && rng.indexA() == 0 && rng.indexB() == 0);
    assert(execution.rng.calls() == 12);
    assert(execution.rng.indexA() == 88 && execution.rng.indexB() == 88);
    assert((execution.ground.points()[0] == GroundPoint{-64, 2944}));
    for (std::size_t index = 0; index < resetState.points.size(); ++index) {
        assert(!execution.ground.pointVisible(index));
    }
    std::uint64_t resetDispatches = 0;
    for (const auto& operation : execution.operations) {
        assert(operation.kind == NormalWorkKind::instructions);
        resetDispatches += operation.count;
    }
    assert(resetDispatches == 16 + 6 * 92 + 2 + 5 + 6);
    assert(execution.pixels.empty());
    auto drawState = state;
    drawState.points.fill({100 * 32, 98 * 32});
    bitmaps[6].pixels = {1, 2, 0, 4, 5, 0, 7, 8};
    const auto drawn = groundClippedExecution(drawState, rng, bitmaps, {24, 295, 24, 145});
    const std::array<std::int16_t, 6> expectedX{100, 104, 101, 106, 103, 107};
    const std::array<std::uint8_t, 6> expectedColor{1, 5, 2, 7, 4, 8};
    assert(drawn.pixels.size() == expectedX.size() * drawState.points.size());
    assert(drawn.rng.calls() == rng.calls());
    for (std::size_t index = 0; index < drawn.pixels.size(); ++index) {
        assert(drawn.pixels[index].point.x == expectedX[index % expectedX.size()]);
        assert(drawn.pixels[index].point.y == 98);
        assert(drawn.pixels[index].color == expectedColor[index % expectedColor.size()]);
    }
    const auto oldDraw = groundClippedOperations(drawState, bitmaps, {24, 295, 24, 145});
    assert(drawn.operations.size() == oldDraw.size());
    for (std::size_t index = 0; index < oldDraw.size(); ++index) {
        assert(drawn.operations[index].kind == oldDraw[index].kind);
        assert(drawn.operations[index].count == oldDraw[index].count);
    }
    auto mixedState = drawState;
    mixedState.points[0] = {296 * 32, 90 * 32};
    const auto mixed = groundClippedExecution(mixedState, rng, bitmaps, {24, 295, 24, 145});
    assert(mixed.pixels.size() == expectedX.size() * (mixedState.points.size() - 1));
    assert(mixed.rng.calls() == 2);

    auto retainedGround = execution.ground;
    auto retainedRng = execution.rng;
    retainedGround.advance(0, 0, retainedRng);
    assert(retainedRng.calls() > execution.rng.calls());

    {
        constexpr Viewport viewport{24, 295, 24, 145};
        std::array<IndexedBitmap, 8> clippedBitmaps;
        for (auto& bitmap : clippedBitmaps) {
            bitmap = {4, 3, {1, 0, 3, 4, 0, 6, 7, 8, 9, 10, 0, 12}};
        }
        GroundControlState clippedState{};
        clippedState.points.fill({160 * 32, 144 * 32});
        const auto clipped = groundClippedExecution(clippedState, rng, clippedBitmaps, viewport);
        const std::array<std::array<int, 3>, 6> expected{{
            {160, 144, 1}, {161, 145, 6}, {162, 144, 3},
            {162, 145, 7}, {163, 144, 4}, {163, 145, 8}}};
        assert(clipped.pixels.size() == expected.size() * clippedState.points.size());
        assert(clipped.rng.calls() == rng.calls());
        for (std::size_t i = 0; i < clipped.pixels.size(); ++i) {
            const auto& pixel = clipped.pixels[i];
            const auto& wanted = expected[i % expected.size()];
            assert(pixel.point.x == wanted[0] && pixel.point.y == wanted[1] && pixel.color == wanted[2]);
        }
        const auto instructionCount = [](const GroundExecutionWork& work) {
            std::uint64_t count = 0;
            for (const auto operation : work.operations) {
                if (operation.kind == NormalWorkKind::instructions) { count += operation.count; }
            }
            return count;
        };
        clippedState.points.fill({160 * 32, 142 * 32});
        const auto interior = groundClippedExecution(clippedState, rng, clippedBitmaps, viewport);
        clippedState.points.fill({160 * 32, 143 * 32});
        const auto bottomTouch = groundClippedExecution(clippedState, rng, clippedBitmaps, viewport);
        assert(bottomTouch.pixels.size() == interior.pixels.size());
        assert(instructionCount(bottomTouch) == instructionCount(interior) + 4 * clippedState.points.size());
        clippedState.points.fill({292 * 32, 142 * 32});
        const auto rightTouch = groundClippedExecution(clippedState, rng, clippedBitmaps, viewport);
        assert(instructionCount(rightTouch) == instructionCount(interior) + 8 * clippedState.points.size());
        const auto rejectsClip = [&]() {
            try { groundClippedExecution(clippedState, rng, clippedBitmaps, viewport); }
            catch (const std::invalid_argument&) { return true; }
            return false;
        };
        clippedState.points.fill({160 * 32, 146 * 32});
        const auto bottomCull = groundClippedExecution(clippedState, rng, clippedBitmaps, viewport);
        assert(bottomCull.pixels.empty());
        assert(bottomCull.rng.calls() == rng.calls());
        assert(instructionCount(bottomCull) == 16 + 6 + 5
            + clippedState.points.size() * (47 + 66 + 7));
        for (std::size_t i = 0; i < clippedState.points.size(); ++i) {
            assert(bottomCull.ground.pointVisible(i));
        }
        clippedState.points.fill({294 * 32, 146 * 32});
        const auto cornerCull = groundClippedExecution(clippedState, rng, clippedBitmaps, viewport);
        assert(cornerCull.pixels.empty());
        assert(instructionCount(cornerCull) == instructionCount(bottomCull) + 8 * clippedState.points.size());
        clippedState.points.fill({160 * 32, 144 * 32});
        clippedBitmaps[0].pixels.resize(8);
        assert(rejectsClip());

        for (auto& bitmap : clippedBitmaps) {
            bitmap = {16, 3, std::vector<std::uint8_t>(48, 7)};
            bitmap.pixels[0] = bitmap.pixels[1] = 0;
            bitmap.pixels[16] = bitmap.pixels[17] = 66;
            bitmap.pixels[32] = bitmap.pixels[33] = 0;
        }
        clippedState.points.fill({294 * 32, 124 * 32});
        const auto thinTwo = groundClippedExecution(clippedState, rng, clippedBitmaps, viewport);
        assert(thinTwo.pixels.size() == 2 * clippedState.points.size());
        assert(thinTwo.rng.calls() == rng.calls());
        for (std::size_t i = 0; i < thinTwo.pixels.size(); ++i) {
            const auto& pixel = thinTwo.pixels[i];
            assert(pixel.point.x == 294 + i % 2 && pixel.point.y == 125 && pixel.color == 66);
        }
        constexpr std::uint64_t thinTwoChildInstructions = 357 - 1 - 8 - 5;
        assert(instructionCount(thinTwo) == 16 + 6 + 5
            + clippedState.points.size() * (47 + thinTwoChildInstructions + 7));
        clippedState.points.fill({295 * 32, 124 * 32});
        const auto thinOne = groundClippedExecution(clippedState, rng, clippedBitmaps, viewport);
        assert(thinOne.pixels.size() == clippedState.points.size());
        for (const auto& pixel : thinOne.pixels) {
            assert(pixel.point.x == 295 && pixel.point.y == 125 && pixel.color == 66);
        }
        constexpr std::uint64_t thinOneChildInstructions = 323 - 1 - 4 - 5;
        assert(instructionCount(thinOne) == 16 + 6 + 5
            + clippedState.points.size() * (47 + thinOneChildInstructions + 7));
        clippedState.points.fill({294 * 32, 145 * 32});
        const auto thinTransparent = groundClippedExecution(clippedState, rng, clippedBitmaps, viewport);
        assert(thinTransparent.pixels.empty());
        constexpr std::uint64_t thinTransparentInstructions = 317 - 1 - 2 - 5;
        assert(instructionCount(thinTransparent) == 16 + 6 + 5
            + clippedState.points.size() * (47 + thinTransparentInstructions + 7));
        for (auto& bitmap : clippedBitmaps) {
            bitmap = {16, 2, std::vector<std::uint8_t>(32, 7)};
            bitmap.pixels[0] = 0;
            bitmap.pixels[1] = 74;
        }
        const auto thinStore = groundClippedExecution(clippedState, rng, clippedBitmaps, viewport);
        assert(thinStore.pixels.size() == clippedState.points.size());
        for (const auto& pixel : thinStore.pixels) {
            assert(pixel.point.x == 295 && pixel.point.y == 145 && pixel.color == 74);
        }
        constexpr std::uint64_t thinStoreInstructions = 319 - 1 - 3 - 5;
        assert(instructionCount(thinStore) == 16 + 6 + 5
            + clippedState.points.size() * (47 + thinStoreInstructions + 7));
        clippedBitmaps[0].pixels.pop_back();
        assert(rejectsClip());
        for (const std::uint16_t height : {1, 5}) {
            for (auto& bitmap : clippedBitmaps) {
                bitmap = {16, height, std::vector<std::uint8_t>(16 * height, 7)};
            }
            assert(rejectsClip());
        }
        for (auto& bitmap : clippedBitmaps) {
            bitmap = {24, 6, std::vector<std::uint8_t>(144, 7)};
        }
        assert(rejectsClip());
        for (auto& bitmap : clippedBitmaps) {
            bitmap = {16, 3, std::vector<std::uint8_t>(48, 7)};
        }
        try {
            groundClippedExecution(clippedState, rng, clippedBitmaps, {24, 294, 24, 145});
            assert(false);
        } catch (const std::invalid_argument&) {
        }

        for (auto& bitmap : clippedBitmaps) {
            bitmap = {8, 2, {1, 2, 3, 4, 21, 22, 23, 24, 5, 6, 7, 8, 25, 26, 27, 28}};
        }
        clippedState.points.fill({289 * 32, 145 * 32});
        const auto partialCrop = groundClippedExecution(clippedState, rng, clippedBitmaps, viewport);
        const std::array<int, 7> partialX{289, 293, 290, 294, 291, 295, 292};
        const std::array<int, 7> partialColors{1, 21, 2, 22, 3, 23, 4};
        assert(partialCrop.pixels.size() == partialX.size() * clippedState.points.size());
        assert(partialCrop.rng.calls() == rng.calls());
        for (std::size_t i = 0; i < partialCrop.pixels.size(); ++i) {
            const auto& pixel = partialCrop.pixels[i];
            assert(pixel.point.x == partialX[i % partialX.size()] && pixel.point.y == 145
                && pixel.color == partialColors[i % partialColors.size()]);
        }
        clippedState.points.fill({295 * 32, 145 * 32});
        assert(rejectsClip());

        clippedState.points.fill({293 * 32, 145 * 32});
        const auto emptyPlane = groundClippedExecution(clippedState, rng, clippedBitmaps, viewport);
        assert(emptyPlane.pixels.size() == 3 * clippedState.points.size());
        assert(emptyPlane.rng.calls() == rng.calls());
        for (std::size_t i = 0; i < emptyPlane.pixels.size(); ++i) {
            const auto& pixel = emptyPlane.pixels[i];
            assert(pixel.point.x == 293 + i % 3 && pixel.point.y == 145 && pixel.color == 1 + i % 3);
        }
        constexpr std::uint64_t emptyChildDispatches = 270 + 3 * 5 + 3 * 5 + 3 * 2 + 27 + 4 - 2;
        assert(instructionCount(emptyPlane) == 16 + 6 + 5
            + clippedState.points.size() * (47 + emptyChildDispatches - 1 - 5 - 3 - 3 + 7));

        for (auto& bitmap : clippedBitmaps) {
            bitmap = {8, 2, {1, 2, 3, 4, 21, 22, 23, 24, 5, 6, 7, 8, 25, 26, 27, 28}};
        }
        clippedState.points.fill({292 * 32, 140 * 32});
        const auto rightCrop = groundClippedExecution(clippedState, rng, clippedBitmaps, viewport);
        const std::array<std::array<int, 3>, 8> rightExpected{{
            {292, 140, 1}, {292, 141, 5}, {293, 140, 2}, {293, 141, 6},
            {294, 140, 3}, {294, 141, 7}, {295, 140, 4}, {295, 141, 8}}};
        assert(rightCrop.pixels.size() == rightExpected.size() * clippedState.points.size());
        assert(rightCrop.rng.calls() == rng.calls());
        for (std::size_t i = 0; i < rightCrop.pixels.size(); ++i) {
            const auto& pixel = rightCrop.pixels[i];
            const auto& wanted = rightExpected[i % rightExpected.size()];
            assert(pixel.point.x == wanted[0] && pixel.point.y == wanted[1] && pixel.color == wanted[2]);
        }
        clippedBitmaps[0].pixels.resize(8);
        assert(rejectsClip());
    }

    try {
        groundClippedExecution(resetState, StellarRng(table, 1, 0, 0, 0), bitmaps, {24, 295, 24, 145});
        assert(false);
    } catch (const std::invalid_argument&) {
    }
}
