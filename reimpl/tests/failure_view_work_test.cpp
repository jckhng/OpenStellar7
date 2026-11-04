#include "stellar7/failure_view_work.hpp"

#include <cassert>
#include <limits>
#include <stdexcept>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace {
using namespace stellar7;
template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
}
struct MatrixControl {
    std::array<std::uint16_t,3> angles;
    FixedMatrixQ14 matrix;
    std::uint64_t dispatches;
    FixedPosition light;
    std::uint64_t viewSuffix;
};
// Frozen independent source34AE/B7AC CPU controls, including low-bit/wrap paths.
constexpr std::array<MatrixControl,5> controls{{
    {{0,0,0},{{16384,0,0,0,16384,0,0,0,16384},0},39,{256,-256,362},149},
    {{0,0,65535},{{16383,-26,0,26,16383,0,0,0,16384},1},70,{256,-256,362},214},
    {{0,32768,8193},{{-11577,-11586,0,-11584,11575,0,8,9,-16384},2},343,{0,-362,-363},520},
    {{1,0,0},{{16384,0,13,13,16384,0,0,0,16384},3},341,{256,-256,362},518},
    {{12345,54321,65535},{{7788,-13349,5448,6,6200,15165,-14421,-7208,2947},3},347,{450,238,-48},524},
}};
}
int main() {
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    using namespace stellar7;
    for (const auto& c : controls) {
        const auto build = fixedMatrixBuildOperations(c.angles);
        assert(build.matrix.values == c.matrix.values && build.matrix.mode == c.matrix.mode);
        assert(build.operations.size() == 1 && build.operations[0].kind == NormalWorkKind::instructions);
        assert(build.operations[0].count == c.dispatches);
        const auto view = failureViewSetupOperations({{-1800,0,0},{},c.angles,0x88,7,{0,318,0,199},{256,-256,362}});
        auto transposed = c.matrix;
        transpose(transposed);
        assert(view.view.matrix.values == transposed.values && view.view.matrix.mode == transposed.mode);
        assert(view.transformedLight == c.light && view.operations.back().count == c.viewSuffix);
    }
    {
        const auto product = fixedMatrixComposeOperations(controls[0].matrix,controls[4].matrix,0x5240);
        const std::array<std::int16_t,9> expected0 = {7788,-13349,5448,6,6200,15165,-14421,-7208,2947};
        assert(product.matrix.values == expected0 && product.matrix.mode == 3);
        assert(product.operations.size() == 3);
        assert(product.operations[0].kind == NormalWorkKind::instructions && product.operations[0].count == 19);
        assert(product.operations[1].kind == NormalWorkKind::repeatString && product.operations[1].count == 9);
        assert(product.operations[2].kind == NormalWorkKind::instructions && product.operations[2].count == 7);
    }
    {
        const auto product = fixedMatrixComposeOperations(controls[4].matrix,controls[0].matrix,0x5240);
        const std::array<std::int16_t,9> expected1 = {7788,-13349,5448,6,6200,15165,-14421,-7208,2947};
        assert(product.matrix.values == expected1 && product.matrix.mode == 3);
        assert(product.operations.size() == 3);
        assert(product.operations[0].kind == NormalWorkKind::instructions && product.operations[0].count == 23);
        assert(product.operations[1].kind == NormalWorkKind::repeatString && product.operations[1].count == 9);
        assert(product.operations[2].kind == NormalWorkKind::instructions && product.operations[2].count == 7);
    }
    {
        const auto product = fixedMatrixComposeOperations(controls[1].matrix,controls[1].matrix,0x5240);
        const std::array<std::int16_t,9> expected2 = {16381,-53,0,51,16379,0,0,0,16384};
        assert(product.matrix.values == expected2 && product.matrix.mode == 1);
        assert(product.operations.size() == 1);
        assert(product.operations[0].kind == NormalWorkKind::instructions && product.operations[0].count == 105);
    }
    {
        const auto product = fixedMatrixComposeOperations(controls[1].matrix,controls[4].matrix,0x5240);
        const std::array<std::int16_t,9> expected3 = {7785,-13361,5421,19,6176,15173,-14421,-7208,2947};
        assert(product.matrix.values == expected3 && product.matrix.mode == 3);
        assert(product.operations.size() == 1);
        assert(product.operations[0].kind == NormalWorkKind::instructions && product.operations[0].count == 141);
    }
    {
        const auto product = fixedMatrixComposeOperations(controls[4].matrix,controls[1].matrix,0x5240);
        const std::array<std::int16_t,9> expected4 = {7764,-13363,5448,15,6196,15165,-14435,-7187,2947};
        assert(product.matrix.values == expected4 && product.matrix.mode == 3);
        assert(product.operations.size() == 1);
        assert(product.operations[0].kind == NormalWorkKind::instructions && product.operations[0].count == 140);
    }
    {
        const auto product = fixedMatrixComposeOperations(controls[4].matrix,controls[2].matrix,0x5240);
        const std::array<std::int16_t,9> expected5 = {3937,-14936,-5448,-4381,4384,-15165,15287,5107,-2947};
        assert(product.matrix.values == expected5 && product.matrix.mode == 3);
        assert(product.operations.size() == 1);
        assert(product.operations[0].kind == NormalWorkKind::instructions && product.operations[0].count == 243);
    }
    const ViewTransform identity{{1,0,0},controls[0].matrix,{0,318,0,199},128};
    const auto center = failureFirstCenterOperations(identity,{0,{std::numeric_limits<std::int32_t>::min(),0,0}});
    assert(center.relativePosition.x == std::numeric_limits<std::int32_t>::max() && center.center == center.relativePosition);
    const auto first = failureFirstCenterOperations(identity,{0x88,{0,0,-400}});
    const auto next = failureNextCenterOperations(identity,{0x88,{0,0,-400}});
    assert(first.operations.front().count == 47 && next.operations.front().count == 39);
    rejects([&] { failureFirstCenterOperations(identity,{0x40,{}}); });
    auto invalid = controls[0].matrix;
    invalid.mode = 4;
    rejects([&] { fixedMatrixComposeOperations(invalid,controls[0].matrix,0x5240); });
    const WorldViewSetupInput view{{},{},{12345,54321,65535},0x88,7,{0,318,0,199},{256,-256,362}};
    rejects([&] { worldViewSetupOperations(view); });
    auto wrong = view;
    wrong.viewAngles[0] = 1;
    rejects([&] { failureViewSetupOperations(wrong); });
    std::vector<IndexedBitmap> frames(4,{256,16,std::vector<std::uint8_t>(256*16,7)});
    frames.push_back({32,27,std::vector<std::uint8_t>(32*27,11)});
    const HorizonWorkState horizon{0,199,0x4000,90,256,16,HorizonBackground::gradient,HorizonOverlay::enabled,233,246};
    const auto full = failureHorizonExecution(horizon,frames,{0,319,0,199});
    const auto retained = failureHorizonExecution(horizon,frames,{0,318,0,199});
    bool lastColumn = false;
    for (const auto& pixel : full.pixels) { lastColumn = lastColumn || pixel.point.x == 319; }
    assert(lastColumn);
    for (const auto& pixel : retained.pixels) { assert(pixel.point.x < 319); }
    rejects([&] { horizonExecution(horizon,frames); });
    rejects([&] { failureHorizonExecution(horizon,frames,{0,317,0,199}); });
    return 0;
}
