#include "xr_view.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace stellar7;
namespace {
void require(bool value,const char* message) {if(!value) {throw std::runtime_error(message);}}
std::array<float,4> transform(const SpatialMatrix& m,std::array<float,4> v) {
    std::array<float,4> result{};
    for(unsigned row=0;row<4;++row) {for(unsigned col=0;col<4;++col) {result[row]+=m[col*4+row]*v[col];}}
    return result;
}
void close(float a,float b,const char* message) {require(std::abs(a-b)<0.0001f,message);}
template<class F> void rejects(F action) {
    bool rejected=false;try {action();} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"invalid XR transform accepted");
}
void check() {
    constexpr float quarterTurn=0.78539816339f;
    const XrViewScale scale{1000};XrView eye{{}, {-quarterTurn,quarterTurn,quarterTurn,-quarterTurn}};
    auto m=xrClipFromNative(eye,{},scale);
    auto p=transform(m,{0,0,-1000,1});close(p[0],0,"neutral horizontal axis");close(p[1],0,"neutral vertical axis");
    close(p[3],1,"native units were not converted to metres");
    const auto near=transform(m,{0,0,-50,1}),far=transform(m,{0,0,-500000,1});
    close(near[2]/near[3],-1,"OpenGL near clip");close(far[2]/far[3],1,"OpenGL far clip");
    eye.pose.position.x=-0.032f;const auto left=transform(xrClipFromNative(eye,{},scale),{0,0,-1000,1});
    eye.pose.position.x=0.032f;const auto right=transform(xrClipFromNative(eye,{},scale),{0,0,-1000,1});
    require(left[0]>right[0],"eye separation has reversed stereo disparity");
    eye.pose.position={0.2f,1.6f,-0.4f};const auto origin=xrSeatedOrigin(eye.pose);
    p=transform(xrClipFromNative(eye,origin,scale),{0,0,-1000,1});close(p[0],0,"seated recenter translation");close(p[1],0,"seated height recenter");
    const float halfYaw=0.4f;eye.pose.orientation={0,std::sin(halfYaw),0,std::cos(halfYaw)};
    p=transform(xrClipFromNative(eye,xrSeatedOrigin(eye.pose),scale),{0,0,-1000,1});close(p[0],0,"yaw recenter");
    // Recenter removes yaw but preserves the physical head pitch/roll.
    eye.pose.orientation={std::sin(0.2f),0,0,std::cos(0.2f)};
    const auto seated=xrSeatedOrigin(eye.pose);close(seated.orientation.x,0,"recenter removed pitch");
    p=transform(xrClipFromNative(eye,seated,scale),{0,0,-1000,1});require(p[1]<-0.3f,"head pitch not applied");
    eye.pose={};eye.fov={-0.4f,0.8f,0.7f,-0.5f};m=xrClipFromNative(eye,{},scale);
    p=transform(m,{1000*std::tan(eye.fov.left),0,-1000,1});close(p[0]/p[3],-1,"asymmetric left edge");
    p=transform(m,{1000*std::tan(eye.fov.right),0,-1000,1});close(p[0]/p[3],1,"asymmetric right edge");
    rejects([&] {xrClipFromNative(eye,{},XrViewScale{0});});
    rejects([&] {xrClipFromNative(eye,{},XrViewScale{1000,1,1});});
    eye.pose.orientation.w=0;rejects([&] {xrClipFromNative(eye,{},scale);});
    eye.pose={};eye.pose.position.x=std::numeric_limits<float>::infinity();rejects([&] {xrClipFromNative(eye,{},scale);});
    eye.pose={};eye.fov.left=eye.fov.right;rejects([&] {xrClipFromNative(eye,{},scale);});
    eye={{{}, {2.1f,1.6f,3}}, {-quarterTurn,quarterTurn,quarterTurn,-quarterTurn}};
    const XrPose seat{{},{2,1.6f,3}},change{{0,std::sin(0.25f),0,std::cos(0.25f)},{0.5f,0,-0.2f}};
    const auto before=xrClipFromNative(eye,seat,scale);
    eye.pose=xrRebasedPose(eye.pose,change);const auto after=xrClipFromNative(eye,xrRebasedPose(seat,change),scale);
    for(unsigned i=0;i<16;++i) {close(before[i],after[i],"reference origin rotation/translation changed physical view");}
    const XrPose vertical{{std::sqrt(0.5f),0,0,std::sqrt(0.5f)},{}};
    close(xrSeatedOrigin(vertical).orientation.x,0,"vertical head pose could not establish a gravity-aligned seat");
    std::cout<<"XR asymmetric stereo/metres/seated yaw/head motion/invalid controls pass\n";
}
}
int main() {try {check();return 0;} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
