#include "Player.h"

// AXEngine Character/Input/Collision backbone for the current third-person action template.
// This implementation belongs to the bundled action-game template, not the reusable engine library.

#include "AXEngine/AXCharacterKit.h"

// PL_SUB00.inl - player movement, collision, camera, and debug subroutines
bool Player::rawKeyDown(SDL_Scancode scancode) const {
    return ax::input::sdlKeyDown(scancode);
}

bool Player::rawKeyPressed(SDL_Scancode scancode) {
    return ax::input::sdlKeyPressed(scancode, prevRawKeys_);
}

void Player::setMouseCaptured(bool captured) {
    SDL_Window* window = SDL_GetMouseFocus();
    if (!window) return;

    SDL_SetWindowRelativeMouseMode(window, captured);

    if (captured) {
        SDL_HideCursor();
        SDL_WarpMouseInWindow(
            window,
            static_cast<float>(getWidth()) * 0.5f,
            static_cast<float>(getHeight()) * 0.5f
        );
    } else {
        SDL_ShowCursor();
    }

    mouseCaptured_ = captured;

    float mx = 0.0f;
    float my = 0.0f;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);
    wasLeftMouseDown_ = (buttons & SDL_BUTTON_LMASK) != 0;
}

bool Player::leftMousePressed() {
    return ax::input::sdlMouseButtonPressed(SDL_BUTTON_LMASK, wasLeftMouseDown_);
}

bool Player::leftMouseDownRaw() const {
    return ax::input::sdlMouseButtonDown(SDL_BUTTON_LMASK);
}

void Player::setSwordVisible(SkinnedRenderComponent* src, bool visible) {
    if (!src) return;

    for (auto& prim : src->gpuPrimitives) {
        const bool isSword =
            prim.nodeName.find("Sword") != std::string::npos ||
            prim.meshName.find("Sword") != std::string::npos;

        if (isSword) {
            prim.visible = visible;
        }
    }

    swordVisible_ = visible;
    print(std::string("Sword mode: ") + (swordVisible_ ? "ON" : "OFF / hands"));
}

void Player::hideDebugPrimitive(SkinnedRenderComponent* src) {
    if (!src) return;

    for (auto& prim : src->gpuPrimitives) {
        if (prim.nodeName == "Joint" || prim.meshName == "Circle.001") {
            prim.visible = false;
        }
    }
}

// ---------------------------------------------------------------------
// Mesh collider ground logic
// ---------------------------------------------------------------------

bool Player::isGroundTriangle(const DemoColliderTriangle& tri) const {
    return ax::character::isGroundTriangle(tri, 0.35f);
}

bool Player::isWallTriangle(const DemoColliderTriangle& tri) const {
    return ax::character::isWallTriangle(tri, 0.55f);
}

bool Player::pointInTriangleXZ(
    float px,
    float pz,
    const DemoColliderTriangle& tri,
    float& outY
) const {
    return ax::character::pointInTriangleXZ(px, pz, tri, outY);
}

bool Player::capsuleFootprintHitsTriangleXZ(
    float x,
    float z,
    float radius,
    const DemoColliderTriangle& tri,
    float& outY
) const {
    // Sample the center and a small ring around the capsule footprint.
    // This catches jumping into the underside of platforms/ramps even when
    // the player's center is just outside a triangle but the capsule edge is not.
    static constexpr float kDirs[8][2] = {
        { 1.0f,  0.0f}, {-1.0f,  0.0f}, {0.0f,  1.0f}, {0.0f, -1.0f},
        { 0.70710678f,  0.70710678f}, {-0.70710678f,  0.70710678f},
        { 0.70710678f, -0.70710678f}, {-0.70710678f, -0.70710678f}
    };

    float y = 0.0f;
    if (pointInTriangleXZ(x, z, tri, y)) {
        outY = y;
        return true;
    }

    const float r = std::max(0.01f, radius);
    for (const auto& d : kDirs) {
        if (pointInTriangleXZ(x + d[0] * r, z + d[1] * r, tri, y)) {
            outY = y;
            return true;
        }
    }

    return false;
}

bool Player::findGroundYAt(float x, float z, float& outGroundY) const {
    float bestY = -std::numeric_limits<float>::max();
    bool found = false;

    for (const DemoMeshCollider& collider : environment_.meshColliders()) {
        for (const DemoColliderTriangle& tri : collider.triangles) {
            if (!isGroundTriangle(tri)) {
                continue;
            }

            float y = 0.0f;

            if (!pointInTriangleXZ(x, z, tri, y)) {
                continue;
            }

            // Only accept ground below or slightly above the player's feet.
            // This prevents snapping to roofs/ceilings while still allowing
            // small steps, ramps, and uneven ground.
            if (y > posY_ + 0.35f) {
                continue;
            }

            if (!found || y > bestY) {
                bestY = y;
                found = true;
            }
        }
    }

    if (!found) {
        return false;
    }

    outGroundY = bestY;
    return true;
}

bool Player::findCeilingYBetween(float oldFeetY, float newFeetY, float& outCeilingY) const {
    if (newFeetY <= oldFeetY) {
        return false;
    }

    const float oldHeadY = oldFeetY + playerHeight_;
    const float newHeadY = newFeetY + playerHeight_;
    const float skin = 0.045f;
    const float footprintRadius = std::max(0.05f, playerRadius_ * 0.88f);

    float bestY = std::numeric_limits<float>::max();
    bool found = false;

    for (const DemoMeshCollider& collider : environment_.meshColliders()) {
        for (const DemoColliderTriangle& tri : collider.triangles) {
            // Ceiling/platform checks care about horizontal or sloped surfaces.
            // Wall pushout is handled separately by resolvePlayerCollisions().
            Vec3 n = triNormal(tri);
            if (std::fabs(n.y) < 0.25f) {
                continue;
            }

            float y = 0.0f;
            if (!capsuleFootprintHitsTriangleXZ(posX_, posZ_, footprintRadius, tri, y)) {
                continue;
            }

            if (y > oldHeadY + skin && y <= newHeadY + skin) {
                if (!found || y < bestY) {
                    bestY = y;
                    found = true;
                }
            }
        }
    }

    if (!found) {
        return false;
    }

    outCeilingY = bestY;
    return true;
}

// ---------------------------------------------------------------------
// Mesh collider player collision.
// This is a simple XZ cylinder-vs-triangle-projection resolver.
// It makes the player slide against actual COLLIDER_* mesh triangle edges,
// rather than using one huge AABB around the whole collider.
// ---------------------------------------------------------------------

float Player::clamp01(float v) {
    return std::clamp(v, 0.0f, 1.0f);
}

void Player::closestPointSegmentXZ(
    float px,
    float pz,
    const Vec3& a,
    const Vec3& b,
    float& outX,
    float& outZ,
    float& outDistSq
) const {
    ax::character::closestPointSegmentXZ(
        px,
        pz,
        ax::Vec3{a.x, a.y, a.z},
        ax::Vec3{b.x, b.y, b.z},
        outX,
        outZ,
        outDistSq
    );
}

bool Player::verticalOverlapWithPlayer(const DemoColliderTriangle& tri, float playerY) const {
    return ax::character::verticalOverlapWithCapsule(tri, playerY, playerHeight_);
}

bool Player::resolveAgainstTriangleXZ(
    const DemoColliderTriangle& tri,
    float& x,
    float y,
    float& z
) const {
    ax::character::CapsuleSettings settings;
    settings.radius = playerRadius_;
    settings.height = playerHeight_;
    settings.wallMaxAbsNormalY = 0.55f;
    settings.groundMinAbsNormalY = 0.35f;
    settings.footEpsilon = 0.05f;
    return ax::character::resolveCapsuleAgainstTriangleXZ(tri, x, y, z, settings);
}

void Player::resolvePlayerCollisions(float& x, float& y, float& z) {
    // A few passes help corners and complex low-poly collision.
    for (int pass = 0; pass < 3; ++pass) {
        bool moved = false;

        for (const DemoMeshCollider& collider : environment_.meshColliders()) {
            for (const DemoColliderTriangle& tri : collider.triangles) {
                if (resolveAgainstTriangleXZ(tri, x, y, z)) {
                    moved = true;
                }
            }
        }

        if (!moved) {
            break;
        }
    }
}

bool Player::movePlayerXZWithCollision(float dx, float dz) {
    const float totalDist = std::sqrt(dx * dx + dz * dz);

    if (totalDist < 0.000001f) {
        return false;
    }

    // High-speed burst attacks must be swept/sub-stepped.
    // Final-position-only collision lets a fast lunge tunnel through thin
    // collider walls. Keep each step smaller than the player radius.
    const float maxStep = std::max(0.05f, playerRadius_ * 0.30f);
    int steps = static_cast<int>(std::ceil(totalDist / maxStep));
    steps = std::clamp(steps, 1, 48);

    const float stepX = dx / static_cast<float>(steps);
    const float stepZ = dz / static_cast<float>(steps);

    bool blocked = false;

    for (int i = 0; i < steps; ++i) {
        const float beforeX = posX_;
        const float beforeZ = posZ_;

        float nextX = posX_ + stepX;
        float nextZ = posZ_ + stepZ;
        float nextY = posY_;

        resolvePlayerCollisions(nextX, nextY, nextZ);

        const float actualX = nextX - beforeX;
        const float actualZ = nextZ - beforeZ;

        const float intendedLenSq = stepX * stepX + stepZ * stepZ;
        const float forwardDot = actualX * stepX + actualZ * stepZ;

        posX_ = nextX;
        posZ_ = nextZ;

        // If collision resolution removed most of the intended forward motion,
        // consider the burst blocked. This prevents Impale/Cross from repeatedly
        // forcing into the wall for the rest of the frame sequence.
        if (intendedLenSq > 0.000001f && forwardDot < intendedLenSq * 0.20f) {
            blocked = true;
            break;
        }
    }

    return blocked;
}

bool Player::sphereHitsTriangleXZ(
    float x,
    float y,
    float z,
    float radius,
    const DemoColliderTriangle& tri
) const {
    float triMinY = std::min({tri.a.y, tri.b.y, tri.c.y});
    float triMaxY = std::max({tri.a.y, tri.b.y, tri.c.y});

    if (y + radius < triMinY || y - radius > triMaxY) {
        return false;
    }

    float ignoredY = 0.0f;
    if (pointInTriangleXZ(x, z, tri, ignoredY)) {
        return true;
    }

    Vec3 a = fromDemoVec3(tri.a);
    Vec3 b = fromDemoVec3(tri.b);
    Vec3 c = fromDemoVec3(tri.c);

    float bestX = 0.0f;
    float bestZ = 0.0f;
    float bestDistSq = std::numeric_limits<float>::max();

    auto testEdge = [&](const Vec3& p0, const Vec3& p1) {
        float cx = 0.0f;
        float cz = 0.0f;
        float d2 = 0.0f;

        closestPointSegmentXZ(x, z, p0, p1, cx, cz, d2);

        if (d2 < bestDistSq) {
            bestDistSq = d2;
            bestX = cx;
            bestZ = cz;
        }
    };

    testEdge(a, b);
    testEdge(b, c);
    testEdge(c, a);

    return bestDistSq <= radius * radius;
}

bool Player::homingSwordHitCollider() const {
    float testY = posY_ + 1.2f;

    for (const DemoMeshCollider& collider : environment_.meshColliders()) {
        for (const DemoColliderTriangle& tri : collider.triangles) {
            if (sphereHitsTriangleXZ(
                    posX_,
                    testY,
                    posZ_,
                    homingMissileRadius_,
                    tri
                )) {
                return true;
            }
        }
    }

    return false;
}

// ---------------------------------------------------------------------
// Movement
// ---------------------------------------------------------------------

bool Player::rayTriangle(
    const Vec3& origin,
    const Vec3& end,
    const DemoColliderTriangle& tri,
    float& outT
) const {
    const float EPS = 0.000001f;

    Vec3 dir = vecSub(end, origin);

    Vec3 a = fromDemoVec3(tri.a);
    Vec3 b = fromDemoVec3(tri.b);
    Vec3 c = fromDemoVec3(tri.c);

    Vec3 edge1 = vecSub(b, a);
    Vec3 edge2 = vecSub(c, a);

    Vec3 h = vecCross(dir, edge2);
    float det = vecDot(edge1, h);

    if (det > -EPS && det < EPS) {
        return false;
    }

    float invDet = 1.0f / det;
    Vec3 s = vecSub(origin, a);
    float u = invDet * vecDot(s, h);

    if (u < 0.0f || u > 1.0f) {
        return false;
    }

    Vec3 q = vecCross(s, edge1);
    float v = invDet * vecDot(dir, q);

    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }

    float t = invDet * vecDot(edge2, q);

    if (t < 0.0f || t > 1.0f) {
        return false;
    }

    outT = t;
    return true;
}

Player::Vec3 Player::resolveCameraCollision(const Vec3& target, const Vec3& desiredCam) const {
    float nearestT = 1.0f;
    bool hit = false;

    for (const DemoMeshCollider& collider : environment_.meshColliders()) {
        for (const DemoColliderTriangle& tri : collider.triangles) {
            float t = 1.0f;

            if (!rayTriangle(target, desiredCam, tri, t)) {
                continue;
            }

            // Only ignore truly immediate self/floor hits. The old 0.08 value
            // made the camera skip walls when the player target was pressed
            // close to them, which allowed the camera to pop through.
            if (t < 0.012f) {
                continue;
            }

            if (isGroundTriangle(tri)) {
                Vec3 hitPos = vecAdd(target, vecMul(vecSub(desiredCam, target), t));
                float horizontalFromTarget = std::sqrt(
                    (hitPos.x - target.x) * (hitPos.x - target.x) +
                    (hitPos.z - target.z) * (hitPos.z - target.z)
                );

                if (horizontalFromTarget < 0.55f && hitPos.y < target.y) {
                    continue;
                }
            }

            if (t < nearestT) {
                nearestT = t;
                hit = true;
            }
        }
    }

    Vec3 resolved = desiredCam;

    if (hit) {
        Vec3 dir = vecSub(desiredCam, target);
        float dist = vecLen(dir);
        Vec3 n = vecNorm(dir);

        // Collision wins over preferred camera distance. Do not force the
        // camera back to camMinDistance_ if a wall is closer than that.
        const float cameraSkin = 0.28f;
        float safeDist = std::max(0.12f, dist * nearestT - cameraSkin);
        resolved = vecAdd(target, vecMul(n, safeDist));
    }

    resolved = resolveCameraSpherePushout(resolved);

    float groundY = 0.0f;
    if (findGroundYAt(resolved.x, resolved.z, groundY)) {
        float minCameraY = groundY + camGroundClearance_;
        if (resolved.y < minCameraY) {
            resolved.y = minCameraY;
        }
    }

    return resolved;
}

Player::Vec3 Player::resolveCameraSpherePushout(const Vec3& camera) const {
    Vec3 resolved = camera;
    const float radius = 0.28f;

    for (int pass = 0; pass < 4; ++pass) {
        bool moved = false;

        for (const DemoMeshCollider& collider : environment_.meshColliders()) {
            for (const DemoColliderTriangle& tri : collider.triangles) {
                if (!isWallTriangle(tri)) {
                    continue;
                }

                float triMinY = std::min({tri.a.y, tri.b.y, tri.c.y});
                float triMaxY = std::max({tri.a.y, tri.b.y, tri.c.y});

                if (resolved.y + radius < triMinY || resolved.y - radius > triMaxY) {
                    continue;
                }

                Vec3 a = fromDemoVec3(tri.a);
                Vec3 b = fromDemoVec3(tri.b);
                Vec3 c = fromDemoVec3(tri.c);

                float bestX = 0.0f;
                float bestZ = 0.0f;
                float bestDistSq = std::numeric_limits<float>::max();

                auto testEdge = [&](const Vec3& p0, const Vec3& p1) {
                    float cx = 0.0f;
                    float cz = 0.0f;
                    float d2 = 0.0f;
                    closestPointSegmentXZ(resolved.x, resolved.z, p0, p1, cx, cz, d2);
                    if (d2 < bestDistSq) {
                        bestDistSq = d2;
                        bestX = cx;
                        bestZ = cz;
                    }
                };

                testEdge(a, b);
                testEdge(b, c);
                testEdge(c, a);

                if (bestDistSq >= radius * radius) {
                    continue;
                }

                float dx = resolved.x - bestX;
                float dz = resolved.z - bestZ;
                float dist = std::sqrt(std::max(bestDistSq, 0.0f));

                if (dist < 0.0001f) {
                    Vec3 n = triNormal(tri);
                    dx = n.x;
                    dz = n.z;
                    dist = std::sqrt(dx * dx + dz * dz);
                    if (dist < 0.0001f) {
                        dx = resolved.x - posX_;
                        dz = resolved.z - posZ_;
                        dist = std::sqrt(dx * dx + dz * dz);
                    }
                    if (dist < 0.0001f) {
                        dx = 1.0f;
                        dz = 0.0f;
                        dist = 1.0f;
                    }
                }

                dx /= dist;
                dz /= dist;

                const float push = (radius - dist) + 0.015f;
                resolved.x += dx * push;
                resolved.z += dz * push;
                moved = true;
            }
        }

        if (!moved) {
            break;
        }
    }

    return resolved;
}

void Player::rebuildColliderDebugLines() {
    if (!renderer()) return;

    renderer()->clearDebugLines();

    if (!showColliderWireframes_) {
        appendCombatDebugLines();
        return;
    }

    const float r = 0.0f;
    const float g = 1.0f;
    const float b = 0.1f;

    for (const DemoMeshCollider& collider : environment_.meshColliders()) {
        for (const DemoColliderTriangle& tri : collider.triangles) {
            renderer()->addDebugLine(
                tri.a.x, tri.a.y, tri.a.z,
                tri.b.x, tri.b.y, tri.b.z,
                r, g, b
            );

            renderer()->addDebugLine(
                tri.b.x, tri.b.y, tri.b.z,
                tri.c.x, tri.c.y, tri.c.z,
                r, g, b
            );

            renderer()->addDebugLine(
                tri.c.x, tri.c.y, tri.c.z,
                tri.a.x, tri.a.y, tri.a.z,
                r, g, b
            );
        }
    }

    appendCombatDebugLines();

    print("Collider debug wireframes rebuilt.");
}

void Player::printColliderList() {
    print("---- Mesh colliders loaded from scene ----");

    int i = 0;
    size_t totalTriangles = 0;

    for (const DemoMeshCollider& collider : environment_.meshColliders()) {
        totalTriangles += collider.triangles.size();

        std::string line =
            std::to_string(i++) + ": \"" + collider.name + "\""
            + " triangles=" + std::to_string(collider.triangles.size());

        print(line);
    }

    print("Mesh collider objects: " + std::to_string(environment_.meshColliders().size()));
    print("Mesh collider triangles: " + std::to_string(totalTriangles));
    print("Debug AABBs: " + std::to_string(environment_.colliders().size()));
    print("----");
}

std::string Player::combatStateName() const {
    switch (combatState_) {
        case CombatState::None: return "none";
        case CombatState::Attack: return "attack";
        case CombatState::Dash: return "dash";
        case CombatState::DashAttack: return "dash attack";
        case CombatState::AirAttack: return "air attack";
        case CombatState::ChargeAttack: return "charge attack";
        case CombatState::HomingSword: return "homing sword";
        default: return "unknown";
    }
}

void Player::updateFpsDebug(float dt) {
    if (dt <= 0.0f) {
        return;
    }

    frameMs_ = dt * 1000.0f;

    if (frameMs_ < minFrameMs_) {
        minFrameMs_ = frameMs_;
    }

    if (frameMs_ > maxFrameMs_) {
        maxFrameMs_ = frameMs_;
    }

    fpsTimer_ += dt;
    fpsFrameCount_++;

    if (fpsTimer_ >= 1.0f) {
        fps_ = static_cast<float>(fpsFrameCount_) / fpsTimer_;

        if (showFpsDebug_) {
            print(
                "[FPS] " + std::to_string(fps_) +
                " | frame " + std::to_string(frameMs_) + " ms" +
                " | min " + std::to_string(minFrameMs_) + " ms" +
                " | max " + std::to_string(maxFrameMs_) + " ms"
            );
        }

        fpsTimer_ = 0.0f;
        fpsFrameCount_ = 0;
        minFrameMs_ = 9999.0f;
        maxFrameMs_ = 0.0f;
    }
}

void Player::printDebugInfo(SkinnedRenderComponent* src) {
    if (!src) return;

    print("---- Player Debug ----");
    print("Clip             : " + src->currentClipName());
    print("Speed            : " + std::to_string(currentSpeed_));
    print("Actual speed     : " + std::to_string(actualGroundSpeed_));
    print("Move blocked     : " + std::string(movementBlocked_ ? "yes" : "no"));
    print("Grounded         : " + std::string(grounded_ ? "yes" : "no"));
    print("PosY             : " + std::to_string(posY_));
    print("GroundY          : " + std::to_string(groundY_));
    print("VerticalVelocity : " + std::to_string(verticalVelocity_));
    print("AnimState        : " + currentClip_);
    print("Sword            : " + std::string(swordVisible_ ? "visible" : "hidden"));
    print("Mouse captured   : " + std::string(mouseCaptured_ ? "yes" : "no"));
    print("Combat           : " + combatStateName());
    print("Combo index      : " + std::to_string(comboIndex_));
    print("Attack timer     : " + std::to_string(attackTimer_) + " / " + std::to_string(attackDuration_));
    print("Attack normalized: " + std::to_string(attackNormalized_));
    print("Attack buffered  : " + std::string(attackQueued_ ? "yes" : "no"));
    print("Combo consumed   : " + std::string(comboConsumed_ ? "yes" : "no"));
    print("Hit active       : " + std::string(isAttackHitActive() ? "yes" : "no"));
    print("Special clip     : " + specialClip_);
    print("Special timer    : " + std::to_string(specialTimer_) + " / " + std::to_string(specialDuration_));
    print("Special move spd : " + std::to_string(specialMoveSpeed_));
    print("Special down spd : " + std::to_string(specialDownSpeed_));
    print("Scripted motion  : " + std::string(specialUsesScriptedMotion_ ? "yes" : "no"));
    print("LMB hold timer   : " + std::to_string(lmbHoldTimer_));
    print("Homing phase     : " + std::to_string(homingSwordPhase_));
    print("Homing timer     : " + std::to_string(homingTimer_));
    print("RootMotion joint : " + std::to_string(rootMotionJointIndex_));
    print("RootMotion delta : " +
          std::to_string(src->anim.rootMotionDelta[0]) + ", " +
          std::to_string(src->anim.rootMotionDelta[1]) + ", " +
          std::to_string(src->anim.rootMotionDelta[2]));
    print("Mesh colliders   : " + std::to_string(environment_.meshColliders().size()));
    print("Debug AABBs      : " + std::to_string(environment_.colliders().size()));
    print("FPS              : " + std::to_string(fps_));
    print("Frame ms         : " + std::to_string(frameMs_));
    print("----");
}
// Third-person camera orbit/follow is engine backbone behavior.
void Player::updateCamera(float dt) {
    float mx = 0.0f;
    float my = 0.0f;

    if (mouseCaptured_) {
        SDL_GetRelativeMouseState(&mx, &my);
    }

    camYawDeg_ -= mx * mouseSensitivity_;
    camPitchDeg_ += my * mouseSensitivity_;

    camPitchDeg_ = std::clamp(camPitchDeg_, -55.0f, 55.0f);

    float yawRad = camYawDeg_ * 3.14159265f / 180.0f;
    float pitchRad = camPitchDeg_ * 3.14159265f / 180.0f;

    Vec3 target{
        posX_,
        posY_ + camHeight_,
        posZ_
    };

    Vec3 offset{
        std::sin(yawRad) * std::cos(pitchRad) * camDistance_,
        std::sin(pitchRad) * camDistance_,
        std::cos(yawRad) * std::cos(pitchRad) * camDistance_
    };

    Vec3 desiredCam = vecAdd(target, offset);
    Vec3 resolvedCam = resolveCameraCollision(target, desiredCam);

    float smooth = std::clamp(14.0f * dt, 0.0f, 1.0f);

    Vec3 smoothedCam{
        camX_ + (resolvedCam.x - camX_) * smooth,
        camY_ + (resolvedCam.y - camY_) * smooth,
        camZ_ + (resolvedCam.z - camZ_) * smooth
    };

    // Interpolation itself can cross a wall if the previous camera was on the
    // wrong side or the player/camera target moved quickly. Sweep the smoothed
    // candidate too so every displayed camera position keeps line-of-sight.
    smoothedCam = resolveCameraCollision(target, smoothedCam);

    camX_ = smoothedCam.x;
    camY_ = smoothedCam.y;
    camZ_ = smoothedCam.z;

    camTargetX_ = target.x;
    camTargetY_ = target.y;
    camTargetZ_ = target.z;

    if (renderer()) {
        renderer()->setCamera(
            camX_, camY_, camZ_,
            camTargetX_, camTargetY_, camTargetZ_
        );
        // Anchor the shadow frustum to the player's foot position, not the
        // chest-height camera orbit target.
        renderer()->setShadowAnchor(posX_, posY_, posZ_);
    }
}

// ---------------------------------------------------------------------
// Debug
// ---------------------------------------------------------------------

