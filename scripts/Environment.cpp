#include "Environment.h"

#include "OGL3D/render.h"
#include "assets/AssetManager.h"
#include "ecs/Entity.h"
#include "ecs/StaticSceneRenderComponent.h"

#include <iostream>

// ---------------------------------------------------------------------------
// DemoEnvironment public API
// ---------------------------------------------------------------------------
// Everything below is deliberately thin: GLB parsing, material/image/mesh
// extraction, collider extraction, gameplay-marker extraction, and normal
// computation all live in AssetManager::buildStaticScene() (engine core,
// src/assets/StaticSceneLoader.cpp) - the same place buildSkinnedMesh()
// lives for character meshes. This file's job is just to ask for that
// result and decide what a *level* does with it: attach it to a renderable
// entity, apply a render profile, and expose collider/marker data to the
// rest of scripts/ (Sub.cpp, EnemyAI.cpp, Player.cpp).

void DemoEnvironment::clear() {
    objects_.clear();
    colliders_.clear();
    meshColliders_.clear();
    debugPoints_.clear();
    sceneMarkers_.clear();
    staticSceneAsset_.reset();
}

RenderStyle DemoEnvironment::makeRenderStyle(EnvironmentRenderProfile profile) const {
    EnvironmentRenderSettings settings;

    switch (profile) {
        case EnvironmentRenderProfile::Basic:
            settings = EnvironmentRenderSettings::Basic();
            break;
        case EnvironmentRenderProfile::BlockWorld:
            settings = EnvironmentRenderSettings::BlockWorld();
            break;
        case EnvironmentRenderProfile::StylizedAction:
            settings = EnvironmentRenderSettings::StylizedAction();
            break;
        case EnvironmentRenderProfile::DarkCelestialBrutalism:
            settings = EnvironmentRenderSettings::DarkCelestialBrutalism();
            break;
    }

    RenderStyle style;
    style.stylizedLightingEnabled = settings.stylizedLightingEnabled;
    style.skyEnabled = settings.skyEnabled;
    style.fogEnabled = settings.fogEnabled;
    style.celShadingEnabled = settings.celShadingEnabled;
    style.rimLightingEnabled = settings.rimLightingEnabled;
    style.bloomEnabled = settings.bloom.enabled;
    style.lighting = settings.lighting;
    style.colorGrade = settings.colorGrade;
    style.sky = settings.sky;
    style.bloom.enabled = settings.bloom.enabled;
    style.bloom.threshold = settings.bloom.threshold;
    style.bloom.strength = settings.bloom.strength;
    style.bloom.radius = settings.bloom.radius;
    return style;
}

void DemoEnvironment::applyRenderSettings(Renderer* renderer, EnvironmentRenderProfile profile) const {
    if (!renderer) return;

    RenderStyle style = makeRenderStyle(profile);
    EnvironmentRenderSettings settings; // re-derive bloom scalars for setBloomSettings below
    switch (profile) {
        case EnvironmentRenderProfile::Basic:
            settings = EnvironmentRenderSettings::Basic();
            break;
        case EnvironmentRenderProfile::BlockWorld:
            settings = EnvironmentRenderSettings::BlockWorld();
            break;
        case EnvironmentRenderProfile::StylizedAction:
            settings = EnvironmentRenderSettings::StylizedAction();
            break;
        case EnvironmentRenderProfile::DarkCelestialBrutalism:
            settings = EnvironmentRenderSettings::DarkCelestialBrutalism();
            break;
    }

    renderer->setRenderStyle(style);
    renderer->setBloomEnabled(settings.bloom.enabled);
    renderer->setBloomSettings(
        settings.bloom.threshold,
        settings.bloom.strength,
        settings.bloom.radius
    );
}

bool DemoEnvironment::loadSceneGltf(
    EntityFactory createEntity,
    Renderer* renderer,
    AssetManager* assets,
    const std::string& glbPath,
    float sceneScale,
    const std::vector<AXSceneRuntimeObject>* sceneObjects
) {
    clear();

    if (!assets) {
        std::cerr << "[DemoEnvironment] loadSceneGltf: no AssetManager provided.\n";
        return false;
    }

    // AssetManager::buildStaticScene() resolves the path itself (same as
    // buildSkinnedMesh()) - pass glbPath straight through. Resolving it here
    // too would double-apply the base path and produce a nonexistent file.
    AXStaticSceneLoadResult result = assets->buildStaticScene(glbPath, sceneScale, sceneObjects);

    if (!result.ok || !result.scene) {
        std::cerr << "[DemoEnvironment] Failed to load scene: " << result.error << "\n";
        return false;
    }

    staticSceneAsset_ = result.scene;
    colliders_ = std::move(result.colliderDebugAABBs);
    meshColliders_ = std::move(result.meshColliders);
    sceneMarkers_ = std::move(result.sceneMarkers);

    DemoDebugPoint origin;
    origin.x = 0.0f; origin.y = 0.0f; origin.z = 0.0f;
    origin.r = 1.0f; origin.g = 0.1f; origin.b = 0.0f;
    origin.size = 18.0f;
    origin.name = "Scene GLB Origin";
    debugPoints_.push_back(origin);

    if (createEntity && renderer && !staticSceneAsset_->primitives.empty()) {
        GameObject* sceneObj = createEntity("SceneGLB");

        if (sceneObj) {
            sceneObj->setPosition(0.0f, 0.0f, 0.0f);
            sceneObj->setRotation(0.0f, 0.0f, 0.0f);
            sceneObj->setScale(1.0f, 1.0f, 1.0f);

            auto* comp = sceneObj->_entity()->addComponent<StaticSceneRenderComponent>();
            comp->asset = staticSceneAsset_;

            renderer->uploadStaticScene(*comp);

            objects_.push_back(sceneObj);

            std::cout << "[DemoEnvironment] Created renderable SceneGLB entity.\n";
        }
    }

    return true;
}

bool DemoEnvironment::loadDefaultScene(
    EntityFactory createEntity,
    Renderer* renderer,
    AssetManager* assets,
    float sceneScale,
    const std::vector<AXSceneRuntimeObject>* sceneObjects
) {
    return loadSceneGltf(
        createEntity,
        renderer,
        assets,
        "assets/Scene1.glb",
        sceneScale,
        sceneObjects
    );
}
