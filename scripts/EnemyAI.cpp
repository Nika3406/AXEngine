#include "Player.h"

#include <algorithm>
#include <cmath>

// User-facing enemy decision loop. AXAIKit backbone owns spawning, animation helpers,
// collision movement helpers and line-of-sight/range utility functions.

void Player::updateEnemy(EnemyActor& enemy, float dt) {
    if (!enemy.object || !enemy.alive) {
        return;
    }

    auto* src = enemy.object->skinnedRender();
    if (src) {
        AnimationSystem::update(*src, dt);
    }

    if (enemy.attackCooldown > 0.0f) {
        enemy.attackCooldown = std::max(0.0f, enemy.attackCooldown - dt);
    }

    if (enemy.state == EnemyState::Dead) {
        enemy.object->setPosition(enemy.x, enemy.y, enemy.z);
        enemy.object->setRotation(0.0f, enemy.yawDeg, 0.0f);
        return;
    }

    if (enemy.state == EnemyState::HitStun) {
        enemy.hitStunTimer -= dt;
        moveEnemyXZWithCollision(enemy, enemy.knockVelX * dt, enemy.knockVelZ * dt);
        enemy.knockVelX *= std::max(0.0f, 1.0f - 8.0f * dt);
        enemy.knockVelZ *= std::max(0.0f, 1.0f - 8.0f * dt);

        if (enemy.hitStunTimer <= 0.0f) {
            enemy.state = EnemyState::Idle;
            setEnemyAnim(enemy, "Idle", true, 0.08f, 1.0f);
        }

        enemy.object->setPosition(enemy.x, enemy.y, enemy.z);
        enemy.object->setRotation(0.0f, enemy.yawDeg, 0.0f);
        return;
    }

    float distSq = enemyDistanceSqXZ(enemy, posX_, posZ_);
    float attackRange = enemy.attackRange + enemy.radius;

    if (enemy.state == EnemyState::Attack) {
        enemy.attackTimer += dt;
        faceEnemyToward(enemy, posX_, posZ_, 540.0f, dt);

        float t = enemy.attackDuration > 0.001f
            ? std::clamp(enemy.attackTimer / enemy.attackDuration, 0.0f, 1.0f)
            : 1.0f;

        if (!enemy.attackHitApplied && t >= enemy.attackActiveStart && t <= enemy.attackActiveEnd) {
            enemy.attackHitApplied = true;
            if (distSq <= attackRange * attackRange) {
                print("Enemy hit player: " + enemy.archetype + " damage=" + std::to_string(enemy.attackDamage));
            }
        }

        if (enemy.attackTimer >= enemy.attackDuration) {
            enemy.state = EnemyState::Idle;
            setEnemyAnim(enemy, "Idle", true, 0.08f, 1.0f);
        }

        enemy.object->setPosition(enemy.x, enemy.y, enemy.z);
        enemy.object->setRotation(0.0f, enemy.yawDeg, 0.0f);
        return;
    }

    if (!enemyCanSeePlayer(enemy)) {
        enemy.state = EnemyState::Idle;
        setEnemyAnim(enemy, "Idle", true, 0.10f, 1.0f);
    } else if (distSq <= attackRange * attackRange && enemy.attackCooldown <= 0.0f) {
        startEnemyAttack(enemy);
    } else {
        enemy.state = EnemyState::Chase;

        float dx = posX_ - enemy.x;
        float dz = posZ_ - enemy.z;
        float dist = std::sqrt(std::max(distSq, 0.0001f));
        float nx = dx / dist;
        float nz = dz / dist;

        faceEnemyToward(enemy, posX_, posZ_, 420.0f, dt);

        if (dist > std::max(enemy.preferredRange, attackRange * 0.88f)) {
            float speed = enemy.moveSpeed;
            moveEnemyXZWithCollision(enemy, nx * speed * dt, nz * speed * dt);
            setEnemyAnim(enemy, enemyHasClip(enemy, "run") ? "run" : "Walk", true, 0.10f, 1.25f);
        } else {
            setEnemyAnim(enemy, "Idle", true, 0.08f, 1.0f);
        }
    }

    enemy.object->setPosition(enemy.x, enemy.y, enemy.z);
    enemy.object->setRotation(0.0f, enemy.yawDeg, 0.0f);
}

void Player::updateEnemies(float dt) {
    for (EnemyActor& enemy : enemies_) {
        updateEnemy(enemy, dt);
    }
}
