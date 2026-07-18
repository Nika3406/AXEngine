#pragma once

#include <string>
#include <vector>

struct AXSceneRuntimeProperty {
    std::string name;
    std::string value;
};

struct AXSceneRuntimeComponent {
    std::string type;
    bool active = true;
    bool dynamic = false;
    std::vector<AXSceneRuntimeProperty> properties;

    std::string property(const std::string& name, const std::string& fallback = "") const;
};

struct AXSceneRuntimeObject {
    std::string id;
    std::string name;
    std::string type;
    std::string sourcePath;
    bool active = true;
    bool isStatic = false;
    int parentIndex = -1;
    float position[3] = {0.0f, 0.0f, 0.0f};
    float rotation[3] = {0.0f, 0.0f, 0.0f};
    float scale[3] = {1.0f, 1.0f, 1.0f};
    std::vector<AXSceneRuntimeComponent> components;

    const AXSceneRuntimeComponent* component(const std::string& type) const;
};

struct AXSceneRuntimeData {
    std::string name = "Scene";
    std::string renderProfile = "StylizedAction";
    std::string previewGlb = "assets/Scene1.glb";
    float gravity = -24.0f;
    float unitScale = 1.0f;
    std::vector<AXSceneRuntimeObject> objects;

    const AXSceneRuntimeObject* firstObjectOfType(const std::string& type) const;
};

namespace AXSceneRuntime {
    bool load(const std::string& path, AXSceneRuntimeData& outScene, std::string* error = nullptr);

    // Converts legacy paths like scripts/assets/Foo.glb into canonical runtime paths
    // like assets/Foo.glb. New projects must author assets/ paths directly.
    std::string runtimeAssetPath(const std::string& path);

    std::string archetypeFromEnemyObject(const AXSceneRuntimeObject& object);
}
