#pragma once

#include <vector>
#include <string>
#include <SDL3/SDL.h>
#include "assets/MeshAsset.h"
#include "assets/SkinnedMeshAsset.h"
#include "assets/StaticSceneAsset.h"
#include "ecs/SkinnedRenderComponent.h"
#include "ecs/StaticSceneRenderComponent.h"
#include "OGL3D/ShaderProgram.h"
#include "OGL3D/ShaderLibrary.h"
#include "OGL3D/RenderSettings.h"
#include "OGL3D/UIRenderer.h"
#include "OGL3D/RenderStyle.h"

class Scene;
class RenderComponent;
class TransformComponent;
class Entity;

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    bool init(SDL_Window* window);
    void renderScene(Scene& scene);

    // Draw scene entities on top of a custom script render pass without clearing.
    // Used by block worlds that render their own world first, then need ECS models.
    void renderSceneOverlay(Scene& scene);

    void onResize(int width, int height);
    void shutdown();

    void setCamera(
        float camX, float camY, float camZ,
        float targetX, float targetY, float targetZ
    );

    // Shadow frustum anchor. Call once per frame with the player's foot-level
    // world position so the shadow camera is centered on the character, not the
    // camera orbit target (which sits at chest height).
    void setShadowAnchor(float x, float y, float z);

    // Debug line rendering. Useful for collider wireframes.
    void clearDebugLines();

    void addDebugLine(
        float ax, float ay, float az,
        float bx, float by, float bz,
        float r, float g, float b
    );

    // Draws sun direction and shadow-camera direction using debug lines.
    // Intended for F4 light/shadow debugging.
    void addLightDebugLines();

    UIRenderer& ui() { return uiRenderer_; }

    RenderSettings& settings() { return settings_; }
    const RenderSettings& settings() const { return settings_; }

    bool uploadMesh(RenderComponent& rc, const MeshAsset& mesh);

    // Upload all primitives of a static scene GLB into GPU buffers
    void uploadStaticScene(StaticSceneRenderComponent& src);

    // Draw all visible static scene primitives for one entity
    void renderStaticSceneEntity(const Entity& entity, unsigned int prog, const float* pv);

    // Upload all primitives of a skinned GLB into GPU buffers
    void uploadSkinnedMesh(SkinnedRenderComponent& src);

    // Draw all skinned primitives for one entity (called inside renderScene)
    void renderSkinnedEntity(const Entity& entity, unsigned int prog);

    // Access the shader library — user loads named shaders through this
    ShaderLibrary& shaders() { return shaderLibrary_; }

    // Override which shader renderScene uses (nullptr = use built-in)
    void setActiveShader(ShaderProgram* shader) { activeShader_ = shader; }

    void beginUI();
    void drawUIRect(float x, float y, float w, float h,
                    float r, float g, float b, float a = 1.0f);
    void endUI();

    void drawUIImage(Texture2D& tex,
                float x, float y, float w, float h,
                float r, float g, float b, float a = 1.0f);

    void drawUIImageRegion(Texture2D& tex,
                       float x, float y, float w, float h,
                       float u0, float v0, float u1, float v1,
                       float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f);

    void drawUIText(const std::string& text,
                float x, float y,
                float scale,
                float r, float g, float b, float a = 1.0f);

    void setRenderStyle(const RenderStyle& style);
    const RenderStyle& renderStyle() const;

    // User-selectable sky mode.
    // Color mode uses glClearColor-style background, good for Minecraft-like skies.
    // Texture mode uses the existing panorama sky renderer.
    void setSkyColor(float r, float g, float b);
    void setSkyTexture(const std::string& path);
    void useSkyColor();
    void useSkyTexture();

    // Screen-space bloom for emissive VFX such as the holy sword.
    void setBloomEnabled(bool enabled) { bloomEnabled_ = enabled; }
    bool bloomEnabled() const { return bloomEnabled_; }
    void setBloomSettings(float threshold, float strength, float radius) {
        bloomThreshold_ = threshold;
        bloomStrength_ = strength;
        bloomRadius_ = radius;
    }

private:
    SDL_GLContext  glContext_       = nullptr;
    int            viewportWidth_   = 1280;
    int            viewportHeight_  = 720;

    float cameraX_ = 0.0f;
    float cameraY_ = 2.0f;
    float cameraZ_ = 8.0f;

    float cameraTargetX_ = 0.0f;
    float cameraTargetY_ = 1.0f;
    float cameraTargetZ_ = 0.0f;

    // Decoupled from camera target so the shadow frustum centers on the player's
    // feet rather than the chest-height orbit point.
    float shadowAnchorX_ = 0.0f;
    float shadowAnchorY_ = 0.0f;
    float shadowAnchorZ_ = 0.0f;
    bool  shadowAnchorSet_ = false;

    struct DebugLineVertex {
        float x, y, z;
        float r, g, b;
    };

    std::vector<DebugLineVertex> debugLines_;
    unsigned int debugLineVAO_ = 0;
    unsigned int debugLineVBO_ = 0;

    void drawDebugLines(const float* pv);

    // Panorama/procedural sky renderer. The panorama texture is optional;
    // if it fails to load, Renderer falls back to a stylized gradient sky.
    unsigned int skyVAO_ = 0;
    unsigned int skyVBO_ = 0;
    unsigned int skyProgramID_ = 0;
    unsigned int skyTextureID_ = 0;
    std::string loadedSkyTexturePath_;

    bool createPanoramaSkyResources();
    unsigned int loadTexture2DFromFile(const std::string& path);
    void renderPanoramaSky(const float* view, const float* proj);

    // Built-in fallback shader (compiled from inline strings)
    ShaderProgram  builtinShader_;

    // User-facing shader registry
    ShaderLibrary  shaderLibrary_;

    // Which shader is active this frame
    ShaderProgram* activeShader_    = nullptr;

    RenderSettings settings_;
    UIRenderer uiRenderer_;

    bool createBuiltinShader();

    unsigned int builtinProgramID_ = 0;

    RenderStyle renderStyle_ = RenderStyle::DefaultStylized();

    void applyRenderStyleUniforms(unsigned int program);

    // Bloom/offscreen post-process resources.
    bool bloomEnabled_ = true;
    float bloomThreshold_ = 0.95f;
    float bloomStrength_ = 0.72f;
    float bloomRadius_ = 1.0f;

    unsigned int sceneFBO_ = 0;
    unsigned int sceneColorTexture_ = 0;
    unsigned int sceneBrightTexture_ = 0;
    unsigned int sceneDepthRBO_ = 0;

    unsigned int bloomPingFBO_[2] = {0, 0};
    unsigned int bloomPingTexture_[2] = {0, 0};

    unsigned int screenQuadVAO_ = 0;
    unsigned int screenQuadVBO_ = 0;
    unsigned int bloomBlurProgramID_ = 0;
    unsigned int bloomCompositeProgramID_ = 0;

    bool createBloomResources();
    void destroyBloomResources();
    void renderBloomComposite();

    unsigned int shadowFBO_ = 0;
    unsigned int shadowDepthTexture_ = 0;
    unsigned int shadowProgramID_ = 0;

    int shadowMapSize_ = 4096;

    float lightSpaceMatrix_[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };

    void buildLightSpaceMatrix();
    bool createShadowResources();
    void renderShadowPass(Scene& scene);
    void applyShadowUniforms(unsigned int program);
};