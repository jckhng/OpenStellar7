#include "destruction_view.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace stellar7 {
std::vector<PresentationFace> destructionFaces(const DestructionCue& cue,const std::uint64_t worldTick,
    const ViewTransform& view,const std::vector<Rgb>& palette) {
    if(worldTick<cue.born||worldTick-cue.born>=destructionLifetime) {return {};}
    constexpr int nearPlane=128;
    const auto point=transformToView(cue.position,view);
    if(point.y<nearPlane) {return {};}
    constexpr double circle=6.283185307179586;
    constexpr std::size_t sides=12,sparks=6;
    const auto age=double(worldTick-cue.born);
    const auto progress=age/destructionLifetime;
    const bool guardian=cue.type==44||cue.type==45;
    const auto radius=std::clamp(cue.radius*double(view.perspectiveScale)/point.y*(guardian?1.6:1.0),3.5,64.0);
    const auto x=(view.viewport.left+view.viewport.right)/2+double(point.x)*view.perspectiveScale/point.y;
    const auto y=(view.viewport.top+view.viewport.bottom)/2-double(point.z)*view.perspectiveScale/point.y;
    std::vector<PresentationFace> faces;
    const auto disk=[&](double cx,double cy,double size,Rgb tint,double opacity) {
        std::size_t nearest=0;int best=std::numeric_limits<int>::max();
        for(std::size_t i=0;i<palette.size();++i) {
            const auto p=palette[i];const int r=int(p.red)-tint.red,g=int(p.green)-tint.green,b=int(p.blue)-tint.blue;
            const auto distance=r*r+g*g+b*b;if(distance<best) {best=distance;nearest=i;}
        }
        PresentationFace face{{},view.viewport,static_cast<std::uint8_t>(nearest),true,
            PresentationIdentity{static_cast<std::size_t>(cue.id),std::numeric_limits<std::size_t>::max()-2,
                faces.size(),0,cue.type,0,cue.id},1,std::clamp(opacity,0.0,1.0),tint};
        for(std::size_t i=0;i<sides;++i) {
            const auto angle=circle*i/sides;
            const auto irregularity=1+0.16*std::sin(i*2.4+cue.id+age*0.7)+0.08*std::cos(i*4.1+age);
            face.points.push_back({cx+std::cos(angle)*size*irregularity,cy+std::sin(angle)*size*irregularity});
            if(WorldPresentationCapture::spatialEnabled()) {
                const auto screen=face.points.back();
                face.spatialPoints.push_back({float((screen.x-(view.viewport.left+view.viewport.right)/2)*point.y/view.perspectiveScale),
                    float(point.y),float(((view.viewport.top+view.viewport.bottom)/2-screen.y)*point.y/view.perspectiveScale)});
            }
        }
        faces.push_back(std::move(face));
    };
    const auto expansion=radius*(0.6+progress*1.2);
    // A retained fireball, rising smoke and sparse sparks all share the death's world position.
    disk(x-radius*0.25,y-radius*progress,expansion,{68,62,58},0.5*(1-progress));
    disk(x+radius*0.3,y-radius*progress*1.4,expansion*0.7,{94,82,70},0.35*(1-progress));
    disk(x,y,expansion*(1-progress*0.35),age<2?Rgb{255,244,178}:Rgb{255,111,18},std::max(0.0,1-progress*1.2));
    disk(x-radius*0.12,y-radius*0.15,expansion*0.45,age<2?Rgb{255,255,245}:Rgb{255,214,65},std::max(0.0,1-progress*1.4));
    for(std::size_t i=0;i<sparks;++i) {
        const auto angle=circle*(i+double(cue.id%11)/11)/sparks;
        const auto travel=radius*(0.3+progress*2.5);
        disk(x+std::cos(angle)*travel,y+std::sin(angle)*travel+radius*progress*progress,
            std::max(0.35,radius*0.09*(1-progress)),{255,185,48},1-progress);
    }
    return faces;
}
}
