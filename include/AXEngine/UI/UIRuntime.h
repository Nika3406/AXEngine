#pragma once

#include "AXEngine/Core.h"
#include "runtime/AXGameplayPrimitives.h"

namespace ax::ui {

enum class Space {
    Screen,
    World
};

struct UIElement {
    String id;
    String type = "Panel";
    Space space = Space::Screen;
    Vec3 position{};
    Vec2 size{100.0f, 32.0f};
    Color color{};
    String text;
    AssetRef texture;
};

namespace Events {
    inline constexpr const char* UIButtonPressed = "OnUIButtonPressed";
    inline constexpr const char* UIValueChanged = "OnUIValueChanged";
}

} // namespace ax::ui
