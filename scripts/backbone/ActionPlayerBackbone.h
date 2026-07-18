#pragma once

#include "Game.h"
#include "OGL3D/render.h"
#include "OGL3D/RenderStyle.h"
#include "ecs/SkinnedRenderComponent.h"
#include "ecs/AnimationSystem.h"
#include "Environment.h"
#include "runtime/AXAttackColliderSystem.h"
#include "runtime/AXEngineSystems.h"
#include "runtime/AXGameplayRuntime.h"
#include "runtime/AXProject.h"
#include "AXEngine/AXEngine.h"
#include "AXEngine/AXCharacterKit.h"
#include "AXEngine/Gameplay/Events.h"

#include <SDL3/SDL.h>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <limits>
#include <string>
#include <vector>
#include <unordered_map>

namespace ax::game {

// Project-owned state/storage for the bundled third-person action template.
// Reusable mechanics belong in AXEngine kits; this class composes them for this game.
class ActionPlayerBackbone : public Game {
protected:
    struct Vec3 {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    enum class AnimState {
        Idle,
        Walk,
        Run,
        Sprint,
        JumpIdle
    };

    enum class CombatState {
        None,
        Attack,
        Dash,
        DashAttack,
        AirAttack,
        ChargeAttack,
        HomingSword
    };

    enum class ComboType {
        Hands,
        Sword
    };

    enum class EnemyState {
        Idle,
        Chase,
        Attack,
        HitStun,
        Dead
    };

    struct AttackDef {
        const char* clip = "";
        float animSpeed = 1.0f;

        // Player input may be buffered inside [bufferStart, bufferEnd],
        // but the combo does not actually cut until cancelAt. This removes
        // the janky "click timing decides animation cut timing" feel.
        float bufferStart = 0.10f;
        float cancelAt = 0.55f;
        float bufferEnd = 0.82f;

        // Future hitbox timing. Not used against enemies yet, but useful for
        // debug arcs/hitboxes and proper damage windows.
        float hitStart = 0.20f;
        float hitEnd = 0.42f;

        bool useRootMotion = true;
        float recoveryEnd = 0.92f;
        int damage = 0;
    };

    struct EnemyActor {
        GameObject* object = nullptr;
        std::string name;
        std::string archetype = "Basic";
        std::string glbPath = "assets/Man1_Cinema.glb";

        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float yawDeg = 0.0f;

        float radius = 0.48f;
        float height = 1.85f;

        int hp = 160;
        int maxHp = 160;
        bool alive = true;

        EnemyState state = EnemyState::Idle;
        float stateTimer = 0.0f;
        float attackTimer = 0.0f;
        float attackDuration = 0.85f;
        float attackCooldown = 0.0f;
        float detectionRange = 18.0f;
        float preferredRange = 9.0f;
        float attackRange = 1.55f;
        float baseAttackCooldown = 1.25f;
        float attackActiveStart = 0.32f;
        float attackActiveEnd = 0.52f;
        float moveSpeed = 2.65f;
        int attackDamage = 10;
        bool attackHitApplied = false;

        float hitStunTimer = 0.0f;
        float knockVelX = 0.0f;
        float knockVelZ = 0.0f;

        int lastHitSerial = -1;
        std::string runtimeId;
    };

    struct CombatVfx {
        Vec3 origin;
        Vec3 dir;
        float age = 0.0f;
        float duration = 0.18f;
        float radius = 1.0f;
        float r = 1.0f;
        float g = 0.85f;
        float b = 0.35f;
    };



    DemoEnvironment environment_;
    GameObject* player_ = nullptr;

    // Public AXEngine module settings mirrored by the current demo controller.
    // These are the bridge toward AXCharacterKit so user scripts can reason in
    // engine-library terms instead of raw SDL/collision implementation terms.
    ax::character::CharacterControllerSettings axCharacterSettings_{};
    ax::character::ThirdPersonCameraSettings axCameraSettings_{};


    // Player world position. posY_ is treated as the feet/root position.
    float posX_ = 0.0f;
    float posY_ = 0.0f;
    float posZ_ = 0.0f;

    float groundY_ = 0.0f;
    float yawDeg_ = 180.0f;

    float currentSpeed_ = 0.0f;
    float targetSpeed_ = 0.0f;

    // Actual horizontal speed after collision resolution. This is different
    // from currentSpeed_, which is the desired/controller speed. If the
    // player is sprinting into a wall, currentSpeed_ may still be high, but
    // actualGroundSpeed_ drops near zero so animation can fall back to walk.
    float actualGroundSpeed_ = 0.0f;
    bool movementBlocked_ = false;

    float moveDirX_ = 0.0f;
    float moveDirZ_ = 0.0f;

    bool hasMoveInput_ = false;

    float walkSpeed_ = 2.2f;
    float runSpeed_ = 5.0f;
    float sprintSpeed_ = 7.5f;
    float acceleration_ = 10.0f;

    bool grounded_ = true;
    float verticalVelocity_ = 0.0f;
    float gravity_ = 38.0f;
    float jumpForce_ = 16.0f;

    float fallGravity_ = 78.5f;
    float jumpGravity_ = 28.0f;
    float terminalFallSpeed_ = -22.0f;

    float strikeDownTerminalFallSpeed_ = -48.0f;

    int airJumpsUsed_ = 0;
    int maxAirJumps_ = 1;

    float airStepForce_ = 12.5f;
    float airStepAnimTimer_ = 0.0f;
    float airStepAnimDuration_ = 0.15f;
    bool airStepAnimating_ = false;

    float edgeFallDelay_ = 0.16f;
    float edgeFallTimer_ = 0.0f;

    float maxGroundSnapDown_ = 0.35f;
    float maxGroundSnapUp_ = 0.45f;

    // Player collision cylinder.
    float playerRadius_ = 0.45f;
    float playerHeight_ = 1.85f;

    AnimState animState_ = AnimState::Idle;
    std::string currentClip_ = "Idle";

    CombatState combatState_ = CombatState::None;
    ComboType activeComboType_ = ComboType::Hands;
    int comboIndex_ = 0;
    bool attackQueued_ = false;      // buffered next-combo request
    bool comboConsumed_ = false;     // prevents one buffered click from chaining twice
    float attackTimer_ = 0.0f;
    float attackDuration_ = 0.0f;
    float attackNormalized_ = 0.0f;
    int attackSerial_ = 0;

    // Legacy fallback values. The active combo now uses AttackDef data below.
    float comboWindowStart_ = 0.20f;
    float comboWindowEnd_ = 0.78f;

    std::string specialClip_;
    float specialTimer_ = 0.0f;
    float specialDuration_ = 0.0f;

    float specialMoveSpeed_ = 0.0f;
    float specialDownSpeed_ = 0.0f;
    bool specialUsesScriptedMotion_ = false;

    // Burst-motion timing.
    // This separates animation duration from physical movement duration.
    float specialMotionTimer_ = 0.0f;
    float specialMotionDuration_ = 0.0f;

    bool airAttackLandingPlayed_ = false;

    bool lmbHeld_ = false;
    float lmbHoldTimer_ = 0.0f;
    float chargedReleaseThreshold_ = 0.22f;

    int homingSwordPhase_ = 0;
    float homingTimer_ = 0.0f;
    float homingDuration_ = 0.45f;
    float homingMoveSpeed_ = 18.0f;
    float homingUpSpeed_ = 7.0f;

    // HomingSwordIdle behaves like a player-steered missile.
    // It stays in idle until it hits scene collision or times out.
    float homingMissileRadius_ = 0.55f;
    float homingMissileMaxTime_ = 1.25f;
    float homingMissileTimer_ = 0.0f;
    float homingMissileSpeed_ = 32.0f;

    bool wasLeftMouseDown_ = false;

    int rootMotionJointIndex_ = -1;
    float rootMotionScale_ = 1.0f;

    bool walkHeld_ = false;
    bool sprintHeld_ = false;

    bool swordVisible_ = true;
    bool mouseCaptured_ = false;
    bool showColliderWireframes_ = false;
    bool showLightDebug_ = false;
    bool prevRawKeys_[SDL_SCANCODE_COUNT] = {};

    bool showFpsDebug_ = true;
    bool showAttackDebug_ = true;

    ax::GameplayRuntime gameplayRuntime_;
    std::string playerRuntimeId_;
    uint64_t gameplayFrame_ = 0;

    ax::AttackColliderSystem attackColliderSystem_;
    std::unordered_map<std::string, ax::AttackDefinition> authoredAttacks_;
    std::unordered_map<std::string, ax::EnemyDefinition> authoredEnemies_;
    std::unordered_map<std::string, ax::VFXDefinition> authoredVfx_;
    bool useAuthoredAttackVolumes_ = true;
    std::string runtimeProjectName_ = "AXEngine";
    std::string runtimeScenePath_ = "assets/scenes/Scene1.axscene";
    float inputBufferSeconds_ = 0.18f;
    float authoredHitVfxLifetime_ = 0.16f;

    std::vector<EnemyActor> enemies_;
    std::vector<CombatVfx> combatVfx_;

    float fpsTimer_ = 0.0f;
    int fpsFrameCount_ = 0;
    float fps_ = 0.0f;
    float frameMs_ = 0.0f;
    float minFrameMs_ = 9999.0f;
    float maxFrameMs_ = 0.0f;

    float camYawDeg_ = 180.0f;
    float camPitchDeg_ = 18.0f;
    float camDistance_ = 9.0f;
    float camMinDistance_ = 0.5f;
    float camHeight_ = 2.0f;
    float camGroundClearance_ = 0.35f;

    float camX_ = 0.0f;
    float camY_ = 2.0f;
    float camZ_ = 9.0f;

    float camTargetX_ = 0.0f;
    float camTargetY_ = 0.0f;
    float camTargetZ_ = 0.0f;

    float mouseSensitivity_ = 0.12f;

};

} // namespace ax::game
