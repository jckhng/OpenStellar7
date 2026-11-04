#include "gl_sdl_presenter.hpp"
#include "enhanced_diagnostics.hpp"
#include <algorithm>

namespace stellar7 {
SDL_Window* GlSdlPresenter::createWindow(int width,int height,GlProfile profile) {
    const std::array<std::pair<SDL_GLattr,int>,6> attributes{{
        {SDL_GL_CONTEXT_PROFILE_MASK,profile==GlProfile::desktop33?SDL_GL_CONTEXT_PROFILE_CORE:SDL_GL_CONTEXT_PROFILE_ES},
        {SDL_GL_CONTEXT_MAJOR_VERSION,3},{SDL_GL_CONTEXT_MINOR_VERSION,profile==GlProfile::desktop33?3:0},
        {SDL_GL_DOUBLEBUFFER,1},{SDL_GL_DEPTH_SIZE,24},{SDL_GL_STENCIL_SIZE,0}}};
    for(const auto& attribute:attributes) {sdlHost::check(SDL_GL_SetAttribute(attribute.first,attribute.second),"SDL_GL_SetAttribute");}
    return SDL_CreateWindow("Stellar 7 Enhanced GL",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,width,height,
        SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
}
GlSdlPresenter::GlSdlPresenter(SDL_Window* window,GlProfile profile):window_(window) {
    context_.reset(SDL_GL_CreateContext(window));
    if(!context_) {throw std::runtime_error(std::string("SDL_GL_CreateContext: ")+SDL_GetError());}
    current();renderer_=std::make_unique<GlSceneRenderer>(SDL_GL_GetProcAddress,profile);
    enhancedHostTrace(renderer_->identity().c_str());
    if(SDL_GL_SetSwapInterval(0)!=0) {enhancedHostTrace("GL swap interval0 unavailable; host may block on swap");}
}
void GlSdlPresenter::current() {
    sdlHost::check(SDL_GL_MakeCurrent(window_,context_.get()),"SDL_GL_MakeCurrent");
}
void GlSdlPresenter::sampling(GlSampling value) {renderer_->sampling(value);}
void GlSdlPresenter::scene(std::shared_ptr<const SpatialScene> source,const IndexedScreen& indexed,const EnhancedView view,
    const SpatialOptions& options,const GlRenderTarget& target,unsigned scale) {
    if(scale!=1&&scale!=2&&scale!=4) {throw std::invalid_argument("Unsupported GL world resolution");}
    current();
    if(prepared_!=source||palette_!=indexed.palette||view_!=view||!(options_==options)
        ||(view==EnhancedView::cockpit&&instruments_!=indexed.pixels)) {
        const auto scene=makeSpatialView(*source,indexed,view,options);renderer_->prepare(scene,indexed.palette);
        camera_=scene.camera;prepared_=source;palette_=indexed.palette;view_=view;options_=options;
        if(view==EnhancedView::cockpit) {instruments_=indexed.pixels;}
    }
    renderer_->renderScaled(spatialProjection(camera_,double(target.width)/target.height),target,320*scale,200*scale);
}
void GlSdlPresenter::bitmap(const std::vector<std::uint32_t>& pixels,int width,int height,const GlRenderTarget& target) {
    current();prepared_.reset();renderer_->bitmap(pixels,width,height,target);
}
void GlSdlPresenter::swap() {SDL_GL_SwapWindow(window_);}
}
