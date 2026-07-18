#pragma once

#include "AXEngine/Core.h"
#include "AXEngine/Components/ActorComponent.h"

namespace ax {

class Actor {
public:
    String name;
    Transform transform;
    Array<String> tags;

    Actor() = default;
    explicit Actor(String actorName) : name(std::move(actorName)) {}
    virtual ~Actor() = default;

    virtual const char* typeName() const { return "Actor"; }
    virtual void onStart() {}
    virtual void onUpdate(float) {}

    bool hasTag(const String& tag) const {
        return std::find(tags.begin(), tags.end(), tag) != tags.end();
    }

    void addTag(const String& tag) {
        if (!hasTag(tag)) tags.push_back(tag);
    }

    template <typename T, typename... Args>
    T& addComponent(Args&&... args) {
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        component->owner_ = this;
        T& ref = *component;
        components_.push_back(std::move(component));
        return ref;
    }

    template <typename T>
    T* getComponent() const {
        for (const auto& component : components_) {
            if (auto* typed = dynamic_cast<T*>(component.get())) return typed;
        }
        return nullptr;
    }

    const Array<std::unique_ptr<ActorComponent>>& components() const { return components_; }

private:
    Array<std::unique_ptr<ActorComponent>> components_;
};

} // namespace ax
