#include "stellar7/level1_deployment_render_work.hpp"
#include "stellar7/level1_owned_view_work.hpp"
#include "stellar7/raster_line_work.hpp"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace stellar7 {
namespace {
using Operations=std::vector<NormalWorkOperation>;
constexpr Viewport viewport{24,294,24,145};
constexpr std::uint32_t craftRadius=13052;
void instructions(Operations& operations,const std::uint64_t count) {
    operations.push_back({NormalWorkKind::instructions,count});
}
bool bound(Operations& operations,const std::int32_t value,const std::int16_t limit,const bool lower) {
    const auto high=value<0 ? -1 : value>>16;
    if ((lower && high<0) || (!lower && high>0)) { instructions(operations,4);return false; }
    if (high!=0) { instructions(operations,5);return true; }
    instructions(operations,7);return lower ? value>=limit : value<=limit;
}
std::int32_t divide(Operations& operations,const std::int32_t value,const std::int32_t depth) {
    instructions(operations,53+(value<0 ? 18 : 0));
    return static_cast<std::int32_t>(std::int64_t{value}*128/depth);
}
}

Level1GuardianAnimationWork level1GuardianAnimationOperations(Level1AnimationState state,const Model3d& model) {
    if ((state.reload!=1 && state.reload!=2) || state.countdown>2 || state.variant>=8
        || model.flags!=3 || model.collisionRadius!=2792
        || model.animationLimits!=std::vector<std::uint8_t>{8,8,8}) {
        throw std::invalid_argument("unsupported Level1 guardian animation");
    }
    Level1GuardianAnimationWork result{state,{},{}};
    if (state.countdown!=0) {
        --result.state.countdown;instructions(result.operations,15);return result;
    }
    result.state.countdown=state.reload;
    const auto incremented=static_cast<std::uint8_t>(state.variant+1);
    const auto wrapped=incremented>=8;
    result.state.variant=wrapped ? 0 : incremented;
    //Shared44 is skipped, then local0 is selected: C05C40, CFBC18, CFEC30.
    constexpr std::uint64_t firstIncrementOffset=157;
    constexpr std::uint64_t selectorIteration=88;
    for (std::uint8_t index=0;index<3;++index) {
        const auto offset=firstIncrementOffset+index*(selectorIteration+(wrapped ? 1 : 0));
        result.selectorWrites.push_back({index,incremented,0x1358,offset});
        if (wrapped) { result.selectorWrites.push_back({index,0,0x1366,offset+6}); }
    }
    instructions(result.operations,19+3*18+5+(18+40)+3*(30+40)+(wrapped ? 3 : 0));
    return result;
}

Level1BeamProjectionWork level1BeamProjectionOperations(const FixedPosition center,const std::uint8_t flags,
    const ProjectedPoint retainedAnchor,const std::uint16_t retainedRadius) {
    const auto rawDepth=std::int64_t{center.y}-craftRadius;
    const auto depth=static_cast<std::int32_t>(std::max<std::int64_t>(128,rawDepth));
    if ((flags!=8 && flags!=12) || depth>=32768 || center.y < -32768 || center.y>32767
        || std::abs(std::int64_t{center.x})>65535 || std::abs(std::int64_t{center.z})>65535
        || retainedRadius>256) {
        throw std::invalid_argument("unsupported Level1 beam projection");
    }
    const auto radius=craftRadius*128/depth;
    Level1BeamProjectionWork result{{center,center,static_cast<std::uint8_t>(flags&0xfb),
        retainedAnchor.x,retainedAnchor.y,retainedRadius,false},static_cast<std::uint16_t>(std::min(256U,radius)),{}};
    auto& operations=result.operations;
    instructions(operations,22+(rawDepth<0 ? 4 : rawDepth<128 ? 6 : 3)+5);
    instructions(operations,111);
    instructions(operations,16+(radius>256 ? 2 : 0));
    operations.push_back({NormalWorkKind::repeatString,6});
    if (center.y<128) {
        instructions(operations,center.y<0 ? 6 : 9);
    } else {
        instructions(operations,13+52);
        const auto x=159+divide(operations,center.x,center.y);
        instructions(operations,50);
        const auto y=84-divide(operations,center.z,center.y);
        instructions(operations,6+11);
        if (bound(operations,x,viewport.left,true) && bound(operations,x,viewport.right,false)
            && bound(operations,y,viewport.top,true) && bound(operations,y,viewport.bottom,false)) {
            instructions(operations,8);
            result.projection.projectedX=static_cast<std::int16_t>(x);
            result.projection.projectedY=static_cast<std::int16_t>(y);
            result.projection.projectedRadius=result.modelRadius;
            result.projection.flags|=4;result.projection.projected=true;
        }
    }
    instructions(operations,7); //BBCE..BBE9, deferred nonzero callback.
    return result;
}

Level1CompletionBeamWork level1CompletionBeamOperations(const RadarActorOutput& projection,
    const std::uint8_t timer,StellarRng rng,const CompletionBeamView view,const PolygonClipMode clipping) {
    if ((view!=CompletionBeamView::main && view!=CompletionBeamView::radar)
        || timer<30 || timer>70 || (projection.flags!=8 && projection.flags!=12)
        || projection.projected!=((projection.flags&4)!=0)
        || (clipping!=PolygonClipMode::Enabled && clipping!=PolygonClipMode::Disabled)) {
        throw std::invalid_argument("unsupported Level1 beam callback");
    }
    Level1CompletionBeamWork result{{},rng,{},{}};
    result.beam.center=projection.cachedCenter;result.beam.projected=projection.projected;
    result.beam.anchor={projection.projectedX,projection.projectedY};
    result.beam.radius=projection.projectedRadius;result.beam.timer=timer;
    if (view==CompletionBeamView::radar) { instructions(result.operations,12);return result; }
    if (!projection.projected) { instructions(result.operations,15);return result; }
    if (projection.projectedX<viewport.left || projection.projectedX>viewport.right
        || projection.projectedY<viewport.top || projection.projectedY>viewport.bottom) {
        throw std::invalid_argument("beam callback lacks its source projected anchor");
    }
    instructions(result.operations,20);
    for (int offset=-5;offset<5;++offset) {
        instructions(result.operations,3); //B0E7/B0EC and B0EE CALLB39E.
        const auto wraps=(result.rng.indexA()==0)+(result.rng.indexB()==0);
        const auto color=static_cast<std::uint8_t>((result.rng.next()&15)+128);
        instructions(result.operations,20+wraps);
        result.beam.colors[static_cast<std::size_t>(offset+5)]=color;
        instructions(result.operations,16); //B0F1..B114, including21B4 CALLF.
        const auto x=static_cast<std::int16_t>(projection.projectedX+offset);
        const auto line=level1CompletionBeamLineOperations({x,static_cast<std::int16_t>(projection.projectedY+std::abs(offset))},
            {x,static_cast<std::int16_t>(projection.projectedY+77-timer)},viewport,clipping);
        result.operations.insert(result.operations.end(),line.operations.begin(),line.operations.end());
        for (const auto point:line.pixels) { result.pixels.push_back({point,color}); }
        instructions(result.operations,5);
    }
    instructions(result.operations,5);return result;
}

static WorldModelCallerWork modelRadiusCallerOperations(const FixedPosition center,const std::uint32_t radius) {
    const auto rawDepth=std::int64_t{center.y}-radius;
    auto divisor=static_cast<std::uint32_t>(std::max<std::int64_t>(128,rawDepth));
    if (radius==0 || divisor>1000000 || std::abs(std::int64_t{center.x})>1000000
        || std::abs(std::int64_t{center.z})>1000000) {
        throw std::invalid_argument("unsupported deployment caller arithmetic");
    }
    const auto originalDepth=divisor;
    auto numerator=radius*128;
    unsigned shifts=0;
    while (divisor>65535) { divisor>>=1;numerator>>=1;++shifts; }
    const auto quotient=numerator/divisor;
    const auto division=shifts!=0 ? 52+6*shifts
        : 53+(static_cast<std::int16_t>(divisor)<=static_cast<int>(numerator>>16) ? 3 : 0);
    return {static_cast<std::int32_t>(originalDepth),static_cast<std::uint16_t>(std::min(256U,quotient)),
        {{NormalWorkKind::instructions,22+(rawDepth>=65536 ? 0U : rawDepth<0 ? 4U : rawDepth<128 ? 6U : 3U)+5},
         {NormalWorkKind::instructions,58+division},
         {NormalWorkKind::instructions,17+(quotient>256 ? 2U : 0U)}}};

}

WorldModelCallerWork level1DeploymentModelCallerOperations(const FixedPosition center,const std::int16_t type) {
    const auto radius=type==7 ? 13052U : type==22 ? 134U : type==44 ? 2792U : type==25 ? 897U : 0U;
    return modelRadiusCallerOperations(center,radius);
}

WorldModelCallerWork level1OwnedWideModelCallerOperations(const FixedPosition center,const Model3d& model,
    const std::uint8_t flags,const std::uint16_t callback) {
    constexpr std::int64_t signedDivisorLimit=32768;
    constexpr std::int64_t wideExtentLimit=64000;
    constexpr std::uint32_t sourceRadiusLimit=16300;
    const auto depth=std::int64_t{center.y}-model.collisionRadius;
    const auto extent=std::max({std::abs(std::int64_t{center.x}),std::abs(std::int64_t{center.y}),
        std::abs(std::int64_t{center.z})})+model.collisionRadius;
    if (flags!=0 || callback!=0 || model.scaleShift!=0 || model.flags>1
        || model.origin!=std::array<std::int16_t,3>{} || model.collisionRadius==0
        || model.collisionRadius>=sourceRadiusLimit || depth<signedDivisorLimit || extent>wideExtentLimit) {
        throw std::invalid_argument("unsupported owned unsigned-divisor model caller");
    }
    return modelRadiusCallerOperations(center,model.collisionRadius);
}
} // namespace stellar7
