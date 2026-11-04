#include "world_presentation.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace stellar7 {
namespace {
constexpr int sourceWidth=320,sourceHeight=200;
constexpr double maximumInterpolationDisplacement=32;
thread_local bool captureEnabled=false;
thread_local ChaseOptions captureCamera;
thread_local bool captureSpatial=false;
std::uint32_t rgb(const Rgb color) {
    return 0xff000000U|(std::uint32_t{color.red}<<16)|(std::uint32_t{color.green}<<8)|color.blue;
}
void patch(std::vector<std::uint32_t>& output,const std::vector<Rgb>& palette,
    std::size_t offset,const std::vector<std::uint8_t>& pixels,const unsigned scale) {
    const auto width=sourceWidth*scale;
    for(std::size_t begin=0;begin<pixels.size();) {
        const auto count=std::min(pixels.size()-begin,sourceWidth-offset%sourceWidth);
        const auto x=(offset%sourceWidth)*scale,y=(offset/sourceWidth)*scale;
        auto* const row=output.data()+y*width+x;
        for(std::size_t i=0;i<count;++i) {std::fill_n(row+i*scale,scale,rgb(palette[pixels[begin+i]]));}
        for(unsigned copy=1;copy<scale;++copy) {std::memcpy(row+copy*width,row,count*scale*sizeof(*row));}
        begin+=count;offset+=count;
    }
}
void face(std::vector<std::uint32_t>& output,const std::vector<Rgb>& palette,
    const PresentationFace& draw,const PresentationFace* next,const double fraction,const unsigned scale) {
    auto points=draw.points;
    if(points.empty()) {return;}
    if(!std::isfinite(draw.rasterStrokeWidth)||draw.rasterStrokeWidth<0||draw.rasterStrokeWidth>1) {
        throw std::invalid_argument("Invalid native raster stroke width");
    }
    const auto stroke=std::max(1,static_cast<int>(std::ceil(draw.rasterStrokeWidth*scale)));
    const auto firstOffset=-stroke/2,lastOffset=firstOffset+stroke-1;
    if(next) {
        for(std::size_t i=0;i<points.size();++i) {
            points[i].x+=(next->points[i].x-points[i].x)*fraction;
            points[i].y+=(next->points[i].y-points[i].y)*fraction;
        }
    }
    for(auto& point:points) {point.x=(point.x+0.5)*scale;point.y=(point.y+0.5)*scale;}
    const auto width=sourceWidth*scale;
    const int left=std::max(0,int{draw.clip.left}*static_cast<int>(scale));
    const int right=std::min(static_cast<int>(width)-1,(int{draw.clip.right}+1)*static_cast<int>(scale)-1);
    const int top=std::max(0,int{draw.clip.top}*static_cast<int>(scale));
    const int bottom=std::min(sourceHeight*static_cast<int>(scale)-1,(int{draw.clip.bottom}+1)*static_cast<int>(scale)-1);
    const auto lighting=std::clamp(draw.lighting+(next?(next->lighting-draw.lighting)*fraction:0),0.0,1.0);
    const auto opacity=std::clamp(draw.opacity+(next?(next->opacity-draw.opacity)*fraction:0),0.0,1.0);
    auto color=draw.tint.value_or(palette[draw.color]);
    color={static_cast<std::uint8_t>(std::lround(color.red*lighting)),
        static_cast<std::uint8_t>(std::lround(color.green*lighting)),static_cast<std::uint8_t>(std::lround(color.blue*lighting))};
    const auto ink=rgb(color);const auto alpha=static_cast<unsigned>(std::lround(opacity*255));
    const auto paint=[&](std::size_t offset) {
        if(alpha==255) {output[offset]=ink;return;}
        const auto old=output[offset];std::uint32_t value=0xff000000U;
        for(const auto shift:{0,8,16}) {
            value|=((((ink>>shift)&255)*alpha+((old>>shift)&255)*(255-alpha)+127)/255)<<shift;
        }
        output[offset]=value;
    };
    const auto pixel=[&](int x,int y) {if(x>=left&&x<=right&&y>=top&&y<=bottom) {paint(y*width+x);}};
    const auto stamp=[&](int x,int y) {
        for(int dy=firstOffset;dy<=lastOffset;++dy) {for(int dx=firstOffset;dx<=lastOffset;++dx) {pixel(x+dx,y+dy);}}
    };
    const auto line=[&](PresentationPoint a,PresentationPoint b) {
        // Clip before stepping so an offscreen near-plane edge has bounded cost.
        const double dx=b.x-a.x,dy=b.y-a.y;
        double begin=0,end=1;
        const auto clip=[&](double p,double q) {
            if(p==0) {return q>=0;}
            const double t=q/p;
            if(p<0) {begin=std::max(begin,t);} else {end=std::min(end,t);}
            return begin<=end;
        };
        if(!clip(-dx,a.x-left+lastOffset)||!clip(dx,right-firstOffset-a.x)
            ||!clip(-dy,a.y-top+lastOffset)||!clip(dy,bottom-firstOffset-a.y)) {return;}
        b={a.x+dx*end,a.y+dy*end};a={a.x+dx*begin,a.y+dy*begin};
        const int steps=static_cast<int>(std::ceil(std::max(std::abs(b.x-a.x),std::abs(b.y-a.y))));
        if(steps==0) {stamp(static_cast<int>(a.x),static_cast<int>(a.y));return;}
        for(int step=0;step<=steps;++step) {
            const double t=static_cast<double>(step)/steps;
            stamp(static_cast<int>(a.x+(b.x-a.x)*t),static_cast<int>(a.y+(b.y-a.y)*t));
        }
    };
    if(points.size()<3||!draw.filled) {
        if(points.size()==1) {stamp(static_cast<int>(points[0].x),static_cast<int>(points[0].y));return;}
        for(std::size_t i=0;i<points.size()-(points.size()==2?1:0);++i) {line(points[i],points[(i+1)%points.size()]);}
        return;
    }
    const auto limits=std::minmax_element(points.begin(),points.end(),[](auto a,auto b) {return a.y<b.y;});
    const int first=static_cast<int>(std::clamp(std::ceil(limits.first->y-0.5),double(top),double(bottom+1)));
    const int last=static_cast<int>(std::clamp(std::floor(limits.second->y-0.5),double(top-1),double(bottom)));
    std::vector<double> intersections;intersections.reserve(points.size());
    bool covered=false;
    for(int y=first;y<=last;++y) {
        intersections.clear();const double scan=y+0.5;
        for(std::size_t i=0;i<points.size();++i) {
            const auto a=points[i],b=points[(i+1)%points.size()];
            if((a.y<=scan&&b.y>scan)||(b.y<=scan&&a.y>scan)) {
                intersections.push_back(a.x+(scan-a.y)*(b.x-a.x)/(b.y-a.y));
            }
        }
        std::sort(intersections.begin(),intersections.end());
        for(std::size_t i=1;i<intersections.size();i+=2) {
            const int start=static_cast<int>(std::clamp(std::ceil(intersections[i-1]-0.5),double(left),double(right+1)));
            const int stop=static_cast<int>(std::clamp(std::floor(intersections[i]-0.5),double(left-1),double(right)));
            if(start<=stop) {
                if(alpha==255) {std::fill(output.begin()+y*width+start,output.begin()+y*width+stop+1,ink);}
                else {for(int x=start;x<=stop;++x) {paint(y*width+x);}}
                covered=true;
            }
        }
    }
    //Source rasterization collapses tiny filled faces to a line/point instead of losing them.
    if(!covered||stroke>1) {
        for(std::size_t i=0;i<points.size();++i) {line(points[i],points[(i+1)%points.size()]);}
    }
}
std::vector<const PresentationFace*> faces(const WorldPresentation& scene) {
    std::vector<const PresentationFace*> result;
    for(const auto& draw:scene.draws) {if(draw.face) {result.push_back(&*draw.face);}}
    return result;
}
bool compatible(const IndexedScreen& current,const IndexedScreen& next) {
    if(!current.presentation||!next.presentation||current.palette!=next.palette) {return false;}
    const auto& a=*current.presentation;const auto& b=*next.presentation;
    if(a.level!=b.level||a.phase!=b.phase||!(a.camera==b.camera)||static_cast<std::uint16_t>(a.tick+1)!=b.tick) {return false;}
    const auto first=faces(a),second=faces(b);
    if(first.size()!=second.size()) {return false;}
    for(std::size_t i=0;i<first.size();++i) {
        const auto& x=*first[i];const auto& y=*second[i];
        if(!x.identity||!y.identity||!x.identity->incarnation||!y.identity->incarnation
            ||!(*x.identity==*y.identity)||x.points.size()!=y.points.size()
            ||x.color!=y.color||x.filled!=y.filled||x.clip.left!=y.clip.left||x.clip.right!=y.clip.right
            ||x.clip.top!=y.clip.top||x.clip.bottom!=y.clip.bottom) {return false;}
        for(std::size_t j=0;j<x.points.size();++j) {
            if(std::abs(x.points[j].x-y.points[j].x)>maximumInterpolationDisplacement
                ||std::abs(x.points[j].y-y.points[j].y)>maximumInterpolationDisplacement) {return false;}
        }
    }
    return true;
}
} // namespace
std::vector<ProjectedPoint> boundedPresentationPolygon(const PresentationFace& face) {
    auto points=face.points;
    const auto pass=[&](int axis,double boundary,bool greater) {
        if(points.empty()) {return;}
        std::vector<PresentationPoint> clipped;clipped.reserve(points.size()+2);
        const auto coordinate=[axis](const PresentationPoint point) {return axis==0?point.x:point.y;};
        auto previous=points.back();auto previousInside=greater?coordinate(previous)>=boundary:coordinate(previous)<=boundary;
        for(const auto current:points) {
            const auto inside=greater?coordinate(current)>=boundary:coordinate(current)<=boundary;
            if(inside!=previousInside) {
                const auto t=(boundary-coordinate(previous))/(coordinate(current)-coordinate(previous));
                clipped.push_back({previous.x+(current.x-previous.x)*t,previous.y+(current.y-previous.y)*t});
            }
            if(inside) {clipped.push_back(current);}
            previous=current;previousInside=inside;
        }
        points=std::move(clipped);
    };
    pass(0,face.clip.left,true);pass(0,face.clip.right,false);
    pass(1,face.clip.top,true);pass(1,face.clip.bottom,false);
    const auto centerX=(face.clip.left+face.clip.right)/2,centerY=(face.clip.top+face.clip.bottom)/2;
    std::vector<ProjectedPoint> result;result.reserve(points.size());
    for(const auto point:points) {
        result.push_back({static_cast<std::int16_t>(std::clamp(centerX+std::trunc(point.x-centerX),double(face.clip.left),double(face.clip.right))),
            static_cast<std::int16_t>(std::clamp(centerY+std::trunc(point.y-centerY),double(face.clip.top),double(face.clip.bottom)))});
    }
    return result;
}
bool PresentationIdentity::operator==(const PresentationIdentity& other) const {
    return std::tie(allocation,group,shape,variant,type,lod,incarnation)
        ==std::tie(other.allocation,other.group,other.shape,other.variant,other.type,other.lod,other.incarnation);
}
WorldPresentationCapture::WorldPresentationCapture(const ChaseOptions camera,const PresentationGeometry geometry):previous_(captureEnabled),previousCamera_(captureCamera),previousSpatial_(captureSpatial) {
    captureEnabled=true;captureCamera=camera;captureSpatial=geometry==PresentationGeometry::spatial;
}
WorldPresentationCapture::~WorldPresentationCapture() {captureEnabled=previous_;captureCamera=previousCamera_;captureSpatial=previousSpatial_;}
bool WorldPresentationCapture::enabled() {return captureEnabled;}
ChaseOptions WorldPresentationCapture::camera() {return captureCamera;}
bool WorldPresentationCapture::spatialEnabled() {return captureSpatial;}
WorldPresentationRecorder::WorldPresentationRecorder(const std::vector<std::uint8_t>& pixels,
    const std::size_t level,const unsigned phase,const unsigned tick,const std::optional<ViewTransform> spatialView):frame_(std::make_shared<WorldPresentation>()),previous_(pixels) {
    frame_->base=pixels;frame_->level=level;frame_->phase=phase;frame_->tick=tick;
    frame_->camera=captureCamera;
    if(captureSpatial&&spatialView) {
        spatial_=std::make_shared<SpatialScene>();spatial_->camera=*spatialView;
        spatial_->background=pixels;spatial_->overlay.assign(pixels.size(),-1);frame_->spatial=spatial_;
    }
}
void WorldPresentationRecorder::spatialSurface(SpatialSurface surface) {
    if(spatial_) {spatial_->surfaces.push_back(std::move(surface));}
}
void WorldPresentationRecorder::spatialBillboard(SpatialBillboard billboard) {
    if(spatial_) {spatial_->billboards.push_back(std::move(billboard));}
}
void WorldPresentationRecorder::spatialWorld(SpatialWorld world,std::vector<SpatialLight> lights) {
    if(spatial_) {spatial_->world=std::move(world);spatial_->lights=std::move(lights);}
}
void WorldPresentationRecorder::spatialOverlay(const std::vector<std::uint8_t>& before,
    const std::vector<std::uint8_t>& after,const std::size_t opaqueBegin) {
    if(!spatial_) {return;}
    if(before.size()!=spatial_->overlay.size()||after.size()!=before.size()) {throw std::invalid_argument("Spatial overlay size mismatch");}
    for(std::size_t i=0;i<before.size();++i) {if(i>=opaqueBegin||before[i]!=after[i]) {spatial_->overlay[i]=after[i];}}
}
void WorldPresentationRecorder::changes(const std::vector<std::uint8_t>& pixels,PresentationDraw& draw) {
    for(std::size_t i=0;i<pixels.size();) {
        if(pixels[i]==previous_[i]) {++i;continue;}
        PresentationPatch value{i,{}};
        do {value.pixels.push_back(pixels[i]);++i;} while(i<pixels.size()&&pixels[i]!=previous_[i]);
        draw.patches.push_back(std::move(value));
    }
}
void WorldPresentationRecorder::beforeFace(const std::vector<std::uint8_t>& pixels,PresentationFace face) {
    if(spatial_&&!face.spatialPoints.empty()) {
        SpatialSurface surface{face.spatialPoints,face.color,face.filled,face.lighting,face.opacity,face.tint};
        surface.strokeRadius=face.spatialStrokeRadius;
        if(face.identity&&face.identity->group==std::numeric_limits<std::size_t>::max()-1) {surface.material=SpatialMaterial::shadow;}
        spatialSurface(std::move(surface));
    }
    PresentationDraw draw;changes(pixels,draw);draw.face=std::move(face);frame_->draws.push_back(std::move(draw));
}
void WorldPresentationRecorder::afterFace(const std::vector<std::uint8_t>& pixels) {previous_=pixels;}
void WorldPresentationRecorder::recordPixels(const std::vector<std::uint8_t>& pixels) {
    PresentationDraw draw;changes(pixels,draw);
    if(!draw.patches.empty()) {frame_->draws.push_back(std::move(draw));}
    previous_=pixels;
}
void WorldPresentationRecorder::recordDestructions(const std::vector<DestructionCue>& cues,const std::uint64_t worldTick) {
    frame_->destructions=cues;frame_->worldTick=worldTick;
}
std::shared_ptr<const WorldPresentation> WorldPresentationRecorder::finish(const std::vector<std::uint8_t>& pixels,
    std::shared_ptr<const WorldPresentation> chase) {
    PresentationDraw draw;changes(pixels,draw);
    if(!draw.patches.empty()) {frame_->draws.push_back(std::move(draw));}
    frame_->source=pixels;frame_->chase=std::move(chase);return std::move(frame_);
}
std::vector<std::uint32_t> renderWorldPresentation(const IndexedScreen& frame,const unsigned scale,
    const IndexedScreen* next,const double fraction,const EnhancedView view) {
    if((scale!=1&&scale!=2&&scale!=4)||frame.pixels.size()!=sourceWidth*sourceHeight||frame.palette.size()!=256
        ||!std::isfinite(fraction)||fraction<0||fraction>1) {throw std::invalid_argument("Invalid enhanced presentation");}
    if(view==EnhancedView::chase&&frame.presentation&&frame.presentation->chase) {
        const auto& scene=frame.presentation->chase;
        const IndexedScreen selected{scene->source,frame.palette,scene};
        IndexedScreen future;
        if(next&&next->presentation&&next->presentation->chase) {
            const auto& other=next->presentation->chase;
            future={other->source,next->palette,other};
        }
        return renderWorldPresentation(selected,scale,future.presentation?&future:nullptr,fraction);
    }
    std::vector<std::uint32_t> output(sourceWidth*sourceHeight*scale*scale);
    if(scale==1||!frame.presentation) {patch(output,frame.palette,0,frame.pixels,scale);return output;}
    const auto& scene=*frame.presentation;
    patch(output,frame.palette,0,scene.base,scale);
    const auto future=next&&compatible(frame,*next)?faces(*next->presentation):std::vector<const PresentationFace*>{};
    std::size_t index=0;
    for(const auto& draw:scene.draws) {
        for(const auto& value:draw.patches) {patch(output,frame.palette,value.offset,value.pixels,scale);}
        if(draw.face) {face(output,frame.palette,*draw.face,future.empty()?nullptr:future[index],fraction,scale);++index;}
    }
    // The retained page owner composes HUD/cockpit after world drawing. Preserve
    // those writes over the enhanced world without changing its indexed page.
    for(std::size_t i=0;i<frame.pixels.size();) {
        if(frame.pixels[i]==scene.source[i]) {++i;continue;}
        const auto begin=i;std::vector<std::uint8_t> pixels;
        do {pixels.push_back(frame.pixels[i]);++i;} while(i<frame.pixels.size()&&frame.pixels[i]!=scene.source[i]);
        patch(output,frame.palette,begin,pixels,scale);
    }
    return output;
}
} // namespace stellar7
