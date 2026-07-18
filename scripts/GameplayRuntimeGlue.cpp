#include "Player.h"
#include "AXEngine/Gameplay/Events.h"

void Player::emitGameplayEvent(
    const std::string& type,
    const std::string& senderId,
    const std::string& targetId,
    const std::string& payload,
    float value,
    const Vec3& position,
    const Vec3& direction
) {
    gameplayRuntime_.emit(
        type,
        senderId,
        targetId,
        payload,
        value,
        ax::Vec3f{position.x, position.y, position.z},
        ax::Vec3f{direction.x, direction.y, direction.z}
    );
}

void Player::registerPlayerGameplayObject() {
    if (!playerRuntimeId_.empty()) return;

    ax::RuntimeObjectRecord& rec = gameplayRuntime_.registerObject(
        "Player",
        "Player",
        ax::Vec3f{posX_, posY_, posZ_},
        ax::Vec3f{0.0f, yawDeg_, 0.0f},
        ax::Vec3f{1.0f, 1.0f, 1.0f},
        {"Player", "CombatActor", "LockOnSource"}
    );

    playerRuntimeId_ = rec.id;
    emitGameplayEvent(
        ax::events::PlayerSpawned,
        playerRuntimeId_,
        {},
        "Player",
        0.0f,
        Vec3{posX_, posY_, posZ_},
        playerForward()
    );
}

void Player::updateGameplayRuntimeObjects() {
    if (!playerRuntimeId_.empty()) {
        gameplayRuntime_.updateTransform(
            playerRuntimeId_,
            ax::Vec3f{posX_, posY_, posZ_},
            ax::Vec3f{0.0f, yawDeg_, 0.0f},
            ax::Vec3f{1.0f, 1.0f, 1.0f}
        );
    }

    for (const EnemyActor& enemy : enemies_) {
        if (enemy.runtimeId.empty()) continue;
        gameplayRuntime_.updateTransform(
            enemy.runtimeId,
            ax::Vec3f{enemy.x, enemy.y, enemy.z},
            ax::Vec3f{0.0f, enemy.yawDeg, 0.0f},
            ax::Vec3f{1.0f, 1.0f, 1.0f}
        );
    }
}
