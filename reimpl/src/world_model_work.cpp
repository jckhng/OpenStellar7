#include "retained_failure_scene.hpp"
#include "stellar7/world_model_work.hpp"
#include "stellar7/level1_owned_view_work.hpp"
#include "level1_departure_model.hpp"
#include "stellar7/level1_bonus_render_work.hpp"
#include "stellar7/level1_arrival_render_work.hpp"
#include "stellar7/level1_deployment_render_work.hpp"
#include "stellar7/failure_model_work.hpp"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace stellar7 {
namespace {
using Operations = std::vector<NormalWorkOperation>;
using Kind = NormalWorkKind;
constexpr unsigned matrixFractionBits = 14;
constexpr std::uint64_t normalMissWrapper = 47;
constexpr std::uint64_t shadeMissWrapper = 40;
constexpr std::uint64_t identityTransform = 13;
constexpr std::uint64_t yawTransform = 47;
constexpr std::uint64_t fullTransform = 80;
constexpr std::uint64_t cachedNormal = 13;
constexpr std::uint64_t cachedShadeNormal = 16;
constexpr std::uint64_t identityFacing = 59;

std::int32_t flatShadeIndex(const std::uint8_t level,const std::int32_t direction) {
    constexpr std::int32_t directionsPerLevel=32,directionOrigin=16;
    return (static_cast<std::int32_t>(level)-1)*directionsPerLevel+direction+directionOrigin;
}

void instructions(Operations& operations, std::uint64_t count) {
    operations.push_back({Kind::instructions, count});
}

std::int16_t signed16(std::int64_t value) {
    const auto bits = static_cast<std::uint16_t>(value);
    return static_cast<std::int16_t>(bits <= 0x7fffU ? bits : static_cast<int>(bits) - 0x10000);
}

std::int32_t signed32(std::int64_t value) {
    const auto bits = static_cast<std::uint32_t>(value);
    return bits <= 0x7fffffffU ? static_cast<std::int32_t>(bits)
        : static_cast<std::int32_t>(static_cast<std::int64_t>(bits) - 0x100000000LL);
}

std::int64_t arithmeticShift(std::int32_t value, unsigned shift) {
    const auto denominator = std::int64_t{1} << shift;
    return value >= 0 ? value / denominator : -((-std::int64_t{value} + denominator - 1) / denominator);
}

std::array<std::int16_t, 3> transform(const std::array<std::int16_t, 3>& vertex, const FixedMatrixQ14& matrix) {
    if (matrix.mode == 0) {
        return vertex;
    }
    auto result = vertex;
    const auto axes = matrix.mode == 1 ? 2U : 3U;
    for (std::size_t axis = 0; axis < axes; ++axis) {
        const auto product = signed32(std::int64_t{vertex[0]} * matrix.values[axis]
            + std::int64_t{vertex[1]} * matrix.values[axis + 3]
            + (matrix.mode == 1 ? 0 : std::int64_t{vertex[2]} * matrix.values[axis + 6]));
        result[axis] = signed16(arithmeticShift(product, matrixFractionBits));
    }
    return result;
}

std::int32_t dot(const std::array<std::int16_t, 3>& first, const std::array<std::int16_t, 3>& second) {
    return signed32(std::int64_t{first[0]} * second[0] + std::int64_t{first[1]} * second[1]
        + std::int64_t{first[2]} * second[2]);
}

struct CachedVertex {
    std::uint8_t state = 0;
    std::array<std::int16_t, 3> coordinates{};
    ProjectedPoint projected{};
};


//4816 emits intersections in original vertex order. State3 is retained in the
//vertex cache;47C4 overwrites only the behind endpoint's projected coordinates.
std::vector<ProjectedPoint> nearPolygon(const std::vector<std::uint8_t>& indices,
    std::vector<CachedVertex>& cache, const WorldModelGeometryInput& input, Operations& operations) {
    if (indices.empty()) {
        throw std::invalid_argument("near polygon requires source indices");
    }
    const auto intersection = [&](CachedVertex& behind, const CachedVertex& front) {
        instructions(operations, 27); //47C4..4815, including RET.
        const auto denominator = signed16(front.coordinates[1] - behind.coordinates[1]);
        const auto distance = signed16(input.nearPlane - behind.coordinates[1]);
        if (denominator == 0) {
            throw std::invalid_argument("near intersection divides by zero");
        }
        const auto coordinate = [&](const std::size_t axis) {
            const auto difference = signed16(front.coordinates[axis] - behind.coordinates[axis]);
            const auto quotient = std::int32_t{difference} * distance / denominator;
            if (quotient < -32768 || quotient > 32767) {
                throw std::invalid_argument("near intersection divides outside signed word");
            }
            return signed16(behind.coordinates[axis] + quotient);
        };
        behind.projected = {signed16(input.screenCenter.x + coordinate(0)),
            signed16(input.screenCenter.y - coordinate(2))};
    };
    std::vector<ProjectedPoint> result;
    instructions(operations, 9); //4816..4828.
    auto previousIndex = indices.back();
    for (std::size_t ordinal = 0; ordinal < indices.size(); ++ordinal) {
        auto& previous = cache.at(previousIndex);
        auto& current = cache.at(indices[ordinal]);
        instructions(operations, 8); //482A..4842.
        if (current.state == 3) {
            instructions(operations, 3); //4844 JMP,48AC CMP,48AE JZ.
            if (previous.state != 3) {
                instructions(operations, 4); //48B0..48B6 includes CALL47C4.
                intersection(current, previous);
                instructions(operations, 9); //48B9..48D6.
                result.push_back(current.projected);
            }
        } else {
            instructions(operations, 2); //4847..4849.
            if (previous.state == 3) {
                instructions(operations, 3); //486D..4871 includes CALL47C4.
                intersection(previous, current);
                instructions(operations, 15); //4874..48A9.
                result.push_back(previous.projected);
                result.push_back(current.projected);
            } else {
                instructions(operations, 10); //484B..486A.
                result.push_back(current.projected);
            }
        }
        instructions(operations, 4); //48D8..48DF.
        if (ordinal + 1 != indices.size()) {
            instructions(operations, 1); //48E1 JMP482A.
        }
        previousIndex = indices[ordinal];
    }
    instructions(operations, 5); //48E4..48EC.
    return result;
}

void dotOperations(Operations& operations) {
    //4051: ordered LODSW instructions each carry one string transfer.
    instructions(operations, 5);
    operations.push_back({Kind::string, 1});
    instructions(operations, 4);
    operations.push_back({Kind::string, 1});
    instructions(operations, 4);
    operations.push_back({Kind::string, 1});
    instructions(operations, 7);
}
constexpr std::uint8_t filledUnculledUnshaded = 0x80;
constexpr std::uint16_t qualifiedVertexCount = 14;
constexpr std::size_t qualifiedGroupCount = 6;
constexpr std::uint16_t projectionBits = 7;

struct WideVertex {
    std::uint8_t state = 0;
    std::array<std::int32_t, 3> coordinates{};
    std::array<std::int32_t, 2> projected{};
};

// 5413's simple signed 64/32 wrapper calls5392. This exact domain never enters
// 5460's d389 compiler division child. A negative numerator negates64 bits and
// the denominator in542e..5446;5392 then restores the quotient sign.
std::int32_t divideProjection(std::int32_t coordinate, std::int32_t depth,
    Operations& operations) {
    const auto numerator = std::int64_t{coordinate} * (std::int64_t{1} << projectionBits);
    const auto magnitude = numerator < 0 ? -numerator : numerator;
    if (depth <= 0 || depth > 0xffff || magnitude >= (std::int64_t{1} << 23)) {
        throw std::invalid_argument("unsupported wide geometry division child");
    }
    instructions(operations, 12); //5413..542c
    if (numerator < 0) {
        instructions(operations, 11); //542e..5446
    }
    instructions(operations, 12); //5449..545d, includes5392 CALL
    instructions(operations, 10); //5392..53a6, nonnegative normalized numerator
    instructions(operations, 2); //53b2..53b4
    if (numerator < 0) {
        instructions(operations, 4); //53b6..53bd, normalized denominator negative
    }
    instructions(operations, 2); //53c0..53c2, denominator high word now0
    instructions(operations, 2); //53eb..53ed, signed low-word comparison
    const auto numeratorHighWord = static_cast<std::uint16_t>(magnitude >> 16U);
    if (signed16(depth) <= static_cast<int>(numeratorHighWord)) {
        instructions(operations, 6); //53f6..53ff, two DIVs in original order
    } else {
        instructions(operations, 3); //53ef..53f3, one DIV
    }
    instructions(operations, 3); //5401..5407
    if (numerator < 0) {
        instructions(operations, 3); //5409..540d
    }
    instructions(operations, 3); //5410..5412
    instructions(operations, 4); //54e4..54e7
    return static_cast<std::int32_t>(numerator / depth);
}

void wideIdentityVertex(WideVertex& cached, const std::array<std::int16_t, 3>& vertex,
    const FixedPosition& center, Operations& operations) {
    instructions(operations, 5); //4da2..4dad
    if (cached.state != 0) {
        instructions(operations, 3); //4daf..4db3: four-byte coordinate-cache stride
        return;
    }
    instructions(operations, 24); //4db4..4dec, includes4ba9 CALL
    cached.state = 1;
    instructions(operations, 3); //4ba9..4bae: identity matrix branch
    const std::array<std::int32_t, 3> fullCenter{center.x, center.y, center.z};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        instructions(operations, 8); //identity4ba9 axis: sign extension, shift0, stores/fraction0
        const auto translated = std::int64_t{vertex[axis]} + fullCenter[axis];
        if (translated < -0x80000000LL || translated > 0x7fffffffLL) {
            throw std::invalid_argument("unsupported wide geometry position overflow");
        }
        cached.coordinates[axis] = static_cast<std::int32_t>(translated);
    }
    instructions(operations, 2); //4bff JMP4da1 RET
    instructions(operations, 29); //4def..4e53: ordered translation/cache writes
}

void wideColors(const ModelShape& shape, const WorldModelGeometryInput& input,
    std::uint8_t& primary, std::uint8_t& secondary, Operations& operations) {
    instructions(operations, 9); //52bc..52d2: VGA primary colors
    instructions(operations, 8); //52e2..52f3: filled flag
    instructions(operations, 2); //52f5..52f7: no shading flag in this immutable LOD
    instructions(operations, 1); //531c
    instructions(operations, 14); //5372..5391: present remap
    primary = input.colorRemap[shape.colors[0]];
    secondary = input.colorRemap[shape.colors[1]];
}

// Adjacent caller passes source DS49f6 and its already-owned unshifted center.
// setup.translation remains halved solely for setup/group ordering. These are
// ephemeral domain inputs; this candidate adds no canonical memory or API visibility.
WorldModelGeometryWork wideGeometry(const ModelLod& lod, const WorldModelGeometryInput& input) {
    const auto& fullCenter = input.fullCenter;
    const auto half = [](std::int32_t value) {
        return value >= 0 ? value / 2 : -((-std::int64_t{value} + 1) / 2);
    };
    if (input.vertexPrecision != 1 || input.matrix.mode != 0 || input.projectionShift != projectionBits
        || input.nearPlane != 128 || input.screenCenter.x != 159 || input.screenCenter.y != 84
        || input.translation.x != half(fullCenter.x) || input.translation.y != half(fullCenter.y)
        || input.translation.z != half(fullCenter.z) || input.groupOrder.size() != qualifiedGroupCount
        || lod.groups.size() != qualifiedGroupCount || lod.threshold != 8 || lod.flags != 128
        || input.clipping != PolygonClipMode::Disabled) {
        throw std::invalid_argument("unsupported precision1 world geometry profile");
    }
    WorldModelGeometryWork result;
    Operations pending;
    instructions(pending, 3); //cec1..cec9
    instructions(pending, 3); //cece..ced4
    std::size_t vertexBlock = 0;
    std::vector<WideVertex> cache;
    for (std::size_t ordinal = 0; ordinal < input.groupOrder.size(); ++ordinal) {
        const auto groupIndex = input.groupOrder[ordinal];
        if (groupIndex >= lod.groups.size()) {
            throw std::invalid_argument("wide geometry group order leaves LOD");
        }
        const auto& group = lod.groups[groupIndex];
        if (group.vertexBlock == 0 || group.vertices.size() != qualifiedVertexCount
            || group.primitives.empty() || group.primitives.front().kind != 0
            || group.primitives.front().shapes.size() != 1) {
            throw std::invalid_argument("unsupported wide geometry immutable group");
        }
        const auto& shape = group.primitives.front().shapes.front();
        if (shape.flags != filledUnculledUnshaded || shape.normalVertex != 255
            || shape.vertexIndices.size() != 2) {
            // 51b3/5116/4eba facing,522e normal/shading aliases and near/viewport
            // clipping have distinct programs. They receive no new admission here.
            throw std::invalid_argument("unsupported wide geometry facing/shading primitive");
        }
        instructions(pending, 6); //ced6..cee7
        if (vertexBlock != group.vertexBlock) {
            vertexBlock = group.vertexBlock;
            cache.assign(group.vertices.size(), {});
            instructions(pending, 11); //cee9..cf09 includes resetCALLF
            instructions(pending, 8); //48ed..48fd
            pending.push_back({NormalWorkKind::repeatString, group.vertices.size() / 2});
            instructions(pending, 1); //4901
            pending.push_back({NormalWorkKind::repeatString, group.vertices.size() % 2});
            instructions(pending, 2); //4905..4906
        }
        instructions(pending, 3); //cf0e..cf10,cf30 animation null
        instructions(pending, 19); //cf35..cf64 first variant
        instructions(pending, 6); //cf72..cf79 includes492d CALLF
        instructions(pending, 4); //492d..4934 includes precision1 JMPF55eb
        instructions(pending, 11); //55eb..5602
        instructions(pending, 2); //5606..560a shape loop
        instructions(pending, 9); //560f..5622, filled/mode0
        instructions(pending, 2); //5640..5641
        std::uint8_t primary, secondary;
        wideColors(shape, input, primary, secondary, pending);
        instructions(pending, 4); //5644..564d
        for (const auto index : shape.vertexIndices) {
            if (index >= cache.size()) {
                throw std::invalid_argument("wide geometry vertex leaves immutable block");
            }
            instructions(pending, 4); //5650..5657
            instructions(pending, 3); //565c..565e includes4da2 CALL
            auto& cached = cache[index];
            wideIdentityVertex(cached, group.vertices[index], fullCenter, pending);
            instructions(pending, 4); //5661..5669
            if (cached.state == 2) {
                instructions(pending, 1); //566b JMP
            } else {
                if (cached.coordinates[1] < input.nearPlane || cached.coordinates[1] > 0xffff) {
                    throw std::invalid_argument("unsupported wide geometry near branch");
                }
                instructions(pending, 7); //566e..5683: Y high0/unsigned near-safe
                cached.state = 2; //5692..5694 precedes the two projection divisions
                instructions(pending, 15); //5692..56bd
                instructions(pending, 4 * projectionBits); //56ce..56d4
                instructions(pending, 7); //56d6..56dd includes5413 CALL
                cached.projected[0] = divideProjection(cached.coordinates[0], cached.coordinates[1], pending);
                instructions(pending, 3); //56e0..56e8
                instructions(pending, 12); //56ed..570f
                instructions(pending, 4 * projectionBits); //5720..5726
                instructions(pending, 7); //5728..572f includes5413 CALL
                cached.projected[1] = divideProjection(cached.coordinates[2], cached.coordinates[1], pending);
                instructions(pending, 3); //5732..573a
            }
            instructions(pending, 9); //573f..5752, original index append/order
        }
        instructions(pending, 5); //5650..5659 terminator
        instructions(pending, 3); //5755..575a no near clipping
        instructions(pending, 4); //576f..5776 nonempty, includes5595 CALL
        instructions(pending, 7); //5595..55a2
        std::vector<ProjectedPoint> points;
        for (const auto index : shape.vertexIndices) {
            instructions(pending, 21); //55a4..55e7: screen add/sub, four-byte stride
            const auto& projected = cache[index].projected;
            const auto x = std::int64_t{input.screenCenter.x} + projected[0];
            const auto y = std::int64_t{input.screenCenter.y} - projected[1];
            if (x < 24 || x > 294 || y < 24 || y > 145) {
                throw std::invalid_argument("unsupported wide geometry viewport clipping child");
            }
            points.push_back({signed16(x), signed16(y)});
        }
        instructions(pending, 2); //55e9..55ea
        instructions(pending, 3); //5779..577e clipped0
        instructions(pending, 5); //578f..5798
        instructions(pending, 7 * points.size()); //579a..57b2 low words -> source DS207c/20a4
        instructions(pending, 7); //57b4..57c2 rasterCALLF excluded
        result.polygons.push_back({groupIndex, 0, std::move(points), primary, secondary,
                                   true, std::move(pending)});
        pending = {};
        instructions(pending, 4); //57cb..57d2 after raster RETF
        instructions(pending, 7); //5606..560c terminal +57d5..57d8 far epilogue
        instructions(pending, 2); //cf7e,cfa7
        instructions(pending, ordinal + 1 == input.groupOrder.size() ? 3 : 4); //cfaa..cfb2
    }
    instructions(pending, 5); //cfb5..cfba cb7a far epilogue
    result.suffix = std::move(pending);
    return result;
}

//55EB's precision1 sibling retains full coordinates; normal/shade cache values
//remain signed words. These entries share the source state byte and block clear.
void arrivalWideVertex(WideVertex& cached, const std::array<std::int16_t,3>& vertex,
    const WorldModelGeometryInput& input, Operations& operations) {
    if (input.matrix.mode == 0) {
        wideIdentityVertex(cached,vertex,input.fullCenter,operations);
        return;
    }
    instructions(operations,5); //4DA2..4DAD.
    if (cached.state != 0) { instructions(operations,3); return; }
    instructions(operations,24); //4DB4..4DEC, including4BA9 CALL.
    instructions(operations,input.matrix.mode == 1 ? 67 : 103); //4BA9, shift0, yaw/full branches.
    const auto transformed = transform(vertex,input.matrix);
    const std::array<std::int32_t,3> center{input.fullCenter.x,input.fullCenter.y,input.fullCenter.z};
    for (std::size_t axis=0;axis<3;++axis) {
        cached.coordinates[axis]=signed32(std::int64_t{transformed[axis]}+center[axis]);
    }
    cached.state=1;
    instructions(operations,29); //4DEF..4E53: fraction/carry and full cache stores.
}

std::int64_t arrivalWideDot(const std::array<std::int32_t,3>& point,
    const std::array<std::int16_t,3>& normal,Operations& operations) {
    //4EBA: first two signed32*16 products; third is skipped when normalZ==0.
    instructions(operations,48 + (normal[2] == 0 ? 0 : 25));
    std::int64_t result=0;
    const auto axes=normal[2] == 0 ? 2U : 3U;
    for (std::size_t axis=0;axis<axes;++axis) {
        if (point[axis]<0) { instructions(operations,4); }
        if (normal[axis]<0) { instructions(operations,2); }
        if ((point[axis]<0)!=(normal[axis]<0)) { instructions(operations,6); }
        result+=std::int64_t{point[axis]}*normal[axis];
    }
    return result;
}

using ArrivalPoint = std::array<std::int32_t,2>;

std::int16_t arrivalIntersectionQuotient(const std::int32_t difference,
    const std::int32_t distance,const std::int32_t denominator) {
    const auto divisor=signed16(denominator);
    if (divisor==0) { throw std::invalid_argument("arrival clipping divides by zero"); }
    const auto quotient=std::int32_t{signed16(difference)}*signed16(distance)/divisor;
    if (quotient < -32768 || quotient>32767) {
        throw std::invalid_argument("arrival clipping signed division overflow");
    }
    return static_cast<std::int16_t>(quotient);
}

std::vector<ArrivalPoint> arrivalWideClip(std::vector<ArrivalPoint> points,Operations& operations,const Viewport viewport) {
    instructions(operations,5); //6197..61A0.
    if (points.size()<=1) { instructions(operations,3);return points; }
    const auto classify=[&](const std::int32_t value,const int lower,const int upper) {
        instructions(operations,4);
        if (value>=65536) { instructions(operations,4);return 2; }
        if (value<0) { instructions(operations,4);return 1; }
        instructions(operations,8+(value<lower || value>upper ? 1 : 0));
        return value<lower ? 1 : value>upper ? 2 : 0;
    };
    for (std::size_t axis=0;axis<2;++axis) {
        const auto lower=axis==0 ? viewport.left : viewport.top;
        const auto upper=axis==0 ? viewport.right : viewport.bottom;
        instructions(operations,5); //Last index/stride and clear code.
        auto previous=points.back();
        auto previousCode=classify(previous[axis],lower,upper);
        instructions(operations,axis==0 ? 1 : 2);
        std::vector<ArrivalPoint> result;
        const auto intersect=[&](const ArrivalPoint outside,const ArrivalPoint other,const int code) {
            //5FF5/60C6: first side30I, second side31I, including signed IMUL/IDIV.
            instructions(operations,code==1 ? 30 : 31);
            const auto boundary=code==1 ? lower : upper;
            auto point=outside;point[axis]=boundary;
            point[1-axis]=signed32(std::int64_t{outside[1-axis]}+
                arrivalIntersectionQuotient(other[1-axis]-outside[1-axis],
                    boundary-outside[axis],other[axis]-outside[axis]));
            result.push_back(point);
        };
        for (std::size_t index=0;index<points.size();++index) {
            const auto current=points[index];
            instructions(operations,1);
            const auto currentCode=classify(current[axis],lower,upper);
            instructions(operations,3);
            if ((previousCode|currentCode)==0) {
                instructions(operations,10);result.push_back(current);
            } else if ((previousCode&currentCode)!=0) {
                instructions(operations,4);
            } else if (previousCode==0) {
                instructions(operations,11);intersect(current,previous,currentCode);
            } else if (currentCode==0) {
                instructions(operations,18);intersect(previous,current,previousCode);result.push_back(current);
            } else {
                instructions(operations,14);intersect(previous,current,previousCode);intersect(current,previous,currentCode);
            }
            instructions(operations,8+(index+1<points.size() ? 1 : 0));
            previous=current;previousCode=currentCode;
        }
        points=std::move(result);
        if (axis==0) {
            instructions(operations,6);
            if (points.size()<=1) {
                instructions(operations,2+10); //63BF/63DC CALL4907.
                operations.push_back({Kind::repeatString,2*points.size()});
                instructions(operations,3);
                operations.push_back({Kind::repeatString,2*points.size()});
                instructions(operations,3+2);return points;
            }
        }
    }
    instructions(operations,7);return points;
}

std::vector<ArrivalPoint> arrivalWideNear(const std::vector<std::uint8_t>& indices,
    std::vector<WideVertex>& cache,const WorldModelGeometryInput& input,Operations& operations) {
    if (indices.empty()) { throw std::invalid_argument("arrival near polygon is empty"); }
    instructions(operations,9); //6474..6486.
    std::vector<ArrivalPoint> points;
    auto previous=indices.back();
    const auto screen=[&](const WideVertex& value) {
        return ArrivalPoint{signed32(std::int64_t{input.screenCenter.x}+value.projected[0]),
            signed32(std::int64_t{input.screenCenter.y}-value.projected[1])};
    };
    const auto intersect=[&](WideVertex& behind,const WideVertex& front) {
        instructions(operations,40); //63E1..6473; radius<3FAC selects signed-word delta helper.
        const auto denominator=front.coordinates[1]-behind.coordinates[1];
        const auto distance=input.nearPlane-behind.coordinates[1];
        behind.projected[0]=signed32(std::int64_t{behind.coordinates[0]}+
            arrivalIntersectionQuotient(front.coordinates[0]-behind.coordinates[0],distance,denominator));
        behind.projected[1]=signed32(std::int64_t{behind.coordinates[2]}+
            arrivalIntersectionQuotient(front.coordinates[2]-behind.coordinates[2],distance,denominator));
    };
    for (std::size_t ordinal=0;ordinal<indices.size();++ordinal) {
        auto& before=cache.at(previous);auto& current=cache.at(indices[ordinal]);
        instructions(operations,8);
        if (current.state==3) {
            instructions(operations,1+2);
            if (before.state!=3) {
                instructions(operations,22);intersect(current,before);points.push_back(screen(current));
            }
        } else {
            instructions(operations,2);
            if (before.state==3) {
                instructions(operations,5+29);intersect(before,current);
                points.push_back(screen(before));points.push_back(screen(current));
            } else {
                instructions(operations,18);points.push_back(screen(current));
            }
        }
        instructions(operations,4+(ordinal+1<indices.size() ? 1 : 0));previous=indices[ordinal];
    }
    instructions(operations,6);return points;
}

WorldModelGeometryWork arrivalWideGeometry(const ModelLod& lod,const WorldModelGeometryInput& input,
    const std::optional<FixedPosition>& rawActor,const Viewport viewport={24,294,24,145}) {
    WorldModelGeometryWork result;
    Operations pending;
    instructions(pending,6); //CEC1..CED4, nonempty groups.
    std::size_t block=0;
    std::vector<WideVertex> cache;
    const auto transformWork=input.matrix.mode==0 ? identityTransform
        : input.matrix.mode==1 ? yawTransform : fullTransform;
    for (std::size_t ordinal=0;ordinal<input.groupOrder.size();++ordinal) {
        const auto groupIndex=input.groupOrder[ordinal];
        const auto& group=lod.groups.at(groupIndex);
        instructions(pending,6);
        if (block!=group.vertexBlock) {
            block=group.vertexBlock;cache.assign(group.vertices.size(),{});
            instructions(pending,19);
            pending.push_back({Kind::repeatString,group.vertices.size()/2});
            instructions(pending,1);
            pending.push_back({Kind::repeatString,group.vertices.size()%2});
            instructions(pending,2);
        }
        std::size_t selected=0;
        if (input.animationSelectors.empty()) { instructions(pending,3); } //CF0E/CF10/CF30: DI0.
        else if (group.animationOffset==-1) { instructions(pending,6); }
        else {
            if (group.animationOffset<0 || static_cast<std::size_t>(group.animationOffset)>=input.animationSelectors.size()
                || (input.matrixProfile!=WorldModelMatrixProfile::level1Deployment && group.animationOffset!=0)) {
                throw std::invalid_argument("arrival selector offset");
            }
            selected=input.animationSelectors.at(group.animationOffset);instructions(pending,12);
            if (selected>=group.primitives.size()) { selected=0;instructions(pending,1); }
        }
        instructions(pending,19+6+4+11); //CF35..CF79;492D precision1;55EB..5602.
        const auto& primitive=group.primitives.at(selected);
        if (primitive.kind!=0) { throw std::invalid_argument("arrival wide primitive kind"); }
        for (std::size_t shapeIndex=0;shapeIndex<primitive.shapes.size();++shapeIndex) {
            const auto& shape=primitive.shapes[shapeIndex];
            const auto filled=(shape.flags&0x80U)!=0;
            instructions(pending,2); //5606/560A.
            instructions(pending,filled ? 9 : 8); //560F..5622.
            const auto cullMode=shape.flags&3U;
            if (cullMode==2) { instructions(pending,4);continue; }
            const auto normal=[&](const std::size_t index,const bool shading) {
                auto& value=cache.at(index);
                if (value.state==0) {
                    instructions(pending,(shading ? 41U : 36U)+transformWork);
                    const auto transformed=transform(group.vertices.at(index),input.matrix);
                    for (std::size_t axis=0;axis<3;++axis) { value.coordinates[axis]=transformed[axis]; }
                    value.state=1;
                } else { instructions(pending,shading ? 17 : 8); }
                return std::array<std::int16_t,3>{signed16(value.coordinates[0]),
                    signed16(value.coordinates[1]),signed16(value.coordinates[2])};
            };
            if (cullMode!=0) {
                instructions(pending,2+4); //5624/5626;5628..5633.
                if (shape.vertexIndices.empty()) { throw std::invalid_argument("arrival facing vertex"); }
                std::array<std::int32_t,3> point;
                std::array<std::int16_t,3> face;
                if (rawActor || input.matrix.mode==0) {
                    instructions(pending,58); //51B3..51BE +5116, no scale shifts.
                    const auto raw=rawActor.value_or(input.fullCenter);
                    const std::array<std::int32_t,3> center{raw.x,raw.y,raw.z};
                    const auto& vertex=group.vertices.at(shape.vertexIndices.front());
                    for (std::size_t axis=0;axis<3;++axis) { point[axis]=signed32(std::int64_t{vertex[axis]}+center[axis]); }
                    face=group.vertices.at(shape.normalVertex);
                } else {
                    instructions(pending,41); //51B3..522D, excluding nested normal/vertex/dot.
                    face=normal(shape.normalVertex,false);
                    auto& value=cache.at(shape.vertexIndices.front());
                    arrivalWideVertex(value,group.vertices.at(shape.vertexIndices.front()),input,pending);
                    point=value.coordinates;
                }
                const auto facing=arrivalWideDot(point,face,pending);
                instructions(pending,2);
                if (facing>=0) { instructions(pending,2);continue; }
            }
            instructions(pending,2); //5640 PUSH/CALL52BC.
            auto primary=shape.colors[0],secondary=shape.colors[1];
            instructions(pending,9);instructions(pending,filled ? 8 : 7);
            if (filled) { instructions(pending,2); }
            if (filled && (shape.flags&0x20U)!=0) {
                instructions(pending,13);
                const auto level=input.shadeLevels[secondary];
                if (level!=0) { instructions(pending,2); }
                if (level>0 && level<=8) {
                    instructions(pending,4);
                    const auto value=normal(shape.normalVertex,true);
                    instructions(pending,5);dotOperations(pending);
                    auto direction=static_cast<std::int32_t>(arithmeticShift(dot(input.light,value),16));
                    instructions(pending,4);
                    if (direction==16) { direction=15;instructions(pending,1); }
                    const auto shadeIndex=flatShadeIndex(level,direction);
                    if (shadeIndex<0 || shadeIndex>=static_cast<std::int32_t>(input.lightingAdjustments.size())) { throw std::invalid_argument("arrival wide shade direction"); }
                    instructions(pending,16);
                    const auto adjusted=static_cast<std::uint8_t>(secondary+
                        input.lightingAdjustments[static_cast<std::size_t>(shadeIndex)]);
                    if (primary==secondary) { primary=adjusted; } else { instructions(pending,1); }
                    secondary=adjusted;instructions(pending,1);
                } else { instructions(pending,1); }
            } else { instructions(pending,1); }
            instructions(pending,14+4); //5372..5391,5644..564D.
            primary=input.colorRemap[primary];secondary=input.colorRemap[secondary];
            std::vector<ArrivalPoint> points;
            bool hasNear=false;
            for (const auto index:shape.vertexIndices) {
                instructions(pending,4+3);
                auto& value=cache.at(index);
                arrivalWideVertex(value,group.vertices.at(index),input,pending);
                instructions(pending,4);
                if (value.state==2) { instructions(pending,1); }
                else {
                    if (value.coordinates[1]<128) {
                        instructions(pending,(value.coordinates[1]<0 ? 4 : 7)+4);
                        value.state=3;hasNear=true;
                    } else {
                    if (value.coordinates[1]>0xffff) {
                        throw std::invalid_argument("unsupported arrival precision1 wide-divisor branch");
                    }
                    instructions(pending,7);value.state=2;
                    instructions(pending,15+4*projectionBits+7);
                    value.projected[0]=divideProjection(value.coordinates[0],value.coordinates[1],pending);
                    instructions(pending,3+12+4*projectionBits+7);
                    value.projected[1]=divideProjection(value.coordinates[2],value.coordinates[1],pending);
                    instructions(pending,3);
                    }
                }
                instructions(pending,9);
                points.push_back({signed32(std::int64_t{input.screenCenter.x}+value.projected[0]),
                    signed32(std::int64_t{input.screenCenter.y}-value.projected[1])});
            }
            instructions(pending,5+3); //Terminator and near flag.
            if (hasNear) {
                instructions(pending,5); //575C..5766:4BFE short-intersection selection.
                points=arrivalWideNear(shape.vertexIndices,cache,input,pending);
            } else {
                instructions(pending,4+7+21*points.size()+2); //5595 conversion.
            }
            instructions(pending,3);
            if (input.clipping==PolygonClipMode::Enabled) {
                instructions(pending,4); //5780..5787:4BFE selects6197.
                points=arrivalWideClip(std::move(points),pending,viewport);
                instructions(pending,1); //578A JMP follows the child, including its REP transfers.
            }
            instructions(pending,5+7*points.size()+7);
            std::vector<ProjectedPoint> projected;
            for (const auto& point:points) { projected.push_back({signed16(point[0]),signed16(point[1])}); }
            result.polygons.push_back({groupIndex,shapeIndex,std::move(projected),primary,secondary,
                filled,std::move(pending)});pending={};instructions(pending,4);
        }
        instructions(pending,7+2+(ordinal+1==input.groupOrder.size() ? 3 : 4));
    }
    instructions(pending,5);result.suffix=std::move(pending);return result;
}


}

enum class GeometryExecutionDomain { legacy, ownedSharedPrecision0 };

static WorldModelGeometryWork modelGeometryOperations(const ModelLod& lod,
    const WorldModelGeometryInput& input, const bool failure,
    const std::optional<FixedPosition>& untransformedActorPosition,
    const GeometryExecutionDomain domain = GeometryExecutionDomain::legacy) {
    const auto ownedShared = domain == GeometryExecutionDomain::ownedSharedPrecision0;
    const auto level2=input.matrixProfile==WorldModelMatrixProfile::level2Constructor;
    const auto banked = input.matrixProfile == WorldModelMatrixProfile::level1Banked;
    const auto inlineSpin = input.matrixProfile == WorldModelMatrixProfile::level1InlineSpin;
    const auto arrival = input.matrixProfile == WorldModelMatrixProfile::level1Arrival
        || input.matrixProfile == WorldModelMatrixProfile::level1Deployment;
    if (!failure && ((input.matrixProfile != WorldModelMatrixProfile::identityOrYaw && !banked && !inlineSpin && !arrival && !level2)
        || (banked && (input.vertexPrecision != 0 || input.matrix.mode < 2 || input.matrix.mode > 3
            || input.animationSelectors.size() != 1))
        || (inlineSpin && (input.vertexPrecision != 0 || input.matrix.mode > 3 || input.animationSelectors.size() != 1)))) {
        throw std::invalid_argument("unsupported banked world geometry profile");
    }
    if (failure && (input.vertexPrecision != 0 || input.matrix.mode > 3
        || input.screenCenter.x != 159 || input.screenCenter.y != 99)) {
        throw std::invalid_argument("unsupported failure geometry arithmetic profile");
    }
    if (input.vertexPrecision != 0) {
        if (arrival || level2) { return arrivalWideGeometry(lod,input,untransformedActorPosition); }
        if (!input.animationSelectors.empty()) {
            throw std::invalid_argument("wide geometry animation selector is not admitted");
        }
        return wideGeometry(lod, input);
    }
    if (((!failure && !banked && !inlineSpin && !arrival) && input.matrix.mode > 1) || input.projectionShift != 7 || input.nearPlane != 128) {
        throw std::runtime_error("unsupported world model geometry profile");
    }
    const std::array<std::int16_t, 3> translation{signed16(input.translation.x),
        signed16(input.translation.y), signed16(input.translation.z)};
    WorldModelGeometryWork result;
    Operations pending;
    instructions(pending, 3); //cec1..cec9
    if (input.groupOrder.empty()) {
        instructions(pending, 6); //cecb JMP andcfb5..cfba
        result.suffix = std::move(pending);
        return result;
    }
    instructions(pending, 3); //cece..ced4
    std::size_t vertexBlock = 0;
    std::vector<CachedVertex> cache;
    const auto transformWork = input.matrix.mode == 0 ? identityTransform
        : input.matrix.mode == 1 ? yawTransform : fullTransform;
    for (std::size_t ordinal = 0; ordinal < input.groupOrder.size(); ++ordinal) {
        const auto groupIndex = input.groupOrder[ordinal];
        if (groupIndex >= lod.groups.size()) {
            throw std::runtime_error("world group order leaves LOD");
        }
        const auto& group = lod.groups[groupIndex];
        if (group.vertexBlock == 0 || group.vertices.empty() || group.vertices.size() > 255
            || group.primitives.empty() || group.primitives.front().kind != 0) {
            throw std::runtime_error("unsupported world primitive or vertex block");
        }
        instructions(pending, 6); //ced6..cee7
        if (vertexBlock != group.vertexBlock) {
            vertexBlock = group.vertexBlock;
            cache.assign(group.vertices.size(), {});
            instructions(pending, 11); //cee9..cf09, includes resetCALLF
            instructions(pending, 8); //48ed..48fd
            pending.push_back({Kind::repeatString, group.vertices.size() / 2});
            instructions(pending, 1); //4901
            pending.push_back({Kind::repeatString, group.vertices.size() % 2});
            instructions(pending, 2); //4905..4906
        } else if (cache.size() != group.vertices.size()) {
            throw std::runtime_error("aliased world vertex block changes length");
        }
        std::size_t selectedVariant = 0;
        if (input.animationSelectors.empty()) {
            instructions(pending, 3); //cf0e..cf10,cf30: null animation pointer.
        } else if (group.animationOffset == -1) {
            instructions(pending, 6); //cf0e..cf1a,cf30: group has no selector.
        } else {
            if (group.animationOffset < 0
                || static_cast<std::size_t>(group.animationOffset) >= input.animationSelectors.size()) {
                throw std::invalid_argument("world group selector leaves owned animation bytes");
            }
            selectedVariant = input.animationSelectors[group.animationOffset];
            instructions(pending, 12); //cf0e..cf2e: owned selector byte and variant bound.
            if (selectedVariant >= group.primitives.size()) {
                selectedVariant = 0;
                instructions(pending, 1); //cf30 fallback to first variant.
            }
        }
        instructions(pending, 19); //cf35..cf64: first primitive variant
        instructions(pending, 6); //cf72..cf79, includes492d CALLF
        instructions(pending, 3); //492d..4932: default precision
        instructions(pending, 11); //4939..4950
        const auto& primitive = group.primitives[selectedVariant];
        if (primitive.kind != 0) {
            throw std::invalid_argument("unsupported selected world primitive kind");
        }
        for (std::size_t shapeIndex = 0; shapeIndex < primitive.shapes.size(); ++shapeIndex) {
            const auto& shape = primitive.shapes[shapeIndex];
            const bool filled = (shape.flags & 0x80U) != 0;
            instructions(pending, 2); //4954..4958
            instructions(pending, filled ? 9 : 8); //495d..4970
            const auto cullMode = shape.flags & 3U;
            if (cullMode == 2) {
                instructions(pending, 4); //4972..4974,4989..498c
                continue;
            }
            const auto obtainNormal = [&](std::size_t index, bool shading) {
                if (index >= cache.size()) {
                    throw std::runtime_error("world normal leaves vertex block");
                }
                auto& cached = cache[index];
                if (cached.state == 0) {
                    instructions(pending, (shading ? shadeMissWrapper : normalMissWrapper) + transformWork);
                    cached.coordinates = transform(group.vertices[index], input.matrix);
                    cached.state = 1;
                } else {
                    instructions(pending, shading ? cachedShadeNormal : cachedNormal);
                }
                return cached.coordinates;
            };
            const auto obtainVertex = [&](std::size_t index) -> CachedVertex& {
                if (index >= cache.size()) {
                    throw std::runtime_error("world vertex leaves block");
                }
                auto& cached = cache[index];
                instructions(pending, vertexCacheDispatches(cached.state == 0
                    ? VertexCacheState::Empty : VertexCacheState::Populated, input.matrix.mode));
                if (cached.state == 0) {
                    cached.coordinates = transform(group.vertices[index], input.matrix);
                    for (std::size_t axis = 0; axis < 3; ++axis) {
                        cached.coordinates[axis] = signed16(cached.coordinates[axis] + translation[axis]);
                    }
                    cached.state = 1;
                }
                return cached;
            };
            if (cullMode != 0) {
                instructions(pending, 2); //4972..4974
                if (shape.vertexIndices.empty()) {
                    throw std::runtime_error("world culling needs first vertex");
                }
                instructions(pending, 4); //4976..4981
                std::int32_t facing;
                if (input.matrix.mode == 0 || ((failure || inlineSpin || arrival || level2 || ownedShared) && untransformedActorPosition)) {
                    instructions(pending, 7 + identityFacing); //5985 prefix +5906 direct tail
                    if (shape.normalVertex >= group.vertices.size() || shape.vertexIndices.front() >= group.vertices.size()) {
                        throw std::runtime_error("world culling vertex leaves block");
                    }
                    auto first = group.vertices[shape.vertexIndices.front()];
                    for (std::size_t axis = 0; axis < 3; ++axis) {
                        const auto directPosition = untransformedActorPosition.value_or(input.translation);
                        const std::array<std::int32_t, 3> direct{directPosition.x, directPosition.y, directPosition.z};
                        first[axis] = signed16(first[axis] + direct[axis]);
                    }
                    facing = dot(group.vertices[shape.normalVertex], first);
                } else {
                    instructions(pending, 9); //5985..5996
                    const auto normal = obtainNormal(shape.normalVertex, false);
                    instructions(pending, 5); //5999..599f
                    const auto first = obtainVertex(shape.vertexIndices.front()).coordinates;
                    instructions(pending, 22); //59a2..59db
                    facing = dot(normal, first);
                }
                instructions(pending, 2); //4984..4987
                if (facing >= 0) {
                    instructions(pending, 2); //4989..498c
                    continue;
                }
            }
            instructions(pending, 2); //498e..498f
            auto primary = shape.colors[0];
            auto secondary = shape.colors[1];
            instructions(pending, 9); //52bc..52d2, primary profile
            instructions(pending, filled ? 8 : 7); //52e2..52f3
            if (filled) {
                instructions(pending, 2); //52f5..52f7
            }
            if (filled && (shape.flags & 0x20U) != 0) {
                instructions(pending, 6); //52f9..5305
                instructions(pending, 7); //5307..5314
                const auto level = input.shadeLevels[secondary];
                if (level != 0) {
                    instructions(pending, 2); //5316..5318
                }
                if (level > 0 && level <= 8) {
                    instructions(pending, 1); //531a
                    instructions(pending, 3); //531f..5326
                    const auto normal = obtainNormal(shape.normalVertex, true);
                    instructions(pending, 5); //5329..5331
                    dotOperations(pending);
                    auto direction = static_cast<std::int32_t>(arithmeticShift(dot(input.light, normal), 16));
                    instructions(pending, 4); //5336..533e
                    if (direction == 16) {
                        direction = 15;
                        instructions(pending, 1); //5340
                    }
                    const auto shadeIndex=flatShadeIndex(level,direction);
                    if (shadeIndex<0 || shadeIndex>=static_cast<std::int32_t>(input.lightingAdjustments.size())) {
                        throw std::runtime_error("world light direction leaves lookup");
                    }
                    instructions(pending, 16); //5343..536a
                    const auto adjusted = static_cast<std::uint8_t>(secondary
                        + input.lightingAdjustments[static_cast<std::size_t>(shadeIndex)]);
                    if (primary == secondary) {
                        primary = adjusted;
                    } else {
                        instructions(pending, 1); //536c
                    }
                    secondary = adjusted;
                    instructions(pending, 1); //536f
                } else {
                    instructions(pending, 1); //531c
                }
            } else {
                instructions(pending, 1); //531c
            }
            instructions(pending, 14); //5372..5391, present remap
            primary = input.colorRemap[primary];
            secondary = input.colorRemap[secondary];
            instructions(pending, 4); //4992..499b
            std::vector<ProjectedPoint> points;
            bool hasNear = false;
            for (const auto index : shape.vertexIndices) {
                instructions(pending, 4); //499e..49a5
                instructions(pending, 2); //49aa..49ab
                auto& vertex = obtainVertex(index);
                instructions(pending, 4); //49ae..49b7
                if (vertex.state == 2) {
                    instructions(pending, 1); //49b9
                } else {
                    instructions(pending, 6); //49bc..49ca
                    if (vertex.coordinates[1] < input.nearPlane) {
                        if (!failure && !arrival && !level2 && !ownedShared) {
                            throw std::runtime_error("unsupported world near-clipped polygon");
                        }
                        vertex.state = 3;
                        hasNear = true;
                        instructions(pending, 5); //49CC..49D8: cached behind state, global near flag.
                    } else {
                    instructions(pending, 7); //49db..49ef
                    instructions(pending, 3 * input.projectionShift); //49fc..4a00
                    instructions(pending, 2); //4a02..4a04
                    instructions(pending, 5); //4a09..4a16
                    instructions(pending, 3 * input.projectionShift); //4a23..4a27
                    instructions(pending, 5); //4a29..4a34
                    const auto project = [&](std::int16_t coordinate) {
                        const auto quotient = (static_cast<std::int32_t>(coordinate) * (1 << input.projectionShift))
                            / vertex.coordinates[1];
                        if (quotient < -32768 || quotient > 32767) {
                            throw std::runtime_error("world projection divide overflow");
                        }
                        return static_cast<std::int16_t>(quotient);
                    };
                    vertex.projected = {signed16(input.screenCenter.x + project(vertex.coordinates[0])),
                        signed16(input.screenCenter.y - project(vertex.coordinates[2]))};
                    vertex.state = 2;
                    }
                }
                instructions(pending, 10); //4a39..4a4d
                points.push_back(vertex.projected);
            }
            instructions(pending, 5); //499e..49a7 terminator
            instructions(pending, 3); //4a50..4a55
            if (hasNear) {
                instructions(pending, 1); //4A57 CALL4816.
                points = nearPolygon(shape.vertexIndices, cache, input, pending);
                instructions(pending, 1); //4A5A JMP4A9A.
            } else {
                if (points.empty()) {
                    throw std::runtime_error("unsupported empty world polygon");
                }
                instructions(pending, 9); //4a5d..4a70
                instructions(pending, 14 * points.size()); //4a72..4a98
            }
            instructions(pending, 7); //4a9a..4aa8, rasterCALLF excluded
            result.polygons.push_back({groupIndex, shapeIndex, std::move(points), primary, secondary,
                filled, std::move(pending)});
            pending = {};
            instructions(pending, 4); //4ab1..4ab8 after raster RETF
        }
        instructions(pending, 7); //4954..495a terminal and4abb..4abe
        instructions(pending, 2); //cf7e JMP,cfa7 stack cleanup
        instructions(pending, ordinal + 1 == input.groupOrder.size() ? 3 : 4); //cfaa..cfb2
    }
    instructions(pending, 5); //cfb5..cfba
    result.suffix = std::move(pending);
    return result;
}

WorldModelGeometryWork worldModelGeometryOperations(const ModelLod& lod, const WorldModelGeometryInput& input) {
    return modelGeometryOperations(lod, input, false, std::nullopt);
}

WorldModelGeometryWork level1BonusModelGeometryOperations(const ModelLod& lod,
    const WorldModelGeometryInput& input, const std::array<std::uint16_t,3> actorAngles,
    const WorldModelViewInput& view) {
    const auto actorMatrix = makeFixedMatrixQ14(actorAngles[0],actorAngles[1],actorAngles[2]);
    const auto combined = composeFixedMatricesQ14(actorMatrix,view.matrix,0x5240);
    if (input.matrixProfile != WorldModelMatrixProfile::level1InlineSpin
        || input.screenCenter.x != 159 || input.screenCenter.y != 84 || input.vertexPrecision != 0
        || actorAngles[0] != 0 || actorAngles[1] % 0x800 != 0 || actorAngles[2] % 0x800 != 0
        || view.matrix.mode > 1 || input.matrix.mode != combined.mode || input.matrix.values != combined.values
        || input.animationSelectors.size() != 1 || (input.animationSelectors[0] != 3 && input.animationSelectors[0] != 7)
        || lod.threshold != 0 || lod.flags != 128 || lod.groups.size() != 5) {
        throw std::invalid_argument("unsupported Level1 bonus geometry profile");
    }
    for (std::size_t index = 0; index < lod.groups.size(); ++index) {
        const auto& group = lod.groups[index];
        if (group.vertices.size() != 137 || group.vertexBlock != lod.groups.front().vertexBlock
            || group.animationOffset != (index == 0 ? 0 : -1)
            || group.primitives.size() != (index == 0 ? 5U : 1U)) {
            throw std::invalid_argument("unsupported immutable Level1 bonus groups");
        }
    }
    return modelGeometryOperations(lod,input,false,actorMatrix.mode == 0
        ? std::optional<FixedPosition>{view.relativePosition} : std::nullopt);
}

WorldModelGeometryWork level1ArrivalModelGeometryOperations(const ModelLod& lod,
    const WorldModelGeometryInput& input,const std::array<std::uint16_t,3> angles,
    const WorldModelViewInput& view) {
    const auto actor=makeFixedMatrixQ14(angles[0],angles[1],angles[2]);
    const auto combined=composeFixedMatricesQ14(actor,view.matrix,0x5240);
    const auto center=transformToView(view.relativePosition,{{},view.matrix,{},0});
    const auto maximum=std::max({std::abs(std::int64_t{center.x}),std::abs(std::int64_t{center.y}),std::abs(std::int64_t{center.z})});
    const auto precision=maximum+13052>32000;
    const auto half=[](const std::int32_t value) { return value>=0 ? value/2 : -((-std::int64_t{value}+1)/2); };
    const FixedPosition translated=precision ? FixedPosition{static_cast<std::int32_t>(half(center.x)),
        static_cast<std::int32_t>(half(center.y)),static_cast<std::int32_t>(half(center.z))} : center;
    if (input.matrixProfile!=WorldModelMatrixProfile::level1Arrival || input.vertexPrecision>1
        || maximum>32600 || input.vertexPrecision!=static_cast<std::uint16_t>(precision)
        || !(input.fullCenter==center) || !(input.translation==translated)
        || input.screenCenter.x!=159 || input.screenCenter.y!=84 || input.projectionShift!=7
        || input.nearPlane!=128 || angles[0]!=0 || view.matrix.mode>1
        || input.matrix.mode!=combined.mode || input.matrix.values!=combined.values
        || input.animationSelectors.size()!=1 || input.animationSelectors[0]>=4
        || lod.threshold!=0 || lod.flags!=128 || lod.groups.size()!=9) {
        throw std::invalid_argument("unsupported arrival geometry profile");
    }
    const auto block=lod.groups.front().vertexBlock;
    for (std::size_t index=0;index<lod.groups.size();++index) {
        const auto& group=lod.groups[index];
        if (block==0 || group.vertexBlock!=block || group.vertices.size()!=179
            || group.originVertex>=179 || group.animationOffset!=(index==6 ? 0 : -1)
            || group.primitives.size()!=(index==6 ? 4U : 1U)) {
            throw std::invalid_argument("unsupported immutable arrival group metadata");
        }
    }
    return modelGeometryOperations(lod,input,false,actor.mode==0
        ? std::optional<FixedPosition>{view.relativePosition} : std::nullopt);
}

WorldModelGeometryWork failureModelGeometryOperations(const ModelLod& lod, const WorldModelGeometryInput& input,
    const std::optional<FixedPosition>& untransformedActorPosition) {
    return modelGeometryOperations(lod, input, true, untransformedActorPosition);
}
enum class DeploymentGeometryDomain { legacy, departure };

static WorldModelGeometryWork deploymentGeometryOperations(const ModelLod& lod,
    const WorldModelGeometryInput& input,const std::array<std::uint16_t,3> angles,
    const WorldModelViewInput& view,const std::int16_t type,const DeploymentGeometryDomain domain) {
    const auto departure=domain==DeploymentGeometryDomain::departure;
    const auto actor=makeFixedMatrixQ14(angles[0],angles[1],angles[2]);
    const auto combined=composeFixedMatricesQ14(actor,view.matrix,0x5240);
    const auto center=transformToView(view.relativePosition,{{},view.matrix,{},0});
    const auto maximum=std::max({std::abs(std::int64_t{center.x}),std::abs(std::int64_t{center.y}),std::abs(std::int64_t{center.z})});
    const auto radius=type==7 ? 13052U : type==22 ? 134U : type==44 ? 2792U : type==25 ? 897U : 0U;
    const auto precision=maximum+radius>32000;
    const auto half=[](const std::int32_t value) { return value>=0 ? value/2 : -((-std::int64_t{value}+1)/2); };
    FixedPosition translated=precision ? FixedPosition{static_cast<std::int32_t>(half(center.x)),
        static_cast<std::int32_t>(half(center.y)),static_cast<std::int32_t>(half(center.z))} : center;
    if (departure) {
        //C877..C8A6 normalizes setup sorting independently of DS49F6 vertex precision.
        constexpr std::int64_t sortAxisLimit=0x3FAC;
        unsigned shifts=0;
        for (auto axis=maximum; precision && axis>sortAxisLimit; axis>>=1) { ++shifts; }
        const auto divisor=std::int64_t{1}<<shifts;
        const auto shifted=[divisor](const std::int32_t value) {
            return static_cast<std::int32_t>(value>=0 ? value/divisor : -((-std::int64_t{value}+divisor-1)/divisor));
        };
        const auto position=actor.mode==0 && view.matrix.mode!=0 ? view.relativePosition : center;
        translated={shifted(position.x),shifted(position.y),shifted(position.z)};
        if (type!=7 || maximum>0xffff || level1DeploymentModelCullOperations(center,type).culled) {
            throw std::invalid_argument("unsupported departure geometry sphere");
        }
    }
    if (input.matrixProfile!=WorldModelMatrixProfile::level1Deployment || input.vertexPrecision>1
        || (!departure && maximum+radius>64000) || input.vertexPrecision!=static_cast<std::uint16_t>(precision)
        || !(input.fullCenter==center) || !(input.translation==translated)
        || input.screenCenter.x!=159 || input.screenCenter.y!=84 || input.projectionShift!=7
        || input.nearPlane!=128 || view.matrix.mode>1
        || input.matrix.mode!=combined.mode || input.matrix.values!=combined.values
        || input.animationSelectors.size()!=(type==44 ? 3U : 1U)
        || input.animationSelectors[0]>=(type==7 || type==25 ? 4 : type==22 ? 2 : 8)
        || (type==44 && (input.animationSelectors[0]!=input.animationSelectors[1]
            || input.animationSelectors[0]!=input.animationSelectors[2]))
        || radius==0 || lod.threshold!=(type==22 ? 30 : 0) || lod.flags!=(type==44 ? 0 : 128)
        || lod.groups.size()!=(type==7 ? 9U : type==22 ? 1U : type==25 ? 6U : 13U)) {
        throw std::invalid_argument("unsupported Level1 guardian geometry");
    }
    constexpr std::array<std::size_t,13> vertices{226,226,226,220,220,137,223,223,190,190,190,251,251};
    constexpr std::array<std::int16_t,13> offsets{2,2,2,0,0,0,0,0,0,1,1,1,1};
    for (std::size_t index=0;index<lod.groups.size();++index) {
        const auto& group=lod.groups[index];
        const auto expectedVertices=type==7 ? 179U : type==22 ? 33U : type==25 ? 142U : vertices[index];
        const auto expectedOffset=type==7 ? (index==6 ? 0 : -1) : type==22 || type==25 ? 0 : offsets[index];
        const auto variants=type==7 ? (index==6 ? 4U : 1U) : type==22 ? 2U : type==25 ? 4U : (index==9 || index==10 ? 4U : 8U);
        if (group.vertexBlock==0 || group.vertices.size()!=expectedVertices || group.originVertex>=expectedVertices
            || group.animationOffset!=expectedOffset || group.primitives.size()!=variants) {
            throw std::invalid_argument("unsupported Level1 guardian immutable groups");
        }
    }
    auto geometryInput=input;
    if (departure && input.vertexPrecision==0) {
        //Precision0 vertices use DS51FE view center; DS496C is only the setup sort receipt.
        geometryInput.translation=center;
    }
    return modelGeometryOperations(lod,geometryInput,false,actor.mode==0
        ? std::optional<FixedPosition>{view.relativePosition} : std::nullopt);
}
WorldModelGeometryWork level1DeploymentModelGeometryOperations(const ModelLod& lod,
    const WorldModelGeometryInput& input,const std::array<std::uint16_t,3> angles,
    const WorldModelViewInput& view,const std::int16_t type) {
    return deploymentGeometryOperations(lod,input,angles,view,type,DeploymentGeometryDomain::legacy);
}
WorldModelGeometryWork level1DepartureModelGeometryOperations(const ModelLod& lod,
    const WorldModelGeometryInput& input,const std::array<std::uint16_t,3> angles,
    const WorldModelViewInput& view) {
    return deploymentGeometryOperations(lod,input,angles,view,7,DeploymentGeometryDomain::departure);
}


}

namespace stellar7 {
WorldModelGeometryWork level2ConstructorModelGeometryOperations(const ModelLod& lod,
    const WorldModelGeometryInput& input,const std::array<std::uint16_t,3> angles,
    const WorldModelViewInput& view,const std::int16_t type) {
    const auto radius=type==8 ? 860U : type==13 ? 559U : type==30 ? 868U : 0U;
    const auto actor=makeFixedMatrixQ14(angles[0],angles[1],angles[2]);
    const auto combined=composeFixedMatricesQ14(actor,view.matrix,0x5240);
    const auto center=transformToView(view.relativePosition,{{},view.matrix,{},0});
    const auto maximum=std::max(std::abs(std::int64_t{center.x}),std::int64_t{center.y});
    const auto precision=maximum+radius>32000;
    const auto half=[](const std::int32_t value) {return value>=0 ? value/2 : -((-std::int64_t{value}+1)/2);};
    const FixedPosition translation=precision ? FixedPosition{static_cast<std::int32_t>(half(center.x)),
        static_cast<std::int32_t>(half(center.y)),static_cast<std::int32_t>(half(center.z))} : center;
    if(radius==0 || angles[0]!=0 || angles[1]!=0 || center.z!=0 || center.y<=0 || maximum+radius>64000
        || view.matrix.mode>1 || input.matrixProfile!=WorldModelMatrixProfile::level2Constructor
        || input.vertexPrecision!=static_cast<std::uint16_t>(precision) || input.projectionShift!=7
        || input.nearPlane!=128 || input.screenCenter.x!=159 || input.screenCenter.y!=84
        || !(input.fullCenter==center) || !(input.translation==translation)
        || input.matrix.mode!=combined.mode || input.matrix.values!=combined.values
        || !input.animationSelectors.empty() || lod.flags!=128 || lod.groups.empty() || lod.groups.size()>16) {
        throw std::invalid_argument("unsupported Level2 constructor geometry profile");
    }
    for(const auto& group:lod.groups) {
        if(group.animationOffset!=-1 || group.primitives.size()!=1 || group.primitives[0].kind!=0) {
            throw std::invalid_argument("Level2 constructor geometry requires its no-selector immutable groups");
        }
    }
    return modelGeometryOperations(lod,input,false,actor.mode==0
        ? std::optional<FixedPosition>{view.relativePosition} : std::nullopt);
}
} // namespace stellar7

namespace stellar7 {
WorldModelGeometryWork level1OwnedModelGeometryOperations(const ModelLod& lod,
    const WorldModelGeometryInput& input,const std::array<std::uint16_t,3> angles,
    const WorldModelViewInput& view) {
    const auto actor=makeFixedMatrixQ14(angles[0],angles[1],angles[2]);
    const auto combined=composeFixedMatricesQ14(actor,view.matrix,0x5240);
    const auto center=transformToView(view.relativePosition,{{},view.matrix,{},0});
    const auto matrixProfile=actor.mode>=2 ? WorldModelMatrixProfile::level1Banked : WorldModelMatrixProfile::identityOrYaw;
    if (input.matrixProfile!=matrixProfile || angles[0]!=0 || view.matrix.mode>1 || input.vertexPrecision!=0
        || input.projectionShift!=7 || input.nearPlane!=128
        || input.screenCenter.x!=159 || input.screenCenter.y!=84
        || !(input.fullCenter==center) || !(input.translation==center)
        || input.matrix.mode!=combined.mode || input.matrix.values!=combined.values
        || std::max({std::abs(std::int64_t{center.x}),std::abs(std::int64_t{center.y}),
            std::abs(std::int64_t{center.z})})>32000 || lod.flags!=128
        || lod.groups.empty() || lod.groups.size()>16) {
        throw std::invalid_argument("unsupported owned Level1 geometry");
    }
    return modelGeometryOperations(lod,input,false,actor.mode==0
        ? std::optional<FixedPosition>{view.relativePosition} : std::nullopt,
        GeometryExecutionDomain::ownedSharedPrecision0);
}
}

namespace stellar7 {
WorldModelGeometryWork level1OwnedWideModelGeometryOperations(const ModelLod& lod,
    const WorldModelGeometryInput& input,const std::array<std::uint16_t,3> angles,
    const WorldModelViewInput& view) {
    const auto actor=makeFixedMatrixQ14(angles[0],angles[1],angles[2]);
    const auto combined=composeFixedMatricesQ14(actor,view.matrix,0x5240);
    const auto center=transformToView(view.relativePosition,{{},view.matrix,{},0});
    const auto matrixProfile=actor.mode>=2 ? WorldModelMatrixProfile::level1Banked : WorldModelMatrixProfile::identityOrYaw;
    constexpr std::int64_t sortAxisLimit=0x3FAC;
    const auto absolute=[](const std::int32_t value) {
        return value==-32768 ? std::int64_t{32767} : std::abs(std::int64_t{value});
    };
    auto maximum=std::max({absolute(center.x),absolute(center.y),absolute(center.z)});
    unsigned sortShifts=0;
    while(maximum>sortAxisLimit){maximum>>=1;++sortShifts;}
    const auto divisor=std::int64_t{1}<<sortShifts;
    const auto shifted=[divisor](const std::int32_t value) {
        return static_cast<std::int32_t>(value>=0 ? value/divisor : -((-std::int64_t{value}+divisor-1)/divisor));
    };
    const auto sortPosition=actor.mode==0 && view.matrix.mode!=0 ? view.relativePosition : center;
    const FixedPosition translation{shifted(sortPosition.x),shifted(sortPosition.y),shifted(sortPosition.z)};
    if (input.matrixProfile!=matrixProfile || angles[0]!=0 || view.matrix.mode>1 || input.vertexPrecision!=1
        || input.projectionShift!=7 || input.nearPlane!=128
        || input.screenCenter.x!=159 || input.screenCenter.y!=84
        || !(input.fullCenter==center) || !(input.translation==translation)
        || input.matrix.mode!=combined.mode || input.matrix.values!=combined.values
        || std::max({std::abs(std::int64_t{center.x}),std::abs(std::int64_t{center.y}),
            std::abs(std::int64_t{center.z})})>64000 || lod.flags!=128
        || lod.groups.empty() || lod.groups.size()>16 || input.groupOrder.empty()) {
        throw std::invalid_argument("unsupported owned precision1 Level1 geometry");
    }
    return arrivalWideGeometry(lod,input,actor.mode==0
        ? std::optional<FixedPosition>{view.relativePosition} : std::nullopt);
}
}

namespace stellar7 {
WorldModelGeometryWork failureFixedWideGeometryOperations(const ModelLod& lod,
    const WorldModelGeometryInput& input,const WorldModelViewInput& view) {
    const auto center=transformToView(view.relativePosition,{{},view.matrix,{},0});
    const auto absolute=[](const std::int32_t value) {return value==-32768 ? std::int64_t{32767} : std::abs(std::int64_t{value});};
    const auto maximum=std::max({absolute(center.x),absolute(center.y),absolute(center.z)});
    auto axis=maximum;unsigned shifts=0;
    constexpr std::int64_t sortAxisLimit=0x3FAC;
    while(axis>sortAxisLimit) {axis>>=1;++shifts;}
    const auto divisor=std::int64_t{1}<<shifts;
    const auto shifted=[divisor](const std::int32_t value) {
        return static_cast<std::int32_t>(value>=0 ? value/divisor : -((-std::int64_t{value}+divisor-1)/divisor));
    };
    const auto position=view.matrix.mode==0 ? center : view.relativePosition;
    const FixedPosition translation{shifted(position.x),shifted(position.y),shifted(position.z)};
    if(input.vertexPrecision!=1 || view.matrix.mode>1 || input.matrix.mode!=view.matrix.mode
        || input.matrix.values!=view.matrix.values || input.matrixProfile!=WorldModelMatrixProfile::identityOrYaw
        || !(input.fullCenter==center) || !(input.translation==translation) || maximum+retainedFailureFixedRadius<=32000 || maximum+retainedFailureFixedRadius>64000
        || input.screenCenter.x!=159 || input.screenCenter.y!=99 || input.projectionShift!=7 || input.nearPlane!=128
        || input.clipping!=PolygonClipMode::Disabled || !input.animationSelectors.empty()
        || lod.flags!=128 || lod.groups.empty() || lod.groups.size()>16 || input.groupOrder.empty()) {
        throw std::invalid_argument("unsupported retained failure fixed geometry");
    }
    for(const auto groupIndex:input.groupOrder) {
        const auto& group=lod.groups.at(groupIndex);
        if(group.vertexBlock==0 || group.vertices.empty() || group.vertices.size()>255
            || group.animationOffset!=-1 || group.primitives.size()!=1 || group.primitives[0].kind!=0) {
            throw std::invalid_argument("unsupported retained failure fixed immutable groups");
        }
        for(const auto& shape:group.primitives[0].shapes) {
            for(const auto index:shape.vertexIndices) {
                const auto vertex=transform(group.vertices.at(index),input.matrix);
                const auto depth=std::int64_t{vertex[1]}+center.y;
                if(depth<input.nearPlane || depth>0xffff) {
                    throw std::invalid_argument("retained failure fixed geometry requires front unsigned-word vertices");
                }
            }
        }
    }
    //No gameplay viewport clip child: this explicit family is wholly front/unclipped.
    return arrivalWideGeometry(lod,input,view.matrix.mode==0 ? std::nullopt : std::optional<FixedPosition>{view.relativePosition});
}
} //namespace stellar7

namespace stellar7 {
WorldModelGeometryWork level2FailureModelGeometryOperations(const ModelLod& lod,
    const WorldModelGeometryInput& input,const std::array<std::uint16_t,3> angles,
    const WorldModelViewInput& view,const std::int16_t type) {
    constexpr std::array<std::int16_t,12> types{0,4,8,13,19,23,30,36,37,38,39,40};
    constexpr std::array<std::uint32_t,12> radii{723,928,860,559,441,134,868,171,107,88,107,157};
    const auto found=std::find(types.begin(),types.end(),type);
    if (found==types.end()) {throw std::invalid_argument("unsupported Level2 failure geometry type");}
    const auto radius=radii[static_cast<std::size_t>(found-types.begin())];
    const auto actor=makeFixedMatrixQ14(angles[0],angles[1],angles[2]);
    const auto combined=composeFixedMatricesQ14(actor,view.matrix,0x5240);
    const auto center=transformToView(view.relativePosition,{{},view.matrix,{},0});
    const auto absolute=[](const std::int32_t value) {return value==-32768 ? std::int64_t{32767} : std::abs(std::int64_t{value});};
    const auto maximum=std::max({absolute(center.x),absolute(center.y),absolute(center.z)});
    const auto precision=maximum+radius>32000;
    FixedPosition translation=center;
    if (precision) {
        unsigned shifts=0;
        constexpr std::int64_t sortAxisLimit=0x3FAC;
        for (auto axis=maximum;axis>sortAxisLimit;axis>>=1) {++shifts;}
        const auto divisor=std::int64_t{1}<<shifts;
        const auto shifted=[divisor](const std::int32_t value) {
            return static_cast<std::int32_t>(value>=0 ? value/divisor : -((-std::int64_t{value}+divisor-1)/divisor));
        };
        const auto position=actor.mode==0 && view.matrix.mode!=0 ? view.relativePosition : center;
        translation={shifted(position.x),shifted(position.y),shifted(position.z)};
    }
    if (view.matrix.mode>3 || input.matrixProfile!=WorldModelMatrixProfile::identityOrYaw
        || maximum+radius>64000 || input.vertexPrecision!=static_cast<unsigned>(precision)
        || input.matrix.mode!=combined.mode || input.matrix.values!=combined.values
        || !(input.fullCenter==center) || !(input.translation==translation)
        || input.screenCenter.x!=159 || input.screenCenter.y!=99 || input.projectionShift!=7 || input.nearPlane!=128
        || lod.flags!=128 || lod.groups.empty() || lod.groups.size()>16 || input.groupOrder.empty()
        || ((type==19 || type==23) ? input.animationSelectors.size()!=6 : !input.animationSelectors.empty())) {
        throw std::invalid_argument("unsupported Level2 full failure geometry profile");
    }
    for (const auto& group:lod.groups) {
        if (group.vertexBlock==0 || group.vertices.empty() || group.vertices.size()>255
            || group.primitives.empty()
            || (!input.animationSelectors.empty() && group.animationOffset!=-1 && (group.animationOffset<0
                || static_cast<std::size_t>(group.animationOffset)>=input.animationSelectors.size()))
            || std::any_of(group.primitives.begin(),group.primitives.end(),[](const auto& primitive) {return primitive.kind!=0;})) {
            throw std::invalid_argument("unsupported Level2 failure immutable groups");
        }
    }
    const auto direct=actor.mode==0 ? std::optional<FixedPosition>{view.relativePosition} : std::nullopt;
    return precision ? arrivalWideGeometry(lod,input,direct,{0,318,0,199})
        : failureModelGeometryOperations(lod,input,direct);
}
} // namespace stellar7
