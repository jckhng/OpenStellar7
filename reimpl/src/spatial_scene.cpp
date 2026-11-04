#include "spatial_scene.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr double areaEpsilon=1e-7;
constexpr std::size_t maximumSurfaceVertices=256;
struct Point2 {double x,y;};
double cross(Point2 a,Point2 b,Point2 c) {return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);}
bool same(SpatialPoint a,SpatialPoint b) {return a.x==b.x&&a.y==b.y&&a.z==b.z;}
SpatialVertex at(SpatialVertex vertex,SpatialPoint point) {
    vertex.x=point.x;vertex.y=point.y;vertex.z=point.z;return vertex;
}
void strokePoint(std::vector<SpatialVertex>& triangles,const SpatialVertex& center,float radius) {
    const std::array<SpatialPoint,6> points{{{center.x+radius,center.y,center.z},{center.x,center.y+radius,center.z},
        {center.x-radius,center.y,center.z},{center.x,center.y-radius,center.z},
        {center.x,center.y,center.z+radius},{center.x,center.y,center.z-radius}}};
    for(std::size_t i=0;i<4;++i) {
        const auto next=(i+1)%4;
        for(const auto pole:{4,5}) {
            triangles.push_back(at(center,points[i]));triangles.push_back(at(center,points[next]));triangles.push_back(at(center,points[pole]));
        }
    }
}
void strokeLine(std::vector<SpatialVertex>& triangles,const SpatialVertex& a,const SpatialVertex& b,float radius) {
    const SpatialPoint delta{b.x-a.x,b.y-a.y,b.z-a.z};
    const auto length=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z);
    if(length==0) {strokePoint(triangles,a,radius);return;}
    const SpatialPoint axis{delta.x/length,delta.y/length,delta.z/length};
    SpatialPoint u{axis.y,-axis.x,0};const auto span=std::sqrt(u.x*u.x+u.y*u.y);
    if(span==0) {u={1,0,0};}
    else {u.x/=span;u.y/=span;}
    const SpatialPoint v{axis.y*u.z-axis.z*u.y,axis.z*u.x-axis.x*u.z,axis.x*u.y-axis.y*u.x};
    constexpr std::size_t sides=8;
    constexpr float circle=6.28318530718f;
    std::array<SpatialVertex,sides> first,last;
    for(std::size_t i=0;i<sides;++i) {
        const auto angle=i*circle/sides,c=radius*std::cos(angle),s=radius*std::sin(angle);
        const SpatialPoint offset{u.x*c+v.x*s,u.y*c+v.y*s,u.z*c+v.z*s};
        first[i]=at(a,{a.x+offset.x,a.y+offset.y,a.z+offset.z});
        last[i]=at(b,{b.x+offset.x,b.y+offset.y,b.z+offset.z});
    }
    for(std::size_t i=0;i<sides;++i) {
        const auto next=(i+1)%sides;
        triangles.insert(triangles.end(),{first[i],last[i],last[next],first[i],last[next],first[next],
            a,first[next],first[i],b,last[i],last[next]});
    }
}
std::vector<std::array<std::size_t,3>> triangulate(const std::vector<SpatialPoint>& points) {
    std::array<double,3> normal{};
    for(std::size_t i=0;i<points.size();++i) {
        const auto a=points[i],b=points[(i+1)%points.size()];
        normal[0]+=(double(a.y)-b.y)*(double(a.z)+b.z);
        normal[1]+=(double(a.z)-b.z)*(double(a.x)+b.x);
        normal[2]+=(double(a.x)-b.x)*(double(a.y)+b.y);
    }
    const auto axis=std::max_element(normal.begin(),normal.end(),[](double a,double b) {return std::abs(a)<std::abs(b);})-normal.begin();
    if(std::abs(normal[axis])<areaEpsilon) {return {};}
    std::vector<Point2> flat;
    for(const auto p:points) {flat.push_back(axis==0?Point2{p.y,p.z}:axis==1?Point2{p.x,p.z}:Point2{p.x,p.y});}
    double area=0;
    for(std::size_t i=0;i<flat.size();++i) {const auto a=flat[i],b=flat[(i+1)%flat.size()];area+=a.x*b.y-b.x*a.y;}
    const double winding=area>0?1:-1;
    std::vector<std::size_t> remaining(points.size());std::iota(remaining.begin(),remaining.end(),0);
    std::vector<std::array<std::size_t,3>> result;
    while(remaining.size()>2) {
        bool removed=false;
        for(std::size_t i=0;i<remaining.size();++i) {
            const auto a=remaining[(i+remaining.size()-1)%remaining.size()],b=remaining[i],c=remaining[(i+1)%remaining.size()];
            const auto turn=cross(flat[a],flat[b],flat[c])*winding;
            if(std::abs(turn)<areaEpsilon) {
                remaining.erase(remaining.begin()+i);removed=true;break;
            }
            if(turn<0) {continue;}
            const auto inside=[&](std::size_t p) {
                return p!=a&&p!=b&&p!=c&&cross(flat[a],flat[b],flat[p])*winding>=-areaEpsilon
                    &&cross(flat[b],flat[c],flat[p])*winding>=-areaEpsilon&&cross(flat[c],flat[a],flat[p])*winding>=-areaEpsilon;
            };
            if(std::any_of(remaining.begin(),remaining.end(),inside)) {continue;}
            result.push_back({a,b,c});remaining.erase(remaining.begin()+i);removed=true;break;
        }
        if(!removed) {throw std::invalid_argument("Non-simple spatial polygon cannot be triangulated");}
    }
    return result;
}
}
SpatialMatrix spatialIdentity() {return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
SpatialMatrix spatialProjection(const ViewTransform& camera,const double displayAspect,const SpatialPoint eye) {
    constexpr float sourceWidth=320,sourceHeight=200,sourceDisplayAspect=4.0f/3.0f;
    constexpr float nearPlane=128,farPlane=2000000;
    if(!std::isfinite(displayAspect)||displayAspect<=0||camera.perspectiveScale<=0
        ||!std::isfinite(eye.x)||!std::isfinite(eye.y)||!std::isfinite(eye.z)) {throw std::invalid_argument("Invalid spatial view");}
    SpatialMatrix m{};
    m[0]=float(2*camera.perspectiveScale/sourceWidth*sourceDisplayAspect/displayAspect);
    m[5]=2*camera.perspectiveScale/sourceHeight;
    m[8]=1-(camera.viewport.left+camera.viewport.right+1)/sourceWidth;
    m[9]=(camera.viewport.top+camera.viewport.bottom+1)/sourceHeight-1;
    m[10]=-(farPlane+nearPlane)/(farPlane-nearPlane);m[11]=-1;
    m[14]=-2*farPlane*nearPlane/(farPlane-nearPlane);
    // Right/forward/up native eye translation becomes right/up/back in GL.
    const auto base=m;
    for(unsigned row=0;row<4;++row) {m[12+row]=base[12+row]-base[row]*eye.x-base[4+row]*eye.z+base[8+row]*eye.y;}
    return m;
}
SpatialGeometry spatialGeometry(const SpatialScene& scene,const std::vector<Rgb>& palette) {
    if(palette.size()!=256) {throw std::invalid_argument("Spatial scene requires 256 palette entries");}
    SpatialGeometry result;
    for(const auto& surface:scene.surfaces) {
        if(surface.points.size()>maximumSurfaceVertices||!std::isfinite(surface.lighting)||!std::isfinite(surface.opacity)
            ||!std::isfinite(surface.strokeRadius)||surface.strokeRadius<0) {
            throw std::invalid_argument("Invalid spatial surface");
        }
        std::vector<SpatialPoint> points;
        std::vector<std::size_t> originals;
        if(!surface.uv.empty()&&surface.uv.size()!=surface.points.size()) {throw std::invalid_argument("Spatial UV count mismatch");}
        for(std::size_t index=0;index<surface.points.size();++index) {
            const auto p=surface.points[index];
            if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)) {throw std::invalid_argument("Non-finite spatial vertex");}
            if(points.empty()||!same(points.back(),p)) {points.push_back(p);originals.push_back(index);}
        }
        if(points.size()>1&&same(points.front(),points.back())) {points.pop_back();originals.pop_back();}
        const auto opacity=float(std::clamp(surface.opacity,0.0,1.0));
        if(points.empty()||opacity==0) {continue;}
        const auto color=surface.tint.value_or(palette[surface.color]);
        const auto light=float(std::clamp(surface.lighting,0.0,1.0));
        SpatialPoint normal{};
        if(surface.filled) {
            for(std::size_t i=2;i<points.size();++i) {
                const SpatialPoint a{points[i-1].x-points[0].x,points[i-1].y-points[0].y,points[i-1].z-points[0].z};
                const SpatialPoint b{points[i].x-points[0].x,points[i].y-points[0].y,points[i].z-points[0].z};
                normal={a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
                const auto length=std::sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z);
                if(length==0) {continue;}
                const auto sign=normal.x*points[0].x+normal.y*points[0].y+normal.z*points[0].z>0?-1.0f:1.0f;
                normal={normal.x*sign/length,normal.y*sign/length,normal.z*sign/length};break;
            }
        }
        const auto vertex=[&](std::size_t index) {
            const auto p=points[index];const auto uv=surface.uv.empty()?std::array<float,2>{}:surface.uv[originals[index]];
            return SpatialVertex{p.x,p.z,-p.y,color.red*light/255,color.green*light/255,color.blue*light/255,opacity,uv[0],uv[1],normal.x,normal.z,-normal.y};
        };
        auto& triangles=surface.material==SpatialMaterial::groundDetail?result.groundTriangles
            :surface.material==SpatialMaterial::ground?result.plainGroundTriangles:opacity<1?result.translucentTriangles:result.triangles;
        auto& lines=opacity<1?result.translucentLines:result.lines;
        auto& dots=opacity<1?result.translucentPoints:result.points;
        if(points.size()==1) {
            if(surface.strokeRadius>0) {strokePoint(triangles,vertex(0),surface.strokeRadius);}
            else {dots.push_back(vertex(0));}
            continue;
        }
        const auto indices=surface.filled&&points.size()>2?triangulate(points):std::vector<std::array<std::size_t,3>>{};
        if(!indices.empty()) {
            for(const auto& triangle:indices) {for(const auto index:triangle) {triangles.push_back(vertex(index));}}
            if(surface.strokeRadius==0) {continue;}
        }
        const auto edges=points.size()==2?1:points.size();
        for(std::size_t i=0;i<edges;++i) {
            const auto a=vertex(i),b=vertex((i+1)%points.size());
            if(surface.strokeRadius>0) {strokeLine(triangles,a,b,surface.strokeRadius);}
            else {lines.push_back(a);lines.push_back(b);}
        }
    }
    return result;
}
} // namespace stellar7
