#pragma once

#include "AXEngine/GameFramework/Actor.h"
#include "AXEngine/Input/InputAction.h"

namespace ax {

class PlayerController : public ActorComponent {
public:
    const char* typeName() const override { return "PlayerController"; }

    virtual void onAction(const InputActionEvent&) {}
    virtual void onLook(const Vec2&) {}
    virtual void onMove(const Vec2&) {}
};

} // namespace ax
