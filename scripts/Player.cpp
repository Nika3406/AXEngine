#include "Player.h"
#include "AXEngine/Gameplay/Events.h"
#include "runtime/AXSceneRuntime.h"
#include "runtime/AXProject.h"


// player lifecycle/input-facing update table (Capcom-style split)
const char* Player::getTitle() const { return "AXEngine - Demo"; }
int Player::getWidth() const { return 1280; }
int Player::getHeight() const { return 720;  }

void Player::onStart() {
    // Startup asset/scene/runtime bootstrap lives in AXEngine.
    // The script keeps the gameplay loop below readable.
    startActionPrototypeRuntime();
}

void Player::onUpdate(float dt) {
    gameplayRuntime_.beginFrame(++gameplayFrame_);
    updateFpsDebug(dt);

    if (!player_) return;

    auto* src = player_->skinnedRender();
    if (!src) return;

    if (rawKeyPressed(SDL_SCANCODE_ESCAPE)) {
        setMouseCaptured(false);
    }

    if (!mouseCaptured_) {
        float mx = 0.0f;
        float my = 0.0f;
        Uint32 buttons = SDL_GetMouseState(&mx, &my);
        if (buttons & SDL_BUTTON_LMASK) {
            setMouseCaptured(true);
        }
    }

    if (keyPressed("ToggleSword")) {
        if (combatState_ != CombatState::None) {
            endCurrentAction(src);
        }
        setSwordVisible(src, !swordVisible_);
    }

    if (keyPressed("DebugInfo")) {
        printDebugInfo(src);
    }

    if (keyPressed("ToggleColliders")) {
        showColliderWireframes_ = !showColliderWireframes_;
        rebuildColliderDebugLines();
        print(std::string("Collider wireframes: ") +
              (showColliderWireframes_ ? "ON" : "OFF"));
    }

    if (rawKeyPressed(SDL_SCANCODE_F4)) {
        showLightDebug_ = !showLightDebug_;

        rebuildColliderDebugLines();

        if (renderer() && showLightDebug_) {
            renderer()->addLightDebugLines();
        }

        print(std::string("Light debug: ") +
              (showLightDebug_ ? "ON" : "OFF"));
    }

    if (rawKeyPressed(SDL_SCANCODE_F3)) {
        showFpsDebug_ = !showFpsDebug_;

        print(std::string("FPS debug: ") +
            (showFpsDebug_ ? "ON" : "OFF"));
    }

    if (keyPressed("Dash")) {
        emitGameplayEvent(ax::events::InputAction, playerRuntimeId_, {}, "Dash", 1.0f, Vec3{posX_, posY_, posZ_}, playerForward());
        handleDashInput(src);
    }

    if (mouseCaptured_) {
        handleLeftMouseCombatInput(src, dt);
    }

    updateCombat(src, dt);
    updateSpecialMove(src, dt);
    applyPlayerAttackHits();
    applyPlayerSpecialHits();

    updateEnemies(dt);
    updateCombatVfx(dt);

    handleMovementInput(dt);
    handleJumpInput();
    updateVerticalMotion(dt);
    updateAnimationState(src, dt);

    // AnimationSystem now extracts root-motion delta from the selected
    // root joint. We consume that delta immediately afterward and move
    // the player/world transform with it.
    AnimationSystem::update(*src, dt);
    applyExtractedRootMotion(src);

    player_->setPosition(posX_, posY_, posZ_);
    player_->setRotation(0.0f, yawDeg_, 0.0f);

    updateCamera(dt);

    rebuildColliderDebugLines();

    updateGameplayRuntimeObjects();
    gameplayRuntime_.endFrame();

    if (showLightDebug_) {
        if (renderer()) {
            renderer()->addLightDebugLines();
        }
    }
}
