#pragma once
#include "spatial_scene.hpp"
#include <memory>
#include <string>

namespace stellar7 {
enum class GlProfile {desktop33,es30};
enum class SpatialOverlay {include,omit};
enum class GlSampling {nearest,linear};
struct GlRenderTarget {
    std::uint32_t framebuffer=0;
    int x=0,y=0,width=0,height=0; // GL bottom-left viewport coordinates.
};
// The caller owns a current context and framebuffers. No window, swap, clock or simulation work.
class GlSceneRenderer {
public:
    using ProcAddress=void*(*)(const char*);
    GlSceneRenderer(ProcAddress loader,GlProfile profile);
    ~GlSceneRenderer();
    GlSceneRenderer(const GlSceneRenderer&)=delete;
    GlSceneRenderer& operator=(const GlSceneRenderer&)=delete;
    void prepare(const SpatialScene& scene,const std::vector<Rgb>& palette);
    void render(const SpatialMatrix& clipFromCamera,const GlRenderTarget& target,SpatialOverlay overlay=SpatialOverlay::include);
    void renderScaled(const SpatialMatrix& clipFromCamera,const GlRenderTarget& target,int width,int height,SpatialOverlay overlay=SpatialOverlay::include);
    // Borrow a single-sample RGBA8 texture; retain no texture ownership after return.
    void renderTexture(const SpatialMatrix& clipFromCamera,std::uint32_t texture,int width,int height,SpatialOverlay overlay=SpatialOverlay::omit);
    void bitmap(const std::vector<std::uint32_t>& pixels,int width,int height,const GlRenderTarget& target);
    const std::string& identity() const;
    void sampling(GlSampling value);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace stellar7
