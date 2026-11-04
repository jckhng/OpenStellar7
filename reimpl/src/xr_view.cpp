#include "xr_view.hpp"
#include <cmath>
#include <stdexcept>
namespace stellar7 {
namespace {
constexpr float halfPi=1.57079632679f,quaternionTolerance=0.01f;
void validate(const XrPose& pose) {
    const auto q=pose.orientation;const auto p=pose.position;
    const auto norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    if(!std::isfinite(norm)||std::abs(norm-1)>quaternionTolerance
        ||!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)) {
        throw std::invalid_argument("Invalid XR pose");
    }
}
SpatialMatrix multiply(const SpatialMatrix& a,const SpatialMatrix& b) {
    SpatialMatrix result{};
    for(unsigned col=0;col<4;++col) {for(unsigned row=0;row<4;++row) {
        for(unsigned k=0;k<4;++k) {result[col*4+row]+=a[k*4+row]*b[col*4+k];}
    }}
    return result;
}
SpatialMatrix poseMatrix(const XrPose& pose) {
    validate(pose);auto q=pose.orientation;
    const auto norm=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    q={q.x/norm,q.y/norm,q.z/norm,q.w/norm};
    return {1-2*(q.y*q.y+q.z*q.z),2*(q.x*q.y+q.z*q.w),2*(q.x*q.z-q.y*q.w),0,
        2*(q.x*q.y-q.z*q.w),1-2*(q.x*q.x+q.z*q.z),2*(q.y*q.z+q.x*q.w),0,
        2*(q.x*q.z+q.y*q.w),2*(q.y*q.z-q.x*q.w),1-2*(q.x*q.x+q.y*q.y),0,
        pose.position.x,pose.position.y,pose.position.z,1};
}
SpatialMatrix inversePose(const XrPose& pose) {
    const auto m=poseMatrix(pose);auto result=spatialIdentity();
    for(unsigned row=0;row<3;++row) {for(unsigned col=0;col<3;++col) {result[col*4+row]=m[row*4+col];}}
    for(unsigned row=0;row<3;++row) {result[12+row]=-(result[row]*m[12]+result[4+row]*m[13]+result[8+row]*m[14]);}
    return result;
}
}
SpatialMatrix xrClipFromNative(const XrView& eye,const XrPose& seat,const XrViewScale& scale) {
    const auto f=eye.fov;
    for(const auto angle:{f.left,f.right,f.up,f.down}) {
        if(!std::isfinite(angle)||angle<=-halfPi||angle>=halfPi) {throw std::invalid_argument("Invalid XR FOV");}
    }
    if(f.left>=f.right||f.down>=f.up||!std::isfinite(scale.nativeUnitsPerMetre)||scale.nativeUnitsPerMetre<=0
        ||!std::isfinite(scale.nearMetres)||!std::isfinite(scale.farMetres)||scale.nearMetres<=0||scale.farMetres<=scale.nearMetres) {
        throw std::invalid_argument("Invalid XR projection/scale");
    }
    const auto left=std::tan(f.left),right=std::tan(f.right),up=std::tan(f.up),down=std::tan(f.down);
    SpatialMatrix projection{};projection[0]=2/(right-left);projection[5]=2/(up-down);
    projection[8]=(right+left)/(right-left);projection[9]=(up+down)/(up-down);
    projection[10]=-(scale.farMetres+scale.nearMetres)/(scale.farMetres-scale.nearMetres);projection[11]=-1;
    projection[14]=-2*scale.farMetres*scale.nearMetres/(scale.farMetres-scale.nearMetres);
    auto native=spatialIdentity();native[0]=native[5]=native[10]=1/scale.nativeUnitsPerMetre;
    return multiply(projection,multiply(inversePose(eye.pose),multiply(poseMatrix(seat),native)));
}
XrPose xrSeatedOrigin(const XrPose& head) {
    const auto m=poseMatrix(head);
    // Keep gravity vertical. Looking up/down or rolling during recenter must not
    // rotate the tank, ground or cabin; head motion remains a view-only transform.
    const auto horizontal=std::hypot(m[8],m[10]);
    const auto halfYaw=horizontal<0.0001f?std::atan2(head.orientation.y,head.orientation.w):std::atan2(m[8],m[10])*0.5f;
    return {{0,std::sin(halfYaw),0,std::cos(halfYaw)},head.position};
}
XrPose xrRebasedPose(const XrPose& previous,const XrPose& previousFromNew) {
    validate(previous);const auto inverse=inversePose(previousFromNew);
    auto a=previousFromNew.orientation,b=previous.orientation;
    const auto lengthA=std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z+a.w*a.w);
    const auto lengthB=std::sqrt(b.x*b.x+b.y*b.y+b.z*b.z+b.w*b.w);
    a={-a.x/lengthA,-a.y/lengthA,-a.z/lengthA,a.w/lengthA};
    b={b.x/lengthB,b.y/lengthB,b.z/lengthB,b.w/lengthB};
    const auto p=previous.position;
    return {{a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z},
        {inverse[0]*p.x+inverse[4]*p.y+inverse[8]*p.z+inverse[12],
         inverse[1]*p.x+inverse[5]*p.y+inverse[9]*p.z+inverse[13],
         inverse[2]*p.x+inverse[6]*p.y+inverse[10]*p.z+inverse[14]}};
}
}
