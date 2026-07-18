#include "runtime/AXLODSystem.h"
#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace ax {
namespace {
static std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) out.push_back(item);
    return out;
}
}

LODGroupDefinition LODSystem::fromComponent(const ComponentDefinition& component) {
    LODGroupDefinition g;
    g.crossFade = getBoolProperty(component, "crossFade", true);
    g.fadeRange = getFloatProperty(component, "fadeRange", 3.0f);

    std::string distances = getProperty(component, "distances", "0,35,90");
    std::string meshes = getProperty(component, "meshes", "LOD0,LOD1,LOD2");
    auto ds = split(distances, ',');
    auto ms = split(meshes, ',');
    size_t n = std::max(ds.size(), ms.size());
    for (size_t i = 0; i < n; ++i) {
        LODLevelDefinition level;
        if (i < ds.size()) level.startDistance = std::strtof(ds[i].c_str(), nullptr);
        if (i < ms.size()) level.mesh = ms[i];
        g.levels.push_back(level);
    }
    std::sort(g.levels.begin(), g.levels.end(), [](const auto& a, const auto& b){ return a.startDistance < b.startDistance; });
    return g;
}

int LODSystem::chooseLOD(const LODGroupDefinition& group, float distanceToCamera) {
    if (group.levels.empty()) return -1;
    int best = 0;
    for (int i = 0; i < (int)group.levels.size(); ++i) {
        if (distanceToCamera >= group.levels[i].startDistance) best = i;
    }
    return best;
}

} // namespace ax
