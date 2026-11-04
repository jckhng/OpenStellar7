#include "stellar7/raster_line_work.hpp"
#include <cassert>
#include <stdexcept>

int main() {
    using namespace stellar7;
    const Viewport viewport{24, 294, 24, 145};
    const auto point = collapsedPolygonOperations({{40, 80}, {40, 80}}, viewport, PolygonClipMode::Disabled);
    assert((point.pixels == std::vector<ProjectedPoint>{{40, 80}}));
    const auto empty = collapsedPolygonOperations({{40, 80}, {40, 80}, {40, 80}}, viewport, PolygonClipMode::Disabled);
    assert(empty.pixels.empty());
    for (const auto& operation : empty.operations) {
        assert(operation.kind != NormalWorkKind::port8Write && operation.kind != NormalWorkKind::port16Write);
    }
    const auto duplicateLine = collapsedPolygonOperations({{49, 85}, {49, 85}, {48, 84}, {48, 84}, {48, 84}},
        viewport, PolygonClipMode::Disabled);
    assert((duplicateLine.pixels == std::vector<ProjectedPoint>{{48, 84}, {49, 85}}));
    const auto clippedLine = collapsedPolygonOperations({{25, 85}, {25, 85}, {23, 84}, {22, 84}, {23, 84}},
        viewport, PolygonClipMode::Enabled);
    assert((clippedLine.pixels == std::vector<ProjectedPoint>{{24, 84}, {25, 85}}));
    assert(clippedLine.operations[0].kind == NormalWorkKind::instructions && clippedLine.operations[0].count == 19);
    assert(clippedLine.operations[1].kind == NormalWorkKind::repeatString && clippedLine.operations[1].count == 5);
    assert(clippedLine.operations[2].kind == NormalWorkKind::instructions && clippedLine.operations[2].count == 3);
    assert(clippedLine.operations[3].kind == NormalWorkKind::repeatString && clippedLine.operations[3].count == 5);
    const auto outsideHorizontal = collapsedPolygonOperations({{22, 85}, {21, 85}, {20, 84}, {20, 84}, {21, 84}},
        viewport, PolygonClipMode::Enabled);
    assert(outsideHorizontal.pixels.empty());
    std::size_t zeroCopies = 0;
    for (const auto& operation : outsideHorizontal.operations) {
        if (operation.kind == NormalWorkKind::repeatString && operation.count == 0) {
            ++zeroCopies;
        }
        assert(operation.kind != NormalWorkKind::port8Write && operation.kind != NormalWorkKind::port16Write);
    }
    assert(zeroCopies == 2);
    bool verticalEmptyRejected = false;
    try {
        collapsedPolygonOperations({{40, 20}, {45, 20}, {43, 21}}, viewport, PolygonClipMode::Enabled);
    } catch (const std::invalid_argument&) {
        verticalEmptyRejected = true;
    }
    assert(verticalEmptyRejected);
    const auto horizontal = collapsedPolygonOperations({{45, 80}, {43, 80}, {40, 80}}, viewport, PolygonClipMode::Disabled);
    assert(horizontal.pixels.size() == 6 && horizontal.pixels.front().x == 40 && horizontal.pixels.back().x == 45);
    std::uint64_t ports = 0;
    for (const auto& operation : horizontal.operations) {
        if (operation.kind == NormalWorkKind::port8Write || operation.kind == NormalWorkKind::port16Write) {
            ports += operation.count;
        }
    }
    assert(ports == 3);
    const auto steep = rasterLineOperations({40, 85}, {42, 80}, viewport, PolygonClipMode::Disabled);
    assert((steep.pixels == std::vector<ProjectedPoint>{{42, 80}, {42, 81}, {41, 82}, {41, 83}, {40, 84}, {40, 85}}));
    bool areaRejected = false;
    try {
        collapsedPolygonOperations({{40, 80}, {45, 80}, {43, 85}}, viewport, PolygonClipMode::Disabled);
    } catch (const std::invalid_argument&) {
        areaRejected = true;
    }
    assert(areaRejected);
    bool clipRejected = false;
    try {
        rasterLineOperations({23, 80}, {45, 80}, viewport, PolygonClipMode::Disabled);
    } catch (const std::invalid_argument&) {
        clipRejected = true;
    }
    assert(clipRejected);
    const auto armorColumn = rasterArmorColumnOperations(112);
    assert(armorColumn.pixels.size() == 7);
    assert(armorColumn.pixels.front().x == 112 && armorColumn.pixels.front().y == 189);
    assert(armorColumn.pixels.back().x == 112 && armorColumn.pixels.back().y == 195);
    bool armorRejected = false;
    try { rasterArmorColumnOperations(113); }
    catch (const std::invalid_argument&) { armorRejected = true; }
    assert(armorRejected);
    armorRejected = false;
    try { rasterLineOperations({112, 189}, {112, 195}, {24, 294, 24, 145}, PolygonClipMode::Disabled); }
    catch (const std::invalid_argument&) { armorRejected = true; }
    assert(armorRejected);
    assert(rasterArmorColumnOperations(196).pixels.size() == 7);
}
