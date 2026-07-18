#include "Player.h"
#include "AXEngine/Gameplay/Events.h"
#include "AXEngine/AXVFXKit.h"
#include "AXEngine/AXSoundKit.h"

#include <cmath>
#include <algorithm>

// User-facing VFX decisions stay here.
// AXVFXKit owns the generic runtime: .axvfx parsing, lifetime, mesh/sprite
// render commands, debug fallback and gameplay-event style spawning.

void Player::spawnHitVfx(const Vec3& point, const Vec3& dir, float radius, float r, float g, float b) {
    // Existing lightweight debug spark for immediate readability.
    CombatVfx fx;
    fx.origin = point;
    fx.dir = vecNorm(dir);
    fx.radius = radius;
    fx.r = r;
    fx.g = g;
    fx.b = b;
    fx.duration = authoredHitVfxLifetime_;
    combatVfx_.push_back(fx);

    // New engine-level VFX path. This can load a Blender-authored mesh VFX from
    // assets/vfx/HitSpark_Mesh.axvfx and exposes render commands to the renderer.
    ax::vfx::SpawnRequest req;
    req.asset = "assets/vfx/HitSpark_Mesh.axvfx";
    req.name = "HitSpark";
    req.position = ax::Vec3{point.x, point.y, point.z};
    req.direction = ax::Vec3{dir.x, dir.y, dir.z};
    req.scale = ax::Vec3{radius, radius, radius};
    req.tint = ax::Color{r, g, b, 1.0f};
    req.ownerId = playerRuntimeId_;
    ax::vfx::runtime().spawn(req);

    // Small AXSound runtime hook. The .axsound file controls clip path,
    // looping/spatial/category/variation; gameplay code decides when to play it.
    ax::sound::PlayRequest soundReq;
    soundReq.sound = "assets/audio/SwordHit.axsound";
    soundReq.position = ax::Vec3{point.x, point.y, point.z};
    soundReq.volumeScale = std::clamp(radius, 0.45f, 1.35f);
    soundReq.ownerId = playerRuntimeId_;
    soundReq.eventName = ax::sound::Events::SoundPlayed;
    ax::sound::runtime().play(soundReq);

    emitGameplayEvent(ax::events::VFXSpawned, playerRuntimeId_, {}, "HitSpark", radius, point, dir);
}

void Player::updateCombatVfx(float dt) {
    ax::vfx::runtime().update(dt);
    ax::sound::runtime().update(dt);

    for (CombatVfx& fx : combatVfx_) {
        fx.age += dt;
    }

    combatVfx_.erase(
        std::remove_if(
            combatVfx_.begin(),
            combatVfx_.end(),
            [](const CombatVfx& fx) { return fx.age >= fx.duration; }
        ),
        combatVfx_.end()
    );
}

void Player::appendCombatDebugLines() {
    if (!renderer()) return;

    // Engine VFX debug fallback / authoring visualization. Once the renderer has
    // a mesh-VFX draw path, it can consume ax::vfx::runtime().renderCommands().
    for (const ax::vfx::VFXDebugLine& line : ax::vfx::runtime().debugLines()) {
        renderer()->addDebugLine(
            line.a.x, line.a.y, line.a.z,
            line.b.x, line.b.y, line.b.z,
            line.color.r, line.color.g, line.color.b
        );
    }

    for (const CombatVfx& fx : combatVfx_) {
        float live = 1.0f - std::clamp(fx.age / fx.duration, 0.0f, 1.0f);
        float s = fx.radius * live;

        Vec3 right = vecNorm(vecCross(Vec3{0.0f, 1.0f, 0.0f}, fx.dir));
        if (vecLen(right) < 0.001f) {
            right = Vec3{1.0f, 0.0f, 0.0f};
        }
        Vec3 up = Vec3{0.0f, 1.0f, 0.0f};

        Vec3 a = vecSub(fx.origin, vecMul(right, s));
        Vec3 b = vecAdd(fx.origin, vecMul(right, s));
        Vec3 c = vecSub(fx.origin, vecMul(up, s * 0.65f));
        Vec3 d = vecAdd(fx.origin, vecMul(up, s * 0.65f));
        Vec3 e = vecSub(fx.origin, vecMul(fx.dir, s * 0.70f));
        Vec3 f = vecAdd(fx.origin, vecMul(fx.dir, s * 0.70f));

        renderer()->addDebugLine(a.x, a.y, a.z, b.x, b.y, b.z, fx.r, fx.g, fx.b);
        renderer()->addDebugLine(c.x, c.y, c.z, d.x, d.y, d.z, fx.r, fx.g, fx.b);
        renderer()->addDebugLine(e.x, e.y, e.z, f.x, f.y, f.z, fx.r, fx.g, fx.b);
    }

    if (!showAttackDebug_) {
        return;
    }

    bool drewAuthoredAttackVolume = false;
    if (combatState_ == CombatState::Attack && isAttackHitActive()) {
        if (const ax::AttackDefinition* authored = currentAuthoredAttack()) {
            const auto active = attackColliderSystem_.activeColliders(*authored, attackTimer_);
            for (const ax::AttackColliderVolume* vol : active) {
                const ax::AttackVec3 cax = ax::authoredOffsetToWorld({posX_, posY_, posZ_}, yawDeg_, vol->offset);
                Vec3 c{cax.x, cax.y, cax.z};
                float rx = vol->shape == ax::AttackColliderShape::Box ? vol->size.x * 0.5f : vol->radius;
                float ry = vol->shape == ax::AttackColliderShape::Box ? vol->size.y * 0.5f : vol->height * 0.5f;
                float rz = vol->shape == ax::AttackColliderShape::Box ? vol->size.z * 0.5f : vol->radius;
                renderer()->addDebugLine(c.x - rx, c.y, c.z, c.x + rx, c.y, c.z, 1.0f, 0.30f, 0.05f);
                renderer()->addDebugLine(c.x, c.y - ry, c.z, c.x, c.y + ry,  c.z, 1.0f, 0.30f, 0.05f);
                renderer()->addDebugLine(c.x, c.y, c.z - rz, c.x, c.y, c.z + rz, 1.0f, 0.30f, 0.05f);
                renderer()->addDebugLine(posX_, posY_ + 1.0f, posZ_, c.x, c.y, c.z, 1.0f, 0.30f, 0.05f);
                drewAuthoredAttackVolume = true;
            }
        }
    }

    if (!drewAuthoredAttackVolume && combatState_ == CombatState::Attack && isAttackHitActive()) {
        Vec3 c = attackHitCenter(attackHitRange());
        float r = attackHitRadius();
        renderer()->addDebugLine(c.x - r, c.y, c.z, c.x + r, c.y, c.z, 1.0f, 0.75f, 0.15f);
        renderer()->addDebugLine(c.x, c.y, c.z - r, c.x, c.y, c.z + r, 1.0f, 0.75f, 0.15f);
        renderer()->addDebugLine(posX_, posY_ + 1.0f, posZ_, c.x, c.y, c.z, 1.0f, 0.75f, 0.15f);
    }

    if (isSpecialHitActive()) {
        Vec3 c = attackHitCenter(specialHitRange());
        float r = specialHitRadius();
        renderer()->addDebugLine(c.x - r, c.y, c.z, c.x + r, c.y, c.z, 0.45f, 0.90f, 1.0f);
        renderer()->addDebugLine(c.x, c.y, c.z - r, c.x, c.y, c.z + r, 0.45f, 0.90f, 1.0f);
        renderer()->addDebugLine(posX_, posY_ + 1.0f, posZ_, c.x, c.y, c.z, 0.45f, 0.90f, 1.0f);
    }

    for (const EnemyActor& enemy : enemies_) {
        if (!enemy.alive && enemy.state != EnemyState::Dead) continue;
        float r = enemy.radius;
        float y = enemy.y + 1.0f;
        renderer()->addDebugLine(enemy.x - r, y, enemy.z, enemy.x + r, y, enemy.z, 1.0f, 0.1f, 0.1f);
        renderer()->addDebugLine(enemy.x, y, enemy.z - r, enemy.x, y, enemy.z + r, 1.0f, 0.1f, 0.1f);
    }
}
