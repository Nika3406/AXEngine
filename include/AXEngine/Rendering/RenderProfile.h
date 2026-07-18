#pragma once

#include "AXEngine/Core.h"
#include "OGL3D/RenderStyle.h"

namespace ax::rendering {

enum class RenderFamily {
    Realistic,
    Stylized,
    Cartoon,
    PS1,
    PS2,
    PS3,
    PS4,
    VFXPreview,
    DebugUnlit
};

struct MaterialDescriptor {
    String name;
    String vertexShader;
    String fragmentShader;
    String baseColorTexture;
    Color baseColor{};
    bool doubleSided = false;
    bool transparent = false;
    bool emissive = false;
    bool bloomSource = false;
    float emissiveStrength = 0.0f;
};

} // namespace ax::rendering
