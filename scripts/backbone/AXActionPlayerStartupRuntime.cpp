#include "Player.h"

#include "runtime/AXSceneRuntime.h"

// AXEngine startup backbone for loading project, scene, render profile, authored data,
// player asset and scene-spawned enemies.

void Player::startActionPrototypeRuntime() {
    useShader("skinned");

    // Stage 3.1.1: mirror the old demo-controller constants into the public
    // AXCharacterKit setting structs. The implementation still lives in the
    // existing Player/Sub.cpp code for now, but user-facing scripts and future
    // engine modules can consume these Unity/Unreal-style public API types.
    axCharacterSettings_.walkSpeed = walkSpeed_;
    axCharacterSettings_.runSpeed = runSpeed_;
    axCharacterSettings_.sprintSpeed = sprintSpeed_;
    axCharacterSettings_.acceleration = acceleration_;
    axCharacterSettings_.radius = playerRadius_;
    axCharacterSettings_.height = playerHeight_;
    axCharacterSettings_.gravity = gravity_;
    axCharacterSettings_.jumpForce = jumpForce_;
    axCharacterSettings_.fallGravity = fallGravity_;
    axCharacterSettings_.jumpGravity = jumpGravity_;
    axCharacterSettings_.terminalFallSpeed = terminalFallSpeed_;
    axCharacterSettings_.maxGroundSnapDown = maxGroundSnapDown_;
    axCharacterSettings_.maxGroundSnapUp = maxGroundSnapUp_;

    axCameraSettings_.yawDeg = camYawDeg_;
    axCameraSettings_.pitchDeg = camPitchDeg_;
    axCameraSettings_.distance = camDistance_;
    axCameraSettings_.minDistance = camMinDistance_;
    axCameraSettings_.height = camHeight_;
    axCameraSettings_.groundClearance = camGroundClearance_;
    axCameraSettings_.mouseSensitivity = mouseSensitivity_;

    gameplayRuntime_.clear();
    gameplayRuntime_.setDebugSink([this](const std::string& msg) { this->print(msg); });
    gameplayFrame_ = 0;
    playerRuntimeId_.clear();

    AXProjectData project;
    std::string projectError;
    runtimeScenePath_ = "assets/scenes/Scene1.axscene";
    if (AXProject::load("project.axproject", project, &projectError)) {
        runtimeProjectName_ = project.name;
        runtimeScenePath_ = project.startupScene.empty()
            ? runtimeScenePath_
            : AXSceneRuntime::runtimeAssetPath(project.startupScene);
        print("AXProject loaded: " + runtimeProjectName_ + " startupScene=" + runtimeScenePath_);
    } else {
        print("AXProject fallback: " + projectError);
    }

    AXSceneRuntimeData axScene;
    std::string axSceneError;
    bool axSceneLoaded = AXSceneRuntime::load(
        runtimeScenePath_,
        axScene,
        &axSceneError
    );

    auto sceneHasRenderableRoot = [](const AXSceneRuntimeData& scene) {
        if (!AXSceneRuntime::runtimeAssetPath(scene.previewGlb).empty()) {
            return true;
        }

        for (const AXSceneRuntimeObject& obj : scene.objects) {
            std::string t = Player::lowerStr(obj.type);
            if ((t == "asset" || t == "staticscene") && !obj.sourcePath.empty()) {
                return true;
            }
        }

        return false;
    };

    // Empty.axscene is a useful editor template, but it is not a runnable game
    // scene. If the project file points to Empty or any scene without a GLB
    // root, do not load the fallback GLB under the wrong Basic render profile.
    // Promote the real authored Scene1.axscene before render settings are applied.
    if (!axSceneLoaded || !sceneHasRenderableRoot(axScene)) {
        const std::string fallbackScene = "assets/scenes/Scene1.axscene";
        if (runtimeScenePath_ != fallbackScene) {
            print("AXScene startup has no renderable GLB root; switching to " + fallbackScene);

            AXSceneRuntimeData fallbackData;
            std::string fallbackError;
            if (AXSceneRuntime::load(fallbackScene, fallbackData, &fallbackError) && sceneHasRenderableRoot(fallbackData)) {
                axScene = fallbackData;
                axSceneLoaded = true;
                runtimeScenePath_ = fallbackScene;
                axSceneError.clear();
            } else {
                print("AXScene fallback failed: " + fallbackError);
            }
        }
    }

    auto profileFromName = [](const std::string& name) {
        std::string n = Player::lowerStr(name);
        if (n.find("basic") != std::string::npos) return EnvironmentRenderProfile::Basic;
        if (n.find("block") != std::string::npos) return EnvironmentRenderProfile::BlockWorld;
        if (n.find("dark") != std::string::npos || n.find("celestial") != std::string::npos) {
            return EnvironmentRenderProfile::DarkCelestialBrutalism;
        }
        return EnvironmentRenderProfile::StylizedAction;
    };

    const EnvironmentRenderProfile sceneProfile = axSceneLoaded
        ? profileFromName(axScene.renderProfile)
        : EnvironmentRenderProfile::StylizedAction;

    // AXScene owns the scene render profile now. If the file is missing, keep
    // the old StylizedAction fallback so the game still runs.
    environment_.applyRenderSettings(renderer(), sceneProfile);

    if (axSceneLoaded) {
        print("AXScene loaded: " + axScene.name);
        emitGameplayEvent(ax::events::SceneLoaded, "Runtime", {}, runtimeScenePath_, 0.0f, Vec3{}, Vec3{});
    } else {
        print("AXScene fallback: " + axSceneError);
    }

    bindKey("Forward",     'W');
    bindKey("Backward",    'S');
    bindKey("Left",        'A');
    bindKey("Right",       'D');
    bindKey("ToggleSword", 'R');
    bindKey("DebugInfo",   'I');
    bindKey("ToggleColliders", 'C');
    bindKey("Dash", 'E');

    ax::InputMapDefinition inputMap;
    std::string inputError;
    if (ax::loadInputMapFile("assets/input/Default.axinput", inputMap, &inputError)) {
        for (const ax::InputBinding& binding : inputMap.bindings) {
            const std::string keyName = ax::firstKeyboardToken(binding.keyboard);
            if (!keyName.empty() && keyName != "Mouse" && keyName != "WASD") {
                bindKeyName(binding.action, keyName);
            }
        }
        inputBufferSeconds_ = inputMap.bufferSeconds;
        print("Input map loaded: assets/input/Default.axinput actions=" + std::to_string(inputMap.bindings.size()) +
              " buffer=" + std::to_string(inputBufferSeconds_));
    }

    loadAuthoredCombatAssets();
    loadAuthoredEnemyAssets();
    loadAuthoredVfxAssets();

    if (axSceneLoaded) {
        std::string sceneGlb = AXSceneRuntime::runtimeAssetPath(axScene.previewGlb);
        float sceneScale = axScene.unitScale > 0.0001f ? axScene.unitScale : 1.0f;

        // The editor writes imported GLB roots as type "Asset". Older runtime
        // code only looked for "StaticScene", so the game loaded the right
        // GLB file but ignored the root object's authored scale. That made Game
        // behave like it was using the raw GLB size even after scaling Scene1 in
        // AXEditor. Treat the root Asset/StaticScene object as the renderable
        // scene root and use its transform as the whole-level transform.
        const AXSceneRuntimeObject* sceneRoot = nullptr;

        auto sameRuntimePath = [](std::string a, std::string b) {
            a = AXSceneRuntime::runtimeAssetPath(a);
            b = AXSceneRuntime::runtimeAssetPath(b);
            return !a.empty() && !b.empty() && a == b;
        };

        for (const AXSceneRuntimeObject& obj : axScene.objects) {
            const std::string t = Player::lowerStr(obj.type);
            const bool renderableRootType = (t == "staticscene" || t == "asset");
            if (!renderableRootType || obj.sourcePath.empty()) {
                continue;
            }

            // Prefer the object that represents the preview/main GLB.
            if (sameRuntimePath(obj.sourcePath, axScene.previewGlb)) {
                sceneRoot = &obj;
                break;
            }

            // Otherwise accept a top-level renderable GLB root as fallback.
            if (!sceneRoot && obj.parentIndex < 0) {
                sceneRoot = &obj;
            }
        }

        if (sceneRoot) {
            if (!sceneRoot->sourcePath.empty()) {
                sceneGlb = AXSceneRuntime::runtimeAssetPath(sceneRoot->sourcePath);
            }

            if (sceneRoot->scale[0] > 0.0001f) {
                sceneScale = sceneRoot->scale[0];
            }

            print("AXScene root object: " + sceneRoot->name +
                  " scale=" + std::to_string(sceneScale));
        }

        if (!environment_.loadSceneGltf(
                [this](const std::string& name) {
                    return this->createEntity(name);
                },
                renderer(),
                assets(),
                sceneGlb,
                sceneScale,
                &axScene.objects
            )) {
            print("AXScene GLB load failed, falling back to default Scene1.glb");
            environment_.loadDefaultScene(
                [this](const std::string& name) {
                    return this->createEntity(name);
                },
                renderer(),
                assets(),
                1.5f
            );
        }

        if (const AXSceneRuntimeObject* spawn = axScene.firstObjectOfType("PlayerStart")) {
            posX_ = spawn->position[0];
            posY_ = spawn->position[1];
            posZ_ = spawn->position[2];
            yawDeg_ = spawn->rotation[1];
            print("PlayerStart from AXScene: " + spawn->name);
        }
    } else {
        environment_.loadDefaultScene(
            [this](const std::string& name) {
                return this->createEntity(name);
            },
            renderer(),
            assets(),
            1.5f
        );
    }

    rebuildColliderDebugLines();
    printColliderList();

    // Snap player to ground only if a collider triangle actually exists under spawn.
    float spawnGroundY = 0.0f;
    if (findGroundYAt(posX_, posZ_, spawnGroundY)) {
        groundY_ = spawnGroundY;
        posY_ = groundY_;
        grounded_ = true;
    } else {
        groundY_ = posY_;
        grounded_ = false;
        verticalVelocity_ = 0.0f;
    }

    player_ = createEntity("Player");
    player_->setPosition(posX_, posY_, posZ_);
    player_->setRotation(0.0f, yawDeg_, 0.0f);
    player_->setScale(1.0f, 1.0f, 1.0f);
    player_->setSkinnedMesh("assets/Man1_Cinema.glb");

    if (auto* src = player_->skinnedRender()) {
        setSwordVisible(src, true);
        hideDebugPrimitive(src);
        src->play("Idle", true);

        registerPlayerGameplayObject();

        print("Player loaded.");
        print("Controls:");
        print("  WASD        move");
        print("  Left Ctrl   walk");
        print("  Left Shift  sprint");
        print("  Space       jump");
        print("  Mouse       orbit camera");
        print("  R           toggle swords");
        print("  I           debug info");
        print("  C           toggle collider wireframes");
        print("  F3          toggle FPS debug");
        print("  F4          toggle light debug");
        print("  Escape      release mouse");
        print("  Left click  combo attack / capture mouse");
        print("  Hold+release LMB Sword OFF  SpinKick");
        print("  Hold+release LMB Sword ON   HomingSword");
        print("  Shift+LMB   dash attack (Impale/Cross)");
        print("  Air+LMB     strike down (AxeKick/Helm Breaker)");
        print("  E           Dash1");
    }

    int axSceneEnemiesSpawned = 0;
    if (axSceneLoaded) {
        for (const AXSceneRuntimeObject& obj : axScene.objects) {
            if (Player::lowerStr(obj.type) != "enemy") {
                continue;
            }

            const std::string archetype = AXSceneRuntime::archetypeFromEnemyObject(obj);
            spawnEnemy(
                archetype,
                obj.position[0],
                obj.position[1],
                obj.position[2],
                obj.rotation[1]
            );
            ++axSceneEnemiesSpawned;
        }

        if (axSceneEnemiesSpawned > 0) {
            print("Enemies spawned from AXScene: " + std::to_string(axSceneEnemiesSpawned));
        }
    }

    // If the .axscene has no Enemy objects, keep the older Blender marker workflow.
    if (axSceneEnemiesSpawned == 0) {
        spawnEnemiesFromScene();
    }

    setMouseCaptured(true);
    updateCamera(0.016f);
}

