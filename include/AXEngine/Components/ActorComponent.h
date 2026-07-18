#pragma once

#include "AXEngine/Core.h"

namespace ax {

class Actor;

class ActorComponent {
public:
    virtual ~ActorComponent() = default;

    Actor* owner() const { return owner_; }
    bool enabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }

    virtual const char* typeName() const { return "ActorComponent"; }
    virtual void onStart() {}
    virtual void onUpdate(float) {}

private:
    friend class Actor;
    Actor* owner_ = nullptr;
    bool enabled_ = true;
};

} // namespace ax
