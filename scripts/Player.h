#pragma once

#include "backbone/ActionPlayerBackbone.h"

// User-facing controller for the Unbound/action prototype.
// This file should stay readable: gameplay decisions live here; template input, collision, movement, camera, combo, combat, AI and VFX composition lives under scripts/backbone/.
class Player : public ax::game::ActionPlayerBackbone {
public:
    const char* getTitle() const override;
    int getWidth() const override;
    int getHeight() const override;

    void onStart() override;
    void onUpdate(float dt) override;

private:
    using Vec3 = ax::game::ActionPlayerBackbone::Vec3;
    using AnimState = ax::game::ActionPlayerBackbone::AnimState;
    using CombatState = ax::game::ActionPlayerBackbone::CombatState;
    using ComboType = ax::game::ActionPlayerBackbone::ComboType;
    using EnemyState = ax::game::ActionPlayerBackbone::EnemyState;
    using AttackDef = ax::game::ActionPlayerBackbone::AttackDef;
    using EnemyActor = ax::game::ActionPlayerBackbone::EnemyActor;
    using CombatVfx = ax::game::ActionPlayerBackbone::CombatVfx;

    static Vec3 vecAdd(const Vec3& a, const Vec3& b);
    static Vec3 vecSub(const Vec3& a, const Vec3& b);
    static Vec3 vecMul(const Vec3& v, float s);
    static float vecDot(const Vec3& a, const Vec3& b);
    static Vec3 vecCross(const Vec3& a, const Vec3& b);
    static float vecLen(const Vec3& v);
    static Vec3 vecNorm(const Vec3& v);
    static Vec3 fromDemoVec3(const DemoVec3& v);
    static Vec3 triNormal(const DemoColliderTriangle& tri);
    static std::string lowerStr(const std::string& s);
    static bool strContains(const std::string& s, const std::string& token);

    bool rawKeyDown(SDL_Scancode scancode) const;
    bool rawKeyPressed(SDL_Scancode scancode);
    void setMouseCaptured(bool captured);
    bool leftMousePressed();
    bool leftMouseDownRaw() const;
    void setSwordVisible(SkinnedRenderComponent* src, bool visible);
    void hideDebugPrimitive(SkinnedRenderComponent* src);

    bool isGroundTriangle(const DemoColliderTriangle& tri) const;
    bool isWallTriangle(const DemoColliderTriangle& tri) const;
    bool pointInTriangleXZ(float px, float pz, const DemoColliderTriangle& tri, float& outY) const;
    bool capsuleFootprintHitsTriangleXZ(float x, float z, float radius, const DemoColliderTriangle& tri, float& outY) const;
    bool findGroundYAt(float x, float z, float& outGroundY) const;
    bool findCeilingYBetween(float oldFeetY, float newFeetY, float& outCeilingY) const;
    static float clamp01(float v);
    void closestPointSegmentXZ(float px, float pz, const Vec3& a, const Vec3& b, float& outX, float& outZ, float& outDistSq) const;
    bool verticalOverlapWithPlayer(const DemoColliderTriangle& tri, float playerY) const;
    bool resolveAgainstTriangleXZ(const DemoColliderTriangle& tri, float& x, float y, float& z) const;
    void resolvePlayerCollisions(float& x, float& y, float& z);
    bool movePlayerXZWithCollision(float dx, float dz);
    bool sphereHitsTriangleXZ(float x, float y, float z, float radius, const DemoColliderTriangle& tri) const;
    bool homingSwordHitCollider() const;

    void handleMovementInput(float dt);
    void handleJumpInput();
    void updateVerticalMotion(float dt);
    bool rayTriangle(const Vec3& origin, const Vec3& end, const DemoColliderTriangle& tri, float& outT) const;
    Vec3 resolveCameraCollision(const Vec3& target, const Vec3& desiredCam) const;
    Vec3 resolveCameraSpherePushout(const Vec3& camera) const;
    void updateCamera(float dt);
    void rebuildColliderDebugLines();
    void printColliderList();
    std::string combatStateName() const;
    void updateFpsDebug(float dt);
    void printDebugInfo(SkinnedRenderComponent* src);

    void updateAnimationState(SkinnedRenderComponent* src, float dt);
    void setAnim(SkinnedRenderComponent* src, AnimState nextState, const std::string& clip, bool loop, float fade);
    const AttackDef& attackDef(ComboType type, int index) const;
    const AttackDef& currentAttackDef() const;
    const char* comboClip(ComboType type, int index) const;
    float comboDuration(SkinnedRenderComponent* src, ComboType type, int index) const;
    float comboAnimSpeed(ComboType type, int index) const;
    float specialAnimSpeed(const std::string& clip) const;
    float specialFallbackDuration(const std::string& clip) const;
    float clipDurationOrFallback(SkinnedRenderComponent* src, const std::string& clip, float fallback) const;
    float defaultSpecialMoveSpeed(const std::string& clip) const;
    float defaultSpecialDownSpeed(const std::string& clip) const;
    bool isInComboWindow() const;
    bool isAttackHitActive() const;

    int findRootMotionJoint(SkinnedRenderComponent* src);
    void enableRootMotionForAttack(SkinnedRenderComponent* src);
    void disableRootMotion(SkinnedRenderComponent* src);
    void endCurrentAction(SkinnedRenderComponent* src);
    void handleLeftMouseCombatInput(SkinnedRenderComponent* src, float dt);
    void handleChargedReleaseInput(SkinnedRenderComponent* src);
    void handleAttackInput(SkinnedRenderComponent* src);
    void handleDashInput(SkinnedRenderComponent* src);
    void handleDashAttackInput(SkinnedRenderComponent* src);
    void handleAirAttackInput(SkinnedRenderComponent* src);
    void startSpinKick(SkinnedRenderComponent* src);
    void startHomingSword(SkinnedRenderComponent* src);
    void startAttack(SkinnedRenderComponent* src, ComboType type, int index);
    void startSpecialMove(SkinnedRenderComponent* src, CombatState state, const std::string& clip, bool scriptedMotion, float moveSpeedOverride = -1.0f, float downSpeedOverride = -1.0f);
    void endCombat(SkinnedRenderComponent* src);
    void endSpecialMove(SkinnedRenderComponent* src);
    void updateCombat(SkinnedRenderComponent* src, float dt);
    float specialMotionDurationForClip(const std::string& clip) const;
    void configureSpecialScriptedMotion(const std::string& clip, bool scripted, float overrideSpeed);
    void updateAirAttack(SkinnedRenderComponent* src);
    void applySpecialScriptedMotion(float dt);
    void updateSpecialMove(SkinnedRenderComponent* src, float dt);
    void startHomingSwordLand(SkinnedRenderComponent* src);
    void updateHomingSword(SkinnedRenderComponent* src, float dt);
    bool applyHomingSwordIdleMotion(float dt);
    void applyExtractedRootMotion(SkinnedRenderComponent* src);

    // Enemy / hit / VFX layer. Kept in scripts/, modeled after the old
    // routine-table/action-table style instead of engine-side behavior.
    void spawnEnemiesFromScene();
    void spawnEnemy(const std::string& archetype, float x, float y, float z, float yawDeg);
    std::string enemyGlbForArchetype(const std::string& archetype) const;
    bool enemyHasClip(const EnemyActor& enemy, const std::string& clip) const;
    void setEnemyAnim(EnemyActor& enemy, const std::string& clip, bool loop, float fade, float speed = 1.0f);
    float enemyClipDurationOrFallback(const EnemyActor& enemy, const std::string& clip, float fallback) const;
    void updateEnemies(float dt);
    void updateEnemy(EnemyActor& enemy, float dt);
    void faceEnemyToward(EnemyActor& enemy, float x, float z, float turnSpeedDeg, float dt);
    bool moveEnemyXZWithCollision(EnemyActor& enemy, float dx, float dz);
    bool enemyCanSeePlayer(const EnemyActor& enemy) const;
    void startEnemyAttack(EnemyActor& enemy);
    void damageEnemy(EnemyActor& enemy, int damage, const Vec3& hitPoint, const Vec3& hitDir, float knockback);
    float enemyDistanceSqXZ(const EnemyActor& enemy, float x, float z) const;

    Vec3 playerForward() const;
    Vec3 attackHitCenter(float range) const;
    float attackHitRange() const;
    float attackHitRadius() const;
    bool enemyInsidePlayerHit(const EnemyActor& enemy, const Vec3& center, float radius, float arcCos) const;
    void applyPlayerAttackHits();
    bool isSpecialHitActive() const;
    int specialDamage() const;
    float specialHitRange() const;
    float specialHitRadius() const;
    void applyPlayerSpecialHits();

    void spawnHitVfx(const Vec3& point, const Vec3& dir, float radius, float r, float g, float b);
    void updateCombatVfx(float dt);
    void appendCombatDebugLines();
    void loadAuthoredCombatAssets();
    void loadAuthoredEnemyAssets();
    void loadAuthoredVfxAssets();
    const ax::AttackDefinition* currentAuthoredAttack() const;

    void emitGameplayEvent(const std::string& type, const std::string& senderId, const std::string& targetId, const std::string& payload, float value, const Vec3& position, const Vec3& direction);
    void registerPlayerGameplayObject();
    void updateGameplayRuntimeObjects();
    void startActionPrototypeRuntime();

};
