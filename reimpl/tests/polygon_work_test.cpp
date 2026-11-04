#include "stellar7/polygon_work.hpp"
#include <cassert>
#include <stdexcept>
namespace {
std::uint64_t count(const std::vector<stellar7::NormalWorkOperation>& operations, stellar7::NormalWorkKind kind) {
    std::uint64_t result=0;
    for (const auto& operation:operations) {
        if (operation.kind==kind) { result+=operation.count; }
    }
    return result;
}
}
int main() {
    using Kind=stellar7::NormalWorkKind;
    const stellar7::Viewport viewport{24,295,24,145};
    const std::vector<stellar7::ProjectedPoint> points{{40,40},{100,70},{45,90}};
    const auto work=stellar7::solidPolygonOperations(points,viewport,stellar7::PolygonClipMode::Disabled);
    assert(work.operations[0].kind==Kind::instructions && work.operations[0].count==19);
    assert(work.operations[1].kind==Kind::repeatString && work.operations[1].count==points.size());
    assert(work.operations[2].kind==Kind::instructions && work.operations[2].count==3);
    assert(work.operations[3].kind==Kind::repeatString && work.operations[3].count==points.size());
    assert(!work.raster.polygon.rows.empty());
    std::uint64_t transfers=count(work.operations,Kind::string)+count(work.operations,Kind::repeatString);
    assert(transfers==work.raster.work.stringIterations);
    bool interleaved=false;
    for (std::size_t index=1;index+1<work.operations.size();++index) {
        if (work.operations[index].kind==Kind::port16Write && work.operations[index+1].kind==Kind::instructions) {
            interleaved=true;
        }
    }
    assert(interleaved);
    // Original3467 raw12 type13 group0/shape5 preserves tied vertex identity after reversal.
    const auto tied=stellar7::solidPolygonOperations({{49,84},{48,84},{49,84},{50,85},{50,85}},
        viewport,stellar7::PolygonClipMode::Disabled);
    constexpr std::uint64_t originalTiedDispatches=595;
    constexpr std::uint64_t originalTiedTransfers=30;
    std::uint64_t tiedDispatches=0;
    for (const auto& operation:tied.operations) {
        tiedDispatches+=operation.kind==Kind::repeatString ? 1 : operation.count;
    }
    assert(tiedDispatches==originalTiedDispatches);
    assert(count(tied.operations,Kind::string)+count(tied.operations,Kind::repeatString)==originalTiedTransfers);
    const std::vector<stellar7::ProjectedPoint> raw35{{32,84},{33,84},{32,85},{30,85},{30,84}};
    const auto unclipped = stellar7::solidPolygonOperations(raw35, viewport, stellar7::PolygonClipMode::Disabled);
    const auto clipped = stellar7::solidPolygonOperations(raw35, viewport, stellar7::PolygonClipMode::Enabled);
    constexpr std::uint64_t originalClipCallDispatches = 264;
    assert(count(clipped.operations, Kind::instructions)
        == count(unclipped.operations, Kind::instructions) + originalClipCallDispatches);
    assert(count(clipped.operations, Kind::string) == count(unclipped.operations, Kind::string));
    assert(count(clipped.operations, Kind::repeatString) == count(unclipped.operations, Kind::repeatString));
    assert(clipped.raster.polygon.rows.size() == unclipped.raster.polygon.rows.size());
    bool rejected=false;
    try { stellar7::solidPolygonOperations({{40,40},{50,40},{60,40}},viewport,stellar7::PolygonClipMode::Disabled); }
    catch (const std::invalid_argument&) { rejected=true; }
    assert(rejected);
}
