#include "EditorPreviewRenderer.h"

#include <glad/glad.h>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <functional>
#include <limits>
#include <sstream>

namespace {
struct Vtx { float px, py, pz; float nx, ny, nz; float u, v; };
static const float PI = 3.14159265358979323846f;

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048] = {};
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::cerr << "[AXEditorPreview] shader compile failed: " << log << "\n";
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint linkProgram(const char* vs, const char* fs) {
    GLuint v = compileShader(GL_VERTEX_SHADER, vs);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) return 0;
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048] = {};
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        std::cerr << "[AXEditorPreview] program link failed: " << log << "\n";
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

static std::string lowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}
static bool containsLower(const std::string& s, const char* token) { return lowerCopy(s).find(lowerCopy(token)) != std::string::npos; }
static bool startsLower(const std::string& s, const char* token) { auto a=lowerCopy(s); auto b=lowerCopy(token); return a.rfind(b,0)==0; }

static void matIdentity(float* m) { std::memset(m, 0, sizeof(float)*16); m[0]=m[5]=m[10]=m[15]=1.0f; }
static void matMul(const float* a, const float* b, float* out) {
    float r[16] = {};
    for (int c=0;c<4;++c) for (int row=0;row<4;++row) {
        r[c*4+row] = a[0*4+row]*b[c*4+0] + a[1*4+row]*b[c*4+1] + a[2*4+row]*b[c*4+2] + a[3*4+row]*b[c*4+3];
    }
    std::memcpy(out, r, sizeof(r));
}
static void matTranslate(float x, float y, float z, float* m) { matIdentity(m); m[12]=x; m[13]=y; m[14]=z; }
static void matScale(float x, float y, float z, float* m) { matIdentity(m); m[0]=x; m[5]=y; m[10]=z; }
static void matRotX(float deg, float* m) { matIdentity(m); float r=deg*PI/180.0f,c=std::cos(r),s=std::sin(r); m[5]=c; m[9]=-s; m[6]=s; m[10]=c; }
static void matRotY(float deg, float* m) { matIdentity(m); float r=deg*PI/180.0f,c=std::cos(r),s=std::sin(r); m[0]=c; m[8]=s; m[2]=-s; m[10]=c; }
static void matRotZ(float deg, float* m) { matIdentity(m); float r=deg*PI/180.0f,c=std::cos(r),s=std::sin(r); m[0]=c; m[4]=-s; m[1]=s; m[5]=c; }
static void matTRS(const float* pos, const float* rot, const float* scale, float* out) {
    float T[16], RX[16], RY[16], RZ[16], S[16], RYX[16], RZYX[16], RS[16];
    matTranslate(pos[0],pos[1],pos[2],T); matRotX(rot[0],RX); matRotY(rot[1],RY); matRotZ(rot[2],RZ); matScale(scale[0],scale[1],scale[2],S);
    matMul(RY, RX, RYX); matMul(RZ, RYX, RZYX); matMul(RZYX, S, RS); matMul(T, RS, out);
}
static void makePivotDelta(const float* basePos, float baseYaw, const float* baseScale,
                           const float* livePos, float liveYaw, const float* liveScale,
                           float* out) {
    float deltaPos[3] = {
        livePos[0] - basePos[0],
        livePos[1] - basePos[1],
        livePos[2] - basePos[2]
    };
    float deltaYaw = liveYaw - baseYaw;
    float scaleRatio[3] = {
        baseScale[0] > 0.0001f ? liveScale[0] / baseScale[0] : 1.0f,
        baseScale[1] > 0.0001f ? liveScale[1] / baseScale[1] : 1.0f,
        baseScale[2] > 0.0001f ? liveScale[2] / baseScale[2] : 1.0f
    };

    float toOrigin[16], fromOrigin[16], rotY[16], scaleM[16], tmp1[16], tmp2[16];
    matTranslate(-basePos[0], -basePos[1], -basePos[2], toOrigin);
    matTranslate(basePos[0] + deltaPos[0], basePos[1] + deltaPos[1], basePos[2] + deltaPos[2], fromOrigin);
    matRotY(deltaYaw, rotY);
    matScale(scaleRatio[0], scaleRatio[1], scaleRatio[2], scaleM);
    matMul(rotY, toOrigin, tmp1);
    matMul(scaleM, tmp1, tmp2);
    matMul(fromOrigin, tmp2, out);
}

static bool almostSame3(const float* a, const float* b, float eps = 0.0005f) {
    return std::fabs(a[0] - b[0]) <= eps && std::fabs(a[1] - b[1]) <= eps && std::fabs(a[2] - b[2]) <= eps;
}

static void perspective(float fovyRad, float aspect, float zn, float zf, float* m) {
    std::memset(m,0,sizeof(float)*16); float f=1.0f/std::tan(fovyRad*0.5f); m[0]=f/std::max(aspect,0.001f); m[5]=f; m[10]=(zf+zn)/(zn-zf); m[11]=-1.0f; m[14]=(2.0f*zf*zn)/(zn-zf);
}
static void normalize3(float* v) { float l=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]); if(l>0.00001f){v[0]/=l;v[1]/=l;v[2]/=l;} }
static void cross3(const float* a, const float* b, float* out) { out[0]=a[1]*b[2]-a[2]*b[1]; out[1]=a[2]*b[0]-a[0]*b[2]; out[2]=a[0]*b[1]-a[1]*b[0]; }
static float dot3(const float* a, const float* b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
static void lookAt(const float* eye, const float* center, const float* up, float* m) {
    float f[3] = {center[0]-eye[0], center[1]-eye[1], center[2]-eye[2]}; normalize3(f);
    float s[3]; cross3(f,up,s); normalize3(s); float u[3]; cross3(s,f,u);
    matIdentity(m); m[0]=s[0];m[4]=s[1];m[8]=s[2]; m[1]=u[0];m[5]=u[1];m[9]=u[2]; m[2]=-f[0];m[6]=-f[1];m[10]=-f[2]; m[12]=-dot3(s,eye);m[13]=-dot3(u,eye);m[14]=dot3(f,eye);
}
static size_t componentSize(int componentType) {
    switch(componentType){case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:case TINYGLTF_COMPONENT_TYPE_BYTE:return 1;case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:case TINYGLTF_COMPONENT_TYPE_SHORT:return 2;case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:case TINYGLTF_COMPONENT_TYPE_FLOAT:return 4;default:return 4;}
}
static const unsigned char* accessorPtr(const tinygltf::Model& model, int accessorIndex, size_t index) {
    if(accessorIndex<0||accessorIndex>=(int)model.accessors.size())return nullptr; const auto& acc=model.accessors[accessorIndex];
    if(acc.bufferView<0||acc.bufferView>=(int)model.bufferViews.size())return nullptr; const auto& view=model.bufferViews[acc.bufferView];
    if(view.buffer<0||view.buffer>=(int)model.buffers.size())return nullptr; const auto& buffer=model.buffers[view.buffer];
    size_t stride=view.byteStride?view.byteStride:(size_t)tinygltf::GetNumComponentsInType(acc.type)*componentSize(acc.componentType);
    size_t offset=view.byteOffset+acc.byteOffset+index*stride; if(offset>=buffer.data.size())return nullptr; return buffer.data.data()+offset;
}
static bool readFloat3(const tinygltf::Model& model, int accessorIndex, size_t index, float out[3]) {
    if(accessorIndex<0)return false; const auto& acc=model.accessors[accessorIndex]; if(acc.componentType!=TINYGLTF_COMPONENT_TYPE_FLOAT||acc.type!=TINYGLTF_TYPE_VEC3)return false; const auto* p=accessorPtr(model,accessorIndex,index); if(!p)return false; const float* f=(const float*)p; out[0]=f[0];out[1]=f[1];out[2]=f[2]; return true;
}
static bool readFloat2(const tinygltf::Model& model, int accessorIndex, size_t index, float out[2]) {
    if(accessorIndex<0)return false; const auto& acc=model.accessors[accessorIndex]; if(acc.componentType!=TINYGLTF_COMPONENT_TYPE_FLOAT||acc.type!=TINYGLTF_TYPE_VEC2)return false; const auto* p=accessorPtr(model,accessorIndex,index); if(!p)return false; const float* f=(const float*)p; out[0]=f[0];out[1]=f[1]; return true;
}
static unsigned int readIndex(const tinygltf::Model& model, int accessorIndex, size_t index) {
    const auto& acc=model.accessors[accessorIndex]; const auto* p=accessorPtr(model,accessorIndex,index); if(!p)return 0; switch(acc.componentType){case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:return *(const uint8_t*)p;case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:return *(const uint16_t*)p;case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:return *(const uint32_t*)p;default:return 0;}
}
static void matFromNode(const tinygltf::Node& node, float* out) {
    matIdentity(out); if(node.matrix.size()==16){for(int i=0;i<16;++i)out[i]=(float)node.matrix[i]; return;}
    float t[3]={0,0,0}, q[4]={0,0,0,1}, sc[3]={1,1,1};
    if(node.translation.size()==3){t[0]=(float)node.translation[0];t[1]=(float)node.translation[1];t[2]=(float)node.translation[2];}
    if(node.rotation.size()==4){q[0]=(float)node.rotation[0];q[1]=(float)node.rotation[1];q[2]=(float)node.rotation[2];q[3]=(float)node.rotation[3];}
    if(node.scale.size()==3){sc[0]=(float)node.scale[0];sc[1]=(float)node.scale[1];sc[2]=(float)node.scale[2];}
    float x=q[0],y=q[1],z=q[2],w=q[3],x2=x+x,y2=y+y,z2=z+z; float xx=x*x2,xy=x*y2,xz=x*z2,yy=y*y2,yz=y*z2,zz=z*z2,wx=w*x2,wy=w*y2,wz=w*z2;
    out[0]=(1-(yy+zz))*sc[0]; out[1]=(xy+wz)*sc[0]; out[2]=(xz-wy)*sc[0]; out[3]=0;
    out[4]=(xy-wz)*sc[1]; out[5]=(1-(xx+zz))*sc[1]; out[6]=(yz+wx)*sc[1]; out[7]=0;
    out[8]=(xz+wy)*sc[2]; out[9]=(yz-wx)*sc[2]; out[10]=(1-(xx+yy))*sc[2]; out[11]=0;
    out[12]=t[0]; out[13]=t[1]; out[14]=t[2]; out[15]=1;
}
static void transformPoint(const float* m, const float* in, float* out) { out[0]=m[0]*in[0]+m[4]*in[1]+m[8]*in[2]+m[12]; out[1]=m[1]*in[0]+m[5]*in[1]+m[9]*in[2]+m[13]; out[2]=m[2]*in[0]+m[6]*in[1]+m[10]*in[2]+m[14]; }
static void transformVector(const float* m, const float* in, float* out) { out[0]=m[0]*in[0]+m[4]*in[1]+m[8]*in[2]; out[1]=m[1]*in[0]+m[5]*in[1]+m[9]*in[2]; out[2]=m[2]*in[0]+m[6]*in[1]+m[10]*in[2]; normalize3(out); }
static std::string collectionFor(const std::string& node, const std::string& mesh, const std::string& mat) { std::string all=lowerCopy(node+" "+mesh+" "+mat); if(all.find("collider")!=std::string::npos||all.find("collision")!=std::string::npos)return "Collider"; if(all.find("bloom")!=std::string::npos||all.find("emissive")!=std::string::npos||all.find("glow")!=std::string::npos)return "Bloom"; if(all.rfind("mat_",0)==0||all.find(" mat_")!=std::string::npos||all.find("mat-")!=std::string::npos)return "MAT"; return "Object"; }
static bool markerTypeForNodeName(const std::string& name, std::string& typeOut) { if(startsLower(name,"player_start")||containsLower(name,"playerstart")){typeOut="PlayerStart";return true;} if(startsLower(name,"enemy")||startsLower(name,"ai")){typeOut="Enemy";return true;} if(containsLower(name,"camera")){typeOut="Camera";return true;} if(containsLower(name,"light")||containsLower(name,"sun")){typeOut="Light";return true;} if(startsLower(name,"vfx")){typeOut="VFX";return true;} return false; }
static bool projectPoint(const float* vp, float x, float y, float z, int width, int height, float& outX, float& outY) { float cx=vp[0]*x+vp[4]*y+vp[8]*z+vp[12]; float cy=vp[1]*x+vp[5]*y+vp[9]*z+vp[13]; float cw=vp[3]*x+vp[7]*y+vp[11]*z+vp[15]; if(std::fabs(cw)<0.00001f)return false; float ndcX=cx/cw, ndcY=cy/cw; outX=(ndcX*0.5f+0.5f)*(float)width; outY=(1.0f-(ndcY*0.5f+0.5f))*(float)height; return cw>0.0f&&ndcX>-1.25f&&ndcX<1.25f&&ndcY>-1.25f&&ndcY<1.25f; }
}

EditorPreviewRenderer::~EditorPreviewRenderer(){ destroyModel(); destroyImage(); destroyFramebuffer(); if(program_)glDeleteProgram(program_); if(gridProgram_)glDeleteProgram(gridProgram_); }

bool EditorPreviewRenderer::initialize(){ if(initialized_)return true; if(!makeProgram())return false; initialized_=true; return true; }

bool EditorPreviewRenderer::makeProgram(){
    const char* vs=R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos; layout(location=1) in vec3 aNormal; layout(location=2) in vec2 aUv;
uniform mat4 uMVP; uniform mat4 uModel; out vec3 vNormal; out vec2 vUv; out vec3 vWorld;
void main(){ vec4 w=uModel*vec4(aPos,1.0); vWorld=w.xyz; vNormal=mat3(uModel)*aNormal; vUv=aUv; gl_Position=uMVP*vec4(aPos,1.0); }
)GLSL";
    const char* fs=R"GLSL(
#version 330 core
in vec3 vNormal; in vec2 vUv; in vec3 vWorld; uniform vec4 uBaseColor; uniform sampler2D uTex; uniform int uHasTex;
uniform int uLit; uniform int uCel; uniform int uRim; uniform int uFog; uniform int uBloomPreview; uniform int uSelected; uniform int uCollection;
out vec4 FragColor;
void main(){
    vec3 n=normalize(vNormal); if(length(n)<0.01)n=vec3(0,1,0);
    vec3 sun=normalize(vec3(-0.35,-0.70,0.42)); float ndl=max(dot(n,-sun),0.0);
    if(uCel!=0){ ndl=smoothstep(0.18,0.22,ndl)*0.45 + smoothstep(0.55,0.60,ndl)*0.55; }
    vec4 tex=uHasTex!=0?texture(uTex,vUv):vec4(1.0); vec3 color=uBaseColor.rgb*tex.rgb;
    vec3 lit = (uLit!=0) ? color*(0.30+ndl*1.05) : color;
    if(uRim!=0){ vec3 viewDir=normalize(vec3(0,0,1)); float rim=pow(1.0-max(dot(n,viewDir),0.0),2.4); lit += vec3(0.35,0.65,1.0)*rim*0.22; }
    if(uCollection==1) lit=mix(lit,vec3(0.15,0.85,0.22),0.38);       // Collider
    if(uCollection==2) lit=mix(lit,vec3(0.75,0.55,1.0),0.24);        // MAT
    if(uCollection==3){ lit += vec3(1.0,0.86,0.55)*(uBloomPreview!=0?0.75:0.25); }
    if(uSelected!=0){ lit=mix(lit,vec3(1.0,0.82,0.22),0.42); }
    if(uFog!=0){ float d=length(vWorld)*0.006; float f=clamp(1.0-exp(-d*d),0.0,0.75); lit=mix(lit,vec3(0.42,0.46,0.55),f); }
    FragColor=vec4(lit,uBaseColor.a*tex.a);
}
)GLSL";
    program_=linkProgram(vs,fs);
    const char* gvs=R"GLSL(#version 330 core
layout(location=0) in vec3 aPos; uniform mat4 uMVP; void main(){gl_Position=uMVP*vec4(aPos,1.0);} )GLSL";
    const char* gfs=R"GLSL(#version 330 core
uniform vec4 uColor; out vec4 FragColor; void main(){FragColor=uColor;} )GLSL";
    gridProgram_=linkProgram(gvs,gfs);
    return program_&&gridProgram_;
}

bool EditorPreviewRenderer::makeFramebuffer(int width,int height){ width=std::max(16,width); height=std::max(16,height); if(fbo_&&fbWidth_==width&&fbHeight_==height)return true; destroyFramebuffer(); fbWidth_=width; fbHeight_=height; glGenFramebuffers(1,&fbo_); glBindFramebuffer(GL_FRAMEBUFFER,fbo_); glGenTextures(1,&colorTex_); glBindTexture(GL_TEXTURE_2D,colorTex_); glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR); glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,colorTex_,0); glGenRenderbuffers(1,&depthRbo_); glBindRenderbuffer(GL_RENDERBUFFER,depthRbo_); glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,width,height); glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,depthRbo_); bool ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE; glBindFramebuffer(GL_FRAMEBUFFER,0); return ok; }
void EditorPreviewRenderer::destroyFramebuffer(){ if(depthRbo_)glDeleteRenderbuffers(1,&depthRbo_); if(colorTex_)glDeleteTextures(1,&colorTex_); if(fbo_)glDeleteFramebuffers(1,&fbo_); fbo_=colorTex_=depthRbo_=0; fbWidth_=fbHeight_=0; }
void EditorPreviewRenderer::destroyImage(){ if(imageTexture_)glDeleteTextures(1,&imageTexture_); imageTexture_=0; imageWidth_=imageHeight_=0; imageLoaded_=false; }
void EditorPreviewRenderer::destroyModel(){ for(auto&p:primitives_){ if(p.ebo)glDeleteBuffers(1,&p.ebo); if(p.vbo)glDeleteBuffers(1,&p.vbo); if(p.vao)glDeleteVertexArrays(1,&p.vao);} for(auto&m:materials_) if(m.texture)glDeleteTextures(1,&m.texture); primitives_.clear(); materials_.clear(); nodeNames_.clear(); materialNames_.clear(); sceneMarkers_.clear(); nodes_.clear(); instanceTransforms_.clear(); loaded_=false; triangleCount_=primitiveCount_=0; minBounds_[0]=minBounds_[1]=minBounds_[2]=std::numeric_limits<float>::max(); maxBounds_[0]=maxBounds_[1]=maxBounds_[2]=-std::numeric_limits<float>::max(); }
void EditorPreviewRenderer::clearModel(){ destroyModel(); loadedPath_.clear(); loadedName_.clear(); }

bool EditorPreviewRenderer::loadImage(const std::filesystem::path& path,std::string* errorOut){ initialize(); if(imageLoaded_&&loadedPath_==path)return true; destroyImage(); destroyModel(); int w=0,h=0,comp=0; unsigned char* pixels=stbi_load(path.string().c_str(),&w,&h,&comp,4); if(!pixels){ if(errorOut)*errorOut=std::string("Unable to load image: ")+stbi_failure_reason(); return false;} glGenTextures(1,&imageTexture_); glBindTexture(GL_TEXTURE_2D,imageTexture_); glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels); glGenerateMipmap(GL_TEXTURE_2D); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE); stbi_image_free(pixels); imageWidth_=w; imageHeight_=h; imageLoaded_=true; loadedPath_=path; loadedName_=path.filename().string(); if(errorOut)errorOut->clear(); return true; }

bool EditorPreviewRenderer::loadGlb(const std::filesystem::path& path,std::string* errorOut){ initialize(); if(loaded_&&loadedPath_==path)return true; destroyImage(); destroyModel(); SceneInstanceDesc inst; inst.name=path.stem().string(); inst.type="Asset"; inst.path=path; bool ok=appendGlb(path,&inst,0,errorOut); if(ok){ loaded_=true; loadedPath_=path; loadedName_=path.filename().string(); if(errorOut)errorOut->clear(); } return ok; }

bool EditorPreviewRenderer::loadSceneInstances(const std::vector<SceneInstanceDesc>& instances,const std::filesystem::path& projectRoot,std::string* errorOut){ initialize(); destroyImage(); destroyModel(); std::ostringstream errors; int loadedCount=0; for(size_t i=0;i<instances.size();++i){ if(!instances[i].active)continue; std::filesystem::path p=instances[i].path; if(p.empty())continue; if(!p.is_absolute())p=projectRoot/p; if(!std::filesystem::exists(p)){ errors<<"Missing: "<<p.string()<<"\n"; continue; } if(appendGlb(p,&instances[i],(int)i,errorOut)){ ++loadedCount; } else if(errorOut){ errors<<*errorOut<<"\n"; }} if(loadedCount==0){ minBounds_[0]=minBounds_[1]=minBounds_[2]=-1.0f; maxBounds_[0]=maxBounds_[1]=maxBounds_[2]=1.0f; loaded_=true; loadedPath_.clear(); loadedName_="Empty AXScene"; primitiveCount_=0; if(errorOut) errorOut->clear(); return true; } loaded_=true; loadedPath_.clear(); loadedName_="AXScene Composition ("+std::to_string(loadedCount)+" GLB objects)"; primitiveCount_=(int)primitives_.size(); if(errorOut) *errorOut=errors.str(); return true; }

bool EditorPreviewRenderer::appendGlb(const std::filesystem::path& path,const SceneInstanceDesc* instance,int objectIndex,std::string* errorOut){
    tinygltf::Model model; tinygltf::TinyGLTF loader; std::string err,warn; bool ok=false; std::string ext=lowerCopy(path.extension().string()); if(ext==".glb") ok=loader.LoadBinaryFromFile(&model,&err,&warn,path.string()); else ok=loader.LoadASCIIFromFile(&model,&err,&warn,path.string()); if(!warn.empty())std::cerr<<"[AXEditorPreview] "<<warn<<"\n"; if(!ok){ if(errorOut)*errorOut="GLB load failed: "+err+" path="+path.string(); return false; }
    const int matBase=(int)materials_.size(); materials_.resize(materials_.size()+std::max<size_t>(1,model.materials.size())); for(size_t i=0;i<materials_.size()-matBase;++i){ MaterialGL mat; if(i<model.materials.size()){ const auto& src=model.materials[i]; mat.name=src.name.empty()?"material_"+std::to_string(i):src.name; const auto& f=src.pbrMetallicRoughness.baseColorFactor; if(f.size()==4){mat.baseColor[0]=(float)f[0];mat.baseColor[1]=(float)f[1];mat.baseColor[2]=(float)f[2];mat.baseColor[3]=(float)f[3];} int texIndex=src.pbrMetallicRoughness.baseColorTexture.index; if(texIndex>=0&&texIndex<(int)model.textures.size()){ int imgIndex=model.textures[texIndex].source; if(imgIndex>=0&&imgIndex<(int)model.images.size()){ const auto& img=model.images[imgIndex]; GLenum fmt=img.component==4?GL_RGBA:img.component==3?GL_RGB:GL_RED; glGenTextures(1,&mat.texture); glBindTexture(GL_TEXTURE_2D,mat.texture); GLenum internalFmt=fmt==GL_RGBA?GL_RGBA8:fmt==GL_RGB?GL_RGB8:GL_R8; glTexImage2D(GL_TEXTURE_2D,0,internalFmt,img.width,img.height,0,fmt,GL_UNSIGNED_BYTE,img.image.data()); glGenerateMipmap(GL_TEXTURE_2D); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT); mat.hasTexture=true; } } } else mat.name="Default"; materialNames_.push_back(mat.name); materials_[matBase+i]=mat; }
    std::vector<bool> jointMask(model.nodes.size(),false); for(const auto& skin:model.skins) for(int j:skin.joints) if(j>=0&&j<(int)jointMask.size()) jointMask[j]=true; for(int i=0;i<(int)model.nodes.size();++i){ auto lname=lowerCopy(model.nodes[i].name); if(lname.find("root")!=std::string::npos||lname.find("hips")!=std::string::npos||lname.find("spine")!=std::string::npos||lname.find("chest")!=std::string::npos||lname.find("neck")!=std::string::npos||lname.find("head")!=std::string::npos||lname.find("arm")!=std::string::npos||lname.find("forearm")!=std::string::npos||lname.find("hand")!=std::string::npos||lname.find("thigh")!=std::string::npos||lname.find("shin")!=std::string::npos||lname.find("foot")!=std::string::npos||lname.find("bone")!=std::string::npos||lname.find("muzzle")!=std::string::npos) jointMask[i]=true; }
    float instanceRoot[16]; if(instance) matTRS(instance->position,instance->rotation,instance->scale,instanceRoot); else matIdentity(instanceRoot);
    // IMPORTANT: Do NOT bake the scene-object transform into the GLB vertices.
    // Vertices stay in the GLB asset's own local/world space. The scene object
    // transform is applied once at draw time as the model matrix. This is what
    // makes selecting Scene1 / Environment and scaling it behave like a true
    // parent/root transform instead of scaling every child mesh around its own pivot.
    float root[16]; matIdentity(root);
    PreviewNode rootNode; rootNode.index=1000000+objectIndex; rootNode.parentIndex=-1; rootNode.objectIndex=objectIndex; rootNode.name=instance?instance->name:path.stem().string(); rootNode.type=instance?instance->type:"Asset"; rootNode.position={instanceRoot[12],instanceRoot[13],instanceRoot[14]}; rootNode.scale={instance?instance->scale[0]:1.0f,instance?instance->scale[1]:1.0f,instance?instance->scale[2]:1.0f}; nodes_.push_back(rootNode);
    auto emitPrimitive=[&](const tinygltf::Primitive& srcPrim,const float* nodeMatrix,const std::string& nodeName,const std::string& meshName,const float* baseWorldPos,float baseYawDeg,const float* baseWorldScale){ auto posIt=srcPrim.attributes.find("POSITION"); if(posIt==srcPrim.attributes.end())return; int posAcc=posIt->second; int normAcc=-1,uvAcc=-1; auto nit=srcPrim.attributes.find("NORMAL"); if(nit!=srcPrim.attributes.end())normAcc=nit->second; auto uit=srcPrim.attributes.find("TEXCOORD_0"); if(uit!=srcPrim.attributes.end())uvAcc=uit->second; const auto& pa=model.accessors[posAcc]; std::vector<Vtx> vertices(pa.count); for(size_t i=0;i<pa.count;++i){ float lp[3]={},ln[3]={0,1,0},uv[2]={},wp[3]={},wn[3]={0,1,0}; readFloat3(model,posAcc,i,lp); if(normAcc>=0)readFloat3(model,normAcc,i,ln); if(uvAcc>=0)readFloat2(model,uvAcc,i,uv); transformPoint(nodeMatrix,lp,wp); transformVector(nodeMatrix,ln,wn); vertices[i]={wp[0],wp[1],wp[2],wn[0],wn[1],wn[2],uv[0],uv[1]}; for(int c=0;c<3;++c){minBounds_[c]=std::min(minBounds_[c],wp[c]); maxBounds_[c]=std::max(maxBounds_[c],wp[c]);}} std::vector<unsigned int> indices; if(srcPrim.indices>=0){ const auto& ia=model.accessors[srcPrim.indices]; indices.resize(ia.count); for(size_t i=0;i<ia.count;++i)indices[i]=readIndex(model,srcPrim.indices,i);} PrimitiveGL gp; gp.indexed=!indices.empty(); gp.indexCount=(int)indices.size(); gp.vertexCount=(int)vertices.size(); gp.materialIndex=matBase+(srcPrim.material>=0?srcPrim.material:0); gp.objectIndex=objectIndex; gp.selected=instance&&instance->selected; gp.objectName=instance?instance->name:""; gp.nodeName=nodeName; gp.meshName=meshName; gp.materialName=(gp.materialIndex>=0&&gp.materialIndex<(int)materials_.size())?materials_[gp.materialIndex].name:"Default"; gp.collection=collectionFor(gp.nodeName,gp.meshName,gp.materialName); gp.baseWorldPos[0]=baseWorldPos[0];gp.baseWorldPos[1]=baseWorldPos[1];gp.baseWorldPos[2]=baseWorldPos[2]; gp.baseYawDeg=baseYawDeg; gp.baseWorldScale[0]=baseWorldScale[0];gp.baseWorldScale[1]=baseWorldScale[1];gp.baseWorldScale[2]=baseWorldScale[2]; if(instance){ gp.objectBasePos[0]=instance->position[0]; gp.objectBasePos[1]=instance->position[1]; gp.objectBasePos[2]=instance->position[2]; gp.objectBaseYawDeg=instance->rotation[1]; gp.objectBaseScale[0]=instance->scale[0]; gp.objectBaseScale[1]=instance->scale[1]; gp.objectBaseScale[2]=instance->scale[2]; } glGenVertexArrays(1,&gp.vao); glGenBuffers(1,&gp.vbo); glBindVertexArray(gp.vao); glBindBuffer(GL_ARRAY_BUFFER,gp.vbo); glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vtx),vertices.data(),GL_STATIC_DRAW); glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vtx),(void*)0); glEnableVertexAttribArray(1); glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vtx),(void*)(sizeof(float)*3)); glEnableVertexAttribArray(2); glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vtx),(void*)(sizeof(float)*6)); if(gp.indexed){glGenBuffers(1,&gp.ebo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,gp.ebo); glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*sizeof(unsigned int),indices.data(),GL_STATIC_DRAW); triangleCount_+=gp.indexCount/3;} else triangleCount_+=gp.vertexCount/3; glBindVertexArray(0); primitives_.push_back(gp); };
    std::function<void(int,int,const float*)> walk=[&](int nodeIndex,int parentIndex,const float* parentMat){ if(nodeIndex<0||nodeIndex>=(int)model.nodes.size())return; const auto& node=model.nodes[nodeIndex]; float local[16], global[16]; matFromNode(node,local); matMul(parentMat,local,global); std::string nodeName=node.name.empty()?"Node_"+std::to_string(nodeIndex):node.name; PreviewNode pn; pn.index=(objectIndex+1)*100000+nodeIndex; pn.parentIndex=parentIndex<0?1000000+objectIndex:(objectIndex+1)*100000+parentIndex; pn.objectIndex=objectIndex; pn.name=(instance?instance->name+"/":"")+nodeName; pn.hasMesh=node.mesh>=0; pn.isJoint=nodeIndex>=0&&nodeIndex<(int)jointMask.size()&&jointMask[nodeIndex]; pn.position={global[12],global[13],global[14]}; float sx=std::sqrt(global[0]*global[0]+global[1]*global[1]+global[2]*global[2]); float sy=std::sqrt(global[4]*global[4]+global[5]*global[5]+global[6]*global[6]); float sz=std::sqrt(global[8]*global[8]+global[9]*global[9]+global[10]*global[10]); pn.scale={std::max(0.001f,sx),std::max(0.001f,sy),std::max(0.001f,sz)}; pn.rotation={0.0f,std::atan2(global[8],global[10])*180.0f/PI,0.0f}; std::string mt; if(!node.name.empty()&&markerTypeForNodeName(node.name,mt)){pn.type=mt; pn.isMarker=true;} else if(pn.isJoint)pn.type="Bone"; else if(pn.hasMesh)pn.type="MeshNode"; else pn.type="Node"; nodes_.push_back(pn); nodeNames_.push_back(pn.name); if(!node.name.empty()&&markerTypeForNodeName(node.name,mt)){ SceneMarker marker; marker.name=pn.name; marker.type=mt; marker.position=pn.position; marker.rotation=pn.rotation; marker.scale=pn.scale; sceneMarkers_.push_back(marker);} if(node.mesh>=0&&node.mesh<(int)model.meshes.size()){ const auto& mesh=model.meshes[node.mesh]; for(const auto& prim:mesh.primitives)emitPrimitive(prim,global,pn.name,mesh.name,pn.position.data(),pn.rotation[1],pn.scale.data()); } for(int child:node.children)walk(child,nodeIndex,global); };
    float identity[16]; matIdentity(identity); float start[16]; matMul(root, identity, start); if(!model.scenes.empty()){ int si=model.defaultScene>=0?model.defaultScene:0; si=std::clamp(si,0,(int)model.scenes.size()-1); for(int ni:model.scenes[si].nodes)walk(ni,-1,root); } else for(int i=0;i<(int)model.nodes.size();++i)walk(i,-1,root);
    primitiveCount_=(int)primitives_.size(); return true;
}

void EditorPreviewRenderer::drawSky(int width,int height,bool showSky){ glDisable(GL_DEPTH_TEST); if(showSky)glClearColor(0.36f,0.40f,0.48f,1.0f); else glClearColor(0.10f,0.11f,0.14f,1.0f); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT); }
void EditorPreviewRenderer::drawGrid(const float* vp){ std::vector<float> lines; int N=40; for(int i=-N;i<=N;++i){ lines.insert(lines.end(),{(float)i,0,(float)-N,(float)i,0,(float)N,(float)-N,0,(float)i,(float)N,0,(float)i}); } GLuint vao=0,vbo=0; glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo); glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,vbo); glBufferData(GL_ARRAY_BUFFER,lines.size()*sizeof(float),lines.data(),GL_DYNAMIC_DRAW); glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0); glUseProgram(gridProgram_); glUniformMatrix4fv(glGetUniformLocation(gridProgram_,"uMVP"),1,GL_FALSE,vp); glUniform4f(glGetUniformLocation(gridProgram_,"uColor"),0.28f,0.30f,0.36f,1.0f); glDrawArrays(GL_LINES,0,(GLsizei)(lines.size()/3)); glBindVertexArray(0); glDeleteBuffers(1,&vbo); glDeleteVertexArrays(1,&vao); }
void EditorPreviewRenderer::drawModel(const float* vp,bool selectedOnly){
    if(!loaded_)return;
    glUseProgram(program_);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(program_,"uTex"),0);
    glUniform1i(glGetUniformLocation(program_,"uLit"),lit_?1:0);
    glUniform1i(glGetUniformLocation(program_,"uCel"),cel_?1:0);
    glUniform1i(glGetUniformLocation(program_,"uRim"),rim_?1:0);
    glUniform1i(glGetUniformLocation(program_,"uFog"),fog_?1:0);
    glUniform1i(glGetUniformLocation(program_,"uBloomPreview"),bloomPreview_?1:0);

    for(const auto&p:primitives_){
        if(selectedOnly&&!p.selected)continue;
        if(p.collection=="Collider"&&!showColliders_)continue;
        if(p.collection=="Bloom"&&!showBloomObjects_)continue;
        if(p.collection=="MAT"&&!showMatObjects_)continue;
        if(p.collection=="Object"&&!showObjects_)continue;

        // GLB composition rule:
        // The imported GLB is one scene object/root. Its internal mesh nodes are
        // asset-local children. Scaling the root must apply ONE model matrix to
        // the whole GLB. Do not also apply child-node transforms by default,
        // because those child transforms were imported from the GLB and will make
        // every wall/pipe/prop scale around its own pivot.
        float model[16];
        matIdentity(model);

        auto objIt = instanceTransforms_.find(p.objectName);
        if (objIt != instanceTransforms_.end()) {
            const InstanceTransform& live = objIt->second;
            matTRS(live.position, live.rotation, live.scale, model);
        }

        // Intentional: nodeName transforms are ignored for imported GLB children
        // in normal scene composition mode. Later we should add an explicit
        // "Edit GLB Node" mode that marks one node as edited. Until then, the
        // stable and correct behavior is root-object transform only.

        float mvp[16]; matMul(vp,model,mvp);
        glUniformMatrix4fv(glGetUniformLocation(program_,"uMVP"),1,GL_FALSE,mvp);
        glUniformMatrix4fv(glGetUniformLocation(program_,"uModel"),1,GL_FALSE,model);

        const auto& mat=materials_[std::clamp(p.materialIndex,0,(int)materials_.size()-1)];
        int coll=0; if(p.collection=="Collider")coll=1; else if(p.collection=="MAT")coll=2; else if(p.collection=="Bloom")coll=3;
        glUniform1i(glGetUniformLocation(program_,"uCollection"),coll);
        glUniform1i(glGetUniformLocation(program_,"uSelected"),p.selected?1:0);
        glUniform4fv(glGetUniformLocation(program_,"uBaseColor"),1,mat.baseColor);
        glUniform1i(glGetUniformLocation(program_,"uHasTex"),mat.hasTexture?1:0);
        if(mat.hasTexture)glBindTexture(GL_TEXTURE_2D,mat.texture); else glBindTexture(GL_TEXTURE_2D,0);
        glBindVertexArray(p.vao);
        if(p.indexed)glDrawElements(GL_TRIANGLES,p.indexCount,GL_UNSIGNED_INT,0); else glDrawArrays(GL_TRIANGLES,0,p.vertexCount);
    }
    glBindVertexArray(0);
}


void EditorPreviewRenderer::setInstanceTransform(
    const std::string& nodeName,
    const float position[3],
    const float rotation[3],
    const float scale[3]
) {
    if (nodeName.empty()) return;
    InstanceTransform t;
    t.position[0] = position ? position[0] : 0.0f;
    t.position[1] = position ? position[1] : 0.0f;
    t.position[2] = position ? position[2] : 0.0f;
    t.rotation[0] = rotation ? rotation[0] : 0.0f;
    t.rotation[1] = rotation ? rotation[1] : 0.0f;
    t.rotation[2] = rotation ? rotation[2] : 0.0f;
    t.scale[0] = scale ? scale[0] : 1.0f;
    t.scale[1] = scale ? scale[1] : 1.0f;
    t.scale[2] = scale ? scale[2] : 1.0f;
    instanceTransforms_[nodeName] = t;
}

int EditorPreviewRenderer::boneCount() const {
    int count = 0;
    for (const PreviewNode& n : nodes_) {
        if (n.isJoint) ++count;
    }
    return count;
}

void EditorPreviewRenderer::drawSkeleton(const float* vp) {
    if (!loaded_ || nodes_.empty()) return;

    std::vector<float> lines;
    lines.reserve(nodes_.size() * 6);

    for (const PreviewNode& n : nodes_) {
        if (!n.isJoint || n.parentIndex < 0) continue;

        auto it = std::find_if(nodes_.begin(), nodes_.end(), [&](const PreviewNode& p) {
            return p.index == n.parentIndex;
        });

        if (it == nodes_.end()) continue;
        if (!it->isJoint && it->type != "Bone" && it->type != "Node") continue;

        lines.push_back(it->position[0]);
        lines.push_back(it->position[1]);
        lines.push_back(it->position[2]);
        lines.push_back(n.position[0]);
        lines.push_back(n.position[1]);
        lines.push_back(n.position[2]);
    }

    if (lines.empty()) return;

    GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, lines.size() * sizeof(float), lines.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glUseProgram(gridProgram_);
    glUniformMatrix4fv(glGetUniformLocation(gridProgram_, "uMVP"), 1, GL_FALSE, vp);
    glUniform4f(glGetUniformLocation(gridProgram_, "uColor"), 0.20f, 0.95f, 1.0f, 1.0f);
    glDrawArrays(GL_LINES, 0, (GLsizei)(lines.size() / 3));

    glBindVertexArray(0);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}

float EditorPreviewRenderer::suggestedCameraDistance(float fovYDeg) const{ float ext[3]={maxBounds_[0]-minBounds_[0],maxBounds_[1]-minBounds_[1],maxBounds_[2]-minBounds_[2]}; float radius=std::sqrt(ext[0]*ext[0]+ext[1]*ext[1]+ext[2]*ext[2])*0.5f; radius=std::max(radius,1.0f); float fov=std::max(10.0f,fovYDeg)*PI/180.0f; return std::clamp(radius/std::tan(fov*0.5f)*1.45f,2.0f,5000.0f); }
static void boundsFocus(bool loaded,bool frameModel,const float* minB,const float* maxB,float* out){ out[0]=0;out[1]=1.2f;out[2]=0; if(loaded&&frameModel){out[0]=(minB[0]+maxB[0])*0.5f;out[1]=(minB[1]+maxB[1])*0.5f;out[2]=(minB[2]+maxB[2])*0.5f;} }
static void previewCameraForward(float yawDeg,float pitchDeg,float*out){ float yaw=yawDeg*PI/180.0f,pitch=pitchDeg*PI/180.0f; out[0]=-std::sin(yaw)*std::cos(pitch); out[1]=-std::sin(pitch); out[2]=-std::cos(yaw)*std::cos(pitch); }
void EditorPreviewRenderer::framedCameraPosition(float yawDeg,float pitchDeg,float distance,bool frameModel,float*outPosition) const{ float center[3]; boundsFocus(loaded_,frameModel,minBounds_,maxBounds_,center); float f[3]; previewCameraForward(yawDeg,pitchDeg,f); distance=std::max(distance,1.0f); outPosition[0]=center[0]-f[0]*distance; outPosition[1]=center[1]-f[1]*distance; outPosition[2]=center[2]-f[2]*distance; }
void EditorPreviewRenderer::buildViewProjection(int width,int height,float yawDeg,float pitchDeg,float distance,bool frameModel,float*outVP,const float*focusOffset) const{ float eye[3],center[3],f[3]; previewCameraForward(yawDeg,pitchDeg,f); if(focusOffset){ eye[0]=focusOffset[0];eye[1]=focusOffset[1];eye[2]=focusOffset[2]; float la=std::max(distance,1.0f); center[0]=eye[0]+f[0]*la; center[1]=eye[1]+f[1]*la; center[2]=eye[2]+f[2]*la; } else { boundsFocus(loaded_,frameModel,minBounds_,maxBounds_,center); distance=std::max(distance,1.0f); eye[0]=center[0]-f[0]*distance; eye[1]=center[1]-f[1]*distance; eye[2]=center[2]-f[2]*distance; } float up[3]={0,1,0}; float view[16],proj[16]; lookAt(eye,center,up,view); perspective(58.0f*PI/180.0f,(float)width/std::max(1,height),0.05f,5000.0f,proj); matMul(proj,view,outVP); }
bool EditorPreviewRenderer::worldToScreen(float x,float y,float z,int width,int height,float yaw,float pitch,float distance,bool frameModel,float&outX,float&outY,const float*focusOffset) const{ float vp[16]; buildViewProjection(width,height,yaw,pitch,distance,frameModel,vp,focusOffset); return projectPoint(vp,x,y,z,width,height,outX,outY); }
unsigned int EditorPreviewRenderer::renderToTexture(int width,int height,float yawDeg,float pitchDeg,float distance,bool showGrid,bool showSky,bool showSkeleton,bool frameModel,const float*focusOffset,bool showObjects,bool showColliders,bool showMatObjects,bool showBloomObjects,bool lit,bool cel,bool rim,bool fog,bool bloomPreview,bool wireframe,bool selectedOnly){ initialize(); showObjects_=showObjects;showColliders_=showColliders;showMatObjects_=showMatObjects;showBloomObjects_=showBloomObjects;lit_=lit;cel_=cel;rim_=rim;fog_=fog;bloomPreview_=bloomPreview; makeFramebuffer(width,height); glBindFramebuffer(GL_FRAMEBUFFER,fbo_); glViewport(0,0,width,height); drawSky(width,height,showSky); glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE); glCullFace(GL_BACK); float vp[16]; buildViewProjection(width,height,yawDeg,pitchDeg,distance,frameModel,vp,focusOffset); if(showGrid)drawGrid(vp); if(wireframe)glPolygonMode(GL_FRONT_AND_BACK,GL_LINE); drawModel(vp,selectedOnly); if(wireframe)glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); if(showSkeleton)drawSkeleton(vp); glBindFramebuffer(GL_FRAMEBUFFER,0); glDisable(GL_CULL_FACE); return colorTex_; }
