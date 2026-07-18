#pragma once
#include "ecs/Component.h"
#include "ecs/TransformComponent.h"
#include <vector>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>

class Entity {
public:
    std::string name;
    bool        active = true;

    Entity() {
        // Every entity always has a transform
        transform_ = addComponent<TransformComponent>();
    }

    template<typename T, typename... Args>
    T* addComponent(Args&&... args) {
        auto comp       = std::make_unique<T>(std::forward<Args>(args)...);
        comp->owner     = this;
        T* raw          = comp.get();
        components_[std::type_index(typeid(T))] = std::move(comp);
        return raw;
    }

    template<typename T>
    T* getComponent() const {
        auto it = components_.find(std::type_index(typeid(T)));
        if (it == components_.end()) return nullptr;
        return static_cast<T*>(it->second.get());
    }

    template<typename T>
    bool hasComponent() const {
        return components_.count(std::type_index(typeid(T))) > 0;
    }

    TransformComponent* transform() const { return transform_; }

    void onStart() {
        for (auto& [type, comp] : components_) comp->onStart();
    }

    void onUpdate(float dt) {
        if (!active) return;
        for (auto& [type, comp] : components_) comp->onUpdate(dt);
    }

private:
    std::unordered_map<std::type_index, std::unique_ptr<Component>> components_;
    TransformComponent* transform_ = nullptr; // non-owning, always valid
};
