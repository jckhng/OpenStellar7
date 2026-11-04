#include "stellar7/combat_effect_work.hpp"
#include "stellar7/radar_state.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace {
using namespace stellar7;
struct MainControl {
    FailureEffectInput input;
    FailureEffectState output;
    std::uint64_t workHash,pixelHash,writeHash;
    std::size_t pixels;
    std::uint64_t instructionBound;
};
const std::array<MainControl,4> mainControls{{
    {{{100,1800,200},0,{136,{17,18},0,1,12,2760},{24,294,24,145},{159,84},43008,8,3},{140,{166,70},0,2,12,2760},0x5ba684b07f6995f2ULL,0xf8f9912145f7e7d5ULL,0xf0815a790c25a13dULL,39,1086},
    {{{0,127,0},0,{140,{17,18},0,11,12,2760},{24,294,24,145},{159,84},43008,8,3},{136,{17,18},0,0,0,0},0x44f1c71bce705731ULL,0xcbf29ce484222325ULL,0x79856699d7d84986ULL,0,280},
    {{{-1900,1800,0},0,{136,{17,18},0,14,17,2760},{24,294,24,145},{159,84},43008,8,3},{140,{24,84},0,15,17,2760},0xe6aff4bb2d8e2e96ULL,0x1fb3ad12e3f5a575ULL,0x91e77362a653f630ULL,30,1066},
    {{{0,1800,0},0,{140,{17,18},0,28,29,2760},{24,294,24,145},{159,84},43008,8,7},{140,{159,84},0,0,0,0},0xe50f2315894d0ab0ULL,0x6def676aac4214caULL,0x13d3e6d534f317a7ULL,39,1164}
}};
struct RadarControl {
    std::uint64_t workHash,pixelHash;
    std::size_t pixels;
    std::uint64_t instructionBound;
};
const std::array<RadarControl,2> radarControls{{
    {0x442700f62dda4d13ULL,0x7ab6226289613812ULL,6722,2884},
    {0xd5ed1ce037293630ULL,0x938bce93bb729606ULL,6722,2863}
}};
std::vector<IndexedBitmap> frames() {
    std::vector<IndexedBitmap> result(29);
    for (auto& bitmap : result) {
        bitmap = {8,5,std::vector<std::uint8_t>(40)};
        for (std::size_t i = 0; i < bitmap.pixels.size(); ++i) {
            bitmap.pixels[i] = static_cast<std::uint8_t>(i*37+(i/bitmap.width)*13);
        }
    }
    return result;
}
void add(std::uint64_t& hash,const std::uint64_t value) { hash = (hash^value)*1099511628211ULL; }
[[maybe_unused]] std::uint64_t operationHash(const std::vector<NormalWorkOperation>& operations) {
    std::uint64_t hash = 14695981039346656037ULL,pending = 0;
    for (const auto& operation : operations) {
        if (operation.kind == NormalWorkKind::instructions) { pending += operation.count; continue; }
        if (pending != 0) { add(hash,0); add(hash,pending); pending = 0; }
        add(hash,static_cast<std::uint64_t>(operation.kind)); add(hash,operation.count);
    }
    if (pending != 0) { add(hash,0); add(hash,pending); }
    return hash;
}
[[maybe_unused]] std::uint64_t pixelHash(const std::vector<RadarPixelWrite>& pixels) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const auto& pixel : pixels) {
        add(hash,static_cast<std::uint64_t>(pixel.point.x)); add(hash,static_cast<std::uint64_t>(pixel.point.y));
        add(hash,pixel.color); add(hash,pixel.page);
    }
    return hash;
}
[[maybe_unused]] std::uint64_t writeHash(const std::vector<FailureEffectStateWrite>& writes) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const auto& value : writes) {
        if (const auto* scalar = std::get_if<FailureEffectScalarWrite>(&value)) {
            add(hash,0); add(hash,static_cast<std::uint64_t>(scalar->field)); add(hash,static_cast<std::uint64_t>(scalar->value));
        } else {
            const auto& point = std::get<FailureEffectCacheWrite>(value).value;
            add(hash,1); add(hash,static_cast<std::uint64_t>(point.x));
            add(hash,static_cast<std::uint64_t>(point.y)); add(hash,static_cast<std::uint64_t>(point.z));
        }
    }
    return hash;
}
template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
std::vector<RadarActorInput> radarActors(const std::size_t ordinal) {
    RadarActorInput effect{{0x8c,{2048,-4096,950}},0,-1,-4,
        static_cast<std::uint16_t>(ordinal == 0 ? 0x0ac8 : 0),WorldActorRenderProfile::bitmapEffectCallback,17,18,0};
    RadarActorInput lift{{0,{8192,8192,1000}},748,11,-1,0x130c,
        WorldActorRenderProfile::heapSelectorDeferredAnimation,17,18,19};
    return ordinal == 0 ? std::vector<RadarActorInput>{effect,lift} : std::vector<RadarActorInput>{lift,effect};
}
std::vector<std::optional<Level1AnimationState>> animations(const std::size_t ordinal) {
    std::vector<std::optional<Level1AnimationState>> result(2);
    result[ordinal == 0 ? 1 : 0] = Level1AnimationState{5,3,1};
    return result;
}
WorldViewSetupInput view() { return {{0,0,200000},{0xc000,0,0},{0,0,0},0x88,7,{112,205,146,180},{256,-256,362}}; }
RadarRetainedState radarState() { return {600,37,{0xa400,0xa800,0xa000,0xa800,0xa400}}; }
IndexedBitmap panel() { return {120,56,std::vector<std::uint8_t>(120*56,7)}; }
Model3d model() { Model3d result{}; result.flags = 1; result.animationLimits = {8}; return result; }
Level1DynamicRadarWork radar(const std::size_t ordinal) {
    return level1CombatEffectRadarOperations(radarState(),view(),{},0,radarActors(ordinal),{},panel(),{228,138},model(),animations(ordinal));
}
[[maybe_unused]] void receiptHandoff() {
    RadarState owner({panel(),{4,1,{0,0,0,0}},{120,18,std::vector<std::uint8_t>(120*18,3)}});
    const auto pages = radarState().pages;
    owner.initializeLevel1(600,{},0,pages);
    for (const auto index : {0U,1U,3U}) {
        const auto main = level1CombatEffectOperations(mainControls[index].input,frames());
        owner.retainLevel1CombatEffectProjection(32,main);
        const auto& cached = owner.projection(32);
        assert(cached.projectedX == main.state.projected.x && cached.projectedY == main.state.projected.y);
        assert(cached.projectedRadius == main.state.projectedRadius && cached.cachedCenter == main.cachedCenter
            && cached.projected == main.projected && cached.flags == main.state.flags);
        const RadarStateActor actor{32,{main.state.flags,mainControls[index].input.center},0,-1,-4,
            main.state.callback,WorldActorRenderProfile::bitmapEffectCallback};
        const auto drawn = owner.drawLevel1CombatEffects(600,{},0,{actor},pages,{},model(),{std::nullopt});
        assert(drawn.radar.actors.size() == 1 && owner.projection(32).projectedY == 0);
        assert(owner.projection(32).flags == main.state.flags && !drawn.animations.front());
    }
    auto receipt = level1CombatEffectOperations(mainControls.front().input,frames());
    rejects([&] { owner.retainLevel1CombatEffectProjection(31,receipt); });
    rejects([&] { owner.retainLevel1CombatEffectProjection(40,receipt); });
    receipt.projected = false; rejects([&] { owner.retainLevel1CombatEffectProjection(32,receipt); });
    receipt.projected = true; receipt.state.projected.x = 295;
    rejects([&] { owner.retainLevel1CombatEffectProjection(32,receipt); });
    receipt = level1CombatEffectOperations(mainControls.front().input,frames()); receipt.retired = true;
    rejects([&] { owner.retainLevel1CombatEffectProjection(32,receipt); });
}
#ifdef STELLAR7_COMBAT_EFFECT_EXPORT
void exportOperations(const std::vector<NormalWorkOperation>& operations) {
    bool first = true;
    for (const auto& operation : operations) {
        std::cout << (first ? "[" : ",[") << unsigned(operation.kind) << ',' << operation.count << ']'; first = false;
    }
}
void exportPixels(const std::vector<RadarPixelWrite>& pixels) {
    bool first = true;
    for (const auto& pixel : pixels) {
        std::cout << (first ? "[" : ",[") << pixel.point.x << ',' << pixel.point.y << ',' << unsigned(pixel.color) << ',' << pixel.page << ']'; first = false;
    }
}
#endif
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
#ifdef STELLAR7_COMBAT_EFFECT_EXPORT
    std::cout << "{\"effects\":[";
#endif
    for (std::size_t ordinal = 0; ordinal < mainControls.size(); ++ordinal) {
        const auto& control = mainControls[ordinal];
        const auto work = level1CombatEffectOperations(control.input,frames());
#ifdef STELLAR7_COMBAT_EFFECT_EXPORT
        std::cout << (ordinal == 0 ? "{" : ",{") << "\"operations\":["; exportOperations(work.operations);
        std::cout << "],\"pixels\":["; exportPixels(work.pixels);
        const auto& state = work.state;
        std::cout << "],\"state\":{\"flags\":" << unsigned(state.flags) << ",\"projected\":[" << state.projected.x << ',' << state.projected.y
            << "],\"projectedRadius\":" << state.projectedRadius << ",\"age\":" << unsigned(state.age) << ",\"endAge\":" << unsigned(state.endAge)
            << ",\"callback\":" << state.callback << "},\"cachedCenter\":[" << work.cachedCenter.x << ',' << work.cachedCenter.y << ',' << work.cachedCenter.z
            << "],\"projected\":" << (work.projected ? "true" : "false") << ",\"retired\":" << (work.retired ? "true" : "false") << ",\"removal\":";
        if (work.removal) { std::cout << '[' << work.removal->removedIndex << ',' << work.removal->lastIndex << ']'; } else { std::cout << "null"; }
        std::cout << ",\"stateWrites\":[";
        bool first = true;
        for (const auto& value : work.stateWrites) {
            std::cout << (first ? "" : ","); first = false;
            if (const auto* scalar = std::get_if<FailureEffectScalarWrite>(&value)) {
                std::cout << "[0," << unsigned(scalar->field) << ',' << scalar->value << ']';
            } else {
                const auto& point = std::get<FailureEffectCacheWrite>(value).value;
                std::cout << "[1," << point.x << ',' << point.y << ',' << point.z << ']';
            }
        }
        std::cout << "]}";
#else
        for (const auto& operation : work.operations) { assert(operation.kind != NormalWorkKind::instructions || operation.count <= control.instructionBound); }
        assert(operationHash(work.operations) == control.workHash);
        assert(pixelHash(work.pixels) == control.pixelHash && work.pixels.size() == control.pixels);
        assert(writeHash(work.stateWrites) == control.writeHash);
        const auto& expected = control.output;
        assert(work.state.flags == expected.flags && work.state.projected == expected.projected
            && work.state.projectedRadius == expected.projectedRadius && work.state.age == expected.age
            && work.state.endAge == expected.endAge && work.state.callback == expected.callback);
        assert(work.cachedCenter == control.input.center && work.retired == (expected.age == 0));
        assert(work.removal.has_value() == work.retired);
        if (work.removal) { assert(work.removal->removedIndex == *control.input.matchingIndex && work.removal->lastIndex+1 == control.input.allocationCount); }
#endif
    }
#ifdef STELLAR7_COMBAT_EFFECT_EXPORT
    std::cout << "],\"radar\":[";
#endif
    for (std::size_t ordinal = 0; ordinal < radarControls.size(); ++ordinal) {
        const auto work = radar(ordinal);
#ifdef STELLAR7_COMBAT_EFFECT_EXPORT
        std::cout << (ordinal == 0 ? "{" : ",{") << "\"operations\":["; exportOperations(work.radar.operations);
        std::cout << "],\"pixels\":["; exportPixels(work.radar.pixels);
        std::cout << "],\"actors\":[";
        bool first = true;
        for (const auto& actor : work.radar.actors) {
            std::cout << (first ? "[" : ",[") << unsigned(actor.flags) << ',' << actor.projectedX << ',' << actor.projectedY << ',' << actor.projectedRadius
                << ',' << actor.cachedCenter.x << ',' << actor.cachedCenter.y << ',' << actor.cachedCenter.z << ']'; first = false;
        }
        std::cout << "],\"animations\":["; first = true;
        for (const auto& animation : work.animations) {
            std::cout << (first ? "" : ","); first = false;
            if (animation) { std::cout << '[' << unsigned(animation->countdown) << ',' << unsigned(animation->variant) << ']'; } else { std::cout << "null"; }
        }
        const auto& p = work.radar.state.pages; const auto& v = work.radar.viewport; const auto& origin = work.radar.setup.view.origin;
        std::cout << "],\"state\":[" << *work.radar.state.armorStrip << ',' << p.cached << ',' << p.back << ',' << p.front << ',' << p.draw << ',' << p.copy
            << ',' << v.left << ',' << v.right << ',' << v.top << ',' << v.bottom << ',' << unsigned(work.radar.enemyFlag) << ',' << unsigned(work.radar.rasterStyle)
            << ',' << unsigned(work.radar.drawColor) << ',' << unsigned(work.radar.fillColor) << ',' << unsigned(work.radar.radarPass)
            << ',' << origin.x << ',' << origin.y << ',' << origin.z << "]}";
#else
        const auto& control = radarControls[ordinal];
        for (const auto& operation : work.radar.operations) { assert(operation.kind != NormalWorkKind::instructions || operation.count <= control.instructionBound); }
        assert(operationHash(work.radar.operations) == control.workHash);
        assert(work.radar.cpuWorkQualified && pixelHash(work.radar.pixels) == control.pixelHash && work.radar.pixels.size() == control.pixels);
        const auto lift = ordinal == 0 ? 1U : 0U;
        assert(work.animations[lift] && work.animations[lift]->countdown == 2 && work.animations[lift]->variant == 1);
        assert(!work.animations[1-lift] && work.radar.actors[1-lift].flags == 0x8c);
#endif
    }
#ifdef STELLAR7_COMBAT_EFFECT_EXPORT
    std::cout << "]}\n";
#else
    receiptHandoff();
    auto input = mainControls.front().input;
    rejects([&] { failureEffectOperations(input,frames()); });
    input.screenCenter = {159,99}; rejects([&] { level1CombatEffectOperations(input,frames()); });
    input = mainControls.front().input; input.state.age = 0; rejects([&] { level1CombatEffectOperations(input,frames()); });
    input.state.age = 12; rejects([&] { level1CombatEffectOperations(input,frames()); });
    input = mainControls.front().input; input.state.endAge = 18; rejects([&] { level1CombatEffectOperations(input,frames()); });
    input = mainControls.front().input; input.state.flags = 0; rejects([&] { level1CombatEffectOperations(input,frames()); });
    input = mainControls.front().input; input.collisionRadius = 1; rejects([&] { level1CombatEffectOperations(input,frames()); });
    input = mainControls.front().input; input.state.callback = 0; rejects([&] { level1CombatEffectOperations(input,frames()); });
    auto actors = radarActors(0);
    rejects([&] { level1DynamicCombatRadarOperations(radarState(),view(),{},0,actors,{},panel(),{228,138},model(),animations(0)); });
    actors.front().callback = 0x130c;
    rejects([&] { level1CombatEffectRadarOperations(radarState(),view(),{},0,actors,{},panel(),{228,138},model(),animations(0)); });
    actors = radarActors(0); actors.front().category = -3;
    rejects([&] { level1CombatEffectRadarOperations(radarState(),view(),{},0,actors,{},panel(),{228,138},model(),animations(0)); });
    auto callbacks = animations(0); callbacks.front() = Level1AnimationState{5,3,1};
    rejects([&] { level1CombatEffectRadarOperations(radarState(),view(),{},0,radarActors(0),{},panel(),{228,138},model(),callbacks); });
    actors = std::vector<RadarActorInput>(9,radarActors(0).front());
    rejects([&] { level1CombatEffectRadarOperations(radarState(),view(),{},0,actors,{},panel(),{228,138},model(),
        std::vector<std::optional<Level1AnimationState>>(9)); });
#endif
    return 0;
}
