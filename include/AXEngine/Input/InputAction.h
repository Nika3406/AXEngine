#pragma once

#include "AXEngine/Core.h"

namespace ax {

enum class InputPhase {
    Started,
    Performed,
    Released,
    Canceled
};

struct InputActionEvent {
    String action;
    InputPhase phase = InputPhase::Performed;
    float value = 0.0f;
    Vec2 axis{};
    float heldSeconds = 0.0f;
    uint64_t frame = 0;
};

class InputRouter {
public:
    using Callback = std::function<void(const InputActionEvent&)>;

    void bind(const String& action, Callback callback) {
        callbacks_[action].push_back(std::move(callback));
    }

    void emit(const InputActionEvent& event) const {
        auto it = callbacks_.find(event.action);
        if (it == callbacks_.end()) return;
        for (const Callback& callback : it->second) {
            if (callback) callback(event);
        }
    }

private:
    Map<String, Array<Callback>> callbacks_;
};

} // namespace ax
