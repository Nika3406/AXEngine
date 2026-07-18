#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "AXEngine/Core.h"

namespace ax {


struct ComponentProperty {
    std::string name;
    std::string value;
};

struct ComponentDefinition {
    std::string type;
    bool active = true;
    bool dynamic = false;
    std::vector<ComponentProperty> properties;
};

struct SceneObjectDefinition {
    std::string id;
    std::string name;
    std::string type;
    std::string sourcePath;
    bool active = true;
    bool isStatic = false;
    int parentIndex = -1;
    Vec3 position;
    Vec3 rotation;
    Vec3 scale = {1.0f, 1.0f, 1.0f};
    std::vector<ComponentDefinition> components;
};

struct SceneDefinition {
    std::string name;
    std::string renderProfile = "StylizedAction";
    float gravity = -24.0f;
    float unitScale = 1.0f;
    std::vector<SceneObjectDefinition> objects;
};

class ComponentRegistry {
public:
    using FactoryName = std::string;

    void registerComponentType(const std::string& type, const std::string& category, const std::string& description);
    bool hasComponentType(const std::string& type) const;
    std::vector<std::string> componentTypes() const;
    std::string categoryFor(const std::string& type) const;
    std::string descriptionFor(const std::string& type) const;

    static ComponentRegistry makeDefaultRuntimeRegistry();

private:
    struct Entry {
        std::string category;
        std::string description;
    };
    std::unordered_map<std::string, Entry> entries_;
};

std::string getProperty(const ComponentDefinition& component, const std::string& name, const std::string& fallback = "");
float getFloatProperty(const ComponentDefinition& component, const std::string& name, float fallback = 0.0f);
bool getBoolProperty(const ComponentDefinition& component, const std::string& name, bool fallback = false);

} // namespace ax
