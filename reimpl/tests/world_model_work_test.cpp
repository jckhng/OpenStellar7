#include "stellar7/world_model_work.hpp"
#include "stellar7/gameplay.hpp"
#include "stellar7/level1_owned_view_work.hpp"
#include <cstdio>
#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

#include <cassert>
#include <stdexcept>

namespace {
stellar7::WorldModelGeometryInput input() {
    stellar7::WorldModelGeometryInput result{};
    result.matrix.mode = 1;
    result.matrix.values = {16384, 0, 0, 0, 16384, 0, 0, 0, 16384};
    result.groupOrder = {0};
    for (std::size_t index = 0; index < result.colorRemap.size(); ++index) {
        result.colorRemap[index] = static_cast<std::uint8_t>(index);
    }
    return result;
}

stellar7::ModelLod lod() {
    stellar7::ModelLod result{};
    stellar7::ModelGroup group{};
    group.vertexBlock = 10;
    group.animationOffset = -1;
    group.vertices = {{{0, -256, 0}}, {{-10, 1000, 0}}, {{10, 1000, 0}}, {{0, 1000, 10}}};
    stellar7::ModelPrimitive primitive{};
    primitive.kind = 0;
    primitive.shapes = {{0x81, {8, 8, 1, 1}, 0, {1, 2, 3}}};
    group.primitives = {primitive};
    result.groups = {group};
    return result;
}

std::uint64_t dispatches(const stellar7::WorldModelGeometryWork& work) {
    std::uint64_t count = 0;
    const auto add = [&](const std::vector<stellar7::NormalWorkOperation>& operations) {
        for (const auto& operation : operations) {
            count += operation.kind == stellar7::NormalWorkKind::instructions ? operation.count : 1;
        }
    };
    for (const auto& polygon : work.polygons) {
        add(polygon.prefix);
    }
    add(work.suffix);
    return count;
}

enum class ShadePrecision { normal,wide };
stellar7::WorldModelGeometryWork flatShade(const std::uint8_t level,const int direction,
    const ShadePrecision precision,const std::size_t groupCount=1) {
    const auto wide=precision==ShadePrecision::wide;
    auto state=input();auto model=lod();
    state.matrix.mode=0;state.light={0,0,16384};state.shadeLevels[8]=level;
    for (std::size_t index=0;index<state.lightingAdjustments.size();++index) {state.lightingAdjustments[index]=static_cast<std::uint8_t>(index);}
    model.flags=128;model.threshold=255;
    model.groups[0].vertices[0]={0,0,static_cast<std::int16_t>(direction*4)};
    model.groups[0].primitives[0].shapes[0].flags=0xa0;
    if (groupCount==2) {model.groups.push_back(model.groups[0]);state.groupOrder={0,1};}
    state.fullCenter={0,wide ? 32000 : 2000,0};state.translation={0,wide ? 16000 : 2000,0};
    const stellar7::WorldModelViewInput view{state.matrix,state.fullCenter};
    if (wide) {
        state.vertexPrecision=1;
        return stellar7::level1OwnedWideModelGeometryOperations(model,state,{0,0,0},view);
    }
    return stellar7::level1OwnedModelGeometryOperations(model,state,{0,0,0},view);
}

void flatShadeFamily() {
    // Actual reverse830 matrix/light/normal: direction-17,index95,qualified table byte+2.
    auto state=input();auto model=lod();
    state.matrix={{{10786,4791,11372,-6649,14978,0,-10388,-4622,11804}},3};
    state.matrixProfile=stellar7::WorldModelMatrixProfile::level1Banked;state.animationSelectors={0};
    state.translation={4091,29937,1000};state.fullCenter=state.translation;state.light={-331,-147,362};
    state.shadeLevels[8]=4;state.lightingAdjustments[95]=2;
    model.groups[0].vertices[0]={0,0,-2048};model.groups[0].primitives[0].shapes[0].flags=0xa0;
    const auto actual=stellar7::worldModelGeometryOperations(model,state);
    assert(actual.polygons.size()==1 && actual.polygons[0].primaryColor==10 && actual.polygons[0].secondaryColor==10);
    // Original52BC indexes one256-byte table, including adjacent rows.
    struct Case {std::uint8_t level;int direction,index;};
    for (const auto precision:{ShadePrecision::normal,ShadePrecision::wide}) {
        for (const auto item:{Case{1,-16,0},Case{1,15,31},Case{1,16,31},Case{1,17,33},
            Case{2,-17,31},Case{4,-17,95},Case{4,128,240},Case{8,15,255},Case{8,16,255},
            Case{8,-240,0},Case{1,239,255}}) {
            const auto work=flatShade(item.level,item.direction,precision);
            assert(work.polygons.size()==1);
            const auto expected=static_cast<std::uint8_t>(8+item.index);
            assert(work.polygons[0].primaryColor==expected && work.polygons[0].secondaryColor==expected);
        }
        for (const auto item:{Case{1,-17,-1},Case{7,48,256},Case{4,-113,-1},Case{4,144,256},Case{8,17,257}}) {
            bool rejected=false;
            try {flatShade(item.level,item.direction,precision);}catch(const std::exception&){rejected=true;}
            assert(rejected);
        }
        for (const auto level:{std::uint8_t{0},std::uint8_t{9}}) {
            const auto work=flatShade(level,-17,precision);
            assert(work.polygons.size()==1 && work.polygons[0].primaryColor==8 && work.polygons[0].secondaryColor==8);
        }
        assert(dispatches(flatShade(1,16,precision))==dispatches(flatShade(1,15,precision))+1);
        assert(dispatches(flatShade(2,-17,precision))==dispatches(flatShade(2,-16,precision)));
        const auto reused=flatShade(4,-17,precision,2);
        assert(reused.polygons.size()==2 && reused.polygons[0].primaryColor==103 && reused.polygons[1].primaryColor==103);
    }
    state=input();model=lod();state.matrix.mode=0;state.light={0,0,-544};state.shadeLevels[8]=4;
    model.groups[0].vertices[0]={0,0,2048};model.groups[0].primitives[0].shapes[0].flags=0xa0;
    state.lightingAdjustments[95]=250;state.colorRemap[2]=42;
    model.groups[0].primitives[0].shapes[0].colors={7,8,1,1};state.colorRemap[7]=43;
    const auto remapped=stellar7::worldModelGeometryOperations(model,state);
    assert(remapped.polygons[0].primaryColor==43 && remapped.polygons[0].secondaryColor==42);
}
}

int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
    try {flatShadeFamily();}catch(const std::exception& error) {
        std::fprintf(stderr,"flat52BC family rejected valid flat lookup: %s\n",error.what());return 1;
    }

    bool missingArraysRejected = false;
    try {
        stellar7::worldModelGeometryInput(stellar7::GameplayAssets{}, stellar7::WorldModelSetupWork{});
    } catch (const std::invalid_argument&) {
        missingArraysRejected = true;
    }
    assert(missingArraysRejected);
    auto wideInput = input();
    wideInput.matrix.mode = 0;
    wideInput.vertexPrecision = 1;
    wideInput.fullCenter = {8192, 32217, 0};
    wideInput.translation = {4096, 16108, 0};
    wideInput.groupOrder = {0, 1, 2, 3, 4, 5};
    auto wideModel = lod();
    wideModel.threshold = 8;
    wideModel.flags = 128;
    auto& wideGroup = wideModel.groups.front();
    wideGroup.vertices.resize(14);
    wideGroup.vertices[1] = {-256, 400, 256};
    wideGroup.vertices[2] = {256, -400, -256};
    wideGroup.primitives.front().shapes = {{0x80, {8, 8, 1, 1}, 255, {1, 2}}};
    wideModel.groups.resize(6, wideGroup);
    const auto wide = stellar7::worldModelGeometryOperations(wideModel, wideInput);
    assert(wide.polygons.size() == 6);
    for (const auto& part : wide.polygons) {
        assert(part.points.size() == 2);
        assert(part.points[0].x == 190 && part.points[0].y == 83);
        assert(part.points[1].x == 192 && part.points[1].y == 85);
    }
    wideInput.clipping = stellar7::PolygonClipMode::Enabled;
    bool wideRejected = false;
    try {
        stellar7::worldModelGeometryOperations(wideModel, wideInput);
    } catch (const std::invalid_argument&) {
        wideRejected = true;
    }
    assert(wideRejected);
    auto state = input();
    auto model = lod();
    const auto drawn = stellar7::worldModelGeometryOperations(model, state);
    assert(drawn.polygons.size() == 1);
    const auto& polygon = drawn.polygons.front();
    assert(polygon.primaryColor == 8 && polygon.secondaryColor == 8);
    assert(polygon.filled);
    assert(polygon.points.size() == 3);
    assert(polygon.points[0].x == 158 && polygon.points[0].y == 84);
    assert(polygon.points[1].x == 160 && polygon.points[1].y == 84);
    assert(polygon.points[2].x == 159 && polygon.points[2].y == 83);

    model.groups.front().vertices[0][1] = 256;
    assert(stellar7::worldModelGeometryOperations(model, state).polygons.empty());
    model = lod();
    model.groups.front().primitives.front().shapes.front().flags = 0x82;
    assert(stellar7::worldModelGeometryOperations(model, state).polygons.empty());

    model = lod();
    model.groups.push_back(model.groups.front());
    state.groupOrder = {0, 1};
    const auto aliased = stellar7::worldModelGeometryOperations(model, state);
    assert(aliased.polygons.size() == 2);
    model.groups.back().vertexBlock = 20;
    const auto distinct = stellar7::worldModelGeometryOperations(model, state);
    assert(distinct.polygons.size() == 2);
    assert(dispatches(distinct) > dispatches(aliased));

    model = lod();
    state = input();
    state.colorRemap[8] = 42;
    const auto remapped = stellar7::worldModelGeometryOperations(model, state);
    assert(remapped.polygons.front().primaryColor == 42);
    assert(remapped.polygons.front().secondaryColor == 42);
    state.translation.y = -1000;
    model.groups.front().primitives.front().shapes.front().flags = 0x80;
    bool rejected = false;
    try {
        stellar7::worldModelGeometryOperations(model, state);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
    state = input();
    state.matrix.mode = 2;
    rejected = false;
    try {
        stellar7::worldModelGeometryOperations(model, state);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}
