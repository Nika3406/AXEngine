#pragma once

#include "AXEngine/Core.h"

namespace ax::character {

// Public character-movement settings. Stage 3.1.1 does not move the old
// Player/Sub.cpp controller yet; it exposes the settings in an engine-library
// shape so the next AXCharacterKit stage can move implementation safely.
struct CharacterControllerSettings {
    float walkSpeed = 2.2f;
    float runSpeed = 5.0f;
    float sprintSpeed = 7.5f;
    float acceleration = 10.0f;

    float radius = 0.45f;
    float height = 1.85f;

    float gravity = 38.0f;
    float jumpForce = 16.0f;
    float fallGravity = 78.5f;
    float jumpGravity = 28.0f;
    float terminalFallSpeed = -22.0f;

    float maxGroundSnapDown = 0.35f;
    float maxGroundSnapUp = 0.45f;
};

struct ThirdPersonCameraSettings {
    float yawDeg = 180.0f;
    float pitchDeg = 18.0f;
    float distance = 9.0f;
    float minDistance = 0.5f;
    float height = 2.0f;
    float groundClearance = 0.35f;
    float mouseSensitivity = 0.12f;
};

struct CharacterMotorState {
    Vec3 position{};
    Vec3 velocity{};
    float yawDeg = 180.0f;
    bool grounded = false;
    bool movementBlocked = false;
};

} // namespace ax::character
