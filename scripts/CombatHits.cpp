#include "Player.h"
#include "AXEngine/AXCombatKit.h"

#include <algorithm>
#include <cmath>

// User-facing combat hit rules: ranges, damage routing and hit application.
// Asset loading for .axattack/.axenemy/.axvfx is AXEngine backbone.

Player::Vec3 Player::playerForward() const {
    float yawRad = yawDeg_ * 3.14159265f / 180.0f;
    return Vec3{std::sin(yawRad), 0.0f, std::cos(yawRad)};
}

Player::Vec3 Player::attackHitCenter(float range) const {
    Vec3 fwd = playerForward();
    return Vec3{posX_ + fwd.x * range, posY_ + 1.05f, posZ_ + fwd.z * range};
}


float Player::attackHitRange() const {
    if (combatState_ != CombatState::Attack) return 0.0f;

    if (activeComboType_ == ComboType::Sword) {
        return comboIndex_ == 2 ? 1.95f : 1.65f;
    }

    return comboIndex_ == 2 ? 1.35f : 1.10f;
}

float Player::attackHitRadius() const {
    if (combatState_ != CombatState::Attack) return 0.0f;

    if (activeComboType_ == ComboType::Sword) {
        return comboIndex_ == 2 ? 1.05f : 0.85f;
    }

    return comboIndex_ == 2 ? 0.95f : 0.65f;
}

bool Player::enemyInsidePlayerHit(const EnemyActor& enemy, const Vec3& center, float radius, float arcCos) const {
    if (!enemy.alive || enemy.state == EnemyState::Dead) return false;

    float enemyMidY = enemy.y + enemy.height * 0.50f;
    if (std::fabs(enemyMidY - center.y) > enemy.height * 0.75f + radius) {
        return false;
    }

    float dx = enemy.x - posX_;
    float dz = enemy.z - posZ_;
    float len = std::sqrt(dx * dx + dz * dz);

    if (len > 0.001f) {
        Vec3 fwd = playerForward();
        float dot = (dx / len) * fwd.x + (dz / len) * fwd.z;
        if (dot < arcCos) {
            return false;
        }
    }

    float cx = enemy.x - center.x;
    float cz = enemy.z - center.z;
    float allowed = radius + enemy.radius;
    return (cx * cx + cz * cz) <= allowed * allowed;
}

void Player::damageEnemy(EnemyActor& enemy, int damage, const Vec3& hitPoint, const Vec3& hitDir, float knockback) {
    if (!enemy.alive || enemy.state == EnemyState::Dead) return;

    Vec3 dir = vecNorm(hitDir);

    ax::combat::Combatant attacker;
    attacker.id = playerRuntimeId_;
    attacker.name = "Player";
    attacker.team = "Player";
    attacker.position = ax::Vec3{posX_, posY_, posZ_};
    attacker.forward = ax::Vec3{dir.x, dir.y, dir.z};

    ax::combat::Combatant target;
    target.id = enemy.runtimeId;
    target.name = enemy.name;
    target.team = "Enemy";
    target.position = ax::Vec3{enemy.x, enemy.y, enemy.z};
    target.health = static_cast<float>(enemy.hp);
    target.maxHealth = static_cast<float>(enemy.maxHp);
    target.poise = 50.0f;
    target.maxPoise = 50.0f;
    target.alive = enemy.alive;
    target.forward = ax::Vec3{std::sin(enemy.yawDeg * 3.14159265f / 180.0f), 0.0f, std::cos(enemy.yawDeg * 3.14159265f / 180.0f)};

    ax::combat::DamagePacket packet = activeComboType_ == ComboType::Hands
        ? ax::combat::CombatRuntime::makeUnarmedLight(currentClip_, static_cast<float>(damage))
        : ax::combat::CombatRuntime::makeWeaponLight(currentClip_, static_cast<float>(damage));
    packet.knockback = ax::Vec3{dir.x * knockback, 0.0f, dir.z * knockback};

    ax::combat::HitContext context;
    context.attackName = currentClip_;
    context.normalizedTime = attackNormalized_;
    context.hitPoint = ax::Vec3{hitPoint.x, hitPoint.y, hitPoint.z};
    context.hitDirection = ax::Vec3{dir.x, dir.y, dir.z};

    const ax::combat::HitResult resolved = ax::combat::CombatRuntime::resolveHit(attacker, target, packet, context);
    const int finalDamage = std::max(0, static_cast<int>(std::round(resolved.finalDamage)));
    if (!resolved.hit || finalDamage <= 0) {
        if (resolved.blocked) {
            print(enemy.name + " blocked " + currentClip_);
        }
        return;
    }

    enemy.hp -= finalDamage;

    emitGameplayEvent(ax::events::EnemyHit, playerRuntimeId_, enemy.runtimeId, enemy.name, static_cast<float>(finalDamage), hitPoint, dir);
    enemy.knockVelX = dir.x * knockback;
    enemy.knockVelZ = dir.z * knockback;

    if (enemy.hp <= 0 || resolved.killed) {
        enemy.hp = 0;
        enemy.alive = false;
        enemy.state = EnemyState::Dead;
        enemy.hitStunTimer = 0.0f;
        setEnemyAnim(enemy, "Dead", false, 0.04f, 1.0f);
        spawnHitVfx(hitPoint, dir, 1.45f, 1.0f, 0.20f, 0.10f);
        emitGameplayEvent(ax::events::EnemyDeath, enemy.runtimeId, {}, enemy.name, 0.0f, hitPoint, dir);
        print(enemy.name + " defeated.");
        return;
    }

    enemy.state = EnemyState::HitStun;
    enemy.hitStunTimer = std::max(0.12f, resolved.reaction.stunSeconds);
    enemy.attackTimer = 0.0f;
    enemy.attackHitApplied = false;
    setEnemyAnim(enemy, resolved.reaction.animation.empty() ? "Hit" : resolved.reaction.animation, false, 0.03f, 1.25f);
    spawnHitVfx(hitPoint, dir, 1.05f, 1.0f, 0.85f, 0.25f);

    print(enemy.name + " HP " + std::to_string(enemy.hp) + "/" + std::to_string(enemy.maxHp));
}

void Player::applyPlayerAttackHits() {
    if (combatState_ != CombatState::Attack || !isAttackHitActive()) {
        return;
    }

    const Vec3 fwd = playerForward();

    if (useAuthoredAttackVolumes_) {
        if (const ax::AttackDefinition* authored = currentAuthoredAttack()) {
            std::vector<ax::Hurtbox> hurtboxes;
            hurtboxes.reserve(enemies_.size());
            for (const EnemyActor& enemy : enemies_) {
                if (!enemy.alive || enemy.state == EnemyState::Dead || enemy.lastHitSerial == attackSerial_) {
                    continue;
                }
                ax::Hurtbox h;
                h.targetId = enemy.name;
                h.center = {enemy.x, enemy.y + enemy.height * 0.5f, enemy.z};
                h.halfExtents = {enemy.radius, enemy.height * 0.5f, enemy.radius};
                hurtboxes.push_back(h);
            }

            const ax::AttackVec3 attackerPos{posX_, posY_, posZ_};
            const std::vector<ax::AttackHitResult> hits = attackColliderSystem_.evaluate(
                *authored,
                attackTimer_,
                attackerPos,
                yawDeg_,
                hurtboxes
            );

            for (const ax::AttackHitResult& hit : hits) {
                for (EnemyActor& enemy : enemies_) {
                    if (enemy.name != hit.targetId || enemy.lastHitSerial == attackSerial_) {
                        continue;
                    }
                    enemy.lastHitSerial = attackSerial_;
                    Vec3 hitPoint{enemy.x, enemy.y + enemy.height * 0.5f, enemy.z};
                    float knockback = std::sqrt(hit.knockback.x * hit.knockback.x + hit.knockback.z * hit.knockback.z);
                    if (knockback < 0.001f) knockback = activeComboType_ == ComboType::Sword ? 7.5f : 5.0f;
                    emitGameplayEvent(ax::events::AttackHit, playerRuntimeId_, enemy.runtimeId, currentClip_, hit.damage, hitPoint, fwd);
                    damageEnemy(enemy, static_cast<int>(std::round(hit.damage)), hitPoint, fwd, knockback);
                }
            }
            return;
        }
    }

    const float range = attackHitRange();
    const float radius = attackHitRadius();
    const float arcCos = activeComboType_ == ComboType::Sword ? 0.18f : 0.35f;
    const Vec3 center = attackHitCenter(range);
    const AttackDef& def = currentAttackDef();

    for (EnemyActor& enemy : enemies_) {
        if (enemy.lastHitSerial == attackSerial_) {
            continue;
        }

        if (!enemyInsidePlayerHit(enemy, center, radius, arcCos)) {
            continue;
        }

        enemy.lastHitSerial = attackSerial_;
        emitGameplayEvent(ax::events::AttackHit, playerRuntimeId_, enemy.runtimeId, currentClip_, static_cast<float>(def.damage), center, fwd);
        damageEnemy(enemy, def.damage, center, fwd, activeComboType_ == ComboType::Sword ? 7.5f : 5.0f);
    }
}

bool Player::isSpecialHitActive() const {
    if (combatState_ == CombatState::DashAttack) {
        if (specialDuration_ <= 0.001f) return false;
        float t = std::clamp(specialTimer_ / specialDuration_, 0.0f, 1.0f);
        return t >= 0.18f && t <= 0.56f;
    }

    if (combatState_ == CombatState::AirAttack) {
        return specialClip_ == "AxeKick" || specialClip_ == "AxeKickIdle" || specialClip_ == "Helm Breaker";
    }

    if (combatState_ == CombatState::ChargeAttack) {
        if (specialDuration_ <= 0.001f) return false;
        float t = std::clamp(specialTimer_ / specialDuration_, 0.0f, 1.0f);
        return t >= 0.18f && t <= 0.72f;
    }

    if (combatState_ == CombatState::HomingSword) {
        return homingSwordPhase_ == 1 || homingSwordPhase_ == 2;
    }

    return false;
}

int Player::specialDamage() const {
    if (combatState_ == CombatState::DashAttack) return swordVisible_ ? 80 : 55;
    if (combatState_ == CombatState::AirAttack) return swordVisible_ ? 95 : 70;
    if (combatState_ == CombatState::ChargeAttack) return 85;
    if (combatState_ == CombatState::HomingSword) return 100;
    return 0;
}

float Player::specialHitRange() const {
    if (combatState_ == CombatState::DashAttack) return swordVisible_ ? 1.90f : 1.35f;
    if (combatState_ == CombatState::AirAttack) return 0.75f;
    if (combatState_ == CombatState::ChargeAttack) return 0.65f;
    if (combatState_ == CombatState::HomingSword) return 0.65f;
    return 0.0f;
}

float Player::specialHitRadius() const {
    if (combatState_ == CombatState::DashAttack) return swordVisible_ ? 1.00f : 0.85f;
    if (combatState_ == CombatState::AirAttack) return 1.05f;
    if (combatState_ == CombatState::ChargeAttack) return 1.45f; // SpinKick: generous 360 area.
    if (combatState_ == CombatState::HomingSword) return 1.15f;
    return 0.0f;
}

void Player::applyPlayerSpecialHits() {
    if (!isSpecialHitActive()) {
        return;
    }

    const float range = specialHitRange();
    const float radius = specialHitRadius();
    const float arcCos = combatState_ == CombatState::ChargeAttack ? -1.0f : 0.10f;
    const Vec3 fwd = playerForward();
    const Vec3 center = attackHitCenter(range);

    for (EnemyActor& enemy : enemies_) {
        if (enemy.lastHitSerial == attackSerial_) {
            continue;
        }

        if (!enemyInsidePlayerHit(enemy, center, radius, arcCos)) {
            continue;
        }

        enemy.lastHitSerial = attackSerial_;
        damageEnemy(enemy, specialDamage(), center, fwd, 10.0f);
    }
}
