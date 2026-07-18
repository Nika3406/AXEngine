#pragma once

#include "Game.h"
#include "OGL3D/RenderStyle.h"
#include "runtime/AXSceneRuntime.h"
#include "assets/StaticSceneLoader.h"

#include <memory>
#include <vector>
#include <string>
#include <functional>

class Renderer;
class AssetManager;
struct StaticSceneAsset;

// Backward-compatible aliases: the actual struct definitions moved to
// include/assets/StaticSceneLoader.h (engine core), alongside GltfLoader /
// AssetManager, so level-geometry loading is a real engine mechanism
// instead of a duplicate GLB parser living in scripts/. Everything below
// this comment that used to be a struct definition is now just a name for
// an engine type, so the rest of scripts/ (Player.cpp, Sub.cpp, EnemyAI.cpp)
// never had to change.
using DemoAABB             = AXCollisionAABB;
using DemoVec3              = AXVec3;
using DemoColliderTriangle  = AXColliderTriangle;
using DemoMeshCollider       = AXMeshCollider;
using DemoSceneMarker        = AXSceneMarker;


enum class EnvironmentRenderProfile {
    Basic,
    BlockWorld,
    StylizedAction,
    DarkCelestialBrutalism
};

// -----------------------------------------------------------------------------
// Environment-authored render tuning
// -----------------------------------------------------------------------------
// These are the actual game/level visual variables. RenderStyle.h is only the
// engine-side data container; edit values here when tuning AXEngine scenes.
struct EnvironmentBloomSettings {
    bool enabled = false;
    float threshold = 0.0f;
    float strength = 0.0f;
    float radius = 1.0f;
};

struct EnvironmentRenderSettings {
    bool stylizedLightingEnabled = true;
    bool skyEnabled = true;
    bool fogEnabled = false;
    bool celShadingEnabled = false;
    bool rimLightingEnabled = false;

    StylizedLightingSettings lighting;
    ColorGradeSettings colorGrade;
    SkySettings sky;
    EnvironmentBloomSettings bloom;

    static EnvironmentRenderSettings Basic() {
        EnvironmentRenderSettings s;

        s.stylizedLightingEnabled = true;
        s.skyEnabled = true;
        s.fogEnabled = false;
        s.celShadingEnabled = false;
        s.rimLightingEnabled = false;

        // Direction the light rays travel. The sky shader now draws a sun glow
        // from this exact vector, so changing this changes both lighting and visible sun.
        s.lighting.sunDirection = {-0.35f, -0.65f, 0.45f};
        s.lighting.sunColor = {1.0f, 1.0f, 1.0f};
        s.lighting.sunIntensity = 1.0f;
        s.lighting.ambientSky = {0.38f, 0.38f, 0.38f};
        s.lighting.ambientGround = {0.26f, 0.26f, 0.26f};
        s.lighting.shadowColor = {0.26f, 0.26f, 0.28f};
        s.lighting.shadowStrength = 0.32f;
        s.lighting.minLight = 0.34f;
        s.lighting.realtimeShadows = true;
        s.lighting.shadowBias = 0.0f;
        s.lighting.shadowMapSoftness = 0.15f;
        s.lighting.shadowOrthoRange = 55.0f;
        s.lighting.shadowSoftness = 0.0f;
        s.lighting.rimColor = {0.45f, 0.75f, 1.0f};
        s.lighting.rimIntensity = 0.0f;
        s.lighting.rimPower = 3.0f;
        s.lighting.fogColor = {0.42f, 0.42f, 0.44f};
        s.lighting.fogDensity = 0.0f;
        s.lighting.celStrength = 0.0f;
        s.lighting.celBandSoftness = 0.18f;

        s.colorGrade.exposure = 1.0f;
        s.colorGrade.saturation = 1.0f;
        s.colorGrade.contrast = 1.0f;
        s.colorGrade.gamma = 2.2f;

        s.sky.enabled = true;
        s.sky.mode = SkySettings::Mode::Color;
        s.sky.color = {0.52f, 0.64f, 0.78f};
        s.sky.intensity = 1.0f;
        s.sky.rotation = 0.0f;
        s.sky.saturation = 1.0f;
        s.sky.contrast = 1.0f;

        s.bloom.enabled = false;
        s.bloom.threshold = 0.0f;
        s.bloom.strength = 0.0f;
        s.bloom.radius = 1.0f;
        return s;
    }

    static EnvironmentRenderSettings BlockWorld() {
        EnvironmentRenderSettings s = Basic();

        s.fogEnabled = true;
        s.celShadingEnabled = false;
        s.rimLightingEnabled = false;

        s.lighting.sunDirection = {-0.35f, -0.70f, 0.40f};
        s.lighting.sunColor = {1.0f, 0.96f, 0.88f};
        s.lighting.sunIntensity = 1.0f;
        s.lighting.ambientSky = {0.42f, 0.45f, 0.50f};
        s.lighting.ambientGround = {0.30f, 0.30f, 0.28f};
        s.lighting.shadowColor = {0.28f, 0.29f, 0.32f};
        s.lighting.shadowStrength = 0.28f;
        s.lighting.minLight = 0.42f;
        s.lighting.shadowBias = 0.0018f;
        s.lighting.shadowMapSoftness = 0.8f;
        s.lighting.shadowOrthoRange = 120.0f;
        s.lighting.shadowSoftness = 0.35f;
        s.lighting.fogColor = {0.58f, 0.64f, 0.72f};
        s.lighting.fogDensity = 0.0007f;
        s.lighting.celStrength = 0.0f;

        s.colorGrade.contrast = 1.04f;

        s.sky.mode = SkySettings::Mode::Color;
        s.sky.color = {0.55f, 0.72f, 0.95f};
        s.sky.intensity = 1.0f;

        s.bloom.enabled = false;
        return s;
    }

    static EnvironmentRenderSettings StylizedAction() {
        EnvironmentRenderSettings s = Basic();

        // Clean action-game graybox profile: readable first, stylish second.
        s.fogEnabled = true;
        s.celShadingEnabled = true;
        s.rimLightingEnabled = true;

        s.lighting.sunDirection = {-0.38f, -0.62f, 0.48f};
        s.lighting.sunColor = {1.00f, 0.84f, 0.72f};
        s.lighting.sunIntensity = 1.25f;
        s.lighting.ambientSky = {0.30f, 0.32f, 0.40f};
        s.lighting.ambientGround = {0.08f, 0.075f, 0.08f};
        s.lighting.shadowColor = {0.22f, 0.24f, 0.30f};
        s.lighting.shadowStrength = 0.72f;
        s.lighting.minLight = 0.22f;
        s.lighting.realtimeShadows = true;
        // Normal/stable shadow profile.
        // The fragment shader now applies receiver normal-offset + receiver-plane
        // depth correction, so this global bias stays tiny. Do not use large
        // values like 0.0015f unless you want detached / peter-panning shadows.
        s.lighting.shadowBias = 0.00014f;
        s.lighting.shadowMapSoftness = 1.0f;
        s.lighting.shadowOrthoRange = 120.0f;
        s.lighting.shadowSoftness = 0.08f;

        // Rim/shadow knobs live here now, not in RenderStyle.h.
        s.lighting.rimColor = {0.45f, 0.72f, 1.00f};
        s.lighting.rimIntensity = 0.20f;
        s.lighting.rimPower = 3.4f;

        s.lighting.fogColor = {0.99f, 0.91f, 0.56f};
        s.lighting.fogDensity = 0.00045f;
        s.lighting.celStrength = 0.65f;
        s.lighting.celBandSoftness = 0.18f;

        s.colorGrade.exposure = 1.02f;
        s.colorGrade.saturation = 1.13f;
        s.colorGrade.contrast = 1.12f;
        s.colorGrade.gamma = 2.2f;

        s.sky.enabled = true;
        s.sky.mode = SkySettings::Mode::Texture;
        s.sky.panoramaTexturePath = "assets/textures/Panorama_Sky_01-512x512.png";
        s.sky.intensity = 1.12f;
        s.sky.rotation = 2.5f;
        s.sky.saturation = 0.82f;
        s.sky.contrast = 1.25f;

        s.bloom.enabled = true;
        s.bloom.threshold = 0.0f;
        s.bloom.strength = 0.35f;
        s.bloom.radius = 1.0f;
        return s;
    }

    static EnvironmentRenderSettings DarkCelestialBrutalism() {
        EnvironmentRenderSettings s = StylizedAction();

        // Boss/finished-scene profile. Do not use as the normal graybox default.
        s.lighting.sunDirection = {-0.62f, -0.50f, 0.62f};
        s.lighting.sunColor = {1.00f, 0.72f, 0.48f};
        s.lighting.sunIntensity = 1.15f;
        s.lighting.ambientSky = {0.16f, 0.18f, 0.25f};
        s.lighting.ambientGround = {0.08f, 0.075f, 0.08f};
        s.lighting.shadowColor = {0.22f, 0.24f, 0.34f};
        s.lighting.shadowStrength = 0.58f;
        s.lighting.minLight = 0.20f;
        s.lighting.shadowBias = 0.0012f;
        s.lighting.shadowMapSoftness = 1.0f;
        s.lighting.shadowOrthoRange = 80.0f;
        s.lighting.shadowSoftness = 0.12f;
        s.lighting.rimColor = {0.36f, 0.78f, 1.00f};
        s.lighting.rimIntensity = 0.28f;
        s.lighting.rimPower = 3.0f;
        s.lighting.fogColor = {0.30f, 0.31f, 0.36f};
        s.lighting.fogDensity = 0.0035f;
        s.lighting.celStrength = 0.36f;
        s.lighting.celBandSoftness = 0.13f;

        s.colorGrade.exposure = 0.98f;
        s.colorGrade.saturation = 0.92f;
        s.colorGrade.contrast = 1.22f;
        s.colorGrade.gamma = 2.2f;

        s.sky.mode = SkySettings::Mode::Texture;
        s.sky.panoramaTexturePath = "assets/textures/Panorama_Sky_01-512x512.png";
        s.sky.intensity = 1.00f;
        s.sky.rotation = 2.5f;
        s.sky.saturation = 0.72f;
        s.sky.contrast = 1.08f;

        s.bloom.enabled = true;
        s.bloom.threshold = 0.0f;
        s.bloom.strength = 0.85f;
        s.bloom.radius = 1.15f;
        return s;
    }
};


struct DemoDebugPoint {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    float r = 1.0f;
    float g = 0.1f;
    float b = 0.0f;

    float size = 16.0f;

    std::string name;
};

class DemoEnvironment {
public:
    using EntityFactory = std::function<GameObject*(const std::string&)>;

    bool loadDefaultScene(
        EntityFactory createEntity,
        Renderer* renderer,
        AssetManager* assets,
        float sceneScale = 1.0f,
        const std::vector<AXSceneRuntimeObject>* sceneObjects = nullptr
    );

    // `sceneObjects` is the full authored object list from the .axscene
    // (AXSceneRuntimeData::objects). If it contains "NodeGroup"/"NodeRef"
    // objects (created via the editor's "Group Selected" action), their
    // position/rotation/scale - composed through NodeGroup parent chains -
    // gets applied to the matching GLB node(s) by name, on top of whatever
    // is naturally baked into the GLB. Nodes with no matching NodeRef are
    // completely unaffected (identity delta), so passing nullptr or an
    // object list with no groups is byte-for-byte the same as before.
    //
    // All the actual GLB parsing/collider/marker extraction happens in
    // AssetManager::buildStaticScene() (engine core) - this just asks for
    // the result and turns it into a renderable entity + gameplay data.
    bool loadSceneGltf(
        EntityFactory createEntity,
        Renderer* renderer,
        AssetManager* assets,
        const std::string& glbPath,
        float sceneScale = 1.0f,
        const std::vector<AXSceneRuntimeObject>* sceneObjects = nullptr
    );

    const std::vector<GameObject*>& objects() const {
        return objects_;
    }

    // Debug AABBs generated from collider meshes.
    // These are NOT the authoritative collision shape anymore.
    const std::vector<DemoAABB>& colliders() const {
        return colliders_;
    }

    // Actual mesh/triangle colliders extracted from COLLIDER_* / Invisible* objects.
    const std::vector<DemoMeshCollider>& meshColliders() const {
        return meshColliders_;
    }

    const std::vector<DemoDebugPoint>& debugPoints() const {
        return debugPoints_;
    }

    // Optional per-scene gameplay markers authored inside the scene GLB.
    // This keeps enemy/VFX placement in the Scene*.glb instead of hardcoded C++.
    const std::vector<DemoSceneMarker>& sceneMarkers() const {
        return sceneMarkers_;
    }

    void clear();

    // Script-owned visual tuning. Player.cpp should call this instead of
    // hardcoding RenderStyle values. RenderStyle remains only the engine
    // data container/fallback.
    RenderStyle makeRenderStyle(EnvironmentRenderProfile profile) const;
    void applyRenderSettings(Renderer* renderer, EnvironmentRenderProfile profile) const;

private:
    std::vector<GameObject*> objects_;

    // Debug bounds.
    std::vector<DemoAABB> colliders_;

    // Real wireframe/mesh collider data.
    std::vector<DemoMeshCollider> meshColliders_;

    std::vector<DemoDebugPoint> debugPoints_;
    std::vector<DemoSceneMarker> sceneMarkers_;

    std::shared_ptr<StaticSceneAsset> staticSceneAsset_;
};
