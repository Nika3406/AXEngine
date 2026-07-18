#include "Player.h"

// User-facing movement/camera decisions for the action prototype.
// Raw SDL input, triangle probing, capsule/wall solving, ray tests and debug
// support live in AXEngine runtime modules.

void Player::handleMovementInput(float dt) {
    moveDirX_ = 0.0f;
    moveDirZ_ = 0.0f;
    actualGroundSpeed_ = 0.0f;
    movementBlocked_ = false;

    if (combatState_ != CombatState::None) {
        hasMoveInput_ = false;
        targetSpeed_ = 0.0f;
        currentSpeed_ = 0.0f;
        return;
    }

    if (keyDown("Forward"))  moveDirZ_ -= 1.0f;
    if (keyDown("Backward")) moveDirZ_ += 1.0f;
    if (keyDown("Left"))     moveDirX_ += 1.0f;
    if (keyDown("Right"))    moveDirX_ -= 1.0f;

    float len = std::sqrt(moveDirX_ * moveDirX_ + moveDirZ_ * moveDirZ_);
    hasMoveInput_ = len > 0.001f;

    if (hasMoveInput_) {
        float yawRad = camYawDeg_ * 3.14159265f / 180.0f;

        float forwardX = -std::sin(yawRad);
        float forwardZ = -std::cos(yawRad);

        float rightX = -std::cos(yawRad);
        float rightZ =  std::sin(yawRad);

        float inputX = moveDirX_ / len;
        float inputZ = moveDirZ_ / len;

        moveDirX_ = rightX * inputX - forwardX * inputZ;
        moveDirZ_ = rightZ * inputX - forwardZ * inputZ;

        float worldLen = std::sqrt(moveDirX_ * moveDirX_ + moveDirZ_ * moveDirZ_);

        if (worldLen > 0.001f) {
            moveDirX_ /= worldLen;
            moveDirZ_ /= worldLen;
        }

        walkHeld_ = rawKeyDown(SDL_SCANCODE_LCTRL) ||
                    rawKeyDown(SDL_SCANCODE_RCTRL);

        sprintHeld_ = rawKeyDown(SDL_SCANCODE_LSHIFT) ||
                      rawKeyDown(SDL_SCANCODE_RSHIFT);

        if (walkHeld_) {
            targetSpeed_ = walkSpeed_;
        } else if (sprintHeld_) {
            targetSpeed_ = sprintSpeed_;
        } else {
            targetSpeed_ = runSpeed_;
        }

        yawDeg_ = std::atan2(moveDirX_, moveDirZ_) * 180.0f / 3.14159265f;

        float t = std::clamp(acceleration_ * dt, 0.0f, 1.0f);
        currentSpeed_ += (targetSpeed_ - currentSpeed_) * t;
    } else {
        walkHeld_ = false;
        sprintHeld_ = false;
        targetSpeed_ = 0.0f;
        currentSpeed_ = 0.0f;
    }

    const float beforeX = posX_;
    const float beforeZ = posZ_;

    const float intendedDX = moveDirX_ * currentSpeed_ * dt;
    const float intendedDZ = moveDirZ_ * currentSpeed_ * dt;

    float nextX = posX_ + intendedDX;
    float nextZ = posZ_ + intendedDZ;
    float nextY = posY_;

    resolvePlayerCollisions(nextX, nextY, nextZ);

    const float actualDX = nextX - beforeX;
    const float actualDZ = nextZ - beforeZ;
    const float actualDist = std::sqrt(actualDX * actualDX + actualDZ * actualDZ);
    const float intendedDist = std::sqrt(intendedDX * intendedDX + intendedDZ * intendedDZ);

    actualGroundSpeed_ = dt > 0.000001f ? actualDist / dt : 0.0f;

    // If the wall removes most of the requested forward movement, the player
    // is not truly sprinting anymore. Drop the controller speed down toward
    // the real post-collision speed so the state machine falls back to walk/run
    // instead of playing a full sprint in-place against the wall.
    if (hasMoveInput_ && intendedDist > 0.0001f) {
        const float forwardProgress = actualDX * moveDirX_ + actualDZ * moveDirZ_;
        movementBlocked_ = forwardProgress < intendedDist * 0.35f;

        if (movementBlocked_) {
            currentSpeed_ = std::min(currentSpeed_, std::max(actualGroundSpeed_, walkSpeed_));
        }
    }

    posX_ = nextX;
    posZ_ = nextZ;
}

// ---------------------------------------------------------------------
// Jump and vertical motion
// ---------------------------------------------------------------------

void Player::handleJumpInput() {
    if (combatState_ != CombatState::None) {
        return;
    }

    if (!rawKeyPressed(SDL_SCANCODE_SPACE)) {
        return;
    }

    if (grounded_) {
        grounded_ = false;
        edgeFallTimer_ = 0.0f;
        verticalVelocity_ = jumpForce_;
        airJumpsUsed_ = 0;
        return;
    }

    // Double jump / air step.
    if (airJumpsUsed_ < maxAirJumps_) {
        airJumpsUsed_++;

        verticalVelocity_ = airStepForce_;
        edgeFallTimer_ = 0.0f;

        airStepAnimating_ = true;
        airStepAnimTimer_ = 0.0f;

        if (player_) {
            if (auto* src = player_->skinnedRender()) {
                src->anim.speed = 1.5f;
                src->crossFade("AirStep", 0.04f, false);
                currentClip_ = "AirStep";
            }
        }

        print("AirStep");
    }
}

void Player::updateVerticalMotion(float dt) {
    if (combatState_ == CombatState::HomingSword &&
        homingSwordPhase_ == 1) {
        verticalVelocity_ = 0.0f;
        grounded_ = false;
        edgeFallTimer_ = 0.0f;
        return;
    }
    float detectedGroundY = 0.0f;
    bool hasGround = findGroundYAt(posX_, posZ_, detectedGroundY);

    if (grounded_) {
        if (hasGround) {
            float deltaToGround = detectedGroundY - posY_;

            bool canSnapUp =
                deltaToGround >= 0.0f &&
                deltaToGround <= maxGroundSnapUp_;

            bool canSnapDown =
                deltaToGround < 0.0f &&
                std::fabs(deltaToGround) <= maxGroundSnapDown_;

            if (canSnapUp || canSnapDown) {
                groundY_ = detectedGroundY;
                posY_ = groundY_;
                verticalVelocity_ = 0.0f;
                edgeFallTimer_ = 0.0f;
                return;

                airJumpsUsed_ = 0;
                airStepAnimating_ = false;
                airStepAnimTimer_ = 0.0f;
            }

            // Ground exists, but it is too far below.
            // Treat this as a ledge/drop, not a stair step.
            if (deltaToGround < -maxGroundSnapDown_) {
                edgeFallTimer_ += dt;

                if (edgeFallTimer_ >= edgeFallDelay_) {
                    grounded_ = false;
                    verticalVelocity_ = -0.35f;
                }

                return;
            }

            // Ground is too far above. Do not snap upward onto it.
            return;
        }

        // No ground under player.
        edgeFallTimer_ += dt;

        if (edgeFallTimer_ >= edgeFallDelay_) {
            grounded_ = false;
            verticalVelocity_ = -0.35f;
        }

        return;
    }

    // Airborne.
    const float previousY = posY_;
    bool falling = verticalVelocity_ <= 0.0f;
    float activeGravity = falling ? fallGravity_ : jumpGravity_;

    verticalVelocity_ -= activeGravity * dt;

    float activeTerminalFallSpeed = terminalFallSpeed_;

    if (combatState_ == CombatState::AirAttack &&
        (specialClip_ == "AxeKick" ||
        specialClip_ == "AxeKickIdle" ||
        specialClip_ == "Helm Breaker")) {
        activeTerminalFallSpeed = strikeDownTerminalFallSpeed_;
    }

    if (verticalVelocity_ < activeTerminalFallSpeed) {
        verticalVelocity_ = activeTerminalFallSpeed;
    }

    posY_ += verticalVelocity_ * dt;

    if (verticalVelocity_ > 0.0f) {
        float ceilingY = 0.0f;
        if (findCeilingYBetween(previousY, posY_, ceilingY)) {
            posY_ = ceilingY - playerHeight_ - 0.045f;
            verticalVelocity_ = 0.0f;
            grounded_ = false;
            edgeFallTimer_ = 0.0f;
        }
    }

    if (hasGround && verticalVelocity_ <= 0.0f && posY_ <= detectedGroundY) {
        posY_ = detectedGroundY;
        groundY_ = detectedGroundY;
        verticalVelocity_ = 0.0f;
        grounded_ = true;
        edgeFallTimer_ = 0.0f;

        airJumpsUsed_ = 0;
        airStepAnimating_ = false;
        airStepAnimTimer_ = 0.0f;
    }
}
// ---------------------------------------------------------------------
// Animation
// ---------------------------------------------------------------------
