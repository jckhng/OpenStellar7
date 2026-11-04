#include "gl_scene_renderer.hpp"
#include <SDL_opengl.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace stellar7 {
namespace {
constexpr int sourceWidth=320,sourceHeight=200;
constexpr int maximumTextureDimension=16384;
constexpr float groundDepthBiasFactor=1,groundDepthBiasUnits=1;
constexpr std::size_t maximumVertices=4*1024*1024;
#define GL_PROCEDURES(X) \
    X(void,GenTextures,(GLsizei,GLuint*)) X(void,DeleteTextures,(GLsizei,const GLuint*)) \
    X(void,BindTexture,(GLenum,GLuint)) X(void,TexParameteri,(GLenum,GLenum,GLint)) \
    X(void,TexImage2D,(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*)) \
    X(void,PixelStorei,(GLenum,GLint)) X(void,ActiveTexture,(GLenum)) \
    X(GLuint,CreateShader,(GLenum)) X(void,ShaderSource,(GLuint,GLsizei,const GLchar* const*,const GLint*)) \
    X(void,CompileShader,(GLuint)) X(void,GetShaderiv,(GLuint,GLenum,GLint*)) \
    X(void,GetShaderInfoLog,(GLuint,GLsizei,GLsizei*,GLchar*)) X(void,DeleteShader,(GLuint)) \
    X(GLuint,CreateProgram,()) X(void,AttachShader,(GLuint,GLuint)) X(void,LinkProgram,(GLuint)) \
    X(void,GetProgramiv,(GLuint,GLenum,GLint*)) X(void,GetProgramInfoLog,(GLuint,GLsizei,GLsizei*,GLchar*)) \
    X(void,DeleteProgram,(GLuint)) X(void,UseProgram,(GLuint)) X(GLint,GetUniformLocation,(GLuint,const GLchar*)) \
    X(void,UniformMatrix4fv,(GLint,GLsizei,GLboolean,const GLfloat*)) X(void,Uniform1i,(GLint,GLint)) X(void,Uniform4fv,(GLint,GLsizei,const GLfloat*)) \
    X(void,GenVertexArrays,(GLsizei,GLuint*)) X(void,DeleteVertexArrays,(GLsizei,const GLuint*)) \
    X(void,BindVertexArray,(GLuint)) X(void,GenBuffers,(GLsizei,GLuint*)) X(void,DeleteBuffers,(GLsizei,const GLuint*)) \
    X(void,BindBuffer,(GLenum,GLuint)) X(void,BufferData,(GLenum,GLsizeiptr,const void*,GLenum)) \
    X(void,EnableVertexAttribArray,(GLuint)) X(void,VertexAttribPointer,(GLuint,GLint,GLenum,GLboolean,GLsizei,const void*)) \
    X(void,Viewport,(GLint,GLint,GLsizei,GLsizei)) X(void,ClearColor,(GLfloat,GLfloat,GLfloat,GLfloat)) \
    X(void,Clear,(GLbitfield)) X(void,Enable,(GLenum)) X(void,Disable,(GLenum)) X(void,PolygonOffset,(GLfloat,GLfloat)) \
    X(void,BlendFunc,(GLenum,GLenum)) X(void,DepthFunc,(GLenum)) X(void,DepthMask,(GLboolean)) \
    X(void,ColorMask,(GLboolean,GLboolean,GLboolean,GLboolean)) X(void,BindFramebuffer,(GLenum,GLuint)) \
    X(void,GenFramebuffers,(GLsizei,GLuint*)) X(void,DeleteFramebuffers,(GLsizei,const GLuint*)) \
    X(void,FramebufferTexture2D,(GLenum,GLenum,GLenum,GLuint,GLint)) X(GLenum,CheckFramebufferStatus,(GLenum)) \
    X(void,GenRenderbuffers,(GLsizei,GLuint*)) X(void,DeleteRenderbuffers,(GLsizei,const GLuint*)) \
    X(void,BindRenderbuffer,(GLenum,GLuint)) X(void,RenderbufferStorage,(GLenum,GLenum,GLsizei,GLsizei)) \
    X(void,FramebufferRenderbuffer,(GLenum,GLenum,GLenum,GLuint)) \
    X(void,DrawArrays,(GLenum,GLint,GLsizei)) X(GLenum,GetError,()) X(void,GetIntegerv,(GLenum,GLint*)) \
    X(const GLubyte*,GetString,(GLenum)) X(void,Flush,())
struct Range {GLint first=0;GLsizei count=0;GLenum primitive=GL_TRIANGLES;};
enum class TextureOrigin {top,bottom};
const std::array<SpatialVertex,6> screenQuad{{
    {-1,1,0,1,1,1,1,0,0},{-1,-1,0,1,1,1,1,0,1},{1,-1,0,1,1,1,1,1,1},
    {-1,1,0,1,1,1,1,0,0},{1,-1,0,1,1,1,1,1,1},{1,1,0,1,1,1,1,1,0}}};
void dimensions(int width,int height) {
    if(width<=0||height<=0||width>maximumTextureDimension||height>maximumTextureDimension) {throw std::invalid_argument("Invalid GL target/texture dimensions");}
}
std::vector<std::uint8_t> indexedRgba(const std::vector<std::uint8_t>& pixels,const std::vector<Rgb>& palette,bool transparent) {
    std::vector<std::uint8_t> result;result.reserve(pixels.size()*4);
    for(const auto p:pixels) {const auto c=palette[p];result.insert(result.end(),{c.red,c.green,c.blue,static_cast<std::uint8_t>(transparent&&p==0?0:255)});}
    return result;
}
}
struct GlSceneRenderer::Impl {
#define DECLARE_GL(result,name,args) using name##Proc=result(APIENTRY*)args;name##Proc name=nullptr;
    GL_PROCEDURES(DECLARE_GL)
#undef DECLARE_GL
    using ClearDepthProc=void(APIENTRY*)(GLdouble);
    using ClearDepthfProc=void(APIENTRY*)(GLfloat);
    ClearDepthProc ClearDepth=nullptr;ClearDepthfProc ClearDepthf=nullptr;
    GlProfile profile;
    std::string description;
    GLuint program=0,vao=0,vbo=0,background=0,overlay=0;
    GLuint scaledFramebuffer=0,scaledColor=0,scaledDepth=0;
    int scaledWidth=0,scaledHeight=0;
    GLuint borrowedFramebuffer=0,borrowedDepth=0;
    int borrowedWidth=0,borrowedHeight=0;
    GLint matrixLocation=-1,textureLocation=-1,flipLocation=-1,lightCountLocation=-1,lightPositionsLocation=-1,lightColorsLocation=-1;
    std::array<Range,8> ranges{};Range quad;
    std::vector<GLuint> billboardTextures;
    std::vector<Range> billboardRanges;
    std::vector<BillboardDepth> billboardDepths;
    bool prepared=false;
    GLint sampling=GL_NEAREST;
    Impl(ProcAddress loader,GlProfile selected):profile(selected) {
        if(!loader) {throw std::invalid_argument("GL procedure loader is required");}
#define LOAD_GL(result,name,args) name=reinterpret_cast<name##Proc>(loader("gl" #name));if(!name) {throw std::runtime_error("Missing GL procedure gl" #name);}
        GL_PROCEDURES(LOAD_GL)
#undef LOAD_GL
        if(profile==GlProfile::desktop33) {ClearDepth=reinterpret_cast<ClearDepthProc>(loader("glClearDepth"));}
        else {ClearDepthf=reinterpret_cast<ClearDepthfProc>(loader("glClearDepthf"));}
        if(!ClearDepth&&!ClearDepthf) {throw std::runtime_error("Missing GL depth-clear procedure");}
    }
    ~Impl() {
        if(borrowedFramebuffer) {DeleteFramebuffers(1,&borrowedFramebuffer);}
        if(borrowedDepth) {DeleteRenderbuffers(1,&borrowedDepth);}
        if(scaledFramebuffer) {DeleteFramebuffers(1,&scaledFramebuffer);}
        if(scaledColor) {DeleteTextures(1,&scaledColor);}
        if(scaledDepth) {DeleteRenderbuffers(1,&scaledDepth);}
        if(!billboardTextures.empty()) {DeleteTextures(static_cast<GLsizei>(billboardTextures.size()),billboardTextures.data());}
        if(background) {DeleteTextures(1,&background);}
        if(overlay) {DeleteTextures(1,&overlay);}
        if(vbo) {DeleteBuffers(1,&vbo);}
        if(vao) {DeleteVertexArrays(1,&vao);}
        if(program) {DeleteProgram(program);}
    }
    void check(const char* operation) {
        const auto error=GetError();if(error!=GL_NO_ERROR) {throw std::runtime_error(std::string(operation)+" GL error "+std::to_string(error));}
    }
    GLuint shader(GLenum type,const std::string& text) {
        const auto object=CreateShader(type);const char* source=text.c_str();ShaderSource(object,1,&source,nullptr);CompileShader(object);
        GLint compiled=0;GetShaderiv(object,GL_COMPILE_STATUS,&compiled);
        if(!compiled) {
            std::array<char,4096> log{};GetShaderInfoLog(object,static_cast<GLsizei>(log.size()),nullptr,log.data());DeleteShader(object);
            throw std::runtime_error(std::string("GL shader compilation: ")+log.data());
        }
        return object;
    }
    void initialize() {
        GLint major=0,minor=0;GetIntegerv(GL_MAJOR_VERSION,&major);GetIntegerv(GL_MINOR_VERSION,&minor);
        if(major<3||(profile==GlProfile::desktop33&&major==3&&minor<3)) {throw std::runtime_error("GL backend requires desktop3.3 or GLES3.0");}
        const auto text=[&](GLenum name) {const auto* value=GetString(name);return value?reinterpret_cast<const char*>(value):"unknown";};
        description=std::string(text(GL_VERSION))+" / "+text(GL_RENDERER);
        const std::string version=profile==GlProfile::desktop33?"#version 330 core\n":"#version 300 es\nprecision highp float;\n";
        const std::string vertex=version+R"(
layout(location=0) in vec3 position;
layout(location=1) in vec4 color;
layout(location=2) in vec2 uv;
layout(location=3) in vec3 normal;
uniform mat4 transform;
out vec4 ink;
out vec2 texcoord;
out vec3 cameraPosition;
out vec3 faceNormal;
void main() {gl_Position=transform*vec4(position,1.0);gl_PointSize=1.0;ink=color;texcoord=uv;cameraPosition=position;faceNormal=normal;}
)";
        const std::string fragment=version+R"(
in vec4 ink;
in vec2 texcoord;
in vec3 cameraPosition;
in vec3 faceNormal;
uniform sampler2D image;
uniform int textured;
uniform int flipped;
uniform int lightCount;
uniform vec4 lightPositionRadius[4];
uniform vec4 lightColorStrength[4];
out vec4 outputColor;
void main() {
    outputColor=ink;
    if(textured==1) {vec2 uv=texcoord;if(flipped!=0) {uv.y=1.0-uv.y;}outputColor*=texture(image,uv);}
    if(textured==2) {
        float footprint=max(fwidth(texcoord.x),fwidth(texcoord.y));
        vec2 edge=min(fract(texcoord),1.0-fract(texcoord));
        float seam=1.0-smoothstep(0.018,0.018+max(footprint,0.00001),min(edge.x,edge.y));
        float variation=mod(dot(floor(texcoord),vec2(37.0,53.0)),17.0)/16.0-0.5;
        float fade=1.0-smoothstep(0.08,0.3,footprint);
        outputColor.rgb*=1.0+fade*(variation*0.12-seam*0.08);
    }
    float nlength=length(faceNormal);
    if(nlength>0.0) {
        vec3 n=faceNormal/nlength;vec3 extra=vec3(0.0);
        for(int i=0;i<4;++i) {if(i<lightCount) {
            vec3 delta=lightPositionRadius[i].xyz-cameraPosition;
            float distanceToLight=length(delta);
            float falloff=max(0.0,1.0-distanceToLight/lightPositionRadius[i].w);
            float diffuse=max(0.0,dot(n,delta/max(distanceToLight,0.001)));
            extra+=lightColorStrength[i].rgb*lightColorStrength[i].w*falloff*falloff*diffuse;
        }}
        outputColor.rgb*=1.0+min(extra,vec3(0.45));
    }
    if(outputColor.a==0.0) {discard;}
}
)";
        program=CreateProgram();
        for(const auto type:{GL_VERTEX_SHADER,GL_FRAGMENT_SHADER}) {
            const auto object=shader(type,type==GL_VERTEX_SHADER?vertex:fragment);AttachShader(program,object);DeleteShader(object);
        }
        LinkProgram(program);GLint linked=0;GetProgramiv(program,GL_LINK_STATUS,&linked);
        if(!linked) {std::array<char,4096> log{};GetProgramInfoLog(program,static_cast<GLsizei>(log.size()),nullptr,log.data());throw std::runtime_error(std::string("GL program link: ")+log.data());}
        matrixLocation=GetUniformLocation(program,"transform");textureLocation=GetUniformLocation(program,"textured");
        flipLocation=GetUniformLocation(program,"flipped");
        lightCountLocation=GetUniformLocation(program,"lightCount");lightPositionsLocation=GetUniformLocation(program,"lightPositionRadius[0]");
        lightColorsLocation=GetUniformLocation(program,"lightColorStrength[0]");
        if(matrixLocation<0||textureLocation<0||flipLocation<0||lightCountLocation<0||lightPositionsLocation<0||lightColorsLocation<0) {throw std::runtime_error("GL shader interface missing");}
        UseProgram(program);Uniform1i(GetUniformLocation(program,"image"),0);
        GenVertexArrays(1,&vao);GenBuffers(1,&vbo);BindVertexArray(vao);BindBuffer(GL_ARRAY_BUFFER,vbo);
        for(GLuint i=0;i<4;++i) {EnableVertexAttribArray(i);}
        VertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(SpatialVertex),reinterpret_cast<void*>(offsetof(SpatialVertex,x)));
        VertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(SpatialVertex),reinterpret_cast<void*>(offsetof(SpatialVertex,r)));
        VertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(SpatialVertex),reinterpret_cast<void*>(offsetof(SpatialVertex,u)));
        VertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,sizeof(SpatialVertex),reinterpret_cast<void*>(offsetof(SpatialVertex,nx)));
        GenTextures(1,&background);GenTextures(1,&overlay);check("GL initialization");
    }
    void texture(GLuint name,int width,int height,const std::vector<std::uint8_t>& pixels) {
        dimensions(width,height);
        if(pixels.size()!=static_cast<std::size_t>(width)*height*4) {throw std::invalid_argument("GL texture buffer mismatch");}
        ActiveTexture(GL_TEXTURE0);BindTexture(GL_TEXTURE_2D,name);PixelStorei(GL_UNPACK_ALIGNMENT,1);
        TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,sampling);TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,sampling);
        TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        TexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    }
    void vertices(const std::vector<SpatialVertex>& data) {
        if(data.size()>maximumVertices) {throw std::invalid_argument("Spatial vertex budget exceeded");}
        BindVertexArray(vao);BindBuffer(GL_ARRAY_BUFFER,vbo);
        BufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(data.size()*sizeof(SpatialVertex)),data.data(),GL_STREAM_DRAW);
    }
    void begin(const GlRenderTarget& target) {
        dimensions(target.width,target.height);
        if(target.x<0||target.y<0||std::int64_t{target.x}+target.width>std::numeric_limits<GLint>::max()
            ||std::int64_t{target.y}+target.height>std::numeric_limits<GLint>::max()) {throw std::invalid_argument("Invalid GL viewport offset");}
        BindFramebuffer(GL_FRAMEBUFFER,target.framebuffer);Viewport(target.x,target.y,target.width,target.height);
        Disable(GL_SCISSOR_TEST);Disable(GL_CULL_FACE);Disable(GL_STENCIL_TEST);Disable(GL_DITHER);Disable(GL_POLYGON_OFFSET_FILL);
        if(profile==GlProfile::desktop33) {Disable(GL_FRAMEBUFFER_SRGB);}
        ColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);DepthMask(GL_TRUE);
        if(ClearDepth) {ClearDepth(1);}else {ClearDepthf(1);}
        ClearColor(0,0,0,1);Clear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        UseProgram(program);BindVertexArray(vao);ActiveTexture(GL_TEXTURE0);
        BlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);DepthFunc(GL_LEQUAL);
    }
    void draw(Range range) {if(range.count) {DrawArrays(range.primitive,range.first,range.count);}}
    void image(GLuint textureName,Range range,TextureOrigin origin=TextureOrigin::top) {
        Uniform1i(textureLocation,1);BindTexture(GL_TEXTURE_2D,textureName);
        Uniform1i(flipLocation,origin==TextureOrigin::bottom?1:0);
        TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,sampling);TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,sampling);draw(range);
    }
};
GlSceneRenderer::GlSceneRenderer(ProcAddress loader,GlProfile profile):impl_(std::make_unique<Impl>(loader,profile)) {impl_->initialize();}
GlSceneRenderer::~GlSceneRenderer()=default;
const std::string& GlSceneRenderer::identity() const {return impl_->description;}
void GlSceneRenderer::sampling(GlSampling value) {impl_->sampling=value==GlSampling::nearest?GL_NEAREST:GL_LINEAR;}
void GlSceneRenderer::prepare(const SpatialScene& scene,const std::vector<Rgb>& palette) {
    auto& g=*impl_;g.prepared=false;
    if(scene.background.size()!=sourceWidth*sourceHeight||scene.overlay.size()!=scene.background.size()||palette.size()!=256) {throw std::invalid_argument("Incomplete spatial scene layers");}
    const auto geometry=spatialGeometry(scene,palette);std::vector<SpatialVertex> data;
    const std::array<const std::vector<SpatialVertex>*,8> groups{&geometry.triangles,&geometry.lines,&geometry.points,
        &geometry.translucentTriangles,&geometry.translucentLines,&geometry.translucentPoints,&geometry.groundTriangles,&geometry.plainGroundTriangles};
    for(std::size_t i=0;i<groups.size();++i) {
        g.ranges[i]={static_cast<GLint>(data.size()),static_cast<GLsizei>(groups[i]->size()),static_cast<GLenum>(i>=6||i%3==0?GL_TRIANGLES:i%3==1?GL_LINES:GL_POINTS)};
        data.insert(data.end(),groups[i]->begin(),groups[i]->end());
    }
    g.texture(g.background,sourceWidth,sourceHeight,indexedRgba(scene.background,palette,false));
    std::vector<std::uint8_t> overlay;overlay.reserve(scene.overlay.size()*4);
    for(const auto p:scene.overlay) {
        if(p< -1||p>255) {throw std::invalid_argument("Spatial overlay palette index invalid");}
        const auto c=p<0?Rgb{}:palette[p];overlay.insert(overlay.end(),{c.red,c.green,c.blue,static_cast<std::uint8_t>(p<0?0:255)});
    }
    g.texture(g.overlay,sourceWidth,sourceHeight,overlay);
    if(scene.lights.size()>4) {throw std::invalid_argument("Spatial local-light budget exceeded");}
    std::vector<float> positions,colors;
    for(const auto& light:scene.lights) {
        if(!std::isfinite(light.position.x)||!std::isfinite(light.position.y)||!std::isfinite(light.position.z)
            ||!std::isfinite(light.radius)||!std::isfinite(light.strength)||light.radius<=0||light.strength<0||light.strength>1) {throw std::invalid_argument("Invalid spatial local light");}
        positions.insert(positions.end(),{light.position.x,light.position.z,-light.position.y,light.radius});
        colors.insert(colors.end(),{light.color.red/255.0f,light.color.green/255.0f,light.color.blue/255.0f,light.strength});
    }
    g.UseProgram(g.program);g.Uniform1i(g.lightCountLocation,static_cast<GLint>(scene.lights.size()));
    if(!scene.lights.empty()) {
        g.Uniform4fv(g.lightPositionsLocation,static_cast<GLsizei>(scene.lights.size()),positions.data());
        g.Uniform4fv(g.lightColorsLocation,static_cast<GLsizei>(scene.lights.size()),colors.data());
    }
    if(!g.billboardTextures.empty()) {g.DeleteTextures(static_cast<GLsizei>(g.billboardTextures.size()),g.billboardTextures.data());}
    g.billboardTextures.assign(scene.billboards.size(),0);g.billboardRanges.clear();g.billboardDepths.clear();
    if(!g.billboardTextures.empty()) {g.GenTextures(static_cast<GLsizei>(g.billboardTextures.size()),g.billboardTextures.data());}
    for(std::size_t i=0;i<scene.billboards.size();++i) {
        const auto& b=scene.billboards[i];dimensions(b.image.width,b.image.height);
        if(!std::isfinite(b.center.x)||!std::isfinite(b.center.y)||!std::isfinite(b.center.z)
            ||(!b.corners&&(!std::isfinite(b.width)||!std::isfinite(b.height)||b.width<=0||b.height<=0))
            ||b.image.pixels.size()!=static_cast<std::size_t>(b.image.width)*b.image.height) {throw std::invalid_argument("Invalid spatial billboard");}
        g.texture(g.billboardTextures[i],b.image.width,b.image.height,indexedRgba(b.image.pixels,palette,b.alpha==BillboardAlpha::colorZero));
        g.billboardRanges.push_back({static_cast<GLint>(data.size()),6,GL_TRIANGLES});
        g.billboardDepths.push_back(b.depth);
        constexpr std::array<std::size_t,6> corners{0,1,2,0,2,3};
        for(std::size_t vertex=0;vertex<screenQuad.size();++vertex) {
            auto v=screenQuad[vertex];
            if(b.corners) {
                const auto p=(*b.corners)[corners[vertex]];
                if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)) {throw std::invalid_argument("Non-finite spatial panel vertex");}
                v.x=p.x;v.y=p.z;v.z=-p.y;
            } else {v.x=b.center.x+v.x*b.width/2;v.y=b.center.z+v.y*b.height/2;v.z=-b.center.y;}
            data.push_back(v);
        }
    }
    g.quad={static_cast<GLint>(data.size()),6,GL_TRIANGLES};data.insert(data.end(),screenQuad.begin(),screenQuad.end());
    g.vertices(data);g.check("GL scene upload");g.prepared=true;
}
void GlSceneRenderer::render(const SpatialMatrix& matrix,const GlRenderTarget& target,const SpatialOverlay overlay) {
    auto& g=*impl_;
    if(!g.prepared) {throw std::logic_error("GL scene must be prepared before rendering views");}
    if(std::any_of(matrix.begin(),matrix.end(),[](float value) {return !std::isfinite(value);})) {throw std::invalid_argument("Non-finite spatial matrix");}
    g.begin(target);const auto identity=spatialIdentity();g.UniformMatrix4fv(g.matrixLocation,1,GL_FALSE,identity.data());
    g.Disable(GL_DEPTH_TEST);g.Disable(GL_BLEND);g.image(g.background,g.quad);
    g.Enable(GL_DEPTH_TEST);g.UniformMatrix4fv(g.matrixLocation,1,GL_FALSE,matrix.data());
    // Keep rasterized ground-contact lines visible without moving world geometry.
    g.Enable(GL_POLYGON_OFFSET_FILL);g.PolygonOffset(groundDepthBiasFactor,groundDepthBiasUnits);
    g.Uniform1i(g.textureLocation,2);g.draw(g.ranges[6]);g.Uniform1i(g.textureLocation,0);
    g.draw(g.ranges[7]);
    g.Disable(GL_POLYGON_OFFSET_FILL);
    for(std::size_t i=0;i<3;++i) {g.draw(g.ranges[i]);}
    for(std::size_t i=0;i<g.billboardRanges.size();++i) {if(g.billboardDepths[i]==BillboardDepth::write) {g.image(g.billboardTextures[i],g.billboardRanges[i]);}}
    g.DepthMask(GL_FALSE);g.Enable(GL_BLEND);
    g.Uniform1i(g.textureLocation,0);
    for(std::size_t i=3;i<6;++i) {g.draw(g.ranges[i]);}
    for(std::size_t i=0;i<g.billboardRanges.size();++i) {if(g.billboardDepths[i]==BillboardDepth::testOnly) {g.image(g.billboardTextures[i],g.billboardRanges[i]);}}
    if(overlay==SpatialOverlay::include) {
        g.Disable(GL_DEPTH_TEST);g.UniformMatrix4fv(g.matrixLocation,1,GL_FALSE,identity.data());g.image(g.overlay,g.quad);
    }
    g.DepthMask(GL_TRUE);g.check("GL scene view");
}
void GlSceneRenderer::renderScaled(const SpatialMatrix& matrix,const GlRenderTarget& target,int width,int height,const SpatialOverlay overlay) {
    dimensions(width,height);auto& g=*impl_;
    if(!g.scaledFramebuffer) {g.GenFramebuffers(1,&g.scaledFramebuffer);g.GenTextures(1,&g.scaledColor);g.GenRenderbuffers(1,&g.scaledDepth);}
    if(width!=g.scaledWidth||height!=g.scaledHeight) {
        g.BindTexture(GL_TEXTURE_2D,g.scaledColor);
        g.TexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        g.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);g.TexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        g.BindRenderbuffer(GL_RENDERBUFFER,g.scaledDepth);g.RenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);
        g.BindFramebuffer(GL_FRAMEBUFFER,g.scaledFramebuffer);
        g.FramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,g.scaledColor,0);
        g.FramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,g.scaledDepth);
        if(g.CheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) {throw std::runtime_error("GL world-resolution framebuffer incomplete");}
        g.scaledWidth=width;g.scaledHeight=height;
    }
    render(matrix,{g.scaledFramebuffer,0,0,width,height},overlay);
    g.begin(target);const auto identity=spatialIdentity();g.UniformMatrix4fv(g.matrixLocation,1,GL_FALSE,identity.data());
    g.Disable(GL_DEPTH_TEST);g.Disable(GL_BLEND);g.image(g.scaledColor,g.quad,TextureOrigin::bottom);g.check("GL world-resolution compose");
}
void GlSceneRenderer::renderTexture(const SpatialMatrix& matrix,std::uint32_t texture,int width,int height,SpatialOverlay overlay) {
    dimensions(width,height);
    if(!texture) {throw std::invalid_argument("Borrowed GL texture must be nonzero");}
    auto& g=*impl_;
    if(!g.borrowedFramebuffer) {g.GenFramebuffers(1,&g.borrowedFramebuffer);g.GenRenderbuffers(1,&g.borrowedDepth);}
    if(width!=g.borrowedWidth||height!=g.borrowedHeight) {
        g.BindRenderbuffer(GL_RENDERBUFFER,g.borrowedDepth);g.RenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);
        g.borrowedWidth=width;g.borrowedHeight=height;
    }
    g.BindFramebuffer(GL_FRAMEBUFFER,g.borrowedFramebuffer);
    g.FramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
    g.FramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,g.borrowedDepth);
    const auto detach=[&] {
        g.BindFramebuffer(GL_FRAMEBUFFER,g.borrowedFramebuffer);
        g.FramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,0,0);
        g.BindFramebuffer(GL_FRAMEBUFFER,0);g.Flush();
    };
    try {
        if(g.CheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) {throw std::runtime_error("Borrowed GL texture framebuffer incomplete");}
        render(matrix,{g.borrowedFramebuffer,0,0,width,height},overlay);detach();g.check("GL borrowed texture view");
    } catch(...) {detach();throw;}
}
void GlSceneRenderer::bitmap(const std::vector<std::uint32_t>& pixels,int width,int height,const GlRenderTarget& target) {
    dimensions(width,height);
    if(pixels.size()!=static_cast<std::size_t>(width)*height) {throw std::invalid_argument("GL bitmap size mismatch");}
    auto& g=*impl_;g.prepared=false;std::vector<std::uint8_t> rgba;rgba.reserve(pixels.size()*4);
    for(const auto p:pixels) {rgba.insert(rgba.end(),{static_cast<std::uint8_t>(p>>16),static_cast<std::uint8_t>(p>>8),static_cast<std::uint8_t>(p),255});}
    g.texture(g.background,width,height,rgba);g.vertices({screenQuad.begin(),screenQuad.end()});g.begin(target);
    const auto identity=spatialIdentity();g.UniformMatrix4fv(g.matrixLocation,1,GL_FALSE,identity.data());g.Disable(GL_DEPTH_TEST);g.Disable(GL_BLEND);
    g.image(g.background,{0,6,GL_TRIANGLES});g.check("GL bitmap view");
}
} // namespace stellar7
