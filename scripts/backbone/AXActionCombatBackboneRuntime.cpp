#include "Player.h"

// AXEngine ActionKit/CombatKit backbone for combo playback, special moves,
// root motion and homing behavior. The template controller owns these mechanics; reusable primitives remain in AXEngine modules.

#include "AXEngine/Gameplay/Events.h"

// PL00_ACT.inl - attack, special move, root-motion, and combat action logic
int Player::findRootMotionJoint(SkinnedRenderComponent* src) {
    if (!src || !src->asset) return -1;
    if (rootMotionJointIndex_ >= 0 &&
        rootMotionJointIndex_ < (int)src->asset->skeleton.joints.size()) {
        return rootMotionJointIndex_;
    }

    const auto& joints = src->asset->skeleton.joints;

    auto nameHas = [](const std::string& name, const std::string& token) {
        return lowerStr(name).find(lowerStr(token)) != std::string::npos;
    };

    const char* preferred[] = {
        "root", "hips", "pelvis", "armature", "mixamorig:hips"
    };

    for (const char* token : preferred) {
        for (int i = 0; i < (int)joints.size(); ++i) {
            if (nameHas(joints[i].name, token)) {
                rootMotionJointIndex_ = i;
                print("Root motion joint: " + std::to_string(i) + " \"" + joints[i].name + "\"");
                return rootMotionJointIndex_;
            }
        }
    }

    // Fallback: first skeleton root.
    for (int i = 0; i < (int)joints.size(); ++i) {
        if (joints[i].parentIndex < 0) {
            rootMotionJointIndex_ = i;
            print("Root motion fallback joint: " + std::to_string(i) + " \"" + joints[i].name + "\"");
            return rootMotionJointIndex_;
        }
    }

    print("WARNING: no root motion joint found.");
    return -1;
}

void Player::enableRootMotionForAttack(SkinnedRenderComponent* src) {
    if (!src) return;

    int idx = findRootMotionJoint(src);
    src->anim.rootMotionJointIndex = idx;
    src->anim.rootMotionEnabled = idx >= 0;
    src->anim.rootMotionRemoveXZFromPose = true;
    src->anim.resetRootMotionTracking();
}

void Player::disableRootMotion(SkinnedRenderComponent* src) {
    if (!src) return;
    src->anim.rootMotionEnabled = false;
    src->anim.resetRootMotionTracking();
}

void Player::endCurrentAction(SkinnedRenderComponent* src) {
    if (combatState_ == CombatState::Attack) {
        endCombat(src);
        return;
    }

    if (combatState_ == CombatState::Dash ||
        combatState_ == CombatState::DashAttack ||
        combatState_ == CombatState::AirAttack ||
        combatState_ == CombatState::ChargeAttack ||
        combatState_ == CombatState::HomingSword) {
        endSpecialMove(src);
        return;
    }

    combatState_ = CombatState::None;
}

void Player::startSpinKick(SkinnedRenderComponent* src) {
    if (!src) return;

    combatState_ = CombatState::ChargeAttack;
    ++attackSerial_;
    specialClip_ = "SpinKick";
    specialTimer_ = 0.0f;
    specialDuration_ =
        clipDurationOrFallback(src, "SpinKick", specialFallbackDuration("SpinKick")) /
        specialAnimSpeed("SpinKick");

    currentClip_ = "SpinKick";

    currentSpeed_ = 0.0f;
    targetSpeed_ = 0.0f;
    hasMoveInput_ = false;
    walkHeld_ = false;
    sprintHeld_ = false;

    specialUsesScriptedMotion_ = false;
    specialMoveSpeed_ = 0.0f;
    specialDownSpeed_ = 0.0f;
    specialMotionTimer_ = 0.0f;
    specialMotionDuration_ = 0.0f;

    src->anim.speed = specialAnimSpeed("SpinKick");
    src->crossFade("SpinKick", 0.04f, false);

    enableRootMotionForAttack(src);

    print("Charge release: SpinKick");
}

void Player::startHomingSword(SkinnedRenderComponent* src) {
    if (!src) return;

    combatState_ = CombatState::HomingSword;
    ++attackSerial_;
    homingSwordPhase_ = 0;
    homingTimer_ = 0.0f;

    specialClip_ = "HomingSword1";
    specialTimer_ = 0.0f;
    specialDuration_ =
        clipDurationOrFallback(src, "HomingSword1", specialFallbackDuration("HomingSword1")) /
        specialAnimSpeed("HomingSword1");

    currentClip_ = "HomingSword1";

    currentSpeed_ = 0.0f;
    targetSpeed_ = 0.0f;
    hasMoveInput_ = false;
    walkHeld_ = false;
    sprintHeld_ = false;

    specialUsesScriptedMotion_ = false;
    specialMoveSpeed_ = 0.0f;
    specialDownSpeed_ = 0.0f;
    specialMotionTimer_ = 0.0f;
    specialMotionDuration_ = 0.0f;

    src->anim.speed = specialAnimSpeed("HomingSword1");
    src->crossFade("HomingSword1", 0.04f, false);

    enableRootMotionForAttack(src);

    print("Charge release: HomingSword1");
}

void Player::startAttack(SkinnedRenderComponent* src, ComboType type, int index) {
    if (!src) return;

    activeComboType_ = type;
    comboIndex_ = std::clamp(index, 0, 2);
    attackQueued_ = false;
    comboConsumed_ = false;
    combatState_ = CombatState::Attack;
    ++attackSerial_;
    attackColliderSystem_.resetHitMemory();
    attackTimer_ = 0.0f;
    attackNormalized_ = 0.0f;

    const AttackDef& def = currentAttackDef();
    float animSpeed = def.animSpeed;

    attackDuration_ = comboDuration(src, activeComboType_, comboIndex_) / animSpeed;
    currentClip_ = comboClip(activeComboType_, comboIndex_);

    currentSpeed_ = 0.0f;
    targetSpeed_ = 0.0f;
    hasMoveInput_ = false;
    walkHeld_ = false;
    sprintHeld_ = false;

    specialClip_.clear();
    specialTimer_ = 0.0f;
    specialDuration_ = 0.0f;
    specialUsesScriptedMotion_ = false;
    specialMoveSpeed_ = 0.0f;
    specialDownSpeed_ = 0.0f;
    
    specialUsesScriptedMotion_ = false;
    specialMoveSpeed_ = 0.0f;
    specialDownSpeed_ = 0.0f;
    specialMotionTimer_ = 0.0f;
    specialMotionDuration_ = 0.0f;

    src->anim.speed = animSpeed;
    src->crossFade(currentClip_, 0.03f, false);

    emitGameplayEvent(ax::events::AttackStarted, playerRuntimeId_, {}, currentClip_, static_cast<float>(attackSerial_), Vec3{posX_, posY_, posZ_}, playerForward());

    if (def.useRootMotion) {
        enableRootMotionForAttack(src);
    } else {
        disableRootMotion(src);
    }
}

void Player::startSpecialMove(
    SkinnedRenderComponent* src,
    CombatState state,
    const std::string& clip,
    bool scriptedMotion,
    float moveSpeedOverride,
    float downSpeedOverride
) {
    if (!src) return;

    combatState_ = state;
    ++attackSerial_;
    attackColliderSystem_.resetHitMemory();
    specialClip_ = clip;
    specialTimer_ = 0.0f;
    airAttackLandingPlayed_ = false;

    const float animSpeed = specialAnimSpeed(clip);
    specialDuration_ =
        clipDurationOrFallback(src, clip, specialFallbackDuration(clip)) / animSpeed;

    currentClip_ = clip;

    currentSpeed_ = 0.0f;
    targetSpeed_ = 0.0f;
    hasMoveInput_ = false;
    walkHeld_ = false;
    sprintHeld_ = false;

    src->anim.speed = animSpeed;
    src->crossFade(clip, 0.04f, false);

    // Special moves are scripted-motion moves, not root-motion moves.
    disableRootMotion(src);

    configureSpecialScriptedMotion(
        clip,
        scriptedMotion,
        moveSpeedOverride
    );

    if (downSpeedOverride >= 0.0f) {
        specialDownSpeed_ = downSpeedOverride;
    }
    emitGameplayEvent(ax::events::AttackStarted, playerRuntimeId_, {}, clip, static_cast<float>(attackSerial_), Vec3{posX_, posY_, posZ_}, playerForward());

    print("Special move: " + clip +
          " moveSpeed=" + std::to_string(specialMoveSpeed_) +
          " downSpeed=" + std::to_string(specialDownSpeed_));
}

void Player::endCombat(SkinnedRenderComponent* src) {
    combatState_ = CombatState::None;
    comboIndex_ = 0;
    attackQueued_ = false;
    comboConsumed_ = false;
    attackTimer_ = 0.0f;
    attackDuration_ = 0.0f;
    attackNormalized_ = 0.0f;
    disableRootMotion(src);
}

void Player::endSpecialMove(SkinnedRenderComponent* src) {
    combatState_ = CombatState::None;
    specialClip_.clear();
    specialTimer_ = 0.0f;
    specialDuration_ = 0.0f;
    specialMoveSpeed_ = 0.0f;
    specialDownSpeed_ = 0.0f;
    specialUsesScriptedMotion_ = false;
    specialMotionTimer_ = 0.0f;
    specialMotionDuration_ = 0.0f;
    airAttackLandingPlayed_ = false;
    homingSwordPhase_ = 0;
    homingTimer_ = 0.0f;
    homingMissileTimer_ = 0.0f;
    lmbHeld_ = false;
    lmbHoldTimer_ = 0.0f;
    disableRootMotion(src);
}

void Player::updateCombat(SkinnedRenderComponent* src, float dt) {
    if (!src) return;

    if (combatState_ != CombatState::Attack) {
        return;
    }

    attackTimer_ += dt;

    attackNormalized_ = attackDuration_ > 0.001f
        ? std::clamp(attackTimer_ / attackDuration_, 0.0f, 1.0f)
        : 1.0f;

    const AttackDef& def = currentAttackDef();

    applyExtractedRootMotion(src);

    // Buffered combo logic:
    // The click only stores intent. The AttackDef's cancelAt decides when
    // the current animation may cut. This makes every combo transition
    // consistent, even when the player clicks early or late.
    if (attackQueued_ &&
        !comboConsumed_ &&
        comboIndex_ < 2 &&
        attackNormalized_ >= def.cancelAt) {
        comboConsumed_ = true;
        startAttack(src, activeComboType_, comboIndex_ + 1);
        return;
    }

    // Let the designer choose recovery length separately from raw GLB length.
    // recoveryEnd keeps short clips responsive but prevents instant movement
    // snapping if an animation has awkward trailing frames.
    if (attackNormalized_ >= def.recoveryEnd || attackTimer_ >= attackDuration_) {
        endCombat(src);
    }
}

float Player::specialMotionDurationForClip(const std::string& clip) const {
    // Keep ground burst moves on the same movement model.
    // Dash1, Impale, and Cross should feel like the same controlled lunge
    // instead of three unrelated physics tricks.
    if (clip == "Dash1") return 0.32f;
    if (clip == "Impale") return 0.32f;
    if (clip == "Cross") return 0.32f;

    if (clip == "AxeKick") return 0.35f;
    if (clip == "Helm Breaker") return 0.38f;
    return 0.0f;
}

void Player::configureSpecialScriptedMotion(const std::string& clip, bool scripted, float overrideSpeed) {
    specialUsesScriptedMotion_ = scripted;
    specialMoveSpeed_ = 0.0f;
    specialDownSpeed_ = 0.0f;
    specialMotionTimer_ = 0.0f;
    specialMotionDuration_ = specialMotionDurationForClip(clip);

    if (!scripted) {
        return;
    }

    if (clip == "Dash1" || clip == "Impale" || clip == "Cross") {
        // Same speed for all forward burst moves. The animation can differ,
        // but the controller motion should not randomly change per clip.
        specialMoveSpeed_ = overrideSpeed > 0.0f
            ? overrideSpeed
            : defaultSpecialMoveSpeed(clip);
        return;
    }

    if (clip == "AxeKick") {
        specialMoveSpeed_ = 0.0f;
        specialDownSpeed_ = 20.0f;
        return;
    }

    if (clip == "Helm Breaker") {
        specialMoveSpeed_ = 0.0f;
        specialDownSpeed_ = 20.0f;
        return;
    }

    if (clip == "AxeKickIdle" || clip == "AxekickLand") {
        specialUsesScriptedMotion_ = true;
        specialMoveSpeed_ = defaultSpecialMoveSpeed("AxeKickIdle");
        specialDownSpeed_ = defaultSpecialDownSpeed("AxeKickIdle");
        specialMotionTimer_ = 0.0f;
        specialMotionDuration_ = 9999.0f;
        return;
    }
}


void Player::updateAirAttack(SkinnedRenderComponent* src) {
    if (!src) return;
    if (combatState_ != CombatState::AirAttack) return;

    if (swordVisible_) {
        // Helm Breaker is a single scripted strike-down move.
        if (grounded_) {
            endSpecialMove(src);
        }
        return;
    }

    // Sword OFF: AxeKick -> AxeKickIdle while falling -> AxekickLand.
    if (!grounded_ && specialClip_ == "AxeKick" && specialTimer_ >= specialDuration_) {
        specialClip_ = "AxeKickIdle";
        currentClip_ = specialClip_;
        specialTimer_ = 0.0f;
        specialDuration_ = 9999.0f;

        const float speed = specialAnimSpeed("AxeKickIdle");
        src->anim.speed = speed;
        src->crossFade("AxeKickIdle", 0.05f, true);

        specialUsesScriptedMotion_ = true;
        specialMoveSpeed_ = defaultSpecialMoveSpeed("AxeKickIdle");
        specialDownSpeed_ = defaultSpecialDownSpeed("AxeKickIdle");
        specialMotionTimer_ = 0.0f;
        specialMotionDuration_ = 9999.0f;
        return;
    }

    if (grounded_ && !airAttackLandingPlayed_) {
        airAttackLandingPlayed_ = true;
        specialClip_ = "AxekickLand";
        currentClip_ = specialClip_;
        specialTimer_ = 0.0f;

        const float speed = specialAnimSpeed("AxekickLand");
        src->anim.speed = speed;
        specialDuration_ =
            clipDurationOrFallback(src, "AxekickLand", specialFallbackDuration("AxekickLand")) / speed;

        src->crossFade("AxekickLand", 0.03f, false);

        specialUsesScriptedMotion_ = false;
        specialMoveSpeed_ = 0.0f;
        specialDownSpeed_ = 0.0f;
        specialMotionTimer_ = 0.0f;
        specialMotionDuration_ = 0.0f;
        return;
    }

    if (airAttackLandingPlayed_ && specialTimer_ >= specialDuration_) {
        endSpecialMove(src);
    }
}

void Player::applySpecialScriptedMotion(float dt) {
    if (!specialUsesScriptedMotion_) {
        return;
    }

    if (combatState_ != CombatState::Dash &&
        combatState_ != CombatState::DashAttack &&
        combatState_ != CombatState::AirAttack) {
        return;
    }

    if (specialMotionDuration_ <= 0.001f) {
        return;
    }

    if (specialMotionTimer_ >= specialMotionDuration_) {
        return;
    }

    specialMotionTimer_ += dt;

    float t = std::clamp(
        specialMotionTimer_ / specialMotionDuration_,
        0.0f,
        1.0f
    );

    // Startup -> active -> recovery motion curve.
    // This avoids the old "full speed on frame 1, then instantly dies" lurch.
    float burst = 0.0f;
    if (t < 0.15f) {
        burst = t / 0.15f;
    } else if (t < 0.55f) {
        burst = 1.0f;
    } else {
        float r = (t - 0.55f) / 0.45f;
        burst = 1.0f - r * r;
    }
    burst = std::clamp(burst, 0.0f, 1.0f);

    float yawRad = yawDeg_ * 3.14159265f / 180.0f;

    float forwardX = std::sin(yawRad);
    float forwardZ = std::cos(yawRad);

    float moveAmount = specialMoveSpeed_ * burst * dt;

    // Use swept/sub-stepped movement for high-speed attacks.
    // The old version only resolved collision at the final position, so a
    // fast Impale/Cross could start before a wall and end beyond it without
    // ever overlapping the wall. That is classic tunneling.
    bool blocked = movePlayerXZWithCollision(
        forwardX * moveAmount,
        forwardZ * moveAmount
    );

    if (blocked &&
        (combatState_ == CombatState::Dash ||
         combatState_ == CombatState::DashAttack)) {
        // Stop only the scripted translation. Let the attack animation finish.
        specialMotionTimer_ = specialMotionDuration_;
    }

    if (specialDownSpeed_ > 0.0f && !grounded_) {
        verticalVelocity_ = -specialDownSpeed_;
    }
}

void Player::updateSpecialMove(SkinnedRenderComponent* src, float dt) {
    if (!src) return;

    if (combatState_ != CombatState::Dash &&
        combatState_ != CombatState::DashAttack &&
        combatState_ != CombatState::AirAttack &&
        combatState_ != CombatState::ChargeAttack &&
        combatState_ != CombatState::HomingSword) {
        return;
    }

    specialTimer_ += dt;
    applySpecialScriptedMotion(dt);

    if (combatState_ == CombatState::ChargeAttack) {
        applyExtractedRootMotion(src);

        if (specialTimer_ >= specialDuration_) {
            endSpecialMove(src);
        }
        return;
    }

    if (combatState_ == CombatState::HomingSword) {
        updateHomingSword(src, dt);
        return;
    }

    if (combatState_ == CombatState::AirAttack) {
        updateAirAttack(src);
        return;
    }

    if (specialTimer_ >= specialDuration_) {
        endSpecialMove(src);
    }
}

void Player::startHomingSwordLand(SkinnedRenderComponent* src) {
    if (!src) return;

    homingSwordPhase_ = 2;
    ++attackSerial_;
    specialTimer_ = 0.0f;
    homingTimer_ = 0.0f;
    homingMissileTimer_ = 0.0f;

    specialClip_ = "HomingSwordLand";
    currentClip_ = specialClip_;
    specialDuration_ =
        clipDurationOrFallback(src, "HomingSwordLand", specialFallbackDuration("HomingSwordLand")) /
        specialAnimSpeed("HomingSwordLand");

    src->anim.speed = specialAnimSpeed("HomingSwordLand");
    src->crossFade("HomingSwordLand", 0.04f, false);

    enableRootMotionForAttack(src);

    print("HomingSwordLand");
}

void Player::updateHomingSword(SkinnedRenderComponent* src, float dt) {
    if (!src) return;
    if (combatState_ != CombatState::HomingSword) return;

    if (homingSwordPhase_ == 0) {
        applyExtractedRootMotion(src);

        if (specialTimer_ >= specialDuration_) {
            homingSwordPhase_ = 1;
            homingTimer_ = 0.0f;
            homingMissileTimer_ = 0.0f;
            specialTimer_ = 0.0f;

            specialClip_ = "HomingSwordIdle";
            currentClip_ = specialClip_;
            specialDuration_ =
                clipDurationOrFallback(src, "HomingSwordIdle", specialFallbackDuration("HomingSwordIdle")) /
                specialAnimSpeed("HomingSwordIdle");

            src->anim.speed = specialAnimSpeed("HomingSwordIdle");
            src->crossFade("HomingSwordIdle", 0.05f, true);

            disableRootMotion(src);

            print("HomingSwordIdle");
        }

        return;
    }

    if (homingSwordPhase_ == 1) {
        bool hit = applyHomingSwordIdleMotion(dt);

        if (hit) {
            startHomingSwordLand(src);
        }

        return;
    }

    if (homingSwordPhase_ == 2) {
        applyExtractedRootMotion(src);

        if (specialTimer_ >= specialDuration_) {
            endSpecialMove(src);
        }
    }
}

bool Player::applyHomingSwordIdleMotion(float dt) {
    homingMissileTimer_ += dt;

    float yawRad = camYawDeg_ * 3.14159265f / 180.0f;
    float pitchRad = camPitchDeg_ * 3.14159265f / 180.0f;

    Vec3 aimDir{
        -std::sin(yawRad) * std::cos(pitchRad),
        -std::sin(pitchRad),
        -std::cos(yawRad) * std::cos(pitchRad)
    };

    aimDir = vecNorm(aimDir);

    float nextX = posX_ + aimDir.x * homingMissileSpeed_ * dt;
    float nextY = posY_ + aimDir.y * homingMissileSpeed_ * dt;
    float nextZ = posZ_ + aimDir.z * homingMissileSpeed_ * dt;

    // Free-flight missile state.
    // No gravity. No ground snap. No normal grounded movement.
    verticalVelocity_ = 0.0f;
    grounded_ = false;
    edgeFallTimer_ = 0.0f;

    float horizontalLen = std::sqrt(aimDir.x * aimDir.x + aimDir.z * aimDir.z);
    if (horizontalLen > 0.001f) {
        yawDeg_ = std::atan2(aimDir.x, aimDir.z) * 180.0f / 3.14159265f;
    }

    posX_ = nextX;
    posY_ = nextY;
    posZ_ = nextZ;

    // Give it a small grace period so it does not instantly hit the floor
    // or the collider it started next to.
    if (homingMissileTimer_ > 0.08f && homingSwordHitCollider()) {
        return true;
    }

    if (homingMissileTimer_ >= homingMissileMaxTime_) {
        return true;
    }

    return false;
}

void Player::applyExtractedRootMotion(SkinnedRenderComponent* src) {
    if (!src) return;

    if (combatState_ != CombatState::Attack &&
        combatState_ != CombatState::ChargeAttack &&
        combatState_ != CombatState::HomingSword) {
        return;
    }

    if (combatState_ == CombatState::HomingSword &&
        homingSwordPhase_ == 1) {
        return;
    }

    if (!src->anim.rootMotionEnabled) return;

    float dx = src->anim.rootMotionDelta[0] * rootMotionScale_;
    float dz = src->anim.rootMotionDelta[2] * rootMotionScale_;

    if (std::fabs(dx) < 0.000001f && std::fabs(dz) < 0.000001f) {
        return;
    }

    // Rotate the local root-motion delta by the player's yaw into world space.
    // If your model's authored forward axis is opposite, flip dz here.
    float yawRad = yawDeg_ * 3.14159265f / 180.0f;
    float c = std::cos(yawRad);
    float s = std::sin(yawRad);

    float worldDx = dx * c + dz * s;
    float worldDz = -dx * s + dz * c;

    float nextX = posX_ + worldDx;
    float nextZ = posZ_ + worldDz;
    float nextY = posY_;

    resolvePlayerCollisions(nextX, nextY, nextZ);

    posX_ = nextX;
    posZ_ = nextZ;

    src->anim.clearRootMotionDelta();
}
