#include "stellar7/raster_line_work.hpp"
#include "stellar7/failure_model_work.hpp"
#include <cassert>
#include <stdexcept>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
namespace {
using namespace stellar7;
struct Control {
    Viewport viewport;
    std::vector<ProjectedPoint> points;
    std::uint64_t workHash, pixelHash;
    std::size_t pixelCount;
};
std::uint64_t workHash(const std::vector<NormalWorkOperation>& operations) {
    std::uint64_t hash = 14695981039346656037ULL, pending = 0;
    const auto add = [&](const auto kind, const auto count) {
        hash = (hash ^ static_cast<std::uint64_t>(kind)) * 1099511628211ULL;
        hash = (hash ^ count) * 1099511628211ULL;
    };
    for (const auto& operation : operations) {
        if (operation.kind == NormalWorkKind::instructions) { pending += operation.count; continue; }
        if (pending != 0) { add(NormalWorkKind::instructions, pending); pending = 0; }
        add(operation.kind, operation.count);
    }
    if (pending != 0) { add(NormalWorkKind::instructions, pending); }
    return hash;
}
std::uint64_t pixelHash(const std::vector<ProjectedPoint>& pixels) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const auto point : pixels) {
        hash = (hash ^ static_cast<std::uint64_t>(point.x)) * 1099511628211ULL;
        hash = (hash ^ static_cast<std::uint64_t>(point.y)) * 1099511628211ULL;
    }
    return hash;
}
template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
const std::vector<Control> controls{
    //ordinary-horizontal-low-empty
    {{24,294,24,145},{{19,50},{22,60},{23,55}},0xf324d0f6c635a8ebULL,0xcbf29ce484222325ULL,0},
    //ordinary-horizontal-high-empty
    {{24,294,24,145},{{299,50},{296,60},{295,55}},0xf324d0f6c635a8ebULL,0xcbf29ce484222325ULL,0},
    //ordinary-vertical-low-empty
    {{24,294,24,145},{{50,19},{60,22},{55,23}},0xd847fd7bbd1dc274ULL,0xcbf29ce484222325ULL,0},
    //ordinary-vertical-high-empty
    {{24,294,24,145},{{50,150},{60,147},{55,146}},0xd847fd7bbd1dc274ULL,0xcbf29ce484222325ULL,0},
    //ordinary-horizontal-boundary-point
    {{24,294,24,145},{{19,50},{24,50},{23,50}},0xd846a57bbd1b79ecULL,0xcbf29ce484222325ULL,0},
    //ordinary-vertical-boundary-point
    {{24,294,24,145},{{50,19},{50,24},{50,23}},0xd846a57bbd1b79ecULL,0xcbf29ce484222325ULL,0},
    //ordinary-horizontal-clipped-line
    {{24,294,24,145},{{19,50},{39,50},{29,50}},0x210d829a748f7f88ULL,0x3e1efc92e493b8e5ULL,16},
    //ordinary-vertical-clipped-line
    {{24,294,24,145},{{50,19},{60,29},{55,24}},0xbd86b7c25e1022afULL,0x458ccda7e2926ac1ULL,6},
    //ordinary-diagonal-duplicate-cleanup
    {{24,294,24,145},{{50,50},{60,60},{60,60}},0x7a1856a5ecc4ca6aULL,0x42628005424570a3ULL,11},
    //ordinary-horizontal-interior
    {{24,294,24,145},{{50,50},{60,50},{55,50}},0xa8d028f67ff15dc5ULL,0x2968ea1d6251207aULL,11},
    //failure-horizontal-low-empty
    {{0,318,0,199},{{-5,50},{-2,60},{-1,55}},0xf324d0f6c635a8ebULL,0xcbf29ce484222325ULL,0},
    //failure-horizontal-high-empty
    {{0,318,0,199},{{323,50},{320,60},{319,55}},0xf324d0f6c635a8ebULL,0xcbf29ce484222325ULL,0},
    //failure-vertical-low-empty
    {{0,318,0,199},{{50,-5},{60,-2},{55,-1}},0xd847fd7bbd1dc274ULL,0xcbf29ce484222325ULL,0},
    //failure-vertical-high-empty
    {{0,318,0,199},{{50,204},{60,201},{55,200}},0xd847fd7bbd1dc274ULL,0xcbf29ce484222325ULL,0},
    //failure-horizontal-boundary-point
    {{0,318,0,199},{{-5,50},{0,50},{-1,50}},0xd846a57bbd1b79ecULL,0xcbf29ce484222325ULL,0},
    //failure-vertical-boundary-point
    {{0,318,0,199},{{50,-5},{50,0},{50,-1}},0xd846a57bbd1b79ecULL,0xcbf29ce484222325ULL,0},
    //failure-horizontal-clipped-line
    {{0,318,0,199},{{-5,50},{15,50},{5,50}},0x210d829a748f7f88ULL,0x528944179a7e3585ULL,16},
    //failure-vertical-clipped-line
    {{0,318,0,199},{{50,-5},{60,5},{55,0}},0xbd86b7c25e1022afULL,0x8f05b21c4534aba1ULL,6},
    //failure-diagonal-duplicate-cleanup
    {{0,318,0,199},{{50,50},{60,60},{60,60}},0x7a1856a5ecc4ca6aULL,0x42628005424570a3ULL,11},
    //failure-horizontal-interior
    {{0,318,0,199},{{50,50},{60,50},{55,50}},0xa8d028f67ff15dc5ULL,0x2968ea1d6251207aULL,11},
};
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    using namespace stellar7;
    for (const auto& control : controls) {
        const auto work = ownedCollapsedPolygonOperations(control.points, control.viewport, PolygonClipMode::Enabled);
        assert(workHash(work.operations) == control.workHash);
        assert(work.pixels.size() == control.pixelCount);
        assert(pixelHash(work.pixels) == control.pixelHash);
        if (control.viewport == Viewport{0,318,0,199}) {
            const auto failure = failurePolygonOperations(control.points, control.viewport,
                PolygonClipMode::Enabled, FailurePolygonStyle::filled, 72, 72);
            assert(workHash(failure.operations) == control.workHash);
            std::vector<ProjectedPoint> points;
            for (const auto& pixel : failure.pixels) { assert(pixel.color == 72); points.push_back(pixel.point); }
            assert(points == work.pixels);
        }
    }
    const Viewport viewport{24,294,24,145};
    const std::vector<ProjectedPoint> empty{{50,19},{60,22},{55,23}};
    rejects([&] { collapsedPolygonOperations(empty, viewport, PolygonClipMode::Enabled); });
    rejects([&] { ownedCollapsedPolygonOperations(empty, {0,319,0,199}, PolygonClipMode::Enabled); });
    rejects([&] { ownedCollapsedPolygonOperations({{50,50},{60,50}}, viewport, PolygonClipMode::Enabled); });
    rejects([&] { ownedCollapsedPolygonOperations({{50,50},{60,50},{55,60}}, viewport, PolygonClipMode::Enabled); });
    rejects([&] { ownedCollapsedPolygonOperations({{-16384,50},{60,50},{55,50}}, viewport, PolygonClipMode::Enabled); });
    rejects([&] { ownedCollapsedPolygonOperations({{50,50},{60,50},{55,50}}, viewport, static_cast<PolygonClipMode>(2)); });
    rejects([&] { ownedCollapsedPolygonOperations(empty, viewport, PolygonClipMode::Disabled); });
    const std::vector<ProjectedPoint> line{{50,50},{60,50},{55,50}};
    const auto old = collapsedPolygonOperations(line, viewport, PolygonClipMode::Disabled);
    const auto owned = ownedCollapsedPolygonOperations(line, viewport, PolygonClipMode::Disabled);
    assert(old.pixels == owned.pixels && workHash(old.operations) == workHash(owned.operations));
}
