#include "stellar7/failure_model_work.hpp"
#include "stellar7/raster_line_work.hpp"

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
struct Control {
    const char* label;
    std::vector<ProjectedPoint> points;
    bool filled;
    bool clipping;
    std::uint8_t primary,secondary;
    std::uint64_t workHash,pixelHash;
    std::size_t pixelCount;
    std::uint64_t instructionBound;
};
const std::vector<Control> controls{
    {"actual-frame128-collapsed-color",{{85,89},{85,87},{85,87},{85,90}},true,false,72,156,0xdfcdf7f6d21ce373ULL,0xa91ef27d1f1bc5e7ULL,14,976},
    {"ordinary-solid-outline",{{80,60},{240,60},{160,140}},true,false,72,156,0xb926796a02b8984eULL,0x7556babbdb796031ULL,6884,7286},
    {"ordinary-clipped-solid-outline",{{10,50},{160,10},{310,160}},true,true,72,156,0xaed6b54c8dbaeecbULL,0x0a2d0e7a2786dc1bULL,14269,15882},
    {"ordinary-clipped-outline",{{10,50},{160,10},{310,160}},false,true,72,156,0x79afb39ad93909a3ULL,0x0872977e16b1d4dbULL,453,9018},
    {"actual-frame137-clipped-early-collapse",{{109,89},{109,87},{109,87},{109,89}},true,true,72,156,0xae85f5c397208c50ULL,0xda350b7c1ee3fae0ULL,3,456}
};
constexpr Viewport ordinary{24,294,24,145};
constexpr Viewport failure{0,318,0,199};
void add(std::uint64_t& hash, const std::uint64_t value) {
    hash = (hash ^ value) * 1099511628211ULL;
}
[[maybe_unused]] std::uint64_t workHash(const std::vector<NormalWorkOperation>& work) {
    std::uint64_t hash = 14695981039346656037ULL, pending = 0;
    for (const auto& operation : work) {
        if (operation.kind == NormalWorkKind::instructions) { pending += operation.count; continue; }
        if (pending != 0) { add(hash,0); add(hash,pending); pending = 0; }
        add(hash,static_cast<std::uint64_t>(operation.kind)); add(hash,operation.count);
    }
    if (pending != 0) { add(hash,0); add(hash,pending); }
    return hash;
}
[[maybe_unused]] std::uint64_t pixelHash(const std::vector<WorldPixelWrite>& pixels) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const auto& pixel : pixels) {
        add(hash,static_cast<std::uint16_t>(pixel.point.x));
        add(hash,static_cast<std::uint16_t>(pixel.point.y)); add(hash,pixel.color);
    }
    return hash;
}
template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
FailurePolygonWork plan(const Control& input, const Viewport viewport) {
    return ownedPolygonOperations(input.points,viewport,
        input.clipping ? PolygonClipMode::Enabled : PolygonClipMode::Disabled,
        input.filled ? FailurePolygonStyle::filled : FailurePolygonStyle::outline,
        input.primary,input.secondary);
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
#ifdef STELLAR7_POLYGON_EXPORT
    std::cout << "{\"rows\":[";
#endif
    for (std::size_t ordinal = 0; ordinal < controls.size(); ++ordinal) {
        const auto& control = controls[ordinal];
        const auto work = plan(control,ordinary);
#ifdef STELLAR7_POLYGON_EXPORT
        std::cout << (ordinal == 0 ? "{" : ",{") << "\"label\":\"" << control.label << "\",\"operations\":[";
        for (std::size_t i = 0; i < work.operations.size(); ++i) {
            const auto& op = work.operations[i];
            std::cout << (i == 0 ? "[" : ",[") << unsigned(op.kind) << ',' << op.count << ']';
        }
        std::cout << "],\"pixels\":[";
        for (std::size_t i = 0; i < work.pixels.size(); ++i) {
            const auto& pixel = work.pixels[i];
            std::cout << (i == 0 ? "[" : ",[") << pixel.point.x << ',' << pixel.point.y << ',' << unsigned(pixel.color) << ']';
        }
        std::cout << "]}";
#else
        for (const auto& operation : work.operations) {
            assert(operation.kind != NormalWorkKind::instructions || operation.count <= control.instructionBound);
        }
        assert(workHash(work.operations) == control.workHash);
        assert(work.pixels.size() == control.pixelCount && pixelHash(work.pixels) == control.pixelHash);
#endif
    }
#ifdef STELLAR7_POLYGON_EXPORT
    std::cout << "]}\n";
#else
    const auto& actual = controls.front();
    //The strict failure entry keeps its old viewport rejection.
    rejects([&] { failurePolygonOperations(actual.points,ordinary,PolygonClipMode::Disabled,
        FailurePolygonStyle::filled,actual.primary,actual.secondary); });
    const auto compatible = failurePolygonOperations(actual.points,failure,PolygonClipMode::Disabled,
        FailurePolygonStyle::filled,actual.primary,actual.secondary);
    const auto shared = plan(actual,failure);
    assert(workHash(compatible.operations) == workHash(shared.operations));
    assert(pixelHash(compatible.pixels) == pixelHash(shared.pixels));
    rejects([&] { plan(actual,{24,295,24,145}); });
    rejects([&] { plan(actual,{0,319,0,199}); });
    rejects([&] { ownedPolygonOperations(actual.points,ordinary,static_cast<PolygonClipMode>(99),
        FailurePolygonStyle::filled,72,156); });
    rejects([&] { ownedPolygonOperations(actual.points,ordinary,PolygonClipMode::Disabled,
        static_cast<FailurePolygonStyle>(99),72,156); });
    rejects([&] { ownedPolygonOperations(std::vector<ProjectedPoint>(21,{85,89}),ordinary,
        PolygonClipMode::Disabled,FailurePolygonStyle::filled,72,156); });
    rejects([&] { ownedPolygonOperations({{16384,89}},ordinary,
        PolygonClipMode::Enabled,FailurePolygonStyle::filled,72,156); });
#endif
    return 0;
}
