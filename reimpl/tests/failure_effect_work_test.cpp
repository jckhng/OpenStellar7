#include "stellar7/failure_effect_work.hpp"
#include <array>
#include <cassert>
#include <stdexcept>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
namespace {
using namespace stellar7;
template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
std::uint64_t operationHash(const std::vector<NormalWorkOperation>& operations) {
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t hash = 14695981039346656037ULL,pending = 0;
    const auto append = [&](auto kind,auto count) { hash = (hash^static_cast<std::uint64_t>(kind))*prime; hash = (hash^count)*prime; };
    for (const auto& o : operations) {
        if (o.kind == NormalWorkKind::instructions) { pending += o.count; continue; }
        if (pending != 0) { append(NormalWorkKind::instructions,pending); pending = 0; }
        append(o.kind,o.count);
    }
    if (pending != 0) { append(NormalWorkKind::instructions,pending); }
    return hash;
}
std::uint64_t pixelHash(const std::vector<RadarPixelWrite>& pixels) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const auto& p : pixels) {
        for (const auto value : {static_cast<std::uint64_t>(p.point.x),static_cast<std::uint64_t>(p.point.y),
            static_cast<std::uint64_t>(p.color),static_cast<std::uint64_t>(p.page)}) {
            hash = (hash^value)*1099511628211ULL;
        }
    }
    return hash;
}
std::vector<IndexedBitmap> frames() {
    std::vector<IndexedBitmap> result(31);
    constexpr std::array<std::array<std::uint16_t,2>,3> sizes{{{88,63},{96,68},{56,46}}};
    for (std::size_t frame = 14; frame < 17; ++frame) {
        const auto& size = sizes[frame-14];
        auto& bitmap = result[frame]; bitmap.width = size[0]; bitmap.height = size[1];
        bitmap.pixels.resize(static_cast<std::size_t>(bitmap.width)*bitmap.height);
        for (std::size_t i = 0; i < bitmap.pixels.size(); ++i) {
            bitmap.pixels[i] = static_cast<std::uint8_t>(i*37+(i/bitmap.width)*13);
        }
    }
    return result;
}
struct Control { FailureEffectInput input; std::uint64_t workHash,pixelsHash; std::size_t pixels; };
const std::array<Control,11> controls{{
    {{{0,1800,0},0,{136, {17,18},0,14,17,0xac8},{0,318,0,199},{159,100},43008,40,39},0xa4557178deaf61b4ULL,0xf4c8cd5f465b7e68ULL,5521},
    {{{-100,1800,200},0,{140, {17,18},0,15,17,0xac8},{0,318,0,199},{159,100},40960,40,5},0xba6af0145f9af1caULL,0xbbbf17d6a7e80b7fULL,6503},
    {{{0,1800,0},0,{136, {17,18},0,16,17,0xac8},{0,318,0,199},{159,100},43008,40,5},0xb06b0c184d56a65dULL,0xb851abd6660fbfaeULL,2575},
    {{{0,1800,0},0,{140, {17,18},0,16,17,0xac8},{0,318,0,199},{159,100},40960,40,39},0xb06a11184d54fbdcULL,0xd59d171970df47aeULL,2575},
    {{{0,127,0},0,{140, {17,18},0,14,17,0xac8},{0,318,0,199},{159,100},40960,40,5},0x44f1f21bce70a042ULL,0xcbf29ce484222325ULL,0},
    {{{0,-1,0},0,{140, {17,18},0,16,17,0xac8},{0,318,0,199},{159,100},43008,0,std::nullopt},0xd55237b02ffdfc7aULL,0xcbf29ce484222325ULL,0},
    {{{-4000,1800,0},0,{136, {17,18},0,16,17,0xac8},{0,318,0,199},{159,100},40960,3,std::nullopt},0xbf5c75da99af97d6ULL,0xcbf29ce484222325ULL,0},
    {{{-2200,1800,0},0,{136, {17,18},0,14,17,0xac8},{0,318,0,199},{159,100},43008,40,5},0x72cb607900eab211ULL,0x0fd2f9bea0b7326fULL,3766},
    {{{2200,1800,0},0,{136, {17,18},0,15,17,0xac8},{0,318,0,199},{159,100},40960,40,5},0x26378384b44ba442ULL,0x248dbe8119113ec8ULL,2575},
    {{{0,1800,1400},0,{136, {17,18},0,16,17,0xac8},{0,318,0,199},{159,100},43008,40,5},0xbfe28eaf0c3bbb80ULL,0xabd4d8d91be4bcd5ULL,1344},
    {{{0,1800,-1400},0,{136, {17,18},0,14,17,0xac8},{0,318,0,199},{159,100},40960,40,5},0xcb61a07a8510f235ULL,0xb89293f09a101e45ULL,2804}
}};
const std::array<Control,6> center99Controls{{
    {{{0,1800,0},0,{136,{17,18},0,14,17,0xac8},{0,318,0,199},{159,99},43008,40,39},0xa4557178deaf61b4ULL,0x8d3ff61f7f6c9061ULL,5521},
    {{{-100,1800,200},0,{140,{17,18},0,15,17,0xac8},{0,318,0,199},{159,99},40960,40,5},0xba6af0145f9af1caULL,0x19efcd571a03082aULL,6503},
    {{{0,1800,0},0,{136,{17,18},0,16,17,0xac8},{0,318,0,199},{159,99},43008,40,5},0xb06b0c184d56a65dULL,0x43a5a8bfa0e7b949ULL,2575},
    {{{0,127,0},0,{140,{17,18},0,14,17,0xac8},{0,318,0,199},{159,99},40960,40,5},0x44f1f21bce70a042ULL,0xcbf29ce484222325ULL,0},
    {{{0,1800,1400},0,{136,{17,18},0,16,17,0xac8},{0,318,0,199},{159,99},43008,40,5},0x30433dd9955ade38ULL,0x6368c022b5b089adULL,1288},
    {{{0,1800,-1400},0,{136,{17,18},0,14,17,0xac8},{0,318,0,199},{159,99},40960,40,5},0x9cbaef34b3a1a9cdULL,0xce79188382cd6fffULL,2892}
}};
void sourceControls() {
    const auto bitmaps = frames();
    std::vector<Control> all(controls.begin(),controls.end());
    all.insert(all.end(),center99Controls.begin(),center99Controls.end());
    for (const auto& c : all) {
        const auto work = failureEffectOperations(c.input,bitmaps);
        assert(operationHash(work.operations) == c.workHash);
        assert(work.pixels.size() == c.pixels && pixelHash(work.pixels) == c.pixelsHash);
        assert(work.cachedCenter == c.input.center && work.projected == ((work.state.flags&4) != 0));
        assert(work.retired == (c.input.state.age == 16));
        assert(work.state.age == (work.retired ? 0 : c.input.state.age+1));
        assert(work.state.callback == (work.retired ? 0 : 0xac8));
        assert(work.state.endAge == (work.retired ? 0 : 17));
        assert(work.removal.has_value() == (work.retired && c.input.matchingIndex.has_value()));
        if (work.removal) {
            assert(work.removal->removedIndex == *c.input.matchingIndex
                && work.removal->lastIndex == c.input.allocationCount-1);
        }
        assert(std::get<FailureEffectCacheWrite>(work.stateWrites.front()).value == c.input.center);
        std::vector<FailureEffectField> fields;
        for (const auto& write : work.stateWrites) {
            if (const auto* scalar = std::get_if<FailureEffectScalarWrite>(&write)) { fields.push_back(scalar->field); }
        }
        std::vector<FailureEffectField> expected{FailureEffectField::flags};
        if (work.projected) {
            expected.insert(expected.end(),{FailureEffectField::projectedRadius,FailureEffectField::projectedX,
                FailureEffectField::projectedY,FailureEffectField::flags});
        } else { assert(work.state.projected.x == 17 && work.state.projected.y == 18); }
        expected.insert(expected.end(),{FailureEffectField::rasterStyle,FailureEffectField::age,FailureEffectField::rasterStyle});
        if (work.removal) { expected.push_back(FailureEffectField::registryRefresh); }
        if (work.retired) { expected.insert(expected.end(),{FailureEffectField::callback,FailureEffectField::age,FailureEffectField::endAge}); }
        assert(fields == expected);
    }
    //Unprojected callbacks age without touching the bitmap table/provider.
    assert(!failureEffectOperations(controls[4].input,{}).projected);
}
void guards() {
    const auto bitmaps = frames();
    for (unsigned mutation = 0; mutation < 11; ++mutation) {
        auto input = controls[0].input;
        if (mutation == 0) { input.collisionRadius = 1; }
        if (mutation == 1) { input.state.flags = 8; }
        if (mutation == 2) { input.state.age = 13; }
        if (mutation == 3) { input.state.age = 17; }
        if (mutation == 4) { input.state.endAge = 18; }
        if (mutation == 5) { input.state.callback = 0; }
        if (mutation == 6) { input.viewport.right = 319; }
        if (mutation == 7) { input.center.y = 32768; }
        if (mutation == 8) { input.center.x = 65536; }
        if (mutation == 9) { input.matchingIndex = input.allocationCount; }
        if (mutation == 10) { input.page = 0xa400; }
        rejects([&] { failureEffectOperations(input,bitmaps); });
    }
    rejects([&] { failureEffectOperations(controls[0].input,{}); });
    for (const auto center : {98,101}) {
        auto input = center99Controls[0].input;
        input.screenCenter.y = static_cast<std::int16_t>(center);
        rejects([&] { failureEffectOperations(input,bitmaps); });
    }
    assert(failureEffectOperations(center99Controls[0].input,bitmaps).state.projected.y == 99);
    auto wrong = bitmaps; wrong[14].height = 64;
    rejects([&] { failureEffectOperations(controls[0].input,wrong); });
}
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
    sourceControls();
    guards();
}
