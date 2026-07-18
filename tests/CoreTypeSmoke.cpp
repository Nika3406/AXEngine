#include "AXEngine/Core.h"
#include "runtime/AXComponentModel.h"

#include <type_traits>

int main() {
    static_assert(std::is_same_v<decltype(ax::SceneObjectDefinition{}.position), ax::Vec3>);
    ax::SceneObjectDefinition object;
    object.position = ax::Vec3{1.0f, 2.0f, 3.0f};
    return object.position.x == 1.0f ? 0 : 1;
}
