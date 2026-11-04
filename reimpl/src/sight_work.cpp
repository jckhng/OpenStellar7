#include "stellar7/sight_work.hpp"
#include "stellar7/raster_point_work.hpp"
#include <stdexcept>
namespace stellar7 {
namespace {
SightWork sightOperationsImpl(const std::uint16_t loopTick, const bool alternatePhase,
    const Viewport viewport, const std::uint8_t color, const SightRefreshInput* refresh) {
    constexpr std::uint16_t refreshMask=7;
    constexpr std::uint64_t lineCall=8;
    constexpr std::uint64_t pointCall=8;
    constexpr std::uint64_t calleeCleanup=1;
    const auto refreshTick=(loopTick&refreshMask)==0;
    if (refreshTick && refresh==nullptr) {
        throw std::invalid_argument("sight refresh predicate work is unsupported");
    }
    SightWork result{alternatePhase,color,{},{}};
    const auto instructions=[&result](std::uint64_t count) {
        result.operations.push_back({NormalWorkKind::instructions,count});
    };
    std::uint64_t refreshDispatches=0;
    if (refreshTick) {
        if (refresh->targets.size()>32767) {
            throw std::invalid_argument("sight target count exceeds signed16 domain");
        }
        constexpr std::int16_t lastTargetType=13;
        constexpr std::uint16_t halfTurn=0x8000;
        constexpr std::int16_t alignmentLimit=0x400;
        constexpr std::uint64_t predicateEntry=8;
        constexpr std::uint64_t targetTypeGate=4;
        constexpr std::uint64_t cachedAlignment=12;
        constexpr std::uint64_t targetAdvance=5;
        constexpr std::uint64_t predicateReturn=6;
        refreshDispatches=predicateEntry;
        result.alternatePhase=false;
        if (!refresh->targets.empty()) {
            ++refreshDispatches;
        }
        for (const auto& target:refresh->targets) {
            refreshDispatches+=targetTypeGate;
            if (target.type<=lastTargetType) {
                refreshDispatches+=cachedAlignment;
                const auto bits=static_cast<std::uint16_t>(target.targetHeading-refresh->playerYaw+halfTurn);
                const auto delta=bits<=0x7fff ? static_cast<int>(bits) : static_cast<int>(bits)-0x10000;
                const auto magnitude=static_cast<std::uint16_t>(delta<0 ? -delta : delta);
                const auto signedMagnitude=magnitude<=0x7fff ? static_cast<int>(magnitude) : static_cast<int>(magnitude)-0x10000;
                if (signedMagnitude<alignmentLimit) {
                    result.alternatePhase=true;
                    break;
                }
            }
            refreshDispatches+=targetAdvance;
        }
        refreshDispatches+=predicateReturn;
        if (!result.alternatePhase && !refresh->targets.empty()) {
            ++refreshDispatches;
        }
        refreshDispatches+=2; //6755 CALL9cb6 and6758 retained-byte assignment.
    }
    const auto append=[&result](const RasterLineWork& work) {
        result.operations.insert(result.operations.end(),work.operations.begin(),work.operations.end());
        for (const auto point:work.pixels) { result.pixels.push_back({point,result.color}); }
    };
    const auto line=[&](ProjectedPoint first,ProjectedPoint second,std::uint64_t prefix) {
        instructions(prefix);
        append(rasterLineOperations(first,second,viewport,PolygonClipMode::Disabled));
    };
    const auto point=[&](ProjectedPoint position,std::uint64_t prefix) {
        instructions(prefix);
        append(rasterPointOperations(position,viewport));
    };
    //850c ADD/CALL,674e TEST/JNZ, then color/clip assignments before first line.
    line({143,75},{175,75},2+2+refreshDispatches+3+lineCall);
    line({143,95},{175,95},calleeCleanup+lineCall);
    point({159,85},calleeCleanup+pointCall);
    if (result.alternatePhase) {
        line({155,77},{163,77},3+lineCall);
        line({155,93},{163,93},calleeCleanup+lineCall);
        line({135,80},{151,80},calleeCleanup+lineCall);
        line({135,90},{151,90},calleeCleanup+lineCall);
        line({183,80},{167,80},calleeCleanup+lineCall);
        line({183,90},{167,90},calleeCleanup+lineCall);
        point({135,81},calleeCleanup+pointCall);
        point({183,81},calleeCleanup+pointCall);
        point({135,89},calleeCleanup+pointCall);
        point({183,89},calleeCleanup+pointCall+1); //6874 jumps to shared final call.
    } else {
        line({135,77},{151,77},4+lineCall);
        line({135,93},{151,93},calleeCleanup+lineCall);
        line({183,77},{167,77},calleeCleanup+lineCall);
        line({183,93},{167,93},calleeCleanup+lineCall);
        point({135,78},calleeCleanup+pointCall);
        point({183,78},calleeCleanup+pointCall);
        point({135,92},calleeCleanup+pointCall);
        point({183,92},calleeCleanup+pointCall);
    }
    instructions(calleeCleanup+1); //691d ADD SP,6;6920 RET.
    return result;
}
}
SightWork sightOperations(const std::uint16_t loopTick, const bool alternatePhase,
    const Viewport viewport, const std::uint8_t color) {
    return sightOperationsImpl(loopTick,alternatePhase,viewport,color,nullptr);
}
SightWork sightOperations(const std::uint16_t loopTick, const bool alternatePhase,
    const Viewport viewport, const std::uint8_t color, const SightRefreshInput& refresh) {
    return sightOperationsImpl(loopTick,alternatePhase,viewport,color,&refresh);
}
}
