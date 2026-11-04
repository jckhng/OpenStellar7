#include "stellar7/render_work.hpp"
#include <array>
#include <cassert>
#include <stdexcept>

namespace {
using namespace stellar7;
constexpr Viewport viewport{24,295,24,145};
std::array<IndexedBitmap,8> bitmaps() {
    std::array<IndexedBitmap,8> result;
    for (std::size_t i = 0; i < result.size(); ++i) {
        result[i] = {4,1,{static_cast<std::uint8_t>(i+1),0,
            static_cast<std::uint8_t>(i+2),static_cast<std::uint8_t>(i+3)}};
    }
    return result;
}
std::uint64_t instructions(const std::vector<NormalWorkOperation>& operations) {
    std::uint64_t result = 0;
    for (const auto operation : operations) {
        if (operation.kind == NormalWorkKind::instructions) { result += operation.count; }
    }
    return result;
}
std::uint64_t controlInstructions(const GroundExecutionWork& result) {
    auto count = instructions(result.operations);
    assert(result.pixels.size() % 3 == 0);
    for (std::size_t i = 0; i < result.pixels.size(); i += 3) {
        const auto first = result.pixels[i].point;
        GroundControlState repeated{};
        repeated.points.fill({static_cast<std::int16_t>(first.x*32),static_cast<std::int16_t>(first.y*32)});
        const auto whole = groundClippedOperations(repeated,bitmaps(),viewport);
        const auto bodySize = (whole.size()-2)/6-2;
        const std::vector<NormalWorkOperation> body(whole.begin()+2,whole.begin()+2+bodySize);
        count -= instructions(body);
    }
    return count;
}
bool rejects(const GroundControlState& input) {
    try { groundClippedExecution(input,StellarRng{},bitmaps(),viewport); }
    catch (const std::invalid_argument&) { return true; }
    return false;
}
}
int main() {
    using namespace stellar7;
    // Nine controlled original cases, two retained calls each. Bitmap child is reused, not original-executed here.
    const std::array<std::uint16_t,StellarRng::wordCount> seed{13,1010,2007,3004,4001,4998,5995,6992,7989,8986,9983,10980,11977,12974,13971,14968,15965,16962,17959,18956,19953,20950,21947,22944,23941,24938,25935,26932,27929,28926,29923,30920,31917,32914,33911,34908,35905,36902,37899,38896,39893,40890,41887,42884,43881,44878,45875,46872,47869,48866,49863,50860,51857,52854,53851,54848};
    {
        GroundControlState input{};
        input.speed = 0;
        input.points = {{{3200,3136},{5000,3200},{6000,3400},{4000,3600},{8000,4000},{2000,4400}}};
        StellarRng rng(seed,0,0,0,0);
        const std::array<GroundPoint,6> moved0{{{3200,3136},{5000,3200},{6000,3400},{4000,3600},{8000,4000},{2000,4400}}};
        for (std::size_t point = 0; point < moved0.size(); ++point) {
            assert(advanceGroundPoint(input.points[point],input.turn,input.speed) == moved0[point]);
        }
        const auto step0 = groundClippedExecution(input,rng,bitmaps(),viewport);
        assert(controlInstructions(step0) == 351);
        assert((step0.ground.points() == std::array<GroundPoint,6>{{{3200,3136},{5000,3200},{6000,3400},{4000,3600},{8000,4000},{2000,4400}}}));
        assert(step0.rng.indexA() == 0 && step0.rng.indexB() == 0);
        assert(step0.rng.last() == 0 && step0.rng.calls() == 0);
        const std::array<std::uint16_t,StellarRng::wordCount> future0{26,44160,42166,40172,38178,36184,34190,32196,30202,28208,26214,24220,22226,20232,18238,16244,14250,12256,10262,8268,6274,4280,2286,292,63834,61840,59846,57852,55858,53864,51870,49876,47882,45888,43894,41900,39906,37912,35918,33924,31930,29936,27942,25948,23954,21960,19966,17972,15978,13984,11990,9996,8002,6008,4014,2020};
        auto futureRng0 = step0.rng;
        for (const auto value : future0) { assert(futureRng0.next() == value); }
        const std::array<std::array<int,3>,18> pixels0{{{100,98,7},{102,98,8},{103,98,9},{156,100,6},{158,100,7},{159,100,8},{187,106,6},{189,106,7},{190,106,8},{125,112,5},{127,112,6},{128,112,7},{250,125,3},{252,125,4},{253,125,5},{62,137,2},{64,137,3},{65,137,4}}};
        assert(step0.pixels.size() == pixels0.size());
        for (std::size_t pixel = 0; pixel < pixels0.size(); ++pixel) {
            assert(step0.pixels[pixel].point.x == pixels0[pixel][0]);
            assert(step0.pixels[pixel].point.y == pixels0[pixel][1]);
            assert(step0.pixels[pixel].color == pixels0[pixel][2]);
        }
        input.points = step0.ground.points();
        rng = step0.rng;
        const std::array<GroundPoint,6> moved1{{{3200,3136},{5000,3200},{6000,3400},{4000,3600},{8000,4000},{2000,4400}}};
        for (std::size_t point = 0; point < moved1.size(); ++point) {
            assert(advanceGroundPoint(input.points[point],input.turn,input.speed) == moved1[point]);
        }
        const auto step1 = groundClippedExecution(input,rng,bitmaps(),viewport);
        assert(controlInstructions(step1) == 351);
        assert((step1.ground.points() == std::array<GroundPoint,6>{{{3200,3136},{5000,3200},{6000,3400},{4000,3600},{8000,4000},{2000,4400}}}));
        assert(step1.rng.indexA() == 0 && step1.rng.indexB() == 0);
        assert(step1.rng.last() == 0 && step1.rng.calls() == 0);
        const std::array<std::uint16_t,StellarRng::wordCount> future1{26,44160,42166,40172,38178,36184,34190,32196,30202,28208,26214,24220,22226,20232,18238,16244,14250,12256,10262,8268,6274,4280,2286,292,63834,61840,59846,57852,55858,53864,51870,49876,47882,45888,43894,41900,39906,37912,35918,33924,31930,29936,27942,25948,23954,21960,19966,17972,15978,13984,11990,9996,8002,6008,4014,2020};
        auto futureRng1 = step1.rng;
        for (const auto value : future1) { assert(futureRng1.next() == value); }
        const std::array<std::array<int,3>,18> pixels1{{{100,98,7},{102,98,8},{103,98,9},{156,100,6},{158,100,7},{159,100,8},{187,106,6},{189,106,7},{190,106,8},{125,112,5},{127,112,6},{128,112,7},{250,125,3},{252,125,4},{253,125,5},{62,137,2},{64,137,3},{65,137,4}}};
        assert(step1.pixels.size() == pixels1.size());
        for (std::size_t pixel = 0; pixel < pixels1.size(); ++pixel) {
            assert(step1.pixels[pixel].point.x == pixels1[pixel][0]);
            assert(step1.pixels[pixel].point.y == pixels1[pixel][1]);
            assert(step1.pixels[pixel].color == pixels1[pixel][2]);
        }
        input.points = step1.ground.points();
        rng = step1.rng;
    }
    {
        GroundControlState input{};
        input.speed = 10;
        input.points = {{{3200,3136},{5000,3200},{6000,3400},{4000,3600},{8000,4000},{2000,4400}}};
        StellarRng rng(seed,0,0,0,0);
        const std::array<GroundPoint,6> moved0{{{3190,3138},{4999,3202},{6004,3403},{3994,3604},{8014,4006},{1984,4408}}};
        for (std::size_t point = 0; point < moved0.size(); ++point) {
            assert(advanceGroundPoint(input.points[point],input.turn,input.speed) == moved0[point]);
        }
        const auto step0 = groundClippedExecution(input,rng,bitmaps(),viewport);
        assert(controlInstructions(step0) == 821);
        assert((step0.ground.points() == std::array<GroundPoint,6>{{{3190,3138},{4999,3202},{6004,3403},{3994,3604},{8014,4006},{1984,4408}}}));
        assert(step0.rng.indexA() == 0 && step0.rng.indexB() == 0);
        assert(step0.rng.last() == 0 && step0.rng.calls() == 0);
        const std::array<std::uint16_t,StellarRng::wordCount> future0{26,44160,42166,40172,38178,36184,34190,32196,30202,28208,26214,24220,22226,20232,18238,16244,14250,12256,10262,8268,6274,4280,2286,292,63834,61840,59846,57852,55858,53864,51870,49876,47882,45888,43894,41900,39906,37912,35918,33924,31930,29936,27942,25948,23954,21960,19966,17972,15978,13984,11990,9996,8002,6008,4014,2020};
        auto futureRng0 = step0.rng;
        for (const auto value : future0) { assert(futureRng0.next() == value); }
        const std::array<std::array<int,3>,18> pixels0{{{99,98,7},{101,98,8},{102,98,9},{156,100,6},{158,100,7},{159,100,8},{187,106,6},{189,106,7},{190,106,8},{124,112,5},{126,112,6},{127,112,7},{250,125,3},{252,125,4},{253,125,5},{62,137,2},{64,137,3},{65,137,4}}};
        assert(step0.pixels.size() == pixels0.size());
        for (std::size_t pixel = 0; pixel < pixels0.size(); ++pixel) {
            assert(step0.pixels[pixel].point.x == pixels0[pixel][0]);
            assert(step0.pixels[pixel].point.y == pixels0[pixel][1]);
            assert(step0.pixels[pixel].color == pixels0[pixel][2]);
        }
        input.points = step0.ground.points();
        rng = step0.rng;
        const std::array<GroundPoint,6> moved1{{{3180,3140},{4998,3204},{6008,3406},{3988,3608},{8028,4012},{1968,4416}}};
        for (std::size_t point = 0; point < moved1.size(); ++point) {
            assert(advanceGroundPoint(input.points[point],input.turn,input.speed) == moved1[point]);
        }
        const auto step1 = groundClippedExecution(input,rng,bitmaps(),viewport);
        assert(controlInstructions(step1) == 821);
        assert((step1.ground.points() == std::array<GroundPoint,6>{{{3180,3140},{4998,3204},{6008,3406},{3988,3608},{8028,4012},{1968,4416}}}));
        assert(step1.rng.indexA() == 0 && step1.rng.indexB() == 0);
        assert(step1.rng.last() == 0 && step1.rng.calls() == 0);
        const std::array<std::uint16_t,StellarRng::wordCount> future1{26,44160,42166,40172,38178,36184,34190,32196,30202,28208,26214,24220,22226,20232,18238,16244,14250,12256,10262,8268,6274,4280,2286,292,63834,61840,59846,57852,55858,53864,51870,49876,47882,45888,43894,41900,39906,37912,35918,33924,31930,29936,27942,25948,23954,21960,19966,17972,15978,13984,11990,9996,8002,6008,4014,2020};
        auto futureRng1 = step1.rng;
        for (const auto value : future1) { assert(futureRng1.next() == value); }
        const std::array<std::array<int,3>,18> pixels1{{{99,98,7},{101,98,8},{102,98,9},{156,100,6},{158,100,7},{159,100,8},{187,106,6},{189,106,7},{190,106,8},{124,112,5},{126,112,6},{127,112,7},{250,125,3},{252,125,4},{253,125,5},{61,138,2},{63,138,3},{64,138,4}}};
        assert(step1.pixels.size() == pixels1.size());
        for (std::size_t pixel = 0; pixel < pixels1.size(); ++pixel) {
            assert(step1.pixels[pixel].point.x == pixels1[pixel][0]);
            assert(step1.pixels[pixel].point.y == pixels1[pixel][1]);
            assert(step1.pixels[pixel].color == pixels1[pixel][2]);
        }
        input.points = step1.ground.points();
        rng = step1.rng;
    }
    {
        GroundControlState input{};
        input.speed = 200;
        input.points = {{{3200,3136},{5000,3200},{6000,3400},{4000,3600},{8000,4000},{2000,4400}}};
        StellarRng rng(seed,0,0,0,0);
        const std::array<GroundPoint,6> moved0{{{3015,3176},{4991,3246},{6089,3466},{3893,3685},{8284,4125},{1698,4564}}};
        for (std::size_t point = 0; point < moved0.size(); ++point) {
            assert(advanceGroundPoint(input.points[point],input.turn,input.speed) == moved0[point]);
        }
        const auto step0 = groundClippedExecution(input,rng,bitmaps(),viewport);
        assert(controlInstructions(step0) == 821);
        assert((step0.ground.points() == std::array<GroundPoint,6>{{{3015,3176},{4991,3246},{6089,3466},{3893,3685},{8284,4125},{1698,4564}}}));
        assert(step0.rng.indexA() == 0 && step0.rng.indexB() == 0);
        assert(step0.rng.last() == 0 && step0.rng.calls() == 0);
        const std::array<std::uint16_t,StellarRng::wordCount> future0{26,44160,42166,40172,38178,36184,34190,32196,30202,28208,26214,24220,22226,20232,18238,16244,14250,12256,10262,8268,6274,4280,2286,292,63834,61840,59846,57852,55858,53864,51870,49876,47882,45888,43894,41900,39906,37912,35918,33924,31930,29936,27942,25948,23954,21960,19966,17972,15978,13984,11990,9996,8002,6008,4014,2020};
        auto futureRng0 = step0.rng;
        for (const auto value : future0) { assert(futureRng0.next() == value); }
        const std::array<std::array<int,3>,18> pixels0{{{94,99,6},{96,99,7},{97,99,8},{155,101,6},{157,101,7},{158,101,8},{190,108,5},{192,108,6},{193,108,7},{121,115,4},{123,115,5},{124,115,6},{258,128,3},{260,128,4},{261,128,5},{53,142,1},{55,142,2},{56,142,3}}};
        assert(step0.pixels.size() == pixels0.size());
        for (std::size_t pixel = 0; pixel < pixels0.size(); ++pixel) {
            assert(step0.pixels[pixel].point.x == pixels0[pixel][0]);
            assert(step0.pixels[pixel].point.y == pixels0[pixel][1]);
            assert(step0.pixels[pixel].color == pixels0[pixel][2]);
        }
        input.points = step0.ground.points();
        rng = step0.rng;
        const std::array<GroundPoint,6> moved1{{{2812,3220},{4981,3297},{6186,3538},{3776,3779},{8596,4262},{1366,4744}}};
        for (std::size_t point = 0; point < moved1.size(); ++point) {
            assert(advanceGroundPoint(input.points[point],input.turn,input.speed) == moved1[point]);
        }
        const auto step1 = groundClippedExecution(input,rng,bitmaps(),viewport);
        assert(controlInstructions(step1) == 865);
        assert((step1.ground.points() == std::array<GroundPoint,6>{{{2812,3220},{4981,3297},{6186,3538},{3776,3779},{8596,4262},{832,1600}}}));
        assert(step1.rng.indexA() == 108 && step1.rng.indexB() == 108);
        assert(step1.rng.last() == 44160 && step1.rng.calls() == 2);
        const std::array<std::uint16_t,StellarRng::wordCount> future1{42166,40172,38178,36184,34190,32196,30202,28208,26214,24220,22226,20232,18238,16244,14250,12256,10262,8268,6274,4280,2286,292,63834,61840,59846,57852,55858,53864,51870,49876,47882,45888,43894,41900,39906,37912,35918,33924,31930,29936,27942,25948,23954,21960,19966,17972,15978,13984,11990,9996,8002,6008,4014,2020,52,22784};
        auto futureRng1 = step1.rng;
        for (const auto value : future1) { assert(futureRng1.next() == value); }
        const std::array<std::array<int,3>,15> pixels1{{{87,100,6},{89,100,7},{90,100,8},{155,103,6},{157,103,7},{158,103,8},{193,110,5},{195,110,6},{196,110,7},{118,118,4},{120,118,5},{121,118,6},{268,133,2},{270,133,3},{271,133,4}}};
        assert(step1.pixels.size() == pixels1.size());
        for (std::size_t pixel = 0; pixel < pixels1.size(); ++pixel) {
            assert(step1.pixels[pixel].point.x == pixels1[pixel][0]);
            assert(step1.pixels[pixel].point.y == pixels1[pixel][1]);
            assert(step1.pixels[pixel].color == pixels1[pixel][2]);
        }
        input.points = step1.ground.points();
        rng = step1.rng;
    }
    {
        GroundControlState input{};
        input.speed = -10;
        input.points = {{{3200,3136},{5000,3200},{6000,3400},{4000,3600},{8000,4000},{2000,4400}}};
        StellarRng rng(seed,0,0,0,0);
        const std::array<GroundPoint,6> moved0{{{3209,3133},{5000,3197},{5995,3396},{4005,3595},{7985,3993},{2015,4391}}};
        for (std::size_t point = 0; point < moved0.size(); ++point) {
            assert(advanceGroundPoint(input.points[point],input.turn,input.speed) == moved0[point]);
        }
        const auto step0 = groundClippedExecution(input,rng,bitmaps(),viewport);
        assert(controlInstructions(step0) == 885);
        assert((step0.ground.points() == std::array<GroundPoint,6>{{{3209,3133},{5000,3197},{5995,3396},{4005,3595},{7985,3993},{2015,4391}}}));
        assert(step0.rng.indexA() == 0 && step0.rng.indexB() == 0);
        assert(step0.rng.last() == 0 && step0.rng.calls() == 0);
        const std::array<std::uint16_t,StellarRng::wordCount> future0{26,44160,42166,40172,38178,36184,34190,32196,30202,28208,26214,24220,22226,20232,18238,16244,14250,12256,10262,8268,6274,4280,2286,292,63834,61840,59846,57852,55858,53864,51870,49876,47882,45888,43894,41900,39906,37912,35918,33924,31930,29936,27942,25948,23954,21960,19966,17972,15978,13984,11990,9996,8002,6008,4014,2020};
        auto futureRng0 = step0.rng;
        for (const auto value : future0) { assert(futureRng0.next() == value); }
        const std::array<std::array<int,3>,18> pixels0{{{100,97,7},{102,97,8},{103,97,9},{156,99,6},{158,99,7},{159,99,8},{187,106,6},{189,106,7},{190,106,8},{125,112,5},{127,112,6},{128,112,7},{249,124,3},{251,124,4},{252,124,5},{62,137,2},{64,137,3},{65,137,4}}};
        assert(step0.pixels.size() == pixels0.size());
        for (std::size_t pixel = 0; pixel < pixels0.size(); ++pixel) {
            assert(step0.pixels[pixel].point.x == pixels0[pixel][0]);
            assert(step0.pixels[pixel].point.y == pixels0[pixel][1]);
            assert(step0.pixels[pixel].color == pixels0[pixel][2]);
        }
        input.points = step0.ground.points();
        rng = step0.rng;
        const std::array<GroundPoint,6> moved1{{{3218,3130},{5000,3194},{5990,3392},{4010,3590},{7970,3986},{2030,4382}}};
        for (std::size_t point = 0; point < moved1.size(); ++point) {
            assert(advanceGroundPoint(input.points[point],input.turn,input.speed) == moved1[point]);
        }
        const auto step1 = groundClippedExecution(input,rng,bitmaps(),viewport);
        assert(controlInstructions(step1) == 885);
        assert((step1.ground.points() == std::array<GroundPoint,6>{{{3218,3130},{5000,3194},{5990,3392},{4010,3590},{7970,3986},{2030,4382}}}));
        assert(step1.rng.indexA() == 0 && step1.rng.indexB() == 0);
        assert(step1.rng.last() == 0 && step1.rng.calls() == 0);
        const std::array<std::uint16_t,StellarRng::wordCount> future1{26,44160,42166,40172,38178,36184,34190,32196,30202,28208,26214,24220,22226,20232,18238,16244,14250,12256,10262,8268,6274,4280,2286,292,63834,61840,59846,57852,55858,53864,51870,49876,47882,45888,43894,41900,39906,37912,35918,33924,31930,29936,27942,25948,23954,21960,19966,17972,15978,13984,11990,9996,8002,6008,4014,2020};
        auto futureRng1 = step1.rng;
        for (const auto value : future1) { assert(futureRng1.next() == value); }
        const std::array<std::array<int,3>,18> pixels1{{{100,97,7},{102,97,8},{103,97,9},{156,99,6},{158,99,7},{159,99,8},{187,106,6},{189,106,7},{190,106,8},{125,112,5},{127,112,6},{128,112,7},{249,124,3},{251,124,4},{252,124,5},{63,136,2},{65,136,3},{66,136,4}}};
        assert(step1.pixels.size() == pixels1.size());
        for (std::size_t pixel = 0; pixel < pixels1.size(); ++pixel) {
            assert(step1.pixels[pixel].point.x == pixels1[pixel][0]);
            assert(step1.pixels[pixel].point.y == pixels1[pixel][1]);
            assert(step1.pixels[pixel].color == pixels1[pixel][2]);
        }
        input.points = step1.ground.points();
        rng = step1.rng;
    }
    {
        GroundControlState input{};
        input.speed = -200;
        input.points = {{{3200,3136},{5000,3200},{6000,3400},{4000,3600},{8000,4000},{2000,4400}}};
        StellarRng rng(seed,0,0,0,0);
        const std::array<GroundPoint,6> moved0{{{3384,3095},{5008,3153},{5910,3333},{4106,3514},{7715,3875},{2301,4235}}};
        for (std::size_t point = 0; point < moved0.size(); ++point) {
            assert(advanceGroundPoint(input.points[point],input.turn,input.speed) == moved0[point]);
        }
        const auto step0 = groundClippedExecution(input,rng,bitmaps(),viewport);
        assert(controlInstructions(step0) == 885);
        assert((step0.ground.points() == std::array<GroundPoint,6>{{{3384,3095},{5008,3153},{5910,3333},{4106,3514},{7715,3875},{2301,4235}}}));
        assert(step0.rng.indexA() == 0 && step0.rng.indexB() == 0);
        assert(step0.rng.last() == 0 && step0.rng.calls() == 0);
        const std::array<std::uint16_t,StellarRng::wordCount> future0{26,44160,42166,40172,38178,36184,34190,32196,30202,28208,26214,24220,22226,20232,18238,16244,14250,12256,10262,8268,6274,4280,2286,292,63834,61840,59846,57852,55858,53864,51870,49876,47882,45888,43894,41900,39906,37912,35918,33924,31930,29936,27942,25948,23954,21960,19966,17972,15978,13984,11990,9996,8002,6008,4014,2020};
        auto futureRng0 = step0.rng;
        for (const auto value : future0) { assert(futureRng0.next() == value); }
        const std::array<std::array<int,3>,18> pixels0{{{105,96,7},{107,96,8},{108,96,9},{156,98,7},{158,98,8},{159,98,9},{184,104,6},{186,104,7},{187,104,8},{128,109,5},{130,109,6},{131,109,7},{241,121,4},{243,121,5},{244,121,6},{71,132,2},{73,132,3},{74,132,4}}};
        assert(step0.pixels.size() == pixels0.size());
        for (std::size_t pixel = 0; pixel < pixels0.size(); ++pixel) {
            assert(step0.pixels[pixel].point.x == pixels0[pixel][0]);
            assert(step0.pixels[pixel].point.y == pixels0[pixel][1]);
            assert(step0.pixels[pixel].color == pixels0[pixel][2]);
        }
        input.points = step0.ground.points();
        rng = step0.rng;
        const std::array<GroundPoint,6> moved1{{{3550,3058},{5015,3110},{5829,3273},{4201,3436},{7458,3762},{2573,4087}}};
        for (std::size_t point = 0; point < moved1.size(); ++point) {
            assert(advanceGroundPoint(input.points[point],input.turn,input.speed) == moved1[point]);
        }
        const auto step1 = groundClippedExecution(input,rng,bitmaps(),viewport);
        assert(controlInstructions(step1) == 885);
        assert((step1.ground.points() == std::array<GroundPoint,6>{{{3550,3058},{5015,3110},{5829,3273},{4201,3436},{7458,3762},{2573,4087}}}));
        assert(step1.rng.indexA() == 0 && step1.rng.indexB() == 0);
        assert(step1.rng.last() == 0 && step1.rng.calls() == 0);
        const std::array<std::uint16_t,StellarRng::wordCount> future1{26,44160,42166,40172,38178,36184,34190,32196,30202,28208,26214,24220,22226,20232,18238,16244,14250,12256,10262,8268,6274,4280,2286,292,63834,61840,59846,57852,55858,53864,51870,49876,47882,45888,43894,41900,39906,37912,35918,33924,31930,29936,27942,25948,23954,21960,19966,17972,15978,13984,11990,9996,8002,6008,4014,2020};
        auto futureRng1 = step1.rng;
        for (const auto value : future1) { assert(futureRng1.next() == value); }
        const std::array<std::array<int,3>,18> pixels1{{{110,95,7},{112,95,8},{113,95,9},{156,97,7},{158,97,8},{159,97,9},{182,102,6},{184,102,7},{185,102,8},{131,107,5},{133,107,6},{134,107,7},{233,117,4},{235,117,5},{236,117,6},{80,127,3},{82,127,4},{83,127,5}}};
        assert(step1.pixels.size() == pixels1.size());
        for (std::size_t pixel = 0; pixel < pixels1.size(); ++pixel) {
            assert(step1.pixels[pixel].point.x == pixels1[pixel][0]);
            assert(step1.pixels[pixel].point.y == pixels1[pixel][1]);
            assert(step1.pixels[pixel].color == pixels1[pixel][2]);
        }
        input.points = step1.ground.points();
        rng = step1.rng;
    }
    {
        GroundControlState input{};
        input.speed = 10;
        input.points = {{{768,2880},{9471,4703},{3200,3136},{0,3200},{4000,2879},{5000,4600}}};
        StellarRng rng(seed,0,0,0,0);
        const std::array<GroundPoint,6> moved0{{{746,2880},{9492,4712},{3190,3138},{-25,3202},{3994,2879},{4999,4609}}};
        for (std::size_t point = 0; point < moved0.size(); ++point) {
            assert(advanceGroundPoint(input.points[point],input.turn,input.speed) == moved0[point]);
        }
        const auto step0 = groundClippedExecution(input,rng,bitmaps(),viewport);
        assert(controlInstructions(step0) == 993);
        assert((step0.ground.points() == std::array<GroundPoint,6>{{{832,1600},{-2080,1216},{3190,3138},{-6976,2624},{-2432,4544},{4999,4609}}}));
        assert(step0.rng.indexA() == 96 && step0.rng.indexB() == 96);
        assert(step0.rng.last() == 32196 && step0.rng.calls() == 8);
        const std::array<std::uint16_t,StellarRng::wordCount> future0{30202,28208,26214,24220,22226,20232,18238,16244,14250,12256,10262,8268,6274,4280,2286,292,63834,61840,59846,57852,55858,53864,51870,49876,47882,45888,43894,41900,39906,37912,35918,33924,31930,29936,27942,25948,23954,21960,19966,17972,15978,13984,11990,9996,8002,6008,4014,2020,52,22784,18796,14808,10820,6832,2844,64392};
        auto futureRng0 = step0.rng;
        for (const auto value : future0) { assert(futureRng0.next() == value); }
        const std::array<std::array<int,3>,6> pixels0{{{99,98,7},{101,98,8},{102,98,9},{156,144,1},{158,144,2},{159,144,3}}};
        assert(step0.pixels.size() == pixels0.size());
        for (std::size_t pixel = 0; pixel < pixels0.size(); ++pixel) {
            assert(step0.pixels[pixel].point.x == pixels0[pixel][0]);
            assert(step0.pixels[pixel].point.y == pixels0[pixel][1]);
            assert(step0.pixels[pixel].color == pixels0[pixel][2]);
        }
        input.points = step0.ground.points();
        rng = step0.rng;
        const std::array<GroundPoint,6> moved1{{{811,1594},{-2115,1208},{3180,3140},{-7035,2623},{-2469,4552},{4998,4618}}};
        for (std::size_t point = 0; point < moved1.size(); ++point) {
            assert(advanceGroundPoint(input.points[point],input.turn,input.speed) == moved1[point]);
        }
        const auto step1 = groundClippedExecution(input,rng,bitmaps(),viewport);
        assert(controlInstructions(step1) == 1025);
        assert((step1.ground.points() == std::array<GroundPoint,6>{{{3584,4160},{8128,3776},{3180,3140},{3232,3392},{7776,3008},{4998,4618}}}));
        assert(step1.rng.indexA() == 80 && step1.rng.indexB() == 80);
        assert(step1.rng.last() == 16244 && step1.rng.calls() == 16);
        const std::array<std::uint16_t,StellarRng::wordCount> future1{14250,12256,10262,8268,6274,4280,2286,292,63834,61840,59846,57852,55858,53864,51870,49876,47882,45888,43894,41900,39906,37912,35918,33924,31930,29936,27942,25948,23954,21960,19966,17972,15978,13984,11990,9996,8002,6008,4014,2020,52,22784,18796,14808,10820,6832,2844,64392,60404,56416,52428,48440,44452,40464,36476,32488};
        auto futureRng1 = step1.rng;
        for (const auto value : future1) { assert(futureRng1.next() == value); }
        const std::array<std::array<int,3>,6> pixels1{{{99,98,7},{101,98,8},{102,98,9},{156,144,1},{158,144,2},{159,144,3}}};
        assert(step1.pixels.size() == pixels1.size());
        for (std::size_t pixel = 0; pixel < pixels1.size(); ++pixel) {
            assert(step1.pixels[pixel].point.x == pixels1[pixel][0]);
            assert(step1.pixels[pixel].point.y == pixels1[pixel][1]);
            assert(step1.pixels[pixel].color == pixels1[pixel][2]);
        }
        input.points = step1.ground.points();
        rng = step1.rng;
    }
    {
        GroundControlState input{};
        input.speed = -200;
        input.points = {{{768,2880},{9471,4703},{3200,3136},{0,3200},{4000,2879},{5000,4600}}};
        StellarRng rng(seed,0,0,0,0);
        const std::array<GroundPoint,6> moved0{{{1189,2864},{9042,4509},{3384,3095},{496,3153},{4106,2863},{5008,4416}}};
        for (std::size_t point = 0; point < moved0.size(); ++point) {
            assert(advanceGroundPoint(input.points[point],input.turn,input.speed) == moved0[point]);
        }
        const auto step0 = groundClippedExecution(input,rng,bitmaps(),viewport);
        assert(controlInstructions(step0) == 1015);
        assert((step0.ground.points() == std::array<GroundPoint,6>{{{832,1600},{9042,4509},{3384,3095},{-2080,1216},{-6976,2624},{5008,4416}}}));
        assert(step0.rng.indexA() == 100 && step0.rng.indexB() == 100);
        assert(step0.rng.last() == 36184 && step0.rng.calls() == 6);
        const std::array<std::uint16_t,StellarRng::wordCount> future0{34190,32196,30202,28208,26214,24220,22226,20232,18238,16244,14250,12256,10262,8268,6274,4280,2286,292,63834,61840,59846,57852,55858,53864,51870,49876,47882,45888,43894,41900,39906,37912,35918,33924,31930,29936,27942,25948,23954,21960,19966,17972,15978,13984,11990,9996,8002,6008,4014,2020,52,22784,18796,14808,10820,6832};
        auto futureRng0 = step0.rng;
        for (const auto value : future0) { assert(futureRng0.next() == value); }
        const std::array<std::array<int,3>,9> pixels0{{{282,140,1},{284,140,2},{285,140,3},{105,96,7},{107,96,8},{108,96,9},{156,138,2},{158,138,3},{159,138,4}}};
        assert(step0.pixels.size() == pixels0.size());
        for (std::size_t pixel = 0; pixel < pixels0.size(); ++pixel) {
            assert(step0.pixels[pixel].point.x == pixels0[pixel][0]);
            assert(step0.pixels[pixel].point.y == pixels0[pixel][1]);
            assert(step0.pixels[pixel].color == pixels0[pixel][2]);
        }
        input.points = step0.ground.points();
        rng = step0.rng;
        const std::array<GroundPoint,6> moved1{{{1247,1709},{8655,4334},{3550,3058},{-1380,1362},{-5798,2633},{5015,4250}}};
        for (std::size_t point = 0; point < moved1.size(); ++point) {
            assert(advanceGroundPoint(input.points[point],input.turn,input.speed) == moved1[point]);
        }
        const auto step1 = groundClippedExecution(input,rng,bitmaps(),viewport);
        assert(controlInstructions(step1) == 1009);
        assert((step1.ground.points() == std::array<GroundPoint,6>{{{-2432,4544},{8655,4334},{3550,3058},{3584,4160},{8128,3776},{5015,4250}}}));
        assert(step1.rng.indexA() == 88 && step1.rng.indexB() == 88);
        assert(step1.rng.last() == 24220 && step1.rng.calls() == 12);
        const std::array<std::uint16_t,StellarRng::wordCount> future1{22226,20232,18238,16244,14250,12256,10262,8268,6274,4280,2286,292,63834,61840,59846,57852,55858,53864,51870,49876,47882,45888,43894,41900,39906,37912,35918,33924,31930,29936,27942,25948,23954,21960,19966,17972,15978,13984,11990,9996,8002,6008,4014,2020,52,22784,18796,14808,10820,6832,2844,64392,60404,56416,52428,48440};
        auto futureRng1 = step1.rng;
        for (const auto value : future1) { assert(futureRng1.next() == value); }
        const std::array<std::array<int,3>,9> pixels1{{{270,135,2},{272,135,3},{273,135,4},{110,95,7},{112,95,8},{113,95,9},{156,132,2},{158,132,3},{159,132,4}}};
        assert(step1.pixels.size() == pixels1.size());
        for (std::size_t pixel = 0; pixel < pixels1.size(); ++pixel) {
            assert(step1.pixels[pixel].point.x == pixels1[pixel][0]);
            assert(step1.pixels[pixel].point.y == pixels1[pixel][1]);
            assert(step1.pixels[pixel].color == pixels1[pixel][2]);
        }
        input.points = step1.ground.points();
        rng = step1.rng;
    }
    {
        GroundControlState input{};
        input.speed = 200;
        input.points = {{{-32768,3136},{3200,-32768},{32767,32767},{-30000,4000},{6000,-31000},{3200,3136}}};
        StellarRng rng(seed,0,0,0,0);
        const std::array<GroundPoint,6> moved0{{{-30065,3176},{3015,-29834},{-30066,-29835},{-27027,4125},{6089,-27893},{3015,3176}}};
        for (std::size_t point = 0; point < moved0.size(); ++point) {
            assert(advanceGroundPoint(input.points[point],input.turn,input.speed) == moved0[point]);
        }
        const auto step0 = groundClippedExecution(input,rng,bitmaps(),viewport);
        assert(controlInstructions(step0) == 1015);
        assert((step0.ground.points() == std::array<GroundPoint,6>{{{832,1600},{-2080,1216},{-6976,2624},{-2432,4544},{3584,4160},{3015,3176}}}));
        assert(step0.rng.indexA() == 92 && step0.rng.indexB() == 92);
        assert(step0.rng.last() == 28208 && step0.rng.calls() == 10);
        const std::array<std::uint16_t,StellarRng::wordCount> future0{26214,24220,22226,20232,18238,16244,14250,12256,10262,8268,6274,4280,2286,292,63834,61840,59846,57852,55858,53864,51870,49876,47882,45888,43894,41900,39906,37912,35918,33924,31930,29936,27942,25948,23954,21960,19966,17972,15978,13984,11990,9996,8002,6008,4014,2020,52,22784,18796,14808,10820,6832,2844,64392,60404,56416};
        auto futureRng0 = step0.rng;
        for (const auto value : future0) { assert(futureRng0.next() == value); }
        const std::array<std::array<int,3>,3> pixels0{{{94,99,6},{96,99,7},{97,99,8}}};
        assert(step0.pixels.size() == pixels0.size());
        for (std::size_t pixel = 0; pixel < pixels0.size(); ++pixel) {
            assert(step0.pixels[pixel].point.x == pixels0[pixel][0]);
            assert(step0.pixels[pixel].point.y == pixels0[pixel][1]);
            assert(step0.pixels[pixel].color == pixels0[pixel][2]);
        }
        input.points = step0.ground.points();
        rng = step0.rng;
        const std::array<GroundPoint,6> moved1{{{416,1490},{-2780,1069},{-8155,2614},{-3167,4722},{3437,4300},{2812,3220}}};
        for (std::size_t point = 0; point < moved1.size(); ++point) {
            assert(advanceGroundPoint(input.points[point],input.turn,input.speed) == moved1[point]);
        }
        const auto step1 = groundClippedExecution(input,rng,bitmaps(),viewport);
        assert(controlInstructions(step1) == 1021);
        assert((step1.ground.points() == std::array<GroundPoint,6>{{{8128,3776},{3232,3392},{7776,3008},{2880,4416},{3437,4300},{2812,3220}}}));
        assert(step1.rng.indexA() == 76 && step1.rng.indexB() == 76);
        assert(step1.rng.last() == 12256 && step1.rng.calls() == 18);
        const std::array<std::uint16_t,StellarRng::wordCount> future1{10262,8268,6274,4280,2286,292,63834,61840,59846,57852,55858,53864,51870,49876,47882,45888,43894,41900,39906,37912,35918,33924,31930,29936,27942,25948,23954,21960,19966,17972,15978,13984,11990,9996,8002,6008,4014,2020,52,22784,18796,14808,10820,6832,2844,64392,60404,56416,52428,48440,44452,40464,36476,32488,28500,24512};
        auto futureRng1 = step1.rng;
        for (const auto value : future1) { assert(futureRng1.next() == value); }
        const std::array<std::array<int,3>,6> pixels1{{{107,134,2},{109,134,3},{110,134,4},{87,100,6},{89,100,7},{90,100,8}}};
        assert(step1.pixels.size() == pixels1.size());
        for (std::size_t pixel = 0; pixel < pixels1.size(); ++pixel) {
            assert(step1.pixels[pixel].point.x == pixels1[pixel][0]);
            assert(step1.pixels[pixel].point.y == pixels1[pixel][1]);
            assert(step1.pixels[pixel].color == pixels1[pixel][2]);
        }
        input.points = step1.ground.points();
        rng = step1.rng;
    }
    {
        GroundControlState input{};
        input.speed = -200;
        input.points = {{{-32768,3136},{3200,-32768},{32767,32767},{-30000,4000},{6000,-31000},{3200,3136}}};
        StellarRng rng(seed,0,0,0,0);
        const std::array<GroundPoint,6> moved0{{{30064,3095},{3384,29833},{30063,29832},{32562,3875},{5910,31428},{3384,3095}}};
        for (std::size_t point = 0; point < moved0.size(); ++point) {
            assert(advanceGroundPoint(input.points[point],input.turn,input.speed) == moved0[point]);
        }
        const auto step0 = groundClippedExecution(input,rng,bitmaps(),viewport);
        assert(controlInstructions(step0) == 1085);
        assert((step0.ground.points() == std::array<GroundPoint,6>{{{832,1600},{-2080,1216},{-6976,2624},{-2432,4544},{3584,4160},{3384,3095}}}));
        assert(step0.rng.indexA() == 92 && step0.rng.indexB() == 92);
        assert(step0.rng.last() == 28208 && step0.rng.calls() == 10);
        const std::array<std::uint16_t,StellarRng::wordCount> future0{26214,24220,22226,20232,18238,16244,14250,12256,10262,8268,6274,4280,2286,292,63834,61840,59846,57852,55858,53864,51870,49876,47882,45888,43894,41900,39906,37912,35918,33924,31930,29936,27942,25948,23954,21960,19966,17972,15978,13984,11990,9996,8002,6008,4014,2020,52,22784,18796,14808,10820,6832,2844,64392,60404,56416};
        auto futureRng0 = step0.rng;
        for (const auto value : future0) { assert(futureRng0.next() == value); }
        const std::array<std::array<int,3>,3> pixels0{{{105,96,7},{107,96,8},{108,96,9}}};
        assert(step0.pixels.size() == pixels0.size());
        for (std::size_t pixel = 0; pixel < pixels0.size(); ++pixel) {
            assert(step0.pixels[pixel].point.x == pixels0[pixel][0]);
            assert(step0.pixels[pixel].point.y == pixels0[pixel][1]);
            assert(step0.pixels[pixel].color == pixels0[pixel][2]);
        }
        input.points = step0.ground.points();
        rng = step0.rng;
        const std::array<GroundPoint,6> moved1{{{1247,1709},{-1380,1362},{-5798,2633},{-1698,4365},{3730,4019},{3550,3058}}};
        for (std::size_t point = 0; point < moved1.size(); ++point) {
            assert(advanceGroundPoint(input.points[point],input.turn,input.speed) == moved1[point]);
        }
        const auto step1 = groundClippedExecution(input,rng,bitmaps(),viewport);
        assert(controlInstructions(step1) == 1049);
        assert((step1.ground.points() == std::array<GroundPoint,6>{{{8128,3776},{3232,3392},{7776,3008},{2880,4416},{3730,4019},{3550,3058}}}));
        assert(step1.rng.indexA() == 76 && step1.rng.indexB() == 76);
        assert(step1.rng.last() == 12256 && step1.rng.calls() == 18);
        const std::array<std::uint16_t,StellarRng::wordCount> future1{10262,8268,6274,4280,2286,292,63834,61840,59846,57852,55858,53864,51870,49876,47882,45888,43894,41900,39906,37912,35918,33924,31930,29936,27942,25948,23954,21960,19966,17972,15978,13984,11990,9996,8002,6008,4014,2020,52,22784,18796,14808,10820,6832,2844,64392,60404,56416,52428,48440,44452,40464,36476,32488,28500,24512};
        auto futureRng1 = step1.rng;
        for (const auto value : future1) { assert(futureRng1.next() == value); }
        const std::array<std::array<int,3>,6> pixels1{{{116,125,3},{118,125,4},{119,125,5},{110,95,7},{112,95,8},{113,95,9}}};
        assert(step1.pixels.size() == pixels1.size());
        for (std::size_t pixel = 0; pixel < pixels1.size(); ++pixel) {
            assert(step1.pixels[pixel].point.x == pixels1[pixel][0]);
            assert(step1.pixels[pixel].point.y == pixels1[pixel][1]);
            assert(step1.pixels[pixel].color == pixels1[pixel][2]);
        }
        input.points = step1.ground.points();
        rng = step1.rng;
    }
    GroundControlState invalid{};
    invalid.speed = 201;
    assert(rejects(invalid));
    invalid.speed = -201;
    assert(rejects(invalid));
    invalid.speed = 10;
    invalid.turn = -1;
    assert(rejects(invalid));
    invalid.turn = 16;
    assert(rejects(invalid));
    invalid.turn = 0;
    invalid.cameraHeight = 1;
    assert(rejects(invalid));
}
