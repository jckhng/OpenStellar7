#include "xr_presentation.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace stellar7 {
XrViewScale xrWorldViewScale(float scale) {
    if(!std::isfinite(scale)||scale<=0) {throw std::invalid_argument("Invalid XR world scale");}
    constexpr float nativeFarPlane=2000000;
    auto result=XrViewScale{scale};result.farMetres=std::max(result.farMetres,nativeFarPlane/scale);return result;
}
XrPresentation xrWorld(const SpatialScene& source,const IndexedScreen& indexed,EnhancedView view,const SpatialOptions& options,float scale) {
    xrWorldViewScale(scale);
    constexpr float authoredCabinUnitsPerMetre=1000;
    const SpatialCockpitLayout layout{scale/authoredCabinUnitsPerMetre,0};
    return {makeSpatialView(source,indexed,view,options,layout),indexed.palette};
}
XrPresentation xrPanel(const IndexedScreen& indexed,float scale) {
    if(indexed.pixels.size()!=64000||indexed.palette.size()!=256||!std::isfinite(scale)||scale<=0) {throw std::invalid_argument("Invalid XR panel frame/scale");}
    constexpr float originalDisplayAspect=4.0f/3,widthMetres=1.4f,heightMetres=widthMetres/originalDisplayAspect,depthMetres=1.8f;
    const auto halfWidth=widthMetres*scale/2,halfHeight=heightMetres*scale/2,depth=depthMetres*scale;
    XrPresentation result;result.palette=indexed.palette;result.scene.background.assign(64000,0);result.scene.overlay.assign(64000,-1);
    SpatialBillboard image;image.image={320,200,indexed.pixels};image.alpha=BillboardAlpha::opaque;image.depth=BillboardDepth::write;
    image.corners=std::array<SpatialPoint,4>{{{-halfWidth,depth,halfHeight},{-halfWidth,depth,-halfHeight},
        {halfWidth,depth,-halfHeight},{halfWidth,depth,halfHeight}}};
    result.scene.billboards.push_back(std::move(image));return result;
}
IndexedScreen xrOptionsScreen(const std::vector<std::uint32_t>& argb) {
    if(argb.size()!=64000) {throw std::invalid_argument("Invalid XR native-options frame");}
    // The authored native options UI is ARGB. Original indexed menu/story frames
    // bypass this conversion and preserve their palette exactly.
    IndexedScreen result;result.palette.resize(256);result.pixels.reserve(argb.size());
    for(unsigned i=0;i<256;++i) {result.palette[i]={std::uint8_t(((i>>5)&7)*255/7),std::uint8_t(((i>>2)&7)*255/7),std::uint8_t((i&3)*255/3)};}
    for(const auto pixel:argb) {
        const auto red=(((pixel>>16)&255)*7+127)/255,green=(((pixel>>8)&255)*7+127)/255,blue=((pixel&255)*3+127)/255;
        result.pixels.push_back(std::uint8_t((red<<5)|(green<<2)|blue));
    }
    return result;
}
void XrPresentationMailbox::publish(std::shared_ptr<const XrPresentation> value) {const std::lock_guard<std::mutex> lock(mutex_);latest_=std::move(value);}
std::shared_ptr<const XrPresentation> XrPresentationMailbox::latest() const {const std::lock_guard<std::mutex> lock(mutex_);return latest_;}
}
