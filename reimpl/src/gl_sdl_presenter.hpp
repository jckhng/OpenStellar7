#pragma once
#include "sdl_host.hpp"
#include "gl_scene_renderer.hpp"
#include "spatial_view.hpp"

namespace stellar7 {
class GlSdlPresenter {
public:
    static SDL_Window* createWindow(int width,int height,GlProfile profile);
    GlSdlPresenter(SDL_Window* window,GlProfile profile);
    void scene(std::shared_ptr<const SpatialScene> source,const IndexedScreen& indexed,EnhancedView view,
        const SpatialOptions& options,const GlRenderTarget& target,unsigned scale);
    void bitmap(const std::vector<std::uint32_t>& pixels,int width,int height,const GlRenderTarget& target);
    void sampling(GlSampling value);
    void swap();
private:
    void current();
    SDL_Window* window_;
    std::unique_ptr<void,decltype(&SDL_GL_DeleteContext)> context_{nullptr,SDL_GL_DeleteContext};
    std::unique_ptr<GlSceneRenderer> renderer_;
    std::shared_ptr<const SpatialScene> prepared_;
    std::vector<Rgb> palette_;
    std::vector<std::uint8_t> instruments_;
    EnhancedView view_=EnhancedView::cockpit;
    SpatialOptions options_;
    ViewTransform camera_{};
};
}
