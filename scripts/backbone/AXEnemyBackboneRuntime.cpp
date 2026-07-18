#include "Player.h"

#include <algorithm>
#include <cmath>

// AXEngine AIKit backbone for enemy spawning, animation and collision helpers.

#include "AXEngine/Gameplay/Events.h"

#include <cmath>
#include <algorithm>
#include "runtime/AXSceneRuntime.h"

// EN00_ACT.inl - simple enemy actor logic, intentionally script-side.
// Scene authoring rule:
//   Add Empty nodes in Scene*.glb named ENEMY_Basic, ENEMY_Sword, etc.
//   The archetype chooses the GLB/behavior table below.

std::string Player::enemyGlbForArchetype(const std::string& archetype) const {
    std::string a = lowerStr(archetype);

    auto it = authoredEnemies_.find(a);
    if (it != authoredEnemies_.end() && !it->second.model.empty()) {
        return AXSceneRuntime::runtimeAssetPath(it->second.model);
    }

    // Fallback aliases while the project still uses Man1_Cinema for test enemies.
    if (strContains(a, "gunner")) return "assets/Man1_Cinema.glb";
    if (strContains(a, "sword")) return "assets/Man1_Cinema.glb";
    if (strContains(a, "heavy")) return "assets/Man1_Cinema.glb";
    return "assets/Man1_Cinema.glb";
}

void Player::spawnEnemiesFromScene() {
    enemies_.clear();

    for (const DemoSceneMarker& marker : environment_.sceneMarkers()) {
        if (marker.kind != "ENEMY") {
            continue;
        }

        float y = marker.y;
        float ground = 0.0f;
        if (findGroundYAt(marker.x, marker.z, ground)) {
            y = ground;
        }

        spawnEnemy(marker.archetype, marker.x, y, marker.z, marker.yawDeg);
    }

    // Fallback graybox enemy so combat can be tested before authoring markers.
    if (enemies_.empty()) {
        Vec3 fwd = playerForward();
        float x = posX_ + fwd.x * 4.2f;
        float z = posZ_ + fwd.z * 4.2f;
        float y = posY_;
        float ground = 0.0f;
        if (findGroundYAt(x, z, ground)) {
            y = ground;
        }

        spawnEnemy("Basic", x, y, z, yawDeg_ + 180.0f);
        print("No ENEMY_* scene markers found; spawned one fallback test enemy.");
    }

    print("Enemies spawned: " + std::to_string(enemies_.size()));
}

void Player::spawnEnemy(const std::string& archetype, float x, float y, float z, float yawDeg) {
    GameObject* obj = createEntity("Enemy_" + archetype);
    if (!obj) return;

    EnemyActor enemy;
    enemy.object = obj;
    enemy.name = obj->name;
    enemy.archetype = archetype;
    enemy.glbPath = enemyGlbForArchetype(archetype);

    auto authoredIt = authoredEnemies_.find(lowerStr(archetype));
    if (authoredIt != authoredEnemies_.end()) {
        const ax::EnemyDefinition& def = authoredIt->second;
        enemy.maxHp = enemy.hp = def.hp;
        enemy.radius = def.radius;
        enemy.height = def.height;
        enemy.detectionRange = def.detectionRange;
        enemy.preferredRange = def.preferredRange;
        enemy.baseAttackCooldown = def.attackCooldown;
        enemy.moveSpeed = def.moveSpeed;
        if (!def.attacks.empty()) {
            const ax::EnemyAttackDefinition& atk = def.attacks.front();
            enemy.attackRange = atk.range;
            enemy.baseAttackCooldown = atk.cooldown;
            enemy.attackActiveStart = atk.windup;
            enemy.attackActiveEnd = atk.activeEnd;
            enemy.attackDamage = atk.damage;
        }
    }

    enemy.x = x;
    enemy.y = y;
    enemy.z = z;
    enemy.yawDeg = yawDeg;

    std::string a = lowerStr(archetype);
    if (authoredEnemies_.find(a) == authoredEnemies_.end() && strContains(a, "heavy")) {
        enemy.maxHp = enemy.hp = 260;
        enemy.radius = 0.62f;
        enemy.attackDuration = 1.05f;
    } else if (authoredEnemies_.find(a) == authoredEnemies_.end() && strContains(a, "fast")) {
        enemy.maxHp = enemy.hp = 110;
        enemy.radius = 0.42f;
        enemy.attackDuration = 0.68f;
    }

    obj->setPosition(x, y, z);
    obj->setRotation(0.0f, yawDeg, 0.0f);
    obj->setScale(1.0f, 1.0f, 1.0f);
    obj->setSkinnedMesh(enemy.glbPath);

    setEnemyAnim(enemy, "Idle", true, 0.0f, 1.0f);

    ax::RuntimeObjectRecord& rec = gameplayRuntime_.registerObject(
        "Enemy",
        enemy.name,
        ax::Vec3f{x, y, z},
        ax::Vec3f{0.0f, yawDeg, 0.0f},
        ax::Vec3f{1.0f, 1.0f, 1.0f},
        {"Enemy", archetype, "CombatTarget", "Hurtbox"}
    );
    enemy.runtimeId = rec.id;
    emitGameplayEvent(ax::events::EnemySpawned, enemy.runtimeId, {}, archetype, static_cast<float>(enemy.hp), Vec3{x, y, z}, Vec3{});

    enemies_.push_back(enemy);
}

bool Player::enemyHasClip(const EnemyActor& enemy, const std::string& clip) const {
    if (!enemy.object) return false;
    auto* src = enemy.object->skinnedRender();
    if (!src || !src->asset) return false;
    return src->asset->findClip(clip) >= 0;
}

void Player::setEnemyAnim(EnemyActor& enemy, const std::string& clip, bool loop, float fade, float speed) {
    if (!enemy.object) return;
    auto* src = enemy.object->skinnedRender();
    if (!src) return;

    std::string selected = clip;
    if (!enemyHasClip(enemy, selected)) {
        if (clip == "Attack" && enemyHasClip(enemy, "PunchAttack1")) selected = "PunchAttack1";
        else if (clip == "Hit" && enemyHasClip(enemy, "HitLight")) selected = "HitLight";
        else if (clip == "Dead" && enemyHasClip(enemy, "KnockDown")) selected = "KnockDown";
        else if (enemyHasClip(enemy, "Idle")) selected = "Idle";
        else return;
    }

    src->anim.speed = speed;
    if (fade <= 0.0f) {
        src->play(selected, loop);
    } else {
        src->crossFade(selected, fade, loop);
    }
}

float Player::enemyClipDurationOrFallback(const EnemyActor& enemy, const std::string& clip, float fallback) const {
    if (!enemy.object) return fallback;
    auto* src = enemy.object->skinnedRender();
    if (!src || !src->asset) return fallback;

    int index = src->asset->findClip(clip);
    if (index < 0) return fallback;

    float d = src->clipDuration(index);
    return d > 0.01f ? d : fallback;
}

float Player::enemyDistanceSqXZ(const EnemyActor& enemy, float x, float z) const {
    float dx = enemy.x - x;
    float dz = enemy.z - z;
    return dx * dx + dz * dz;
}

bool Player::enemyCanSeePlayer(const EnemyActor& enemy) const {
    // First-pass arena logic: distance-gated awareness. Later this can raycast
    // against environment_.meshColliders() without changing the AI state table.
    return enemyDistanceSqXZ(enemy, posX_, posZ_) < enemy.detectionRange * enemy.detectionRange;
}

void Player::faceEnemyToward(EnemyActor& enemy, float x, float z, float turnSpeedDeg, float dt) {
    float dx = x - enemy.x;
    float dz = z - enemy.z;

    if (std::fabs(dx) + std::fabs(dz) < 0.0001f) {
        return;
    }

    float desired = std::atan2(dx, dz) * 180.0f / 3.14159265f;
    float delta = desired - enemy.yawDeg;

    while (delta > 180.0f) delta -= 360.0f;
    while (delta < -180.0f) delta += 360.0f;

    float maxTurn = turnSpeedDeg * dt;
    delta = std::clamp(delta, -maxTurn, maxTurn);
    enemy.yawDeg += delta;
}

bool Player::moveEnemyXZWithCollision(EnemyActor& enemy, float dx, float dz) {
    float nextX = enemy.x + dx;
    float nextY = enemy.y;
    float nextZ = enemy.z + dz;

    // Uses the same scene triangle collision resolver as the player. This stays in the game template and avoids coupling engine physics to a specific enemy actor.
    resolvePlayerCollisions(nextX, nextY, nextZ);

    float actualX = nextX - enemy.x;
    float actualZ = nextZ - enemy.z;
    bool blocked = (actualX * dx + actualZ * dz) < (dx * dx + dz * dz) * 0.20f;

    enemy.x = nextX;
    enemy.z = nextZ;

    float ground = 0.0f;
    if (findGroundYAt(enemy.x, enemy.z, ground)) {
        enemy.y = ground;
    }

    return blocked;
}

void Player::startEnemyAttack(EnemyActor& enemy) {
    enemy.state = EnemyState::Attack;
    enemy.attackTimer = 0.0f;
    enemy.attackHitApplied = false;
    enemy.attackCooldown = enemy.baseAttackCooldown;

    std::string clip = enemyHasClip(enemy, "PunchAttack1") ? "PunchAttack1" : "Attack";
    enemy.attackDuration = enemyClipDurationOrFallback(enemy, clip, enemy.attackDuration);
    setEnemyAnim(enemy, clip, false, 0.04f, 1.05f);
}

