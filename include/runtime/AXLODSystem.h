#pragma once

#include "runtime/AXComponentModel.h"
#include <string>
#include <vector>

namespace ax {

struct LODLevelDefinition {
    float startDistance = 0.0f;
    std::string mesh;
    bool visible = true;
};

struct LODGroupDefinition {
    std::vector<LODLevelDefinition> levels;
    bool crossFade = true;
    float fadeRange = 3.0f;
};

class LODSystem {
public:
    static LODGroupDefinition fromComponent(const ComponentDefinition& component);
    static int chooseLOD(const LODGroupDefinition& group, float distanceToCamera);
};

} // namespace ax
